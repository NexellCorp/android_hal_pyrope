/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES2_CONFIG_EXTENSION_M200_H_
#define _GLES2_CONFIG_EXTENSION_M200_H_

#include "gles_config_extension_m200.h"

#if (!defined(USING_MALI200) && !defined(USING_MALI400) && !defined(USING_MALI450)) || !MALI_USE_GLES_2
	#error "Extension file for GLES 2.x on Mali200/300/400/450 included, but either the core or the GLES 2.x API is not selected\n"
#endif

#define EXTENSION_FRAGMENT_SHADER_DERIVATIVE_HINT_OES_ENABLE 1
#define EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE              1
#define EXTENSION_DEPTH24_OES_ENABLE                         1
#define EXTENSION_ARM_MALI_SHADER_BINARY_ENABLE              1
#define EXTENSION_DEPTH_TEXTURE_ENABLE                       1
#define EXTENSION_PACKED_DEPTH_STENCIL_ENABLE                1
#define EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE    1
#define EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE              0
#define EXTENSION_TEXTURE_HALF_FLOAT_LINEAR_OES_ENABLE       0
#define EXTENSION_BLEND_MINMAX_EXT_ENABLE                    1
#define EXTENSION_OES_EGL_IMAGE_EXTERNAL                     1
#define EXTENSION_VIDEO_CONTROLS_ARM_ENABLE                  0
#if !defined(__SYMBIAN32__)
#define EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE              1
#else
#define EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE              0
#endif

#if RGB_IS_XRGB
	#define EXTENSION_RGB8_RGBA8_ENABLE                  1
#endif

#if !defined(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES) && defined(GL_FRAGMENT_SHADER_DERIVATIVE_HINT)
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES GL_FRAGMENT_SHADER_DERIVATIVE_HINT
#endif

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	#define EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE     1
	#define EXTENSION_DISCARD_FRAMEBUFFER                1
#endif

#define EXTENSION_EXT_SHADER_TEXTURE_LOD                     1
#define EXTENSION_ROBUSTNESS_EXT_ENABLE                      1

#endif /* _GLES2_CONFIG_EXTENSION_M200_H_ */
