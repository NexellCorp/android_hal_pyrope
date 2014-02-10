/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_system.h"
#include "base/mali_debug.h"
#include "base/mali_memory.h"
#include "base/gp/mali_gp_job.h"

#include <gles_util.h>

#include "regs/MALIGP2/mali_gp_vs_config.h" /* needed for stream-config */
#include "shared/binary_shader/bs_symbol.h"

#include "gles_geometry_backend_context.h"
#include "gles_instrumented.h"
#include "gles_gb_rendering_state.h"

MALI_STATIC void _gles_gb_setup_position_stream(const gles_gb_context *gb_ctx, u32 *streams, const int position_stream, u32 *stride_array)
{
	MALI_DEBUG_ASSERT_POINTER( gb_ctx );
	MALI_DEBUG_ASSERT_POINTER( streams );
	MALI_DEBUG_ASSERT_POINTER( stride_array );
	MALI_DEBUG_ASSERT_RANGE(position_stream, 0, (int)gb_ctx->prs->binary.vertex_pass.flags.vertex.num_output_registers);

    /* setup position stream */
    MALI_DEBUG_ASSERT(0 != gb_ctx->position_addr, ("No position buffer set up"));
    streams[GP_VS_CONF_REG_STREAM_ADDR(position_stream )] = _SWAP_ENDIAN_U32_U32(gb_ctx->position_addr + (4 * 4) * gb_ctx->parameters.output_buffer_offset);
    /* GP_VS_VSTREAM_FORMAT_VDATA_BLOCK -Vertex coordinates(XYZW) in FP32 format, stride set to 4*4 */
    streams[GP_VS_CONF_REG_STREAM_SPEC(position_stream )] = _SWAP_ENDIAN_U32_U32(GP_VS_VSTREAM_FORMAT_VDATA_BLOCK | ((4 * 4) << MALI_STRIDE_SHIFT));
    stride_array[position_stream] = 16 /*  4*4 */;
}

MALI_STATIC MALI_CHECK_RESULT mali_err_code _gles_gb_setup_pointsize_stream(gles_gb_context *gb_ctx, u32 *streams, const int pointsize_stream, u32 *stride_array)
{
	MALI_DEBUG_ASSERT_POINTER( gb_ctx );
	MALI_DEBUG_ASSERT_POINTER( streams );
	MALI_DEBUG_ASSERT_POINTER( stride_array );

	/* only setup stream if actually needed */
	if ( gb_ctx->parameters.mode == GL_POINTS)
	{
		/* pointsize must be written to in the current vertex shader (-1 means not used) */
		if (pointsize_stream >= 0)
		{
			/* Allocate point size buffer and add it to the plbu job list, that way it will be release on plbu callback */
			void* point_size_cpu_ptr;
            u32 point_size_addr;

			MALI_DEBUG_ASSERT_RANGE(pointsize_stream, 0, (int)gb_ctx->prs->binary.vertex_pass.flags.vertex.num_output_registers);

			/* NOTE we are over-allocating slightly due to HARDWARE_ISSUE_6973 - the point size stream will be written
			        in units of 16 bytes */
			point_size_cpu_ptr = _mali_mem_pool_alloc(gb_ctx->frame_pool, gb_ctx->parameters.vertex_count * sizeof( gles_fp32 ) + 3 * sizeof( gles_fp32 ), &point_size_addr);	
			if( point_size_cpu_ptr == NULL )  return MALI_ERR_OUT_OF_MEMORY;
			gb_ctx->point_size_addr = point_size_addr;

			streams[GP_VS_CONF_REG_STREAM_ADDR(pointsize_stream)] = _SWAP_ENDIAN_U32_U32(point_size_addr);

#if HARDWARE_ISSUE_6973
			if ( MALI_TRUE == gb_ctx->parameters.indexed )
			{
				streams[GP_VS_CONF_REG_STREAM_SPEC(pointsize_stream)] = _SWAP_ENDIAN_U32_U32(GP_VS_VSTREAM_FORMAT_1_FP32 | (4 << MALI_STRIDE_SHIFT));
			} else
#endif
			{
				streams[GP_VS_CONF_REG_STREAM_SPEC(pointsize_stream)] = _SWAP_ENDIAN_U32_U32(GP_VS_VSTREAM_FORMAT_POINT_SIZE | (4 << MALI_STRIDE_SHIFT));
			}

            stride_array[pointsize_stream] = sizeof( gles_fp32);
		} else {
			MALI_INSTRUMENTED_LOG((_instrumented_get_context() , MALI_LOG_LEVEL_WARNING, "Undefined Pointsize - not written to by vertex shader"));
		}

	}
	return MALI_ERR_NO_ERROR;
}

MALI_STATIC void _gles_gb_setup_varying_stream_base(const gles_gb_context *gb_ctx, u32 *streams, const bs_program *prog_binary_state, u32 *stride_array)
{
	unsigned int i;
	int block_offset;
	u32* vstream_from_prs;
	MALI_DEBUG_ASSERT_POINTER( gb_ctx );
	MALI_DEBUG_ASSERT_POINTER( streams );
	MALI_DEBUG_ASSERT_POINTER( prog_binary_state );
	MALI_DEBUG_ASSERT_POINTER( stride_array );
	MALI_DEBUG_ASSERT_RANGE((s32)prog_binary_state->varying_streams.count, 0, GP_OUTPUT_STREAM_COUNT - 1);

	vstream_from_prs = gb_ctx->prs->gb_prs->varying_streams;
	
	MALI_DEBUG_ASSERT_POINTER( vstream_from_prs );

	block_offset = prog_binary_state->varying_streams.block_stride * gb_ctx->parameters.output_buffer_offset;

    /* calculate output stream stride type and address, */
    for (i = 0; i < prog_binary_state->varying_streams.count; ++i)
    {
        streams[GP_VS_CONF_REG_STREAM_ADDR(i)] = 
            _SWAP_ENDIAN_U32_U32(
                    vstream_from_prs[ GP_VS_CONF_REG_OUTP_ADDR(i)] /* this is the current "streams" value after pushing the PRS, 
                                                                                                    but it's faster to write it again than reading from 
                                                                                                    uncached memory by using a += */
                    + (u32)gb_ctx->varyings_addr + block_offset   /* add the base adress */
                    );
        stride_array[i] = prog_binary_state->varying_streams.block_stride;
    }
}

mali_err_code _gles_gb_setup_output_streams(gles_gb_context *gb_ctx, u32 *streams, u32 *stride_array)
{
	mali_err_code err;
	const bs_program *prog_binary_state;
	MALI_DEBUG_ASSERT_POINTER( gb_ctx );
	MALI_DEBUG_ASSERT_POINTER( gb_ctx->prs );
	prog_binary_state = &gb_ctx->prs->binary;
	MALI_DEBUG_ASSERT_POINTER( streams );
	MALI_DEBUG_ASSERT_POINTER( prog_binary_state );
	MALI_DEBUG_ASSERT_POINTER( stride_array );

	/* setup output streams */
	_gles_gb_apply_program_rendering_state_output_streams( streams, gb_ctx->prs->gb_prs );

	_gles_gb_setup_position_stream(gb_ctx, streams, prog_binary_state->vertex_pass.flags.vertex.position_register, stride_array);
	err = _gles_gb_setup_pointsize_stream((gles_gb_context *)gb_ctx, streams, prog_binary_state->vertex_pass.flags.vertex.pointsize_register, stride_array);
	if (MALI_ERR_NO_ERROR != err)	return err;

	_gles_gb_setup_varying_stream_base(gb_ctx, streams, prog_binary_state, stride_array);

	return MALI_ERR_NO_ERROR;
}
