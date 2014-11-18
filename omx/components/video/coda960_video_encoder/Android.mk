LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false

NX_HW_TOP := $(TOP)/hardware/nexell/pyrope/
NX_HW_INCLUDE := $(NX_HW_TOP)/include
OMX_TOP := $(TOP)/hardware/nexell/pyrope/omx
NX_LINUX_INCLUDE := $(TOP)/linux/nxp5430/library/include

RATECONTROL_PATH := $(TOP)/linux/nxp5430/library/lib/ratecontrol

LOCAL_SRC_FILES:= \
	NX_OMXVideoEncoder.c

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/include \
	$(TOP)/hardware/libhardware/include \
	$(TOP)/hardware/nexell/pyrope/include \
	$(TOP)/hardware/nexell/pyrope/nxutil

LOCAL_C_INCLUDES += \
	$(OMX_TOP)/include \
	$(OMX_TOP)/core/inc \
	$(OMX_TOP)/codec/video/coda960 \
	$(OMX_TOP)/components/base \
	$(NX_LINUX_INCLUDE)


LOCAL_SHARED_LIBRARIES := \
	libNX_OMX_Common \
	libNX_OMX_Base \
	libdl \
	liblog \
	libhardware \
	libnx_vpu \
	libion \
	libion-nexell \
	libnxutil

LOCAL_LDFLAGS += \
	-L$(RATECONTROL_PATH)	\
	-lnxvidrc_android

LOCAL_CFLAGS += $(NX_OMX_CFLAGS)
LOCAL_CFLAGS += -DNX_DYNAMIC_COMPONENTS

LOCAL_MODULE := libNX_OMX_VIDEO_ENCODER

include $(BUILD_SHARED_LIBRARY)
