/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_M200_DRAW_COMMON_H
#define GLES_M200_DRAW_COMMON_H

#include <mali_rsw.h>
#include <base/mali_memory_types.h>

/**
 * @brief Set some common states info rsw
 * @param rsw  The rsw will be set
 *
 */
void _gles_fb_setup_rsw_const(m200_rsw *rsw);

/**
 * @brief Set shader information into rsw
 * @param rsw The rsw will be set
 * @param shader_mem mali address of the shader
 * @param first_instruction_length 
 */
void _gles_fb_setup_rsw_shader(m200_rsw *rsw, mali_addr shader_mem, int first_instruction_length);

#endif
