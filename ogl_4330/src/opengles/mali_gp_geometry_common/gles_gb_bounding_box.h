/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#ifndef GLES_GB_BOUNDING_BOX_H
#define GLES_GB_BOUNDING_BOX_H

#include "../gles_context.h"
#include "../gles_gb_interface.h"

#ifdef MALI_BB_TRANSFORM

typedef enum {
	BB_INPUT_STREAM = 0,
	BB_MVP_MATRIX,
	BB_OUTPUT_STREAM,
	BB_CONSTANT_VALUE_1, /* constant values we use in our patterns*/
	BB_CONSTANT_VALUE_2,
	BB_INPUT_STREAM_STRIDE,
	BB_NUM_VERTICES,   /* number of vertices to transform*/

	/* clip bits output*/
	BB_CLIP_BITS_AND_OUTPUT,
	BB_CLIP_BITS_OR_OUTPUT,
	
	BB_NEON_MAX_INPUT
} gles_bb_input_enum;

typedef u32 gles_bb_input[BB_NEON_MAX_INPUT];

/**
 * @brief Packs input parameter for bounding box transformation function
 * @param input  - integer array is being used for transform function input
 * @param name - one of the values from the gles_bb_input_enum enum
 * @param value - exact value to pack
 */
MALI_STATIC_INLINE  void _gles_pack_bbt_input_parameter(gles_bb_input input, u32 name, u32 value) 
{
    MALI_DEBUG_ASSERT( name < BB_NEON_MAX_INPUT, ("neon input index is outside the bounds"));

    input[name] = value;
}

/**
 * @brief Attempt to reject entire drawcall
 * @param vs_range_buffer A pointer to a buffer that can be used to fill in new ranges for this drawcall
 * @param ctx gles context
 * @param bb_has_collision (output parameter) if MALI_FALSE we can reject entrire drawcall, otherwise we should proceed with it
 */
mali_err_code _gles_gb_try_reject_drawcall(  struct gles_context *ctx,
											 const index_range *vs_range_buffer,
											 mali_bool* bb_has_collision );
#endif 

#endif
