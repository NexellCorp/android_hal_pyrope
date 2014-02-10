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
#include <gles_context.h>
#include <gles_sg_interface.h>
#include "gles1_state.h"
#include "gles1_coloring.h"

#include <gles_convert.h>

void _gles1_coloring_init(gles1_coloring *coloring)
{
	MALI_DEBUG_ASSERT_POINTER(coloring);

	/* fogging parameters */
	coloring->fog_color[0] = FLOAT_TO_FTYPE( 0.f );
	coloring->fog_color[1] = FLOAT_TO_FTYPE( 0.f );
	coloring->fog_color[2] = FLOAT_TO_FTYPE( 0.f );
	coloring->fog_color[3] = FLOAT_TO_FTYPE( 0.f );
	coloring->fog_density  = FLOAT_TO_FTYPE( 1.f );
	coloring->fog_start    = FLOAT_TO_FTYPE( 0.f );
	coloring->fog_end      = FLOAT_TO_FTYPE( 1.f );
	coloring->fog_mode     = GL_EXP;

}

GLenum _gles1_fogv( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type )
{
	GLftype param_float;
	GLenum  param_enum;
	gles1_coloring* coloring;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	coloring = &ctx->state.api.gles1->coloring;
	MALI_DEBUG_ASSERT_POINTER(coloring);


	switch( pname )
	{
		case GL_FOG_MODE:
			{
				fog_kind fogstate = FOG_NONE;
				param_enum = _gles_convert_to_enum( params, type );
				if( coloring->fog_mode == param_enum) return GL_NO_ERROR; /* early out */
				switch(param_enum)
				{
					case GL_LINEAR:
						{
							float fog_factor = coloring->fog_end - coloring->fog_start != 0.0f
								? -1.0f/(coloring->fog_end - coloring->fog_start)
								: -1.0f;
							float fog_add = coloring->fog_end - coloring->fog_start != 0.0f
							    ? coloring->fog_end/(coloring->fog_end - coloring->fog_start)
								: 0.0f;

							MALI_GLES_SG_WRITE_FLOAT(FogFactor, fog_factor);
							MALI_GLES_SG_WRITE_FLOAT(FogAdd, fog_add);

							fogstate = FOG_LINEAR;
						}
					break;
					case GL_EXP:
						MALI_GLES_SG_WRITE_FLOAT(FogFactor, log2_e * coloring->fog_density);
						MALI_GLES_SG_WRITE_FLOAT(FogAdd, 0.0f);
						fogstate = FOG_EXP;
					break;
					case GL_EXP2:
						MALI_GLES_SG_WRITE_FLOAT(FogFactor, sqrt_log2_e * coloring->fog_density);
						MALI_GLES_SG_WRITE_FLOAT(FogAdd, 0.0f);
						fogstate = FOG_EXP2;
					break;
					default: return GL_INVALID_ENUM;
				}
				coloring->fog_mode = param_enum;
				if( vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST) ) /* any enable flag; ie fog is on */
				{
					/* fog mode has been changed. Update to new piece */
					if(param_enum == GL_LINEAR)
					{
						vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST, DIST_ADD);
					} else {
						vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST, DIST_ON);
					}
					fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_FOG_MODE, fogstate);
				} else {
					fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_FOG_MODE, FOG_NONE);
				}
			}
		break;

		case GL_FOG_DENSITY:
			param_float = _gles_convert_element_to_ftype( params, 0, type );
			if ( param_float < 0 ) MALI_ERROR( GL_INVALID_VALUE );
			coloring->fog_density = param_float;

			if( vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST) ) 
			{
				if ( GL_EXP == coloring->fog_mode )
				{
					/* The HW will calculate exp²(FogFactor). We want exp(FogFactor). 
					 * exp(x) = exp²(x*log2_e), so we have to multiply by log2_e here */
					MALI_GLES_SG_WRITE_FLOAT(FogFactor, log2_e * coloring->fog_density);
					MALI_GLES_SG_WRITE_FLOAT(FogAdd, 0);
				}
				if ( GL_EXP2 == coloring->fog_mode )
				{
					MALI_GLES_SG_WRITE_FLOAT(FogFactor, sqrt_log2_e * coloring->fog_density);
					MALI_GLES_SG_WRITE_FLOAT(FogAdd, 0);
				}
				/* No GL_LINEAR since that mode does not care about density */
			}
		break;

		case GL_FOG_START:
			coloring->fog_start = _gles_convert_element_to_ftype( params, 0 ,type );
			if( vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST)  && GL_LINEAR == coloring->fog_mode )
			{
				float fog_factor = coloring->fog_end - coloring->fog_start != 0.0f
								? -1.0f/(coloring->fog_end - coloring->fog_start)
								: -1.0f;
				float fog_add = coloring->fog_end - coloring->fog_start != 0.0f
							    ? coloring->fog_end/(coloring->fog_end - coloring->fog_start)
								: 0.0f;

				MALI_GLES_SG_WRITE_FLOAT(FogFactor, fog_factor);
				MALI_GLES_SG_WRITE_FLOAT(FogAdd, fog_add);
			}
			/* No GL_EXP or GL_EXP2 since they do not care about start */
		break;

		case GL_FOG_END:
			coloring->fog_end = _gles_convert_element_to_ftype( params, 0 ,type );
			if( vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST)  && GL_LINEAR == coloring->fog_mode )
			{
				float fog_factor = coloring->fog_end - coloring->fog_start != 0.0f
								? -1.0f/(coloring->fog_end - coloring->fog_start)
								: -1.0f;
				float fog_add = coloring->fog_end - coloring->fog_start != 0.0f
							    ? coloring->fog_end/(coloring->fog_end - coloring->fog_start)
								: 0.0f;

				MALI_GLES_SG_WRITE_FLOAT(FogFactor, fog_factor);
				MALI_GLES_SG_WRITE_FLOAT(FogAdd, fog_add);
			}
			/* No GL_EXP or GL_EXP2 since they do not care about end */
		break;

		case GL_FOG_COLOR:
			_gles_convert_array_to_ftype( coloring->fog_color, params, 4, type );
			mali_statebit_set( &ctx->state.common, GLES_STATE_FOG_COLOR_DIRTY);
		break;

		default: MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

GLenum _gles1_fog( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type )
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* only parameter thats illegal for this function and not handled in _gles1_fogv() */
	if (GL_FOG_COLOR == pname) MALI_ERROR( GL_INVALID_ENUM );

	return _gles1_fogv( ctx, pname, param, type );
}

GLenum _gles1_shade_model( struct gles_context *ctx, GLenum mode )
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	if ( ( mode != GL_FLAT ) && ( mode != GL_SMOOTH ) ) MALI_ERROR( GL_INVALID_ENUM );

#ifdef USING_SHADER_GENERATOR
	/* set flatshading to true if mode is flat */
	MALI_DEBUG_ASSERT_POINTER(ctx->sg_ctx);
	fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_FLATSHADED_COLORS, (mode == GL_FLAT) );
#endif

	GL_SUCCESS;
}

