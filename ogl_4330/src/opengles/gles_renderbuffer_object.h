/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_renderbuffer_object.h
 * @brief gles state structures and setters for renderbuffer object states
 */

#ifndef GLES_RENDERBUFFER_OBJECT_H
#define GLES_RENDERBUFFER_OBJECT_H

#include "gles_base.h"
#include "gles_config_extension.h"
#include "shared/mali_named_list.h"
#include <shared/mali_linked_list.h>
#include <shared/mali_surface.h>

struct gles_renderbuffer_state;
struct gles_framebuffer_state;
struct gles_wrapper;
struct gles_context;
struct egl_image;

/** a structure containing the state for one renderbuffer object */
typedef struct gles_renderbuffer_object
{
	GLenum internalformat; /* GL_NONE or something specified */
	u32 fsaa_samples; /* samples used for multialiased FBO */
	GLsizei width, height;
	GLuint red_bits, blue_bits, green_bits, alpha_bits, depth_bits, stencil_bits;
	mali_bool is_egl_image_sibling;
	mali_surface *render_target;
	mali_atomic_int ref_count;

	mali_linked_list *fbo_connection;
}gles_renderbuffer_object;

/**
 *	A struct containing the render buffer object state.
 */
typedef struct gles_renderbuffer_state
{
	gles_renderbuffer_object *current_object;
	GLuint current_object_id;

} gles_renderbuffer_state;



/******************************* STATE FUNCTIONS *****************************/

/**
 * Initializes the renderbuffer state.
 * @param rb_state The renderbuffer state to initialize
 */
void _gles_renderbuffer_state_init( gles_renderbuffer_state* rb_state );


/*********************** INTERNAL FUNCTIONALITY ******************************/

/**
 *	unbinds the bound renderbuffer from the gles state, setting it to
 *	the given object, and handles all required reference count changes.
 *	@param rb_state the renderbuffer state
 *	@param rb_obj The object you want to bind.
 *	@param rb_obj_id The id matching this object. If you lie here, nothing bad will happen
 *	                 apart from the fact that we will lie to our users.
 *  @note This function is NOT a wrapper around glBindRenderbuffer,
 *        but performs the actual job of said gl API entrypoint without any checking or locking.
 *        To unbind/bind to 0, call function with rb_obj=NULL and rb_obj_id = 0
 */
void _gles_internal_bind_renderbuffer( gles_renderbuffer_state* rb_state, gles_renderbuffer_object* rb_obj, GLuint rb_obj_id );

/*********************** RENDERBUFFER OBJECT FUNCTIONS ************************/


/**
 * @brief Increments reference count for a renderbuffer object.
 * @param rb_obj The pointer to the renderbuffer-object we want to reference.
 */
MALI_STATIC_INLINE void _gles_renderbuffer_object_addref(gles_renderbuffer_object *rb_obj)
{
	MALI_DEBUG_ASSERT_POINTER( rb_obj );
	MALI_DEBUG_ASSERT(_mali_sys_atomic_get( &rb_obj->ref_count ) > 0, ("invalid ref count (%d)", _mali_sys_atomic_get( &rb_obj->ref_count )));
	_mali_sys_atomic_inc( &rb_obj->ref_count );
}

/**
 * @brief Decrement reference count for a renderbuffer object, and delete the object if zero.
 * @param rb_obj The pointer to the renderbuffer-object we want to dereference.
 */
void _gles_renderbuffer_object_deref(gles_renderbuffer_object *rb_obj);

/**
 * Dereferences (and probably frees) a renderbuffer wrapper and its associated object.
 * @param wrapper The wrapper to delete
 * @note this is mainly used by the context deletion system to delete all created renderbuffer objects
 */
void _gles_renderbuffer_object_list_entry_delete(struct gles_wrapper *wrapper);



/**
 * @brief Deletes renderbuffer objects from the context, unbinds the deleted object if bound
 * @param renderbuffer_object_list The list of previously generated renderbuffer objects
 * @param rb_state the gles state containing all relevant info on renderbuffers
 * @param n The number of renderbuffers to delete
 * @param renderbuffers An array of names, each representing a renderbuffer name to delete
 * @note This function is a wrapper around the glDeleteRenderbuffers api call.
 */
GLenum _gles_delete_renderbuffers( struct mali_named_list *renderbuffer_object_list,
                                   struct gles_renderbuffer_state *rb_state,
                                   struct gles_framebuffer_state *fb_state,
                                   GLsizei n, const GLuint *renderbuffers ) MALI_CHECK_RESULT;


/**
 * @brief Binds the given renderbuffer as the active renderbuffer
 * @param renderbuffer_object_list The list of previously generated renderbuffer objects
 * @param rb_state The pointer to the renderbuffer_state
 * @param target The target to bind the renderbuffer to (must be GL_RENDERBUFFER)
 * @param name The identifier/name of the renderbuffer to bind
 * @return GL_NO_ERROR if succesful, errorcode if not
 * @note This function is a wrapper around glBindRenderbuffer
 */
GLenum _gles_bind_renderbuffer( struct mali_named_list *renderbuffer_object_list,
                                struct gles_renderbuffer_state *rb_state,
                                GLenum target, GLuint name) MALI_CHECK_RESULT;

/**
 * @brief Specifies the format and samples of the renderbuffer, and allocates internal
 * structures (cleaning up old ones if present).
 * @param base_ctx The current Base driver context
 * @param renderbuffer_object_list The current renderbuffer object list
 * @param rb_state the state structure containing renderbuffer info
 * @param target Must be GL_RENDERBUFFER_EXT
 * @param samples represents a request for a desired minimum number of samples
 * @param internalformat Any of the following:
 *                    GL_RGB565,
 *                    GL_RGBA4,
 *                    GL_RGB5_A1,
 *                    GL_DEPTH_COMPONENT16,
 *                    GL_STENCIL_INDEX4,
 *                    GL_STENCIL_INDEX8 (M200),
 *                    GL_RGB8 (M200)
 *                    GL_RGBA8 (M200),
 *                    GL_DEPTH_COMPONENT24 (M200)
 * @param width The width of this renderbuffer.
 * @param height The height of this renderbuffer.
 * @note This is a wrapper around glRenderbufferStorage
 * @return An error code
 */
GLenum _gles_renderbuffer_storage_multisample(
                                mali_base_ctx_handle base_ctx,
				struct mali_named_list *renderbuffer_object_list,
                                struct gles_renderbuffer_state *rb_state,
                                GLenum target,
                                GLsizei samples,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height ) MALI_CHECK_RESULT;

/**
 * @brief Specifies the format of the renderbuffer, and allocates internal
 * structures (cleaning up old ones if present).
 * @param base_ctx The current Base driver context
 * @param renderbuffer_object_list The current renderbuffer object list
 * @param rb_state the state structure containing renderbuffer info
 * @param target Must be GL_RENDERBUFFER_EXT
 * @param internalformat Any of the following:
 *                    GL_RGB565,
 *                    GL_RGBA4,
 *                    GL_RGB5_A1,
 *                    GL_DEPTH_COMPONENT16,
 *                    GL_STENCIL_INDEX4,
 *                    GL_STENCIL_INDEX8 (M200),
 *                    GL_RGB8 (M200)
 *                    GL_RGBA8 (M200),
 *                    GL_DEPTH_COMPONENT24 (M200)
 * @param width The width of this renderbuffer.
 * @param height The height of this renderbuffer.
 * @note This is a wrapper around glRenderbufferStorage
 * @return An error code
 */
GLenum _gles_renderbuffer_storage(
                                mali_base_ctx_handle base_ctx,
				struct mali_named_list *renderbuffer_object_list,
                                struct gles_renderbuffer_state *rb_state,
                                GLenum target,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height ) MALI_CHECK_RESULT;

/**
 * @brief Sets up a renderbuffer with data from an EGLImage
 * @param ctx A pointer to the GLES context
 * @param target The target to use (must be GL_RENDERBUFFER)
 * @param image The  EGLimage that contains the renderbuffer data
 * @return An error code
 */
#if EXTENSION_EGL_IMAGE_OES_ENABLE
GLenum _gles_egl_image_target_renderbuffer_storage( struct gles_context *ctx,
                                                    GLenum target,
                                                    GLeglImageOES image );
#endif

/**
 * @brief Sets up an EGLImage from a renderbuffer
 * @param ctx A pointer to the GLES context
 * @param name name of renderbuffer object
 * @param image A pointer to an egl_image struct
 * @return An error code represented by egl_image_from_gles_surface_status
 */
#if EXTENSION_EGL_IMAGE_OES_ENABLE
enum egl_image_from_gles_surface_status _gles_setup_egl_image_from_renderbuffer(
	struct gles_context *ctx,
	GLuint name,
	struct egl_image *image );
#endif

/**
 * @brief retrieve renderbuffer information
 * @param rb_state the state containing renderbuffer info
 * @param target must be GL_RENDERBUFFER
 * @param pname a legal getter enum
 * @param params the output parameter from this function.
 * @note This funciton is a wrapper arounf glGetRenderbufferParameter.
 */
GLenum _gles_get_renderbuffer_parameter( gles_renderbuffer_state *rb_state,
                                         GLenum target, GLenum pname, GLint *params ) MALI_CHECK_RESULT;


#endif /* GLES_RENDERBUFFER_OBJECT_H */
