/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles1_texture_object.h"

#include "gles_context.h"
#include "gles_texture_object.h"
#include "gles_mipmap.h"
#include "gles_convert.h"
#include "gles_share_lists.h"
#include "gles_object.h"
#include "gles_common_state/gles_tex_state.h"
#include "gles1_state/gles1_state.h"
#include "gles_vtable.h"
#include <opengles/gles_sg_interface.h>

/**
 * Check for mipmap type and format errors.
 *
 * @param tex_obj The texture object
 * @param level The mipmap level
 * @param format The format of the texture.
 * @param type The type of texture.
 *
 * @return GL_NO_ERROR if there is no error, the appropriate error code otherwise.
 */
MALI_STATIC GLenum _gles_check_mipmap_type_error(gles_texture_object* tex_obj,
                                                 GLint level,
                                                 GLenum format,
                                                 GLenum type)
{
	GLenum value = GL_NO_ERROR;

	if ( tex_obj->generate_mipmaps && level == 0 )
	{
		/* we don't support creating mipmaps for these formats. */
		if ( GL_UNSIGNED_SHORT == type &&
		     ( GL_RGBA == format || GL_LUMINANCE_ALPHA == format))
		{
			value = GL_INVALID_OPERATION;
		}
	}
	return value;
}


GLenum _gles1_tex_image_2d(
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLint internalformat,
	GLsizei width,
	GLsizei height,
	GLint border,
	GLenum format,
	GLenum type,
	const GLvoid *pixels,
	GLint unpack_alignment )
{
	GLenum err;

	gles_texture_object *tex_obj = NULL;
	int pitch;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	err = _gles_get_active_bound_texture_object(target, &ctx->state.common.texture_env, &tex_obj);
	if (err != GL_NO_ERROR) return err;

	/* Bail out early if we have a mipmap error */
	err = _gles_check_mipmap_type_error(tex_obj, level, format, type);
	if (GL_NO_ERROR != err) return err;

	/* calculate pitch from unpack alignment */
	pitch = _gles_unpack_alignment_to_pitch(unpack_alignment, width, format, type);

	err = _gles_tex_image_2d(tex_obj, ctx, target, level, internalformat,
	                         width, height, border, format, type, pixels, pitch);
	if (GL_NO_ERROR != err) return err;

	if ( tex_obj->generate_mipmaps && level == 0 )
	{
		err = _gles_generate_mipmap_chain(tex_obj, ctx, target, 0); 
	}

	return err;
}

GLenum _gles1_tex_sub_image_2d(
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLenum type,
	const GLvoid *pixels,
	GLint unpack_alignment
	)
{
	GLenum err;

	gles_texture_object *tex_obj = NULL;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	err = _gles_get_active_bound_texture_object(target, &ctx->state.common.texture_env, &tex_obj);
	if (err != GL_NO_ERROR) return err;

	/* Bail out early if we have a mipmap error */
	err = _gles_check_mipmap_type_error(tex_obj, level, format, type);
	if (GL_NO_ERROR != err) return err;

	err = _gles_tex_sub_image_2d(tex_obj, ctx, target, level, xoffset, yoffset,
	                             width, height, format, type, pixels, unpack_alignment);
	if (GL_NO_ERROR != err) return err;

	if ( tex_obj->generate_mipmaps && 0 == level )
	{
		err = _gles_generate_mipmap_chain(tex_obj, ctx, target, 0);
	}

	return err;
}

MALI_STATIC_INLINE GLenum _gles1_check_compressed_texture_image_format(GLenum format)
{
	switch ( format )
	{
#if EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE
		/* The format / type combinations used to get the texel format are taken from Table 3.11 of Section 3.7.4 of the OpenGL ES 1.1 spec */
	case GL_PALETTE4_RGB8_OES:
	case GL_PALETTE4_RGBA8_OES:
	case GL_PALETTE4_R5_G6_B5_OES:
	case GL_PALETTE4_RGBA4_OES:
	case GL_PALETTE4_RGB5_A1_OES:
	case GL_PALETTE8_RGB8_OES:
	case GL_PALETTE8_RGBA8_OES:
	case GL_PALETTE8_R5_G6_B5_OES:
	case GL_PALETTE8_RGBA4_OES:
	case GL_PALETTE8_RGB5_A1_OES:
		return GL_NO_ERROR;
#endif /* EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE */

#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
	case GL_ETC1_RGB8_OES:
		return GL_NO_ERROR;
#endif
	default:
		return GL_INVALID_VALUE;
	}
}

GLenum _gles1_compressed_texture_image_2d(
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLenum internalformat,
	GLsizei width,
	GLsizei height,
	GLint border,
	GLsizei imageSize,
	const GLvoid *data )
{
	GLenum err;
	gles_texture_object *tex_obj = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	err = _gles_get_active_bound_texture_object(target, &ctx->state.common.texture_env, &tex_obj);
	if (err != GL_NO_ERROR) return err;

	MALI_CHECK(GL_INVALID_VALUE != _gles1_check_compressed_texture_image_format(internalformat), GL_INVALID_ENUM);

	return _gles_compressed_texture_image_2d(tex_obj, ctx, target, level, internalformat,
	                                         width, height, border, imageSize, data);
}

GLenum _gles1_compressed_texture_sub_image_2d(
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLsizei imageSize,
	const GLvoid *data )
{
	GLenum err;
	gles_texture_object *tex_obj = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	err = _gles_get_active_bound_texture_object(target, &ctx->state.common.texture_env, &tex_obj);
	if (err != GL_NO_ERROR) return err;

	MALI_CHECK(GL_INVALID_VALUE != _gles1_check_compressed_texture_image_format(format), GL_INVALID_ENUM);

	return _gles_compressed_texture_sub_image_2d(tex_obj, ctx, target, level, xoffset, yoffset,
	                                             width, height, format, imageSize, data);
}

MALI_INTER_MODULE_API GLenum _gles1_bind_tex_image(
	struct gles_context *ctx,
	GLenum target,
	GLenum internalformat,
	mali_bool egl_mipmap_texture,
	mali_surface *surface,
	void** out_texture_obj )
{
	GLenum err = GL_NO_ERROR;
	gles_texture_object *tex_obj = NULL;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );

	err = _gles_get_active_bound_texture_object(target, &ctx->state.common.texture_env, &tex_obj);
	if ( GL_NO_ERROR != err )
	{
		_gles_share_lists_unlock( ctx->share_lists );
		return err;
	}

	if ( ( surface->format.width > GLES_MAX_TEXTURE_SIZE ) || ( surface->format.height > GLES_MAX_TEXTURE_SIZE ) )
	{
		_gles_share_lists_unlock( ctx->share_lists );
		return GL_INVALID_VALUE;
	}

	if( tex_obj->internal == NULL)
	{
		_gles_share_lists_unlock( ctx->share_lists );
		return GL_OUT_OF_MEMORY;
	}

	err = _gles_bind_tex_image( tex_obj, ctx, target, internalformat, egl_mipmap_texture, surface );
	_gles_share_lists_unlock( ctx->share_lists );
	if(err == GL_NO_ERROR)
	{
		*out_texture_obj = tex_obj;
		_gles_texture_object_addref(tex_obj);
	}
	return err;
}

MALI_INTER_MODULE_API void _gles1_unbind_tex_image( struct gles_context *ctx, void* in_tex_obj )
{
	mali_err_code err = MALI_ERR_NO_ERROR;
	gles_texture_object* tex_obj = in_tex_obj;
	int mipchain = _gles_texture_object_get_mipchain_index( GL_TEXTURE_2D );
	int miplevel = 0;	/* egl images can only be bound to level 0 */

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	_gles_share_lists_lock( ctx->share_lists );

	err = _gles_texture_miplevel_assign(ctx, tex_obj, mipchain, miplevel, 0, 0, 0, NULL);

	/* Need to deref even if there is an error */
	_gles_texture_object_deref(tex_obj);

	if (err != GL_NO_ERROR)
	{
		ctx->vtable->fp_set_error( ctx, _gles_convert_mali_err( err ) );

		_gles_share_lists_unlock( ctx->share_lists );

		return;
	}

	tex_obj->mipgen_dirty = MALI_TRUE;

	_gles_share_lists_unlock( ctx->share_lists );
}

MALI_INTER_MODULE_API GLenum _gles1_copy_texture_image_2d(
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLenum internalformat,
	GLint x,
	GLint y,
	GLsizei width,
	GLsizei height,
	GLint border )
{
	GLenum err;
	gles_texture_object *tex_obj = NULL;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	err = _gles_get_active_bound_texture_object(target, &ctx->state.common.texture_env, &tex_obj);
	if (err != GL_NO_ERROR) return err;

	err = _gles_copy_texture_image_2d(tex_obj, ctx, target, level, internalformat,
	                                  x, y, width, height, border);
	if ( err != GL_NO_ERROR ) return err;

	if ( tex_obj->generate_mipmaps && 0==level )
	{
		err = _gles_generate_mipmap_chain(tex_obj, ctx, target, 0);
	}

	return err;
}

GLenum _gles1_copy_texture_sub_image_2d(
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLint x,
	GLint y,
	GLsizei width,
	GLsizei height )
{
	GLenum err;
	gles_texture_object *tex_obj = NULL;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	err = _gles_get_active_bound_texture_object(target, &ctx->state.common.texture_env, &tex_obj);
	if (err != GL_NO_ERROR) return err;

	err = _gles_copy_texture_sub_image_2d(tex_obj, ctx, target, level, xoffset, yoffset,
	                                      x, y, width, height);
	if ( err != GL_NO_ERROR ) return err;

	if ( tex_obj->generate_mipmaps && 0==level )
	{
		err = _gles_generate_mipmap_chain( tex_obj, ctx, target, 0);
	}

	return err;
}

GLenum _gles1_delete_textures( gles_context *ctx, GLsizei n, const GLuint *textures )
{
	GLsizei i;
	mali_named_list *texture_object_list = NULL;
	struct gles_texture_environment *texture_env = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists->texture_object_list );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );
	MALI_DEBUG_ASSERT_POINTER( &ctx->state.api.gles1->texture_env );

	if ( n < 0 ) return GL_INVALID_VALUE;

	/* The GLES specification does not specify an error code for this. At least we avoid crashing */
	if ( NULL == textures ) return GL_NO_ERROR;

	texture_env = &ctx->state.common.texture_env;
	texture_object_list = ctx->share_lists->texture_object_list;

	for ( i = 0; i < n; i++ )
	{
		struct gles_wrapper *name_wrapper = NULL;
		GLuint name = textures[ i ];

		/* silently ignore 0 */
		if ( name == 0 ) continue;

		name_wrapper = __mali_named_list_get( texture_object_list, name );
		if ( NULL == name_wrapper ) continue; /* name not found, silently ignore */
		if ( NULL != name_wrapper->ptr.tex )
		{
			/* remove all state-bindings to the object */
			_gles_texture_env_remove_binding_by_ptr( texture_env, name_wrapper->ptr.tex, ctx->default_texture_object );

			/* delete object from name wrapper */
			if( name_wrapper->ptr.tex != NULL )	name_wrapper->ptr.tex->is_deleted = MALI_TRUE;
			_gles_texture_object_deref( name_wrapper->ptr.tex );
			name_wrapper->ptr.tex = NULL;
		}

		/* remove name and free name wrapper */
		__mali_named_list_remove( texture_object_list, name );
		_gles_wrapper_free( name_wrapper );
	}

	return GL_NO_ERROR;
}

GLenum _gles_texture_gen( gles_context *ctx, GLenum coord, GLenum pname, GLenum *param)
{
	int idx = ctx->state.common.texture_env.active_texture;
	MALI_DEBUG_ASSERT_POINTER( ctx );

	MALI_CHECK( coord == GL_TEXTURE_GEN_STR_OES, GL_INVALID_ENUM );
	MALI_CHECK( pname == GL_TEXTURE_GEN_MODE_OES, GL_INVALID_ENUM );

	switch(*param)
	{
		case GL_NORMAL_MAP_OES:
			vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_STAGE_TEXGEN_MODE(idx), TEXGEN_NORMALMAP);
			break;
		case GL_REFLECTION_MAP_OES:
			vertex_shadergen_encode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_STAGE_TEXGEN_MODE(idx), TEXGEN_REFLECTIONMAP);				
			break;
		default:
			return GL_INVALID_ENUM;			
	}
	return GL_NO_ERROR;
}

GLenum _gles_get_texture_gen( gles_context *ctx, GLenum coord, GLenum pname, GLenum *param)
{
	texgen_mode mode = -1;
	int idx = ctx->state.common.texture_env.active_texture;	
	MALI_DEBUG_ASSERT_POINTER( ctx );

	MALI_CHECK( coord == GL_TEXTURE_GEN_STR_OES, GL_INVALID_ENUM );
	MALI_CHECK( pname == GL_TEXTURE_GEN_MODE_OES, GL_INVALID_ENUM );

	mode = vertex_shadergen_decode(&ctx->sg_ctx->current_vertex_state, VERTEX_SHADERGEN_STAGE_TEXGEN_MODE(idx));

	switch(mode)
	{
		case TEXGEN_NORMALMAP:
			*param = GL_NORMAL_MAP_OES;
			break;
		case TEXGEN_REFLECTIONMAP:
			*param = GL_REFLECTION_MAP_OES;
			break;
		default:
			MALI_DEBUG_ASSERT(0, ("Illegal state"));
			return GL_INVALID_ENUM;			
	}

	return GL_NO_ERROR;	
}

#if EXTENSION_TEXTURE_CUBE_MAP_ENABLE
GLenum _gles1_tex_genf_oes( gles_context *ctx, GLenum coord, GLenum pname, GLfloat param )
{
	GLenum err;
	GLenum parameter = (GLenum)param;
	err = _gles_texture_gen( ctx, coord, pname, &parameter);	
	return err;
}

GLenum _gles1_tex_genfv_oes(gles_context *ctx, GLenum coord, GLenum pname, const GLfloat *params)
{
	GLenum err;
	err = _gles_texture_gen( ctx, coord, pname, (GLenum*)params);
	return err;
}

GLenum _gles1_tex_geni_oes(gles_context *ctx, GLenum coord, GLenum pname, GLint param)
{
	GLenum err;
	err = _gles_texture_gen( ctx, coord, pname, (GLenum*)&param);
	return err;
}

GLenum _gles1_tex_geniv_oes(gles_context *ctx, GLenum coord, GLenum pname, const GLint *params)
{
	GLenum err;
	err = _gles_texture_gen( ctx, coord, pname, (GLenum*)params);
	return err;
}

GLenum _gles1_tex_genx_oes(gles_context *ctx, GLenum coord, GLenum pname, GLfixed param)
{
	GLenum err;
	err = _gles_texture_gen( ctx, coord, pname, (GLenum*)&param);
	return err;
}

GLenum _gles1_tex_genxv_oes(gles_context *ctx, GLenum coord, GLenum pname, const GLfixed *params)
{
	GLenum err;
	err = _gles_texture_gen( ctx, coord, pname, (GLenum*)params);
	return err;
}

GLenum _gles1_get_tex_genfv_oes(gles_context *ctx, GLenum coord, GLenum pname, GLfloat *params)
{
	GLenum err;
	err = _gles_get_texture_gen(ctx, coord, pname, (GLenum*)params);
	return err;
}

GLenum _gles1_get_tex_geniv_oes(gles_context *ctx, GLenum coord, GLenum pname, GLint *params)
{
	GLenum err;
	err = _gles_get_texture_gen(ctx, coord, pname, (GLenum*)params);
	return err;
}

GLenum _gles1_get_tex_genxv_oes(gles_context *ctx, GLenum coord, GLenum pname, GLfixed *params)
{
	GLenum err;
	err = _gles_get_texture_gen(ctx, coord, pname, (GLenum*)params);
	return err;
}
#endif /* EXTENSION_TEXTURE_CUBE_MAP_ENABLE */

MALI_INTER_MODULE_API GLenum _gles1_egl_image_target_texture_2d(
	struct gles_context *ctx,
	GLenum target,
	GLeglImageOES image )
{
	GLenum err;
	gles_texture_object *tex_obj = NULL;
	MALI_DEBUG_ASSERT_POINTER(ctx);

	MALI_CHECK( GL_TEXTURE_2D == target || GL_TEXTURE_EXTERNAL_OES == target, GL_INVALID_ENUM );

	/* Retrieve texture object */
	err = _gles_get_active_bound_texture_object(target, &ctx->state.common.texture_env, &tex_obj);

	/* we have already checked that target is correct, so _gles_get_active_bound_texture_object
	   should not be able to fail */
	MALI_DEBUG_ASSERT(GL_NO_ERROR == err, ("Unexpected error returned from _gles_get_active_bound_texture_object\n"));
	MALI_IGNORE( err );
	MALI_DEBUG_ASSERT_POINTER( tex_obj );

	MALI_CHECK_NON_NULL( tex_obj->internal, GL_OUT_OF_MEMORY );

	return _gles_egl_image_target_texture_2d(tex_obj, ctx, target, image);
}
