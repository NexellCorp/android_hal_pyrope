/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_debug.h>
#include <gles_fb_interface.h>
#include <gles_common_state/gles_pixel_operations.h>

#include "gles2_pixel_operations.h"

MALI_EXPORT GLenum _gles2_stencil_op( struct gles_context *ctx, GLenum face, GLenum sfail, GLenum zfail, GLenum zpass )
{
	static const GLenum valid_enum_actions[] =
	{
		GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP
	};
	static const GLenum valid_enum_faces[] =
	{
		GL_FRONT, GL_BACK, GL_FRONT_AND_BACK
	};

	if( MALI_TRUE == _gles_verify_enum( valid_enum_actions, (u32)MALI_ARRAY_SIZE( valid_enum_actions ), sfail  ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_actions, (u32)MALI_ARRAY_SIZE( valid_enum_actions ), zfail ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_actions, (u32)MALI_ARRAY_SIZE( valid_enum_actions ), zpass ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_faces, (u32)MALI_ARRAY_SIZE( valid_enum_faces ), face ) )
	{
		const u8 mali_sfail = _gles_m200_gles_to_mali_stencil_operation( sfail );
		const u8 mali_zfail = _gles_m200_gles_to_mali_stencil_operation( zfail );
		const u8 mali_zpass = _gles_m200_gles_to_mali_stencil_operation( zpass );

		if( face == GL_FRONT || face == GL_FRONT_AND_BACK )
		{
			_gles_fb_front_stencil_op( ctx, mali_sfail, mali_zfail, mali_zpass );
		}
		if( face == GL_BACK || face == GL_FRONT_AND_BACK )
		{
			_gles_fb_back_stencil_op( ctx, mali_sfail, mali_zfail, mali_zpass );
		}
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

MALI_EXPORT GLenum _gles2_blend_func( struct gles_context *ctx, GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha )
{
	static const GLenum valid_enum_src[] =
	{
		GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
		GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR,
		GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_SRC_ALPHA_SATURATE
	};
	static const GLenum valid_enum_dst[] =
	{
		GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
		GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR,
		GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA
	};

	if( MALI_TRUE == _gles_verify_enum( valid_enum_src, (u32)MALI_ARRAY_SIZE( valid_enum_src ), src_rgb ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_dst, (u32)MALI_ARRAY_SIZE( valid_enum_dst ), dst_rgb ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_src, (u32)MALI_ARRAY_SIZE( valid_enum_src ), src_alpha ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_dst, (u32)MALI_ARRAY_SIZE( valid_enum_dst ), dst_alpha ) )
	{
		const u8 mali_src = _gles_m200_gles_to_mali_blend_func( src_rgb );
		const u8 mali_dst = _gles_m200_gles_to_mali_blend_func( dst_rgb );
		const u8 mali_alpha_src = _gles_m200_gles_to_mali_blend_func( src_alpha );
		const u8 mali_alpha_dst = _gles_m200_gles_to_mali_blend_func( dst_alpha );

		_gles_fb_blend_func( ctx, mali_src, mali_dst, mali_alpha_src, mali_alpha_dst );
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

MALI_EXPORT GLenum _gles2_blend_equation( struct gles_context *ctx, GLenum mode_rgb, GLenum mode_alpha )
{
	static const GLenum valid_enum_blend_modes[] =
	{
		GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT
#if EXTENSION_BLEND_MINMAX_EXT_ENABLE
		, GL_MIN_EXT, GL_MAX_EXT
#endif
	};

	if( MALI_TRUE == _gles_verify_enum( valid_enum_blend_modes, (u32)MALI_ARRAY_SIZE( valid_enum_blend_modes ), mode_rgb ) &&
	    MALI_TRUE == _gles_verify_enum( valid_enum_blend_modes, (u32)MALI_ARRAY_SIZE( valid_enum_blend_modes ), mode_alpha ) )
	{
		const u8 mali_mode_rgb = _gles_m200_gles_to_mali_blend_equation( mode_rgb );
		const u8 mali_mode_alpha = _gles_m200_gles_to_mali_blend_equation( mode_alpha );

		_gles_fb_blend_equation( ctx, mali_mode_rgb, mali_mode_alpha );

#if EXTENSION_BLEND_MINMAX_EXT_ENABLE
	{
		/* The GLES max-min blend is defined to ignore the blend weighting factors. So they will behave
		 * as if the blendfunc was GL_ONE, GL_ZERO. The code below ensures that this occurs if the blend equation
		 * is either MAX or MIN, and uses the regular blendfunc if not.
		 */
		mali_rsw_raster *rsw = ctx->rsw_raster;
		const int maxmin_rgb   = GL_MIN_EXT == mode_rgb || GL_MAX_EXT == mode_rgb;
		const int maxmin_alpha = GL_MIN_EXT == mode_alpha || GL_MAX_EXT == mode_alpha;
		if ( maxmin_rgb != 0 && maxmin_alpha != 0 )
		{
			_gles_fb_blend_func( ctx, M200_RSW_BLEND_ONE, M200_RSW_BLEND_ZERO, M200_RSW_BLEND_ONE, M200_RSW_BLEND_ZERO );
		}
		else if ( maxmin_rgb != 0 )
		{
			_gles_fb_blend_func( ctx, M200_RSW_BLEND_ONE, M200_RSW_BLEND_ZERO, rsw->blend_alpha_src, rsw->blend_alpha_dst );
		}
		else if ( maxmin_alpha != 0 )
		{
			_gles_fb_blend_func( ctx, rsw->blend_src, rsw->blend_dst, M200_RSW_BLEND_ONE, M200_RSW_BLEND_ZERO );
		}
		else
		{
			_gles_fb_blend_func( ctx, rsw->blend_src, rsw->blend_dst, rsw->blend_alpha_src, rsw->blend_alpha_dst );
		}
	}
#endif
	}
	else
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

MALI_EXPORT GLenum _gles2_blend_color( gles_context *ctx, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	_gles_fb_blend_color( ctx, red, green, blue, alpha, _mali_frame_builder_get_fp16_flag( _gles_get_current_draw_frame_builder( ctx ) ) );

	GL_SUCCESS;
}
