# Copyright (C) 2008 The Android Open Source Project
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
#

# disable implicit rules
.SUFFIXES:
%:: %,v
%:: RCS/%
%:: RCS/%,v
%:: s.%
%:: SCCS/s.%
%.c: %.w %.ch


# this is a set of definitions that allow the usage of Makefile.android
# even if we're not using the Android build system.
#

BUILD_SYSTEM := android/build
OBJS_DIR     := objs
LIBS_DIR     := $(OBJS_DIR)/libs
CONFIG_MAKE  := $(OBJS_DIR)/config.make
CONFIG_H     := $(OBJS_DIR)/config-host.h

ifeq ($(wildcard $(CONFIG_MAKE)),)
    $(error "The configuration file '$(CONFIG_MAKE)' doesnt' exist, please run the "android-configure.sh" script)
endif

include $(CONFIG_MAKE)
include $(BUILD_SYSTEM)/definitions.make

VPATH := $(OBJS_DIR)
VPATH += :$(SRC_PATH)/android/config
VPATH += :$(SRC_PATH):$(SRC_PATH)/target-$(TARGET_ARCH)

.PHONY: all libraries executables clean clean-config clean-objs-dir \
        clean-executables clean-libraries

CLEAR_VARS                := $(BUILD_SYSTEM)/clear_vars.make
BUILD_HOST_EXECUTABLE     := $(BUILD_SYSTEM)/host_executable.make
BUILD_HOST_STATIC_LIBRARY := $(BUILD_SYSTEM)/host_static_library.make

DEPENDENCY_DIRS :=

all: libraries executables
EXECUTABLES :=
LIBRARIES   :=

ifneq ($(SDL_CONFIG),)
SDL_LIBS   := $(filter %.a,$(shell $(SDL_CONFIG) --static-libs))
$(foreach lib,$(SDL_LIBS), \
    $(eval $(call copy-prebuilt-lib,$(lib))) \
)
endif

clean: clean-intermediates

distclean: clean clean-config

# let's roll
include Makefile.android

libraries: $(LIBRARIES)
executables: $(EXECUTABLES)

clean-intermediates:
	rm -rf $(OBJS_DIR)/intermediates $(EXECUTABLES) $(LIBRARIES)

clean-config:
	rm -f $(CONFIG_MAKE) $(CONFIG_H)

# include dependency information
DEPENDENCY_DIRS := $(sort $(DEPENDENCY_DIRS))
-include $(wildcard $(DEPENDENCY_DIRS:%=%/*.d))
