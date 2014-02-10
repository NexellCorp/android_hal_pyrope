/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_shader_generator_context.h"
#include <shared/shadergen_interface.h>
#include "../mali_gp_geometry_common/gles_geometry_backend.h"
#include "../gles_context.h"

#include "../gles1_state/gles1_state.h"

/* Set the given position in the attribute remap table to map to the internal attribute
   position in the vertex shader for the given attribute, provided that the given condition
   is true (indicating that the attribute in question is used by the shader). If the
   condition is false, set the attribute remap table entry for the attribute to -1 to
   indicate that the attribute is not used by the shader.
*/
#define MAP(cond, pos, attr) remap_table[pos] = cond ? VERTEX_ATTRIBUTE_POS(attr)/4 : -1

/* Set the given texture coordinate attribute in the attribute remap table to map to the
   internal attribute position in the vertex shader for that texture coordinate, provided
   that the texture unit is enabled. If the texture unit is disabled, set the attribute
   remap table entry for that texture coordinate to -1 to indicate that it is not used by
   the shader.
*/
#define MAPTEXCOORD(i) \
  MAP(vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_STAGE_ENABLE(i)), \
  GLES_VERTEX_ATTRIB_TEX_COORD##i, attr_TexCoord##i)

void _gles_sg_make_attribute_remap_table(const struct vertex_shadergen_state * sgstate, int *remap_table)
{
	int i;
	mali_bool bTexgen = MALI_FALSE;

	MALI_DEBUG_ASSERT_POINTER(remap_table);

	for( i = 0; i < MAX_TEXTURE_STAGES ; i++)
	{
		if( vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_STAGE_TEXGEN_ENABLE(i)))
		{
			bTexgen = MALI_TRUE;
			break;
		}
	}	

	/* Set up attribute remap table, with -1 for unused attributes */
	MAP( MALI_TRUE,
		GLES_VERTEX_ATTRIB_POSITION, attr_Position);
	MAP(   NUMLIGHTS_ONE == vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS) 
	    || NUMLIGHTS_MANY == vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS)
	    || bTexgen,
		GLES_VERTEX_ATTRIB_NORMAL, attr_Normal);
	MAP(   NUMLIGHTS_OFF == vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS) 
	    || COLORMATERIAL_OFF != vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL),
		GLES_VERTEX_ATTRIB_COLOR, attr_PrimaryColor);
	MAP(vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_FEATURE_POINTSIZE_COPY),
		GLES_VERTEX_ATTRIB_POINT_SIZE, attr_PointSize);
	MAP(vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING),
		GLES_VERTEX_ATTRIB_WEIGHT, attr_SkinningWeights);
	MAP(vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING),
		GLES_VERTEX_ATTRIB_MATRIX_INDEX, attr_SkinningIndices);
	MAPTEXCOORD(0);
	MAPTEXCOORD(1);
#if !HARDWARE_ISSUE_4326
	MAPTEXCOORD(2);
	MAPTEXCOORD(3);
	MAPTEXCOORD(4);
	MAPTEXCOORD(5);
	MAPTEXCOORD(6);
	MAPTEXCOORD(7);
#endif
	{
		int i;
		for (i = GLES1_VERTEX_ATTRIB_COUNT ; i < GLES_VERTEX_ATTRIB_COUNT ; i++) {
			remap_table[i] = -1;
		}
	}
}

#undef MAP
#undef MAPTEXCOORD

#define SET_ATTR_4(arr_idx, current_name) _gles_copy_vec4(state->common.vertex_array.attrib_array[arr_idx].value,  state->api.gles1->current.current_name)
#define SET_ATTR_3(arr_idx, current_name) _gles_copy_vec3(state->common.vertex_array.attrib_array[arr_idx].value,  state->api.gles1->current.current_name)
void _gles_sg_update_current_attribute_values(gles_state *state)
{
	int i;
	MALI_DEBUG_ASSERT_POINTER(state);

	SET_ATTR_3(GLES_VERTEX_ATTRIB_NORMAL, normal);
	SET_ATTR_4(GLES_VERTEX_ATTRIB_COLOR, color);
	for(i = 0; i < GLES_MAX_TEXTURE_UNITS; i++)
	{
		SET_ATTR_4(GLES_VERTEX_ATTRIB_TEX_COORD0+i, tex_coord[i]);
	}
	state->common.vertex_array.attrib_array[GLES_VERTEX_ATTRIB_POINT_SIZE].value[0] = state->common.rasterization.point_size;
}
#undef SET_ATTR_4
#undef SET_ATTR_3

#define MAX_MATRIX_PALETTE_UNIFORMS GLES1_MAX_PALETTE_MATRICES_OES

void _gles_sg_extract_vertex_uniforms(gles_context *ctx, const struct vertex_shadergen_state* sgstate, float *uniforms, bs_program *program_binary) {
	gles_common_state *state_common;
	gles1_state       *state_gles1;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->state.api.gles1);
	MALI_DEBUG_ASSERT_POINTER(uniforms);

	state_common = &(ctx->state.common);
	state_gles1  = ctx->state.api.gles1;

		/* *** lighting *** */
	if (state_gles1->lighting.enabled) {
		int i,j;
		j = 0;
		for( i = 0; i < GLES1_MAX_LIGHTS; i++ )
		{
			if( mali_statebit_get( state_common, GLES_STATE_LIGHT0_ENABLED + i ) )
			{
				if ( mali_statebit_get( &ctx->state.common, GLES_STATE_LIGHT_ALL_DIRTY ) ||
				     mali_statebit_get( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + i ) ||
				     ( state_gles1->lighting.enabled_field != ctx->sg_ctx->last_light_enabled_field ) )
				{
					const gles_light * const light = &state_gles1->lighting.light[i];
					float ambient[3];
					float diffuse[3];
					float specular[3];
					float spotdir[3];

					/* If color material is enabled, the multiplication of ambient and diffuse
					   colors by the material colors is performed in the vertex shader.
					   If color material is disabled, the light colors are pre-multiplied by
					   the material colors.
					   The specular color is always premultiplied.
					*/
					if (COLORMATERIAL_AMBIENTDIFFUSE == vertex_shadergen_decode( &ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL ))
					{
						MALI_GLES_SG_WRITE_ARRAY_VEC3(LightAmbient, j, light->ambient);
						MALI_GLES_SG_WRITE_ARRAY_VEC3(LightDiffuse, j, light->diffuse);
					}
					else
					{
						_gles_mul_vec3(ambient, light->ambient, state_gles1->lighting.ambient);
						_gles_mul_vec3(diffuse, light->diffuse, state_gles1->lighting.diffuse);
						MALI_GLES_SG_WRITE_ARRAY_VEC3(LightAmbient, j, ambient);
						MALI_GLES_SG_WRITE_ARRAY_VEC3(LightDiffuse, j, diffuse);
					}
					_gles_mul_vec3(specular, light->specular, state_gles1->lighting.specular);
					MALI_GLES_SG_WRITE_ARRAY_VEC3(LightSpecular, j, specular);
                    if(light->position[3] == 1.0 || light->position[3] == 0.0 || light->position[3] == -0.0)
                    {
                        MALI_GLES_SG_WRITE_ARRAY_VEC4(LightPosition, j, light->position);
                    }
                    else
                    {
                        float invw = (1.0/light->position[3]);
#if defined(__ARMCC__)
                        float positionPrescaled[4];
                        positionPrescaled[0] = light->position[0]*invw;
                        positionPrescaled[1] = light->position[1]*invw;
                        positionPrescaled[2] = light->position[2]*invw;
                        positionPrescaled[3] = light->position[3]*invw;
#else
                        float positionPrescaled[4] = { light->position[0]*invw, light->position[1]*invw, light->position[2]*invw, light->position[3]*invw };
#endif
                        MALI_GLES_SG_WRITE_ARRAY_VEC4(LightPosition, j, positionPrescaled);
                    }
					_gles_normalize_vec3(spotdir, light->spot_direction);
					MALI_GLES_SG_WRITE_ARRAY_VEC3(LightSpotDirection, j, spotdir);
					MALI_GLES_SG_WRITE_ARRAY_FLOAT(LightSpotExponent, j, light->spot_exponent);
					MALI_GLES_SG_WRITE_ARRAY_FLOAT( LightSpotCosCutoffAngle, j, light->cos_spot_cutoff );
                    if(light->position[3] < -0.0 || light->position[3] > 0.0)
                    {
                        MALI_GLES_SG_WRITE_ARRAY_VEC3(LightAttenuation, j, light->attenuation);
                    }
                    else
                    {
                        /* Directional light (w = 0) should have no attenuation */
                        float noAttenuation[3] = { 1.0, 0.0, 0.0 };
                        MALI_GLES_SG_WRITE_ARRAY_VEC3(LightAttenuation, j, noAttenuation);
                    }
					mali_statebit_unset( &ctx->state.common, GLES_STATE_LIGHT0_DIRTY + i );
					mali_statebit_set( & ctx->state.common, MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING);
				}
				j++;
			}
		}

		mali_statebit_unset( &ctx->state.common, GLES_STATE_LIGHT_ALL_DIRTY );


		/* inits handle the zero case and this should cover change in vertex shader */
		if( mali_statebit_get( & ctx->state.common, MALI_STATE_VS_PRS_UPDATE_PENDING ) ||
			ctx->sg_ctx->last_light_enabled_field != state_gles1->lighting.enabled_field)
		{
			MALI_GLES_SG_WRITE_FLOAT(NumLights, (float)j);
			ctx->sg_ctx->last_light_enabled_field = state_gles1->lighting.enabled_field;
			mali_statebit_set( & ctx->state.common, MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING);
		}


		if ( mali_statebit_get( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY ) )
		{
			float scenecolor[3];
			_gles_mul_vec3(scenecolor, state_gles1->lighting.ambient, state_gles1->lighting.light_model_ambient);
			_gles_add_vec3(scenecolor, scenecolor, state_gles1->lighting.emission);
			MALI_GLES_SG_WRITE_VEC3(SceneAmbient, state_gles1->lighting.light_model_ambient);
			MALI_GLES_SG_WRITE_VEC3(SceneColor, scenecolor);
			MALI_GLES_SG_WRITE_FLOAT(MaterialDiffuseAlpha, state_gles1->lighting.diffuse[3]);
			MALI_GLES_SG_WRITE_VEC3(MaterialEmissive, state_gles1->lighting.emission);
			MALI_GLES_SG_WRITE_FLOAT(MaterialSpecularExponent, state_gles1->lighting.shininess);
			mali_statebit_set( & ctx->state.common, MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING);
		}
		mali_statebit_unset( &ctx->state.common, GLES_STATE_LIGHTING_DIRTY );
	}

	/* *** transformation *** */
	{
		float (*mv)[4] = state_gles1->transform.modelview_matrix[state_gles1->transform.modelview_matrix_stack_depth-1];
		float (*proj)[4] = state_gles1->transform.projection_matrix[state_gles1->transform.projection_matrix_stack_depth-1];

		if( mali_statebit_get( state_common, MALI_STATE_PROJECTION_VIEWPORT_MATRIX_UPDATE_PENDING ))
		{
			float viewport[8];
			int i, j;
			float (*proj_vp)[4] = state_gles1->transform.projection_viewport_matrix;

			_gles_gb_calculate_vs_viewport_transform(ctx, viewport);
			for (i = 0; i < 4; i++)
			{
				for (j = 0; j < 3; j++)
				{
					proj_vp[i][j] = viewport[j] * proj[i][j];
				}
				proj_vp[i][3] = proj[i][3];
			}

			/* Do not unset dirty-state yet, also need to check if the modelview*projection should be updated */
		}

		if ( vertex_shadergen_decode( &ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING ) &&
		     mali_statebit_get( & ctx->state.common, MALI_STATE_SKINNING_MATRIX_GROUP_UPDATE_PENDING ) )
		{
			/* Matrix palette transformation */
			int i;
			float (*palette_vectors)[4] = (float (*)[4])state_gles1->transform.matrix_palette;
			int numbones = state_common->vertex_array.attrib_array[GLES_VERTEX_ATTRIB_MATRIX_INDEX].size;

			/* Single modelview matrix transformation */
			/* the dirty bit for this is MALI_STATE_MODELVIEW_PROJECTION_MATRIX_UPDATE_PENDING but is needed here */
			MALI_GLES_SG_WRITE_MAT4(ModelView, mv);

			/* Each dirty flag for matrix-palettes are grouped in 4 */
			for (i = 0 ; i < MAX_MATRIX_PALETTE_UNIFORMS / 4; i++)
			{
				int j;

				if ( mali_statebit_get( & ctx->state.common, MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING0 + i ) )
				{
					/* There are 4 matrices to be updated * 4 vec4 floats */
#if (!MALI_OSU_MATH)
					for ( j = 0; j < 4 * 4; j ++ )
					{
						MALI_GLES_SG_WRITE_ARRAY_VEC4(SkinningMatrices, 16 * i + j, palette_vectors[ 16 * i + j ]);
					}
#else
					MALI_GLES_SG_WRITE_MAT4_WITH_OFFSET( SkinningMatrices, 16 * i, palette_vectors[ 16 * i]);
					MALI_GLES_SG_WRITE_MAT4_WITH_OFFSET( SkinningMatrices, 16 * i + 4, palette_vectors[ 16 * i +4]);
					MALI_GLES_SG_WRITE_MAT4_WITH_OFFSET( SkinningMatrices, 16 * i + 8 , palette_vectors[ 16 * i + 8]);
					MALI_GLES_SG_WRITE_MAT4_WITH_OFFSET( SkinningMatrices, 16 * i + 12, palette_vectors[ 16 * i + 12]);
#endif

					mali_statebit_unset( & ctx->state.common, MALI_STATE_SKINNING_MATRIX_UPDATE_PENDING0 + i );
				}
			}

			MALI_GLES_SG_WRITE_MAT4(ProjViewport, state_gles1->transform.projection_viewport_matrix);
			MALI_GLES_SG_WRITE_FLOAT(NumBones, (float)numbones);
			mali_statebit_unset( & ctx->state.common, MALI_STATE_SKINNING_MATRIX_GROUP_UPDATE_PENDING );
			mali_statebit_set( & ctx->state.common, MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING);
		} 
		else 
		{
			if ( mali_statebit_get( state_common, MALI_STATE_PROJECTION_VIEWPORT_MATRIX_UPDATE_PENDING) ||
				 mali_statebit_get( state_common, MALI_STATE_MODELVIEW_PROJECTION_MATRIX_UPDATE_PENDING) )
			{
				/* Single modelview matrix transformation */
				/* the dirty bit for this is MALI_STATE_MODELVIEW_PROJECTION_MATRIX_UPDATE_PENDING */
				MALI_GLES_SG_WRITE_MAT4(ModelView, mv);
#if (!MALI_OSU_MATH)
				__mali_float_matrix4x4_multiply(state_gles1->transform.modelview_projection_matrix, state_gles1->transform.projection_viewport_matrix, *(MALI_REINTERPRET_CAST(const mali_matrix4x4*)mv));
#else
				_mali_osu_matrix4x4_multiply( (float*)state_gles1->transform.modelview_projection_matrix, (float*)state_gles1->transform.projection_viewport_matrix, (float*)mv );
#endif

				MALI_GLES_SG_WRITE_MAT4(ModelViewProjViewport, state_gles1->transform.modelview_projection_matrix);

				mali_statebit_unset( state_common, MALI_STATE_PROJECTION_VIEWPORT_MATRIX_UPDATE_PENDING );
				mali_statebit_unset( state_common, MALI_STATE_MODELVIEW_PROJECTION_MATRIX_UPDATE_PENDING );
				mali_statebit_set( & ctx->state.common, MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING);
			}

			/* Normal matrix is only needed if lighting is enabled */
			if ( (state_gles1->lighting.enabled || state_gles1->texture_env.tex_gen_enabled) &&
			     mali_statebit_get( state_common, MALI_STATE_NORMAL_MATRIX_UPDATE_PENDING ))

			{
				/* Calculate inverse transpose of upper-left 3x3 of modelview matrix */
				mali_matrix4x4 normal;
				float a, b, c, d, e, f, g, h, k;
				float ek_fh, fg_kd, dh_eg;
				float det;

				a = mv[0][0];
				b = mv[1][0];
				c = mv[2][0];
				d = mv[0][1];
				e = mv[1][1];
				f = mv[2][1];
				g = mv[0][2];
				h = mv[1][2];
				k = mv[2][2];

				ek_fh = e*k - f*h;
				fg_kd = f*g - k*d;
				dh_eg = d*h - e*g;

				det = a*(ek_fh) + b*(fg_kd) + c*(dh_eg);

				if(det!=0.0f)
				{
					float inv_det = 1.0/det;
					normal[0][0] = (ek_fh)*inv_det; 
					normal[0][1] = (c*h-b*k)*inv_det; 
					normal[0][2] = (b*f-c*e)*inv_det; 

					normal[1][0] = (fg_kd)*inv_det; 
					normal[1][1] = (a*k-c*g)*inv_det; 
					normal[1][2] = (c*d-a*f)*inv_det; 

					normal[2][0] = (dh_eg)*inv_det; 
					normal[2][1] = (b*g-a*h)*inv_det; 
					normal[2][2] = (a*e-b*d)*inv_det; 
				}
				else
				{
					/* Don't care if the inversion fails. Resulting normal matrix is simply undefined */
#if (!MALI_OSU_MATH)
					__mali_float_matrix4x4_transpose( normal, mv );
#else
					_mali_osu_matrix4x4_transpose( (float*)normal, (float*)mv );
#endif

				}

				normal[0][3] = 0.0f;
				normal[1][3] = 0.0f;
				normal[2][3] = 0.0f;
				normal[3][0] = 0.0f;
				normal[3][1] = 0.0f;
				normal[3][2] = 0.0f;
				normal[3][3] = 1.0f;

				if ( state_gles1->transform.rescale_normal )
				{
					/* Rescale normals by pre-multiplying scale factor into normal matrix */
					float recscale = _mali_sys_sqrt
					    ( normal[0][2]*normal[0][2] +
					      normal[1][2]*normal[1][2] +
					      normal[2][2]*normal[2][2]);
					float scale = recscale != 0.0f ? 1.0f / recscale : 1.0f;
					int i,j;
					for (i = 0 ; i < 3 ; i++)
					{
						for (j = 0 ; j < 3 ; j++)
						{
							normal[ i ][ j ] *= scale;
						}
					}
				}

				MALI_GLES_SG_WRITE_MAT3(NormalMatrix, normal);

				mali_statebit_unset( state_common, MALI_STATE_NORMAL_MATRIX_UPDATE_PENDING );
				mali_statebit_set( & ctx->state.common, MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING);
			}
		}
	}

	/* *** texture coordinate transformation *** */
	{
		u16 i;
		const u8 num_tex_matrices = MALI_STATE_TEXTURE_MATRIX_UPDATE_END - MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING0;
		u8  tex_mat_update_bits = 0;
		

		for (i=0; i < num_tex_matrices; i++)
		{ 
			tex_mat_update_bits |= (u8)mali_statebit_get( & ctx->state.common, MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING0+i) << i;
		}

		if (tex_mat_update_bits > 0 || mali_statebit_get( & ctx->state.common, MALI_STATE_VS_PRS_UPDATE_PENDING)  )
		{
			u32 j;
			u32 *transform_field = &(ctx->state.api.gles1->transform.texture_transform_field);
		

			j = 0;
			for ( i = 0; i < program_binary->samplers.num_samplers; i++ )
			{
				u32 gl_idx = program_binary->samplers.info[ i ].API_value;
				MALI_DEBUG_ASSERT( gl_idx < ( MIN( MAX_TEXTURE_STAGES, GLES_MAX_TEXTURE_UNITS )), ("gl_idx out of range") );

				if ( _gles_verify_bool(ctx->state.common.texture_env.unit[ gl_idx ].enable_vector, GLES_TEXTURE_TARGET_COUNT, GL_TRUE) &&
					 vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_STAGE_TRANSFORM(gl_idx)) == TEX_TRANSFORM )
				{

					if (  ( tex_mat_update_bits & (1 << gl_idx) ) != 0  ||
						 ctx->sg_ctx->last_texture_transform_field != (*transform_field) )

					{
						float (*tex_mat)[4] = state_gles1->transform.texture_matrix[gl_idx]
							[state_gles1->transform.texture_matrix_stack_depth[gl_idx]-1];
#if (!MALI_OSU_MATH)
						MALI_GLES_SG_WRITE_ARRAY_VEC4(TextureMatrices, gl_idx*4+0, tex_mat[0]);
						MALI_GLES_SG_WRITE_ARRAY_VEC4(TextureMatrices, gl_idx*4+1, tex_mat[1]);
						MALI_GLES_SG_WRITE_ARRAY_VEC4(TextureMatrices, gl_idx*4+2, tex_mat[2]);
						MALI_GLES_SG_WRITE_ARRAY_VEC4(TextureMatrices, gl_idx*4+3, tex_mat[3]);
#else
						MALI_GLES_SG_WRITE_MAT4_WITH_OFFSET( TextureMatrices, gl_idx*4, tex_mat );
#endif
						mali_statebit_unset( & ctx->state.common, MALI_STATE_TEXTURE_MATRIX_UPDATE_PENDING0 + gl_idx );
					}
					j = MAX(j, gl_idx+1);
				}
			}
			ctx->sg_ctx->last_texture_transform_field = *transform_field;
			MALI_GLES_SG_WRITE_FLOAT(NumTextureMatrices, (float)j);
			mali_statebit_set( & ctx->state.common, MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING);
 		}
	}
}


int _gles_sg_get_vertex_uniform_array_size(const struct vertex_shadergen_state* sgstate)
{
	int i;
	mali_bool bTexgen = MALI_FALSE;

	if (vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING))
	{
		return VERTEX_UNIFORM_GROUP_POS_zkinning + VERTEX_UNIFORM_GROUP_SIZE_zkinning;
	}

	for (i = 0 ; i < MAX_TEXTURE_STAGES ; i++)
	{
		if (vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_STAGE_TRANSFORM(i)) == TEX_TRANSFORM)
		{
			return VERTEX_UNIFORM_GROUP_POS_textrans + VERTEX_UNIFORM_GROUP_SIZE_textrans;
		}
	}

	for (i = 0 ; i < MAX_TEXTURE_STAGES; i++)
	{
		if (vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_STAGE_ENABLE(i)))
		{
			bTexgen = MALI_TRUE;
			break;
		}
	}

	if (vertex_shadergen_decode(sgstate, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS) > NUMLIGHTS_ZERO || bTexgen)
	{
		return VERTEX_UNIFORM_GROUP_POS_lighting + VERTEX_UNIFORM_GROUP_SIZE_lighting;
	}

	return VERTEX_UNIFORM_GROUP_POS_basic + VERTEX_UNIFORM_GROUP_SIZE_basic;
}

