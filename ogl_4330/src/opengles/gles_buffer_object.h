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
 * @file gles_buffer_object.h
 * @brief gles state structures and setters for buffer object states
 */

#ifndef GLES_BUFFER_OBJECT_H
#define GLES_BUFFER_OBJECT_H

#include "gles_base.h"
#include "gles_context.h"
#include "gles_state.h"
#include "shared/mali_named_list.h"


/** a structure containing the state for one buffer object */
typedef struct gles_buffer_object
{
	struct gles_gb_buffer_object_data *gb_data; /**< data owned by the geometry bakcend */
	GLuint    size;      /**< size of data. can be any non-negative integer. */
	GLenum    usage;     /**< intended usage, basicly a hint for the driver. can be GL_STATIC_DRAW or GL_DYNAMIC_DRAW */
	GLenum    access;    /**< intended access for buffer object. can only be GL_WRITE_ONLY. */
	GLenum    mapped;    /**< mapped state of buffer object. can only be GL_FALSE. */
	mali_atomic_int       ref_count; /**< reference count */
	mali_bool     is_deleted;       /**< Flag indicating if the buffer-object has been marked for deletion */
} gles_buffer_object;

/**
 * Retrieves or creates a buffer objects from the supplied name
 * @param buffer_object_list The list of the buffer objects
 * @param name The name oto be retrieved from the list
 * @return Pointer to the found object
 * @note This function creates the buffer object in case the provided name is not found in the list.
 */
MALI_CHECK_RESULT gles_buffer_object *_gles_get_buffer_object( struct mali_named_list *buffer_object_list, GLint name );

/**
 * Creates a new empty buffer object, but does not name it.
 * @return Pointer to the new object, or NULL on failure
 */
gles_buffer_object *_gles_buffer_object_new( void );

/**
 * Initializes the buffer-object-state
 * @param buffer_object The buffer-object that is to be initialized
 * @note This function initializes the buffer-object-state.
 */
void _gles_buffer_object_init(gles_buffer_object *buffer_object);


/**
 * Add a reference to a buffer-object
 * @param buffer_object The buffer-object to modify
 */
MALI_STATIC_INLINE void _gles_buffer_object_addref(gles_buffer_object *buffer_object)
{
	MALI_DEBUG_ASSERT_POINTER(buffer_object);
	MALI_DEBUG_ASSERT(_mali_sys_atomic_get( &buffer_object->ref_count ) > 0, ("Negative ref count"));
	_mali_sys_atomic_inc( &buffer_object->ref_count );
}

/**
 * Remove a reference to a buffer-object
 * @param buffer_object The buffer-object to modify
 */
void _gles_buffer_object_deref(gles_buffer_object *buffer_object);

/**
 * Delete a buffer object name wrapper and its associated object
 * @param wrapper the buffer object name wrapper object to delete
 * @note this is mainly used on exit to clean up the entire set of allocated resources.
 */
void _gles_buffer_object_list_entry_delete(struct gles_wrapper *wrapper);

/**
 * Deletes the bufferobjects identified by the buffer-array
 * @param buffer_object_list The list containing all buffer-objects
 * @param vertex_array The vertex-array-state, needed because they might attempt to delete the active buffer
 * @param n The number of buffers to be deleted
 * @param buffers The array of identifiers for the buffer-objects that are to be deleted
 * @return An errorcode
 * @note This function is a wrapper around glDeleteBuffers().
 */
GLenum _gles_delete_buffers( mali_named_list *buffer_object_list, struct gles_vertex_array *vertex_array, GLsizei n, const GLuint * buffers ) MALI_CHECK_RESULT;

/**
 * Binds the buffer to the vertex-array
 * @param ctx	 The context the buffer should be bound to
 * @param target Which type of buffer are they trying to bind
 * @param buffer The identifier for the object that is to be bound
 * @return An errorcode
 * @note This function is a wrapper around glBindBuffer().
 */
GLenum _gles_bind_buffer( gles_context *ctx, GLenum target, GLuint buffer ) MALI_CHECK_RESULT;

/**
 * Adds data to the current buffer
 * @param base_ctx The current Base driver context
 * @param vertex_array The vertex-array-state
 * @param api_version Used for API-specific error handling
 * @param target Which buffer to add data to(ELEMENT_ARRAY or ARRAY)
 * @param size The size of the buffer
 * @param data The pointer to the data
 * @param usage How is this buffer to be used, MALI_STATIC or DYNAMIC
 * @return An errorcode
 * @note This function is a wrapper around glBufferData().
 */
GLenum _gles_buffer_data(
	mali_base_ctx_handle base_ctx,
	struct gles_vertex_array *vertex_array,
	enum gles_api_version api_version,
	GLenum target,
	GLsizeiptr size,
	const GLvoid * data,
	GLenum usage) MALI_CHECK_RESULT;

/**
 * Adds data to the current buffer, but only within the given region
 * @param base_ctx The current Base driver context
 * @param vertex_array The vertex-array-state
 * @param target Which buffer to add data to(ELEMENT_ARRAY or ARRAY)
 * @param offset Points to where we should begin to add the new data
 * @param  size The size of data to be added
 * @param  data The pointer to the data
 * @return An errorcode
 * @note This function is a wrapper around glBufferSubData().
 */
GLenum _gles_buffer_sub_data( mali_base_ctx_handle base_ctx, struct gles_vertex_array *vertex_array, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) MALI_CHECK_RESULT;

#endif /* GLES_BUFFER_OBJECT_H */
