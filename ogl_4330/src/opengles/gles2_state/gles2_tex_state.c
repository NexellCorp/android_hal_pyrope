/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_debug.h>

#include "gles2_tex_state.h"
#include <gles_common_state/gles_tex_state.h>
#include <gles2_texture_object.h>
#include <gles_texture_object.h>
#include <gles_util.h>
#include <gles_convert.h>
#include <gles_fb_interface.h>

void _gles2_texture_env_init(
	gles_texture_environment *texture_env,
	struct gles_texture_object *default_texture_object[GLES_TEXTURE_TARGET_COUNT]
	)
{
	u32 i;
	MALI_DEBUG_ASSERT_POINTER( texture_env );
	MALI_DEBUG_ASSERT_POINTER( default_texture_object );

	texture_env->active_texture = 0;

	for (i = 0; i < GLES_MAX_TEXTURE_UNITS; i++)
	{
		int j;
		gles_texture_unit *tex_unit = &texture_env->unit[ i ];

		/* update all bindings */
		for (j = 0; j < GLES_TEXTURE_TARGET_COUNT; ++j)
		{
			struct gles_texture_object *tex_obj = default_texture_object[j];

			/* OpenGL ES 2.0 texture units are always enabled */
			tex_unit->enable_vector[j] = (GLboolean)GL_TRUE;

			tex_unit->current_texture_object[j] = tex_obj;
			tex_unit->current_texture_id[j]     = 0;
			_gles_texture_object_addref(tex_obj);
		}
	}
}

GLenum _gles2_tex_parameter( gles_texture_environment *texture_env, GLenum target,
                             GLenum pname, const GLvoid * params, gles_datatype type)
{
	gles_texture_object *tex_obj;
	gles_texture_unit   *tex_unit;
	enum gles_texture_target dimensionality;
	GLenum param;

	/* the 2 first are standard, the rest are supported through different extensions */
	const GLenum valid_enum_wrap_modes[] =
		{
			GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT
		};

	const GLenum valid_enum_min_filters[] =
		{
			GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR,         GL_NEAREST_MIPMAP_LINEAR,
		};

	const GLenum valid_enum_mag_filters[] = { GL_NEAREST, GL_LINEAR };

	MALI_DEBUG_ASSERT_POINTER( texture_env );

	param = _gles_convert_to_enum( params, type );

	/* check target */
	dimensionality = _gles_get_dimensionality(target, GLES_API_VERSION_2);
	if ((int)dimensionality == GLES_TEXTURE_TARGET_INVALID) return GL_INVALID_ENUM;

	tex_unit = &texture_env->unit[ texture_env->active_texture ];

	MALI_DEBUG_ASSERT_RANGE((int)dimensionality, 0, GLES_TEXTURE_TARGET_COUNT - 1);
	tex_obj =  tex_unit->current_texture_object[dimensionality];
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	switch( pname )
	{
		/* first the only multi-parameter tex param */
	case GL_TEXTURE_MIN_FILTER:
		if (tex_obj->min_filter == param) return GL_NO_ERROR;

		if ( _gles_verify_enum( valid_enum_min_filters, (u32)MALI_ARRAY_SIZE( valid_enum_min_filters ), param )  == 0 ) return GL_INVALID_ENUM;

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
		if (tex_obj->mag_filter == param) return GL_NO_ERROR;
		if ( _gles_verify_enum( valid_enum_mag_filters, (u32)MALI_ARRAY_SIZE( valid_enum_mag_filters ), param )  == 0 ) return GL_INVALID_ENUM;
		tex_obj->mag_filter = param;
		_gles_m200_td_mag_filter( tex_obj );
		break;

	case GL_TEXTURE_WRAP_S:
		if (tex_obj->wrap_s == param) return GL_NO_ERROR;

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

		if ( _gles_verify_enum( valid_enum_wrap_modes, (u32)MALI_ARRAY_SIZE( valid_enum_wrap_modes ), param ) == 0 ) return GL_INVALID_ENUM;
		tex_obj->wrap_s = param;
		_gles_m200_td_set_wrap_mode_s( tex_obj );
		break;

	case GL_TEXTURE_WRAP_T:
		if (tex_obj->wrap_t == param) return GL_NO_ERROR;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		/* external images accept only GL_CLAMP_TO_EDGE as wrap_t */
		if ( GL_TEXTURE_EXTERNAL_OES == target )
		{
			if ( tex_obj->wrap_t != GL_CLAMP_TO_EDGE )
			{
				return GL_INVALID_ENUM;
			}
		}
#endif

		if ( _gles_verify_enum( valid_enum_wrap_modes, (u32)MALI_ARRAY_SIZE( valid_enum_wrap_modes ), param ) == 0 ) return GL_INVALID_ENUM;
		tex_obj->wrap_t = param;
		_gles_m200_td_set_wrap_mode_t( tex_obj );
		break;

#if EXTENSION_TEXTURE_3D_OES_ENABLE
	case GL_TEXTURE_WRAP_R_OES:
		if (target != GL_TEXTURE_3D_OES) return GL_INVALID_ENUM; /* only valid on 3D textures */
		if ( _gles_verify_enum( valid_enum_wrap_modes, (u32)MALI_ARRAY_SIZE( valid_enum_wrap_modes ), param ) == 0 ) return GL_INVALID_ENUM;
		tex_obj->wrap_r = param;
		_gles_m200_td_set_wrap_mode_r( tex_obj );
		break;
#endif

#if EXTENSION_VIDEO_CONTROLS_ARM_ENABLE
	case GL_TEXTURE_BRIGHTNESS_ARM:
		if (target != GL_TEXTURE_EXTERNAL_OES) return GL_INVALID_ENUM; /* only valid for external images */
		tex_obj->yuv_info.brightness = ((float *)params)[0];
		break;

	case GL_TEXTURE_CONTRAST_ARM:
		if (target != GL_TEXTURE_EXTERNAL_OES) return GL_INVALID_ENUM; /* only valid for external images */
		tex_obj->yuv_info.contrast = ((float *)params)[0];
		break;

	case GL_TEXTURE_SATURATION_ARM:
		if (target != GL_TEXTURE_EXTERNAL_OES) return GL_INVALID_ENUM; /* only valid for external images */
		tex_obj->yuv_info.saturation = ((float *)params)[0];
		break;
#endif /* EXTENSION_OES_EGL_IMAGE_EXTERNAL */

	default:
		return GL_INVALID_ENUM;
	}

	tex_obj->dirty = MALI_TRUE;
	return GL_NO_ERROR;

}

