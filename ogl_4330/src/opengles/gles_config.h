/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_config.h
 * @brief Global configurations of the GLES driver
 */

#ifndef _GLES_CONFIG_H_
#define _GLES_CONFIG_H_

#if !MALI_USE_GLES_1 && !MALI_USE_GLES_2
	#error "MALI_USE_GLES_1 or MALI_USE_GLES_2 needs to be defined; try running make from the top level"
#endif

#if USING_MALI200
	#include "gles_config_m200.h"
#endif

/* Include version specific settings */
#if MALI_USE_GLES_1
	#include "gles1_config_m200.h"
	#include "gles1_config_extension_m200.h"
#endif

#if MALI_USE_GLES_2
	#include "gles2_config_m200.h"
	#include "gles2_config_extension_m200.h"
#endif

#define GLES_MAX_MIPMAP_LEVELS (GLES_MAX_TEXTURE_SIZE_LOG2 + 1)

#endif /* _GLES_CONFIG_H_ */
