ifeq ($(BOARD_HAS_CAMERA),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)

LOCAL_C_INCLUDES += \
	frameworks/native/include \
	system/media/camera/include \
	$(LOCAL_PATH)/../include

LOCAL_SRC_FILES := \
	S5K4ECGX.cpp \
	S5K5CAGX.cpp \
	SP0838.cpp \
	SP0A19.cpp \
	SP2518.cpp

LOCAL_SHARED_LIBRARIES := liblog libv4l2-nexell
LOCAL_MODULE := libcamerasensor
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

endif
