/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_base.h>

#include "../gles_config.h"
#include <gles_common_state/gles_rasterization.h>
#include "gles2_rasterization.h"
#include <gles_context.h>

void _gles2_rasterization_init( struct gles_rasterization *raster )
{
	MALI_DEBUG_ASSERT_POINTER( raster );

	raster->cull_face      = GL_FALSE;
	raster->cull_face_mode = GL_BACK;
	raster->front_face     = GL_CCW;

	/* This state is not used in GLES2; point size is only changed through a shader. 
	 * The state is kept for consistency reasons with GLES1. */
	raster->point_size     = FLOAT_TO_FTYPE( 1.f );
	raster->point_size_min = FLOAT_TO_FTYPE( (float) GLES_POINT_SIZE_MIN );
	raster->point_size_max = FLOAT_TO_FTYPE( (float) GLES_POINT_SIZE_MAX );

	raster->line_width     = FLOAT_TO_FTYPE( 1.f );
}

GLenum _gles2_front_face( struct gles_rasterization *raster, GLenum mode )
{
	MALI_DEBUG_ASSERT_POINTER( raster );

	if (mode != GL_CW && mode != GL_CCW) return GL_INVALID_ENUM;

	raster->front_face = mode;
	return GL_NO_ERROR;
}

GLenum _gles2_cull_face( struct gles_rasterization *raster, GLenum mode )
{
	MALI_DEBUG_ASSERT_POINTER( raster );

	if (mode != GL_FRONT && mode != GL_BACK && mode != GL_FRONT_AND_BACK)
	{
		return GL_INVALID_ENUM;
	}

	raster->cull_face_mode = mode;
	return GL_NO_ERROR;
}

GLenum _gles2_line_width( struct gles_context *ctx, GLftype width )
{
	struct gles_rasterization *raster;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	raster = &ctx->state.common.rasterization;

	if( width <= FLOAT_TO_FTYPE( 0.f ) ) return GL_INVALID_VALUE;

	raster->line_width = width;
	return GL_NO_ERROR;
}
