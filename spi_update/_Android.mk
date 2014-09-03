LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := spieeprom.c update.c
LOCAL_MODULE := libspi_update
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
