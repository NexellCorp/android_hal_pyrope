/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_M200_UNIFORMS_H
#define GLES_M200_UNIFORMS_H

#include "shared/binary_shader/bs_object.h"

/**
 * @brief Copies the fragment uniform array into mali memory and assigns it to the current frame as the verte shader constant register
 * @param frame_pool A pointer to the frame_pool to use for memory allocations
 * @param ctx The context
 * @param prs the program rendering state that contains cache information for current uniforms.
 * @return MALI_ERR_NO_ERROR if everything went well, MALI_ERR_OUT_OF_MEMORY if out of memory.
 * @note The allocated mali memory will adhere to the size requirements of M200.
 */

mali_err_code MALI_CHECK_RESULT _gles_m200_update_fragment_uniforms( mali_mem_pool *frame_pool, struct gles_context *ctx, struct gles_program_rendering_state* prs);

#endif /* GLES_M200_UNIFORMS_H */
