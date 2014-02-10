/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles2_draw.h"

#include <base/mali_debug.h>

#include "gles_context.h"
#include "gles2_state/gles2_state.h"
#include "gles_convert.h"
#include "gles_share_lists.h"
#include "gles_util.h"
#include "gles_draw.h"
#include "gles_fb_interface.h"
#include "gles_framebuffer_object.h"
#include "gles_instrumented.h"
#include "mali_gp_geometry_common/gles_geometry_backend_context.h"

#if HARDWARE_ISSUE_7571_7572
/**
 * @brief Draws a dummy_quad if points are to be drawn, which happens it the framebuffer has been drawn to with triangles after last dummy_quad or clear
 * @param ctx The pointer to the GLES context
 * @param mode The draw current draw mode
 * @return An errorcode if it fails, MALI_ERR_NO_ERROR if not.
 */
MALI_STATIC mali_err_code MALI_CHECK_RESULT _gles2_draw_current_draw_mode( struct gles_context *ctx, GLenum mode )
{
	mali_bool *has_triangles;

	gles_framebuffer_object *fb_object = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	/* Check if we have a bound framebuffer */
	fb_object = ctx->state.common.framebuffer.current_object;
	MALI_DEBUG_ASSERT_POINTER( fb_object );
	has_triangles = &fb_object->framebuffer_has_triangles;

	if ( GL_POINTS == mode && MALI_TRUE == *has_triangles )
	{
		MALI_CHECK_NO_ERROR( _gles_drawcall_begin( ctx ) );
		MALI_CHECK_NO_ERROR( _gles_draw_dummyquad( ctx ) );
		_gles_drawcall_end( ctx );
		*has_triangles = MALI_FALSE;
	}
	if ( GL_TRIANGLES == mode || GL_TRIANGLE_STRIP == mode || GL_TRIANGLE_FAN == mode )
	{
		*has_triangles = MALI_TRUE;
	}

	MALI_SUCCESS;
}
#endif

GLenum _gles2_draw_elements( gles_context *ctx, GLenum mode, GLint count, GLenum type, const void *indices )
{
	mali_err_code merr;
	GLenum        err;
	GLenum        prev_mode;
	u32           vs_range_count = 1;
    u32           coherence = 0;
    index_range   local_idx_range[MALI_LARGEST_INDEX_RANGE];
    index_range  *idx_vs_range = local_idx_range;

	gles2_state  *state_gles2;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles2 );

	state_gles2 = ctx->state.api.gles2;

	err = _gles_draw_elements_error_checking( mode, count, type );
	MALI_CHECK(GL_NO_ERROR == err, err);

	count = _gles_round_down_vertex_count(mode, count);
	if (count == 0) GL_SUCCESS;

	/* According to the OpenGL ES 2.0 spec, section 2.15.3, drawing with an invalid program gives undefined
	   vertex and fragments. Program 0 is the only way to get an invalid program active.

	   We'll just nop the draw-call.
	 */
	if( 0 == state_gles2->program_env.current_program )
	{
		MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING, "No current program set - draw call ignored"));
		GL_SUCCESS;
	}

	/* At this point, the program should be legal and linked */
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.current_program_rendering_state );
	MALI_DEBUG_ASSERT( ctx->state.common.current_program_rendering_state->binary.linked == MALI_TRUE, ("Program is not linked!") );

	/* Ready the bound framebuffer */
	err = _gles_fbo_internal_draw_setup( ctx );
	if ( err != GL_NO_ERROR )
	{
		return err;
	}

#if HARDWARE_ISSUE_7571_7572
	merr = _gles2_draw_current_draw_mode( ctx, mode );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_merr );
#endif

	merr = _gles_drawcall_begin(ctx);
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_merr );

	merr = _gles_init_draw_elements(ctx, count, type, mode, indices, &idx_vs_range, &vs_range_count, &coherence);

	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_endraw_merr );

	merr = _gles_fb_init_draw_call( ctx, mode );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_endraw_merr );

	prev_mode = state_gles2->previous_draw_mode;
	if( prev_mode != mode )
	{
		/* mode bit masked by 0x04 indicates a line mode */
		if((prev_mode ^ mode) & 0x04) 
		{
			/* We need to set the viewport again since we need to prevent 
			 * discarding of lines and points more than half-way outside the viewport.
			 * We also need to set it back when we are done.
			 */
			mali_statebit_set( &ctx->state.common, MALI_STATE_VIEWPORT_SCISSOR_UPDATE_PENDING );
		}
		state_gles2->previous_draw_mode = mode;
	}

	merr = _gles_draw_elements_internal( ctx, mode, count, type, indices, idx_vs_range, local_idx_range, vs_range_count, coherence );

exit_endraw_merr:
	_gles_drawcall_end(ctx);
exit_merr:
	return _gles_convert_mali_err( merr );
}

GLenum _gles2_draw_arrays( gles_context *ctx, GLenum mode, GLint first, GLint count )
{
	mali_err_code merr;
	GLenum err;
	GLenum prev_mode;
	gles2_state        *state_gles2;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles2 );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists->texture_object_list );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists->renderbuffer_object_list );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists->framebuffer_object_list );

	state_gles2 = ctx->state.api.gles2;

	err = _gles_draw_arrays_error_checking( mode, first, count );
	MALI_CHECK(GL_NO_ERROR == err, err);

	/* According to the OpenGL ES 2.0 spec, section 2.15.3, drawing with an invalid program gives undefined
	   vertex and fragments. Program 0 is the only way to get an invalid program active.

	   We'll just nop the draw-call.
	 */
	if( 0 == state_gles2->program_env.current_program )
	{
		MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING, "No current program set - draw call ignored"));
		GL_SUCCESS;
	}

	/* At this point, the program should be legal and linked */
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.current_program_rendering_state );
	MALI_DEBUG_ASSERT( ctx->state.common.current_program_rendering_state->binary.linked == MALI_TRUE, ("Program is not linked!") );

	count = _gles_round_down_vertex_count(mode, count);
	if (count == 0) GL_SUCCESS;

	MALI_DEBUG_ASSERT(count >= 0, ("Count is unexpectantly negative"));

	/* Ready the bound framebuffer */
	err = _gles_fbo_internal_draw_setup( ctx );
	if ( err != GL_NO_ERROR )
	{
		return err;
	}

#if HARDWARE_ISSUE_7571_7572
	merr = _gles2_draw_current_draw_mode( ctx, mode );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_merr );
#endif


	merr = _gles_drawcall_begin(ctx);
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_merr );

	merr = _gles_init_draw_arrays( ctx, mode, count );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_endraw_merr );

	merr = _gles_fb_init_draw_call( ctx, mode );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == merr, exit_endraw_merr );

	prev_mode = state_gles2->previous_draw_mode;
	if( prev_mode != mode )
	{	
		/* mode bit masked by 0x04 indicates a line mode */
		if((prev_mode ^ mode) & 0x04)
		{
			/* We need to set the viewport again since we need to prevent 
			 * discarding of lines and points more than half-way outside the viewport.
			 * We also need to set it back when we are done.
			 */
			mali_statebit_set( &ctx->state.common, MALI_STATE_VIEWPORT_SCISSOR_UPDATE_PENDING );
		}
		state_gles2->previous_draw_mode = mode;
	}

	merr = _gles_draw_arrays_internal( ctx, mode, first, count );

exit_endraw_merr:
	_gles_drawcall_end(ctx);
exit_merr:
	return _gles_convert_mali_err( merr );
}

