/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES2_M200_SHADERS_H
#define GLES2_M200_SHADERS_H

#include "shared/m200_gp_frame_builder.h"
#include "shared/binary_shader/bs_object.h"

/**
 * @brief Adds the vertex and fragment shaders in the PRS to the frame, ensuring they live long enough. 
 *        Also updates the fragment shader stack to match the program. 
 * @param prs The current program rendering state, holding one valid program
 * @param frame_builder The frame builder associated with this draw call
 */
mali_err_code MALI_CHECK_RESULT _gles_m200_update_shader( struct gles_program_rendering_state *prs, mali_frame_builder *frame_builder );

#endif /* GLES2_M200_SHADERS_H */

