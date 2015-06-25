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

# this turns off the suffix rules built into make
.SUFFIXES:

# this turns off the RCS / SCCS implicit rules of GNU Make
% : RCS/%,v
% : RCS/%
% : %,v
% : s.%
% : SCCS/s.%

# If a rule fails, delete $@.
.DELETE_ON_ERROR:

# shared definitions
ifeq ($(strip $(SHOW)),)
define pretty
@echo $1
endef
hide := 
else
define pretty
endef
hide :=
endif

define my-dir
.
endef

# return the directory containing the intermediate files for a given
# kind of executable
# $1 = type (EXECUTABLES or STATIC_LIBRARIES)
# $2 = module name
# $3 = ignored
#
define intermediates-dir-for
$(OBJS_DIR)/intermediates/$(2)
endef

# Generate the full path of a given static library
define library-path
$(OBJS_DIR)/libs/$(1).a
endef

define executable-path
$(OBJS_DIR)/$(1)$(EXE)
endef

# Compile a C source file
#
define  compile-c-source
SRC:=$(1)
OBJ:=$$(LOCAL_OBJS_DIR)/$$(SRC:%.c=%.o)
LOCAL_OBJECTS += $$(OBJ)
DEPENDENCY_DIRS += $$(dir $$(OBJ))
$$(OBJ): PRIVATE_CFLAGS := $$(CFLAGS) $$(LOCAL_CFLAGS) -I$$(LOCAL_PATH) -I$$(LOCAL_OBJS_DIR)
$$(OBJ): PRIVATE_CC     := $$(LOCAL_CC)
$$(OBJ): PRIVATE_OBJ    := $$(OBJ)
$$(OBJ): PRIVATE_MODULE := $$(LOCAL_MODULE)
$$(OBJ): PRIVATE_SRC    := $$(SRC_PATH)/$$(SRC)
$$(OBJ): PRIVATE_SRC0   := $$(SRC)
$$(OBJ): $$(SRC_PATH)/$$(SRC)
	@mkdir -p $$(dir $$(PRIVATE_OBJ))
	@echo "Compile: $$(PRIVATE_MODULE) <= $$(PRIVATE_SRC0)"
	$(hide) $$(PRIVATE_CC) $$(PRIVATE_CFLAGS) -c -o $$(PRIVATE_OBJ) -MMD -MP -MF $$(PRIVATE_OBJ).d.tmp $$(PRIVATE_SRC)
	$(hide) $$(BUILD_SYSTEM)/mkdeps.sh $$(PRIVATE_OBJ) $$(PRIVATE_OBJ).d.tmp $$(PRIVATE_OBJ).d
endef

# Compile a C++ source file
#
define  compile-cxx-source
SRC:=$(1)
OBJ:=$$(LOCAL_OBJS_DIR)/$$(SRC:%$(LOCAL_CPP_EXTENSION)=%.o)
LOCAL_OBJECTS += $$(OBJ)
DEPENDENCY_DIRS += $$(dir $$(OBJ))
$$(OBJ): PRIVATE_CFLAGS := $$(CFLAGS) $$(LOCAL_CFLAGS) -I$$(LOCAL_PATH) -I$$(LOCAL_OBJS_DIR)
$$(OBJ): PRIVATE_CXX    := $$(LOCAL_CC)
$$(OBJ): PRIVATE_OBJ    := $$(OBJ)
$$(OBJ): PRIVATE_MODULE := $$(LOCAL_MODULE)
$$(OBJ): PRIVATE_SRC    := $$(SRC_PATH)/$$(SRC)
$$(OBJ): PRIVATE_SRC0   := $$(SRC)
$$(OBJ): $$(SRC_PATH)/$$(SRC)
	@mkdir -p $$(dir $$(PRIVATE_OBJ))
	@echo "Compile: $$(PRIVATE_MODULE) <= $$(PRIVATE_SRC0)"
	$(hide) $$(PRIVATE_CXX) $$(PRIVATE_CFLAGS) -c -o $$(PRIVATE_OBJ) -MMD -MP -MF $$(PRIVATE_OBJ).d.tmp $$(PRIVATE_SRC)
	$(hide) $$(BUILD_SYSTEM)/mkdeps.sh $$(PRIVATE_OBJ) $$(PRIVATE_OBJ).d.tmp $$(PRIVATE_OBJ).d
endef

# Compile an Objective-C source file
#
define  compile-objc-source
SRC:=$(1)
OBJ:=$$(LOCAL_OBJS_DIR)/$$(notdir $$(SRC:%.m=%.o))
LOCAL_OBJECTS += $$(OBJ)
DEPENDENCY_DIRS += $$(dir $$(OBJ))
$$(OBJ): PRIVATE_CFLAGS := $$(CFLAGS) $$(LOCAL_CFLAGS) -I$$(LOCAL_PATH) -I$$(LOCAL_OBJS_DIR)
$$(OBJ): PRIVATE_CC     := $$(LOCAL_CC)
$$(OBJ): PRIVATE_OBJ    := $$(OBJ)
$$(OBJ): PRIVATE_MODULE := $$(LOCAL_MODULE)
$$(OBJ): PRIVATE_SRC    := $$(SRC_PATH)/$$(SRC)
$$(OBJ): PRIVATE_SRC0   := $$(SRC)
$$(OBJ): $$(SRC_PATH)/$$(SRC)
	@mkdir -p $$(dir $$(PRIVATE_OBJ))
	@echo "Compile: $$(PRIVATE_MODULE) <= $$(PRIVATE_SRC0)"
	$(hide) $$(PRIVATE_CC) $$(PRIVATE_CFLAGS) -c -o $$(PRIVATE_OBJ) -MMD -MP -MF $$(PRIVATE_OBJ).d.tmp $$(PRIVATE_SRC)
	$(hide) $$(BUILD_SYSTEM)/mkdeps.sh $$(PRIVATE_OBJ) $$(PRIVATE_OBJ).d.tmp $$(PRIVATE_OBJ).d
endef

# Compile a generated C source files#
#
define compile-generated-c-source
SRC:=$(1)
OBJ:=$$(LOCAL_OBJS_DIR)/$$(notdir $$(SRC:%.c=%.o))
LOCAL_OBJECTS += $$(OBJ)
DEPENDENCY_DIRS += $$(dir $$(OBJ))
$$(OBJ): PRIVATE_CFLAGS := $$(CFLAGS) $$(LOCAL_CFLAGS) -I$$(LOCAL_PATH) -I$$(LOCAL_OBJS_DIR)
$$(OBJ): PRIVATE_CC     := $$(LOCAL_CC)
$$(OBJ): PRIVATE_OBJ    := $$(OBJ)
$$(OBJ): PRIVATE_MODULE := $$(LOCAL_MODULE)
$$(OBJ): PRIVATE_SRC    := $$(SRC)
$$(OBJ): PRIVATE_SRC0   := $$(SRC)
$$(OBJ): $$(SRC)
	@mkdir -p $$(dir $$(PRIVATE_OBJ))
	@echo "Compile: $$(PRIVATE_MODULE) <= $$(PRIVATE_SRC0)"
	$(hide) $$(PRIVATE_CC) $$(PRIVATE_CFLAGS) -c -o $$(PRIVATE_OBJ) -MMD -MP -MF $$(PRIVATE_OBJ).d.tmp $$(PRIVATE_SRC)
	$(hide) $$(BUILD_SYSTEM)/mkdeps.sh $$(PRIVATE_OBJ) $$(PRIVATE_OBJ).d.tmp $$(PRIVATE_OBJ).d
endef

# Install a file
#
define install-target
SRC:=$(1)
DST:=$(2)
$$(DST): PRIVATE_SRC := $$(SRC)
$$(DST): PRIVATE_DST := $$(DST)
$$(DST): PRIVATE_DST_NAME := $$(notdir $$(DST))
$$(DST): PRIVATE_SRC_NAME := $$(SRC)
$$(DST): $$(SRC)
	@mkdir -p $$(dir $$(PRIVATE_DST))
	@echo "Install: $$(PRIVATE_DST_NAME) <= $$(PRIVATE_SRC_NAME)"
	$(hide) cp -f $$(PRIVATE_SRC) $$(PRIVATE_DST)
install: $$(DST)
endef

# for now, we only use prebuilt SDL libraries, so copy them
define copy-prebuilt-lib
_SRC := $(1)
_SRC1 := $$(notdir $$(_SRC))
_DST := $$(LIBS_DIR)/$$(_SRC1)
LIBRARIES += $$(_DST)
$$(_DST): PRIVATE_DST := $$(_DST)
$$(_DST): PRIVATE_SRC := $$(_SRC)
$$(_DST): $$(_SRC)
	@mkdir -p $$(dir $$(PRIVATE_DST))
	@echo "Prebuilt: $$(PRIVATE_DST)"
	$(hide) cp -f $$(PRIVATE_SRC) $$(PRIVATE_DST)
endef

define  create-dir
$(1):
	mkdir -p $(1)
endef

define transform-generated-source
@echo "Generated: $(PRIVATE_MODULE) <= $<"
@mkdir -p $(dir $@)
$(hide) $(PRIVATE_CUSTOM_TOOL)
endef

