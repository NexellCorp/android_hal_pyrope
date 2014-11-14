LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_C_INCLUDES += hardware/nexell/pyrope/include \
					hardware/nexell/pyrope/kernel-headers
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_SRC_FILES := nxp-v4l2.cpp nxp-v4l2-dev.cpp
LOCAL_SRC_FILES += ../../../../device/nexell/$(TARGET_BOOTLOADER_BOARD_NAME)/v4l2/android-nxp-v4l2.cpp
LOCAL_MODULE := libv4l2-nexell
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
