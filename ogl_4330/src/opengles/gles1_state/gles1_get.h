/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_get.h
 * @brief Get GLES-state
 */

#ifndef _GLES1_GET_H_
#define _GLES1_GET_H_

#include <mali_system.h>
#include "gles_base.h"
#include "gles_util.h"

struct gles_context;
struct gles_state;
struct gles_common_state;

/**
 * @brief get a state value, converted to a specified datatype
 * @param ctx The pointer to the GLES context
 * @param pname The enum corresponding to the state-value
 * @param params An array to store the result in
 * @param type Which type they want the data returned as
 * @return An errorcode.
 * @note This implements the GLES entrypoints glGet() consists of.
 */
GLenum _gles1_getv( struct gles_context *ctx, GLenum pname, GLvoid *params, gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief get a texture parameter
 * @param state A pointer to the part of the OpenGL ES state that is common to both APIs
 * @param target Must be GL_TEXTURE_2D
 * @param pname The enum identifying which texture-parameter they want
 * @param params An array to store the result in
 * @param type Which type they want the data returned as
 * @return An errorcode.
 * @note This implements the GLES entrypoint glGetTexParameter().
 */
GLenum _gles1_get_tex_param( struct gles_common_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief see if the setting is enabled
 * @param ctx The pointer to the GLES context
 * @param cap Which enable state they want
 * @param enabled The array to return the result in
 * @return An errorcode.
 * @note This implements the GLES entrypoint glIsEnabled().
 */
GLenum _gles1_is_enabled( struct gles_context *ctx, GLenum cap, GLboolean *enabled ) MALI_CHECK_RESULT;

/**
 * @brief get texture environment settings
 * @param state The pointer to the GLES state
 * @param target Must be GL_TEXTURE_2D or GL_TEXTURE_ENV
 * @param pname Which state they want for the target
 * @param params An array to return the result in
 * @param type Which data-type they want the result to be
 * @return An errorcode.
 * @note This implements the GLES entrypoint glGetTexEnv().
 */
GLenum _gles1_get_tex_env( struct gles_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief Get a clip plane equation
 * @param state The pointer to the GLES state
 * @param plane Which plane they want the equation of
 * @param equation The array to store the result in
 * @param type The data-type they want the result to be
 * @return An errorcode
 * @note This implements the GLES entrypoint glGetClipPlane().
 */
GLenum MALI_CHECK_RESULT _gles1_get_clip_plane( struct gles_state *state, GLenum plane, GLvoid *equation, gles_datatype type );

/**
 * @brief get pointers to arrays in the vertex-array
 * @param state The pointer to the GLES state
 * @param pname The identifier for the array they want the pointer to
 * @param pointer An array to store the pointer in
 * @return An errorcode.
 * @note This implements the GLES entrypoint glGetPointer().
 */
GLenum MALI_CHECK_RESULT _gles1_get_pointer( struct gles_state *state, GLenum pname, GLvoid **pointer );

/**
 * @brief get buffer parameters
 * @param state A pointer to the part of the OpenGL ES state that is common to both APIs
 * @param target Specifies which buffer they want to retrieve info from(ELEMENT_ARRAY or ARRAY)
 * @param pname The identifier for which info they want from the buffer
 * @param params The array to return the result in
 * @return An errorcode.
 * @note This implements the GLES entrypoint glGetBufferParameter().
 */
GLenum MALI_CHECK_RESULT _gles1_get_buffer_parameter( struct gles_common_state *state, GLenum target, GLenum pname, GLint *params );

/**
 * @brief get material properties
 * @param state The pointer to the GLES state
 * @param face Which face they want to get the material info from
 * @param pname The identifier for which info they want from the material
 * @param params The array to store the result in
 * @param type The data-type they want the data to be returned as.
 * @return An errorcode.
 * @note This implements the GLES entrypoint glGetMaterial().
 */
GLenum MALI_CHECK_RESULT _gles1_get_material( struct gles_state *state, GLenum face, GLenum pname, GLvoid *params, gles_datatype type );

/**
 * @brief get light properties
 * @param state The pointer to the GLES state
 * @param light Which light they want to retrieve info from
 * @param pname The identifier for the info they want
 * @param params The array to store the result in
 * @return An errorcode.
 * @note This implements the GLES entrypoint glGetLight().
 */
GLenum MALI_CHECK_RESULT _gles1_get_light( struct gles_state *state, GLenum light, GLenum pname, GLvoid *params, gles_datatype type);

/**
 * @brief get a string describing the current GLES configuration
 * @param ctx The pointer to the GLES context
 * @param name The identifier for which string they want
 * @param string an array of strings to return the result in
 * @note This implements the GLES entrypoint glGetString().
 */
GLenum MALI_CHECK_RESULT _gles1_get_string( struct gles_context *ctx, GLenum name, const GLubyte **return_string );

#endif /* _GLES1_GET_H_ */
