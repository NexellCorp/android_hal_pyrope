/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_config.h
 * @brief EGL config handling
 */

#ifndef _EGL_CONFIG_H_
#define _EGL_CONFIG_H_

/**
 * @addtogroup eglcore EGL core API
 *
 * @{
 */

/**
 * @defgroup eglcore_config EGL core config API
 *
 * @note EGL core config API
 *
 * @{
 */

#include <EGL/mali_egl.h>
#include <shared/mali_pixel_format.h>
#include <base/mali_macros.h>
#include "egl_display.h"
#include "egl_config_extension.h"

/* This is used to fill the EGL_MATCH_FORMAT_KHR config value with EGL_NONE
 * for configs not supporting the EGL_LOCK_SURFACE_BIT_KHR in EGL_SURFACE_TYPE.
 * If the extension is not enabled this will be applied to an unused variable 
 * in the config structure.
 */
#define EGL_KHR_lock_surface_MATCH_FORMAT_NONE EGL_NONE

/**
 * The different rendering methods we can have.
 */
typedef enum egl_render_method
{
	RENDER_METHOD_NO_BLIT          = -1, /**< No blit/copy performed                             */
	RENDER_METHOD_BUFFER_BLIT      =  0, /**< Buffer blitting, copies src buffer into dst buffer */
	RENDER_METHOD_DIRECT_RENDERING =  1, /**< Direct rendering into dst buffer                   */
	RENDER_METHOD_SINGLE_BUFFERING =  2  /**< Single buffered direct rendering into dst buffer   */
} egl_render_method;


/**
 * The internal EGLConfig
 * @note Remember to update the static configs in egl_config.c whenever changing this struct.
 */
typedef struct egl_config
{
	EGLint buffer_size;
	EGLint red_size;
	EGLint green_size;
	EGLint blue_size;
	EGLint luminance_size;
	EGLint alpha_size;
	EGLint alpha_mask_size;
	EGLint bind_to_texture_rgb; /* boolean */
	EGLint bind_to_texture_rgba; /* boolean */
	EGLenum color_buffer_type;
	EGLint config_caveat; /* enum */
	EGLint config_id;
	EGLint conformant; /* whether contexts created with this config are conformant */
	EGLint depth_size;
	EGLint level;
	EGLint max_pbuffer_width;
	EGLint max_pbuffer_height;
	EGLint max_pbuffer_pixels;
	EGLint max_swap_interval;
	EGLint min_swap_interval;
	EGLint native_renderable; /* boolean */
	EGLint native_visual_id;
	EGLint native_visual_type;
	EGLint renderable_type;
	EGLint sample_buffers;
	EGLint samples;
	EGLint stencil_size;
	EGLint surface_type;
	EGLenum transparent_type;
	EGLint transparent_red_value;
	EGLint transparent_green_value;
	EGLint transparent_blue_value;
	mali_pixel_format pixel_format;
	egl_render_method render_method;
	EGLint match_format_khr;
    EGLint surface_scaling;
	EGLint recordable_android;
	EGLBoolean framebuffer_target_android;
} egl_config;

/**
 * @brief Initializes configurations for the given display
 * @param display_handle display for which to initialize configs
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_initialize_configs( EGLDisplay display_handle );

/**
 * @brief Looks for any config supporting the pixmap settings
 * @param display display for which to look for configs in
 * @param pixmap the native pixmap to find support for
 * @return EGL_TRUE on support, EGL_FALSE else
 */
EGLBoolean _egl_config_support_pixmap( egl_display *display, EGLNativePixmapType pixmap );

/**
 * @brief Returns the supported configurations on a given display
 * @param __dpy display handle
 * @param __configs returned array of config handles
 * @param config_size maximum number of configs to return
 * @param num_config updated with the number of actual configs found
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_get_configs( EGLDisplay __dpy, EGLConfig *__configs, EGLint config_size, EGLint *num_config, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Returns config handles that matches a given set of attributes
 * @param __dpy display handle
 * @param attrib_list NULL or EGL_NONE terminated list of wanted attributes
 * @param __configs returned array of config handles
 * @param config_size maximum number of configs to return
 * @param num_config updated with the number of actual configs found
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_choose_config(EGLDisplay __dpy, const EGLint *attrib_list, EGLConfig *__configs, EGLint config_size, EGLint *num_config, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Returns the value of an EGLConfig attribute
 * @param __dpy display handle
 * @param __config config to fetch attribute from
 * @param attribute EGLConfig attribute to fetch value of
 * @param value pointer to attribute value
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_get_config_attrib( EGLDisplay __dpy, EGLConfig __config, EGLint attribute, EGLint *value, void *thread_state ) MALI_CHECK_RESULT;

/** @} */ /* end group eglcore_config */

/** @} */ /* end group eglcore */

#endif /* _EGL_CONFIG_H_ */

