/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_tex_state.h
 * @brief Handle GL texture environment and object state
 */

#ifndef _GLES1_TEX_STATE_H_
#define _GLES1_TEX_STATE_H_

#include <mali_system.h>

#include "gles_config.h"
#include "gles_ftype.h"
#include "gles_util.h"
#include "gles_texture_object.h"
#include "gles_common_state/gles_tex_state.h"

struct gles_state;
struct gles_context;

/**
 * GLES 1.1 texture state / unit state. This is the part of the texture unit state that
 * is specific to GLES 1.1.
 */
typedef struct gles1_texture_unit
{
	GLboolean                   coord_replace;               /**< extension: OES_point_sprite */
	GLenum                      env_mode;                    /**< Which texture-environment mode is active for this texture-unit */
	GLenum                      combine_rgb, combine_alpha;  /**< Specifies which functions to use to combine the rgb/alpha components */
	GLenum                      src_rgb[ 3 ];                /**< GL_SRCn_RGB */
	GLenum                      src_alpha[ 3 ];              /**< GL_SRCn_ALPHA */
	GLenum                      operand_rgb[ 3 ];            /**< GL_OPERANDn_RGB */
	GLenum                      operand_alpha[ 3 ];          /**< GL_OPERANDn_ALPHA */
	GLuint                      rgb_scale, alpha_scale;
	GLftype                     env_color[ 4 ];              /**< The environment color, used as initial color for multitexturing */
	GLftype                     lod_bias;                    /**< extension: EXT_texture_lod_bias  */

} gles1_texture_unit;

/**
 * GLES 1.1 texture environment state. This is the part of the texture environment state that
 * is specific to GLES 1.1.
 * */
typedef struct gles1_texture_environment
{
	gles1_texture_unit unit[ GLES_MAX_TEXTURE_UNITS ]; /**< The different texture-units */
	GLboolean point_sprite_enabled;                    /**< The global texture state for point_sprites */
	GLubyte tex_gen_enabled;                           /**< The global texture state for tex coordinate generating. Each bit stands for each texture unit */
} gles1_texture_environment;

/**
 * @brief Retrieves the current texture unit.
 * @param texture_env the texture environment state struct
 * @return Return a pointer to a texture unit state. Do not delete.
 */
MALI_STATIC_INLINE gles_texture_unit *_gles1_texture_env_get_active_unit(struct gles_texture_environment *texture_env)
{
	MALI_DEBUG_ASSERT_POINTER(texture_env);
	MALI_DEBUG_ASSERT(
		texture_env->active_texture < GLES_MAX_TEXTURE_UNITS,
		("active texture out of range!")
	);
	return &texture_env->unit[texture_env->active_texture];
}

/**
 * @brief Retrieves the current texture object from a unit, as seen by GLES1 getter rules.
 *        Basically, this means returning the latest target enabled within the unit.
 * @param tex_unit - The texture unit to query
 * @param out_target - A return parameter, returning the GLES_TEXTUE_TARGET_* enum matching the found object 
 * @return tex_obj - The found texture object that govens the texture unit.
 *                   May be NULL if no texture was available. 
 */
MALI_STATIC_INLINE gles_texture_object* _gles1_texture_env_get_dominant_texture_object(const struct gles_texture_unit* unit, enum gles_texture_target *out_target)
{
	int j;
	MALI_DEBUG_ASSERT_POINTER(unit);
	
	for(j = GLES_TEXTURE_TARGET_COUNT-1; j>=0; j--)
    {
		if( ! unit->enable_vector[j] ) continue; /* the target is not enabled, try the next one */

		if(out_target) *out_target = j;
		return unit->current_texture_object[j];
	}
	return NULL; /* no targets enabled, return NULL */
}


/**
 * @brief Initialize the texture-environment-state
 * @param ctx A pointer to the gles context
 * @param gles_texture_object The pointer to the texture-object
 * @return A boolean stating the success of this initialization
 */
mali_bool _gles1_texture_env_init( struct gles_context *ctx, struct gles_texture_object *default_texture_object[GLES_TEXTURE_TARGET_COUNT] );

/**
 * @brief Set texture-environment parameters
 * @param ctx The pointer to the GL context
 * @param target Must be GL_TEXTURE_ENV or GL_POINT_SPRITE_OES
 * @param pname Identifier specifying which parameter we want to change
 * @param params The new values for the parameter
 * @param type The data-type of the new values
 * @return An errorcode
 * @note This implements the GLES entrypoint glTexEnvv().
 */
GLenum _gles1_tex_envv(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *params,
	gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief Set texture-environment parameters
 * @param ctx The pointer to the GL context
 * @param target Must be GL_TEXTURE_ENV or GL_POINT_SPRITE_OES
 * @param pname Identifier specifying which parameter we want to change
 * @param param The new value for the parameter
 * @param type The data-type of the new value
 * @return An errorcode.
 * @note This implements the GLES entrypoint glTexEnv().
 */
GLenum _gles1_tex_env(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *param,
	gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief Set the texture parameters
 * @param texture_env GLES texture state pointer
 * @param target Must be GL_TEXTURE_2D
 * @param pname Which parameter is to be altered
 * @param param The new value for the parameter
 * @param type The type of the new value
 * @note This is a wrapper around the GLES-entrypoint glTexParameter()
 */
GLenum _gles1_tex_parameter ( gles_texture_environment *texture_env, GLenum target, GLenum pname, const GLvoid *param, gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief Set the texture parameters
 * @param texture_env GLES texture state pointer
 * @param target Must be GL_TEXTURE_2D
 * @param pname Which parameter is to be altered
 * @param param The new value for the parameter
 * @param type The type of the new value
 * @note This is a wrapper around the GLES-entrypoint glTexParameterv()
 */
GLenum _gles1_tex_parameter_v( gles_texture_environment *texture_env, GLenum target, GLenum pname, const GLvoid *params, gles_datatype type ) MALI_CHECK_RESULT;


/*** functions to update bindings, used by the texture object ***/

/**
 * @brief Set the active texture-object for the current texture-unit
 * @param ctx The gles context
 * @param texture The identifier for the new active texture
 * @return An errocode.
 * @note This implements the GLES entrypoint glActiveTexture().
 */
GLenum _gles1_active_texture( struct gles_context *ctx, GLenum texture ) MALI_CHECK_RESULT;

#endif /* _GLES1_TEX_STATE_H_ */
