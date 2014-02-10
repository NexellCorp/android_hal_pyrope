/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_M200_RSW_H
#define GLES_M200_RSW_H

#include <mali_system.h>
#include <gles_context.h>
#include <gles_util.h>

#include <mali_rsw.h>
#include "gles_m200_rsw_map.h"

#include "shared/m200_gp_frame_builder.h"
#include "shared/m200_gp_frame_builder_inlines.h"


enum rsw_enable_bit
{
	M200_ALPHA_TEST,
	M200_DEPTH_TEST,
	M200_BLEND,
	M200_COLOR_LOGIC_OP,
	M200_STENCIL_TEST,
	M200_MULTISAMPLE,
	M200_SAMPLE_COVERAGE,
	M200_POLYGON_OFFSET_FILL,
	M200_LINE_SMOOTH,
	M200_POINT_SMOOTH,
	M200_SAMPLE_ALPHA_TO_COVERAGE,
	M200_SAMPLE_ALPHA_TO_ONE,
	M200_DEPTH_MASK,
	M200_RSW_MULTISAMPLE_INVERT,
	M200_RSW_DITHER,
};

typedef struct mali_rsw_raster
{
	m200_rsw mirror;

	/* Enable/Disable functionality requires we overwrite bits in the RSW.
	 * As a result of this, we need to store some redundant data here for
	 * restoring state when Enable is called
	 */
	u32 enable_bits;

	u8 alpha_func;
	u8 alpha_ref;

	u8 blend_mode_rgb;
	u8 blend_mode_alpha;
	u8 blend_src;
	u8 blend_dst;
	u8 blend_alpha_src;
	u8 blend_alpha_dst;
	u8 color_logic_op;

	u8 depth_func;

	u8 front_stencil_func;
	u32 front_stencil_ref;
	u32 front_stencil_mask;
	u32 front_stencil_write_mask;
	u8 front_stencil_sfail;
	u8 front_stencil_zfail;
	u8 front_stencil_zpass;

	u8 back_stencil_func;
	u32 back_stencil_ref;
	u32 back_stencil_mask;
	u32 back_stencil_write_mask;
	u8 back_stencil_sfail;
	u8 back_stencil_zfail;
	u8 back_stencil_zpass;

	mali_float multisample_coverage;
	u8 polygon_offset_factor;
	u8 polygon_offset_units;

	/* Lossy encoding in RSW's requires redundant data for API Gets
	 * functionality
	 */
	GLclampf opengl_blend_r;
	GLclampf opengl_blend_g;
	GLclampf opengl_blend_b;
	GLclampf opengl_blend_a;

	GLclampf opengl_alpha_ref;

	GLfloat opengl_polygon_offset_factor;
	GLfloat opengl_polygon_offset_units;

	GLuint opengl_front_stencil_mask;
	GLuint opengl_back_stencil_mask;

} mali_rsw_raster;

MALI_STATIC_FORCE_INLINE mali_bool _gles_fb_enables_get( mali_rsw_raster *rsw_raster, enum rsw_enable_bit which )
{
	/* Note: This must return 1 or 0, as the result is used directly to encode
	 *       RSW's, hence the downshift and mask
	 */
	return ( rsw_raster->enable_bits >> which ) & 1U;
}

MALI_STATIC_FORCE_INLINE void _gles_fb_enables_set( mali_rsw_raster *rsw_raster, enum rsw_enable_bit which, u32 value )
{
	MALI_DEBUG_ASSERT( value == 0 || value == 1, ("ill conditioned bit vector value") );
	rsw_raster->enable_bits &= ~( 1U << which );
	rsw_raster->enable_bits |= value << which;
}


MALI_STATIC_INLINE void _gles_fb_blend_color( gles_context *ctx, GLclampf r, GLclampf g, GLclampf b, GLclampf a, mali_bool fp16_format  )
{
    mali_rsw_raster *rsw = ctx->rsw_raster;
        
    rsw->opengl_blend_r = CLAMP( r, 0.0f, 1.0f );
    rsw->opengl_blend_g = CLAMP( g, 0.0f, 1.0f );
    rsw->opengl_blend_b = CLAMP( b, 0.0f, 1.0f );
    rsw->opengl_blend_a = CLAMP( a, 0.0f, 1.0f );

    if (!fp16_format)
    {
       __m200_rsw_encode( &rsw->mirror, M200_RSW_CONSTANT_BLEND_COLOR_RED,   (u8)(rsw->opengl_blend_r * MALI_U8_MAX) );
       __m200_rsw_encode( &rsw->mirror, M200_RSW_CONSTANT_BLEND_COLOR_GREEN, (u8)(rsw->opengl_blend_g * MALI_U8_MAX) );
       __m200_rsw_encode( &rsw->mirror, M200_RSW_CONSTANT_BLEND_COLOR_BLUE,  (u8)(rsw->opengl_blend_b * MALI_U8_MAX) );
       __m200_rsw_encode( &rsw->mirror, M200_RSW_CONSTANT_BLEND_COLOR_ALPHA, (u8)(rsw->opengl_blend_a * MALI_U8_MAX) );
   }
   else
   {
       __m200_rsw_encode(  &rsw->mirror, M200_RSW_CONSTANT_BLEND_COLOR_RED,   _gles_fp32_to_fp16_predefined(rsw->opengl_blend_r) );
       __m200_rsw_encode(  &rsw->mirror, M200_RSW_CONSTANT_BLEND_COLOR_GREEN, _gles_fp32_to_fp16_predefined (rsw->opengl_blend_g));
       __m200_rsw_encode(  &rsw->mirror, M200_RSW_CONSTANT_BLEND_COLOR_BLUE,  _gles_fp32_to_fp16_predefined (rsw->opengl_blend_b));
       __m200_rsw_encode(  &rsw->mirror, M200_RSW_CONSTANT_BLEND_COLOR_ALPHA, _gles_fp32_to_fp16_predefined (rsw->opengl_blend_a));       
    }

}

/**
 * Sets the blend equation.
 */
void _gles_fb_blend_equation( gles_context *ctx, u8 mode_rgb, u8 mode_alpha );

/**
 * Applies the dither setting to the RSW word, conditional on Logic_op and tile buffer format.
 */
void _gles_fb_init_apply_dither( gles_context *ctx, mali_bool flag );
void _gles_fb_apply_dither( gles_context *ctx, mali_bool flag );


MALI_STATIC_INLINE u8 no_dst_alpha_fix_rgb( u8 mali_factor )
{
	switch( mali_factor )
	{
		case M200_RSW_BLEND_DST_ALPHA:           return M200_RSW_BLEND_ONE;
		case M200_RSW_BLEND_ONE_MINUS_DST_ALPHA: return M200_RSW_BLEND_ZERO;
		case M200_RSW_BLEND_SRC_ALPHA_SATURATE:  return M200_RSW_BLEND_ZERO;
	}
	return mali_factor;
}

MALI_STATIC_INLINE u8 no_dst_alpha_fix_alpha( u8 mali_factor )
{
	switch( mali_factor )
	{
		case M200_RSW_BLEND_ONE_MINUS_DST_ALPHA: return M200_RSW_BLEND_ZERO;
		case M200_RSW_BLEND_DST_ALPHA:
		case M200_RSW_BLEND_SRC_ALPHA_SATURATE:  return M200_RSW_BLEND_ONE;
	}
	return mali_factor;
}


MALI_STATIC_INLINE void _gles_fb_init_blend_func( gles_context *ctx, u8 src, u8 dst, u8 alpha_src, u8 alpha_dst )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->blend_src = src;
	rsw->blend_dst = dst;
	rsw->blend_alpha_src = alpha_src;
	rsw->blend_alpha_dst = alpha_dst;

	/* Disabled blending is encoded as 1 * Cs + 0 * Cd */
	src = alpha_src = M200_RSW_BLEND_ONE;
	dst = alpha_dst = M200_RSW_BLEND_ZERO;

	/* Set the blend operation again as it may have been overwritten by a logic op
	 */
	_gles_fb_blend_equation( ctx, rsw->blend_mode_rgb, rsw->blend_mode_alpha );

	/* If the egl surface has alpha, we must change blend operations as Mali
	 * always has destination alpha internally. */
	src = no_dst_alpha_fix_rgb( src );
	dst = no_dst_alpha_fix_rgb( dst );
	alpha_src = no_dst_alpha_fix_alpha( alpha_src );
	alpha_dst = no_dst_alpha_fix_alpha( alpha_dst );

	/* alpha saturate (see OpenGL ES full spec 1.1.10 Table 4.1 for source alpha behavior)
	 * Note: this mapping must occur after the no_dst_alpha fixup.
	 */
	if( alpha_src == M200_RSW_BLEND_SRC_ALPHA_SATURATE ) alpha_src = M200_RSW_BLEND_ONE;

	/* Set blend functions, alpha blends are masked as they do not need to
	 * specify the alpha expand bit.
	 */
	__m200_rsw_encode( &rsw->mirror, M200_RSW_RGB_S_SOURCE_COMBINED, src );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_S_SOURCE_COMBINED, M200_RSW_ALPHA_BLEND_MASK & alpha_src );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_RGB_D_SOURCE_COMBINED, dst );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_D_SOURCE_COMBINED, M200_RSW_ALPHA_BLEND_MASK & alpha_dst );
}

MALI_STATIC_INLINE void _gles_fb_blend_func( gles_context *ctx, u8 src, u8 dst, u8 alpha_src, u8 alpha_dst )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->blend_src = src;
	rsw->blend_dst = dst;
	rsw->blend_alpha_src = alpha_src;
	rsw->blend_alpha_dst = alpha_dst;

	/* If logic op is enabled, we don't change the actual rendering behavior
	 */
	if( MALI_TRUE == _gles_fb_enables_get( rsw, M200_COLOR_LOGIC_OP ) ) return;

	/* Disabled blending is encoded as 1 * Cs + 0 * Cd
	 */
	if( MALI_FALSE == _gles_fb_enables_get( rsw, M200_BLEND ) )
	{
		src = alpha_src = M200_RSW_BLEND_ONE;
		dst = alpha_dst = M200_RSW_BLEND_ZERO;
	}

	/* Set the blend operation again as it may have been overwritten by a logic op
	 */
	_gles_fb_blend_equation( ctx, rsw->blend_mode_rgb, rsw->blend_mode_alpha );

	/* If the egl surface has alpha, we must change blend operations as Mali
	 * always has destination alpha internally.
	 */
	if( 0 == _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_ALPHA_BITS ) )
	{
		src = no_dst_alpha_fix_rgb( src );
		dst = no_dst_alpha_fix_rgb( dst );
		alpha_src = no_dst_alpha_fix_alpha( alpha_src );
		alpha_dst = no_dst_alpha_fix_alpha( alpha_dst );
	}

	/* alpha saturate (see OpenGL ES full spec 1.1.10 Table 4.1 for source alpha behavior)
	 * Note: this mapping must occur after the no_dst_alpha fixup.
	 */
	if( alpha_src == M200_RSW_BLEND_SRC_ALPHA_SATURATE ) alpha_src = M200_RSW_BLEND_ONE;

	/* Set blend functions, alpha blends are masked as they do not need to
	 * specify the alpha expand bit.
	 */
	__m200_rsw_encode( &rsw->mirror, M200_RSW_RGB_S_SOURCE_COMBINED, src );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_S_SOURCE_COMBINED, M200_RSW_ALPHA_BLEND_MASK & alpha_src );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_RGB_D_SOURCE_COMBINED, dst );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_D_SOURCE_COMBINED, M200_RSW_ALPHA_BLEND_MASK & alpha_dst );
}

MALI_STATIC_INLINE void _gles_fb_set_blend( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_BLEND, flag );

	_gles_fb_blend_func( ctx, rsw->blend_src, rsw->blend_dst, rsw->blend_alpha_src, rsw->blend_alpha_dst );
}

MALI_STATIC_INLINE void _gles_fb_logic_op( gles_context *ctx, u8 logic_op )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->color_logic_op = logic_op;

	/* Logic op has no RSW "default", behavior falls back to a blending op
	 */
	if( MALI_TRUE != _gles_fb_enables_get( rsw, M200_COLOR_LOGIC_OP ) ) return;

	/* this is done as we may be switching from blending mode and need to
	 * re-set the logic op function */
	__m200_rsw_encode( &rsw->mirror, M200_RSW_RGB_BLEND_FUNC, M200_BLEND_LOGICOP_MODE );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_BLEND_FUNC, M200_BLEND_LOGICOP_MODE );

	__m200_rsw_encode( &rsw->mirror, M200_RSW_RGB_LOGIC_OP_TRUTH_TABLE,	logic_op );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_LOGIC_OP_TRUTH_TABLE, logic_op );
}

MALI_STATIC_INLINE void _gles_fb_set_color_logic_op( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_COLOR_LOGIC_OP, flag );

	/* Logic op overrides dither, with the complication that in some HW cases, Logic_op needs dither to be on */
	_gles_fb_apply_dither(ctx, _gles_fb_enables_get( rsw, M200_RSW_DITHER ));

	/*  Logic op overrides blend (see OpenGL ES full spec 1.1.12 Section 4.1.9)
	 */
	if( flag )
	{
		_gles_fb_logic_op( ctx, rsw->color_logic_op );
	}
	else
	{
		_gles_fb_blend_func( ctx, rsw->blend_src, rsw->blend_dst, rsw->blend_alpha_src, rsw->blend_alpha_dst );
	}
}

MALI_STATIC_INLINE void _gles_fb_color_mask( gles_context *ctx, mali_bool r, mali_bool g, mali_bool b, mali_bool a )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_R_WRITE_MASK, r );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_G_WRITE_MASK, g );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_B_WRITE_MASK, b );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_A_WRITE_MASK, a );
}

MALI_STATIC_INLINE void _gles_fb_polygon_offset_mali( gles_context *ctx, u8 factor, u8 units )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	MALI_IGNORE( units );

	rsw->polygon_offset_factor = factor;
	rsw->polygon_offset_units  = units;
}

MALI_STATIC_INLINE void _gles_fb_polygon_offset( gles_context *ctx, mali_float factor, mali_float units )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

#if HARDWARE_ISSUE_3713
	const u8 mali_factor = _gles_fp32_to_fixed_6_2( -factor );
#else
	const u8 mali_factor = _gles_fp32_to_fixed_6_2( factor );
#endif

	const u8 mali_units = 0;

	rsw->opengl_polygon_offset_factor = factor;
	rsw->opengl_polygon_offset_units  = units;

	_gles_fb_polygon_offset_mali( ctx, mali_factor, mali_units );
}

MALI_STATIC_INLINE void _gles_fb_polygon_offset_deferred( gles_context *ctx, mali_bool is_triangle_type )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	u8 factor = rsw->polygon_offset_factor;
	u8 units  = rsw->polygon_offset_units;

	if( MALI_FALSE == _gles_fb_enables_get( rsw, M200_POLYGON_OFFSET_FILL ) ||
	    MALI_FALSE == is_triangle_type )
	{
		factor = 0;
		units  = 0;
	}

	__m200_rsw_encode( &rsw->mirror, M200_RSW_POLYGON_Z_OFFSET_FACTOR, factor );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_POLYGON_Z_OFFSET_OFFSET, units );
}

MALI_STATIC_INLINE void _gles_fb_set_polygon_offset_fill( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_POLYGON_OFFSET_FILL, flag );

	_gles_fb_polygon_offset( ctx, rsw->opengl_polygon_offset_factor, rsw->opengl_polygon_offset_units );
}

MALI_STATIC_INLINE void _gles_fb_depth_range( gles_context *ctx, mali_float mali_near, mali_float mali_far )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	MALI_DEBUG_ASSERT( _mali_sys_isnan(mali_near) || _mali_sys_isnan(mali_far) || mali_near <= mali_far, ("Near/far plane ordering incorrect"));

	/* both mali_near and mali_far bound should be inclusive, so we round the mali_near bound down and the mali_far bound up */
	__m200_rsw_encode( &rsw->mirror, M200_RSW_Z_NEAR_BOUND, (u32)_mali_sys_floor( M200_RSW_Z_BOUND_MAX * mali_near ) );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_Z_FAR_BOUND,  (u32)_mali_sys_ceil( M200_RSW_Z_BOUND_MAX * mali_far ) );
}

/**
 * @return MALI_TRUE if depth test is enabled and there is a depth buffer present.
 */
MALI_STATIC_FORCE_INLINE mali_bool _gles_fb_depth_test_allowed( gles_context *ctx )
{
	/* While multisampling may be set to one it can be overridden by the
	 * buffer samples or this being a FBO which does not support them.
	 */
	mali_bool has_bits = _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_DEPTH_BITS ) > 0;
	return _gles_fb_enables_get( ctx->rsw_raster, M200_DEPTH_TEST ) && has_bits;
}


MALI_STATIC_INLINE void _gles_fb_init_depth_mask( gles_context *ctx, mali_bool mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	MALI_DEBUG_ASSERT( (0 == mask) || (1 == mask), ("Non-boolean value given for mask") );

	_gles_fb_enables_set( rsw, M200_DEPTH_MASK, mask );

	__m200_rsw_encode( &rsw->mirror, M200_RSW_Z_WRITE_MASK, 0 );
}

MALI_STATIC_INLINE void _gles_fb_depth_mask( gles_context *ctx, mali_bool mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	MALI_DEBUG_ASSERT( (0 == mask) || (1 == mask), ("Non-boolean value given for mask") );

	_gles_fb_enables_set( rsw, M200_DEPTH_MASK, mask );

	/* both glDisable( GL_DEPTH_TEST ) and glDepthMask impact depth writing
	 */
	__m200_rsw_encode( &rsw->mirror, M200_RSW_Z_WRITE_MASK, mask & _gles_fb_depth_test_allowed( ctx ) );
}

MALI_STATIC_INLINE void _gles_fb_init_depth_func( gles_context *ctx, u8 func )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->depth_func = func;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_Z_COMPARE_FUNC, M200_TEST_ALWAYS_SUCCEED );
}

MALI_STATIC_INLINE void _gles_fb_depth_func( gles_context *ctx, u8 func )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->depth_func = func;

	/* glDisable( GL_DEPTH_TEST ) is encoded as the test always passing */
	if( MALI_FALSE == _gles_fb_depth_test_allowed( ctx ) )
	{
		func = M200_TEST_ALWAYS_SUCCEED;
	}

	__m200_rsw_encode( &rsw->mirror, M200_RSW_Z_COMPARE_FUNC, func );
}

MALI_STATIC_INLINE void _gles_fb_set_depth_test( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_DEPTH_TEST, flag );

	_gles_fb_depth_func( ctx, rsw->depth_func );
	_gles_fb_depth_mask( ctx, _gles_fb_enables_get( rsw, M200_DEPTH_MASK ) );
}

/**
 * @return MALI_TRUE if stencil test is enabled and there is a stencil buffer present.
 */
MALI_STATIC_FORCE_INLINE mali_bool _gles_fb_stencil_test_allowed( gles_context *ctx )
{
	/* While multisampling may be set to one it can be overridden by the
	 * buffer samples or this being a FBO which does not support them.
	 */
	mali_bool has_bits = _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_STENCIL_BITS ) > 0;
	return _gles_fb_enables_get( ctx->rsw_raster, M200_STENCIL_TEST ) && has_bits;
}


#if HARDWARE_ISSUE_7585
MALI_STATIC_INLINE void _gles_fb_stencil_fixup_deferred( gles_context *ctx )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	if( _gles_fb_stencil_test_allowed( ctx ) )
	{
		u32 stencil_bits = _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_STENCIL_BITS );
		stencil_bits = ( 1 << stencil_bits ) - 1;

		if( ( M200_TEST_NOT_EQUAL == rsw->front_stencil_func ) &&
		    ( M200_STENCIL_OP_KEEP_CURRENT == rsw->front_stencil_sfail ) &&
		    ( M200_STENCIL_OP_SET_TO_REFERENCE == rsw->front_stencil_zfail ) &&
		    ( M200_STENCIL_OP_SET_TO_REFERENCE == rsw->front_stencil_zpass ) &&
		    ( stencil_bits == (rsw->front_stencil_mask & GLES_MAX_STENCIL_MASK)) )
		{
			if( 0 == rsw->front_stencil_ref )
			{
				__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_IF_ZFAIL, M200_STENCIL_OP_SET_TO_ZERO );
				__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_IF_ZPASS, M200_STENCIL_OP_SET_TO_ZERO );
			}
			else
			{
				__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_IF_SFAIL, M200_STENCIL_OP_SET_TO_REFERENCE );
			}
		}

		if( ( M200_TEST_NOT_EQUAL == rsw->back_stencil_func ) &&
		    ( M200_STENCIL_OP_KEEP_CURRENT == rsw->back_stencil_sfail ) &&
		    ( M200_STENCIL_OP_SET_TO_REFERENCE == rsw->back_stencil_zfail ) &&
		    ( M200_STENCIL_OP_SET_TO_REFERENCE == rsw->back_stencil_zpass ) &&
		    ( stencil_bits == (rsw->back_stencil_mask & GLES_MAX_STENCIL_MASK) ) )
		{
			if( 0 == rsw->back_stencil_ref )
			{
				__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_IF_ZFAIL, M200_STENCIL_OP_SET_TO_ZERO );
				__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_IF_ZPASS, M200_STENCIL_OP_SET_TO_ZERO );
			}
			else
			{
				__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_IF_SFAIL, M200_STENCIL_OP_SET_TO_REFERENCE );
			}
		}
	}
}
#endif

MALI_STATIC_INLINE void _gles_fb_init_front_stencil_func( gles_context *ctx, u8 func, u32 ref, GLuint mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->opengl_front_stencil_mask = mask;

	rsw->front_stencil_func = func;
	rsw->front_stencil_ref	= ref;
	rsw->front_stencil_mask = mask;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_COMPARE_FUNC, M200_TEST_ALWAYS_SUCCEED );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_REF_VALUE, ref & GLES_MAX_STENCIL_MASK );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_MASK, rsw->front_stencil_mask & GLES_MAX_STENCIL_MASK);
}

MALI_STATIC_INLINE void _gles_fb_front_stencil_func( gles_context *ctx, u8 func, u32 ref, GLuint mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->opengl_front_stencil_mask = mask;

	rsw->front_stencil_func = func;
	rsw->front_stencil_ref	= ref;
	rsw->front_stencil_mask = mask;

	if( MALI_FALSE == _gles_fb_stencil_test_allowed( ctx ) )
	{
		func = M200_TEST_ALWAYS_SUCCEED;
	}
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_COMPARE_FUNC, func );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_REF_VALUE, ref & GLES_MAX_STENCIL_MASK );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_MASK, rsw->front_stencil_mask & GLES_MAX_STENCIL_MASK );
}

MALI_STATIC_INLINE void _gles_fb_init_back_stencil_func( gles_context *ctx, u8 func, u32 ref, GLuint mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->opengl_back_stencil_mask = mask;

	rsw->back_stencil_func = func;
	rsw->back_stencil_ref  = ref;
	rsw->back_stencil_mask = mask;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_COMPARE_FUNC, M200_TEST_ALWAYS_SUCCEED );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_REF_VALUE, ref & GLES_MAX_STENCIL_MASK );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_MASK, rsw->back_stencil_mask & GLES_MAX_STENCIL_MASK);
}

MALI_STATIC_INLINE void _gles_fb_back_stencil_func( gles_context *ctx, u8 func, u32 ref, GLuint mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->opengl_back_stencil_mask = mask;

	rsw->back_stencil_func = func;
	rsw->back_stencil_ref  = ref;
	rsw->back_stencil_mask = mask;

	if( MALI_FALSE == _gles_fb_stencil_test_allowed( ctx ) )
	{
		func = M200_TEST_ALWAYS_SUCCEED;
	}
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_COMPARE_FUNC, func );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_REF_VALUE, ref & GLES_MAX_STENCIL_MASK );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_MASK, rsw->back_stencil_mask & GLES_MAX_STENCIL_MASK);
}

MALI_STATIC_INLINE void _gles_fb_init_front_stencil_op( gles_context *ctx, u8 sfail, u8 zfail, u8 zpass )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->front_stencil_sfail = sfail;
	rsw->front_stencil_zfail = zfail;
	rsw->front_stencil_zpass = zpass;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_IF_SFAIL, sfail );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_IF_ZFAIL, M200_STENCIL_OP_KEEP_CURRENT );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_IF_ZPASS, M200_STENCIL_OP_KEEP_CURRENT );
}
MALI_STATIC_INLINE void _gles_fb_front_stencil_op( gles_context *ctx, u8 sfail, u8 zfail, u8 zpass )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->front_stencil_sfail = sfail;
	rsw->front_stencil_zfail = zfail;
	rsw->front_stencil_zpass = zpass;

	/* Z testing may be enabled with stencil being disabled so we must deal with
	 * such a possibility, sfail does not matter as the stencil test will always
	 * pass */
	if( MALI_FALSE == _gles_fb_stencil_test_allowed( ctx ) )
	{
		zfail = M200_STENCIL_OP_KEEP_CURRENT;
		zpass = M200_STENCIL_OP_KEEP_CURRENT;
	}
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_IF_SFAIL, sfail );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_IF_ZFAIL, zfail );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_OP_IF_ZPASS, zpass );
}

MALI_STATIC_INLINE void _gles_fb_init_back_stencil_op( gles_context *ctx, u8 sfail, u8 zfail, u8 zpass )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->back_stencil_sfail = sfail;
	rsw->back_stencil_zfail = zfail;
	rsw->back_stencil_zpass = zpass;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_IF_SFAIL, sfail );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_IF_ZFAIL, M200_STENCIL_OP_KEEP_CURRENT );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_IF_ZPASS, M200_STENCIL_OP_KEEP_CURRENT );
}

MALI_STATIC_INLINE void _gles_fb_back_stencil_op( gles_context *ctx, u8 sfail, u8 zfail, u8 zpass )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->back_stencil_sfail = sfail;
	rsw->back_stencil_zfail = zfail;
	rsw->back_stencil_zpass = zpass;

	/* Z testing may be enabled with stencil being disabled so we must deal with
	 * such a possibility, sfail does not matter as the stencil test will always
	 * pass */
	if( MALI_FALSE == _gles_fb_stencil_test_allowed( ctx ) )
	{
		zfail = M200_STENCIL_OP_KEEP_CURRENT;
		zpass = M200_STENCIL_OP_KEEP_CURRENT;
	}
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_IF_SFAIL, sfail );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_IF_ZFAIL, zfail );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_OP_IF_ZPASS, zpass );
}

MALI_STATIC_INLINE void _gles_fb_init_front_stencil_mask( gles_context *ctx, u32 mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->front_stencil_write_mask = mask;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_WRITE_MASK, 0 );
}

MALI_STATIC_INLINE void _gles_fb_front_stencil_mask( gles_context *ctx, u32 mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->front_stencil_write_mask = mask;

	if( MALI_FALSE == _gles_fb_stencil_test_allowed( ctx ) )
	{
		mask = 0x0;
	}
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_FRONT_WRITE_MASK, mask & GLES_MAX_STENCIL_MASK );
}

MALI_STATIC_INLINE void _gles_fb_init_back_stencil_mask( gles_context *ctx, u32 mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->back_stencil_write_mask = mask;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_WRITE_MASK, 0 );
}

MALI_STATIC_INLINE void _gles_fb_back_stencil_mask( gles_context *ctx, u32 mask )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->back_stencil_write_mask = mask;

	if( MALI_FALSE == _gles_fb_stencil_test_allowed( ctx ) )
	{
		mask = 0x0;
	}
	__m200_rsw_encode( &rsw->mirror, M200_RSW_STENCIL_BACK_WRITE_MASK, mask & GLES_MAX_STENCIL_MASK);
}

MALI_STATIC_INLINE void _gles_fb_set_stencil_test( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_STENCIL_TEST, flag );

	/* overloading of RSW means a stencil test toggle impacts all stencil related bits
	 */
	_gles_fb_front_stencil_func( ctx, rsw->front_stencil_func, rsw->front_stencil_ref, rsw->front_stencil_mask );
	_gles_fb_front_stencil_mask( ctx, rsw->front_stencil_write_mask );
	_gles_fb_front_stencil_op( ctx, rsw->front_stencil_sfail, rsw->front_stencil_zfail, rsw->front_stencil_zpass );
	_gles_fb_back_stencil_func( ctx, rsw->back_stencil_func, rsw->back_stencil_ref, rsw->back_stencil_mask );
	_gles_fb_back_stencil_mask( ctx, rsw->back_stencil_write_mask );
	_gles_fb_back_stencil_op( ctx, rsw->back_stencil_sfail, rsw->back_stencil_zfail, rsw->back_stencil_zpass );
}

MALI_STATIC_INLINE void _gles_fb_alpha_func_mali( gles_context *ctx, u8 func, u8 ref )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->alpha_func = func;
	rsw->alpha_ref  = ref;

	if( MALI_FALSE == _gles_fb_enables_get( rsw, M200_ALPHA_TEST ) )
	{
		func = M200_TEST_ALWAYS_SUCCEED;
	}

	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_TEST_FUNC, func );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_TEST_REF_VALUE, ref );
}

MALI_STATIC_INLINE void _gles_fb_alpha_func( gles_context *ctx, u8 mali_func, GLclampf ref )
{
	const u8 mali_ref = _gles_fp16_to_fix8( _gles_fp32_to_fp16_predefined( ref ) );

	ctx->rsw_raster->opengl_alpha_ref = ref;

	_gles_fb_alpha_func_mali( ctx, mali_func, mali_ref );
}

MALI_STATIC_INLINE void _gles_fb_init_alpha_func( gles_context *ctx, u8 mali_func, GLclampf ref )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;
	const u8 mali_ref = _gles_fp16_to_fix8( _gles_fp32_to_fp16_predefined( ref ) );

	rsw->opengl_alpha_ref = ref;

	rsw->alpha_func = mali_func;
	rsw->alpha_ref  = mali_ref;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_TEST_FUNC, M200_TEST_ALWAYS_SUCCEED );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_TEST_REF_VALUE, mali_ref );
}

MALI_STATIC_INLINE void _gles_fb_set_alpha_test( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_ALPHA_TEST, flag );

	_gles_fb_alpha_func_mali( ctx, rsw->alpha_func, rsw->alpha_ref );
}

/**
 * @return MALI_TRUE if multisampling is enabled and the current buffer supports multisampling.
 */
MALI_STATIC_FORCE_INLINE mali_bool _gles_fb_multisampling_allowed( gles_context *ctx )
{
	/* While multisampling may be set to one it can be overridden by the
	 * buffer samples or this being a FBO which does not support them.
	 */
	mali_bool has_bits = _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_SAMPLE_BUFFERS ) > 0;
	return _gles_fb_enables_get( ctx->rsw_raster, M200_MULTISAMPLE ) && has_bits;
}

MALI_STATIC_INLINE void _gles_fb_init_sample_coverage( gles_context *ctx, mali_float coverage, mali_bool invert )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;
	u8 writemask = 0;

	MALI_DEBUG_ASSERT( (0 == invert) || (1 == invert), ("Non-boolean value given for invert") );

	rsw->multisample_coverage = coverage;
	_gles_fb_enables_set( rsw, M200_RSW_MULTISAMPLE_INVERT, invert );

	if( coverage > 0.875f ) writemask |= 0x08;
	if( coverage > 0.625f ) writemask |= 0x04;
	if( coverage > 0.375f ) writemask |= 0x02;
	if( coverage > 0.125f ) writemask |= 0x01;

	/* Invert is a bitwise not operation, use a xor to ensure bits above the
	 * maximum mask size are not set.
	 */
	if( invert ) writemask ^= M200_RSW_MULTISAMPLE_WRITE_MASK_ALL;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_MULTISAMPLE_WRITE_MASK, writemask );
}
MALI_STATIC_INLINE void _gles_fb_sample_coverage( gles_context *ctx, mali_float coverage, mali_bool invert )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;
	u8 writemask = 0;

	MALI_DEBUG_ASSERT( (0 == invert) || (1 == invert), ("Non-boolean value given for invert") );

	rsw->multisample_coverage = coverage;
	_gles_fb_enables_set( rsw, M200_RSW_MULTISAMPLE_INVERT, invert );

	if( MALI_FALSE == _gles_fb_multisampling_allowed( ctx ) ||
	    MALI_FALSE == _gles_fb_enables_get( rsw, M200_SAMPLE_COVERAGE ) )
	{
		coverage = 1.0f;
		invert = MALI_FALSE;
	}

	if( coverage > 0.875f ) writemask |= 0x08;
	if( coverage > 0.625f ) writemask |= 0x04;
	if( coverage > 0.375f ) writemask |= 0x02;
	if( coverage > 0.125f ) writemask |= 0x01;

	/* Invert is a bitwise not operation, use a xor to ensure bits above the
	 * maximum mask size are not set.
	 */
	if( invert ) writemask ^= M200_RSW_MULTISAMPLE_WRITE_MASK_ALL;

	__m200_rsw_encode( &rsw->mirror, M200_RSW_MULTISAMPLE_WRITE_MASK, writemask );
}

MALI_STATIC_INLINE void _gles_fb_multisampling( gles_context *ctx )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	const mali_bool enabled = _gles_fb_multisampling_allowed( ctx );

	__m200_rsw_encode( &rsw->mirror, M200_RSW_MULTISAMPLE_GRID_ENABLE,        enabled );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_MULTISAMPLED_ALPHA_TEST_ENABLE, enabled );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_MULTISAMPLED_Z_TEST_ENABLE,     enabled );
}

MALI_STATIC_INLINE void _gles_fb_set_sample_alpha_to_coverage( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_SAMPLE_ALPHA_TO_COVERAGE, flag );

	__m200_rsw_encode( &rsw->mirror, M200_RSW_SAMPLE_ALPHA_TO_COVERAGE, flag && _gles_fb_multisampling_allowed( ctx ) );
}

MALI_STATIC_INLINE void _gles_fb_set_sample_alpha_to_one( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_SAMPLE_ALPHA_TO_ONE, flag );

	__m200_rsw_encode( &rsw->mirror, M200_RSW_SAMPLE_ALPHA_TO_ONE, flag && _gles_fb_multisampling_allowed( ctx ) );
}

/* Multisample override per drawcall for GLES 1.1 point/line smooth.
 * Point/line smooth is enabled even if there are no additional sample buffers.
 */
MALI_STATIC_INLINE void _gles_fb_multisampling_deferred( gles_context *ctx, mali_bool is_point_type, mali_bool is_line_type )
{
#if MALI_USE_GLES_1
	IF_API_IS_GLES1(ctx->api_version)
	{
		mali_rsw_raster *rsw = ctx->rsw_raster;

		mali_bool multisample_mode = _gles_fb_multisampling_allowed( ctx );
		mali_bool smooth_mode =
			( is_point_type && _gles_fb_enables_get( rsw, M200_POINT_SMOOTH ) ) ||
			( is_line_type && _gles_fb_enables_get( rsw, M200_LINE_SMOOTH ) );

		__m200_rsw_encode( &rsw->mirror, M200_RSW_MULTISAMPLE_GRID_ENABLE, multisample_mode || smooth_mode );
		__m200_rsw_encode( &rsw->mirror, M200_RSW_SAMPLE_ALPHA_TO_COVERAGE, multisample_mode && _gles_fb_enables_get( rsw, M200_SAMPLE_ALPHA_TO_COVERAGE ) );
		__m200_rsw_encode( &rsw->mirror, M200_RSW_SAMPLE_ALPHA_TO_ONE, multisample_mode && _gles_fb_enables_get( rsw, M200_SAMPLE_ALPHA_TO_ONE ) );
	}
#endif
}

MALI_STATIC_INLINE void _gles_fb_set_sample_coverage( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_SAMPLE_COVERAGE, flag );

	_gles_fb_sample_coverage( ctx, rsw->multisample_coverage, _gles_fb_enables_get( rsw, M200_RSW_MULTISAMPLE_INVERT ) );
}

MALI_STATIC_INLINE void _gles_fb_set_multisample( gles_context *ctx, mali_bool flag )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_gles_fb_enables_set( rsw, M200_MULTISAMPLE, flag );

	_gles_fb_multisampling( ctx );
	_gles_fb_sample_coverage( ctx, rsw->multisample_coverage, _gles_fb_enables_get( rsw, M200_RSW_MULTISAMPLE_INVERT ) );
	_gles_fb_set_sample_alpha_to_one( ctx, _gles_fb_enables_get( rsw, M200_SAMPLE_ALPHA_TO_ONE ) );
	_gles_fb_set_sample_alpha_to_coverage( ctx, _gles_fb_enables_get( rsw, M200_SAMPLE_ALPHA_TO_COVERAGE ) );
}

MALI_STATIC_INLINE void _gles_fb_set_point_smooth( gles_context *ctx, mali_bool flag )
{
	_gles_fb_enables_set( ctx->rsw_raster, M200_POINT_SMOOTH, flag );
}

MALI_STATIC_INLINE void _gles_fb_set_line_smooth( gles_context *ctx, mali_bool flag )
{
	_gles_fb_enables_set( ctx->rsw_raster, M200_LINE_SMOOTH, flag );
}

MALI_STATIC_INLINE void _gles_fb_init_set_dither( gles_context *ctx, mali_bool flag )
{
	_gles_fb_enables_set( ctx->rsw_raster, M200_RSW_DITHER, flag );
	
	_gles_fb_init_apply_dither( ctx, flag );
}


MALI_STATIC_INLINE void _gles_fb_set_dither( gles_context *ctx, mali_bool flag )
{
	_gles_fb_enables_set( ctx->rsw_raster, M200_RSW_DITHER, flag );
	
	_gles_fb_apply_dither( ctx, flag );
}

/* Called on context create, after which time fb state should remain valid with
 * only closed changes applied.
 */
MALI_STATIC_INLINE void _gles_fb_init( gles_context *ctx )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	_mali_sys_memset( rsw, 0, sizeof( mali_rsw_raster ) );

	_gles_fb_init_set_dither( ctx, MALI_TRUE );

	_gles_fb_enables_set( rsw, M200_MULTISAMPLE, 1 );

	_gles_fb_init_sample_coverage( ctx, 1.0f, 0 );

	_gles_fb_init_depth_func( ctx, M200_TEST_LESS_THAN );
	_gles_fb_init_depth_mask( ctx, 1 );
	_gles_fb_depth_range( ctx, 0.0f, 1.0f );

	_gles_fb_color_mask( ctx, 1, 1, 1, 1 );

	_gles_fb_blend_equation( ctx, M200_BLEND_CsS_pCdD, M200_BLEND_CsS_pCdD );
	_gles_fb_init_blend_func( ctx, M200_RSW_BLEND_ONE, M200_RSW_BLEND_ZERO, M200_RSW_BLEND_ONE, M200_RSW_BLEND_ZERO );
	_gles_fb_blend_color( ctx, 0, 0, 0, 0, MALI_FALSE );

	_gles_fb_init_front_stencil_func( ctx, M200_TEST_ALWAYS_SUCCEED, 0, 0xFF );
	_gles_fb_init_front_stencil_op( ctx, M200_STENCIL_OP_KEEP_CURRENT, M200_STENCIL_OP_KEEP_CURRENT, M200_STENCIL_OP_KEEP_CURRENT );
	_gles_fb_init_front_stencil_mask( ctx, GLES_MAX_STENCIL_MASK );
	_gles_fb_init_back_stencil_func( ctx, M200_TEST_ALWAYS_SUCCEED, 0, 0xFF );
	_gles_fb_init_back_stencil_op( ctx, M200_STENCIL_OP_KEEP_CURRENT, M200_STENCIL_OP_KEEP_CURRENT, M200_STENCIL_OP_KEEP_CURRENT );
	_gles_fb_init_back_stencil_mask( ctx, GLES_MAX_STENCIL_MASK );

	_gles_fb_init_alpha_func( ctx, M200_TEST_ALWAYS_SUCCEED, 0.0f );

	/* Constant non-zero RSW fields
	 */
	__m200_rsw_encode( &rsw->mirror, M200_RSW_RGB_RESULT_COLOR_CLAMP_0_1, 1 );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_RESULT_COLOR_CLAMP_0_1, 1 );

	__m200_rsw_encode( &rsw->mirror, M200_RSW_Z_FAR_DEPTH_BOUND_OP, M200_Z_BOUND_DISCARD );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_Z_NEAR_DEPTH_BOUND_OP, M200_Z_BOUND_DISCARD );
}

/* This function changes the rasterisation behavior for internal mali buffers when
 * there is a change to the api level buffer configuration.
 */
MALI_STATIC_INLINE void _gles_fb_api_buffer_change( gles_context *ctx )
{
	/* The surface can modify the number of depth, stencil or alpha bits. Any of
	 * these changing to 0 disables pixel test operations related to these bits.
	 */
	_gles_fb_set_depth_test( ctx, _gles_fb_enables_get( ctx->rsw_raster, M200_DEPTH_TEST ) );
	_gles_fb_set_stencil_test( ctx, _gles_fb_enables_get( ctx->rsw_raster, M200_STENCIL_TEST ) );
	_gles_fb_set_blend( ctx, _gles_fb_enables_get( ctx->rsw_raster, M200_BLEND ) );

	/* As the surface type can also impact the multisampling settings, the rsw multisample
	 * settings are forced to update. This allows 0 sample surfaces to disable the setting
	 * if this context was used with a multisampled surface previously.
	 */
	_gles_fb_set_multisample( ctx, _gles_fb_enables_get( ctx->rsw_raster, M200_MULTISAMPLE ) );
}


/*
 * GL Gets additional functionality
 */

MALI_STATIC_INLINE mali_float _gles_fb_get_sample_coverage_value( gles_context *ctx )
{
	return ctx->rsw_raster->multisample_coverage;
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_sample_coverage_invert( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_RSW_MULTISAMPLE_INVERT );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_sample_alpha_to_coverage( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_SAMPLE_ALPHA_TO_COVERAGE );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_sample_alpha_to_one( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_SAMPLE_ALPHA_TO_ONE );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_sample_coverage( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_SAMPLE_COVERAGE );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_multisample( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_MULTISAMPLE );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_depth_test( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_DEPTH_TEST );
}

MALI_STATIC_INLINE u8 _gles_fb_get_depth_func( gles_context *ctx )
{
	return ctx->rsw_raster->depth_func;
}

MALI_STATIC_INLINE u8 _gles_fb_get_logic_op_mode( gles_context *ctx )
{
	return ctx->rsw_raster->color_logic_op;
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_color_logic_op( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_COLOR_LOGIC_OP );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_alpha_test( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_ALPHA_TEST );
}

MALI_STATIC_INLINE u8 _gles_fb_get_alpha_test_func( gles_context *ctx )
{
	return ctx->rsw_raster->alpha_func;
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_dither( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_RSW_DITHER );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_blend( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_BLEND );
}

MALI_STATIC_INLINE u8 _gles_fb_get_blend_blend_equation_rgb( gles_context *ctx )
{
	return ctx->rsw_raster->blend_mode_rgb;
}

MALI_STATIC_INLINE u8 _gles_fb_get_blend_equation_alpha( gles_context *ctx )
{
	return ctx->rsw_raster->blend_mode_alpha;
}

MALI_STATIC_INLINE u8 _gles_fb_get_stencil_func( gles_context *ctx )
{
	return ctx->rsw_raster->front_stencil_func;
}

MALI_STATIC_INLINE u8 _gles_fb_get_stencil_back_func( gles_context *ctx )
{
	return ctx->rsw_raster->back_stencil_func;
}

MALI_STATIC_INLINE u8 _gles_fb_get_stencil_fail( gles_context *ctx )
{
	return ctx->rsw_raster->front_stencil_sfail;
}

MALI_STATIC_INLINE u8 _gles_fb_get_stencil_back_fail( gles_context *ctx )
{
	return ctx->rsw_raster->back_stencil_sfail;
}

MALI_STATIC_INLINE u8 _gles_fb_get_stencil_depth_pass( gles_context *ctx )
{
	return ctx->rsw_raster->front_stencil_zpass;
}

MALI_STATIC_INLINE u8 _gles_fb_get_stencil_back_depth_pass( gles_context *ctx )
{
	return ctx->rsw_raster->back_stencil_zpass;
}

MALI_STATIC_INLINE u8 _gles_fb_get_stencil_depth_fail( gles_context *ctx )
{
	return ctx->rsw_raster->front_stencil_zfail;
}

MALI_STATIC_INLINE u8 _gles_fb_get_stencil_back_depth_fail( gles_context *ctx )
{
	return ctx->rsw_raster->back_stencil_zfail;
}

MALI_STATIC_INLINE u32 _gles_fb_get_stencil_ref( gles_context *ctx )
{
	return ctx->rsw_raster->front_stencil_ref;
}
MALI_STATIC_INLINE u32 _gles_fb_get_stencil_back_ref( gles_context *ctx )
{
	return ctx->rsw_raster->back_stencil_ref;
}

MALI_STATIC_INLINE GLuint _gles_fb_get_stencil_value_mask( gles_context *ctx )
{
	return ctx->rsw_raster->opengl_front_stencil_mask;
}

MALI_STATIC_INLINE GLuint _gles_fb_get_stencil_back_value_mask( gles_context *ctx )
{
	return ctx->rsw_raster->opengl_back_stencil_mask;
}

MALI_STATIC_INLINE u32 _gles_fb_get_stencil_write_mask( gles_context *ctx )
{
	return ctx->rsw_raster->front_stencil_write_mask;
}

MALI_STATIC_INLINE u32 _gles_fb_get_stencil_back_write_mask( gles_context *ctx )
{
	return ctx->rsw_raster->back_stencil_write_mask;
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_stencil_test( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_STENCIL_TEST );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_polygon_offset_fill( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_POLYGON_OFFSET_FILL );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_point_smooth( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_POINT_SMOOTH );
}

MALI_STATIC_INLINE mali_bool _gles_fb_get_line_smooth( gles_context *ctx )
{
	return _gles_fb_enables_get( ctx->rsw_raster, M200_LINE_SMOOTH );
}
/*
 * GL type gets for redundant storage from lossy rsw packing
 */

MALI_STATIC_INLINE GLenum _gles_fb_get_blend_src_rgb( gles_context *ctx )
{
	return ctx->rsw_raster->blend_src;
}

MALI_STATIC_INLINE GLenum _gles_fb_get_blend_dst_rgb( gles_context *ctx )
{
	return ctx->rsw_raster->blend_dst;
}

MALI_STATIC_INLINE GLenum _gles_fb_get_blend_src_alpha( gles_context *ctx )
{
	return ctx->rsw_raster->blend_alpha_src;
}

MALI_STATIC_INLINE GLenum _gles_fb_get_blend_dst_alpha( gles_context *ctx )
{
	return ctx->rsw_raster->blend_alpha_dst;
}

MALI_STATIC_INLINE GLclampf _gles_fb_get_blend_color_r( gles_context *ctx )
{
	return ctx->rsw_raster->opengl_blend_r;
}

MALI_STATIC_INLINE GLclampf _gles_fb_get_blend_color_g( gles_context *ctx )
{
	return ctx->rsw_raster->opengl_blend_g;
}

MALI_STATIC_INLINE GLclampf _gles_fb_get_blend_color_b( gles_context *ctx )
{
	return ctx->rsw_raster->opengl_blend_b;
}

MALI_STATIC_INLINE GLclampf _gles_fb_get_blend_color_a( gles_context *ctx )
{
	return ctx->rsw_raster->opengl_blend_a;
}

MALI_STATIC_INLINE GLclampf _gles_fb_get_alpha_test_ref( gles_context *ctx )
{
	return ctx->rsw_raster->opengl_alpha_ref;
}

MALI_STATIC_INLINE GLclampf _gles_fb_get_polygon_offset_factor( gles_context *ctx )
{
	return ctx->rsw_raster->opengl_polygon_offset_factor;
}

MALI_STATIC_INLINE GLclampf _gles_fb_get_polygon_offset_units( gles_context *ctx )
{
	return ctx->rsw_raster->opengl_polygon_offset_units;
}

#endif /* GLES_M200_RSW_H */
