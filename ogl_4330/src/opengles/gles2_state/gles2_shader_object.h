/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_shader_object.h
 * @brief Shader Object settings in GLES2 driver
 */
#ifndef GLES2_SHADER_OBJECT_H
#define GLES2_SHADER_OBJECT_H

#include <mali_system.h>
#include <GLES2/mali_gl2.h>
#include <shared/binary_shader/bs_object.h>

typedef struct gles2_shader_object
{
	GLenum shader_type;
	GLboolean delete_status;

	char* combined_source;             /**< An allocated source string, holding the entire shader */
	u32 combined_source_length;        /**< Length of the combined source string. Equal to calling strlen(combined_source). */
	u32 source_string_count;           /**< How many source strings did we upload when we called glSourceString? */
	s32* source_string_lengths_array;  /**< An array of the lengths of those strings at the time of upload */

	mali_atomic_int references;			     /**< num program objects this is attached to */

	/***** binary shader related hidden state *****/
	bs_shader binary;

} gles2_shader_object;

/********************
 * INTERNAL FUNCTIONS
 ********************/

/**
 * @brief allocates the shader object
 * @param type GL_FRAGMENT_SHADER or GL_VERTEX_SHADER
 */
gles2_shader_object* _gles2_shader_internal_alloc( GLenum type);

/**
 * @brief Deletes the shader object, without any questions.
 * @param vs The pointer to the program object
 * @note ensure that the number of references are 0 before calling this!
 */
void _gles2_shader_internal_free( gles2_shader_object *so );

/***************
 * API FUNCTIONS
 ***************/

/**
 * @brief Creates a new shader object
 * @param program_object_list ctx->program_object_list
 * @param type type of the shader object, being GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
 * @param name returnparameter, the name of the newly created shader
 * @note This function is a wrapper around glCreateShader
 */
GLenum _gles2_create_shader( mali_named_list *program_object_list, GLenum type, GLuint* name ) MALI_CHECK_RESULT;

/**
 * @brief Deletes an existing shader object
 * @param name Name of the shader, as given by glCreateShader
 * @param program_object_list context->program_object_list
 * @note This function is a wrapper around glDeleteShader
 */
GLenum _gles2_delete_shader( mali_named_list *program_object_list, GLuint name ) MALI_CHECK_RESULT;

/**
 * @brief Uploads strings to shader object
 * @param shadername Name of the shader, as given by glCreateShader
 * @param count number of strings
 * @param string Array of strings with program code
 * @param length Lenght of given strings. Negative values means null-terminated string, a NULL mean all are null-terminated.
 * @note This function is a wrapper around glShaderSource
 */
GLenum _gles2_shader_source( mali_named_list *program_object_list, GLuint shadername, GLsizei count, const char** string,   const GLint* length ) MALI_CHECK_RESULT;

/**
 * @brief Gets the string from the shader object
 * @param program_object_list ctx->program_object_list
 * @param shader name of shader as given by create_shader
 * @param bufSize size of the buffer
 * @param length output parameter, denoting how many letters were written to buffer
 * @param source the buffer to write to
 * @note This function is a wrapper around glGetShaderSource
 **/
GLenum _gles2_get_shader_source (mali_named_list * program_object_list, GLuint shader, GLsizei bufSize, GLsizei *length, char *source) MALI_CHECK_RESULT;

/**
 * @brief Compiles the shader object
 * @param program_object_list ctx->program_object_list
 * @param shadername Name of the shader, as given by glCreateShader
 * @note This function is a wrapper around glCompileShader
 */
GLenum _gles2_compile_shader( mali_named_list * program_object_list, GLuint shadername ) MALI_CHECK_RESULT;

/**
 * @brief Function for releasing the resources of the shader compiler.
 * @return An error if something went wrong
 * @note This function is a dummy function and has currently no effect
 *       for our drivers
 */
GLenum _gles2_release_shader_compiler( void ) MALI_CHECK_RESULT;

/**
 * @brief Checks whether object is a shader
 * @param shadername name of the shader to check
 * @note This function is a wrapper around glIsShader
 */
GLboolean _gles2_is_shader( mali_named_list *program_object_list, GLuint shadername ) MALI_CHECK_RESULT;

/**
 * @brief Returns the infolog bound to this shader object
 * @param program_object_list ctx->program_object_list
 * @param shader the shader of which we want the infolog
 * @param maxLength length of given buffer
 * @param length length of given buffer used
 * @param infoLog buffer to where we will copy the log
 * @note This function is a wrapper around glGetShaderInfoLog
 */

GLenum _gles2_get_shader_info_log(mali_named_list* program_object_list, GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog) MALI_CHECK_RESULT;

/**
 * @brief retrieves info about shader objects
 * @param program_object_list ctx->program_object_list
 * @param shader The shader we need info about
 * @param pname The info descriptor we require
 * @param params output parameter, returning the value we require.
 * @note This function is a wrapper around glGetShaderiv
 */
GLenum _gles2_get_shader(mali_named_list *program_object_list, GLuint shader, GLenum pname, GLint* params) MALI_CHECK_RESULT;

/**
 * @brief Reads n shaders, where each entry in the shaders array is a shader of an unique type (ie, one fragmentshader, one vertexshader) from a binary block.
 * @param program_object_list the active program object list
 * @param n The number of shaders to read; but as we can only read max one of each type, this number is 2, 1 or 0 (which is rather silly, but possible).
 * @param binaryformat Always GL_MALI_SHADER_BINARY_ARM
 * @param binary The binary data from this shader
 * @param length The length of this binary
 * @note This function is a wrapper around the glShaderBinary entrypoint.
 */

GLenum _gles2_shader_binary( mali_named_list *program_object_list, GLsizei n, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length ) MALI_CHECK_RESULT;

/**
 * @brief Increases the atomic refcounter
 * @param so The shader object to refcount
 */
MALI_STATIC_INLINE void _gles2_shader_object_addref(gles2_shader_object *so)
{
	MALI_DEBUG_ASSERT_POINTER( so );
	/*NOTE - do not assert on ref_count > 0 because it is initialized to 0 */
	_mali_sys_atomic_inc( &so->references );

}

/**
 * @brief decreases the atomic refcounter
 * @param so The shader object to refcount
 */
void _gles2_shader_object_deref(gles2_shader_object *so);


#endif /* GLES2_SHADER_OBJECT_H */
