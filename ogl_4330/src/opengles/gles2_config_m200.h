/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_config_m200.h
 * @brief GLES2-specific flags for the GL driver
 */

#include "gles_config_m200.h"

#ifndef _GLES2_CONFIG_M200_H_
#define _GLES2_CONFIG_M200_H_

#define GLES2_MAX_VARYING_VECTORS MALI200_MAX_INPUT_STREAMS
#define GLES2_MAX_VERTEX_UNIFORM_VECTORS 256

/** Maximum number of elements */
#define GLES2_MAX_ELEMENTS_INDICES MALI_S32_MAX
#define GLES2_MAX_ELEMENTS_VERTICES MALI_U16_MAX

/** */
#define GLES2_MAX_TEXTURE_IMAGE_UNITS GLES_MAX_TEXTURE_UNITS

/** */
#define GLES2_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0

/** */
#define GLES2_MAX_COMBINED_TEXTURE_IMAGE_UNITS (GLES2_MAX_TEXTURE_IMAGE_UNITS + GLES2_MAX_VERTEX_TEXTURE_IMAGE_UNITS)

/** */
#define GLES2_MAX_FRAGMENT_UNIFORM_VECTORS 256

/* Shader Binary */
#define GLES2_NUM_SHADER_BINARY_FORMATS 1

/* Program Binary */
#define GLES2_NUM_PROGRAM_BINARY_FORMATS 1

/** the maximum bits in the colorbuffer */
#define GLES2_MAX_RED_BITS          8
#define GLES2_MAX_GREEN_BITS        8
#define GLES2_MAX_BLUE_BITS         8
#define GLES2_MAX_ALPHA_BITS        8

/** Maximum Viewport Dimensions */
#define GLES2_MAX_VIEWPORT_DIM_Y 4096
#define GLES2_MAX_VIEWPORT_DIM_X 4096

/** Subpixel precision */
#define GLES2_SUBPIXEL_BITS 4

#endif /* _GLES2_CONFIG_M200_H_ */
