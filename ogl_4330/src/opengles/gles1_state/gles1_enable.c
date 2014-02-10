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
#include <gles_state.h>
#include <gles_context.h>
#include <gles_config_extension.h>
#include <gles_fb_interface.h>
#include <gles_sg_interface.h>

#include "gles1_state.h"
#include "gles1_error.h"
#include "gles1_vertex_array.h"
#include "gles1_enable.h"

#include <opengles/gles_sg_interface.h>
#include <opengles/gles_gb_interface.h>

/* Internal function declaration */
MALI_STATIC GLenum _gles1_vertex_attrib_pointer_state(gles_common_state *pstate, GLuint index, GLboolean state );

/* External function definitions */
GLenum _gles1_client_state( struct gles_context *ctx, GLenum cap, GLboolean state )
{
	GLuint index;
	struct gles_state *pstate;
	GLenum ret;
	struct gles_vertex_array* vertex_array;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->state.api.gles1);

	pstate = &( ctx->state );

	switch (cap)
	{
		case GL_VERTEX_ARRAY:
			index = GLES_VERTEX_ATTRIB_POSITION;
		break;
		case GL_NORMAL_ARRAY:
			index = GLES_VERTEX_ATTRIB_NORMAL;
		break;
		case GL_COLOR_ARRAY:
			index = GLES_VERTEX_ATTRIB_COLOR;
			if (!ctx->state.api.gles1->lighting.enabled && !state)
			{
				fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE, ARG_CONSTANT_COLOR);
			} else {
				fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE, ARG_PRIMARY_COLOR);
			}

		break;
		case GL_TEXTURE_COORD_ARRAY:
		{
			u32 idx = (GLuint)pstate->common.vertex_array.client_active_texture;
			index = (GLuint)GLES_VERTEX_ATTRIB_TEX_COORD0 + idx;
		}
		break;

		/* OES extensions */
		case GL_POINT_SIZE_ARRAY_OES:
			index = GLES_VERTEX_ATTRIB_POINT_SIZE;
			if ( GL_TRUE == state )
			{
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_POINTSIZE_COPY, GL_TRUE);
			}
			else
			{
				GLboolean point_size_attenuation =
					ctx->state.api.gles1->rasterization.point_distance_attenuation[GL_CONSTANT_ATTENUATION_INDEX] != 1.0f ||
					ctx->state.api.gles1->rasterization.point_distance_attenuation[GL_LINEAR_ATTENUATION_INDEX] != 0.0f ||
					ctx->state.api.gles1->rasterization.point_distance_attenuation[GL_QUADRATIC_ATTENUATION_INDEX] != 0.0f;

				/* If point-size-attenuation is present, it can still be true */
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_POINTSIZE_COPY, point_size_attenuation);
			}
		break;
#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
		case GL_MATRIX_INDEX_ARRAY_OES:
			index = GLES_VERTEX_ATTRIB_MATRIX_INDEX;
		break;
		case GL_WEIGHT_ARRAY_OES:
			index = GLES_VERTEX_ATTRIB_WEIGHT;
		break;
#endif
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	ret = _gles1_vertex_attrib_pointer_state( &pstate->common, index, state );

	vertex_array = &ctx->state.common.vertex_array;
	MALI_DEBUG_ASSERT_POINTER(vertex_array);

	_gles_gb_modify_attribute_stream( ctx, vertex_array->attrib_array, index);

	return ret;
}

GLenum _gles1_enable( struct gles_context *ctx, GLenum cap, GLboolean state )
{
	GLint index;
	gles_common_state *state_common;
	gles1_state       *state_gles1;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->state.api.gles1);

	state_common = &(ctx->state.common);
	state_gles1  = ctx->state.api.gles1;

	MALI_DEBUG_ASSERT((GL_TRUE == state) || (GL_FALSE == state), ("invalid state %d\n", state));

	switch( cap )
	{
		case GL_ALPHA_TEST:
			if( _gles_fb_get_alpha_test( ctx ) != state )
			{
				_gles_fb_set_alpha_test( ctx, state );
			}
			break;
		case GL_BLEND:
			if( _gles_fb_get_blend( ctx ) != state )
			{
				_gles_fb_set_blend( ctx, state );
			}
			break;
		case GL_COLOR_LOGIC_OP:
			if( _gles_fb_get_color_logic_op( ctx ) != state )
			{
				_gles_fb_set_color_logic_op( ctx, state );
#if defined(USING_MALI400) || defined(USING_MALI450)
				fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_LOGIC_OPS_ROUNDING_ENABLE, (state)?(MALI_TRUE):(MALI_FALSE));
#endif
			}
			break;
		case GL_DEPTH_TEST:
			if( _gles_fb_get_depth_test( ctx ) != state )
			{
				_gles_fb_set_depth_test( ctx, state );
			}
			break;
		case GL_MULTISAMPLE:
			if( _gles_fb_get_multisample( ctx ) != state )
			{
				_gles_fb_set_multisample( ctx, state );
			}
			break;
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
			if( _gles_fb_get_sample_alpha_to_coverage( ctx ) != state )
			{
				_gles_fb_set_sample_alpha_to_coverage( ctx, state );
			}
			break;
		case GL_STENCIL_TEST:
			if( _gles_fb_get_stencil_test( ctx ) != state )
			{
				_gles_fb_set_stencil_test( ctx, state );
			}
			break;
		case GL_POLYGON_OFFSET_FILL:
			if( _gles_fb_get_polygon_offset_fill( ctx ) != state )
			{
				_gles_fb_set_polygon_offset_fill( ctx, state );
				mali_statebit_set( state_common, MALI_STATE_VS_POLYGON_OFFSET_UPDATE_PENDING );
			}
			break;
		case GL_SAMPLE_COVERAGE:
			if( _gles_fb_get_sample_coverage( ctx ) != state )
			{
				_gles_fb_set_sample_coverage( ctx, state );
			}
			break;
		case GL_SAMPLE_ALPHA_TO_ONE:
			if( _gles_fb_get_sample_alpha_to_one( ctx ) != state )
			{
				_gles_fb_set_sample_alpha_to_one( ctx, state );
			}
			break;
		case GL_DITHER:
			if( _gles_fb_get_dither( ctx ) != state )
			{
				_gles_fb_set_dither( ctx, state );
			}
			break;
		case GL_POINT_SMOOTH:
			if( _gles_fb_get_point_smooth( ctx ) != state )
			{
				_gles_fb_set_point_smooth( ctx, state );
			}
			break;
		case GL_LINE_SMOOTH:
			if( _gles_fb_get_line_smooth( ctx ) != state )
			{
				_gles_fb_set_line_smooth( ctx, state );
			}
			break;


		case GL_CLIP_PLANE0:
		case GL_CLIP_PLANE1:
		case GL_CLIP_PLANE2:
		case GL_CLIP_PLANE3:
		case GL_CLIP_PLANE4:
		case GL_CLIP_PLANE5:
			index = (int)cap - (int)GL_CLIP_PLANE0;

			/* Need to handle the situation that the number of clip-planes supported by the driver is != the number that can get here */
			MALI_CHECK_RANGE(index, 0, GLES1_MAX_CLIP_PLANES-1, GL_INVALID_ENUM);
			MALI_DEBUG_ASSERT( 1 == GLES1_MAX_CLIP_PLANES, ("Invalid number of clip-planes, need to change code here to work") );

			if ( state != vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_CLIPPLANE_PLANE0))
			{
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_CLIPPLANE_PLANE0, ( GL_TRUE == state ? 0x1 : 0x0 ) );
				fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_CLIP_PLANE_ENABLE(index), state);

				if ( GL_TRUE == state )
				{
					_mali_gles_sg_write_clip_plane_patched( ctx, &ctx->state.api.gles1->transform.clip_plane[ index ][ 0 ] );
				}
			}
		break;
		case GL_COLOR_MATERIAL:
		{
			lighting_colormaterial colormaterial_state;

			if ( GL_TRUE == state )
			{
				colormaterial_state = COLORMATERIAL_AMBIENTDIFFUSE;
			}
			else
			{
				colormaterial_state = COLORMATERIAL_OFF;
			}

			if ( colormaterial_state != vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL))
			{
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL, colormaterial_state);

				if ( GL_TRUE == state )
				{
					int i;
					for ( i = 0; i < 4; i++ )
					{
						state_gles1->lighting.ambient[i] = state_gles1->current.color[i];
						state_gles1->lighting.diffuse[i] = state_gles1->current.color[i];
					}
					mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT_ALL_DIRTY );
					mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY );
				}
			}
		}
		break;
		case GL_CULL_FACE:
			state_common->rasterization.cull_face = state;
		break;
		case GL_FOG:
			if ( state != vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST))
			{
			 	fog_kind fogstate = FOG_NONE;
				if(state)
				{
					switch(state_gles1->coloring.fog_mode)
					{
							case GL_LINEAR:
								{
									float fog_factor = state_gles1->coloring.fog_end - state_gles1->coloring.fog_start != 0.0f
										? -1.0f/(state_gles1->coloring.fog_end - state_gles1->coloring.fog_start)
										: -1.0f;
									float fog_add = state_gles1->coloring.fog_end - state_gles1->coloring.fog_start != 0.0f
										? state_gles1->coloring.fog_end/(state_gles1->coloring.fog_end - state_gles1->coloring.fog_start)
										: 0.0f;
									MALI_GLES_SG_WRITE_FLOAT(FogFactor, fog_factor);
									MALI_GLES_SG_WRITE_FLOAT(FogAdd, fog_add);
									fogstate = FOG_LINEAR;
									vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST, DIST_ADD);
								}
							break;
							case GL_EXP:
								MALI_GLES_SG_WRITE_FLOAT(FogFactor, log2_e * state_gles1->coloring.fog_density);
								MALI_GLES_SG_WRITE_FLOAT(FogAdd, 0.0);
								vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST, DIST_ON);
								fogstate = FOG_EXP;
							break;
							case GL_EXP2:
								MALI_GLES_SG_WRITE_FLOAT(FogFactor, sqrt_log2_e * state_gles1->coloring.fog_density);
								MALI_GLES_SG_WRITE_FLOAT(FogAdd, 0.0);
								vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST, DIST_ON);
								fogstate = FOG_EXP2;
							break;
							default: MALI_DEBUG_ASSERT(0, ("illegal state")); break;
					}
				} else 
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST, DIST_OFF);
				}

				fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_FOG_MODE, fogstate);
			}
		break;

		case GL_LIGHT0:
		case GL_LIGHT1:
		case GL_LIGHT2:
		case GL_LIGHT3:
		case GL_LIGHT4:
		case GL_LIGHT5:
		case GL_LIGHT6:
		case GL_LIGHT7:
			index = (GLint) cap - (GLint) GL_LIGHT0;
			MALI_CHECK_RANGE( index, 0, GLES1_MAX_LIGHTS - 1, GL_INVALID_ENUM );

			if( state != mali_statebit_get( state_common, GLES_STATE_LIGHT0_ENABLED + index ))
			{
				gles1_lighting *lighting = &ctx->state.api.gles1->lighting;

				if ( GL_TRUE == state )
				{
					mali_statebit_set( state_common, GLES_STATE_LIGHT0_ENABLED + index );
					mali_statebit_set( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY );
					state_gles1->lighting.n_lights++;
				}
				else
				{
					mali_statebit_unset( state_common, GLES_STATE_LIGHT0_ENABLED + index );
					state_gles1->lighting.n_lights--;
				}

				_gles1_push_twosided_lighting_state(ctx);

				if ( state_gles1->lighting.enabled )
				{
					switch( state_gles1->lighting.n_lights )
					{
						case 0:
							vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS, NUMLIGHTS_ZERO);
						break;
						case 1:
							vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS, NUMLIGHTS_ONE);
						break;
						default:
							vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS, NUMLIGHTS_MANY);
						break;
					}
				}
				state_gles1->lighting.enabled_field &= ~( 1 << index );
				state_gles1->lighting.enabled_field |= state << index;

				/* Update all values for lighting here */
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_SPECULAR, 
				                        ( 0x0 != ( lighting->specular_field & lighting->enabled_field ) ? GL_TRUE : GL_FALSE ) );
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_ATTENUATION, 
				                        ( ( 0x0 != ( lighting->attenuation_field & lighting->enabled_field ) ) ? GL_TRUE : GL_FALSE ) );
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_SPOTLIGHT, 
				                        ( 0x0 != ( lighting->spot_enabled_field & lighting->enabled_field ) ? GL_TRUE : GL_FALSE ) );
			}
		break;
		case GL_LIGHTING:
			if ( state != state_gles1->lighting.enabled )
			{
				state_gles1->lighting.enabled = state;
				_gles1_push_twosided_lighting_state(ctx);
				if (!state && !ctx->state.common.vertex_array.attrib_array[GLES_VERTEX_ATTRIB_COLOR].enabled)
				{
					fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE, ARG_CONSTANT_COLOR);
				} else {
					fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE, ARG_PRIMARY_COLOR);
				}

				if ( state_gles1->lighting.enabled )
				{
					switch( state_gles1->lighting.n_lights )
					{
						case 0:
							vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS, NUMLIGHTS_ZERO);
						break;
						case 1:
							vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS, NUMLIGHTS_ONE);
						break;
						default:
							vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS, NUMLIGHTS_MANY);
						break;
					}

					mali_statebit_set( &ctx->state.common, MALI_STATE_NORMAL_MATRIX_UPDATE_PENDING );
				}
				else
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS, NUMLIGHTS_OFF);
				}
			}
		break;


#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
		case GL_MATRIX_PALETTE_OES:
			if ( state != vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING) )
			{
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING, ( GL_TRUE == state ? 0x1 : 0x0 ));
				if ( GL_TRUE == state_gles1->transform.normalize || 
				     ( vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING) 
					   && state_gles1->transform.rescale_normal 
					 ) 
				   )
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_NORMALIZE, NORMALIZE_NORMALIZE);
				}
				else
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_NORMALIZE, NORMALIZE_OFF);
				}
			}
		break;
#endif
		case GL_NORMALIZE:
			if ( state != state_gles1->transform.normalize )
			{
				state_gles1->transform.normalize = state;
				if ( GL_TRUE == state || 
				     ( vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING) 
					   && state_gles1->transform.rescale_normal 
					 ) 
				   )
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_NORMALIZE, NORMALIZE_NORMALIZE);
				}
				else
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_NORMALIZE, NORMALIZE_OFF);
				}
			}
		break;
		case GL_POINT_SPRITE_OES:
			state_gles1->texture_env.point_sprite_enabled = state;
		break;
		case GL_RESCALE_NORMAL:
			if ( state != state_gles1->transform.rescale_normal )
			{
				state_gles1->transform.rescale_normal = state;
				if ( GL_TRUE == state_gles1->transform.normalize || 
				     ( vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING) 
					   && state_gles1->transform.rescale_normal 
					 ) 
				   )
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_NORMALIZE, NORMALIZE_NORMALIZE);
				}
				else
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_NORMALIZE, NORMALIZE_OFF);
				}
			}
		break;
		case GL_SCISSOR_TEST:
			if( state != mali_statebit_get( state_common, GLES_STATE_SCISSOR_ENABLED ))
			{
				(void) mali_statebit_exchange( state_common, GLES_STATE_SCISSOR_ENABLED, state );
				mali_statebit_set( state_common, MALI_STATE_SCISSOR_DIRTY ); 
			}
			break;
		case GL_TEXTURE_2D:
		case GL_TEXTURE_EXTERNAL_OES:
		case GL_TEXTURE_CUBE_MAP_OES:
		{
			gles_texture_unit *tex_unit;
			u32* transform_field = &(ctx->state.api.gles1->transform.texture_transform_field);
			enum gles_texture_target dimensionality;

			tex_unit = & state_common->texture_env.unit[ state_common->texture_env.active_texture ];
			
			dimensionality = _gles_get_dimensionality(cap, GLES_API_VERSION_1);
			if ((int)dimensionality == GLES_TEXTURE_TARGET_INVALID) return GL_INVALID_ENUM;

			if ( state != tex_unit->enable_vector[dimensionality] )
			{
				int i; 
				int idx = state_common->texture_env.active_texture;
				mali_bool is_any_enabled = MALI_FALSE;
				tex_unit->enable_vector[dimensionality] = state;
				
				for(i = 0; i < GLES_TEXTURE_TARGET_COUNT; i++)
				{
					if( tex_unit->enable_vector[i] ) is_any_enabled = MALI_TRUE;
				}
				
				if ( GL_TRUE == is_any_enabled )
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_STAGE_ENABLE(idx), GL_TRUE);
					(*transform_field) |= ( 1 << idx);
				}
				else
				{
					vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_STAGE_ENABLE(idx), GL_FALSE);
					(*transform_field) &= ~( 1 << idx);
				}
			}
			break;
		}
		case GL_TEXTURE_GEN_STR_OES:
		{
			int idx = state_common->texture_env.active_texture;
			if ( state != vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_STAGE_TEXGEN_ENABLE(idx)))
			{
				vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_STAGE_TEXGEN_ENABLE(idx), state);
			}

			if ( state )
			{
				state_gles1->texture_env.tex_gen_enabled |= 1 << idx;
			}
			else
			{
				state_gles1->texture_env.tex_gen_enabled &= ~( 1 << idx);
			}

			if ( state_gles1->texture_env.tex_gen_enabled )
			{
				mali_statebit_set( &ctx->state.common, MALI_STATE_NORMAL_MATRIX_UPDATE_PENDING );
			}
			break;
		}
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;
}


/* Internal function definitions */
MALI_STATIC GLenum _gles1_vertex_attrib_pointer_state(gles_common_state *pstate, GLuint index, GLboolean state )
{
	MALI_DEBUG_ASSERT_POINTER(pstate);

	/* check any input-errors */
	if (index >= GLES_VERTEX_ATTRIB_COUNT)
	{
		MALI_ERROR( GL_INVALID_VALUE );
	}

	if ( state != pstate->vertex_array.attrib_array[ index ].enabled )
	{
		pstate->vertex_array.attrib_array[ index ].enabled = state;
	}

	GL_SUCCESS;
}
