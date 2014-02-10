/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_config_m200.h
 * @brief Flags for the GL driver that are common between gles 1.1 and 2.0.
 */

#ifndef _GLES_CONFIG_M200_H_
#define _GLES_CONFIG_M200_H_

#include <regs/MALI200/mali200_core.h>
#include <regs/MALIGP2/mali_gp_core.h>
#include "base/mali_types.h"

/** Maximum number of vertex attributes (min 16) */
#if HARDWARE_ISSUE_4326
	#if MALI_USE_GLES_1
		/* HW issue 4326 ruins a lot of streams, leaving us with only 2 TU's in GLES11/M200 r0p1 */
		#define GLES_MAX_TEXTURE_UNITS   2
	#else
		/* GLES2 only builds doesn't care, is only limited by the amount of attributes */
		#define GLES_MAX_TEXTURE_UNITS   8
	#endif
	#define GLES_VERTEX_ATTRIB_COUNT (MALIGP2_MAX_VS_INPUT_REGISTERS/2)
#else
	#define GLES_MAX_TEXTURE_UNITS   8
	/* hardware issue 8733 may corrupt the last stream. As a workaround, we add a dummy stream to be corrupted,
	 * but this fix leaves one less stream available. */
	#if HARDWARE_ISSUE_8733
		#define GLES_VERTEX_ATTRIB_COUNT (MALIGP2_MAX_VS_INPUT_REGISTERS-1)
	#else
		#define GLES_VERTEX_ATTRIB_COUNT MALIGP2_MAX_VS_INPUT_REGISTERS
	#endif
#endif

/** Maximum number of vertex attributes (min 16) */
#define GLES_SHADER_GENERATOR_VERTEX_ATTRIB_COUNT MALIGP2_MAX_VS_INPUT_REGISTERS

/** the maximum dimensions of a texture */
#define GLES_MAX_TEXTURE_SIZE_LOG2 MALI200_MAX_TEXTURE_SIZE_LOG2
#define GLES_MAX_TEXTURE_SIZE MALI200_MAX_TEXTURE_SIZE

/** the maximum samples of a multisampled FBO attachment */
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	#define GLES_MAX_FBO_SAMPLES 4
	#define GLES_SUPPORTED_FBO_SAMPLES(samples) ( ((samples)==0) || ((samples)==4) )
	#define GLES_NEXT_SUPPORTED_FBO_SAMPLES(samples) ( (samples>0) ? (4) : (0) )
#endif

/** the maximum bits in the stencil buffer */
#define GLES_MAX_STENCIL_BITS      8
#define GLES_MAX_STENCIL_MASK      ( ( 1 << GLES_MAX_STENCIL_BITS ) - 1 )

/** Max cube map texture size*/
#define GLES_MAX_CUBE_MAP_TEXTURE_SIZE 1024

/** Range of the width of aliased lines */
#define GLES_ALIASED_LINE_WIDTH_MIN 1.0f
#define GLES_ALIASED_LINE_WIDTH_MAX 100.0f

/** min and max point sizes */
#define GLES_POINT_SIZE_MIN 1.0f
#define GLES_POINT_SIZE_MAX 100.0f

/** Range of the size of aliased points */
#define GLES_ALIASED_POINT_SIZE_MIN 1.f
#define GLES_ALIASED_POINT_SIZE_MAX 100.0f

/** Framebuffer related defines */
#define GLES_MAX_RENDERBUFFER_SIZE 4096 /* max size of a framebuffer */

/** Stride bit shift used in GP_VS_CONF_REG_INP_SPEC(X) Register */
#define MALI_STRIDE_SHIFT (11)

/** Number of bits to encode Stride in GP_VS_CONF_REG_INP_SPEC(X) Register */
#define MALI_STRIDE_BITS (20)

/** Mask used to mask out bits that do not fit into the encoding size
 * of Stride in GP_VS_CONF_REG_INP_SPEC(X) Register
 */
#define MALI_STRIDE_MASK ((1 << (MALI_STRIDE_BITS)) - 1)

/* The TD has 11 miplevel pointers in it. Level 0-10 inclusive.  */
#define MALI_TD_MIPLEVEL_POINTERS 11

/* Calculated these values with gles2_performance suite,
 *   with 128x128 textures HW is 8% slower than sw,
 *   with 256x256+ textures HW is almost twice as fast.
 * So taking a value of 150x150 as the threshold
 */
#define GLES_HW_SWIZZLE_THRESHOLD ( 256 * 256 )
#define GLES_HW_MIPGEN_THRESHOLD ( 32*32 )

/** Implementation type/format allowed through glReadPixels */
#define GLES_IMPLEMENTATION_COLOR_READ_TYPE GL_UNSIGNED_SHORT_5_6_5
#define GLES_IMPLEMENTATION_COLOR_READ_FORMAT GL_RGB


/** The addresses for mipmap 11 and upwards are not explicitly specified; instead they are computed as follows:
    Address(mipmap n) = Address(mipmap 10) + (n-10)*12288 bytes for cubemaps and 3d-textures */

#define GLES_SPACING_BETWEEN_MIPMAPS_CUBEMAPS_TARGETS 12288

/** Spacing is 1024 bytes for other textures. */
#define GLES_SPACING_BETWEEN_MIPMAPS_OTHER_TARGETS 1024

/* 
 * We know that we run OOM when trying to draw more than 1 million tringles, 
 * so we trigger an incremental rendering for half million 
 */
#define GLES_TRIANGLES_DRAWN_BEFORE_INCREMENTAL_RENDERING 0x1B0000

#endif /* _GLES_CONFIG_M200_H_ */
