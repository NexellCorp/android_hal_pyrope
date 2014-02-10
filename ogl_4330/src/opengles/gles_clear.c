/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_clear.h"
#include "gles_context.h"
#include "gles_draw.h"
#include "gles_fb_interface.h"
#include "gles_convert.h"
#include <gles_common_state/gles_framebuffer_state.h>
#include "gles_framebuffer_object.h"
#include "shared/m200_gp_frame_builder_inlines.h"
#include <shared/m200_gp_frame_builder.h>

MALI_STATIC_INLINE void _gles_set_clear_color( struct gles_context *ctx, struct mali_frame_builder *frame_builder, s32 component_index )
{
	u32 buffer_type = 1<<component_index;
	u32 color_clear_value = 0;
	gles_framebuffer_control *framebuffer_control;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT( (MALI_FRAME_BUILDER_RED_INDEX==component_index) ||
				(MALI_FRAME_BUILDER_GREEN_INDEX==component_index) ||
				(MALI_FRAME_BUILDER_BLUE_INDEX==component_index) ||
				(MALI_FRAME_BUILDER_ALPHA_INDEX==component_index),
				("component_index is not a proper color channel name\n"));

	framebuffer_control = &((gles_context*)ctx)->state.common.framebuffer_control;

	/* storing the clear value with 16bits fixed point precision (using only the 8 major bits) */
	color_clear_value = ((s32)(framebuffer_control->color_clear_value[component_index]*0xFF + 0.5f)<<8);

	if ( GL_FALSE == ctx->state.common.framebuffer_control.color_is_writeable[ component_index ] )
	{
		color_clear_value = _mali_frame_builder_get_clear_value( frame_builder, buffer_type);
	}

	/* special check for alpha */
	if(MALI_FRAME_BUILDER_ALPHA_INDEX == component_index)
	{
		/* buffers without alpha capabilities should use 1.0 instead. This will ensure WBx output on alpha channel is 1.0 */
		if( 0 == _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_ALPHA_BITS ) ) 
		{
			color_clear_value = 0xffff;
		}
	}

	_mali_frame_builder_set_clear_value( frame_builder, buffer_type, color_clear_value);
}

MALI_STATIC_INLINE void _gles_set_clear_depth( struct gles_context *ctx, struct mali_frame_builder *frame_builder )
{
	u32 clear_depth;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	/* depth clear value is stored as float, so we get max precision */
	clear_depth = (u32) (((1<<24)-1) * ((gles_context*)ctx)->state.common.framebuffer_control.depth_clear_value);
	_mali_frame_builder_set_clear_value( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH, clear_depth );
}

MALI_STATIC_INLINE void _gles_set_clear_stencil( struct gles_context *ctx, struct mali_frame_builder *frame_builder )
{
	u32 clear_stencil;
	u32 clear_value;
	u32 write_mask;
	u32 stencil_bit;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	clear_value   = ctx->state.common.framebuffer_control.stencil_clear_value;
	write_mask    = ctx->state.common.framebuffer_control.stencil_writemask;
	stencil_bit   = _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_STENCIL_BITS );

	/* apply stencil write-mask to the clear-value */
	clear_stencil = ( clear_value & write_mask & ((1<<stencil_bit)-1) );

	_mali_frame_builder_set_clear_value( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL, clear_stencil );
}

GLenum _gles_clear( struct gles_context *ctx, GLbitfield mask )
{
	mali_frame_builder     *frame_builder = NULL;

	mali_bool               clear_full = MALI_FALSE;

	GLbitfield              buffer_mask = 0;
	GLbitfield              limit_mask = 0;
	u32                     frame_builder_get_clearstate_mask = 0;

	mali_err_code           err = MALI_ERR_NO_ERROR;
	GLenum                  gerr = GL_NO_ERROR;
	mali_bool               scissor_in_use = MALI_FALSE;


	MALI_DEBUG_ASSERT_POINTER( ctx );

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	gerr = _gles_fbo_internal_draw_setup( ctx );
	MALI_CHECK( GL_NO_ERROR == gerr, gerr );
#endif

	if( GL_NO_ERROR == gerr )
	{
		/* GL_INVALID_VALUE is generated if any bit other than the three defined bits is set in mask. */
		if ( (mask & ~( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT )) != 0 ) gerr = GL_INVALID_VALUE;
	}

	/* clear nothing */
	if( GL_NO_ERROR != gerr )
	{
		return gerr;
	}

	frame_builder = _gles_get_current_draw_frame_builder( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	/* Early-out if dealing with a zero-sized scissor box */
	if ( GL_TRUE == _gles_scissor_zero_size_check( ctx, frame_builder ) )
	{
		return GL_NO_ERROR;
	}

	if( mali_statebit_get( &ctx->state.common, GLES_STATE_SCISSOR_ENABLED ) )
	{
		int supersample_scalef = _gles_get_current_draw_supersample_scalefactor(ctx);
		if ( (ctx->state.common.scissor.x == 0) &&
		     (ctx->state.common.scissor.y == 0) &&
		     (supersample_scalef * ctx->state.common.scissor.width == _mali_frame_builder_get_width(frame_builder)) &&
		     (supersample_scalef * ctx->state.common.scissor.height == _mali_frame_builder_get_height(frame_builder)))
		{
			/* treat fullscreen scissor as if scissoring was disabled */
			scissor_in_use = MALI_FALSE;
		} else
		{
			scissor_in_use = MALI_TRUE;
		}
	}

	/* fetch the clearstate of the current framebuilder */
	frame_builder_get_clearstate_mask = _mali_frame_builder_get_clearstate( frame_builder );

	if (mask & GL_COLOR_BUFFER_BIT)
	{
		limit_mask |= ctx->state.common.framebuffer_control.color_is_writeable[0] ? MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_R : 0;
		limit_mask |= ctx->state.common.framebuffer_control.color_is_writeable[1] ? MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_G : 0;
		limit_mask |= ctx->state.common.framebuffer_control.color_is_writeable[2] ? MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_B : 0;
		limit_mask |= ctx->state.common.framebuffer_control.color_is_writeable[3] ? MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_A : 0;
	}

	if (mask & GL_DEPTH_BUFFER_BIT)
	{
		limit_mask |= (ctx->state.common.framebuffer_control.depth_is_writeable) ? MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH : 0;
	}

	if (mask & GL_STENCIL_BUFFER_BIT)
	{
		mali_bool value_intersect_mask = ctx->state.common.framebuffer_control.stencil_writemask & ctx->state.common.framebuffer_control.stencil_clear_value;
		limit_mask |= value_intersect_mask ? MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL : 0;
	}

	/* binary "OR" the clearstate mask of the current framebuilder to the limit_mask so that we re-clear buffers that are already cleared */
	buffer_mask = frame_builder_get_clearstate_mask | limit_mask;

	/* if the scissor box is enabled the clear operation is confined and must be performed using a clear quad */
	if (GL_FALSE == scissor_in_use)
	{
		/* if we are able to do a full clear */
		clear_full =	(MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS == (buffer_mask & MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS)) &&
				(buffer_mask & (MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH)) &&
				(buffer_mask & (MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL));
	}

	if (MALI_TRUE == clear_full)
	{
		MALI_DEBUG_ASSERT_POINTER(ctx->state.common.framebuffer.current_object);

		err = _gles_clean_frame(ctx);
		_mali_frame_builder_set_clearstate( frame_builder, limit_mask);	

		if( limit_mask & MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_R ) _gles_set_clear_color( ctx, frame_builder, MALI_FRAME_BUILDER_RED_INDEX );
		if( limit_mask & MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_G ) _gles_set_clear_color( ctx, frame_builder, MALI_FRAME_BUILDER_GREEN_INDEX );
		if( limit_mask & MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_B ) _gles_set_clear_color( ctx, frame_builder, MALI_FRAME_BUILDER_BLUE_INDEX );
		if( limit_mask & MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_A ) _gles_set_clear_color( ctx, frame_builder, MALI_FRAME_BUILDER_ALPHA_INDEX );
		if( limit_mask & MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH   ) _gles_set_clear_depth( ctx, frame_builder );
		if( limit_mask & MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL ) _gles_set_clear_stencil( ctx, frame_builder );

		MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object );
		ctx->state.common.framebuffer.current_object->num_verts_drawn_per_frame = 0;
	}
	else
	{
		u32 frame_builder_set_clearstate_mask = 0;
		/* the buffers that'll be cleared with the fullscreen quad can now be flagged as clean */
		if (buffer_mask  & ((MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_R) |
					(MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_G) | 
					(MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_B) | 
					(MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_A)))
		{
			if((mask & GL_COLOR_BUFFER_BIT) && (MALI_FALSE == scissor_in_use)) frame_builder_set_clearstate_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS;
		}

		if (buffer_mask & (MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH))
		{
			if((mask & GL_DEPTH_BUFFER_BIT) && (MALI_FALSE == scissor_in_use)) frame_builder_set_clearstate_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH;
		}

		if (buffer_mask & (MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL))
		{
			if((mask & GL_STENCIL_BUFFER_BIT) && (MALI_FALSE == scissor_in_use)) frame_builder_set_clearstate_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL;
		}

		err = _gles_drawcall_begin( ctx );
		MALI_CHECK( MALI_ERR_NO_ERROR == err, _gles_convert_mali_err( err ) );

		err = _gles_draw_clearquad( ctx, mask );
		_gles_drawcall_end(ctx);

		if(MALI_FALSE == scissor_in_use) _mali_frame_builder_set_clearstate( frame_builder, frame_builder_set_clearstate_mask );	
	}

	MALI_CHECK( MALI_ERR_NO_ERROR == err, _gles_convert_mali_err( err ) );

#if HARDWARE_ISSUE_7571_7572
	ctx->state.common.framebuffer.current_object->framebuffer_has_triangles = MALI_FALSE;
#endif

	GL_SUCCESS;
}

