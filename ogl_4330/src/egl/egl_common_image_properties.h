/*
 * This confidential and proprietary software may be used only as
 * authorized by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorized
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_common_image_properties.h
 * @brief Common include file for EGLImage extension.
 */

#ifndef _EGL_COMMON_IMAGE_PROPS_
#define _EGL_COMMON_IMAGE_PROPS_
/** EGLImage properties, this is here as it is needed both by EGL_IMAGE_SYSTEM and EGL_IMAGE */
typedef struct egl_image_properties
{
	EGLenum data_type;
	EGLenum element_type;
	EGLint num_components;
	EGLenum compression_type;
	EGLint min_bits[4];
	EGLint min_samples;
	EGLenum image_format;
	EGLenum colorspace;
	EGLenum colorrange;
	EGLenum alpha_format;
	EGLenum premult_order;
	EGLint opengl_usage;
	EGLint openvg_usage;
	/* NOTE: WFD and MAX is not supported */

	EGLint miplevels; /* Maximum number of mipmap levels in the eglImage */
	EGLint layout;    /* Layout of the image */
} egl_image_properties;

#endif /* _EGL_COMMON_IMAGE_PROPS_ */
