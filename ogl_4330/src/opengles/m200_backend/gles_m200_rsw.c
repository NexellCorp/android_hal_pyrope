/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_m200_rsw.c
 * @brief Utility functionality to work with RSWs.
 */

/* The gles_m200_rsw header is included in this header */
#include "gles_framebuffer_object.h"

void _gles_fb_blend_equation( gles_context *ctx, u8 mode_rgb, u8 mode_alpha )
{
	mali_rsw_raster *rsw = ctx->rsw_raster;

	rsw->blend_mode_rgb = mode_rgb;
	rsw->blend_mode_alpha = mode_alpha;

	/* If logic op is enabled, we don't change the actual rendering behavior
	 */
	if( MALI_TRUE == _gles_fb_enables_get( rsw, M200_COLOR_LOGIC_OP ) ) return;

	/* Disabled blending is encoded as 1 * Cs + 0 * Cd
	 */
	if( MALI_FALSE == _gles_fb_enables_get( rsw, M200_BLEND ) )
	{
		mode_rgb = mode_alpha = M200_BLEND_CsS_pCdD;
	}

	__m200_rsw_encode( &rsw->mirror, M200_RSW_RGB_BLEND_FUNC, mode_rgb );
	__m200_rsw_encode( &rsw->mirror, M200_RSW_ALPHA_BLEND_FUNC, mode_alpha );
}

void _gles_fb_init_apply_dither( gles_context *ctx, mali_bool flag )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT(_gles_fb_enables_get( ctx->rsw_raster, M200_COLOR_LOGIC_OP ) == MALI_FALSE, ("Logic op must be disabled at the time of init") );

	/* Just a note: When logic op (default disabled) is enabled, dithering is enabled based on color buffer format.
	 * When logic op is disabled, dithering is set to the input variable. Logic op is disabled at the time of init */

	__m200_rsw_encode( &ctx->rsw_raster->mirror, M200_RSW_DITHERING_ENABLE, flag );
}


void _gles_fb_apply_dither( gles_context *ctx, mali_bool flag )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object->draw_frame_builder );

	__m200_rsw_encode( &ctx->rsw_raster->mirror, M200_RSW_DITHERING_ENABLE, flag );
}


