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
#include <gles_context.h>
#include <shared/binary_shader/bs_object.h>

#include "gles_m200_rsw_setup.h"
#include "gles_fb_context.h"
#include "gles_framebuffer_object.h"

/**
 * @brief tests wether we can set the pixel kill permissible flag for this drawcall
 * @param ctx The pointer to the GLES context
 * @param rsw The rsw to set the flag in
 * @param fragment_shader_flags all the fragment shader flags
 * @note  must occur after all other rsw operations for a frame have been completed.
 */
MALI_STATIC mali_bool _gles_m200_check_pixel_kill_permissible( gles_context *ctx, m200_rsw *rsw, struct bs_fragment_flags *fragment_shader_flags )
{
	MALI_DEBUG_ASSERT_POINTER( rsw );
	MALI_DEBUG_ASSERT_POINTER( fragment_shader_flags );

	if( ( !_gles_fb_get_blend( ctx ) ) &&
	    ( 0xf == __m200_rsw_decode( rsw, M200_RSW_MULTISAMPLE_WRITE_MASK )) &&
	    ( 0xf == __m200_rsw_decode( rsw, M200_RSW_ABGR_WRITE_MASK )) &&

	    ( M200_TEST_ALWAYS_SUCCEED == __m200_rsw_decode( rsw, M200_RSW_ALPHA_TEST_FUNC )) &&

	    ( 0 == __m200_rsw_decode( rsw, M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_UPDATE )) &&

	    ( MALI_FALSE == fragment_shader_flags->color_read ) &&
	    ( 0 == __m200_rsw_decode( rsw, M200_RSW_HINT_SHADER_CONTAINS_DISCARD )) &&

	    ( 0 == __m200_rsw_decode( rsw, M200_RSW_ROP4_ENABLE )))
	{
		return MALI_TRUE;
	}
	else
	{
		return MALI_FALSE;
	}
}

MALI_STATIC mali_bool _gles_m200_is_early_z_allowed( gles_context *ctx, bs_program *program_bs )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( program_bs );

	if( program_bs->fragment_pass.flags.fragment.used_discard ) return MALI_FALSE;
	if( program_bs->fragment_pass.flags.fragment.depth_read) return MALI_FALSE;
	if( program_bs->fragment_pass.flags.fragment.stencil_read) return MALI_FALSE;
	if( _gles_fb_get_alpha_test( ctx ) ) return MALI_FALSE;

	return MALI_TRUE;
}

/**
 * @brief set all the rsw states related to shader programs
 * @param ctx The pointer to the GLES context
 * @param rsw The rsw to set states in
 * @param state the parts of the gles state that is common between OpenGL ES 1.x and OpenGL ES 2.x
 */
MALI_STATIC void _gles_m200_set_shader_parameters( gles_context *ctx, m200_rsw *rsw )
{
	struct bs_fragment_flags *fragment_shader_flags;
	bs_program *program_bs;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( rsw );

	program_bs = &(ctx->state.common.current_program_rendering_state->binary);
	fragment_shader_flags = &program_bs->fragment_pass.flags.fragment;

	__m200_rsw_encode( rsw, M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_UPDATE, _gles_m200_is_early_z_allowed( ctx, program_bs ) );
	__m200_rsw_encode( rsw, M200_RSW_FORWARD_PIXEL_KILL_PERMISSIBLE, _gles_m200_check_pixel_kill_permissible( ctx, rsw, fragment_shader_flags ) );

#if HARDWARE_ISSUE_4924
	/* disable all early z operations */
	__m200_rsw_encode( rsw, M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_UPDATE, 0 );
	__m200_rsw_encode( rsw, M200_RSW_FORWARD_PIXEL_KILL_PERMISSIBLE, 0 );
#endif
}

void _gles_m200_set_rsw_parameters( gles_context *ctx, gles_fb_context *fb_ctx, m200_rsw *rsw, u32 mode )
{
	gles_common_state *state_common;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	state_common = &ctx->state.common;

	MALI_DEBUG_ASSERT_POINTER( fb_ctx );
	MALI_DEBUG_ASSERT_POINTER( rsw );

#if HARDWARE_ISSUE_7585
	_gles_fb_stencil_fixup_deferred( ctx );
#endif

	/* When flatshading is enabled you must use the last vertex's color. It is OK to
	 * do this when setting up RSW's for OpenGL ES 2.x as well since flatshading won't
	 * be enabled.
	 */
	{
		u32 vertex_selector = 0;
		if( mode >= GL_LINES) vertex_selector++;
		if( mode >= GL_TRIANGLES) vertex_selector++;
			
		MALI_DEBUG_ASSERT( mode <= 6, ("Unexpected primitive mode in rsw setup") );
		__m200_rsw_encode( rsw, M200_RSW_FLATSHADING_VERTEX_SELECTOR, vertex_selector );
	}

	/* This depends upon primitive type and so may change per drawcall
	 */
	_gles_fb_polygon_offset_deferred( ctx, mali_statebit_get( & ctx->state.common, MALI_STATE_MODE_TRIANGLE_TYPE ));

	{
		const mali_bool is_point_type = mali_statebit_get( & ctx->state.common, MALI_STATE_MODE_POINT_TYPE );
		const mali_bool is_line_type  = mali_statebit_get( & ctx->state.common, MALI_STATE_MODE_LINE_TYPE );

		_gles_fb_multisampling_deferred( ctx, is_point_type, is_line_type );
	}

	/* polygon orientation (flip if rendering to a texture - same logic as when flipping the viewport)
	 *	from gles header GL_CW = 0x0900, GL_CCW = 0x0901 
	 */
	{
		u32 flipped = state_common->framebuffer.current_object->draw_flip_y;
		u32 ccw		= state_common->rasterization.front_face;
		const u32 orientation = ( 0 == ((flipped ^ ccw ) & 1) ) ? M200_POLYGON_ORIENTATION_ANTI_CLOCKWISE : M200_POLYGON_ORIENTATION_CLOCKWISE;
		__m200_rsw_encode( rsw, M200_RSW_POLYGON_ORIENTATION_FLAG, orientation );
	}

	/* Set the base addresses of the texture and uniform remapping tables
	 */
	__m200_rsw_encode( rsw, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_ADDRESS, fb_ctx->current_td_remap_table_address );
	__m200_rsw_encode( rsw, M200_RSW_UNIFORMS_REMAPPING_TABLE_ADDRESS, fb_ctx->current_uniform_remap_table_addr );

	/* This must occur at the end as it depends upon bits in the RSW
	 */
	_gles_m200_set_shader_parameters( ctx, rsw );
}
