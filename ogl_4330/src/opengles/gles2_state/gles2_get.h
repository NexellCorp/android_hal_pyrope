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
 * @file gles2_get.h
 * @brief Get GLES-state
 */

#ifndef _GLES2_GET_H_
#define _GLES2_GET_H_

#include <mali_system.h>
#include <gles_base.h>
#include <gles_util.h>

struct gles_state;
struct gles_common_state;
struct gles_context;
struct gles_vertex_array;

/**
 * @brief get a state value, converted to a specified datatype
 * @param ctx The pointer to the GLES context
 * @param pname The enum corresponding to the state-value
 * @param params An array to store the result in
 * @param type Which type they want the data returned as
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoints glGet() consists of.
 */
GLenum _gles2_getv( struct gles_context *ctx, GLenum pname, GLvoid *params, gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief get a texture parameter
 * @param state A pointer to the part of the OpenGL ES state that is common to both APIs
 * @param target Must be GL_TEXTURE_2D
 * @param pname The enum identifying which texture-parameter they want
 * @param params An array to store the result in
 * @param type Which type they want the data returned as
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glGetTexParameter().
 */
GLenum _gles2_get_tex_param( struct gles_common_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief see if the setting is enabled
 * @param ctx The pointer to the GLES context
 * @param cap Which enable state they want
 * @param enabled The array to return the result in
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glIsEnabled().
 */
GLenum _gles2_is_enabled( struct gles_context *ctx, GLenum cap, GLboolean *enabled ) MALI_CHECK_RESULT;

/**
 * @brief get texture environment settings
 * @param ctx The pointer to the GLES context
 * @param target Must be GL_TEXTURE_2D or GL_TEXTURE_ENV
 * @param pname Which state they want for the target
 * @param params An array to return the result in
 * @param type Which data-type they want the result to be
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glGetTexEnv().
 */
/*GLenum _gles2_get_tex_env( gles2_context *ctx, GLenum target, GLenum pname, GLvoid *params, gles_datatype type ) MALI_CHECK_RESULT;
*/


/**
 * @brief get buffer parameters
 * @param state A pointer to the part of the OpenGL ES state that is common to both APIs
 * @param target Specifies which buffer they want to retrieve info from(ELEMENT_ARRAY or ARRAY)
 * @param pname The identifier for which info they want from the buffer
 * @param params The array to return the result in
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glGetBufferParameter().
 */
GLenum _gles2_get_buffer_parameter( struct gles_common_state *state, GLenum target, GLenum pname, GLint *params ) MALI_CHECK_RESULT;


/**
 * @brief get a string describing the current GL connection
 * @param ctx The pointer to the GLES context
 * @param name The identifier for which string they want
 * @param string an array of strings to return the result in
 * @note This is a wrapper function for the GLES entrypoint glGetString().
 */
GLenum _gles2_get_string( struct gles_context *ctx, GLenum name, const GLubyte **return_string);

/**
 * @brief returns the range and precision for various precision formats supported by the implementation
 * @param shadertype either GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
 * @param precisiontype can be LOW_FLOAT, MEDIUM_FLOAT, HIGH_FLOAT, LOW_INT, MEDIUM_INT or HIGH_INT
 * @param range an array of the maximum and minimum representable range as log based 2 number
 * @param precision returns the precision as a log based 2 number
 * @note This is a wrapper function for the GLES entrypoint glGetString().
 */
GLenum _gles2_get_shader_precision_format(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);

/**
 * @brief Returns vertex array attributes
 * @param vertex_array The vertex array state
 * @param index The indexed array to get info from
 * @param pname The GLES datatype of the return parameter
 * @param intype The internal datatype of the return parameter
 * @param param the return parameter value.
 * @note This function is a wrapper around glGetVertexAttrib
 */
MALI_CHECK_RESULT GLenum _gles2_get_vertex_attrib(struct gles_vertex_array *vertex_array, GLuint index, GLenum pname, gles_datatype intype, void* param);

/**
 * @brief returns the vertex array pointer
 * @param vertex_array The vertex array state
 * @param index the index to gedt the pointer from
 * @param param the output pointer value
 * @note This function is a wrapper around glGetVertexAttribPointer
 */
MALI_CHECK_RESULT GLenum _gles2_get_vertex_attrib_pointer(struct gles_vertex_array *vertex_array, GLuint index, GLenum pname, void** param);

#endif /* _GLES_GET_H_ */
