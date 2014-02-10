/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_OBJECT_H_
#define _GLES_OBJECT_H_

/**
 * @file gles_object.h
 * @brief Routines that relate to gles object creation and deletion, and the wrappers that hold them.
 * */

#include "gles_context.h"


/** The wrappertype enum holds all types of wrapped up objects possible in the werapper */
enum gles_wrappertype
{
	WRAPPER_NO_OBJECT      = 0,   /**< illegal state, used for error handling */

	WRAPPER_TEXTURE        = 1,   /**< handled by gles_texture.c */
	WRAPPER_RENDERBUFFER   = 2,   /**< handled by gles_renderbuffer.c */
	WRAPPER_FRAMEBUFFER    = 3,   /**< handled by gles_framebuffer.c */
	WRAPPER_VERTEXBUFFER   = 4,   /**< handled by gles_buffer_object.c */

};

/** Structure to hold wrapped objects */
typedef struct gles_wrapper
{
	enum gles_wrappertype type;
	union
	{
		void*  generic;
		struct gles_texture_object* tex;
		struct gles_renderbuffer_object* rbo;
		struct gles_framebuffer_object* fbo;
		struct gles_buffer_object* vbo;

	} ptr;

} gles_wrapper;



/****************** WRAPPER FUNCTIONS *****************/

/**
 * Allocates and initializes a wrapper.
 * @param type The type of wrapper you want to create
 * @return A newly allocated and initialized wrapper
 */
struct gles_wrapper* _gles_wrapper_alloc( enum gles_wrappertype type ) MALI_CHECK_RESULT;

/**
 * Frees a wrapper. The wrapped object must already have been deleted.
 * @param wrapper The wrapper to delete
 */
void _gles_wrapper_free( struct gles_wrapper* wrapper );


/****************** API FUNCTIONS **********************/

/**
 * @brief Generates n empty object wrappers of a specified type in the provided list.
 * @param list the list to add the new objects into.
 * @param n The number of objects to generate.
 * @param buffers output parameter, will contain the generated names.
 * @param wrappertype The type of object we are creating.
 * @note This is a wrapper around the GLES-entrypoints glGenTexture, glGenBuffers, glGenFramebuffer, glGenRenderBuffer and so on.
 * @note Name zero is reserved for all named lists (according to GL21 spec, BindBuffers, BindTexture and GLES20 spec BindFramebuffer, BindRenderbuffer)
 *       and will never be returned.
 */
GLenum _gles_gen_objects( mali_named_list *list, GLsizei n, GLuint *buffers, enum gles_wrappertype type ) MALI_CHECK_RESULT;

/**
 * Checks whether a wrapper exists in a list, and whether wrapper has an object. This mimics the required behavior for all
 * glIsBuffer/glIsRenderbuffer/glIsFramebuffer/glIsTexture functionality.
 * @param list The list to check in
 * @param name The name to check for.
 * @note This frunction is a wrapper around the GLES entrypoints glIsBuffer, glIsRenderbuffer, glIsFramebuffer and glIstexture
 * @return MALI_TRUE if in list and contains a valid object. Otherwise false.
 */
GLboolean _gles_is_object( mali_named_list *list, GLuint name ) MALI_CHECK_RESULT;


#endif /* _GLES_OBJECT_H_ */
