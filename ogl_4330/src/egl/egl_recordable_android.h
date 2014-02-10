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
 * @file egl_recordable_android.h
 * @brief EGL_ANDROID_recordable extension.
 */

#ifndef _EGL_RECORDABLE_ANDROID_H_
#define _EGL_RECORDABLE_ANDROID_H_

/**
 * @addtogroup egl_extension EGL Extensions
 *
 * @{
 */

/**
 * @defgroup egl_extension_recordable EGL_ANDROID_recordable extension
 *
 * @note EGL_ANDROID_recordable extension
 *
 * @{
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>

/**
 * @brief create an internal render target
 * @param buffer android native buffer
 * @param surface the egl surface
 * @param base_ctx
 * @return pointer to mali surface is success, NULL if not 
 * @note ANDROID only:this is call if the native window is used as a media recorder and its format is set to HAL_PIXMAP_YV12 
 */
mali_surface *__egl_platform_create_recordable_surface_from_native_buffer(void *buffer, egl_surface *surface, void * base_ctx);	

/**
 * @brief check if color conversion is needed 
 * @param native_win android native window
 * @param surface the egl surface
 * @return EGL_TRUE if color conversion is needed, EGL_FALSE if not
 * @note ANDROID ONLY:color conversion is needed if the native window is used as a media recorder and its format is set to HAL_PIXMAP_YV12 
 */
EGLBoolean __egl_recordable_color_conversion_needed(void *native_win, egl_surface *surface);

/**
 * @brief convert color value to matching with the native window format 
 * @param native_buf android native buffer
 * @param render_target the source mali surface 
 * @param yuv_base virtual address to the native buffer 
 * @note basicly this function provides rgb8888->yv12 color coversion with non-accelerated CPU transfermation
 */
void __egl_platform_convert_color_space_to_native(void *native_buf, void *render_target, void *yuv_base);

/** @} */ /* end group egl_extension_recordable */

/** @} */ /* end group egl_extension */

#endif /* _EGL_RECORDABLE_ANDROID_H_ */

