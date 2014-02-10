/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_fb_context.h"

#include "mali_system.h"
#include "mali_render_regs.h"
#include <gles_incremental_rendering.h>

#include "gles_m200_shaders.h"
#include "gles_m200_td.h"
#include "gles_m200_uniforms.h"

#include "../gles_context.h"
#include "../gles_texture_object.h"

#include "gles_m200_rsw_setup.h"
#include "gles_fb_rendering_state.h"
#include "gles_framebuffer_object.h"
#include "shared/m200_gp_frame_builder.h"
#include "shared/m200_gp_frame_builder_inlines.h"

MALI_STATIC void set_single_fragment_uniform(bs_program *program_bs,
                                                         struct gles_program_rendering_state* prs,
                                                         int loc, float val)
{
	MALI_DEBUG_ASSERT_RANGE(loc, -1, (int)program_bs->fragment_shader_uniforms.cellsize - 1);
	if ((loc >= 0) && (program_bs->fragment_shader_uniforms.array[loc] != val))
	{
		program_bs->fragment_shader_uniforms.array[loc] = val;
		prs->fp16_cached_fs_uniforms[loc] = _gles_fp32_to_fp16(val);
	}
}

MALI_STATIC void setup_flip_scale_bias_uniforms( struct gles_context *ctx,
                                                        struct gles_program_rendering_state *prs,
                                                        bs_program *program_bs)
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( prs );
	MALI_DEBUG_ASSERT_POINTER( program_bs );

	if(prs->fragcoordscale_uniform_fs_location != -1)
	{
		u32 supersample_scale = 0;

		if( mali_statebit_get( &ctx->state.common, MALI_STATE_MODE_SUPERSAMPLE_16X ) )
		{
			supersample_scale = 2;
		}

		MALI_DEBUG_ASSERT(supersample_scale % 2 == 0, ("Supersample factors supported in the following calculation must be an even number"));
		set_single_fragment_uniform(program_bs, prs, prs->fragcoordscale_uniform_fs_location + 0, (float)1/(1 << (supersample_scale/2)) ); /* x scale */
		set_single_fragment_uniform(program_bs, prs, prs->fragcoordscale_uniform_fs_location + 1, (float)1/(1 << (supersample_scale/2)) ); /* y scale */
		set_single_fragment_uniform(program_bs, prs, prs->fragcoordscale_uniform_fs_location + 2, 1.0  ); /* z scale */
	}

	/* we need to explicitly check that the start-location is not -1, since -1 + [1..3] > -1 */
	if (prs->pointcoordscalebias_uniform_fs_location  != -1)
	{
		s32 loc = prs->pointcoordscalebias_uniform_fs_location;
		if ( ctx->state.common.framebuffer.current_object->draw_flip_y ) /* rendering to default target */
		{
			set_single_fragment_uniform(program_bs, prs, loc + 0, 1.0);
			set_single_fragment_uniform(program_bs, prs, loc + 1, 1.0);
			set_single_fragment_uniform(program_bs, prs, loc + 2, 0.0);
			set_single_fragment_uniform(program_bs, prs, loc + 3, 0.0);
		}
		else /* flip pointcoord if we have a bound FBO */
		{
			set_single_fragment_uniform(program_bs, prs, loc + 0,  1.0);
			set_single_fragment_uniform(program_bs, prs, loc + 1, -1.0);
			set_single_fragment_uniform(program_bs, prs, loc + 2,  0.0);
			set_single_fragment_uniform(program_bs, prs, loc + 3,  1.0);
		}
	}

	/* we need to explicitly check that the start-location is not -1, since -1 + [1..3] > -1
	 * The uniform values are set as follows:
	 * Bound FBO, no 16x: (1,1)
	 * No Bound FBO, no 16x: (1,-1)
	 * Bound FBO, 16x: (2,2)
	 * No Bound FBO, 16x: (2,-2)
	 * */
	if (prs->derivativescale_uniform_fs_location != -1)
	{
		s32 flip = 1;
		float scale = 1.0f;

		/* Flip if we are not rendering to texture */
		if ( ctx->state.common.framebuffer.current_object->draw_flip_y ) /* rendering to default target */
		{
			flip = -1;
		}

		if( mali_statebit_get( &ctx->state.common, MALI_STATE_MODE_SUPERSAMPLE_16X ) )
		{
			scale = 2.0f;
		}

		set_single_fragment_uniform(program_bs, prs, prs->derivativescale_uniform_fs_location + 0, scale);
		set_single_fragment_uniform(program_bs, prs, prs->derivativescale_uniform_fs_location + 1, flip * scale);
	}
}

mali_err_code MALI_CHECK_RESULT _gles_fb_init_draw_call( struct gles_context *ctx, GLenum mode )
{
	mali_frame_builder *frame_builder;
	gles_fb_context    *fb_ctx;
	m200_rsw           *rsw;
	bs_program         *program_bs;
	struct gles_program_rendering_state* prs;
	mali_base_frame_id frameid; 

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->fb_ctx );
	MALI_DEBUG_ASSERT_POINTER( &ctx->state );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.current_program_rendering_state );
	prs =  ctx->state.common.current_program_rendering_state;

	fb_ctx     = ctx->fb_ctx;
	rsw        = &ctx->rsw_raster->mirror;

	frame_builder = _gles_get_current_draw_frame_builder( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	if ( MALI_TRUE == _mali_frame_builder_incremental_rendering_requested( frame_builder ) )
	{
		MALI_CHECK_NO_ERROR( _gles_incremental_render(ctx, ctx->state.common.framebuffer.current_object) );
	}

	/* shader program binary state */
	program_bs = &(prs->binary);
	if( mali_statebit_get( &ctx->state.common, MALI_STATE_VS_PRS_UPDATE_PENDING ) )
	{
		_gles_fb_apply_program_rendering_state(rsw, prs->fb_prs);
	}

	/* handle texture descriptors */
	MALI_CHECK_NO_ERROR( _gles_m200_update_texture_descriptors( ctx, fb_ctx, frame_builder, &ctx->state, program_bs ) );

	/* update built-in uniforms if they're changed */
	if ( prs->depthrange_locations_fs_in_use )
	{
		set_single_fragment_uniform(program_bs, prs, prs->depthrange_uniform_fs_location_near, ctx->state.common.viewport.z_nearfar[0]);
		set_single_fragment_uniform(program_bs, prs, prs->depthrange_uniform_fs_location_far,  ctx->state.common.viewport.z_nearfar[1]);
		set_single_fragment_uniform(program_bs, prs, prs->depthrange_uniform_fs_location_diff, ctx->state.common.viewport.z_nearfar[1] - ctx->state.common.viewport.z_nearfar[0]);
	}

	if ( prs->flip_scale_bias_locations_fs_in_use )
	{
		setup_flip_scale_bias_uniforms(ctx, prs, program_bs);
	}

	frameid = _mali_frame_builder_get_current_frame_id(frame_builder);

	/* update the uniforms */
	if( 0 == prs->binary.fragment_shader_uniforms.cellsize )
	{
		fb_ctx->current_uniform_remap_table_addr = 0;
	}
	else
	{
		if( (0 != mali_statebit_raw_get( &ctx->state.common,	( 1 << MALI_STATE_VS_PRS_UPDATE_PENDING ) |
																( 1 << MALI_STATE_FB_FRAGMENT_UNIFORM_UPDATE_PENDING ) ) ) ||
			prs->depthrange_locations_fs_in_use || 
			prs->flip_scale_bias_locations_fs_in_use ||
			(0 == fb_ctx->current_uniform_remap_table_addr) ||
			(prs->binary.num_fragment_projob_passes > 0) || 
			(fb_ctx->fb_uniform_last_frame_id != frameid) )
		{ 
			MALI_CHECK_NO_ERROR( _gles_m200_update_fragment_uniforms( ctx->frame_pool, ctx, prs ) );
			mali_statebit_unset( &ctx->state.common, MALI_STATE_FB_FRAGMENT_UNIFORM_UPDATE_PENDING );
			/* and update the frameid */
			fb_ctx->fb_uniform_last_frame_id = frameid;
		}
	}

	/* if the PRS isn't addref'ed on this frame yet, do so */
	if( prs->last_frame_used != frameid ||
		0 != mali_statebit_raw_get( &ctx->state.common,	( 1 << MALI_STATE_VS_PRS_UPDATE_PENDING ) ) )
	{
		MALI_CHECK_NO_ERROR( _gles_m200_update_shader( prs, frame_builder ) );
		/* and update the frameid */
		prs->last_frame_used = frameid;
		/* the PRS_UPDATE_PENDING bit is cleared at the end of all drawcalls  */

	}

	_gles_m200_set_rsw_parameters( ctx, fb_ctx, rsw, mode );

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC mali_err_code _gles_fb_context_init( gles_fb_context *fb_ctx )
{
	MALI_DEBUG_ASSERT_POINTER(fb_ctx);

	_mali_sys_memset( fb_ctx, 0x0, sizeof( gles_fb_context ) );

	fb_ctx->position_addr = 0;
	fb_ctx->varyings_addr = 0;
	fb_ctx->current_td_remap_table_address   = 0;
	fb_ctx->current_uniform_remap_table_addr = 0;

	MALI_SUCCESS;
}

void _gles_fb_context_deinit( gles_fb_context *fb_ctx )
{

	MALI_DEBUG_ASSERT_POINTER(fb_ctx);

	/* deref the position buffer and varyings buffer. fb_ctx is no longer referencing this memory */

	fb_ctx->position_addr = 0;
	fb_ctx->varyings_addr = 0;
}

void _gles_fb_free( struct gles_fb_context *fb_ctx );

gles_fb_context *_gles_fb_alloc( struct gles_context *ctx )
{
	mali_err_code err;
	gles_fb_context *fb_ctx;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	fb_ctx = ( gles_fb_context * ) _mali_sys_malloc( sizeof( gles_fb_context ) );
	if( fb_ctx == NULL ) return NULL;

	err = _gles_fb_context_init( fb_ctx );
	if (MALI_ERR_NO_ERROR != err)
	{
		MALI_DEBUG_ASSERT(err == MALI_ERR_OUT_OF_MEMORY, ("unexpected error: 0x%x", err));
		_gles_fb_free( fb_ctx );
		return NULL;
	}

	return fb_ctx;
}

void _gles_fb_free( struct gles_fb_context *fb_ctx )
{
	MALI_DEBUG_ASSERT_POINTER( fb_ctx );

	_gles_fb_context_deinit( fb_ctx );

	_mali_sys_free( fb_ctx );
	fb_ctx = NULL;
}
