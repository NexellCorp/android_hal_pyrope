/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <gles_context.h>
#include <gles_config.h>
#include <gles_config_extension.h>
#include <gles_util.h>
#include <gles_fb_interface.h>

#include "gles1_pixel_operations.h"

static const GLenum valid_enum_conditionals[] =
{
	GL_NEVER, GL_LESS, GL_GEQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_EQUAL, GL_ALWAYS
};

static const GLenum valid_enum_actions[] =
{
	GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT
};

static const GLenum valid_enum_sfactors[] =
{
	GL_ZERO, GL_ONE, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE
};

static const GLenum valid_enum_dfactors[] =
{
	GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA
};

static const GLenum valid_enum_logic_ops[] =
{
	GL_CLEAR, GL_SET, GL_COPY, GL_COPY_INVERTED, GL_NOOP, GL_INVERT, GL_AND, GL_NAND, GL_OR, GL_NOR, GL_XOR,
	GL_EQUIV, GL_AND_REVERSE, GL_AND_INVERTED, GL_OR_REVERSE, GL_OR_INVERTED
};

MALI_EXPORT GLenum _gles1_alpha_func( struct gles_context *ctx, GLenum func, GLftype ref )
{
	if( MALI_TRUE == _gles_verify_enum( valid_enum_conditionals, (u32)MALI_ARRAY_SIZE( valid_enum_conditionals ), func ) )
	{
		const u8 mali_func = _gles_m200_gles_to_mali_conditional( func );
		const mali_float mali_ref  = CLAMP( ref, FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 1.f ) );

		_gles_fb_alpha_func( ctx, mali_func, mali_ref );
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

MALI_EXPORT GLenum _gles1_stencil_op( struct gles_context *ctx, GLenum face, GLenum sfail, GLenum zfail, GLenum zpass )
{
	MALI_IGNORE( face );

	if( MALI_TRUE == _gles_verify_enum( valid_enum_actions, (u32)MALI_ARRAY_SIZE( valid_enum_actions ), sfail  ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_actions, (u32)MALI_ARRAY_SIZE( valid_enum_actions ), zfail )  &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_actions, (u32)MALI_ARRAY_SIZE( valid_enum_actions ), zpass ) )
	{
		const u8 mali_sfail = _gles_m200_gles_to_mali_stencil_operation( sfail );
		const u8 mali_zfail = _gles_m200_gles_to_mali_stencil_operation( zfail );
		const u8 mali_zpass = _gles_m200_gles_to_mali_stencil_operation( zpass );

		_gles_fb_front_stencil_op( ctx, mali_sfail, mali_zfail, mali_zpass );
		_gles_fb_back_stencil_op( ctx, mali_sfail, mali_zfail, mali_zpass );
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

MALI_EXPORT GLenum _gles1_blend_func( struct gles_context *ctx, GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha )
{
	/* This is intentional, 1.1 doesn't have "Separate" calls
	 */
	MALI_IGNORE( src_alpha );
	MALI_IGNORE( dst_alpha );

	if( MALI_TRUE == _gles_verify_enum( valid_enum_sfactors, (u32)MALI_ARRAY_SIZE( valid_enum_sfactors ), src_rgb ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_dfactors, (u32)MALI_ARRAY_SIZE( valid_enum_dfactors ), dst_rgb ) )
	{
		const u8 mali_src = _gles_m200_gles_to_mali_blend_func( src_rgb );
		const u8 mali_dst = _gles_m200_gles_to_mali_blend_func( dst_rgb );
		const u8 mali_alpha_src = _gles_m200_gles_to_mali_blend_func( src_rgb );
		const u8 mali_alpha_dst = _gles_m200_gles_to_mali_blend_func( dst_rgb );

		_gles_fb_blend_func( ctx, mali_src, mali_dst, mali_alpha_src, mali_alpha_dst );
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

MALI_EXPORT GLenum _gles1_logic_op( struct gles_context *ctx, GLenum opcode )
{
	if( MALI_TRUE == _gles_verify_enum( valid_enum_logic_ops, (u32)MALI_ARRAY_SIZE( valid_enum_logic_ops ), opcode ) )
	{
		const u8 mali_opcode = _gles_m200_gles_to_mali_logicop( opcode );

		_gles_fb_logic_op( ctx, mali_opcode );
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}
	/* gles1 has a system of dirty bit protection to avoid repeated fragment uniform updates */
	mali_statebit_set( &ctx->state.common, GLES_STATE_COLOR_ATTACHMENT_DIRTY);

	GL_SUCCESS;
}
