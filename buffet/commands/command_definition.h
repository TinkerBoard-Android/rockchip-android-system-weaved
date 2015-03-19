// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_DEFINITION_H_
#define BUFFET_COMMANDS_COMMAND_DEFINITION_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include "buffet/commands/object_schema.h"

namespace buffet {

// A simple GCD command definition. This class contains the command category
// and a full object schema describing the command parameter types and
// constraints. See comments for CommandDefinitions::LoadCommands for the
// detailed description of what command categories are and what they are used
// for.
class CommandDefinition {
 public:
  CommandDefinition(const std::string& category,
                    std::unique_ptr<const ObjectSchema> parameters,
                    std::unique_ptr<const ObjectSchema> results);

  // Gets the category this command belongs to.
  const std::string& GetCategory() const { return category_; }
  // Gets the object schema for command parameters.
  const ObjectSchema* GetParameters() const { return parameters_.get(); }
  // Gets the object schema for command results.
  const ObjectSchema* GetResults() const { return results_.get(); }

 private:
  std::string category_;  // Cmd category. Could be "powerd" for "base.reboot".
  std::unique_ptr<const ObjectSchema> parameters_;  // Command parameters def.
  std::unique_ptr<const ObjectSchema> results_;  // Command results def.
  DISALLOW_COPY_AND_ASSIGN(CommandDefinition);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_DEFINITION_H_
