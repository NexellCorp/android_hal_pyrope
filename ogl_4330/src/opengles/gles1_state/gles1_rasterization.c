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

#include "gles1_rasterization.h"
#include "gles1_state.h"
#include <gles_common_state/gles_rasterization.h>
#include <gles_convert.h>
#include <gles_config.h>
#include <gles_context.h>
#include <opengles/gles_sg_interface.h>

void _gles1_rasterization_init( struct gles_context *ctx )
{
	struct gles_rasterization *raster;
	struct gles1_rasterization *raster_gles1;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	raster = &ctx->state.common.rasterization;
	raster_gles1 = &ctx->state.api.gles1->rasterization;

	raster->cull_face      = GL_FALSE;
	raster->cull_face_mode = GL_BACK;
	raster->front_face     = GL_CCW;

	raster->point_size     = FLOAT_TO_FTYPE( 1.f );
	raster->point_size_min = FLOAT_TO_FTYPE( (float) GLES_POINT_SIZE_MIN );
	raster->point_size_max = FLOAT_TO_FTYPE( (float) GLES_POINT_SIZE_MAX );

	raster->line_width  = FLOAT_TO_FTYPE( 1.f );

	MALI_DEBUG_ASSERT_POINTER( raster_gles1 );

	raster_gles1->point_fade_threshold_size        = FLOAT_TO_FTYPE( 1.f );
	raster_gles1->point_distance_attenuation[ 0 ]  = FLOAT_TO_FTYPE( 1.f );
	raster_gles1->point_distance_attenuation[ 1 ]  = FLOAT_TO_FTYPE( 0.f );
	raster_gles1->point_distance_attenuation[ 2 ]  = FLOAT_TO_FTYPE( 0.f );

	MALI_GLES_SG_WRITE_VEC3(PointSizeAttenuationCoefs, raster_gles1->point_distance_attenuation);
}

GLenum _gles1_front_face( struct gles_rasterization *raster, GLenum mode )
{
	MALI_DEBUG_ASSERT_POINTER( raster );

	if (mode != GL_CW && mode != GL_CCW) MALI_ERROR( GL_INVALID_ENUM );

	raster->front_face = mode;
	GL_SUCCESS;
}

GLenum _gles1_cull_face( struct gles_rasterization *raster, GLenum mode )
{
	MALI_DEBUG_ASSERT_POINTER( raster );

	if (mode != GL_FRONT && mode != GL_BACK && mode != GL_FRONT_AND_BACK)
	{
		MALI_ERROR( GL_INVALID_ENUM );
	}

	raster->cull_face_mode = mode;
	GL_SUCCESS;
}

GLenum _gles1_point_size( struct gles_rasterization *raster, GLftype size )
{
	MALI_DEBUG_ASSERT_POINTER( raster );

	if (size <= FLOAT_TO_FTYPE( 0.f )) MALI_ERROR( GL_INVALID_VALUE );

	raster->point_size = size;
	GL_SUCCESS;
}

GLenum _gles1_line_width( struct gles_context *ctx, GLftype width )
{
	struct gles_rasterization *raster;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	raster = &ctx->state.common.rasterization;

	if (width <= FLOAT_TO_FTYPE( 0.f )) MALI_ERROR( GL_INVALID_VALUE );

	if ( width != raster->line_width )
	{
		raster->line_width = width;
	}

	GL_SUCCESS;
}

GLenum _gles1_point_parameterv(
	struct gles_context *ctx,
	GLenum pname,
	const GLvoid *params,
	gles_datatype type )
{
	GLftype param;
	struct gles_rasterization  *raster_common;
	struct gles1_rasterization *raster_gles1;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	raster_common = &ctx->state.common.rasterization;
	raster_gles1 = &ctx->state.api.gles1->rasterization;

	switch( pname )
	{
		case GL_POINT_SIZE_MIN:
			param = _gles_convert_element_to_ftype( params, 0, type );
			if (param < FLOAT_TO_FTYPE( 0.f )) MALI_ERROR( GL_INVALID_VALUE );
			/* clamping to the implementation defined size.
				This is ok as long as:
					-aliased and smooth sizes are equal
					- we accept that the corresponding glGet returns the clamped size
			*/
			MALI_DEBUG_ASSERT( GLES1_SMOOTH_POINT_SIZE_MIN==GLES_ALIASED_POINT_SIZE_MIN, ("Aliased and Smooth point sizes must match for this to be correct" ));
			param = CLAMP( param, FLOAT_TO_FTYPE( GLES_POINT_SIZE_MIN ), FLOAT_TO_FTYPE( GLES_POINT_SIZE_MAX ) );
			raster_common->point_size_min = param;
		break;

		case GL_POINT_SIZE_MAX:
			param = _gles_convert_element_to_ftype( params, 0, type );
			if (param < FLOAT_TO_FTYPE( 0.f )) MALI_ERROR( GL_INVALID_VALUE );
			/* clamping to the implementation defined size (see comment for GL_POINT_SIZE_MIN)*/
			MALI_DEBUG_ASSERT( GLES1_SMOOTH_POINT_SIZE_MAX==GLES_ALIASED_POINT_SIZE_MAX, ("Aliased and Smooth point sizes must match for this to be correct" ));
			param = CLAMP( param, FLOAT_TO_FTYPE( GLES_POINT_SIZE_MIN ), FLOAT_TO_FTYPE( GLES_POINT_SIZE_MAX ) );
			raster_common->point_size_max = param;
		break;

		case GL_POINT_FADE_THRESHOLD_SIZE:
			param = _gles_convert_element_to_ftype( params, 0, type );
			if (param < FLOAT_TO_FTYPE( 0.f )) MALI_ERROR( GL_INVALID_VALUE );
			raster_gles1->point_fade_threshold_size = param;
		break;

		case GL_POINT_DISTANCE_ATTENUATION:
		{
			GLboolean point_size_attenuation;
			_gles_convert_array_to_ftype( raster_gles1->point_distance_attenuation, params, 3, type );
			point_size_attenuation =
				ctx->state.api.gles1->rasterization.point_distance_attenuation[GL_CONSTANT_ATTENUATION_INDEX] != 1.0f ||
				ctx->state.api.gles1->rasterization.point_distance_attenuation[GL_LINEAR_ATTENUATION_INDEX] != 0.0f ||
				ctx->state.api.gles1->rasterization.point_distance_attenuation[GL_QUADRATIC_ATTENUATION_INDEX] != 0.0f;

			vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_POINTSIZE_ATTENUATION, point_size_attenuation);

			vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_POINTSIZE_COPY, 
				point_size_attenuation ||
				ctx->state.common.vertex_array.attrib_array[GLES_VERTEX_ATTRIB_POINT_SIZE].enabled );

			MALI_GLES_SG_WRITE_VEC3(PointSizeAttenuationCoefs, raster_gles1->point_distance_attenuation);
		}
		break;

		default: MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

GLenum _gles1_point_parameter(
	struct gles_context *ctx,
	GLenum pname,
	const GLvoid *param,
	gles_datatype type )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( param );

	if (GL_POINT_DISTANCE_ATTENUATION == pname) MALI_ERROR( GL_INVALID_ENUM );
	return _gles1_point_parameterv( ctx, pname, param, type );
}
