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

#include <gles_context.h>
#include <gles_state.h>

#include <gles_convert.h>
#include <gles_context.h>

#include "gles1_state.h"
#include <gles_sg_interface.h>

#include <opengles/gles_sg_interface.h>

#define MALI_ATTENUATION_PRESENT_IN_SHADER( constant, linear, quadratic ) ( ( 1.0f == constant ) && ( 0.0f == linear ) && ( 0.0f == quadratic ) ? 0x0 : 0x1 )
#define MALI_SPECULAR_PRESENT_IN_SHADER( specular, mat_specular ) ( ( specular[0] * mat_specular[0] == 0.0f ) && ( specular[1] * mat_specular[1] == 0.0f ) && ( specular[2] * mat_specular[2] == 0.0f ) ? 0x0 : 0x1 )

MALI_STATIC_INLINE void _mali_set_attenuation_in_shader( struct gles_context *ctx, gles_light *l, u32 lindex )
{
	struct gles1_lighting *lighting;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( l );
	MALI_DEBUG_ASSERT( lindex < GLES1_MAX_LIGHTS, ("Invalid light-index given\n") );

	lighting = &ctx->state.api.gles1->lighting;

	/* First clear the bit from the bitfield */
	lighting->attenuation_field &= ~( 1 << lindex );

	/* Check if attenuation == [1, 0, 0] or not, shift the result to correct place with lindex */
	lighting->attenuation_field |= MALI_ATTENUATION_PRESENT_IN_SHADER(
	                                   l->attenuation[ GL_CONSTANT_ATTENUATION_INDEX ],
	                                   l->attenuation[ GL_LINEAR_ATTENUATION_INDEX ],
	                                   l->attenuation[ GL_QUADRATIC_ATTENUATION_INDEX ] ) << lindex;

	/* Update the bitfield with the new value, also taking into consideration if the light is enabled */
	vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_ATTENUATION, 
                            ( ( 0x0 != ( lighting->attenuation_field & lighting->enabled_field ) ) ? GL_TRUE : GL_FALSE ) );

	mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + lindex );
}
/* External function definitions */
void _gles1_lighting_init( struct gles_state * const state )
{
	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles1 );

	{
		u32 idx;

		gles_common_state * const state_common 	= & state->common;
		gles1_state       * const state_gles1 	= state->api.gles1;
		gles1_lighting    * const lighting 	= & state_gles1->lighting;

		lighting->enabled        = (GLboolean)GL_FALSE;

		/* OpenGL ES spec specifies initial values (see Table 2.8 in version 1.1.10 of the GLES specification) */
		lighting->ambient[0] = FLOAT_TO_FTYPE( 0.2f );
		lighting->ambient[1] = FLOAT_TO_FTYPE( 0.2f );
		lighting->ambient[2] = FLOAT_TO_FTYPE( 0.2f );
		lighting->ambient[3] = FLOAT_TO_FTYPE( 1.0f );

		lighting->diffuse[0] = FLOAT_TO_FTYPE( 0.8f );
		lighting->diffuse[1] = FLOAT_TO_FTYPE( 0.8f );
		lighting->diffuse[2] = FLOAT_TO_FTYPE( 0.8f );
		lighting->diffuse[3] = FLOAT_TO_FTYPE( 1.0f );

		lighting->specular[0] = FLOAT_TO_FTYPE( 0.0f );
		lighting->specular[1] = FLOAT_TO_FTYPE( 0.0f );
		lighting->specular[2] = FLOAT_TO_FTYPE( 0.0f );
		lighting->specular[3] = FLOAT_TO_FTYPE( 1.0f );

		lighting->emission[0] = FLOAT_TO_FTYPE( 0.0f );
		lighting->emission[1] = FLOAT_TO_FTYPE( 0.0f );
		lighting->emission[2] = FLOAT_TO_FTYPE( 0.0f );
		lighting->emission[3] = FLOAT_TO_FTYPE( 1.0f );

		lighting->shininess = FLOAT_TO_FTYPE( 0.0f );

		lighting->light_model_ambient[0] = FLOAT_TO_FTYPE( 0.2f );
		lighting->light_model_ambient[1] = FLOAT_TO_FTYPE( 0.2f );
		lighting->light_model_ambient[2] = FLOAT_TO_FTYPE( 0.2f );
		lighting->light_model_ambient[3] = FLOAT_TO_FTYPE( 1.0f );

		for( idx = 0; idx < GLES1_MAX_LIGHTS; ++idx )
		{
			mali_statebit_unset( state_common, GLES_STATE_LIGHT0_ENABLED + idx );
			mali_statebit_set( state_common, GLES_STATE_LIGHT0_DIRTY + idx );

			{
				gles_light * const light = &lighting->light[idx];

				light->ambient[0] = FLOAT_TO_FTYPE( 0.0f );
				light->ambient[1] = FLOAT_TO_FTYPE( 0.0f );
				light->ambient[2] = FLOAT_TO_FTYPE( 0.0f );
				light->ambient[3] = FLOAT_TO_FTYPE( 1.0f );

				/* OpenGL ES spec specifies different values for light0
				 */
				if( 0 == idx )
				{
					light->diffuse[0] = FLOAT_TO_FTYPE( 1.0f );
					light->diffuse[1] = FLOAT_TO_FTYPE( 1.0f );
					light->diffuse[2] = FLOAT_TO_FTYPE( 1.0f );
					light->diffuse[3] = FLOAT_TO_FTYPE( 1.0f );

					light->specular[0] = FLOAT_TO_FTYPE( 1.0f );
					light->specular[1] = FLOAT_TO_FTYPE( 1.0f );
					light->specular[2] = FLOAT_TO_FTYPE( 1.0f );
					light->specular[3] = FLOAT_TO_FTYPE( 1.0f );
				}
				else
				{
					light->diffuse[0] = FLOAT_TO_FTYPE( 0.0f );
					light->diffuse[1] = FLOAT_TO_FTYPE( 0.0f );
					light->diffuse[2] = FLOAT_TO_FTYPE( 0.0f );
					light->diffuse[3] = FLOAT_TO_FTYPE( 1.0f );

					light->specular[0] = FLOAT_TO_FTYPE( 0.0f );
					light->specular[1] = FLOAT_TO_FTYPE( 0.0f );
					light->specular[2] = FLOAT_TO_FTYPE( 0.0f );
					light->specular[3] = FLOAT_TO_FTYPE( 1.0f );
				}

				light->position[0] = FLOAT_TO_FTYPE( 0.0f );
				light->position[1] = FLOAT_TO_FTYPE( 0.0f );
				light->position[2] = FLOAT_TO_FTYPE( 1.0f );
				light->position[3] = FLOAT_TO_FTYPE( 0.0f );

				light->attenuation[0] = FLOAT_TO_FTYPE( 1.0f );
				light->attenuation[1] = FLOAT_TO_FTYPE( 0.0f );
				light->attenuation[2] = FLOAT_TO_FTYPE( 0.0f );

				light->spot_direction[0] = FLOAT_TO_FTYPE(  0.0f );
				light->spot_direction[1] = FLOAT_TO_FTYPE(  0.0f );
				light->spot_direction[2] = FLOAT_TO_FTYPE( -1.0f );

				light->spot_exponent = FLOAT_TO_FTYPE( 0.0f );
				light->cos_spot_cutoff = FLOAT_TO_FTYPE( -1.0f );
				lighting->spotlight_cutoff[idx] = FLOAT_TO_FTYPE( 180.0f );
			}
		}
		lighting->spot_enabled_field = 0x0;
		lighting->specular_field = 0x0;
		lighting->attenuation_field = 0x0;
		lighting->n_lights = 0;
		lighting->enabled_field = 0x0;
		mali_statebit_set( state_common, GLES_STATE_LIGHT_ALL_DIRTY );
		mali_statebit_set( state_common, GLES_STATE_LIGHTING_DIRTY );
	}
}

/* material states */

GLenum _gles1_materialv(struct gles_context *ctx, GLenum face, GLenum pname, const GLvoid *params, const gles_datatype type)
{
	gles1_lighting *lighting;
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );
	MALI_DEBUG_ASSERT( (GLES_FIXED == type) || (GLES_FLOAT == type), ("invalid type") );

	lighting = &ctx->state.api.gles1->lighting;

	if ( GL_FRONT_AND_BACK != face ) MALI_ERROR( GL_INVALID_ENUM );

	switch (pname)
	{
		case GL_AMBIENT:
			if ( COLORMATERIAL_OFF == vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL) )
			{
				_gles_convert_array_to_ftype(lighting->ambient, params, 4, type);

				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT_ALL_DIRTY );
				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY );
			}

		break;

		case GL_DIFFUSE:
			if ( COLORMATERIAL_OFF == vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL) )
			{
				_gles_convert_array_to_ftype(lighting->diffuse, params, 4, type);

				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT_ALL_DIRTY );
				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY );
			}
		break;

		case GL_SPECULAR:
		{
			int i;
			_gles_convert_array_to_ftype(lighting->specular, params, 4, type);

			for ( i = 0; i < GLES1_MAX_LIGHTS; i++ )
			{
				lighting->specular_field &= ~( 1 << i );
				lighting->specular_field |= MALI_SPECULAR_PRESENT_IN_SHADER( lighting->light[ i ].specular, lighting->specular ) << i;
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_SPECULAR, 
				                        ( 0x0 != ( lighting->specular_field & lighting->enabled_field ) ? GL_TRUE : GL_FALSE ) );
			}

			mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT_ALL_DIRTY );
		}
		break;

		case GL_EMISSION:
			_gles_convert_array_to_ftype(lighting->emission, params, 4, type);
			mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY );
		break;

		case GL_SHININESS:
		{
			GLftype shininess = _gles_convert_element_to_ftype(params, 0, type);
			if ((shininess < FLOAT_TO_FTYPE(0.0)) || (shininess > FLOAT_TO_FTYPE(128.0))) MALI_ERROR( GL_INVALID_VALUE );
			lighting->shininess = shininess;
			mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY );
		}
		break;

		case GL_AMBIENT_AND_DIFFUSE:
			if ( COLORMATERIAL_OFF == vertex_shadergen_decode( &ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL ) )
			{
				_gles_convert_array_to_ftype(lighting->ambient, params, 4, type);
				_gles_convert_array_to_ftype(lighting->diffuse, params, 4, type);

				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT_ALL_DIRTY );
				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY );
			}
		break;

		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}

GLenum _gles1_light_modelv( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type )
{
	GLftype param;
	gles1_lighting* lighting;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	lighting = &ctx->state.api.gles1->lighting;
	MALI_DEBUG_ASSERT_POINTER( lighting );

	MALI_DEBUG_ASSERT_POINTER( ctx );

	lighting = &ctx->state.api.gles1->lighting;

	switch( pname )
	{
		case GL_LIGHT_MODEL_AMBIENT:
			_gles_convert_array_to_ftype( lighting->light_model_ambient, params, 4, type );
			mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY );
		break;

		case GL_LIGHT_MODEL_TWO_SIDE:
			param = _gles_convert_element_to_ftype( params, 0, type );
			vertex_shadergen_encode( &ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_TWOSIDED, 
			                         ( param != FIXED_TO_FTYPE( 0 ) ) ? (GLboolean) GL_TRUE : (GLboolean) GL_FALSE );
			_gles1_push_twosided_lighting_state(ctx);
		break;

		default: return GL_INVALID_ENUM;
	}

	GL_SUCCESS;
}

GLenum _gles1_light_model( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	switch( pname )
	{
		case GL_LIGHT_MODEL_TWO_SIDE:
			return _gles1_light_modelv( ctx, pname, param, type );

		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}
}

GLenum _gles1_material(struct gles_context *ctx, GLenum face, GLenum pname, const GLvoid *param, const gles_datatype type)
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	/* NOTE: Returns GL_SUCCESS to avoid segmentation fault as spec doesn't specify any error code when param is NULL */
	if (NULL == param) GL_SUCCESS;

	switch (pname)
	{
		case GL_SHININESS:
			return _gles1_materialv(ctx, face, pname, param, type );

		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}
}


/* light states */

GLenum _gles1_lightv(struct gles_context * ctx, GLenum light, const GLenum pname, const GLvoid *params, const gles_datatype type)
{
	GLftype     f;
	const int   lindex = (int) light - (int) GL_LIGHT0;

	if( lindex < 0 )			MALI_ERROR(  GL_INVALID_ENUM );
	if( lindex >= GLES1_MAX_LIGHTS )	MALI_ERROR(  GL_INVALID_ENUM );

	MALI_DEBUG_ASSERT_POINTER( ctx);
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	MALI_DEBUG_ASSERT_RANGE( light, GL_LIGHT0, (GL_LIGHT0 + GLES1_MAX_LIGHTS - 1) );
	MALI_DEBUG_ASSERT( (GLES_FIXED == type) || (GLES_FLOAT == type), ("invalid type") );

	{
		gles1_lighting *lighting = &ctx->state.api.gles1->lighting;
		gles_light * const l = &lighting->light[lindex];

		switch (pname)
		{
			/* general states */
			case GL_AMBIENT:
				_gles_convert_array_to_ftype( l->ambient, params, 4, type );
				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + lindex );
			break;

			case GL_DIFFUSE:
				_gles_convert_array_to_ftype( l->diffuse, params, 4, type );
				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + lindex );
			break;

			case GL_SPECULAR:
				_gles_convert_array_to_ftype( l->specular, params, 4, type );
				lighting->specular_field &= ~( 1 << lindex );
				lighting->specular_field |= MALI_SPECULAR_PRESENT_IN_SHADER( l->specular, lighting->specular ) << lindex;
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_SPECULAR, 
				                        ( 0x0 != ( lighting->specular_field & lighting->enabled_field ) ? GL_TRUE : GL_FALSE ) );
				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + lindex );
			break;

			case GL_POSITION:
			{
				gles1_transform * const transform = & ctx->state.api.gles1->transform;

				mali_matrix4x4 *modelview;
				mali_ftype temp[4];

				MALI_DEBUG_ASSERT_POINTER( transform );
				modelview = _gles1_transform_get_current_modelview_matrix( transform );
				MALI_DEBUG_ASSERT_POINTER( modelview );

				_gles_convert_array_to_ftype( temp, params, 4, type );

				__mali_matrix4x4_transform_array4( l->position, *modelview, temp );

				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + lindex );
			}
			break;

			/* attenuation states. */
			/* NOTE: we could make attenuation states not an array to conform better to the naming in the spec. */
			case GL_CONSTANT_ATTENUATION:
				f = _gles_convert_element_to_ftype(params, 0, type);

				if (f < 0) MALI_ERROR( GL_INVALID_VALUE );
				l->attenuation[GL_CONSTANT_ATTENUATION_INDEX] = f;
				_mali_set_attenuation_in_shader( ctx, l, lindex );
			break;

			case GL_LINEAR_ATTENUATION:
				f = _gles_convert_element_to_ftype(params, 0, type);
				if (f < 0) MALI_ERROR( GL_INVALID_VALUE );
				l->attenuation[GL_LINEAR_ATTENUATION_INDEX] = f;
				_mali_set_attenuation_in_shader( ctx, l, lindex );
			break;

			case GL_QUADRATIC_ATTENUATION:
				f = _gles_convert_element_to_ftype(params, 0, type);
				if (f < 0) MALI_ERROR( GL_INVALID_VALUE );
				l->attenuation[GL_QUADRATIC_ATTENUATION_INDEX] = f;
				_mali_set_attenuation_in_shader( ctx, l, lindex );
			break;

			/* spot states */
			case GL_SPOT_DIRECTION:
			{
				gles1_transform * const transform = & ctx->state.api.gles1->transform;

				mali_matrix4x4 *modelview;
				mali_ftype temp[3];

				MALI_DEBUG_ASSERT_POINTER( transform );
				modelview = _gles1_transform_get_current_modelview_matrix( transform );
				MALI_DEBUG_ASSERT_POINTER( modelview );

				_gles_convert_array_to_ftype(temp, params, 3, type);

				__mali_float_matrix4x4_rotate_array3( l->spot_direction, *modelview, temp );

				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + lindex );
			}
			break;

			case GL_SPOT_EXPONENT:
				f = _gles_convert_element_to_ftype(params, 0, type);
				if ((f < FLOAT_TO_FTYPE(0.0)) || (f > FLOAT_TO_FTYPE(128.0))) MALI_ERROR( GL_INVALID_VALUE );
				l->spot_exponent = f;
				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + lindex );
			break;

			case GL_SPOT_CUTOFF:
			{
				/* The legal range for spotlight cutoff is 0->90
				 * (inclusive) or 180 (which means 'disabled').
				 */

				f = _gles_convert_element_to_ftype(params, 0, type);

				if( FLOAT_TO_FTYPE( 180.0f ) == f )
				{
					lighting->spot_enabled_field &= ~( 1 << lindex );

					l->cos_spot_cutoff = -1.0f;
				}
				else if(( FLOAT_TO_FTYPE( 90.0f ) >= f ) && ( FLOAT_TO_FTYPE( 0.0f ) <= f ))
				{
					lighting->spot_enabled_field |= 1 << lindex;

					l->cos_spot_cutoff = _mali_sys_cos( ( float ) ( f * MALI_DEGREES_TO_RADIANS ) );
				}
				else
				{
					MALI_ERROR( GL_INVALID_VALUE );
				}

				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_SPOTLIGHT, 
				                        ( 0x0 != ( lighting->spot_enabled_field & lighting->enabled_field ) ? GL_TRUE : GL_FALSE ) );

				/* glGet must return the cutoff angle
				 * verbatim.  Since arc-cos cannot
				 * guarantee to return the same value we
				 * save a copy specifically for glGet.
				 */
				lighting->spotlight_cutoff[lindex] = f;

				mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + lindex );
			}
			break;

			default:
				MALI_ERROR( GL_INVALID_ENUM );
		}
	}
	GL_SUCCESS;
}

/*
this routine should basicly just check for invalid pnames, and pass the parameter to the
array-setter to minimize error-checking code
*/
GLenum _gles1_light(struct gles_context * ctx, GLenum light, GLenum pname, const GLvoid *param, const gles_datatype type)
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	switch (pname)
	{
		case GL_SPOT_CUTOFF:
		case GL_SPOT_EXPONENT:
		case GL_CONSTANT_ATTENUATION:
		case GL_LINEAR_ATTENUATION:
		case GL_QUADRATIC_ATTENUATION:
			return _gles1_lightv( ctx, light, pname, param, type );
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}
}

void _gles1_push_twosided_lighting_state(struct gles_context* ctx)
{
	int i;
	mali_bool state = MALI_FALSE;

	if( vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_TWOSIDED) && ctx->state.api.gles1->lighting.enabled )
	{
		gles_common_state * const state_common  = & ctx->state.common;
		for( i = 0; i < GLES1_MAX_LIGHTS; i++ )
		{
			if( mali_statebit_get( state_common, GLES_STATE_LIGHT0_ENABLED + i ) )
			{
				state = MALI_TRUE;
				break;
			}
		}
	}
	fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_TWOSIDED_LIGHTING, state);

}

