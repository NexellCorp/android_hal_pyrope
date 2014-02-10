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
 * @file egl_image_gles.h
 * @brief EGLImage extension functionality for GLES
 */

#ifndef _EGL_IMAGE_GLES_H_
#define _EGL_IMAGE_GLES_H_

/**
 * @addtogroup eglcore_image EGL core EGLImageKHR API
 *
 * @{
 */

/**
 * @defgroup eglcore_image_gles EGLImageKHR OpenGL ES API
 *
 * @note EGLImageKHR OpenGL ES API
 *
 * @{
 */

#include <EGL/mali_egl.h>
#include <EGL/mali_eglext.h>
#include <shared/mali_image.h>
#include <shared/m200_td.h>
#include "egl_image.h"

/**
 * Creates an EGLImage from a gles resource
 * @param display display to use
 * @param context context to use
 * @param target the GLES resource type
 * @param buffer the GLES resource id
 * @param attr_list attribute list
 * @param thread_state current thread state
 * @param buffer_data, any other buffer data needed, NULL if not needed
 * @return pointer to egl_image, NULL on failure
 */
egl_image* _egl_create_image_KHR_gles( egl_display *display,
                                       egl_context *context,
                                       EGLenum target,
                                       EGLClientBuffer buffer,
                                       EGLint *attr_list,
                                       void *thread_state,
                                       void *buffer_data);

/** @} */ /* end group eglcore_image */

/** @} */ /* end group eglcore_image_gles */

#endif /* _EGL_IMAGE_GLES_H_ */
