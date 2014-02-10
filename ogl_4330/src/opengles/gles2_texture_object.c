/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles2_texture_object.h"

#include "gles_context.h"
#include "gles_texture_object.h"
#include "gles_share_lists.h"
#include "gles_object.h"
#include "gles_framebuffer_object.h"
#include "gles_common_state/gles_tex_state.h"
#include "gles_convert.h"
#include "gles_vtable.h"

GLenum _gles2_tex_image_2d(
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

	pitch = _gles_unpack_alignment_to_pitch(unpack_alignment, width, format, type);

	tex_obj->mipgen_dirty = MALI_TRUE;

	return _gles_tex_image_2d(tex_obj, ctx, target, level, internalformat,
			width, height, border, format, type, pixels, pitch);
}

GLenum _gles2_tex_sub_image_2d(
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

	tex_obj->mipgen_dirty = MALI_TRUE;

	return _gles_tex_sub_image_2d(tex_obj, ctx, target, level, xoffset, yoffset,
			width, height, format, type, pixels, unpack_alignment);
}

MALI_STATIC_INLINE GLenum _gles2_check_compressed_texture_image_format(GLenum format)
{
	switch ( format )
	{

#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
		case GL_ETC1_RGB8_OES:
			return GL_NO_ERROR;
#endif
		default:
			return GL_INVALID_VALUE;
	}
}

GLenum _gles2_compressed_texture_image_2d(
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

	MALI_CHECK(GL_INVALID_VALUE != _gles2_check_compressed_texture_image_format(internalformat), GL_INVALID_ENUM);

	tex_obj->mipgen_dirty = MALI_TRUE;

	return _gles_compressed_texture_image_2d(tex_obj, ctx, target, level, internalformat,
			width, height, border, imageSize, data);
}

GLenum _gles2_compressed_texture_sub_image_2d(
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

	MALI_CHECK(GL_INVALID_VALUE != _gles2_check_compressed_texture_image_format(format), GL_INVALID_ENUM);

	tex_obj->mipgen_dirty = MALI_TRUE;

	return _gles_compressed_texture_sub_image_2d(tex_obj, ctx, target, level, xoffset, yoffset,
			width, height, format, imageSize, data);
}

MALI_INTER_MODULE_API GLenum _gles2_bind_tex_image(
   struct gles_context *ctx,
   GLenum target,
   GLenum internalformat,
   mali_bool egl_mipmap_texture,
   mali_surface *surface,
   void** out_texture_object)
{
	GLenum err = GL_NO_ERROR;
	gles_texture_object *tex_obj = NULL;

	MALI_DEBUG_ASSERT_POINTER(ctx);

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
		_gles_texture_object_addref(tex_obj);
		*out_texture_object = tex_obj;
	}

	return err;
}

MALI_INTER_MODULE_API void _gles2_unbind_tex_image( struct gles_context *ctx, void* in_tex_obj )
{
	mali_err_code err = MALI_ERR_NO_ERROR;
	gles_texture_object* tex_obj = in_tex_obj;
	int mipchain = _gles_texture_object_get_mipchain_index( GL_TEXTURE_2D );
	int miplevel = 0;	/* egl images can only be bound to level 0 */

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	_gles_share_lists_lock( ctx->share_lists );

	err = _gles_texture_miplevel_assign(ctx, tex_obj, mipchain, miplevel, 0, 0, 0, NULL);

	/* Need to deref, even on error */
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

MALI_INTER_MODULE_API GLenum _gles2_copy_texture_image_2d(
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

	tex_obj->mipgen_dirty = MALI_TRUE;

	return _gles_copy_texture_image_2d(tex_obj, ctx, target, level, internalformat,
			x, y, width, height, border);
}

GLenum _gles2_copy_texture_sub_image_2d(
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

	tex_obj->mipgen_dirty = MALI_TRUE;

	return _gles_copy_texture_sub_image_2d(tex_obj, ctx, target, level, xoffset, yoffset,
			x, y, width, height);
}

GLenum _gles2_delete_textures( gles_context *ctx, GLsizei n, const GLuint *textures )
{
	GLsizei i;
	mali_named_list *texture_object_list = NULL;
	struct gles_texture_environment *texture_env = NULL;
	mali_err_code err = MALI_ERR_NO_ERROR;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists->texture_object_list );
	MALI_DEBUG_ASSERT_POINTER( &ctx->state.common.texture_env );

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

			/* unattach it from any attached points on the active framebuffer. */
			/* we need a mutex lock here because we are effectively altering the framebuffer state */
			if (NULL != ctx->share_lists->framebuffer_object_list)
			{
				mali_err_code purge_err = _gles_internal_purge_texture_from_framebuffer( ctx->state.common.framebuffer.current_object, name_wrapper->ptr.tex );
				if( err == MALI_ERR_NO_ERROR )
				{
					err = purge_err;
				}
			}

			/* delete object from name wrapper */
			if( name_wrapper->ptr.tex != NULL )	name_wrapper->ptr.tex->is_deleted = MALI_TRUE;
			_gles_texture_object_deref( name_wrapper->ptr.tex );
			name_wrapper->ptr.tex = NULL; /* needed for the wrapper deletion */
		}

		/* remove name and free name wrapper */
		__mali_named_list_remove( texture_object_list, name );
		_gles_wrapper_free( name_wrapper );
	}

	return _gles_convert_mali_err(err);
}

MALI_INTER_MODULE_API GLenum _gles2_egl_image_target_texture_2d(
	struct gles_context *ctx,
	GLenum target,
	GLeglImageOES image )
{
	GLenum err;
	gles_texture_object *tex_obj = NULL;
	MALI_DEBUG_ASSERT_POINTER(ctx);

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_CHECK( (GL_TEXTURE_2D == target) || (GL_TEXTURE_EXTERNAL_OES == target), GL_INVALID_ENUM );
#else
	MALI_CHECK( GL_TEXTURE_2D == target, GL_INVALID_ENUM );
#endif

	/* Retrieve texture object */
	err = _gles_get_active_bound_texture_object(target, &ctx->state.common.texture_env, &tex_obj);

	/* we have already checked that target is correct, so _gles_get_active_bound_texture_object
	   should not be able to fail */
	MALI_DEBUG_ASSERT(GL_NO_ERROR == err, ("Unexpected error returned from _gles_get_active_bound_texture_object\n"));
	MALI_IGNORE(err);
	MALI_DEBUG_ASSERT_POINTER( tex_obj );

	MALI_CHECK_NON_NULL( tex_obj->internal, GL_OUT_OF_MEMORY );

	tex_obj->mipgen_dirty = MALI_TRUE;

	return _gles_egl_image_target_texture_2d(tex_obj, ctx, target, image);
}
