/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <gles_base.h>
#include <gles_util.h>
#include <gles_config.h>
#include <gles_convert.h>
#include <gles_fb_interface.h>

#include "gles_framebuffer_control.h"

/*
	NOTE:
		Changes to the RSW in the M100/M200 backend for the following fields are
		updated if the "dirty" flag is set:
		- depth_is_writeable
		- color_is_writeable
		- stencil_writemask
		Any changes to these fields should set dirty to MALI_TRUE
*/

/* forward declaration */
MALI_STATIC void _gles_init_depth_mask( struct gles_context *ctx, GLboolean flag );
MALI_STATIC void _gles_init_stencil_mask( struct gles_context *ctx, GLenum face, GLuint mask );

void _gles_framebuffer_control_init( gles_context *ctx )
{
	gles_framebuffer_control *fb_control;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	fb_control = &ctx->state.common.framebuffer_control;

	_gles_color_mask( ctx, (GLboolean)GL_TRUE, (GLboolean)GL_TRUE, (GLboolean)GL_TRUE, (GLboolean)GL_TRUE );
	_gles_init_depth_mask( ctx, (GLboolean)GL_TRUE );

	_gles_init_stencil_mask( ctx, GL_FRONT_AND_BACK, (( 1U << GLES_MAX_STENCIL_BITS ) - 1) );

	_gles_clear_color( fb_control, FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 0.f ) );
	_gles_clear_depth( fb_control, FLOAT_TO_FTYPE( 1.f ) );
	_gles_clear_stencil(fb_control, 0);

}

void _gles_clear_color( struct gles_framebuffer_control *fb_control, GLftype red, GLftype green, GLftype blue, GLftype alpha )
{
	MALI_DEBUG_ASSERT_POINTER( fb_control );

	fb_control->color_clear_value[ MALI_FRAME_BUILDER_RED_INDEX   ] = CLAMP( red, FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 1.f ) );
	fb_control->color_clear_value[ MALI_FRAME_BUILDER_GREEN_INDEX ] = CLAMP( green, FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 1.f ) );
	fb_control->color_clear_value[ MALI_FRAME_BUILDER_BLUE_INDEX  ] = CLAMP( blue, FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 1.f ) );
	fb_control->color_clear_value[ MALI_FRAME_BUILDER_ALPHA_INDEX ] = CLAMP( alpha, FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 1.f ) );
}

void _gles_clear_depth( gles_framebuffer_control *fb_control, GLftype depth )
{
	MALI_DEBUG_ASSERT_POINTER( fb_control );
	fb_control->depth_clear_value = CLAMP( depth, FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 1.f ) );
}

void _gles_clear_stencil( gles_framebuffer_control *fb_control, GLint s )
{
	MALI_DEBUG_ASSERT_POINTER( fb_control );

	fb_control->stencil_clear_value = s;
}

void _gles_color_mask( struct gles_context *ctx, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	{
		gles_framebuffer_control *fb_control = &ctx->state.common.framebuffer_control;

		mali_bool mali_red   = INT_TO_BOOLEAN( red );
		mali_bool mali_green = INT_TO_BOOLEAN( green );
		mali_bool mali_blue  = INT_TO_BOOLEAN( blue );
		mali_bool mali_alpha = INT_TO_BOOLEAN( alpha );

		fb_control->color_is_writeable[ MALI_FRAME_BUILDER_RED_INDEX   ] = (GLboolean)mali_red;
		fb_control->color_is_writeable[ MALI_FRAME_BUILDER_GREEN_INDEX ] = (GLboolean)mali_green;
		fb_control->color_is_writeable[ MALI_FRAME_BUILDER_BLUE_INDEX  ] = (GLboolean)mali_blue;
		fb_control->color_is_writeable[ MALI_FRAME_BUILDER_ALPHA_INDEX ] = (GLboolean)mali_alpha;

		_gles_fb_color_mask( ctx, mali_red, mali_green, mali_blue, mali_alpha );
	}
}

MALI_STATIC void _gles_init_depth_mask( struct gles_context *ctx, GLboolean flag )
{
	mali_bool mali_flag = INT_TO_BOOLEAN( flag );
	MALI_DEBUG_ASSERT_POINTER( ctx );

	ctx->state.common.framebuffer_control.depth_is_writeable = (GLboolean)mali_flag;

	_gles_fb_init_depth_mask( ctx, mali_flag );
}

void _gles_depth_mask( struct gles_context *ctx, GLboolean flag )
{
	mali_bool mali_flag = INT_TO_BOOLEAN( flag );
	MALI_DEBUG_ASSERT_POINTER( ctx );

	ctx->state.common.framebuffer_control.depth_is_writeable = (GLboolean)mali_flag;

	_gles_fb_depth_mask( ctx, mali_flag );
}

void _gles_init_stencil_mask( struct gles_context *ctx, GLenum face, GLuint mask )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	MALI_DEBUG_ASSERT(face == GL_FRONT || face == GL_BACK || face == GL_FRONT_AND_BACK, ("Invalid face"));

	if( face == GL_FRONT || face == GL_FRONT_AND_BACK )
	{
		ctx->state.common.framebuffer_control.stencil_writemask = mask;
		_gles_fb_init_front_stencil_mask( ctx, mask );
	}
	if( face == GL_BACK || face == GL_FRONT_AND_BACK )
	{
		ctx->state.common.framebuffer_control.stencil_back_writemask = mask;
		_gles_fb_init_back_stencil_mask( ctx, mask );
	}
}

GLenum _gles_stencil_mask( struct gles_context *ctx, GLenum face, GLuint mask )
{
	const GLenum valid_enum_faces[] =
	{
		GL_FRONT, GL_BACK, GL_FRONT_AND_BACK
	};

	MALI_DEBUG_ASSERT_POINTER( ctx );

	if( MALI_TRUE == _gles_verify_enum( valid_enum_faces, (u32)MALI_ARRAY_SIZE( valid_enum_faces ), face ) )
	{
		if( face == GL_FRONT || face == GL_FRONT_AND_BACK )
		{
			ctx->state.common.framebuffer_control.stencil_writemask = mask;
			_gles_fb_front_stencil_mask( ctx, mask );
		}
		if( face == GL_BACK || face == GL_FRONT_AND_BACK )
		{
			ctx->state.common.framebuffer_control.stencil_back_writemask = mask;
			_gles_fb_back_stencil_mask( ctx, mask );
		}
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}
