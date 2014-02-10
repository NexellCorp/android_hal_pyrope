/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES1_CONFIG_EXTENSION_M200_H_
#define _GLES1_CONFIG_EXTENSION_M200_H_

#include "gles_config_extension_m200.h"

#if (!defined(USING_MALI200) && !defined(USING_MALI400) && !defined(USING_MALI450)) || !MALI_USE_GLES_1
	#error "Extension file for GLES 1.x on Mali200/300/400/450 included, but none of the cores or the GLES 1.x API is selected\n"
#endif

/** Core-extensions */
#define EXTENSION_BYTE_COORDINATES_OES_ENABLE				1
#define EXTENSION_FIXED_POINT_OES_ENABLE				1
#define EXTENSION_SINGLE_PRECISION_ENABLE				1
#define EXTENSION_MATRIX_GET_OES_ENABLE					1
#define EXTENSION_TEXTURE_CUBE_MAP_ENABLE				1

/** Required-extensions */
#define EXTENSION_READ_FORMAT_OES_ENABLE				1
#define EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE		1
#define EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE		1
#define EXTENSION_POINT_SPRITE_OES_ENABLE				1
#define EXTENSION_POINT_SIZE_ARRAY_OES_ENABLE				1

/** Optional-extensions */
#define EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE				1
#define EXTENSION_DEPTH24_OES_ENABLE					1
#define EXTENSION_ARM_RGBA8_ENABLE					1
#define EXTENSION_BGRA8888_ENABLE					1
#define EXTENSION_PALETTE_MATRIX_OES_ENABLE				1
#define EXTENSION_DRAW_TEX_OES_ENABLE					1
#define EXTENSION_ROBUSTNESS_EXT_ENABLE                                 1
#if !defined MALI_USE_GLES_2 || !MALI_USE_GLES_2
	#define EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE			0
	#define EXTENSION_TEXTURE_HALF_FLOAT_LINEAR_OES_ENABLE		0
#endif

/** OES-extensions */
#define EXTENSION_EXTENDED_MATRIX_PALETTE_OES_ENABLE			1
#define EXTENSION_OES_EGL_IMAGE_EXTERNAL				1

/** Extension dependencies */
#if EXTENSION_FIXED_POINT_OES_ENABLE
	/* Query-matrix is dependent on fixed-point extension */
	#define EXTENSION_QUERY_MATRIX_OES_ENABLE			1
#endif

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	/* Depth textures are just glorified luminance textures unless FBOs are enabled */
	#define EXTENSION_DEPTH_TEXTURE_ENABLE				1
	#define EXTENSION_PACKED_DEPTH_STENCIL_ENABLE			1
	#if RGB_IS_XRGB
		#define EXTENSION_RGB8_RGBA8_ENABLE                  	1
	#endif
	#define EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE		1
	#define EXTENSION_DISCARD_FRAMEBUFFER                1
#endif


#endif /* _GLES1_CONFIG_EXTENSION_M200_H_ */
