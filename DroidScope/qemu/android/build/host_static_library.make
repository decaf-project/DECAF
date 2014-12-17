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

# build a host executable, the name of the final executable should be
# put in LOCAL_BUILT_MODULE for use by the caller
#

#$(info STATIC_LIBRARY SRCS=$(LOCAL_SRC_FILES))
LOCAL_BUILT_MODULE := $(call library-path,$(LOCAL_MODULE))
LOCAL_CC ?= $(CC)
include $(BUILD_SYSTEM)/binary.make

LOCAL_AR := $(strip $(LOCAL_AR))
ifndef LOCAL_AR
    LOCAL_AR := $(AR)
endif
ARFLAGS := crs

$(LOCAL_BUILT_MODULE): PRIVATE_AR := $(LOCAL_AR)
$(LOCAL_BUILT_MODULE): PRIVATE_OBJECTS := $(LOCAL_OBJECTS)
$(LOCAL_BUILT_MODULE): $(LOCAL_OBJECTS)
	@mkdir -p $(dir $@)
	@echo "Library: $@"
	$(hide) $(PRIVATE_AR) $(ARFLAGS) $@ $(PRIVATE_OBJECTS)

LIBRARIES += $(LOCAL_BUILT_MODULE)
