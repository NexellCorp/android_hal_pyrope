LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_C_INCLUDES += hardware/nexell/pyrope/include \
					hardware/nexell/pyrope/kernel-headers
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_SRC_FILES := ion.cpp
LOCAL_MODULE := libion-nexell
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
