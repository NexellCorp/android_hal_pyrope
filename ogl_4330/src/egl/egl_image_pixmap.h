/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_image_pixmap.h
 * @brief EGLImage extension functionality for Pixmaps
 */

#ifndef _EGL_IMAGE_PIXMAP_H_
#define _EGL_IMAGE_PIXMAP_H_

/**
 * @addtogroup eglcore_image EGL core EGLImageKHR API
 *
 * @{
 */

/**
 * @defgroup eglcore_image_pixmap EGLImageKHR Native pixmap API
 *
 * @note EGLImageKHR Native pixmap API
 *
 * @{
 */

#include <EGL/mali_egl.h>
#include <EGL/mali_eglext.h>
#include <shared/mali_image.h>
#include <shared/m200_td.h>
#include "egl_image.h"

/**
 * Creates an EGLImage from a native pixmap
 * @param display display to use
 * @param context context to use
 * @param buffer the native pixmap resource
 * @param attr_list attribute list
 * @param thread_state current thread state
 * @return pointer to egl_image, NULL on failure
 */
egl_image* _egl_create_image_KHR_pixmap( egl_display *display,
                                         egl_context *context,
                                         EGLClientBuffer buffer,
                                         EGLint *attr_list,
                                         void *thread_state );

/** @} */ /* end group eglcore_image */

/** @} */ /* end group eglcore_image_pixmap */

#endif /* _EGL_IMAGE_PIXMAP_H_ */
