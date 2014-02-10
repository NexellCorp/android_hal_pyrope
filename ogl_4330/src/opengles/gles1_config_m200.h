/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES1_CONFIG_M200_H_
#define _GLES1_CONFIG_M200_H_

#include "gles_config_m200.h"

/** the maximum number of user defined clip planes */
#define GLES1_MAX_CLIP_PLANES       1

/** the maximum amount of lights */
#define GLES1_MAX_LIGHTS            8

/** modelview matrix stack depth */
#define GLES1_MODELVIEW_MATRIX_STACK_DEPTH  32
/** projection matrix stack depth */
#define GLES1_PROJECTION_MATRIX_STACK_DEPTH 32
/** texture  matrix stack depth */
#define GLES1_TEXTURE_MATRIX_STACK_DEPTH    32

/** Maximum number of elements */
#define GLES1_MAX_ELEMENTS_INDICES MALI_S32_MAX
#define GLES1_MAX_ELEMENTS_VERTICES MALI_U16_MAX

/** Maximum number of palette-matrices */
#define GLES1_MAX_PALETTE_MATRICES_OES 32

/** Maximum number of matrices per vertex */
#define GLES1_MAX_VERTEX_UNITS_OES 4

/** max lod bias. (EXT_texture_lod_bias) in GLfixed format */
#define GLES1_MAX_TEXTURE_LOD_BIAS -1

/** Maximum Viewport Dimensions */
#define GLES1_MAX_VIEWPORT_DIM_Y 4096
#define GLES1_MAX_VIEWPORT_DIM_X 4096

/** Range of the size of smooth points */
#define GLES1_SMOOTH_POINT_SIZE_MIN GLES_POINT_SIZE_MIN
#define GLES1_SMOOTH_POINT_SIZE_MAX GLES_POINT_SIZE_MAX

/** Range of the width of smooth lines */
#define GLES1_SMOOTH_LINE_WIDTH_MIN 0.25f
#define GLES1_SMOOTH_LINE_WIDTH_MAX 100.0f

/** Subpixel precision */
#define GLES1_SUBPIXEL_BITS 4

#endif /* _GLES1_CONFIG_M200_H_ */

