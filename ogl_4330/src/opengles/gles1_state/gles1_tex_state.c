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
#include "gles1_tex_state.h"
#include <gles1_texture_object.h>
#include <gles_config_extension.h>
#include <gles_convert.h>
#include <gles_texture_object.h>
#include <gles_state.h>
#include <gles_context.h>
#include "gles1_state.h"
#include <gles_fb_interface.h>
#include <gles_sg_interface.h>

MALI_STATIC void _gles1_push_texture_stage_state(struct gles_context* ctx, int stage);

mali_bool _gles1_texture_env_init( struct gles_context *ctx, struct gles_texture_object *default_texture_object[GLES_TEXTURE_TARGET_COUNT] )
{
	u32 i;
	gles_state* state;

	MALI_DEBUG_ASSERT_POINTER(  ctx );
	state = &ctx->state;
	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( default_texture_object );

	state->api.gles1->texture_env.point_sprite_enabled = 0;
	state->api.gles1->texture_env.tex_gen_enabled = 0;

	state->common.texture_env.active_texture = (u8)0;

	for (i = 0; i < GLES_MAX_TEXTURE_UNITS; i++)
	{
		int j;
		gles_texture_unit  *tex_unit       = &state->common.texture_env.unit[ i ];
		gles1_texture_unit *gles1_tex_unit = &state->api.gles1->texture_env.unit[ i ];

		/* update all bindings */
		for (j = 0; j < GLES_TEXTURE_TARGET_COUNT; ++j)
		{
			struct gles_texture_object *tex_obj = default_texture_object[j];
			tex_unit->enable_vector[j] = (GLboolean)GL_FALSE;
			tex_unit->current_texture_object[j] = tex_obj;
			tex_unit->current_texture_id[j]     = 0;
			_gles_texture_object_addref(tex_obj);
		}

		gles1_tex_unit->coord_replace = (GLboolean)GL_FALSE;
		gles1_tex_unit->env_mode = GL_MODULATE;
		gles1_tex_unit->combine_rgb = GL_MODULATE;
		gles1_tex_unit->combine_alpha = GL_MODULATE;
		gles1_tex_unit->src_rgb[ 0 ] = GL_TEXTURE;
		gles1_tex_unit->src_rgb[ 1 ] = GL_PREVIOUS;
		gles1_tex_unit->src_rgb[ 2 ] = GL_CONSTANT;
		gles1_tex_unit->src_alpha[ 0 ] = GL_TEXTURE;
		gles1_tex_unit->src_alpha[ 1 ] = GL_PREVIOUS;
		gles1_tex_unit->src_alpha[ 2 ] = GL_CONSTANT;
		gles1_tex_unit->operand_rgb[ 0 ] = GL_SRC_COLOR;
		gles1_tex_unit->operand_rgb[ 1 ] = GL_SRC_COLOR;
		gles1_tex_unit->operand_rgb[ 2 ] = GL_SRC_ALPHA;
		gles1_tex_unit->operand_alpha[ 0 ] = GL_SRC_ALPHA;
		gles1_tex_unit->operand_alpha[ 1 ] = GL_SRC_ALPHA;
		gles1_tex_unit->operand_alpha[ 2 ] = GL_SRC_ALPHA;
		gles1_tex_unit->rgb_scale = 1;
		gles1_tex_unit->alpha_scale = 1;
		gles1_tex_unit->env_color[ 0 ] = FLOAT_TO_FTYPE( 0.f );
		gles1_tex_unit->env_color[ 1 ] = FLOAT_TO_FTYPE( 0.f );
		gles1_tex_unit->env_color[ 2 ] = FLOAT_TO_FTYPE( 0.f );
		gles1_tex_unit->env_color[ 3 ] = FLOAT_TO_FTYPE( 0.f );
		gles1_tex_unit->lod_bias = FLOAT_TO_FTYPE( 0.f );

		_gles1_push_texture_stage_state(ctx, i);
	}

	return MALI_TRUE;
}

GLenum _gles1_tex_envv(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *params,
	gles_datatype type )
{
	GLenum  param;
	GLftype param_f;
	GLuint  i;
	gles_texture_environment *texture_env;
	gles1_texture_environment *gles1_texture_env;
	gles1_texture_unit *gles1_tex_unit;

#define VERIFY_ENUM( a, p, r) if ( MALI_FALSE == _gles_verify_enum( a, (u32)MALI_ARRAY_SIZE( a ), p )) return r;

	static const GLenum valid_enums_env_mode[] =
	{
		GL_MODULATE, GL_BLEND, GL_DECAL, GL_REPLACE, GL_ADD, GL_COMBINE,
	};

	/* NOTE: DOT3 is an ARB-extension, but it's a valid entry for texenv to have without specifying extension in the gl-spec pp 22 in the OpenGL ES 1.1 spec */
	static const GLenum valid_enums_rgb_combine[] =
	{
		GL_DOT3_RGBA, GL_DOT3_RGB, GL_REPLACE, GL_MODULATE, GL_ADD, GL_ADD_SIGNED, GL_INTERPOLATE, GL_SUBTRACT,
	};

	static const GLenum valid_enums_alpha_combine[] =
	{
		GL_REPLACE, GL_MODULATE, GL_ADD, GL_ADD_SIGNED, GL_INTERPOLATE, GL_SUBTRACT,
	};

	static const GLenum valid_enums_rgb_alpha_source[] =
	{
		GL_TEXTURE, GL_CONSTANT, GL_PRIMARY_COLOR, GL_PREVIOUS,
	};

	static const GLenum valid_enums_rgb_operand[] =
	{
		GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
	};

	static const GLenum valid_enums_alpha_operand[] =
	{
		GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
	};

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );
	texture_env = &ctx->state.common.texture_env;
	gles1_texture_env = &ctx->state.api.gles1->texture_env;
	MALI_DEBUG_ASSERT(texture_env->active_texture < GLES_MAX_TEXTURE_UNITS, ("active_texture out of range") );

	gles1_tex_unit = & gles1_texture_env->unit[ texture_env->active_texture ];

	param = _gles_convert_to_enum( params, type );

	switch( target )
	{
		case GL_TEXTURE_ENV:
			switch( pname )
			{
				case GL_TEXTURE_ENV_MODE:
					VERIFY_ENUM( valid_enums_env_mode, param, GL_INVALID_ENUM );
					if ( param != gles1_tex_unit->env_mode )
					{
						gles1_tex_unit->env_mode = param;
						_gles1_push_texture_stage_state(ctx, texture_env->active_texture);
					}
				break;

				case GL_TEXTURE_ENV_COLOR:
					/* small fix to be able to use util_convert_array_to_ftype() */
					if (GLES_INT == type) type = GLES_NORMALIZED_INT;
					for (i = 0; i < 4; i++)
					{
						param_f = _gles_convert_element_to_ftype( params, i, type );
						gles1_tex_unit->env_color[ i ] = CLAMP( param_f, FLOAT_TO_FTYPE( 0.f ), FLOAT_TO_FTYPE( 1.f ) );
					}
					mali_statebit_set( &ctx->state.common, GLES_STATE_TEXENV_COLOR_DIRTY);
				break;

				case GL_COMBINE_RGB:
					VERIFY_ENUM( valid_enums_rgb_combine, param, GL_INVALID_ENUM );
					if(gles1_tex_unit->combine_rgb != param)
					{
						gles1_tex_unit->combine_rgb = param;
						_gles1_push_texture_stage_state(ctx, texture_env->active_texture);
					}
				break;


				case GL_COMBINE_ALPHA:
					VERIFY_ENUM( valid_enums_alpha_combine, param, GL_INVALID_ENUM );
					if(gles1_tex_unit->combine_alpha != param)
					{
						gles1_tex_unit->combine_alpha = param;
						_gles1_push_texture_stage_state(ctx, texture_env->active_texture);
					}
				break;

				case GL_SRC0_RGB:
				case GL_SRC1_RGB:
				case GL_SRC2_RGB:
					VERIFY_ENUM( valid_enums_rgb_alpha_source, param, GL_INVALID_ENUM );
					if(gles1_tex_unit->src_rgb[ pname - GL_SRC0_RGB ] != param)
					{
						gles1_tex_unit->src_rgb[ pname - GL_SRC0_RGB ] = param;
						_gles1_push_texture_stage_state(ctx, texture_env->active_texture);
					}
				break;


				case GL_SRC0_ALPHA:
				case GL_SRC1_ALPHA:
				case GL_SRC2_ALPHA:
					VERIFY_ENUM( valid_enums_rgb_alpha_source, param, GL_INVALID_ENUM );
					if(gles1_tex_unit->src_alpha[ pname - GL_SRC0_ALPHA ] != param)
					{
						gles1_tex_unit->src_alpha[ pname - GL_SRC0_ALPHA ] = param;
						_gles1_push_texture_stage_state(ctx, texture_env->active_texture);
					}
				break;

				case GL_OPERAND0_RGB:
				case GL_OPERAND1_RGB:
				case GL_OPERAND2_RGB:
					VERIFY_ENUM( valid_enums_rgb_operand, param, GL_INVALID_ENUM );
					if(gles1_tex_unit->operand_rgb[ pname - GL_OPERAND0_RGB ] != param)
					{
						gles1_tex_unit->operand_rgb[ pname - GL_OPERAND0_RGB ] = param;
						_gles1_push_texture_stage_state(ctx, texture_env->active_texture);
					}
				break;

				case GL_OPERAND0_ALPHA:
				case GL_OPERAND1_ALPHA:
				case GL_OPERAND2_ALPHA:
					VERIFY_ENUM( valid_enums_alpha_operand, param, GL_INVALID_ENUM );
					if( gles1_tex_unit->operand_alpha[ pname - GL_OPERAND0_ALPHA ] != param)
					{
						gles1_tex_unit->operand_alpha[ pname - GL_OPERAND0_ALPHA ] = param;
						_gles1_push_texture_stage_state(ctx, texture_env->active_texture);
					}
				break;

				case GL_RGB_SCALE:
				{
					GLftype scale_ftype = _gles_convert_element_to_ftype( params, 0, type );
					if (( scale_ftype != FLOAT_TO_FTYPE(1.0) ) && ( scale_ftype != FLOAT_TO_FTYPE(2.0) ) && ( scale_ftype != FLOAT_TO_FTYPE(4.0) )) MALI_ERROR( GL_INVALID_VALUE );
					gles1_tex_unit->rgb_scale = ( u32 ) FTYPE_TO_INT( scale_ftype );
					if(gles1_tex_unit->env_mode == GL_COMBINE)
					{
						_gles1_push_texture_stage_state(ctx, texture_env->active_texture);
					}
				}
				break;

				case GL_ALPHA_SCALE:
				{
					GLftype scale_ftype = _gles_convert_element_to_ftype( params, 0, type );
					if (( scale_ftype != FLOAT_TO_FTYPE(1.0) ) && ( scale_ftype != FLOAT_TO_FTYPE(2.0) ) && ( scale_ftype != FLOAT_TO_FTYPE(4.0) )) MALI_ERROR( GL_INVALID_VALUE );
					gles1_tex_unit->alpha_scale = ( u32 ) FTYPE_TO_INT( scale_ftype );
					if(gles1_tex_unit->env_mode == GL_COMBINE)
					{
						_gles1_push_texture_stage_state(ctx, texture_env->active_texture);
					}
				}
				break;


				default: MALI_ERROR( GL_INVALID_ENUM );
			}
		break;

		/* extension: OES_point_sprite */
		case GL_POINT_SPRITE_OES:
			if (pname != GL_COORD_REPLACE_OES) MALI_ERROR( GL_INVALID_ENUM );
			if (( param != GL_TRUE ) && ( param != GL_FALSE )) MALI_ERROR( GL_INVALID_ENUM );
			if ( param != gles1_tex_unit->coord_replace )
			{
				gles1_tex_unit->coord_replace = param;
			}
		break;

#if EXTENSION_TEXTURE_LOD_BIAS_EXT_ENABLE
		/* extension: EXT_texture_lod_bias */
		case GL_TEXTURE_FILTER_CONTROL_EXT:
			if (pname != GL_TEXTURE_LOD_BIAS_EXT) MALI_ERROR( GL_INVALID_ENUM );
			param_f = _gles_convert_element_to_ftype( params, 0, type );
			gles1_tex_unit->lod_bias = CLAMP( param_f, -FIXED_TO_FTYPE( GLES1_MAX_TEXTURE_LOD_BIAS ), FIXED_TO_FTYPE( GLES1_MAX_TEXTURE_LOD_BIAS ) );
		break;
#endif


		default: MALI_ERROR( GL_INVALID_ENUM );
	}

	GL_SUCCESS;

#undef VERIFY_ENUM
}


GLenum _gles1_tex_env(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *param,
	gles_datatype type )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	/* this is the only pname that's illegal for this function, without being handled in _gles_tex_envv */
	if (GL_TEXTURE_ENV_COLOR == pname) MALI_ERROR( GL_INVALID_ENUM );
	if (NULL == param) GL_SUCCESS;
	return _gles1_tex_envv( ctx, target, pname, param, type );
}

GLenum _gles1_tex_parameter( gles_texture_environment *texture_env, GLenum target, GLenum pname, const GLvoid *params, gles_datatype type )
{
	/* some pnames require a glTexParameterv call. */
	#if EXTENSION_DRAW_TEX_OES_ENABLE
	if(pname == GL_TEXTURE_CROP_RECT_OES) return GL_INVALID_ENUM;
	#endif
	
	return _gles1_tex_parameter_v(texture_env, target, pname, params, type);
}

GLenum _gles1_tex_parameter_v( gles_texture_environment *texture_env, GLenum target, GLenum pname, const GLvoid *params, gles_datatype type )
{
	gles_texture_object *tex_obj;
	gles_texture_unit   *tex_unit;
	GLenum param;

	/* the 2 first are standard, the rest are supported through different extensions */
	static const GLenum valid_enum_wrap_modes[] =
	{
		GL_CLAMP_TO_EDGE, GL_REPEAT,
#if EXTENSION_TEXTURE_BORDER_CLAMP_ARB_ENABLE
		GL_CLAMP_TO_BORDER_ARB,
#endif
#if EXTENSION_TEXTURE_MIRRORED_REPEAT_OES_ENABLE
		GL_MIRRORED_REPEAT_ARB,
#endif
#if EXTENSION_TEXTURE_MIRROR_CLAMP_EXT_ENABLE
		GL_MIRROR_CLAMP_EXT,
		GL_MIRROR_CLAMP_TO_EDGE_EXT,
		GL_MIRROR_CLAMP_TO_BORDER_EXT,
#endif
	};

	static const GLenum valid_enum_min_filters[] =
	{
		GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR, GL_NEAREST_MIPMAP_LINEAR,
	};

	static const GLenum valid_enum_mag_filters[] = { GL_NEAREST, GL_LINEAR };
	enum gles_texture_target dimensionality;

	MALI_DEBUG_ASSERT_POINTER( texture_env );
	MALI_DEBUG_ASSERT_POINTER( params ); /* This pointer does not come from user but from us */
	MALI_DEBUG_ASSERT(texture_env->active_texture < GLES_MAX_TEXTURE_UNITS, ("active_texture out of range") );
	
	if (NULL == params) GL_SUCCESS;

    /* check target */
	dimensionality = _gles_get_dimensionality(target, GLES_API_VERSION_1);
	if ((int)dimensionality == GLES_TEXTURE_TARGET_INVALID) return GL_INVALID_ENUM;

	tex_unit = &texture_env->unit[ texture_env->active_texture ];

	MALI_DEBUG_ASSERT_RANGE((int)dimensionality, 0, GLES_TEXTURE_TARGET_COUNT - 1);
	tex_obj =  tex_unit->current_texture_object[dimensionality];
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	/* for the single parameter pnames */
	param = _gles_convert_to_enum( params, type );
	switch( pname )
	{
		/* single params */

		case GL_TEXTURE_MIN_FILTER:
			if ( MALI_FALSE == _gles_verify_enum( valid_enum_min_filters, (u32)MALI_ARRAY_SIZE( valid_enum_min_filters ), param )) MALI_ERROR( GL_INVALID_ENUM );

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
			/* external images accept only GL_NEAREST and GL_LINEAR as min filters */
			if ( GL_TEXTURE_EXTERNAL_OES == target )
			{
				if ( ( tex_obj->min_filter != GL_NEAREST && tex_obj->min_filter != GL_LINEAR ) )
				{
					return GL_INVALID_ENUM;
				}
			}
#endif

			tex_obj->min_filter = param;

			_gles_m200_td_min_filter( tex_obj );

			/* The min_filter has changed, we now know that this tex_obj has to be checked for completeness again */
			tex_obj->completeness_check_dirty = MALI_TRUE;
			break;

		case GL_TEXTURE_MAG_FILTER:
			if ( MALI_FALSE == _gles_verify_enum( valid_enum_mag_filters, (u32)MALI_ARRAY_SIZE( valid_enum_mag_filters ), param )) MALI_ERROR( GL_INVALID_ENUM );
			tex_obj->mag_filter = param;

			_gles_m200_td_mag_filter( tex_obj );

			break;

		case GL_TEXTURE_WRAP_S:
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
			/* external images accept only GL_CLAMP_TO_EDGE as wrap_s */
			if ( GL_TEXTURE_EXTERNAL_OES == target )
			{
				if ( tex_obj->wrap_s != GL_CLAMP_TO_EDGE )
				{
					return GL_INVALID_ENUM;
				}
			}
#endif
			if ( MALI_FALSE == _gles_verify_enum( valid_enum_wrap_modes, (u32)MALI_ARRAY_SIZE( valid_enum_wrap_modes ), param )) MALI_ERROR( GL_INVALID_ENUM );
			tex_obj->wrap_s = param;
			_gles_m200_td_set_wrap_mode_s( tex_obj );
			break;

		case GL_TEXTURE_WRAP_T:
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
			/* external images accept only GL_CLAMP_TO_EDGE as wrap_s */
			if ( GL_TEXTURE_EXTERNAL_OES == target )
			{
				if ( tex_obj->wrap_s != GL_CLAMP_TO_EDGE )
				{
					return GL_INVALID_ENUM;
				}
			}
#endif
			if ( MALI_FALSE == _gles_verify_enum( valid_enum_wrap_modes, (u32)MALI_ARRAY_SIZE( valid_enum_wrap_modes ), param )) MALI_ERROR( GL_INVALID_ENUM );
			tex_obj->wrap_t = param;
			_gles_m200_td_set_wrap_mode_t( tex_obj );
			break;

		case GL_GENERATE_MIPMAP:
			if (( param != GL_TRUE ) && ( param != GL_FALSE )) MALI_ERROR( GL_INVALID_ENUM );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
			if ( GL_TEXTURE_EXTERNAL_OES == target ) MALI_ERROR( GL_INVALID_ENUM );
#endif
			tex_obj->generate_mipmaps = param;
			break;

#if EXTENSION_DRAW_TEX_OES_ENABLE
		case GL_TEXTURE_CROP_RECT_OES:
			tex_obj->crop_rect[ 0 ] = FTYPE_TO_INT( _gles_convert_element_to_ftype( params, 0, type ) );
			tex_obj->crop_rect[ 1 ] = FTYPE_TO_INT( _gles_convert_element_to_ftype( params, 1, type ) );
			tex_obj->crop_rect[ 2 ] = FTYPE_TO_INT( _gles_convert_element_to_ftype( params, 2, type ) );
			tex_obj->crop_rect[ 3 ] = FTYPE_TO_INT( _gles_convert_element_to_ftype( params, 3, type ) );
			break;
#endif
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	tex_obj->dirty = MALI_TRUE;
	GL_SUCCESS;
}

GLenum _gles1_active_texture( struct gles_context *ctx, GLenum texture )
{
	GLenum err = GL_NO_ERROR;
	gles_texture_environment *texture_env = &ctx->state.common.texture_env;
	gles1_transform *transform = &ctx->state.api.gles1->transform;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	err = _gles_active_texture( ctx, texture );
	MALI_CHECK(GL_NO_ERROR == err, err);


	if(transform->matrix_mode == GL_TEXTURE)
	{
		transform->current_matrix = &transform->texture_matrix[texture_env->active_texture][transform->texture_matrix_stack_depth[texture_env->active_texture] - 1];
		transform->current_matrix_is_identity = &transform->texture_matrix_identity[texture_env->active_texture][transform->texture_matrix_stack_depth[texture_env->active_texture] - 1];
		transform->current_texture_matrix_id = texture_env->active_texture;
	}

	GL_SUCCESS;
}

MALI_STATIC scale_kind scale_from_int(GLuint gl_scale)
{
	switch (gl_scale)
	{
		case 1:
			return SCALE_ONE;
		case 2:
			return SCALE_TWO;
		case 4:
			return SCALE_FOUR;
		default:
			MALI_DEBUG_ASSERT( 0, ("Illegal scale kind: 0x%x\n", gl_scale) );
			return SCALE_ONE;
	}
}

MALI_STATIC texture_combiner combiner_from_enum(GLenum gl_combiner)
{
	switch (gl_combiner)
	{
		case GL_REPLACE: return COMBINE_REPLACE;
		case GL_MODULATE: return COMBINE_MODULATE;
		case GL_ADD: return COMBINE_ADD;
		case GL_ADD_SIGNED: return COMBINE_ADD_SIGNED;
		case GL_INTERPOLATE: return COMBINE_INTERPOLATE;
		case GL_SUBTRACT: return COMBINE_SUBTRACT;
		case GL_DOT3_RGB: return COMBINE_DOT3_RGB;
		case GL_DOT3_RGBA: return COMBINE_DOT3_RGBA;
		default: MALI_DEBUG_ASSERT( 0, ("Illegal combiner: 0x%x\n", gl_combiner) ); return COMBINE_REPLACE;
	}
}

MALI_STATIC arg_operand operand_from_enum(GLenum gl_op)
{
	switch (gl_op)
	{
		case GL_SRC_COLOR: return OPERAND_SRC_COLOR;
		case GL_ONE_MINUS_SRC_COLOR: return OPERAND_ONE_MINUS_SRC_COLOR;
		case GL_SRC_ALPHA: return OPERAND_SRC_ALPHA;
		case GL_ONE_MINUS_SRC_ALPHA: return OPERAND_ONE_MINUS_SRC_ALPHA;
		default: MALI_DEBUG_ASSERT( 0, ("Illegal operand: 0x%x\n", gl_op) ); return OPERAND_SRC_COLOR;
	}
}

MALI_STATIC arg_source source_from_enum(GLenum gl_src, int stage)
{
	switch (gl_src)
	{
		case GL_TEXTURE:
			return MALI_STATIC_CAST(arg_source)(ARG_TEXTURE_0+stage);
		case GL_CONSTANT:
			return MALI_STATIC_CAST(arg_source)(ARG_CONSTANT_0+stage);
		case GL_PRIMARY_COLOR:
			return ARG_INPUT_COLOR;
		case GL_PREVIOUS:
			return ARG_PREVIOUS_STAGE_RESULT;
		default:
			MALI_DEBUG_ASSERT( 0, ("Illegal source: 0x%x\n", gl_src) );
		return ARG_PREVIOUS_STAGE_RESULT;
	}
}


/**
 * Pushes the texture stage state to the fragment shader extractor.
 * @param ctx A pointer to the GLES context
 * @param stage The stage that was just updated
 */
MALI_STATIC void _gles1_push_texture_stage_state(struct gles_context* ctx, int stage)
{
	gles1_texture_unit *gles1_unit;
	arg_source tex_input;
	int s;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_RANGE(stage, 0, MAX_TEXTURE_STAGES-1);

	gles1_unit = &ctx->state.api.gles1->texture_env.unit[stage];
	tex_input = MALI_STATIC_CAST(arg_source)(ARG_TEXTURE_0+stage);

	/* reset the stage data */
	fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_RGB_BITS(stage), 0);
	fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_ALPHA_BITS(stage), 0);

	switch(gles1_unit->env_mode)
	{
		case GL_REPLACE:
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_RGB_BITS(stage),
					MAKE_STAGE_DATA(tex_input, ARG_DISABLE, ARG_DISABLE, OPERAND_SRC_COLOR, OPERAND_SRC_COLOR, OPERAND_SRC_COLOR, COMBINE_REPLACE, SCALE_ONE)
				);
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_ALPHA_BITS(stage),
					MAKE_STAGE_DATA(tex_input, ARG_DISABLE, ARG_DISABLE, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, COMBINE_REPLACE, SCALE_ONE)
				);

			break;
		case GL_MODULATE:
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_RGB_BITS(stage),
					MAKE_STAGE_DATA(ARG_PREVIOUS_STAGE_RESULT, tex_input, ARG_DISABLE, OPERAND_SRC_COLOR, OPERAND_SRC_COLOR, OPERAND_SRC_COLOR, COMBINE_MODULATE, SCALE_ONE)
				);
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_ALPHA_BITS(stage),
					MAKE_STAGE_DATA(ARG_PREVIOUS_STAGE_RESULT, tex_input, ARG_DISABLE, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, COMBINE_MODULATE, SCALE_ONE)
				);
			break;
		case GL_DECAL:
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_RGB_BITS(stage),
					MAKE_STAGE_DATA(tex_input, ARG_PREVIOUS_STAGE_RESULT, tex_input, OPERAND_SRC_COLOR, OPERAND_SRC_COLOR, OPERAND_SRC_ALPHA, COMBINE_INTERPOLATE, SCALE_ONE)
				);
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_ALPHA_BITS(stage),
					MAKE_STAGE_DATA(ARG_PREVIOUS_STAGE_RESULT, ARG_DISABLE, ARG_DISABLE, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, COMBINE_REPLACE, SCALE_ONE)
				);
			break;
		case GL_BLEND:
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_RGB_BITS(stage),
					MAKE_STAGE_DATA(ARG_PREVIOUS_STAGE_RESULT, MALI_STATIC_CAST(arg_source)(ARG_CONSTANT_0+stage), tex_input, OPERAND_SRC_COLOR, OPERAND_SRC_COLOR, OPERAND_ONE_MINUS_SRC_COLOR, COMBINE_INTERPOLATE, SCALE_ONE)
				);
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_ALPHA_BITS(stage),
					MAKE_STAGE_DATA(ARG_PREVIOUS_STAGE_RESULT, tex_input, ARG_DISABLE, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, COMBINE_MODULATE, SCALE_ONE)
				);
			break;
		case GL_ADD:
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_RGB_BITS(stage),
					MAKE_STAGE_DATA(ARG_PREVIOUS_STAGE_RESULT, tex_input, ARG_DISABLE, OPERAND_SRC_COLOR, OPERAND_SRC_COLOR, OPERAND_SRC_COLOR, COMBINE_ADD, SCALE_ONE)
				);
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_ALL_STAGE_ALPHA_BITS(stage),
					MAKE_STAGE_DATA(ARG_PREVIOUS_STAGE_RESULT, tex_input, ARG_DISABLE, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, OPERAND_SRC_ALPHA, COMBINE_MODULATE, SCALE_ONE)
				);
			break;
		case GL_COMBINE:
			for (s = 0 ; s < 3 ; s++)
			{
				arg_source rgb_source = source_from_enum(gles1_unit->src_rgb[s], stage);
				arg_source alpha_source = source_from_enum(gles1_unit->src_alpha[s], stage);

				fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stage, s), rgb_source);
				fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stage, s), alpha_source);
				fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_STAGE_RGB_OPERAND(stage, s), operand_from_enum(gles1_unit->operand_rgb[s]));
				fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_STAGE_ALPHA_OPERAND(stage, s), operand_from_enum(gles1_unit->operand_alpha[s]));
			}
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_STAGE_RGB_COMBINER(stage), combiner_from_enum(gles1_unit->combine_rgb) );
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_STAGE_ALPHA_COMBINER(stage), combiner_from_enum(gles1_unit->combine_alpha) );
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_STAGE_RGB_SCALE(stage), scale_from_int(gles1_unit->rgb_scale) );
			fragment_shadergen_encode(&ctx->sg_ctx->current_fragment_state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SCALE(stage), scale_from_int(gles1_unit->alpha_scale) );
			break;
	}


}

