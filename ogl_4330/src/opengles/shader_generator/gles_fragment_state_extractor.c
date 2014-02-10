/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <shared/shadergen_interface.h>
#include "gles_shader_generator_context.h"
#include "../gles_context.h"
#include "../gles1_state/gles1_state.h"
#include "shadergen_state.h"
#include "gles_fb_interface.h"


/**
 * Returns enabled modes RGB and ALPHA for the current texture object
 * @param target_per_unit  Texture target
 * @param unit A pointer to the current texture unit
 * @param gles1_unit A pointer to the gles 1.1 specific texture unit state
 * @return  rgb_alpha  2 elements array containing RGB, ALPHA support flags
 */

MALI_STATIC_INLINE void getTextureObjectMode(enum gles_texture_target target_per_unit, const gles_texture_unit  *unit, const gles1_texture_unit *gles1_unit, mali_bool* rgb_alpha  )
{
	if(GL_COMBINE == gles1_unit->env_mode)
	{
		/* combiner modes are never reduced to a passthrough */
		rgb_alpha[0] = MALI_TRUE;
		rgb_alpha[1] = MALI_TRUE;
	} else {
		GLenum format = unit->current_texture_object[target_per_unit]->mipchains[0]->levels[0]->format;
		/* builtin combiner modes are reduced to a passthrough if texture channel is missing */
		switch (format)
		{
			case GL_ALPHA:
				rgb_alpha[0] = MALI_FALSE;
				rgb_alpha[1] = MALI_TRUE;
				break;
			case GL_LUMINANCE:
			case GL_DEPTH_COMPONENT:
			case GL_DEPTH_STENCIL_OES:
			case GL_RGB:
			case GL_ETC1_RGB8_OES:
			case GL_PALETTE4_RGB8_OES:
			case GL_PALETTE4_R5_G6_B5_OES:
			case GL_PALETTE8_RGB8_OES:
			case GL_PALETTE8_R5_G6_B5_OES:
				rgb_alpha[0] = MALI_TRUE;
				rgb_alpha[1] = MALI_FALSE;
				break;
			case GL_LUMINANCE_ALPHA:
			case GL_PALETTE4_RGBA8_OES:
			case GL_PALETTE4_RGBA4_OES:
			case GL_PALETTE4_RGB5_A1_OES:
			case GL_PALETTE8_RGBA8_OES:
			case GL_PALETTE8_RGBA4_OES:
			case GL_PALETTE8_RGB5_A1_OES:
			case GL_RGBA:
			#if EXTENSION_BGRA8888_ENABLE 
			case GL_BGRA_EXT:
			#endif
				rgb_alpha[0] = MALI_TRUE;
				rgb_alpha[1] = MALI_TRUE;
				break;
			default: /* other texture formats should not exist */
				MALI_DEBUG_ASSERT(0, ("This internalformat isn't supported"));
				rgb_alpha[0] = MALI_TRUE;
				rgb_alpha[1] = MALI_TRUE;
				break;
		}
	}
}


void _gles_sg_extract_fragment_state(struct gles_context *ctx, GLenum primitive_type) {
	/* *** texture environment *** */
	int i;
	gles_state *glstate;
	fragment_shadergen_state *sgstate;
	mali_bool points = GL_POINTS == primitive_type;
	texturing_kind kind_per_unit[GLES_MAX_TEXTURE_UNITS];
	enum gles_texture_target target_per_unit[GLES_MAX_TEXTURE_UNITS]; /* may be GLES_TEXTURE_TARGET_INVALID, denoting disabled */
	mali_bool points_enabled[GLES_MAX_TEXTURE_UNITS];
	mali_bool rgb_alpha[2] = {MALI_FALSE, MALI_FALSE};

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->sg_ctx);

	glstate = &ctx->state;
	sgstate = &ctx->sg_ctx->current_fragment_state;
	
	/* Walk through all enabled texture unit IDs. 
	 * Find highest-ranking texturebled existing object on this unit ID, check if it is complete. 
	 * If complete, set 
	 * 		is_usable to true 
	 * 		kind_per_unit[i] to the matching dimensionality
	 * 		points_enabled to enabled or not depending on texunit state.
	 *
	 *  - If YUV, Cube or 3D textures are enabled, they take precedence over 2D textures. 
	 *  - YUV take presedence over cube textures. 
 	 *  - There is no defined order for the remaining combinations. 
	 *  - The code assumes that the lastmost target takes priority. 
	 *  - The compiler shadergen does not support 3D textures yet. 
	 *    To support that, more bits are needed, or we need to remove unused enums. 
	 *  - If primitive type is points, projective w is forbidden, since points should always face the screen. 
	 */

	/* disable all stages by default.*/
	fragment_shadergen_encode(sgstate, FRAGMENT_SHADERGEN_ALL_STAGE_ENABLE_BITS, 0);

	for(i = 0; i < MIN(MAX_TEXTURE_STAGES, GLES_MAX_TEXTURE_UNITS); i++)
	{
		const gles_texture_unit *unit = &ctx->state.common.texture_env.unit[i];
		const gles1_texture_unit *gles1_unit = &glstate->api.gles1->texture_env.unit[i];
		struct gles_texture_object *tex_obj = NULL;

		points_enabled[i] = points &&
				glstate->api.gles1->texture_env.point_sprite_enabled &&
				gles1_unit->coord_replace;
		target_per_unit[i] = GLES_TEXTURE_TARGET_INVALID; /* by default, all units are disabled */
		kind_per_unit[i] = TEXTURE_2D; /* just a dummy default value, keeps coverity happy */
	
		
		tex_obj = _gles1_texture_env_get_dominant_texture_object(unit, &target_per_unit[i]);
		/* if the Texobj isn't complete or don't exist, it's just going to be white. Meaning its behavior will act as if the TU was disabled */
		if(tex_obj == NULL || !_gles_m200_is_texture_usable(tex_obj))
		{
			target_per_unit[i] = GLES_TEXTURE_TARGET_INVALID;
			continue; /* disable the entire TU, move on to the next TU */
		}
		
		/* if it is complete, find the matching Shadergen texture kind  */
		switch(target_per_unit[i])
		{
			case GLES_TEXTURE_TARGET_CUBE:
				kind_per_unit[i] = TEXTURE_CUBE; /* projective cube texcoords make no sense anyway - skip that option */
				break;
			case GLES_TEXTURE_TARGET_2D:
				kind_per_unit[i] = (points_enabled[i] ? TEXTURE_2D: TEXTURE_2D_PROJ_W);
				break;
			case GLES_TEXTURE_TARGET_EXTERNAL:
				if(MALI_TRUE==tex_obj->yuv_info.is_rgb)
				{
					kind_per_unit[i] = (points_enabled[i] ? TEXTURE_EXTERNAL_NO_TRANSFORM: TEXTURE_EXTERNAL_NO_TRANSFORM_PROJ_W);
				}
				else
				{
					kind_per_unit[i] = (points_enabled[i] ? TEXTURE_EXTERNAL: TEXTURE_EXTERNAL_PROJ_W);
				}
				break;
			default: 
				MALI_DEBUG_ASSERT(0, ("Unknown texture target found"));
				target_per_unit[i] = GLES_TEXTURE_TARGET_INVALID; /* disable the unit */
				break;
		}

		if ( target_per_unit[i] != GLES_TEXTURE_TARGET_INVALID)
		{
			getTextureObjectMode(target_per_unit[i], unit, gles1_unit, rgb_alpha);
			fragment_shadergen_encode(sgstate, FRAGMENT_SHADERGEN_STAGE_RGB_ENABLE(i), rgb_alpha[0]);
			fragment_shadergen_encode(sgstate, FRAGMENT_SHADERGEN_STAGE_ALPHA_ENABLE(i), rgb_alpha[1]);
			fragment_shadergen_encode(sgstate, FRAGMENT_SHADERGEN_STAGE_TEXTURE_DIMENSIONALITY(i), kind_per_unit[i]);
			fragment_shadergen_encode(sgstate, FRAGMENT_SHADERGEN_STAGE_TEXTURE_POINT_COORDINATE(i), points_enabled[i]);
		}
	} /* for each unit - set up texture_kind and is_usable */
}


static void copy_vec3(gles_fp16 *dst, const float *src) {
	dst[0] = _gles_fp32_to_fp16_predefined(src[0]);
	dst[1] = _gles_fp32_to_fp16_predefined(src[1]);
	dst[2] = _gles_fp32_to_fp16_predefined(src[2]);
}

static void copy_vec4(gles_fp16 *dst, const float *src) {
	dst[0] = _gles_fp32_to_fp16_predefined(src[0]);
	dst[1] = _gles_fp32_to_fp16_predefined(src[1]);
	dst[2] = _gles_fp32_to_fp16_predefined(src[2]);
	dst[3] = _gles_fp32_to_fp16_predefined(src[3]);
}

#define WRITE_FLOAT(name, value) (uniforms[FRAGMENT_UNIFORM_##name] = _gles_fp32_to_fp16_predefined(value))
#define WRITE_VEC3(name, src) copy_vec3(&uniforms[FRAGMENT_UNIFORM_##name], src)
#define WRITE_VEC4(name, src) copy_vec4(&uniforms[FRAGMENT_UNIFORM_##name], src)

#define WRITE_COMPONENT_FLOAT(name, index, value) (uniforms[FRAGMENT_UNIFORM_##name+(index)] = _gles_fp32_to_fp16_predefined(value))
#define WRITE_ARRAY_VEC4(name, index, src) copy_vec4(&uniforms[FRAGMENT_UNIFORM_##name+(index)*4], src)

void _gles_sg_extract_fragment_uniforms(gles_context *ctx,
                                        const fragment_shadergen_state *sgstate,
                                        gles_fp16 *uniforms,
                                        bs_program *program_binary) {
	u32 i;
	mali_bool is_touched = MALI_FALSE;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(sgstate);
	MALI_DEBUG_ASSERT_POINTER(uniforms);

	/* *** constants *** */
	if ( mali_statebit_get( &ctx->state.common, GLES_STATE_TEXENV_COLOR_DIRTY) && 
		program_binary->fragment_shader_uniforms.cellsize >= FRAGMENT_UNIFORM_Constants)
	{
		for( i = 0; i < MIN( MAX_TEXTURE_STAGES, GLES_MAX_TEXTURE_UNITS ); i++)
		{
			WRITE_ARRAY_VEC4(Constants, i, ctx->state.api.gles1->texture_env.unit[i].env_color);
        }
	 	mali_statebit_unset( &ctx->state.common, GLES_STATE_TEXENV_COLOR_DIRTY);
		is_touched = MALI_TRUE;
	}

	if (mali_statebit_get( &ctx->state.common, GLES_STATE_CURRENT_COLOR_DIRTY))
	{
		for (i = 0 ; i < 4 ; i++)
		{
			WRITE_COMPONENT_FLOAT(ConstantColor, i, CLAMP(ctx->state.api.gles1->current.color[i], 0.0f, 1.0f));
		}
		mali_statebit_unset( &ctx->state.common, GLES_STATE_CURRENT_COLOR_DIRTY);
 		is_touched = MALI_TRUE;
	}

	/* *** clip plane *** */
	if (	fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_CLIP_PLANE_ENABLE(0)))
	{
		/* Tie breaking rule for clip planes to make sure that two opposite
		   half-spaces include every pixel exactly once.
		*/
		float epsilon = _mali_gles_sg_clip_plane_sign(ctx->state.api.gles1->transform.clip_plane[0]);
		WRITE_FLOAT(ClipPlaneTie, epsilon);
 		is_touched = MALI_TRUE;
	}

	/* *** fog *** */
	if (mali_statebit_get( &ctx->state.common, GLES_STATE_FOG_COLOR_DIRTY) &&
		vertex_shadergen_decode( &ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_FEATURE_FOG_DIST ))
	{
		WRITE_VEC3(FogColor, ctx->state.api.gles1->coloring.fog_color);
		mali_statebit_unset( &ctx->state.common, GLES_STATE_FOG_COLOR_DIRTY);
		is_touched = MALI_TRUE;
	}

	/* *** logic_op rounding compensation *** */
#if defined(USING_MALI400) || defined(USING_MALI450)
	if (mali_statebit_get( &ctx->state.common, GLES_STATE_COLOR_ATTACHMENT_DIRTY) && _gles_fb_get_color_logic_op(ctx))
	{
		int rb_bits = _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS );
		MALI_DEBUG_ASSERT(rb_bits==_gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_BLUE_BITS ), ("We have a configuration where red and blue bits are different"));
		if(8!=rb_bits)
		{
			float rb_size = 0.5f / (float) ((1<<rb_bits) - 1);
			float g_size = 0.5f / (float) ((1<<_gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_GREEN_BITS )) - 1);
			float a_size = 0.5f / (float) ((1<<8));
#if defined(__ARMCC__)
			float addtores[4];
            addtores[0] = rb_size;
            addtores[1] = g_size;
            addtores[2] = rb_size;
            addtores[3] = a_size;
#else
			float addtores[] = {rb_size, g_size, rb_size, a_size};
#endif
			WRITE_VEC4(AddConstToRes, addtores);
		}
		else
		{
			float addtores[] = {0.0, 0.0, 0.0, 0.0};
			WRITE_VEC4(AddConstToRes, addtores);
		}
		mali_statebit_unset( &ctx->state.common, GLES_STATE_COLOR_ATTACHMENT_DIRTY);
		is_touched = MALI_TRUE;
	}
#endif
	if( is_touched)
	{
		mali_statebit_set( &ctx->state.common, MALI_STATE_FB_FRAGMENT_UNIFORM_UPDATE_PENDING);	
	}
}


#define USE(var,i) do { int n = FRAGMENT_UNIFORM_##var + (i)*4 + 4; if (n > n_uniforms) n_uniforms = n; } while (0)

static void use_source(const fragment_shadergen_state *sgstate, arg_source src, int *n_ptr, int stage_index)
{
#define n_uniforms (*n_ptr)
	switch (src)
	{
	case ARG_CONSTANT_0:
	case ARG_CONSTANT_1:
	case ARG_CONSTANT_2:
	case ARG_CONSTANT_3:
	case ARG_CONSTANT_4:
	case ARG_CONSTANT_5:
	case ARG_CONSTANT_6:
	case ARG_CONSTANT_7:
		USE(Constants, src-ARG_CONSTANT_0);
		break;
	case ARG_CONSTANT_COLOR:
		USE(ConstantColor,0);
		break;
	case ARG_PREVIOUS_STAGE_RESULT:
		if (stage_index == 0)
		{
			use_source(sgstate, ARG_INPUT_COLOR, n_ptr, stage_index);
		}
		break;
	case ARG_INPUT_COLOR:
		use_source(sgstate, fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE), n_ptr, stage_index);
		break;
	default:
		break;
	}
#undef n_uniforms
}

int _gles_sg_get_fragment_uniform_array_size(const fragment_shadergen_state *sgstate)
{
	int n_uniforms = 0;
	int t;

	switch (fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_FOG_MODE) )
	{
	case FOG_NONE:
		break;
	case FOG_LINEAR:
	case FOG_EXP:
	case FOG_EXP2:
		USE(FogColor,0);
		break;
	}

	if (fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_CLIP_PLANE_ENABLE(0)))
	{
		USE(ClipPlaneTie,0);
	}

#if defined(USING_MALI400) || defined(USING_MALI450)
	if (fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_LOGIC_OPS_ROUNDING_ENABLE))
	{
		USE(AddConstToRes,0);
	}
#endif

	use_source(sgstate, fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_RESULT_COLOR_SOURCE), &n_uniforms, MAX_TEXTURE_STAGES);

	for (t = 0 ; t < MAX_TEXTURE_STAGES ; t++)
	{
		int s;
		mali_bool rgb_enable = fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_STAGE_RGB_ENABLE(t));
		mali_bool alpha_enable = fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_STAGE_ALPHA_ENABLE(t));
		texturing_kind kind = fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_STAGE_TEXTURE_DIMENSIONALITY(t));
		if (rgb_enable)
		{
			for (s = 0 ; s < 3 ; s++)
			{
				use_source(sgstate, fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(t, s)), &n_uniforms, t);
			}
		}
		if (alpha_enable)
		{
			for (s = 0 ; s < 3 ; s++)
			{
				use_source(sgstate, fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(t, s)), &n_uniforms, t);
			}
		}
		if (rgb_enable || alpha_enable)
		{
			if (fragment_shadergen_decode(sgstate, FRAGMENT_SHADERGEN_STAGE_TEXTURE_POINT_COORDINATE(t) ) )
			{
				USE(gl_mali_PointCoordScaleBias,0);
			}
		}
		if (!rgb_enable || !alpha_enable)
		{
			use_source(sgstate, ARG_PREVIOUS_STAGE_RESULT, &n_uniforms, t);
		}

		if(kind == TEXTURE_EXTERNAL || kind == TEXTURE_EXTERNAL_PROJ_W || kind == TEXTURE_EXTERNAL_NO_TRANSFORM || kind == TEXTURE_EXTERNAL_NO_TRANSFORM_PROJ_W)
		{
			USE(YUV_Constants, (4*t)+3);
		}
	}

	return n_uniforms;
}

