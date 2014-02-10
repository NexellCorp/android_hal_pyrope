LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
# LOCAL_PRELINK_MODULE := false

NX_HW_TOP := $(TOP)/hardware/nexell/pyrope/
NX_HW_INCLUDE := $(NX_HW_TOP)/include

LOCAL_SHARED_LIBRARIES :=	\
	liblog \
	libcutils \
	libion \
	libion-nexell

LOCAL_C_INCLUDES := $(TOP)/system/core/include/ion
LOCAL_C_INCLUDES += $(NX_HW_TOP)/include

LOCAL_CFLAGS := 

LOCAL_SRC_FILES := \
	nx_video_api.c \
	nx_alloc_mem_ion.c

LOCAL_MODULE := libnx_vpu

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

