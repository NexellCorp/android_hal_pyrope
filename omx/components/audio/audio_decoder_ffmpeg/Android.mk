ifeq ($(EN_FFMPEG_AUDIO_DEC),true)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false

NX_HW_TOP 		:= $(TOP)/hardware/nexell/pyrope
NX_HW_INCLUDE	:= $(NX_HW_TOP)/include
OMX_TOP			:= $(TOP)/hardware/nexell/pyrope/omx
FFMPEG_PATH		:= $(OMX_TOP)/codec/ffmpeg

LOCAL_SRC_FILES:= \
	NX_OMXAudioDecoderFFMpeg.c

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/include \
	$(TOP)/hardware/libhardware/include \
	$(TOP)/hardware/nexell/pyrope/include \
	$(OMX_TOP)/include \
	$(OMX_TOP)/core/inc \
	$(OMX_TOP)/components/base \
	$(FFMPEG_PATH)/include

LOCAL_SHARED_LIBRARIES := \
	libNX_OMX_Common \
	libNX_OMX_Base \
	libdl \
	liblog \
	libhardware \

LOCAL_LDFLAGS += \
	-L$(FFMPEG_PATH)/libs	\
	-lavutil-1.2 			\
	-lavcodec-1.2   		\
	-lavformat-1.2			\
	-lavdevice-1.2			\
	-lavfilter-1.2			\
	-lswresample-1.2

LOCAL_CFLAGS := $(NX_OMX_CFLAGS)

LOCAL_CFLAGS += -DNX_DYNAMIC_COMPONENTS

LOCAL_MODULE:= libNX_OMX_AUDIO_DECODER_FFMPEG

include $(BUILD_SHARED_LIBRARY)

endif	# EN_FFMPEG_AUDIO_DEC
