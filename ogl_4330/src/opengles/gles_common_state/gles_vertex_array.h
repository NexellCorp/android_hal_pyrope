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
 * @file gles_vertex_array.h
 * @brief All routines related to settings for vertex arrays that are common
 *        between the ES 1.1 and 2.0 APIs.
 */

#ifndef _GLES_VERTEX_ARRAY_H_
#define _GLES_VERTEX_ARRAY_H_

#include "gles_base.h"
#include "gles_config.h"

struct gles_context;

/**
 * the state required for any vertex attribute array
 */
typedef struct gles_vertex_attrib_array
{
	GLint			size;			/**< the size of each element in the array. 3 would mean XYZ-data (ie implicit 1 for W) */
	GLsizei         given_stride;   /**< the stride between two elements in the array, as given as an input to GLES */
	GLsizei			stride;	        /**< the stride between two elements in the array. If given_stride is 0, this is
	                                     the real stride, otherwise the two fields are identical. */
	GLenum			type;			/**< the datatype for each sub-element in the array. typical values are GL_FIXED, GL_FLOAT and GL_UNSIGNED_BYTE */
	const GLvoid	*pointer;		/**< the data-pointer. if this is a VBO, then this will be zero, and data should rather be gotten from the buffer object */

	GLuint         buffer_binding;	/**< vertex buffer object binding */
	struct gles_buffer_object *vbo;	/**< Pointer to the buffer-object that this array is bound to, and thus offset from */

	GLboolean		enabled;		/**< the enable-flag. this decides if the vertex-array is the place to pick data from. */
	GLboolean		normalized;		/**< the flag that decides if the input is normalized (i.e. its datatype's range is scaled to [0, 1]) */

	/* Note that elem_type and elem_bytes are derived from the type
	 * member above.
 */
	u8				elem_type;		/**< Type of element data: GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_FLOAT, GL_FIXED map to 0-5 */
	u8				elem_bytes;		/**< Number of bytes required to represent 'elem-type' */

	u32				stream_spec;	/**< Vertex Shader stream specification */

	float          value[4];        /**< Values (valid if pointer and vbo == 0) */
} gles_vertex_attrib_array;

/**
 * struct for holding information about the GL-arrays, textures and vbos
 */
typedef struct gles_vertex_array
{
	gles_vertex_attrib_array attrib_array[GLES_VERTEX_ATTRIB_COUNT];
	                                           /**< The attribute arrays. An attribute array holds the information
	                                                about the corresponding array, e.g.
	                                                attrib_array[GLES_VERTEX_ATTRIB_POSITION] holds information
	                                                about the array passed along by glVertexPointer(). */
	/* bindings */
	GLuint array_buffer_binding;               /**< Tells us which(if any) buffer object is to be used for new
	                                             Pointer assignments. If this value is != NULL, the Pointers
	                                             passed along to us are offset by this value */
	GLuint element_array_buffer_binding;       /**< Same as array_buffer_binding, but for element-arrays */

	/* underlying objects */
	struct gles_buffer_object *vbo;            /**< The buffer-object, glBindBuffer(x) binds x to this parameter */
	struct gles_buffer_object *element_vbo;    /**< Same as vbo, but for element-arrays */

	/* ES 1.1 specific vertex array variable(s) */
	u8 client_active_texture;                  /**< The active client-texture, used for identifying which
	                                                texture-unit the new texture-coordinates are to be assigned to */

} gles_vertex_array;


/**
 * Deinits the vertex array, dereffing any buffer object bindings
 * @param vertex_array The vertex-array for our GLES context
 * @note This is an deinitialization method for the vertex-array-state.
 */
void _gles_vertex_array_deinit(struct gles_vertex_array *vertex_array);


/**
 * Sets the vertex attrib type
 * @param ctx           The gles context
 * @param index         The attribute index to set
 * @param type          The datatype of the attribute
 *
 * @note:	As a side-effect the attribute members elem_bytes and elem_type
 *			will be updated appropriately.
 */
void _gles_push_vertex_attrib_type( struct gles_context * const ctx, const GLuint index, const GLenum type );

/**
 * Sets the vertex attrib pointer
 * @param ctx           The gles context
 * @param index         The attribute index to set
 * @param size          The size of a vertex
 * @param type          The datatype of the attribute
 * @param normalized    Should the attribute be interpreted as normalized or non-normalized? (applies only to integer types)
 * @param stride        The memory stride between two vertices. Specifying a value of 0 will make the function calculate this, as if all vertices were thightly packed.
 * @param pointer       The pointer or offset vertices will be read from.
 */
void _gles_set_vertex_attrib_pointer(struct gles_context *ctx, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);

/**
 * Sets the binding for the current buffer object
 * @param vertex_array gles1_vertex_array pointer
 * @param target must be one of GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER
 * @param binding pointer to return the buffer name
 * @param buffer_object pointer to return the current buffer object
 */
void _gles_vertex_array_get_binding(gles_vertex_array *vertex_array, GLenum target, GLuint *binding, struct gles_buffer_object **buffer_object);

/**
 * Removes any binding for the buffer.
 * @param vertex_array gles1_vertex_array pointer
 * @param ptr Any buffer bound to this pointer adress will be unbound
 */
void _gles_vertex_array_remove_binding_by_ptr(gles_vertex_array *vertex_array, const void* ptr);

/**
 * Sets the binding for the current buffer object
 * @param vertex_array gles1_vertex_array pointer
 * @param target must be one of GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER
 * @param binding buffer name
 * @param buffer_object buffer object to be bound
 */
void _gles_vertex_array_set_binding(gles_vertex_array *vertex_array, GLenum target, GLuint binding, struct gles_buffer_object *buffer_object);

#if MALI_API_TRACE

/**
 * Retrieves the user attrib array buffers (non-VBO)
 * Will detect interlieved arrays and join them into a single buffer
 * @param ctx           The gles context
 * @param nvert         Number of vertices (given as param to glDrawArrays and glDrawElements)
 * @param buffers       The resulting buffers
 * @param sizes         The sizes of the resulting buffers
 * @param count         Number of elements returned in both buffers and sizes
 */
void _gles_gb_get_non_vbo_buffers(struct gles_context *ctx, u32 nvert, char* buffers[GLES_VERTEX_ATTRIB_COUNT], u32 sizes[GLES_VERTEX_ATTRIB_COUNT], u32* count);

#endif

#endif /* _GLES_VERTEX_ARRAY_H_ */
