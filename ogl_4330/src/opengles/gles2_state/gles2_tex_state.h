/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_tex_state.h
 * @brief Handle GL texture state
 */

#ifndef GLES2_TEX_STATE_H
#define GLES2_TEX_STATE_H

#include "mali_system.h"
#include "base/mali_debug.h"
#include "shared/mali_named_list.h"
#include "../gles_config.h"
#include "../gles_ftype.h"
#include "../gles_util.h"
#include "../gles_texture_object.h"
#include <gles_common_state/gles_tex_state.h>

/**
 * @brief Initialize the texture-environment-state
 * @param texture_env The pointer to the texture-environment-state
 * @param default_texture_object The context-specific default texture unit
 */
void _gles2_texture_env_init( gles_texture_environment *texture_env, struct gles_texture_object *default_texture_object[GLES_TEXTURE_TARGET_COUNT] );


/**
 * @brief Set the texture parameters
 * @param texture_env GLES2 texture state pointer
 * @param target Must be GL_TEXTURE_2D
 * @param pname Which parameter is to be altered
 * @param params The new value for the parameter
 * @param type The type of the new value
 * @note This is a wrapper around the GLES-entrypoint glTexParameteri/f/x(v)()
 */
GLenum _gles2_tex_parameter ( gles_texture_environment *texture_env, GLenum target, GLenum pname, const GLvoid *params, gles_datatype type) MALI_CHECK_RESULT;

#endif /* GLES2_TEX_STATE_H */
