LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := liblog libcutils

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)	\
	$(LOCAL_PATH)/../include

LOCAL_SRC_FILES:= libnxmem.c platform.c

LOCAL_MODULE:= libNX_Mem

include $(BUILD_SHARED_LIBRARY)
