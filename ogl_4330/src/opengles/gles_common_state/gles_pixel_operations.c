/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_util.h>
#include <gles_config.h>
#include <gles_fb_interface.h>

#include "gles_pixel_operations.h"

/* Used for depth func and stencil func parameter error-checking */
static const GLenum valid_enum_conditionals[] =
{
	GL_NEVER, GL_LESS, GL_GEQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_EQUAL, GL_ALWAYS
};


/* The MSB of last_state (see struct gles_scissor) is reserved to mark
 * an 'invalid' or initial state.
 *
 * Since this bit is reserved, the value of last_state can never match
 * the current state - hence forcing an update to occur.
 */
static const u32 invalid_last_scissor_state = (1u << 31);


MALI_EXPORT GLenum _gles_stencil_func( gles_context *ctx, GLenum face, GLenum func, GLint ref, GLuint mask )
{
	static const GLenum valid_enum_faces[] =
	{
		GL_FRONT, GL_BACK, GL_FRONT_AND_BACK
	};

	MALI_DEBUG_ASSERT_POINTER( ctx );

	if( MALI_TRUE == _gles_verify_enum( valid_enum_conditionals, (u32)MALI_ARRAY_SIZE( valid_enum_conditionals ), func ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_faces, (u32)MALI_ARRAY_SIZE( valid_enum_faces ), face ) )
	{
		const u8 mali_func = _gles_m200_gles_to_mali_conditional( func );
		/* mask and ref is not clamped according to spec, so we pass it through unchanged */

		if( face == GL_FRONT || face == GL_FRONT_AND_BACK )
		{
			_gles_fb_front_stencil_func( ctx, mali_func, ref, mask );
		}
		if( face == GL_BACK || face == GL_FRONT_AND_BACK )
		{
			_gles_fb_back_stencil_func( ctx, mali_func, ref, mask );
		}
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

MALI_EXPORT GLenum _gles_depth_func( struct gles_context *ctx, GLenum func )
{
	if( MALI_TRUE == _gles_verify_enum( valid_enum_conditionals, (u32)MALI_ARRAY_SIZE( valid_enum_conditionals ), func ) )
	{
		const u8 mali_func = _gles_m200_gles_to_mali_conditional( func );

		_gles_fb_depth_func( ctx, mali_func );
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

MALI_EXPORT void _gles_pixel_operations_init_scissor( struct gles_context *const ctx )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	{
		struct gles_scissor * const scissor = &ctx->state.common.scissor;

		MALI_DEBUG_ASSERT_POINTER( scissor );

		scissor->x = 0;
		scissor->y = 0;
		scissor->width = 0;
		scissor->height = 0;
		scissor->last_state = invalid_last_scissor_state;

		mali_statebit_unset( &ctx->state.common, GLES_STATE_SCISSOR_ENABLED );
		mali_statebit_set( &ctx->state.common, MALI_STATE_SCISSOR_DIRTY );
	}
}

MALI_EXPORT GLenum _gles_scissor( struct gles_context *const ctx, GLint x, GLint y, GLsizei width, GLsizei height )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	{
		struct gles_scissor *const scissor = &ctx->state.common.scissor;

		MALI_DEBUG_ASSERT_POINTER( scissor );

		/* check for input-errors */
		if( width < 0 || height < 0 )
		{
			MALI_ERROR( GL_INVALID_VALUE );
		}

		/* update state */
		scissor->x = x;
		scissor->y = y;
		scissor->width = width;
		scissor->height = height;

		mali_statebit_set( &ctx->state.common, MALI_STATE_SCISSOR_DIRTY );
	}
	GL_SUCCESS;
}
