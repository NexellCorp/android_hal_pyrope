/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_tex_state.h
 * @brief Handles the parts of the GL texture environment and object state
 *        that is common between the ES 1.1 and 2.0 APIs.
 */

#ifndef _GLES_TEX_STATE_H_
#define _GLES_TEX_STATE_H_

#include "gles_texture_object.h"

/**
 * texture state / unit state. This is the part of the state that is common between
 * GLES 1.1 and 2.0.
 */
typedef struct gles_texture_unit
{
	GLboolean                    enable_vector[GLES_TEXTURE_TARGET_COUNT];          /**< Is this texture-target enabled?. OpenGL ES 2.0
	                                                                                     everything is always enabled */
	struct gles_texture_object  *current_texture_object[GLES_TEXTURE_TARGET_COUNT]; /**< The active texture-object for this texture-unit */
	GLuint                       current_texture_id[GLES_TEXTURE_TARGET_COUNT];     /**< The active texture-id for this texture-unit */

} gles_texture_unit;

/**
 * The texture environment state. This is the part of the state that is common
 * between GLES 1.1 and 2.0.
 */
typedef struct gles_texture_environment
{
	GLint active_texture;                             /**< The active texture-unit, all changes to
	                                                       texture-environment is done to this unit */
	gles_texture_unit unit[ GLES_MAX_TEXTURE_UNITS ]; /**< The different texture-units */

} gles_texture_environment;


/*** functions to update bindings, used by the texture object ***/

/**
 * @brief Gets the texture binding for the active texture unit
 * @param tex_env The texture environment to retrieve the texture object currently
 *                bound to the active texture unit at the given target from
 * @param target The target to retrieve the current texture object for
 * @param binding An output parameter for retrieving the name of the texture object
 *                bound to the given target for the active texture unit
 * @param texture_object An output parameter for retrieving the texture object bound
 *                       to the given target for the active texture unit
 * @param api_version The current API version: 1 for gles 1.x, 2 for gles 2.0
 */
MALI_STATIC_INLINE void _gles_texture_env_get_binding(struct gles_texture_environment *tex_env, GLenum target, GLuint *binding, struct gles_texture_object **texture_object, enum gles_api_version api_version)
{
	gles_texture_unit *tex_unit;
	enum gles_texture_target dimensionality = GLES_TEXTURE_TARGET_2D;

	MALI_DEBUG_ASSERT_POINTER(tex_env);
	tex_unit = &tex_env->unit[tex_env->active_texture];
	MALI_DEBUG_ASSERT_POINTER(tex_unit);

	dimensionality = _gles_get_dimensionality(target, api_version);
	MALI_DEBUG_ASSERT_RANGE((int)dimensionality, 0, GLES_TEXTURE_TARGET_COUNT - 1);

	if (NULL != binding)        *binding        = tex_unit->current_texture_id[dimensionality];
	if (NULL != texture_object) *texture_object = tex_unit->current_texture_object[dimensionality];
}

/**
 * @brief Sets the texture binding for the active texture unit
 * @param tex_env The texture environment to set the texture object as the bound
 *                texture object of the given target for the active texture unit
 * @param target The target of the active texture unit to bind the given texture
 *               object to
 * @param binding The name of the texture object to bind
 * @param texture_object The texture object to bind
 * @param api_version The current API version: 1 for gles 1.x, 2 for gles 2.0
 */
MALI_STATIC_INLINE void _gles_texture_env_set_binding(struct gles_texture_environment *tex_env, GLenum target, GLuint binding, struct gles_texture_object *texture_object, enum gles_api_version api_version)
{
	gles_texture_unit *tex_unit;
	enum gles_texture_target dimensionality;

	MALI_DEBUG_ASSERT_POINTER(tex_env);
	tex_unit = &tex_env->unit[tex_env->active_texture];
	MALI_DEBUG_ASSERT_POINTER(tex_unit);

	dimensionality = _gles_get_dimensionality(target, api_version);
	MALI_DEBUG_ASSERT_RANGE((int)dimensionality, 0, GLES_TEXTURE_TARGET_COUNT - 1);

	tex_unit->current_texture_id[dimensionality]     = binding;
	tex_unit->current_texture_object[dimensionality] = texture_object;
}

/**
 * @brief Set the active texture-object for the current texture-unit
 * @param ctx The gles context
 * @param texture The identifier for the new active texture
 * @return An errocode.
 * @note This implements the GLES entrypoint glActiveTexture().
 */
GLenum _gles_active_texture( struct gles_context *ctx, GLenum texture )MALI_CHECK_RESULT;

/**
 * @brief returns the texture object currently bound to a texture unit.
 * @param texture_env the current gl state texture environment
 * @param target A GLES_TEXTURE_TARGET_* constant. Must be a legal enum. Cannot be TARGET_INVALID or TARGET_COUNT.
 * @param texunit_id The ID to get the bound texture from. Must be in legal range.
 * @return the bound texture object to the given unit. May be NULL if nothing was bound to this target/unit pair.
 */
MALI_STATIC_INLINE struct gles_texture_object* _gles_get_texobj_from_unit_id( gles_texture_environment *texture_env, enum gles_texture_target target, int texunit_id )
{
	gles_texture_unit *texture_unit;
	MALI_DEBUG_ASSERT_RANGE( texunit_id, 0, GLES_MAX_TEXTURE_UNITS-1);
	MALI_DEBUG_ASSERT_RANGE( target, 0, GLES_TEXTURE_TARGET_COUNT-1);

	texture_unit = &texture_env->unit[texunit_id];
	MALI_DEBUG_ASSERT_POINTER(texture_unit->current_texture_object[target]);
	return texture_unit->current_texture_object[target];
}

/**
 * Removes any texture bindings to the given texture name
 * texture_env the current gl state texture environment
 * ptr texture to remove
 * texture_object pointer to texture object
 **/
void _gles_texture_env_remove_binding_by_ptr(struct gles_texture_environment *tex_env, const void* ptr, struct gles_texture_object *default_texture_object[GLES_TEXTURE_TARGET_COUNT] );

/**
 * Derefernces the current gl_state texture environment
 * texture_env the current gl state texture environment
 */
void _gles_texture_env_deref_textures(struct gles_texture_environment *tex_env);


#endif /* _GLES_TEX_STATE_H_ */
