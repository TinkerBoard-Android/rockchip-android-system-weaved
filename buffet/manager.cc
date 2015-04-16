// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/manager.h"

#include <map>
#include <string>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/time/time.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/errors/error.h>
#include <chromeos/key_value_store.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>
#include <dbus/values_util.h>

#include "buffet/commands/command_instance.h"
#include "buffet/commands/command_manager.h"
#include "buffet/states/state_change_queue.h"
#include "buffet/states/state_manager.h"
#include "buffet/storage_impls.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

namespace {
// Max of 100 state update events should be enough in the queue.
const size_t kMaxStateChangeQueueSize = 100;
}  // anonymous namespace

Manager::Manager(const base::WeakPtr<ExportedObjectManager>& object_manager)
    : dbus_object_(object_manager.get(),
                   object_manager->GetBus(),
                   org::chromium::Buffet::ManagerAdaptor::GetObjectPath()) {
}

Manager::~Manager() {
}

void Manager::Start(const base::FilePath& config_path,
                    const base::FilePath& state_path,
                    const base::FilePath& test_definitions_path,
                    bool xmpp_enabled,
                    const AsyncEventSequencer::CompletionAction& cb) {
  command_manager_ =
      std::make_shared<CommandManager>(dbus_object_.GetObjectManager());
  command_manager_->SetOnCommandDefChanged(
      base::Bind(&Manager::OnCommandDefsChanged, base::Unretained(this)));
  command_manager_->Startup(base::FilePath{"/etc/buffet"},
                            test_definitions_path);
  state_change_queue_ = std::unique_ptr<StateChangeQueue>(
      new StateChangeQueue(kMaxStateChangeQueueSize));
  state_manager_ = std::make_shared<StateManager>(state_change_queue_.get());
  state_manager_->Startup();
  std::unique_ptr<BuffetConfig> config{new BuffetConfig};
  config->Load(config_path);
  std::unique_ptr<FileStorage> state_store{new FileStorage{state_path}};
  // TODO(avakulenko): Figure out security implications of storing
  // device info state data unencrypted.
  device_info_ = std::unique_ptr<DeviceRegistrationInfo>(
      new DeviceRegistrationInfo(
          command_manager_,
          state_manager_,
          std::move(config),
          chromeos::http::Transport::CreateDefault(),
          std::move(state_store),
          xmpp_enabled,
          base::Bind(&Manager::OnRegistrationStatusChanged,
                     base::Unretained(this))));
  device_info_->Load();
  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(cb);
}

void Manager::CheckDeviceRegistered(DBusMethodResponse<std::string> response) {
  LOG(INFO) << "Received call to Manager.CheckDeviceRegistered()";
  chromeos::ErrorPtr error;
  bool registered = device_info_->CheckRegistration(&error);
  // If it fails due to any reason other than 'device not registered',
  // treat it as a real error and report it to the caller.
  if (!registered &&
      !error->HasError(kErrorDomainGCD, "device_not_registered")) {
    response->ReplyWithError(error.get());
    return;
  }

  response->Return(registered ? device_info_->GetDeviceId() : std::string());
}

void Manager::GetDeviceInfo(DBusMethodResponse<std::string> response) {
  LOG(INFO) << "Received call to Manager.GetDeviceInfo()";

  chromeos::ErrorPtr error;
  auto device_info = device_info_->GetDeviceInfo(&error);
  if (!device_info) {
    response->ReplyWithError(error.get());
    return;
  }

  std::string device_info_str;
  base::JSONWriter::WriteWithOptions(device_info.get(),
      base::JSONWriter::OPTIONS_PRETTY_PRINT, &device_info_str);
  response->Return(device_info_str);
}

void Manager::RegisterDevice(DBusMethodResponse<std::string> response,
                             const chromeos::VariantDictionary& params) {
  LOG(INFO) << "Received call to Manager.RegisterDevice()";

  chromeos::ErrorPtr error;
  std::map<std::string, std::string> str_params;
  for (const auto& pair : params) {
    if (!pair.second.IsTypeCompatible<std::string>()) {
      response->ReplyWithError(FROM_HERE, chromeos::errors::dbus::kDomain,
                               DBUS_ERROR_INVALID_ARGS,
                               "String value expected");
      return;
    }
    str_params.emplace_hint(str_params.end(), pair.first,
                            pair.second.Get<std::string>());
  }
  std::string device_id = device_info_->RegisterDevice(str_params, &error);
  if (!device_id.empty()) {
    response->Return(device_id);
    return;
  }
  if (!error) {
    // TODO(zeuthen): This can be changed to CHECK(error) once
    // RegisterDevice() has been fixed to set |error| when failing.
    chromeos::Error::AddTo(&error, FROM_HERE, kErrorDomainGCD, "internal_error",
                           "device_id empty but error not set");
  }
  response->ReplyWithError(error.get());
}

void Manager::UpdateState(DBusMethodResponse<> response,
                          const chromeos::VariantDictionary& property_set) {
  chromeos::ErrorPtr error;
  base::Time timestamp = base::Time::Now();
  bool all_success = true;
  for (const auto& pair : property_set) {
    if (!state_manager_->SetPropertyValue(pair.first, pair.second, timestamp,
                                          &error)) {
      // Remember that an error occurred but keep going and update the rest of
      // the properties if possible.
      all_success = false;
    }
  }
  if (!all_success)
    response->ReplyWithError(error.get());
  else
    response->Return();
}

bool Manager::GetState(chromeos::ErrorPtr* error, std::string* state) {
  auto json = state_manager_->GetStateValuesAsJson(error);
  if (!json)
    return false;
  base::JSONWriter::WriteWithOptions(
      json.get(), base::JSONWriter::OPTIONS_PRETTY_PRINT, state);
  return true;
}

void Manager::AddCommand(DBusMethodResponse<std::string> response,
                         const std::string& json_command) {
  static int next_id = 0;
  std::string error_message;
  std::unique_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      json_command, base::JSON_PARSE_RFC, nullptr, &error_message));
  if (!value) {
    response->ReplyWithError(FROM_HERE, chromeos::errors::json::kDomain,
                             chromeos::errors::json::kParseError,
                             error_message);
    return;
  }
  chromeos::ErrorPtr error;
  auto command_instance = buffet::CommandInstance::FromJson(
      value.get(), command_manager_->GetCommandDictionary(), &error);
  if (!command_instance) {
    response->ReplyWithError(error.get());
    return;
  }
  std::string id = std::to_string(++next_id);
  command_instance->SetID(id);
  command_manager_->AddCommand(std::move(command_instance));
  response->Return(id);
}

void Manager::GetCommand(DBusMethodResponse<std::string> response,
                         const std::string& id) {
  const CommandInstance* command = command_manager_->FindCommand(id);
  if (!command) {
    response->ReplyWithError(FROM_HERE, kErrorDomainGCD, "unknown_command",
                             "Can't find command with id: " + id);
    return;
  }
  std::string command_str;
  base::JSONWriter::WriteWithOptions(command->ToJson().get(),
      base::JSONWriter::OPTIONS_PRETTY_PRINT, &command_str);
  response->Return(command_str);
}

std::string Manager::TestMethod(const std::string& message) {
  LOG(INFO) << "Received call to test method: " << message;
  return message;
}

void Manager::OnRegistrationStatusChanged() {
  dbus_adaptor_.SetStatus(
      StatusToString(device_info_->GetRegistrationStatus()));
  dbus_adaptor_.SetDeviceId(device_info_->GetDeviceId());
}

void Manager::OnCommandDefsChanged() {
  chromeos::ErrorPtr error;
  std::unique_ptr<base::DictionaryValue> commands =
      command_manager_->GetCommandDictionary().GetCommandsAsJson(true, &error);
  CHECK(commands);
  std::string json;
  base::JSONWriter::WriteWithOptions(commands.get(),
      base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  dbus_adaptor_.SetCommandDefs(json);
}

}  // namespace buffet
