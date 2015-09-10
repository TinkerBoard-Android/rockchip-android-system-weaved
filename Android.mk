# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

# TODO(avakulenko): Remove this condition when libchromeos can be built on
# non-Linux host.
ifeq ($(HOST_OS),linux)

# Common variables
# ========================================================

buffetCommonCppExtension := .cc
buffetCommonCFlags := -DBUFFET_USE_WIFI_BOOTSTRAPPING -Wall -Werror \
	-Wno-char-subscripts -Wno-missing-field-initializers \
	-Wno-unused-function -Wno-unused-parameter \

buffetCommonCppFlags := \
	-Wno-deprecated-register \
	-Wno-sign-compare \
	-Wno-sign-promo \
	-Wno-non-virtual-dtor \

buffetCommonCIncludes := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/dbus-proxies \
	external/gtest/include \

buffetSharedLibraries := \
	libavahi-common \
	libchrome \
	libchrome-dbus \
	libchromeos \
	libchromeos-dbus \
	libchromeos-http \
	libchromeos-stream \
	libdbus \
	libweave \
	libwebserv \

ifdef BRILLO
buffetSharedLibraries += libconnectivity
endif

# buffet-common
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := buffet-common
LOCAL_CPP_EXTENSION := $(buffetCommonCppExtension)
LOCAL_CFLAGS := $(buffetCommonCFlags)
LOCAL_CPPFLAGS := $(buffetCommonCppFlags)
LOCAL_C_INCLUDES := $(buffetCommonCIncludes)
LOCAL_SHARED_LIBRARIES := $(buffetSharedLibraries)
LOCAL_STATIC_LIBRARIES :=
LOCAL_RTTI_FLAG := -frtti
LOCAL_CLANG := true
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_SRC_FILES := \
	buffet/buffet_config.cc \
	buffet/dbus_command_dispatcher.cc \
	buffet/dbus_command_proxy.cc \
	buffet/dbus_conversion.cc \
	buffet/dbus_constants.cc \
	buffet/http_transport_client.cc \
	buffet/manager.cc \
	buffet/socket_stream.cc \
	buffet/webserv_client.cc \
	buffet/dbus_bindings/dbus-service-config.json \
	buffet/dbus_bindings/com.android.Weave.Command.dbus-xml \
	buffet/dbus_bindings/com.android.Weave.Manager.dbus-xml \

ifdef BRILLO

LOCAL_SRC_FILES += \
	buffet/avahi_mdns_client.cc \
	buffet/brillo_network_client.cc \

else # BRILLO

LOCAL_SRC_FILES += \
	buffet/stub_mdns_client.cc \
	buffet/stub_network_client.cc \

endif # BRILLO

include $(BUILD_STATIC_LIBRARY)

# buffet
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := weaved
LOCAL_REQUIRED_MODULES := \
	avahi-daemon \
	base_state.defaults.json \
	base_state.schema.json \
	weaved.json \
	com.android.Weave.conf \
	gcd.json \
	webservd \

LOCAL_CPP_EXTENSION := $(buffetCommonCppExtension)
LOCAL_CFLAGS := $(buffetCommonCFlags)
LOCAL_CPPFLAGS := $(buffetCommonCppFlags)
LOCAL_C_INCLUDES := $(buffetCommonCIncludes)
LOCAL_INIT_RC := init.weaved.rc
LOCAL_SHARED_LIBRARIES := $(buffetSharedLibraries)
LOCAL_WHOLE_STATIC_LIBRARIES := buffet-common
LOCAL_CLANG := true
LOCAL_RTTI_FLAG := -frtti

LOCAL_SRC_FILES := \
	buffet/main.cc

include $(BUILD_EXECUTABLE)

# libweaved-client
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libweaved-client
LOCAL_DBUS_PROXY_PREFIX := buffet

LOCAL_SRC_FILES := \
	buffet/dbus_bindings/dbus-service-config.json \
	buffet/dbus_bindings/com.android.Weave.Command.dbus-xml \
	buffet/dbus_bindings/com.android.Weave.Manager.dbus-xml \

include $(BUILD_SHARED_LIBRARY)

# buffet_testrunner
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := buffet_testrunner
LOCAL_CPP_EXTENSION := $(buffetCommonCppExtension)
LOCAL_CFLAGS := $(buffetCommonCFlags)
LOCAL_CPPFLAGS := $(buffetCommonCppFlags)
LOCAL_C_INCLUDES := \
	$(buffetCommonCIncludes) \
	external/gmock/include \

LOCAL_SHARED_LIBRARIES := \
	$(buffetSharedLibraries) \

LOCAL_STATIC_LIBRARIES := \
	buffet-common \
	libchrome_dbus_test_helpers \
	libchrome_test_helpers \
	libchromeos-test-helpers \
	libgtest \
	libgmock \
	libweave-test \

LOCAL_RTTI_FLAG := -frtti
LOCAL_CLANG := true

LOCAL_SRC_FILES := \
	buffet/buffet_testrunner.cc \
	buffet/dbus_command_proxy_unittest.cc \
	buffet/dbus_conversion_unittest.cc \

include $(BUILD_NATIVE_TEST)

# weaved_client
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := weaved_client
LOCAL_CPP_EXTENSION := $(buffetCommonCppExtension)
LOCAL_CFLAGS := $(buffetCommonCFlags)
LOCAL_CPPFLAGS := $(buffetCommonCppFlags)
LOCAL_C_INCLUDES := $(buffetCommonCIncludes)
LOCAL_SHARED_LIBRARIES := $(buffetSharedLibraries)
LOCAL_STATIC_LIBRARIES :=
LOCAL_DBUS_PROXY_PREFIX := buffet
LOCAL_RTTI_FLAG := -frtti
LOCAL_CLANG := true

LOCAL_SRC_FILES := \
	buffet/buffet_client.cc \
	buffet/dbus_bindings/dbus-service-config.json \
	buffet/dbus_bindings/com.android.Weave.Command.dbus-xml \
	buffet/dbus_bindings/com.android.Weave.Manager.dbus-xml \

include $(BUILD_EXECUTABLE)

# Config files for /etc/weaved
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := base_state.defaults.json
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/weaved
LOCAL_SRC_FILES := buffet/etc/weaved/base_state.defaults.json
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := base_state.schema.json
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/weaved
LOCAL_SRC_FILES := buffet/etc/weaved/base_state.schema.json
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := gcd.json
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/weaved
LOCAL_SRC_FILES := buffet/etc/weaved/gcd.json
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := weaved.json
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/weaved/commands
LOCAL_SRC_FILES := buffet/etc/weaved/commands/weaved.json
include $(BUILD_PREBUILT)

# DBus config files for /etc/dbus-1
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := com.android.Weave.conf
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/dbus-1
LOCAL_SRC_FILES := buffet/etc/dbus-1/com.android.Weave.conf
include $(BUILD_PREBUILT)

endif # HOST_OS == linux
