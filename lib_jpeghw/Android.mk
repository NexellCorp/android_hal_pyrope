LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../include \
	$(TOP)/linux/pyrope/library/include

LOCAL_SRC_FILES := NXJpegHWEnc.cpp

LOCAL_SHARED_LIBRARIES := liblog libcutils libion-nexell libion libnx_vpu

LOCAL_MODULE := libNX_Jpeghw

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
