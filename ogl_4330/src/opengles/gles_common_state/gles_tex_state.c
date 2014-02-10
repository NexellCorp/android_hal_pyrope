/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_tex_state.h"
#include "gles_context.h"
#include "gles_base.h"

GLenum _gles_active_texture( struct gles_context *ctx, GLenum texture )
{
	GLint tex;
	gles_texture_environment *texture_env = &ctx->state.common.texture_env;

	MALI_DEBUG_ASSERT_POINTER( texture_env );

	tex = (GLint) texture - GL_TEXTURE0;

	if (( tex < 0 ) || ( tex >= GLES_MAX_TEXTURE_UNITS )) MALI_ERROR( GL_INVALID_ENUM );

	texture_env->active_texture = tex;

	GL_SUCCESS;
}

void _gles_texture_env_remove_binding_by_ptr(struct gles_texture_environment *tex_env, const void* ptr, struct gles_texture_object *default_texture_object[GLES_TEXTURE_TARGET_COUNT])
{
	int i, j;

	MALI_DEBUG_ASSERT_POINTER(tex_env);
	MALI_DEBUG_ASSERT_POINTER(default_texture_object);

	for (i = 0; i < GLES_MAX_TEXTURE_UNITS; i++)
	{
		gles_texture_unit *tex_unit = &tex_env->unit[i];

		for (j = 0; j < GLES_TEXTURE_TARGET_COUNT; ++j)
		{
			if ((const void*)tex_unit->current_texture_object[j] == ptr)
			{
				gles_texture_object *tex_obj = tex_unit->current_texture_object[j];
				MALI_DEBUG_ASSERT_POINTER(tex_obj);

				tex_unit->current_texture_object[j] = default_texture_object[j];
				tex_unit->current_texture_id[j]     = 0;

				/* update reference counts */
				_gles_texture_object_addref(default_texture_object[j]);
				_gles_texture_object_deref(tex_obj);
				tex_obj = NULL;
			}
		}
	}
}

void _gles_texture_env_deref_textures(struct gles_texture_environment *tex_env)
{
	int i, j;
	MALI_DEBUG_ASSERT_POINTER(tex_env);

	for (i = 0; i < GLES_MAX_TEXTURE_UNITS; i++)
	{
		gles_texture_unit *tex_unit = &tex_env->unit[i];

		for (j = 0; j < GLES_TEXTURE_TARGET_COUNT; ++j)
		{
			gles_texture_object *tex_obj = tex_unit->current_texture_object[j];
			MALI_DEBUG_ASSERT_POINTER(tex_obj);

			tex_unit->current_texture_object[j] = NULL;
			tex_unit->current_texture_id[j]     = 0;

			/* update reference count */
			_gles_texture_object_deref(tex_obj);
			tex_obj = NULL;
		}
	}
}



