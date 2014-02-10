/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_config_extension.h
 * @brief Global configuration extensions of the GLES driver
 */

#ifndef _GLES_CONFIG_EXTENSION_H_
#define _GLES_CONFIG_EXTENSION_H_

#define GL_GLEXT_PROTOTYPES

#if MALI_USE_GLES_1
	#include "gles1_config_extension_m200.h"
#endif

#if MALI_USE_GLES_2
	#include "gles2_config_extension_m200.h"
#endif

#if !MALI_USE_GLES_1 && !MALI_USE_GLES_2
	#error "Neither OpenGL ES 1.1 nor OpenGL ES 2.0 was defined\n"
#endif

#endif /* _GLES_CONFIG_EXTENSION_H_ */
