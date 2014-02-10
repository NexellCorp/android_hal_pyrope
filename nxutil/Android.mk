LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)

LOCAL_C_INCLUDES += \
	system/core/include \
	frameworks/native/include \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../include

LOCAL_SRC_FILES := \
	NXAllocator.cpp \
	NXScaler.cpp \
	NXCpu.cpp \
	csc.cpp \
	NXCsc.cpp

LOCAL_SHARED_LIBRARIES := liblog libutils libcutils libion-nexell libion

LOCAL_MODULE := libnxutil

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
