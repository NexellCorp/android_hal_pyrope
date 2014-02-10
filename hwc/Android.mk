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

ifeq ($(PYROPE_HWC_VERSION),1)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libsync libEGL libcutils libhardware libhardware_legacy libutils libion-nexell libv4l2-nexell libion
LOCAL_CFLAGS += -DLOG_TAG=\"hwcomposer\"

LOCAL_C_INCLUDES += \
	frameworks/native/include \
	system/core/include \
	hardware/libhardware/include \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../include

LOCAL_SRC_FILES := hdmi.cpp hwc.cpp

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := test.cpp
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libgui libui libv4l2-nexell
LOCAL_C_INCLUDES := frameworks/native/include \
					system/core/include \
					hardware/libhardware/include  \
					$(LOCAL_PATH)/../include

LOCAL_MODULE := hwc_test
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

endif
