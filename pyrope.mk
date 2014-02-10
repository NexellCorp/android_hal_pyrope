# hw composer HAL
PRODUCT_PACKAGES += \
	hwcomposer.pyrope

# gralloc HAL
PRODUCT_PACKAGES += \
	gralloc.pyrope

# camera HAL
# PRODUCT_PACKAGES += \
# 	camera.pyrope

# audio HAL
PRODUCT_PACKAGES += \
	audio.primary.pyrope

# lights HAL
PRODUCT_PACKAGES += \
	lights.pyrope

# power HAL
PRODUCT_PACKAGES += \
	power.pyrope

# keymaster HAL
PRODUCT_PACKAGES += \
	keymaster.pyrope

# ion library
PRODUCT_PACKAGES += \
	libion-nexell

# v4l2 library
PRODUCT_PACKAGES += \
	libv4l2-nexell

# omx
PRODUCT_PACKAGES += \
	libstagefrighthw \
	libNX_OMX_VIDEO_DECODER \
	libNX_OMX_VIDEO_ENCODER \
	libNX_OMX_Base \
	libNX_OMX_Core \
	libNX_OMX_Common


# stagefright FFMPEG compnents
ifeq ($(EN_FFMPEG_AUDIO_DEC),true)
PRODUCT_PACKAGES += libNX_OMX_AUDIO_DECODER_FFMPEG
endif

ifeq ($(EN_FFMPEG_EXTRACTOR),true)
PRODUCT_PACKAGES += libNX_FFMpegExtractor
endif

# for legacy : only 3200
# PRODUCT_PACKAGES += \
# 	libNX_Mem

# for ogl_4330
PRODUCT_PACKAGES += \
	libVR \
	libGLESv1_CM_vr \
	libGLESv2_vr \
	libEGL_vr

PRODUCT_VENDOR_KERNEL_HEADERS := hardware/nexell/pyrope/kernel-headers
