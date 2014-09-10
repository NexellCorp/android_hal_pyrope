LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false

NX_HW_TOP := $(TOP)/hardware/nexell/pyrope/
NX_HW_INCLUDE := $(NX_HW_TOP)/include
NX_LINUX_INCLUDE := $(TOP)/linux/nxp5430/library/include

OMX_TOP := $(TOP)/hardware/nexell/pyrope/omx

LOCAL_SRC_FILES:= \
	NX_AVCDecoder.c \
	NX_MPEG2Decoder.c \
	NX_MPEG4Decoder.c \
	NX_Div3Decoder.c \
	NX_RVDecoder.c \
	NX_VC1Decoder.c \
	NX_DecoderUtil.c \
	NX_AVCUtil.c \
	NX_OMXVideoDecoder.c

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/include \
	$(TOP)/hardware/libhardware/include \
	$(TOP)/hardware/nexell/pyrope/include

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
	libion-nexell

LOCAL_CFLAGS += $(NX_OMX_CFLAGS)
LOCAL_CFLAGS += -DNX_DYNAMIC_COMPONENTS

LOCAL_MODULE:= libNX_OMX_VIDEO_DECODER

include $(BUILD_SHARED_LIBRARY)
