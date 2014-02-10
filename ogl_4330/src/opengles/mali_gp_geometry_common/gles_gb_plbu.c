/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <base/mali_debug.h>
#include <base/mali_memory.h>

#include <base/gp/mali_gp_job.h>
#include <regs/MALIGP2/mali_gp_plbu_config.h> /* needed for stream-config */

#include <gles_util.h>
#include <gles_gb_interface.h>

#include "gles_geometry_backend.h"
#include "gles_geometry_backend_context.h"
#include "../gles_buffer_object.h"
#include "gles_gb_buffer_object.h"
#include "gles_instrumented.h"
#include "../gles_framebuffer_object.h"
#include "shared/m200_gp_frame_builder_inlines.h"


mali_err_code MALI_CHECK_RESULT _gles_gb_plbu_arrays_semaphore_begin( gles_gb_context *gb_ctx )
{
	mali_gp_job_handle gp_job;

	MALI_DEBUG_ASSERT_POINTER(gb_ctx);
	MALI_DEBUG_ASSERT_POINTER( gb_ctx->frame_builder );
	MALI_DEBUG_ASSERT( MALI_FALSE==gb_ctx->parameters.indexed, ("Invalid draw mode" ) );

	gp_job = _mali_frame_builder_get_gp_job( gb_ctx->frame_builder );
	MALI_DEBUG_ASSERT_POINTER( gp_job );

	return _mali_gp_job_add_plbu_cmd( gp_job, GP_PLBU_COMMAND_WAIT_DEC_SEM(  2, 1, 0 ) );
}

mali_err_code MALI_CHECK_RESULT _gles_gb_plbu_arrays_semaphore_end( gles_gb_context *gb_ctx )
{
	mali_gp_job_handle gp_job;

	MALI_DEBUG_ASSERT_POINTER(gb_ctx);
	MALI_DEBUG_ASSERT_POINTER( gb_ctx->frame_builder );
	MALI_DEBUG_ASSERT( MALI_FALSE==gb_ctx->parameters.indexed, ("Invalid draw mode" ) );

	gp_job = _mali_frame_builder_get_gp_job( gb_ctx->frame_builder );
	MALI_DEBUG_ASSERT_POINTER( gp_job );

	return _mali_gp_job_add_plbu_cmd( gp_job, GP_PLBU_COMMAND_WAIT_DEC_SEM(  1, 1, 0 ) );
}

mali_err_code MALI_CHECK_RESULT _gles_gb_plbu_setup_points_lines(	struct gles_context *const ctx,
																	mali_gp_plbu_cmd * const buffer,
																	u32 * const index, const u32 buffer_len );

mali_err_code MALI_CHECK_RESULT _gles_gb_setup_plbu_scissor(	struct gles_context *const ctx, gles_gb_context *gb_ctx,
																mali_gp_plbu_cmd * const buffer,
																u32 * const index, const u32 buffer_len);

mali_err_code MALI_CHECK_RESULT _gles_gb_plbu_setup_draw(	struct gles_context *const ctx, 
															mali_gp_plbu_cmd * const buffer,
															u32 * const index, const u32 buffer_len );

MALI_STATIC_INLINE void _gles_gb_setup_plbu_get_cull_flags( gles_gb_context *gb_ctx, u32 *cull_ccw, u32 *cull_cw)
{
	*cull_ccw = 0;
	*cull_cw = 0;

	/* Set up backface culling */
	if( gb_ctx->parameters.cull_face == GL_TRUE )
	{
		switch (gb_ctx->parameters.cull_face_mode)
		{
			case GL_FRONT:
				if( gb_ctx->parameters.front_face == GL_CCW ) *cull_cw = 1;
				else  *cull_ccw = 1;
			break;

			case GL_BACK:
				if( gb_ctx->parameters.front_face == GL_CCW ) *cull_ccw = 1;
				else *cull_cw = 1;
			break;

			case GL_FRONT_AND_BACK:
				*cull_ccw = 1;
				*cull_cw = 1;
				MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING, "Culling both frontface and backface"));
			break;

			default:
				MALI_DEBUG_ASSERT( MALI_FALSE, ("unsupported cull-mode"));
		}
	}
}

MALI_STATIC u32 _gles_gb_setup_plbu_config( gles_gb_context *gb_ctx )
{
	u32 temp_conf_reg;
	u32 cull_ccw = 0;
	u32 cull_cw = 0;
	u32 point_clipping_bounding_box = MALI_FALSE;

	if(gb_ctx->parameters.indexed == MALI_TRUE)
	{
		if ( GL_UNSIGNED_BYTE == gb_ctx->parameters.index_type )
		{
			temp_conf_reg = 0 | ( 0 << 10 );
		} else
		{
			temp_conf_reg = 0 | ( 1 << 10 );
		}
	} else
	{
		temp_conf_reg = 0;
	}

	_gles_gb_setup_plbu_get_cull_flags( gb_ctx, &cull_ccw, &cull_cw );

	MALI_DEBUG_ASSERT( !( MALI_TILELIST_CHUNK_FORMAT > 3), ("illegal list chunk format \n") );

	/* Setup PLBU config register, ( 1 << 13 ) is setting point-clipping to use bounding-box instead of center */
	if ( GLES_API_VERSION_2 == gb_ctx->api_version )
	{
#if MALI_USE_GLES_2
		/* Enable bound-box clipping on points, compared to 0 which means clipping based on center of point */
		point_clipping_bounding_box = MALI_TRUE;
#endif /* MALI_USE_GLES_2 */
	}

	temp_conf_reg |= ( GLES_GB_RSW_INDEX_ZERO |
                     ( MALI_TILELIST_CHUNK_FORMAT << 8) |
                     ( gb_ctx->parameters.point_size_override << 12 ) |
                     ( cull_ccw << 17 ) | ( cull_cw << 18 )  | ( point_clipping_bounding_box << 13 ) );

	return temp_conf_reg;
}

/**
 * Common PLBU command list prologue
 *
 * @param         gb_ctx        Pointer to the geometry backend context
 * @param         common_state  Pointer to the state flags
 * @param         gp_job        Handle to the gp job
 * @param[in,out] buffer        Pointer to an array of PLBU commands
 * @param[in,out] index         Index of first free entry in the array of PLBU commands
 * @param         buffer_len    The total number of elements in #buffer
 * @param scissor_boundaries [out] - the return value of this function: an array for storing scissor's left, right, bottom, top values
 * @param viewport_dimensions [out] - the return value of this function: an array for storing viewport's left, right, bottom, top values
 */
MALI_STATIC_INLINE mali_err_code MALI_CHECK_RESULT _gles_gb_plbu_setup_common( struct gles_context *const ctx,
									gles_gb_context *const gb_ctx,
									gles_common_state * const common_state,
									mali_gp_plbu_cmd * const buffer, u32 * const index,
									const u32 buffer_len)
{
	u32 idx;

	MALI_DEBUG_ASSERT_POINTER( gb_ctx );
	MALI_DEBUG_ASSERT_POINTER( index );
	MALI_DEBUG_ASSERT_POINTER( buffer );

	idx = *index;

	MALI_DEBUG_ASSERT( idx < buffer_len, ( "plbu stream buffer overflow" ) );

	{
		u32 dirty_bits;
		mali_bool this_call_is_triangle, prev_call_is_triangle;
		mali_base_frame_id frameid;

		MALI_DEBUG_ASSERT_POINTER( common_state );
		MALI_DEBUG_ASSERT_POINTER( gb_ctx );

		/* Unconditional since we expect these to change
		 * frequently.
		 */
		{
			const u32 temp_conf_reg = _gles_gb_setup_plbu_config( gb_ctx );

			buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( temp_conf_reg, GP_PLBU_CONF_REG_PARAMS );

			/* Setup RSW and vertex base addresss
			 */
			buffer[idx++] =
			    GP_PLBU_COMMAND_RSW_VERTEX_ADDR( gb_ctx->parameters.rsw_base_addr,
			                                     gb_ctx->parameters.vertex_array_base_addr );
		}

		/**
		 * Scissor and viewport and depth parameter setup in the PLBU is much of the same thing:
		 * It's all state that must be set in the PLBU before we can accept any drawcalls. 
		 *
		 * Updates to these states need to happen on a new frame 
		 * Updates to these states need to happen if glScissor, glViewport or glDepthRange respectively is called
		 * The scissor bit also need to be updated if glEnable/glDisableScissor is set 
		 *    (as that is HW-functionally equivalent of setting a scissorbox equal to the screen size)
		 * 
		 * Also: we need to update the viewport when switching between TRIANGLES and non-TRIANGLE draw modes. 
		 * And if we are in a line mode, we need to update the viewport every time we change the line width (bugzilla 11282).
		 *
		 * We could set these every drawcall, but that's too slow. So the driver employs a few DIRTY bits. 
		 *
		 * MALI_STATE_PLBU_FRAME_OR_DEPTH_PENDING is set on glDepthRange calls, and is checked and cleared here. 
		 * MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING is set on glViewport calls, and is checked and cleared here.
		 * MALI_STATE_SCISSOR_DIRTY is set on glScissor and glEnable/glDisable(GL_SCISSOR), and is checked and cleared here. 
		 *
		 * Alone this is not quite enough. We also need a test for whether the frame ID has changed. This will change on a 
		 * glBindFramebuffer, and in case we are working on an FBO with the aggressive flush mechanics it could even happen 
		 * after every flush. In case we are working on the default FBO this will happen
		 * after a swapbuffers. All cases are caught by comparing a frame ID with "what was used for the last drawcall".
		 *
		 * Finally, it's the drawcall modes. Simple solution there: 
		 *  - if the drawmode is a triangle type, and was a triangle type, don't do setup (based on this criteria)
		 *  - if the drawmode is a triangle type, and was a non-triangle type, always do the setup (need new viewport setup)
		 *  - if the drawmode is a non-triangle type, and was a triangle type, always do the setup (need new viewport setup)
		 *  - if the drawmode is a non-triangle type, and was a non-triangle type, always do the setup (workaround bugzilla 11282)
		 *
		 */
		dirty_bits  = mali_statebit_raw_get( common_state, ( 1 << MALI_STATE_PLBU_FRAME_OR_DEPTH_PENDING ) |
															( 1 << MALI_STATE_SCISSOR_DIRTY ) | 
															( 1 << MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING ));

		this_call_is_triangle = mali_statebit_get( & ctx->state.common, MALI_STATE_MODE_TRIANGLE_TYPE );
		prev_call_is_triangle = gb_ctx->plbu_prev_call_is_triangle;
		gb_ctx->plbu_prev_call_is_triangle = this_call_is_triangle;

		frameid = _mali_frame_builder_get_current_frame_id( ctx->state.common.framebuffer.current_object->draw_frame_builder );

		if( dirty_bits != 0 || gb_ctx->plbu_scissor_viewport_last_frame_id != frameid || !this_call_is_triangle || !prev_call_is_triangle )
		{
			/* update the frame_id so that the next time around we don't hit this case */
			gb_ctx->plbu_scissor_viewport_last_frame_id = frameid;

			/* Setup the scissor and viewport. */
			MALI_CHECK_NO_ERROR( _gles_gb_setup_plbu_scissor( ctx, gb_ctx, buffer, &idx, buffer_len ) );
			
			/* Required once per frame, but since
			 * scissor/viewport/depthrange updates are
			 * usually infrequent we'll emit this command
			 * unconditionally.
			 */
			buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( GLES_GB_RSW_POST_INDEX_BIAS_ZERO, GP_PLBU_CONF_REG_OFFSET_VERTEX_ARRAY );

			/* Handle near/far plane updates.
			 */
			{
				const u32 znear = _mali_convert_fp32_to_binary( common_state->viewport.z_minmax[0] );
				const u32 zfar  = _mali_convert_fp32_to_binary( common_state->viewport.z_minmax[1] );

				MALI_DEBUG_ASSERT( common_state->viewport.z_minmax[0] /* near */ <= common_state->viewport.z_minmax[1] /*far*/, ( "Near/far planes incorrectly ordered" ) );

				buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( znear, GP_PLBU_CONF_REG_Z_NEAR );
				buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( zfar, GP_PLBU_CONF_REG_Z_FAR );
			}

			mali_statebit_unset( common_state, MALI_STATE_SCISSOR_DIRTY );
			mali_statebit_unset( common_state, MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING);
			mali_statebit_unset( common_state, MALI_STATE_PLBU_FRAME_OR_DEPTH_PENDING );
		}
	}

	MALI_DEBUG_ASSERT( idx < buffer_len, ( "plbu stream buffer overflow" ) );

	*index = idx;

	MALI_SUCCESS;
}


/**
 * Setup the PLBU registers.
 *
 * Sets up scissor box, updates rsw and vertex array base address.
 * Sets address of vertex and index data.
 * Copies index array to mali memory.
 * Sets the PLBU configuration register, z_near and z_far registers.
 * Sets point size buffer address.
 *
 * Decrements the semaphore, and sets the callback function
 * Also triggers the shaded vertices command and flushes the frame builder.
 *
 * @param ctx Pointer to the GLES context
 */
mali_err_code _gles_gb_plbu_setup( struct gles_context *const ctx )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( _gles_gb_get_draw_context( ctx ) );
	MALI_DEBUG_ASSERT_POINTER( _gles_gb_get_draw_context( ctx )->frame_builder );

	{
		gles_gb_context* const gb_ctx = _gles_gb_get_draw_context( ctx );
		mali_gp_job_handle gp_job     = _mali_frame_builder_get_gp_job( gb_ctx->frame_builder );

		mali_err_code ret;

		u32  local_command_buffer_index = 0;
		u32* command_buffer_index = &local_command_buffer_index;

        /*Size of command buffer for plbu commands. 16 is the fixed command size */
        u32 max_plbu_buffer_size = 16 + (gb_ctx->parameters.indexed ? gb_ctx->parameters.plbu_range_count : 1) * 2;

		/* try to get the cmd_list buffer directly */
		mali_gp_plbu_cmd* command_buffer = _mali_gp_job_plbu_cmds_reserve( gp_job, max_plbu_buffer_size );

		/* Allocate storage for a backup copy of the state we
         * are about to change.  In the event of an error
         * we restore the state.
         */
        u32 state_backup;
 
        MALI_CHECK_NON_NULL(command_buffer, MALI_ERR_OUT_OF_MEMORY);
 
        /* Make a backup copy of the state.  This is needed only
         * for error handling/recovery.
         */
        state_backup = ctx->state.common.state_flags[0];

		/* Now start building the PLBU command list
		 */
		ret = _gles_gb_plbu_setup_common( ctx, gb_ctx, &ctx->state.common, command_buffer, command_buffer_index, max_plbu_buffer_size );
		MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == ret, restore );

		if( !mali_statebit_get( &ctx->state.common, MALI_STATE_MODE_TRIANGLE_TYPE ) )
		{
			ret = _gles_gb_plbu_setup_points_lines( ctx, command_buffer, command_buffer_index, max_plbu_buffer_size );
			MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == ret, restore );
		}

		ret = _gles_gb_plbu_setup_draw( ctx, command_buffer, command_buffer_index, max_plbu_buffer_size );
		MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == ret, restore );

		MALI_DEBUG_ASSERT( *command_buffer_index < max_plbu_buffer_size, ( "plbu stream buffer overflow" ) );

		/* Commit the commands to the PLBU list */
		_mali_gp_job_plbu_cmds_confirm( gp_job, local_command_buffer_index );

		MALI_SUCCESS;
 
restore:
		/* An error occurred - rollback the state changes
		*/
		 ctx->state.common.state_flags[0] = state_backup;
		 MALI_ERROR( ret );
	}
}

/**
 * Extracts the scissor parameters as needed by the HW, and returns them through the output parameters.
 *
 * @param ctx - the gl context
 * @param gb_ctx - the geometry backend context
 * @param frame_builder - the framebuilder to get screen dimensions from
 * @param intersect_viewport - true for triangles
 */
void _gles_gb_extract_scissor_parameters(
		gles_context *ctx,
		mali_frame_builder  *frame_builder,
		mali_bool intersect_viewport,
		u32 *scissor_boundaries,
		mali_bool *scissor_closed )
{
	gles_common_state *state_common;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	state_common = &(ctx->state.common);

	{
		int frame_width  = _mali_frame_builder_get_width( frame_builder );
		int frame_height = _mali_frame_builder_get_height( frame_builder );
		int supersample_scalef = _gles_get_current_draw_supersample_scalefactor(ctx);	
		int gl_width = frame_width / supersample_scalef;
		int gl_height= frame_height / supersample_scalef;

		/* in OpenGL coordinate system (y-up), exclusive, not supersampled
		 */
		int scissor_left, scissor_right;
		int scissor_top, scissor_bottom;


		/* Start with the scissor from GLES API: if disabled use a fullscreen-scissor
		 */
		if( mali_statebit_get( state_common, GLES_STATE_SCISSOR_ENABLED ) )
		{
			scissor_left   = state_common->scissor.x;
			scissor_right  = state_common->scissor.x + state_common->scissor.width;
			scissor_bottom = state_common->scissor.y;
			scissor_top    = state_common->scissor.y + state_common->scissor.height;
		}
		else
		{
		
			scissor_left   = 0;
			scissor_right  = gl_width;
			scissor_bottom = 0;
			scissor_top    = gl_height;
		}

		/* Fragments of triangles are affected by the scissor - calculate
		 * the intersection of the viewport and the scissor for triangles.
		 */
		if( MALI_TRUE == intersect_viewport )
		{
			const int viewport_left   = state_common->viewport.x;
			const int viewport_right  = state_common->viewport.x + state_common->viewport.width;
			const int viewport_bottom = state_common->viewport.y;
			const int viewport_top    = state_common->viewport.y + state_common->viewport.height;

			scissor_left   = MAX( scissor_left,	viewport_left );
			scissor_right  = MIN( scissor_right, 	viewport_right );
			scissor_bottom = MAX( scissor_bottom, 	viewport_bottom );
			scissor_top    = MIN( scissor_top, 	viewport_top );
		}

		/* Convert to Mali200 coordinate system (flip y) when rendering
		 * to the default framebuffer
		 */
		if( state_common->framebuffer.current_object->draw_flip_y )
		{
			scissor_bottom = gl_height - scissor_bottom;
			scissor_top    = gl_height - scissor_top;
		}
		else
		{
			const int temp = scissor_bottom;

			scissor_bottom = scissor_top;
			scissor_top    = temp;
		}

		scissor_left	*= supersample_scalef;
		scissor_right	*= supersample_scalef;
		scissor_bottom	*= supersample_scalef;
		scissor_top	*= supersample_scalef;

		scissor_right -= 1;
		scissor_bottom -= 1;

		/* Clamp left/right/top/bottom scissor
		 */
		{
			const int horizontal_clamp	= frame_width-1;
			const int vertical_clamp	= frame_height-1;

			/* clamp scissor parameters to screen size (exclusive, for the scissorsettings)
			 */
			if( scissor_left < 0 )					scissor_left = 0;
			if( scissor_left > horizontal_clamp )	scissor_left = horizontal_clamp;
			scissor_boundaries[0] = scissor_left;

			if( scissor_right < 0 )					scissor_right = 0;
			if( scissor_right > horizontal_clamp )	scissor_right = horizontal_clamp;
			scissor_boundaries[1] = scissor_right;

			if( scissor_bottom < 0 )				scissor_bottom = 0;
			if( scissor_bottom > vertical_clamp )	scissor_bottom = vertical_clamp;
			scissor_boundaries[2] = scissor_bottom;

			if( scissor_top < 0 )					scissor_top = 0;
			if( scissor_top > vertical_clamp )		scissor_top = vertical_clamp;
			scissor_boundaries[3] = scissor_top;

			*scissor_closed = MALI_FALSE;
			if( ( scissor_left > scissor_right ) || ( scissor_top > scissor_bottom ))
			{
				*scissor_closed = MALI_TRUE;
			}

		}
	}
	
}

#if ! HARDWARE_ISSUE_3396
void _gles_gb_extract_viewport_dimensions(
	gles_context *ctx,
	mali_frame_builder  *frame_builder,
	s32 *viewport_boundaries )
{
	s32 vp_top, vp_bottom, vp_left, vp_right;

	int frame_height = _mali_frame_builder_get_height( frame_builder );
	int frame_width  = _mali_frame_builder_get_width( frame_builder );
	int supersample_scalef = _gles_get_current_draw_supersample_scalefactor(ctx);

	int viewport_x	    = ctx->state.common.viewport.x * supersample_scalef;
	int viewport_width  = ctx->state.common.viewport.width * supersample_scalef;
	int viewport_y	    = ctx->state.common.viewport.y * supersample_scalef;
	int viewport_height = ctx->state.common.viewport.height * supersample_scalef;

	if ( ctx->state.common.framebuffer.current_object->draw_flip_y )
	{
		vp_bottom = frame_height - viewport_y;
		vp_top    = vp_bottom - viewport_height;
	} else
	{
		vp_bottom = viewport_y + viewport_height;
		vp_top    = viewport_y;
	}
	vp_left   = viewport_x;
	vp_right  = vp_left + viewport_width;

	/* clamp to the size of the current rendertarget */
	vp_bottom = CLAMP( vp_bottom, 0, frame_height );
	vp_top    = CLAMP( vp_top,    0, frame_height );
	vp_left   = CLAMP( vp_left,   0, frame_width );
	vp_right  = CLAMP( vp_right,  0, frame_width );

	viewport_boundaries[0] = vp_bottom;
	viewport_boundaries[1] = vp_top;
	viewport_boundaries[2] = vp_left;
	viewport_boundaries[3] = vp_right;
}
#endif /* ! HARDWARE_ISSUE_3396 */


/**
 * Setup update the scissoring settings through the frame builder
 *
 * @param         gb_ctx        Pointer to the geometry backend context
 * @param[in,out] buffer		Pointer to an array of PLBU commands
 * @param[in,out] index			Index of first free entry in the array of PLBU commands
 * @param         buffer_len    The total number of elements in #buffer
 */
#if !HARDWARE_ISSUE_3396
MALI_STATIC mali_err_code MALI_CHECK_RESULT _gles_gb_setup_plbu_scissor_non_retarded(
	struct gles_context *const ctx, gles_gb_context *gb_ctx,
	mali_gp_plbu_cmd * const buffer, u32 * const index, const u32 buffer_len )
{
	mali_frame_builder *frame_builder;
	u32 scissor_boundaries[4];
	mali_bool scissor_closed;
	mali_err_code       error;
	float viewport_left;
	float viewport_right;
	float viewport_bottom;
	float viewport_top;
	float scissor_left;
	float scissor_right;
	float scissor_bottom;
	float scissor_top;
	mali_bool triangle_mode;

	MALI_DEBUG_ASSERT_POINTER( gb_ctx );
	MALI_DEBUG_ASSERT_POINTER( buffer );
	MALI_DEBUG_ASSERT_POINTER( index );

	frame_builder = gb_ctx->frame_builder;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	triangle_mode = (gb_ctx->parameters.mode > GL_LINE_STRIP);

	_gles_gb_extract_scissor_parameters( ctx, frame_builder, 
	                                     triangle_mode,
	                                     scissor_boundaries,
										 &scissor_closed );
	if(scissor_closed) return MALI_ERR_EARLY_OUT; /* HW cannot encode this, abort drawcall */

	scissor_left = (float)scissor_boundaries[0];
	scissor_right = (float)MAX( scissor_boundaries[1], 0 );
	scissor_bottom = (float)MAX( scissor_boundaries[2], 0 );
	scissor_top = (float)scissor_boundaries[3];


	
#if ( GL_POINTS != 0 ) || (GL_LINE_STRIP != 3)
#error msg "This code depends on the definition of GL_POINTS and GL_LINE_STRIP"
#endif
	if ( !triangle_mode )
	{
		float halfwidth_correction;
		int frame_width = _mali_frame_builder_get_width(frame_builder);
		int frame_height = _mali_frame_builder_get_height(frame_builder);

		if ( gb_ctx->parameters.mode == GL_POINTS )
		{
			halfwidth_correction = GLES_ALIASED_LINE_WIDTH_MAX/2.f; /* also supersample scalefactor corrected */
		}
		else { 
			halfwidth_correction = ((float) gb_ctx->parameters.line_width)/2.f; /* also supersample scalefactor corrected */ 
		}

		viewport_left = -halfwidth_correction;
		viewport_top =  -halfwidth_correction;
		viewport_right = ( float ) frame_width + halfwidth_correction;
		viewport_bottom = ( float ) frame_height + halfwidth_correction;
	}
	else
	{
		s32 viewport_boundaries[4];
		_gles_gb_extract_viewport_dimensions( ctx, frame_builder, viewport_boundaries);
		
		viewport_bottom = (float)viewport_boundaries[0];
		viewport_top = (float)viewport_boundaries[1];
		viewport_left = (float)viewport_boundaries[2];
		viewport_right = (float)viewport_boundaries[3];
	
		scissor_left = MAX( scissor_left, viewport_left );
		scissor_right = MIN( scissor_right, viewport_right-1 );
		scissor_top = MAX( scissor_top, viewport_top );
		scissor_bottom = MIN( scissor_bottom, viewport_bottom-1 );
	}


	if ( ( scissor_right < scissor_left ) || ( scissor_bottom < scissor_top ) )
	{
		MALI_ERROR( MALI_ERR_EARLY_OUT );
	}

	/* This interface differs depending on hw rev. Using scissor settings in this specialcase */
	error = _mali_frame_builder_viewport(
		frame_builder,
		viewport_left,
		viewport_top,
		viewport_right,
		viewport_bottom,
		buffer,
		index,
		buffer_len
	);
	
	if( MALI_ERR_NO_ERROR == error )
	{
		error = _mali_frame_builder_scissor(
			frame_builder,
			(u32)scissor_left,
			(u32)scissor_top,
			(u32)scissor_right,
			(u32)scissor_bottom,
			buffer,
			index,
			buffer_len
		);
	}

	return error;
}
#endif /* HARDWARE_ISSUE_3396 */

/**
 * Setup update the scissoring settings through the frame builder
 *
 * @param gb_ctx                Pointer to the geometry backend context
 * @param[in,out] buffer		Pointer to an array of PLBU commands
 * @param[in,out] index			Index of first free entry in the array of PLBU commands
 * @param         buffer_len    The total number of elements in #buffer
 * @param scissor_boundaries [out] - the return value of this function: an array for storing scissor's left, right, bottom, top values
 * @param viewport_dimensions [out] - the return value of this function: an array for storing viewport's left, right, bottom, top values
 */
MALI_STATIC mali_err_code MALI_CHECK_RESULT _gles_gb_setup_plbu_scissor_retarded(
	struct gles_context *const ctx, gles_gb_context *gb_ctx,
	mali_gp_plbu_cmd * const buffer, u32 * const index, const u32 buffer_len )
{
	mali_frame_builder *frame_builder;
	u32 scissor_boundaries[4];
	s32 viewport_boundaries[4];
	mali_err_code       error;
	float viewport_left;
	float viewport_right;
	float viewport_bottom;
	float viewport_top;
	float scissor_left;
	float scissor_right;
	float scissor_bottom;
	float scissor_top;

	mali_bool triangle_mode;
	mali_bool scissor_closed;
	
	MALI_DEBUG_ASSERT_POINTER( gb_ctx );
	MALI_DEBUG_ASSERT_POINTER( buffer );
	MALI_DEBUG_ASSERT_POINTER( index );

	frame_builder = gb_ctx->frame_builder;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	triangle_mode = (gb_ctx->parameters.mode > GL_LINE_STRIP);
	_gles_gb_extract_scissor_parameters( ctx, frame_builder, 
	                                     triangle_mode,
	                                     scissor_boundaries,
										 &scissor_closed );
	if(scissor_closed) return MALI_ERR_EARLY_OUT; /* HW cannot encode this, abort drawcall */

#if HARDWARE_ISSUE_3396
	viewport_left = scissor_boundaries[0];
	viewport_top = scissor_boundaries[3];
	viewport_right = MAX( scissor_boundaries[1], 0 );
	viewport_bottom = MAX( scissor_boundaries[2], 0 );
#else /* HARDWARE_ISSUE_3396 */
	_gles_gb_extract_viewport_dimensions( ctx, frame_builder, viewport_boundaries);
	viewport_left = ( float ) viewport_boundaries[2];
	viewport_top = ( float ) viewport_boundaries[1];
	viewport_right = ( float ) viewport_boundaries[3];
	viewport_bottom = ( float ) viewport_boundaries[0];
#endif /* HARDWARE_ISSUE_3396 */
	
	/* Note: extracted scissor and viewport parameters are already multiplied by supersample scalefactor */

	scissor_left =(float)scissor_boundaries[0];
	scissor_top = (float)scissor_boundaries[3];
	scissor_right = (float)MAX( scissor_boundaries[1], 0 );
	scissor_bottom = (float)MAX( scissor_boundaries[2], 0 );

	/* This interface differs depending on hw rev. Using scissor settings in this specialcase */
	error = _mali_frame_builder_viewport(
		frame_builder,
		viewport_left,
		viewport_top,
		viewport_right,
		viewport_bottom,
		buffer,
		index,
		buffer_len
	);

	if( MALI_ERR_NO_ERROR == error )
	{
		error = _mali_frame_builder_scissor(
			frame_builder,
			(u32)scissor_left,
			(u32)scissor_top,
			(u32)scissor_right,
			(u32)scissor_bottom,
			buffer,
			index,
			buffer_len
		);
	}

	return error;
}

/**
 * Setup update the scissoring settings through the frame builder
 *
 * @param gb_ctx                Pointer to the geometry backend context
 * @param[in,out] buffer		Pointer to an array of PLBU commands
 * @param[in,out] index			Index of first free entry in the array of PLBU commands
 * @param         buffer_len    The total number of elements in #buffer
 * @param scissor_boundaries [out] - the return value of this function: an array for storing scissor's left, right, bottom, top values
 * @param viewport_dimensions [out] - the return value of this function: an array for storing viewport's left, right, bottom, top values
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_setup_plbu_scissor(
	struct gles_context *const ctx,
	gles_gb_context *gb_ctx,
	mali_gp_plbu_cmd * const buffer, u32 * const index, const u32 buffer_len )
{	
	if ( GLES_API_VERSION_2 == gb_ctx->api_version )
	{
#if HARDWARE_ISSUE_3396
		MALI_CHECK_NO_ERROR( _gles_gb_setup_plbu_scissor_retarded( ctx, gb_ctx, buffer, index, buffer_len ) );
#else
		MALI_CHECK_NO_ERROR( _gles_gb_setup_plbu_scissor_non_retarded( ctx, gb_ctx, buffer, index, buffer_len ) );
#endif
	}
	else
	{
		MALI_CHECK_NO_ERROR( _gles_gb_setup_plbu_scissor_retarded( ctx, gb_ctx, buffer, index, buffer_len ) );
	}

	MALI_SUCCESS;
}

MALI_STATIC u32 _gles_gb_setup_plbu_get_point_size( struct gles_context * const ctx )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( _gles_gb_get_draw_context( ctx ) );

	{
		gles_gb_context * const gb_ctx = _gles_gb_get_draw_context( ctx );

		float point_size = gb_ctx->parameters.fixed_point_size;

		if( mali_statebit_get( &ctx->state.common, MALI_STATE_MODE_SUPERSAMPLE_16X ) )
		{
			point_size *= 2;
		}

		return _mali_convert_fp32_to_binary(point_size);
	}
}


/**
 * Setup plbu GP_PLBU_CONF_REG_INDEX_ARRAY_ADDR register. Handles using buffer objects and not using them.
 * @param gb_ctx Pointer to the geometry backend context
 * @param gp_job handle to the gp job
 *  @param array_size The size of the index array
 */
MALI_STATIC mali_err_code MALI_CHECK_RESULT _gles_gb_setup_plbu_index_array( gles_gb_context *gb_ctx, u32 array_size, u32 *element_addr)
{
	mali_err_code err = MALI_ERR_NO_ERROR;
	int typesize;

	/* copy size */
	typesize = _gles_get_type_size(gb_ctx->parameters.index_type);

	MALI_DEBUG_ASSERT_POINTER(gb_ctx);
	MALI_DEBUG_ASSERT_POINTER(gb_ctx->vertex_array);
	MALI_DEBUG_ASSERT_POINTER(element_addr);

	if (NULL == gb_ctx->vertex_array->element_vbo)
	{
		/* not using a buffer object for indices */
		void *element_ptr;


		/* sanity-check */
		MALI_DEBUG_ASSERT(gb_ctx->vertex_array->element_array_buffer_binding == 0, ("element buffer binding inconsistency - NULL pointer, but %d binding", gb_ctx->vertex_array->element_array_buffer_binding));

		/* Copy index array to mali memory */
		element_ptr = _mali_mem_pool_alloc(gb_ctx->frame_pool, array_size, element_addr);
		if (NULL == element_ptr) return MALI_ERR_OUT_OF_MEMORY;


		#if MALI_INSTRUMENTED
		if(gb_ctx->parameters.index_count * typesize != array_size){
			/* Force last 4 bytes to 0 before copying data to make
			 * sure that unused portion of buffer is set to 0s and
			 * not some random value.  This is to prevent causing
			 * unintentional differences in instrumentation dump
			 * during debug runs. */
			_mali_sys_memset((u8*)element_ptr + array_size - 4, 0, 4);
		}
		#endif
		_mali_sys_memcpy_cpu_to_mali( element_ptr, gb_ctx->parameters.indices, gb_ctx->parameters.index_count * typesize, typesize );

	}
	else
	{
		/* using a buffer object for indices */
		struct gles_gb_buffer_object_data *vbo = gb_ctx->vertex_array->element_vbo->gb_data;

		/* sanity-check */
		MALI_DEBUG_ASSERT(gb_ctx->vertex_array->element_array_buffer_binding != 0, ("element buffer binding inconsistency - NULL pointer, but 0 binding"));

		MALI_DEBUG_ASSERT_POINTER(vbo);
		MALI_DEBUG_ASSERT_POINTER(vbo->mem);
		MALI_DEBUG_ASSERT_POINTER(vbo->mem->mali_memory);

		_mali_mem_ref_addref(vbo->mem);
		err = _mali_frame_builder_add_callback( gb_ctx->frame_builder,
		                                        MALI_STATIC_CAST(mali_frame_cb_func)_mali_mem_ref_deref,
		                                        MALI_STATIC_CAST(mali_frame_cb_param)vbo->mem );

		if (MALI_ERR_NO_ERROR != err) 
		{
			_mali_mem_ref_deref( vbo->mem );
			return err;
		}

		*element_addr = _mali_mem_mali_addr_get(vbo->mem->mali_memory, (int)gb_ctx->parameters.indices);

		/* Process convert endianess if the buffer has not already been processed */
		_gles_gb_buffer_object_process_endianess(vbo, gb_ctx->vertex_array->element_vbo->size, 1, typesize, typesize, 0);
	}
	return err;
}

/**
 * Set position data input address and index array addr, only necessary in draw elements mode
 * @param gb_ctx Pointer to the geometry backend context
 * @param gp_job handle to the gp job
 */
MALI_STATIC mali_err_code MALI_CHECK_RESULT _gles_gb_setup_plbu_position( gles_gb_context *gb_ctx, u32 *element_addr )
{
	u32 array_size = 0;

	MALI_DEBUG_ASSERT_POINTER(gb_ctx);
	MALI_DEBUG_ASSERT( gb_ctx->parameters.indexed == MALI_TRUE, ("Only valid for indexed geometry") );

	/* Setup index input datatype and address */
	switch( gb_ctx->parameters.index_type )
	{
		case GL_UNSIGNED_BYTE:
			array_size = ( ( ( gb_ctx->parameters.index_count ) * sizeof(u8) ) + 0x03 ) & ~0x03;
		break;
		case GL_UNSIGNED_SHORT:
			array_size = ( ( ( gb_ctx->parameters.index_count ) * sizeof(u16) ) + 0x03 ) & ~0x03;
		break;
		default:
			MALI_DEBUG_ASSERT( 0, ("illegal index type!\n") );
	}

	return _gles_gb_setup_plbu_index_array( gb_ctx, array_size, element_addr );
}

/**
 * Optional PLBU command list setup for point/line parameters
 *
 * @param gb_ctx                Pointer to the geometry backend context
 * @param[in,out] buffer        Pointer to an array of PLBU commands
 * @param[in,out] index         Index of first free entry in the array of PLBU commands
 * @param         buffer_len    The total number of elements in #buffer
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_plbu_setup_points_lines( struct gles_context *const ctx,
									      mali_gp_plbu_cmd * const buffer, u32 * const index,
									      const u32 buffer_len )
{
	u32 idx;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( index );
	MALI_DEBUG_ASSERT_POINTER( buffer );

	idx = *index;

	MALI_DEBUG_ASSERT( idx < buffer_len, ( "plbu stream buffer overflow" ) );

	{
		gles_gb_context *const gb_ctx = _gles_gb_get_draw_context( ctx );
		struct gles_state *const state = &ctx->state;

		MALI_DEBUG_ASSERT_POINTER( state );
		MALI_DEBUG_ASSERT_POINTER( gb_ctx );

		if( mali_statebit_get( &state->common, MALI_STATE_MODE_POINT_TYPE ) )
		{
			/* Setup point size - or pointer to point size array
			 */
			if( gb_ctx->parameters.point_size_override == 1 )
			{
				const u32 size = _gles_gb_setup_plbu_get_point_size( ctx );

				buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( size, GP_PLBU_CONF_REG_POINT_SIZE );
			}
			else if( mali_statebit_get( &state->common, MALI_STATE_MODE_INDEXED ) )
			{
				const u32 point_size_addr = gb_ctx->point_size_addr;

				MALI_DEBUG_ASSERT_ALIGNMENT( point_size_addr, 16 );

				buffer[idx++] =
				    GP_PLBU_COMMAND_WRITE_CONF_REG( point_size_addr, GP_PLBU_CONF_REG_POINT_SIZE_ADDR );
			}
			else
			{
				/* NOP */
			}
		}
		else if( mali_statebit_get( &state->common, MALI_STATE_MODE_LINE_TYPE ) )
		{
			/* Setup line width
			 */
			const u32 width = _mali_convert_fp32_to_binary( gb_ctx->parameters.line_width );

			buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( width, GP_PLBU_CONF_REG_POINT_SIZE );
		}
		else
		{
			/* Triangles - NOP */
		}
	}

	MALI_DEBUG_ASSERT( idx < buffer_len, ( "plbu stream buffer overflow" ) );

	*index = idx;

	MALI_SUCCESS;
}

/**
 * PLBU command list instructions to 'draw' with current parameters
 *
 * @param gb_ctx                Pointer to the geometry backend context
 * @param gp_job                Handle to the gp job
 * @param[in,out] buffer        Pointer to an array of PLBU commands
 * @param[in,out] index         Index of first free entry in the array of PLBU commands
 * @param         buffer_len    The total number of elements in #buffer
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_plbu_setup_draw( struct gles_context *const ctx, 
								      mali_gp_plbu_cmd * const buffer, u32 * const index,
								      const u32 buffer_len )
{
	u32 idx;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( index );
	MALI_DEBUG_ASSERT_POINTER( buffer );

	idx = *index;

	MALI_DEBUG_ASSERT( idx < buffer_len, ( "plbu stream buffer overflow" ) );

	{
		gles_gb_context *const gb_ctx = _gles_gb_get_draw_context( ctx );

		MALI_DEBUG_ASSERT_POINTER( gb_ctx );
		MALI_DEBUG_ASSERT( gb_ctx->frame_pool == _mali_frame_builder_frame_pool_get( _gles_get_current_draw_frame_builder( ctx ) ),
						("Wrong Frame_pool!\n") );

		if( gb_ctx->parameters.indexed )
        {
            u32 range_index;
            u32 element_addr;
            u32 tmp_addr;

            MALI_CHECK_NO_ERROR( _gles_gb_setup_plbu_position( gb_ctx, &element_addr ) );
            buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( gb_ctx->position_addr, GP_PLBU_CONF_REG_VERTEX_ARRAY_ADDR );
            buffer[idx++] = GP_PLBU_COMMAND_WAIT_DEC_SEM( 1, 1, 0 );

            for(range_index = 0; range_index < gb_ctx->parameters.plbu_range_count; range_index++)
            {
                tmp_addr = element_addr + gb_ctx->parameters.idx_plbu_range[range_index].start * _gles_get_type_size(gb_ctx->parameters.index_type);
                buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( tmp_addr, GP_PLBU_CONF_REG_INDEX_ARRAY_ADDR );

                MALI_DEBUG_ASSERT( gb_ctx->parameters.index_count < ( 1 << 24 ), ( "index count above range - see defect 4481" ) );

                /* GL 'mode' encoding matches the GP2
                 */
                buffer[idx++] = GP_PLBU_COMMAND_PROCESS_SHADED_VERTICES( gb_ctx->parameters.start_index, gb_ctx->parameters.idx_plbu_range[range_index].count,
                        gb_ctx->parameters.mode, MALI_TRUE );
            }
        }
        else
        {
            MALI_DEBUG_ASSERT( gb_ctx->parameters.index_count < ( 1 << 24 ),
                    ( "index count above range - see defect 4481" ) );

            buffer[idx++] = GP_PLBU_COMMAND_PROCESS_SHADED_VERTICES( gb_ctx->parameters.start_index, gb_ctx->parameters.index_count,
                    gb_ctx->parameters.mode, MALI_FALSE );
        }
	}

	MALI_DEBUG_ASSERT( idx < buffer_len, ( "plbu stream buffer overflow" ) );

	*index = idx;

	MALI_SUCCESS;
}

