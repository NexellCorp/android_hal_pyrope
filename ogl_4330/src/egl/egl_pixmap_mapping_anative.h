/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_pixmap_mapping_anative.h
 * @brief Android native pixel format mappings.
 */

#ifndef _EGL_PIXMAP_MAPPING_ANATIVE_H_
#define _EGL_PIXMAP_MAPPING_ANATIVE_H_

#if MALI_ANDROID_API >= 15
#include <system/window.h>
#ifdef NEXELL_ANDROID_FEATURE_EGL_YV12_KHR_444_FORMAT
#include <gralloc_priv.h>
#endif
#else
#include <ui/egl/android_natives.h>
#endif

#include <mali_system.h>
#include "shared/m200_texel_format.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _egl_android_pixel_format
{
	int android_pixel_format;
	m200_texel_format egl_pixel_format;
	mali_bool red_blue_swap;
	mali_bool reverse_order;
	int yuv_format;
} egl_android_pixel_format;

/* Possible pixel formats: (see hardware/libhardware/include/hardware/hardware.h
 * HAL_PIXEL_FORMAT_RGBA_8888    = 1,
 * HAL_PIXEL_FORMAT_RGBX_8888    = 2,
 * HAL_PIXEL_FORMAT_RGB_888      = 3,
 * HAL_PIXEL_FORMAT_RGB_565      = 4,
 * HAL_PIXEL_FORMAT_BGRA_8888    = 5,
 * HAL_PIXEL_FORMAT_RGBA_5551    = 6,
 * HAL_PIXEL_FORMAT_RGBA_4444    = 7,
 * HAL_PIXEL_FORMAT_YV12   = 0x32315659, (HAL_PIXEL_FORMAT_YCbCr_420_P on versions prior to gingerbread)
 */
static egl_android_pixel_format __egl_android_pixel_mapping[] = 
{
	/* RGB formats */
	{ HAL_PIXEL_FORMAT_RGBA_8888, M200_TEXEL_FORMAT_ARGB_8888,  MALI_TRUE,  MALI_FALSE, 0 },
	{ HAL_PIXEL_FORMAT_RGBX_8888, M200_TEXEL_FORMAT_xRGB_8888,  MALI_TRUE,  MALI_FALSE, 0 },
#if !RGB_IS_XRGB
	{ HAL_PIXEL_FORMAT_RGB_888,   M200_TEXEL_FORMAT_RGB_888,    MALI_TRUE,  MALI_FALSE, 0 },
#endif
	{ HAL_PIXEL_FORMAT_RGB_565,   M200_TEXEL_FORMAT_RGB_565,    MALI_FALSE, MALI_FALSE, 0 },
	{ HAL_PIXEL_FORMAT_BGRA_8888, M200_TEXEL_FORMAT_ARGB_8888,  MALI_FALSE, MALI_FALSE, 0 },
#if !defined( NEXELL_FEATURE_KITKAT_USE )
	{ HAL_PIXEL_FORMAT_RGBA_5551, M200_TEXEL_FORMAT_ARGB_1555,  MALI_TRUE,  MALI_TRUE,  0 },
	{ HAL_PIXEL_FORMAT_RGBA_4444, M200_TEXEL_FORMAT_ARGB_4444,  MALI_TRUE,  MALI_TRUE,  0 },
#endif
	/* YUV formats */
	/* YV12 is a 4:2:0 YVU planar format containing a WxH Y plane followed by (W/2)x(H/2) V and U planes */
#if MALI_ANDROID_API > 8 
	#ifdef NEXELL_ANDROID_FEATURE_EGL_YV12_KHR_444_FORMAT
	{ HAL_PIXEL_FORMAT_YV12_444, M200_TEXEL_FORMAT_ARGB_8888, MALI_FALSE, MALI_FALSE, EGL_YV12_KHR_444 },
	#endif
	{ HAL_PIXEL_FORMAT_YV12, M200_TEXEL_FORMAT_ARGB_8888, MALI_FALSE, MALI_FALSE, EGL_YV12_KHR },
	{ HAL_PIXEL_FORMAT_YCrCb_420_SP, M200_TEXEL_FORMAT_ARGB_8888, MALI_FALSE, MALI_FALSE, EGL_YVU420SP_KHR },
#else
	{ HAL_PIXEL_FORMAT_YCbCr_420_P, M200_TEXEL_FORMAT_ARGB_8888, MALI_FALSE, MALI_FALSE, EGL_YUV420P_KHR },
#endif
};

MALI_STATIC_INLINE egl_android_pixel_format* _egl_android_get_native_buffer_format( android_native_buffer_t *buffer)
{
	u32 index;
	
	for ( index = 0; index < (sizeof(__egl_android_pixel_mapping) / sizeof(__egl_android_pixel_mapping[0])); index++ )
	{
		if ( buffer->format == __egl_android_pixel_mapping[index].android_pixel_format )
		{
			return  &__egl_android_pixel_mapping[index];
		}
	}
	return NULL;
}

#ifdef __cplusplus 
}
#endif

#endif
