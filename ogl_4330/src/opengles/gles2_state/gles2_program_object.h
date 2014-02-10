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
 * @file gles2_program_object.h
 * @brief Program Object settings in GLES2 driver
 */
#ifndef GLES2_PROGRAM_OBJECT_H
#define GLES2_PROGRAM_OBJECT_H

#include "../gles_util.h"
#include <GLES2/mali_gl2.h>
#include <mali_system.h>
#include "gles2_shader_object.h"
#include <shared/mali_named_list.h>
#include <shared/mali_linked_list.h>
#include "../gles_base.h"
#include "../gles_config.h"
#include "../gles_ftype.h"
#include <shared/binary_shader/bs_object.h>
#include <shared/binary_shader/bs_symbol.h>
#include "../gles_common_state/gles_program_rendering_state.h"


/* The glGetActive* functions should only return a subset of all present
 * symbols in any shader. The filter list itself is present in gles2_program_object.c
 * as a global const array, but this define declares its size (required a lot of places).
 */
#define GLES_ACTIVE_FILTER_SIZE 2
extern const char* _gles_active_filters[GLES_ACTIVE_FILTER_SIZE];

/* forward declaration */
struct gles2_vertex_array;
struct gles_context;
struct gles_state;

/* Every attribute that is hard linked to a vertex attrib array through
 * glBindAttribLocation is placed in a linked list. We use a linked list
 * because of aliasing; there may be many names linked to the same index. As
 * such, this list contains all the binding combinations performed on this
 * program. The information is used when linking, ensuring that the proper
 * names are assigned to the proper attribute arrays. */
typedef struct gles2_attrib_binding
{
	char* name; /**< allocated name of this attribute */
	int index; /**< the index this name is bound to */
} gles2_attrib_binding;

enum { GLES_SHADER, GLES_PROGRAM };

/* GLES2 Program object. This struct will be added to the program object list (in a wrapper); it represents a GLES2 Program object */
typedef struct gles2_program_object
{
	/* default state settings */
	GLboolean 		delete_status;
	GLboolean 		validate_status;
	GLuint		 	attached_shaders;

	/* bindings - provides hints for linking */
	mali_linked_list      	shaders;         /**< Linked list of attached shader id's, holds all glAttachShader data */
	mali_linked_list      	attrib_bindings; /**< Linked list of gles2_attrib_bindings, holds all glBindAttribLocation data */

	/* rendering state. Guaranteed to always be present; never NULL */
	struct gles_program_rendering_state* render_state;

	/* This object is increased once per bound context but not for the named list it resides in.
	 * If the count reaches zero while the delete flag is set, the object will be deleted from the list.
	 */
	int bound_context_count;

} gles2_program_object;


/* since the openGL spec specifies that program objects and shader objects share
 * namespaces (GL21-spec, p86), we need a wrapper struct to put in the named list,
 * specifying type, which is either GLES_PROGRAM or GLES_SHADER. Yes, this is annoying. */
typedef struct gles2_program_object_wrapper
{
	GLenum type;    /**< GLES_PROGRAM or GLES_SHADER */
	void* content;  /**< pointer to shader or program object. Never NULL. */

} gles2_program_object_wrapper;


/* The program environment contains global states for all programs */
typedef struct gles2_program_environment
{
	/* Queriable states */
	GLuint current_program; 			/**< Name of current program. May or may nto have any relation to the current program rendering state*/

} gles2_program_environment;


/********************
 * Internal functions
 ********************/

/**
 * @brief Sets up a remapping table from the compiled locations to the required locations (given by glBindAttribLocation), then rewrites the linked program.
 * @param po The program object to remap
 * @return may return MALI_ERR_OUT_OF_MEMORY if out of memory, MALI_ERR_FUNCTION_FAILED if remapping impossible, or MALI_ERR_NO_ERROR.
 * @note function assumes list lock is set */
mali_err_code _gles2_link_attributes(gles2_program_object *po) MALI_CHECK_RESULT;

/**
 * @brief Set up the glUniformLocation array, indexing all legal opengl uniform locations and placing the result in a datastruct for quick access.
 * @param po The program to index for uniforms
 * @return May return MALI_ERR_OUT_OF_MEMORY if running out of memory, or MALI_ERR_NO_ERROR if successful
 * @note function assumes list lock is set */
mali_err_code _gles2_create_gl_uniform_location_table(gles2_program_object* po) MALI_CHECK_RESULT;

/**
 * @brief Create our internal cache of uniforms.  These cache the uniforms in
 * fp16 format so we don't need to convert them all when we load the uniforms
 * to mali.
 * @param render_state The program render state where we will cache the uniforms
 * @return May return MALI_ERR_OUT_OF_MEMORY if running out of memory, or MALI_ERR_NO_ERROR if successful
 */
mali_err_code _gles2_create_fp16_fs_uniform_cache(struct gles_program_rendering_state* render_state) MALI_CHECK_RESULT;

/**
 * @brief Populate our internal cache of uniforms.  These cache the uniforms in
 * fp16 format so we don't need to convert them all when we load the uniforms
 * to mali.
 * @param render_state The program render state where we will cache the uniforms
 * @return May return MALI_ERR_OUT_OF_MEMORY if running out of memory, or MALI_ERR_NO_ERROR if successful
 * @note Assumes that _gles2_create_fp16_fs_uniform_cache has run successfully.
 */
mali_err_code _gles2_fill_fp16_fs_uniform_cache(struct gles_program_rendering_state* render_state) MALI_CHECK_RESULT;

/**
 * @brief Allocates a program object
 * @return A new program object
 * @note function assumes list lock is set
 */
gles2_program_object*_gles2_program_internal_alloc(void);

/**
 * @brief DeAllocates a program object state
 * @param po The pointer to the program object
 * @note function assumes list lock is set
 */
void _gles2_program_internal_free(gles2_program_object* po);

/**
 * @brief Initializes the program environment state
 * @param pe The pointer to the program environment
 */
void _gles2_program_env_init( gles2_program_environment *pe);

/**
 * @brief Clears the attrib bindings of the program object and frees this memory.
 * @param po The pointer to the program object
 */
void _gles2_clear_attrib_bindings(gles2_program_object* po);

/**
 * @brief Helper function to find whether a name is a shader or a program, providing a layer of abstraction that removes the program wrapper.
 * @param program_object_list ctx->program_object_list
 * @param program program/shader object to consider
 * @param type The type of the returned object. GLES_PROGRAM, GLES_SHADER or GL_INVALID_VALUE if none of the above
 * @return program_object_list->listentry->data if valid, or NULL if not a valid object.
 * @note function assumes list lock is set */
void* _gles2_program_internal_get_type(mali_named_list *program_object_list, GLuint program, GLenum *type);

/**
 * @brief Helper function that unattaches a shader from a program, deleting the shader if nescessary
 * @param po program object which the shader is assumed to be attached to.
 * @param so shader object assumed attached to the list
 * @param shader_name the name of the shader object
 * @param program_object_list ctx->program_object_list
 * @note Assumes program object list lock is set, assumes the shader is actually attached, and assumes the shader name matches the shader object. */

void _gles2_program_internal_unattach(mali_named_list* program_object_list, gles2_program_object* po, gles2_shader_object* so, GLuint shader_name);

/**
 * @brief setup magic uniform locations; done automatically by glLinkProgram before returning.
 * @param po The program object in which magic uniforms should be set up.
 * @note A magical uniform is a uniform which have a special mening in gles.
 */
void _gles2_setup_magic_uniforms(struct gles2_program_object* po);

/**
 * @brief Autodelete function used by context deletion function to delete a member of the program object list
 * @param wrapper a wrapper in the program list
 * @note The function ignores all attachments between shaders and programs as it deletes
 */
void _gles2_program_object_list_entry_delete( struct gles2_program_object_wrapper* wrapper );

/***************
 * API Functions
 ***************/

/**
 * @brief Creates a new shader object
 * @param program_object_list list of program objects, from state
 * @param program return value, name of program created
 * @note This function is a wrapper around glCreateProgram
 */
GLenum _gles2_create_program( mali_named_list *program_object_list, GLuint *program ) MALI_CHECK_RESULT;

/**
 * @brief Deletes an existing program object
 * @param po The pointer to the program object state
 * @param name Name of the program, as given by glCreateProgram
 * @note This function is a wrapper around glDeleteProgram
 */
GLenum _gles2_delete_program(mali_named_list *program_object_list, GLuint name ) MALI_CHECK_RESULT;

/**
 * @brief Attaches a shader pbject to the program
 * @param program_object_list pointer to program object list
 * @param program Program object to bind to
 * @param shader Shader object to bind
 * @note This function is a wrapper around glAttachShader
 */
GLenum _gles2_attach_shader( mali_named_list *program_object_list, GLuint program,  GLuint shader) MALI_CHECK_RESULT;

/**
 * @brief Detaches a shader pbject from the program
 * @brief program_object_list context -> program_object_list
 * @param program_env pointer to program environment
 * @param program Program object to unbind from
 * @param shader Shader object to unbind
 * @note This function is a wrapper around glDetachShader
 */
GLenum _gles2_detach_shader( mali_named_list *program_object_list, gles2_program_environment *program_env, GLuint program,  GLuint shader) MALI_CHECK_RESULT;

/**
 * @brief Links the program
 * @param base_ctx the base context
 * @param program_object_list The pointer to the program object list
 * @param program Program object to link
 * @note This function is a wrapper around glLinkProgram
 */
GLenum _gles2_link_program( struct gles_context *ctx, mali_named_list *program_object_list, GLuint program ) MALI_CHECK_RESULT;

/**
 * @brief Changes the active program
 * @param po The pointer to the program object state
 * @param program Program object to use
 * @note This function is a wrapper around glUseProgram
 */
GLenum _gles2_use_program( struct gles_state *state, mali_named_list *program_object_list, GLuint program ) MALI_CHECK_RESULT;

/**
 * @brief Validates a given program
 * @param program_object_list The pointer to the program object list
 * @param program the program to be validated
 * @note This function is a wrapper adoun glValidateProgram
 */
GLenum _gles2_validate_program( mali_named_list *program_object_list, GLuint program ) MALI_CHECK_RESULT;

/**
 * @brief returns whether the program object is a program or not
 * @param program_object_list context->program_object_list
 * @param program The program to test
 * @return returns GL_TRUE of param is a program object, GL_FALSE if it is a shader object.
 * @note This function is a wrapper around glIsProgram
 */
GLboolean _gles2_is_program(mali_named_list *program_object_list, GLuint program) MALI_CHECK_RESULT;

/**
 * @brief returns whether the object is a shader or not
 * @param program_object_list context->program_object_list
 * @param program The program to test
 * @return returns GL_FALSE of param is a program object, GL_TRUE if it is a shader object.
 * @note This function is a wrapper around glIsShader
 */
GLboolean _gles2_is_shader(mali_named_list *program_object_list, GLuint program) MALI_CHECK_RESULT;

/**
 * @brief returns the program info log data, up to maxlength bytes.
 * @param program_object_list The pointer to the program object list
 * @param program the program fow which we require information
 * @param maxlength size of the infolog buffer
 * @param length number of bytes written to buffer
 * @param infoLog a buffer where the log is to be placed
 * @note This function is a wrapper around glGetProgramInfoLog
 */
GLenum _gles2_get_program_info_log( mali_named_list *program_object_list, GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog) MALI_CHECK_RESULT;

/**
 * @brief Binds a name to a generic attribute
 * @param program_object_list ctx->program_object_list
 * @param gles2_vertex_array_state state of vertex array
 * @param program The program on which to do the binding
 * @param index The number of the attribute to bind
 * @param name The name of the attribute
 * @note This function is a wrapper around glBindAttribLocation
 */
GLenum _gles2_bind_attrib_location( mali_named_list* program_object_list, GLuint program, GLuint index, const char* name ) MALI_CHECK_RESULT;

/**
 * @brief Returns program information
 * @param program_object_list ctx->program_object_list
 * @param program the program to get info from
 * @param pname the name of the attribute you want
 * @param params output parameter, returning the requested value
 * @note This function is a wrapper around glGetProgramiv
 */
GLenum _gles2_get_programiv( mali_named_list* program_object_list, GLuint program, GLenum pname, GLint* params ) MALI_CHECK_RESULT;

/**
 * @brief Returns info on active attributes
 * @param program_object_list ctx->program_object_list
 * @param program The program to get info about
 * @param index The index you want info about
 * @param bufsize Size of name parameter buffer
 * @param length output parameter, tells how much of name buffer was used
 * @param size output parameter giving size of attribute, 1 for non-array elements, arraysize otherwise
 * @param type ouput parameter giving type of attribute
 * @param name output parameter, giving name of attribute
 * @note This function is a wrapper around glGetActiveAttrib
 */
GLenum _gles2_get_active_attrib(mali_named_list* program_object_list, GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint *size, GLenum* type, char* name) MALI_CHECK_RESULT;


/**
 * @brief Returns info on active uniforms
 * @param program_object_list ctx->program_object_list
 * @param program The program to get info about
 * @param index The index you want info about
 * @param bufsize Size of name parameter buffer
 * @param length output parameter, tells how much of name buffer was used
 * @param size output parameter giving size of uniform, 1 for non-array elements, arraysize otherwise
 * @param type ouput parameter giving type of uniform
 * @param name output parameter, giving name of uniform
 * @note This function is a wrapper around glGetActiveUniform
 */
GLenum _gles2_get_active_uniform(mali_named_list* program_object_list, GLuint program, GLuint index, GLsizei bufsize,       GLsizei* length, GLint *size, GLenum* type, char* name) MALI_CHECK_RESULT;

/**
 * @brief Returns the attribute location (ie, bound vertex attribute index) of an attribute name
 * @param program_object_list ctx->program_object_list
 * @param program The program to be queried
 * @param name The name of the attribute we require the location of
 * @param retval output parameter, returning the location
 * @note This function is a wrapper around glGetAttribLocation
 */

GLenum _gles2_get_attrib_location( mali_named_list* program_object_list, GLuint program, const char* name, GLint *retval ) MALI_CHECK_RESULT;


/**
 * @brief Returns the uniform location (ie, uniform data matrix position)
 * @param program_object_list ctx->program_object_list
 * @param program The program to be queried
 * @param name The name of the uniform we require the location of
 * @param retval output parameter, returning the location
 * @note This function is a wrapper around glGetUniformLocation
 */

GLenum _gles2_get_uniform_location( mali_named_list* program_object_list, GLuint program, const char* name, GLint *retval ) MALI_CHECK_RESULT;

/**
 * @brief returns the attached shaders to this program object
 * @param program_object_list ctx->program_object_list
 * @param program The program object to query
 * @param maxcount The size of the shaders parameter
 * @param count output parameter returning the number of shaders attached
 * @param shaders output parameter returning the actual shaders
 * @note This is a wrapper around the glGetAttachedShaders function
 */

GLenum _gles2_get_attached_shaders (mali_named_list* program_object_list, GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders) MALI_CHECK_RESULT;

/**
 * @brief Typeconverts and sets a uniform value
 * @param input_type Type corresponding to the uniform call; ex GLES_FLOAT for GL_FLOAT, etc
 * @param input_width width of uniform call; ex glUniform3f = 3, glUniformMatrix2iv = 2
 * @param input_height height of uniform call; ex glUniform3f = 1, glUniformMatrix2iv = 2 (because: 2x2 matrix)
 * @param count This is the count parameter as sent to the glUniform call.
 * @param location The location of the given uniform.
 * @param value a pointer to the value(s)
 * @note This is a wrapper around all glUniform* functions
 */
GLenum _gles2_uniform(struct gles_context *ctx, gles_datatype input_type, GLuint input_width, GLuint input_height, GLint count, GLint location, const void* value) MALI_CHECK_RESULT;


GLenum _gles2_uniform1i(struct gles_context *ctx, GLint location, GLint val);

/**
 * @brief Retrieves a uniform
 * @param program_object_list the program object list
 * @param program The program on which to operate
 * @param location the location to retrieve data from, as given by glGetUniformLocation
 * @param output_array The place to put the values. Ensure this is big enough to fit all the values from the symbol
 * @param output_type The type of this array
 */
GLenum _gles2_get_uniform(mali_named_list* program_object_list, GLuint program, GLint location, void* output_array, GLenum output_type ) MALI_CHECK_RESULT;

/**
 * @brief Convert from DATATYPE_* to GL_*
 * @param type a DATATYPE_* symbol from bs_symbol.h
 * @param size a size (1-4)
 */
GLenum _gles2_convert_datatype_to_gltype(u32 type, u32 size) MALI_CHECK_RESULT;

/**
 * @brief recursively locates symbol number [target_iterator], as required by glGetActiveUniform|glGetActiveAttribute.
 * This function iterates through all symbols, by spooling through all scalars, arrays  and structures increasing the iterator at every turn.
 *
 * @param iterator - increases every time we spool through a symbol.
 * @param target_iterator - the stop condition; when this symbol is located, end traversion
 * @param members - the symbols to spool through; an array of pointers
 * @param member_count - size of this array
 * @param name - a buffer where the name of the current symbol is held. add new data directly to this pointer.
 * @param namesize - size remaining in this buffer.
 * @param symbol - a return parameter, returning the symbol located when we stop traversing
 * @return - return search result - did we find what we were looking for?
 */
mali_err_code _gles2_recursively_locate_active_symbol(GLuint* iterator, GLuint target_iterator, struct bs_symbol** members, u32 member_count, char* name, int namesize, struct bs_symbol** return_symbol) MALI_CHECK_RESULT;

#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE

/**
 * @brief Returns the program object's executable, referred to as its program binary.
 * @param program_object_list The pointer to the program object list
 * @param program Program object to link
 * @param maximum number of bytes that may be written into binary
 * @param actual number of bytes written into binary
 * @param format of the binary code
 * @param program object's binary
 * @note This function is a wrapper around glLinkProgram
*/

GLenum _gles2_get_program_binary ( mali_named_list *program_object_list, GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, GLvoid *binary );

/**
 * @brief loads a program object with a program binary previously from GetProgramBinaryOES
 * @param base_ctx the base context
 * @param program_object_list The pointer to the program object list
 * @param program Program object to link
 * @param format of the binary code
 * @param program object's binary
 * @param actual number of bytes written into binary
 * @note This function is a wrapper around glLinkProgram
*/

GLenum _gles2_program_binary ( struct gles_context *ctx, mali_named_list *program_object_list, GLuint program, GLenum binaryFormat, const GLvoid *binary, GLint length );

#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
/**
 * @brief Retrieves a uniform
 * @param ctx Gles context
 * @param program_object_list the program object list
 * @param program The program on which to operate
 * @param location the location to retrieve data from, as given by glGetUniformLocation
 * @param bufSize no more than bufSize will be written into output_array
 * @param output_array The place to put the values. Ensure this is big enough to fit all the values from the symbol
 * @param output_type The type of this array
 */
GLenum _gles2_get_n_uniform_ext( struct gles_context *ctx, mali_named_list* program_object_list, GLuint program, GLint location, GLsizei bufSize, void* output_array, GLenum output_type );
#endif

#endif /* GLES2_PROGRAM_OBJECT_H */

