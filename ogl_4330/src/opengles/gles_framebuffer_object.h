/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_framebuffer_object.h
 * @brief gles state structures and setters for framebuffer object states
 */

#ifndef GLES_FRAMEBUFFER_OBJECT_H
#define GLES_FRAMEBUFFER_OBJECT_H

#include "gles_base.h"
#include "gles_config.h"
#include "shared/mali_named_list.h"
#include <gles_context.h>
#include "shared/m200_gp_frame_builder.h"
#include <gles_common_state/gles_framebuffer_state.h>
#include <m200_backend/gles_fb_texture_object.h>

struct gles_renderbuffer_object;
struct gles_renderbuffer_state;
struct gles_framebuffer_state;
struct gles_texture_object;
struct gles_state;

enum attachment_type
{
	GLES_ATTACHMENT_TYPE_NONE,
	GLES_ATTACHMENT_TYPE_TEXTURE,
	GLES_ATTACHMENT_TYPE_RENDERBUFFER,
};

/* a structure defining an attachment point */
typedef struct gles_framebuffer_attachment
{
	u32 point; /* either MALI_OUTPUT_COLOR, MALI_OUTPUT_DEPTH or MALI_OUTPUT_STENCIL */
	u32 wbx_index; /* Fbuilder WBx ID. */
	u32 fsaa_samples; /* samples used for multialiased FBO */
	enum attachment_type attach_type; /* bound to a texture, rbo, default framebuffer, or none? */
	GLuint name; /* the object number we attached to, at the time of attachment */

	/** This union contains the pointers to the possible types of attachments. Check type for which pointer to chose */
	union {
		struct gles_renderbuffer_object *rb_obj;             /* valid if attach_type = GLES_ATTACHMENT_TYPE_RENDERBUFFER, */
		struct gles_texture_object *tex_obj;                 /* valid if attach_type = GLES_ATTACHMENT_TYPE_TEXTURE */
		struct egl_gles_surface_capabilities surface_capabilities; /* valid if implicit_framebuilder = MALI_TRUE */
	} ptr;

	mali_bool completeness_dirty; /* set whenever the constraints of the attached object change */

	/* the following two fields are only used for textures */
	GLint miplevel; /**< Which miplevel are we writing to? */
	GLenum cubeface; /**< If we are using a cubemap, this contains the cubeface enum, otherwise GL_TEXTURE_2D */

#if EXTENSION_DISCARD_FRAMEBUFFER
	/* DiscardFramebufferEXT support */
	mali_bool discarded;
#endif

} gles_framebuffer_attachment;


/** a structure containing the state for one framebuffer object */
typedef struct gles_framebuffer_object
{
	/* these attachments wrap the surfaces for the draw_frame_builder */
	gles_framebuffer_attachment color_attachment;
	gles_framebuffer_attachment depth_attachment;
	gles_framebuffer_attachment stencil_attachment;

	/* The implicit framebuilder determines whether
	 * MALI_TRUE if the framebuilder is created when creating this FBO.
	 * MALI_FALSE if the framebuilders is owned by an exterior instance (EGL).
	 * If false, the framebuilder properties and its attachments will never be queried or modified by GLES.
	 */
	mali_bool implicit_framebuilder;

	/* Framebuilders associated with this FBO. For FBOs with implicit framebuilders, these will be identical,
	 * and the framebuilder in this case will be created along with the FBO, and its lifetime follow that of the FBO.
	 * For FBOs without implicit FBOs, the framebuilders may differ, and are owned by external instances (like EGL).
	 * */
	mali_frame_builder *read_frame_builder;
	mali_frame_builder *draw_frame_builder;


	mali_bool draw_flip_y;               /* if set, the draw framebuilder should flip its Y output. FBO #0 may have this behavior */
	mali_bool read_flip_y;               /* if set, the read framebuilder should flip its y output. FBO #0 may have this behavior */
	mali_bool retain_commands_on_unbind; /* if set, the FBO will retain its commandlists after being unbound. FBO #0 should have this set */

	int draw_supersample_scalefactor;    /* Always 1 on FBOs, 2 if 16x FSAA */

	/* whenever any attachments change, the completeness dirty is set TRUE.
	 * Subsequent drawcalls or queries to the number of bits need to resolve the completeness dirty.
	 * Non-implicit framebuilder FBOs (ie, FBO#0) will never have this field true.
	 */
	mali_bool completeness_dirty;

	mali_atomic_int ref_count;
	mali_bool     is_deleted;       /**< Flag indicating if the framebuffer-object has been marked for deletion */

#if HARDWARE_ISSUE_7571_7572
	mali_bool framebuffer_has_triangles; /* Does the framebuffer contain triangles drawn after the last dummy_quad was drawn? */
#endif

	u32 num_verts_drawn_per_frame;

#if EXTENSION_DISCARD_FRAMEBUFFER
	/* bitmask of discarded attachments; lowest bit for the color, second
	 * lowest for the depth, third for the stencil */
	u32 discarded_attachments;
#endif
} gles_framebuffer_object;


/*****************************************************************************/
/*                                Internal functions                         */
/*****************************************************************************/

/**
 * @brief Increase the number of vertices drawn in this fbo in this frame
 * @param fb_obj The framebuffer object
 * @param count Number of vertices to add
 */
MALI_STATIC_INLINE void _gles_fbo_increase_num_verts_drawn_per_frame( gles_framebuffer_object* fb_obj, GLint count )
{
	MALI_DEBUG_ASSERT_POINTER(fb_obj);
	fb_obj->num_verts_drawn_per_frame += count;
}

/**
 * @brief Reset the number of vertices drawn in this fbo in this frame
 * @param fb_obj The framebuffer object
 */
MALI_STATIC_INLINE void _gles_fbo_reset_num_verts_drawn_per_frame( gles_framebuffer_object* fb_obj )
{
	MALI_DEBUG_ASSERT_POINTER(fb_obj);
	fb_obj->num_verts_drawn_per_frame = 0;
}

/**
 * @brief Return the number of vertices drawn in this fbo in this frame
 * @param fb_obj The framebuffer object
 */
MALI_STATIC_INLINE int _gles_fbo_get_num_verts_drawn_per_frame( gles_framebuffer_object* fb_obj )
{
	MALI_DEBUG_ASSERT_POINTER(fb_obj);
	return fb_obj->num_verts_drawn_per_frame;
}

/**
 * @brief Retrieves the supersample scale factor for the draw operations on the current FBO
 * @param ctx A pointer to the gles_context to set as the current OpenGL ES context
 * @return 1 or 2, depending on whether we are using 16xFSAA
 */
MALI_STATIC_INLINE int _gles_get_current_draw_supersample_scalefactor(gles_context *ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->state.common.framebuffer.current_object);
	return ctx->state.common.framebuffer.current_object->draw_supersample_scalefactor;
}

/**
 * @brief Retrieves the frame builder to use for draw operations
 * @param ctx A pointer to the gles_context to set as the current OpenGL ES context
 * @return A pointer to the frame builder to use for draw operations
 */
MALI_STATIC_INLINE mali_frame_builder *_gles_get_current_draw_frame_builder(gles_context *ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->state.common.framebuffer.current_object);
	return ctx->state.common.framebuffer.current_object->draw_frame_builder;
}

/**
 * @brief Retrieves the frame builder to use for read operations
 * @param ctx A pointer to the gles_context to set as the current OpenGL ES context
 * @return A pointer to the frame builder to use for read operations
 */
MALI_STATIC_INLINE mali_frame_builder *_gles_get_current_read_frame_builder(gles_context *ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->state.common.framebuffer.current_object);
	return ctx->state.common.framebuffer.current_object->read_frame_builder;
}

/**
 * @brief Get the current surface for the given attachment.
 * @param attachment_point the attachment for which to retrieve the surface
 * @return the surface for the framebuffer attachment
 */
mali_surface* _gles_get_attachment_surface(gles_framebuffer_attachment *attachment_point);

/**
 * @brief Gets the number of bits precision for the given value( GL_RED_BITS, etc. )
 * @param fb_obj a framebuffer object to query for sizes
 * @param pname The field-specifier( GL_RED_BITS, GL_DEPTH_BITS, etc. )
 * @return The number of bits precision of the given value
 */
GLint _gles_fbo_get_bits( gles_framebuffer_object* fb_obj, GLenum pname );

/**
 * Purges all attachments of the renderbuffer object from the framebuffer object.
 * This function is typically called when deleting the renderbuffer object.
 * If purging a rb_object that is not attached, this function will do nothing.
 *
 * @param fb_object The framebuffer object to purge references from
 * @param rb_object The renderbuffer object to purge from a framebuffer
 * @return any error produced when detaching fbo attachments (which could involve flushing)
 */

MALI_CHECK_RESULT mali_err_code _gles_internal_purge_renderbuffer_from_framebuffer(struct gles_framebuffer_object* fb_object,
                                                                 struct gles_renderbuffer_object* rb_object );


/**
 * Purges all attachments of a given texture object from the framebuffer object.
 * This function works in the same manner as the renderbuffer version defined above.
 *
 * @param fb_object The framebuffer object to purge references from
 * @param tex_object The texture object to purge from a framebuffer
 * @return any error produced when detaching fbo attachments (which could involve flushing)
 */
MALI_CHECK_RESULT mali_err_code _gles_internal_purge_texture_from_framebuffer(struct gles_framebuffer_object* fb_object,
                                                           struct gles_texture_object* tex_object );


/**
 *  unbinds the bound framebuffer from the gles state, setting it to
 *  the given object, and handles all required reference count changes.
 *  @param ctx The GLES context
 *  @param fb_obj The object you want to bind.
 *  @param fb_obj_id The name corresponding to this object. It should match the object,
 *                   but nothing will crash if it doesn't. It just means we will lie to our users.
 *  @note function assumes texture, renderbuffer and framebuffer list lock is set(!). In that order.
 */
MALI_CHECK_RESULT mali_err_code _gles_internal_bind_framebuffer( struct gles_context *ctx, gles_framebuffer_object* fb_obj, GLuint fb_obj_id );

/**
 * Dereferences (and probably frees) a framebuffer wrapper and its associated object.
 * @param wrapper The wrapper to delete
 * @note this is mainly used by the context deletion system to delete all created framebuffer objects
 * @note You must have the texture list lock, the renderbuffer list lock and the framebuffer list lock set
 *       (in that order) prior to calling this function.
 */
void _gles_framebuffer_object_list_entry_delete(struct gles_wrapper *wrapper);

/**
 * @brief Increments reference count for a framebuffer object.
 * @param fb_obj The pointer to the framebuffer-object we want to reference.
 */
MALI_STATIC_INLINE void _gles_framebuffer_object_addref(gles_framebuffer_object *fb_obj)
{
	MALI_DEBUG_ASSERT_POINTER( fb_obj );
	MALI_DEBUG_ASSERT(_mali_sys_atomic_get( &fb_obj->ref_count ) > 0, ("invalid ref count (%d)", _mali_sys_atomic_get( &fb_obj->ref_count )));
	_mali_sys_atomic_inc( &fb_obj->ref_count );

}

/**
 * @brief Decrement reference count for a framebuffer object, and delete the object if zero.
 * @param fb_obj The pointer to the framebuffer-object we want to dereference.
 * @note Any deref which may free the fbo should ensure that the FBO is flushed first
 */
void _gles_framebuffer_object_deref(gles_framebuffer_object *fb_obj);

#include <m200_backend/gles_m200_rsw.h>

/**
 * Allocates a new Framebuffer Object, but this object is not placed in any list or anything, just returned.
 * @param ctx      The current gles context
 * @param base_ctx The current base context used by the working GLES context
 * @param implicit Set to true if the framebuilders are provided from elsewhere
 *                 Caller is responsible to ensuring that the framebuilder gets a write and draw framebuilder before use.
 * @return A new fresh framebuffer object, or NULL if out of memory ocurred.
 **/
struct gles_framebuffer_object* _gles_framebuffer_object_new( struct gles_context *ctx, mali_base_ctx_handle base_ctx, mali_bool implicit ) MALI_CHECK_RESULT;

/**
 * @brief Setup drawing to an FBO. This function must be called prior to any drawcall, and will
 *        ready all attachments so that they are ready to be used.
 * @param ctx The context to setup
 * @return a GL return code: GL_NO_ERROR, GL_OUT_OF_MEMORY or GL_INVALID_FRAMEBUFFER_OPERATION
 *         if the framebuffer has been framebuffer incomplete.
 **/
GLenum _gles_fbo_internal_draw_setup( gles_context *ctx );


/*****************************************************************************/
/*                                    API functions                          */
/*****************************************************************************/

/**
 * Binds a framebuffer object as the active framebuffer object.
 * @param base_ctx The GL base_ctx.
 * @param framebuffer_object_list The list containing all created framebuffer objects.
 * @param fb_state The state containing framebuffer related stuff
 * @param target - must be GL_FRAMEBUFFER
 * @param framebuffer - the framebuffer to set to.
 * @return An error value as specified in the GL spec.
 * @note This function is a wrapper for glBindFramebuffer
 */
GLenum _gles_bind_framebuffer( struct gles_context *ctx,
                               mali_named_list *framebuffer_object_list,
                               struct gles_framebuffer_state *fb_state,
                               GLenum target,
                               GLuint framebuffer ) MALI_CHECK_RESULT;


/**
 * @brief Deletes framebuffer objects from the context, unbinds the deleted object if bound
 * @param ctx The OpenGL ES context
 * @param n The number of framebuffers to delete
 * @param framebuffers An array of names, each representing a framebuffer name to delete
 */
GLenum _gles_delete_framebuffers( struct gles_context *ctx, GLsizei n, const GLuint *framebuffers );


/**
 * @brief test whether a framebuffer is framebuffer complete or not.
 * @param fb_object the framebuffer object to test.
 * @return Returns the same info as _gles_check_framebuffer_status.
 * @note This function assumes the listlocks for textures, renderbuffers and framebuffers are all set (in that order)
 */
GLenum _gles_framebuffer_internal_complete( gles_framebuffer_object *fb_object ) MALI_CHECK_RESULT;


/**
 * @brief The framebuffer object target is said to be framebuffer complete if it is
 * the window-system-provided framebuffer, or if all the following conditons are true:
 * 1) All framebuffer attachment points are framebuffer attachment complete.
 * 2) There is at least one image attached to the framebuffer.
 * 3) All attached images have the same width and height.
 * 4) The combination of internal formats of the attached images do not violate an
 * 		implementation-dependent set of restrictions.
 * @param framebuffer_object_list The list of framebuffer-objects. Included because the list needs to be locked
 * @param renderbuffer_object_list The list of renderbuffer-objects. Included because the list needs to be locked
 * @param texture_object_list The list of texture-objects. Included because the list needs to be locked
 * @param fb_state The framebuffer state
 * @param target The framebuffer object to check for completeness
 * @return FRAMEBUFFER_COMPLETE_EXT is framebuffer was complete, or one of the following if not
 *         FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT
 *         FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT
 *         FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT
 *         FRAMEBUFFER_INCOMPLETE_FORMATS_EXT
 *         FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT
 *         FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT
 *         FRAMEBUFFER_UNSUPPORTED_EXT
 * @note This function is a wrapper around glCheckFramebufferStatus
 */
GLenum _gles_check_framebuffer_status( struct gles_framebuffer_state *fb_state,
                                       GLenum target,
                                       GLenum *status ) MALI_CHECK_RESULT;

/**
 * @brief Attaches a renderbuffer to the active framebuffer
 * @param fb_state The framebuffer state struct, so we can know which framebuffer is currently bound
 * @param framebuffer_object_list The framebuffer object list; mainly required for mutex locking
 * @param renderbuffer_object_list The renderbuffer object list; used to lookup the renderbuffer to bind
 * @param texture_object_list The texture object list; we might need to lock this one. Seriously.
 * @param target Must be GL_FRAMEBUFFER
 * @param attachment Must be a valis attachment point
 * @param renderbuffertarget Must be GL_RENDERBUFFER
 * @param renderbuffertarget The name we want to attach.
 * @note This function is a wrapper around glFramebufferRenderbuffer
 */
GLenum _gles_framebuffer_renderbuffer( struct gles_framebuffer_state *fb_state,
                                      mali_named_list *framebuffer_object_list,
                                      mali_named_list *renderbuffer_object_list,
                                      mali_named_list *texture_object_list,
                                      GLenum target,
                                      GLenum attachment,
                                      GLenum renderbuffertarget,
                                      GLuint renderbuffer ) MALI_CHECK_RESULT;

/**
 * @brief Attaches a 2d texture to the active framebuffer and set the number of samples
 * @param ctx
 * @param fb_state The framebuffer state struct, so we can know which framebuffer is currently bound
 * @param framebuffer_object_list The framebuffer object list; mainly required for mutex locking
 * @param renderbuffer_object_list The renderbuffer object list; used to lookup the renderbuffer to bind
 * @param texture_object_list The texture object list; we might need to lock this one. Seriously.
 * @param target Must be GL_FRAMEBUFFER
 * @param attachment Must be a valis attachment point
 * @param textarget Must be GL_TEXTURE_2D.
 * @param texture The texture id to attach.
 * @param level The mipmap level you want to attach to.
 * @note This function is a wrapper around glFramebufferTexture2D
 */
GLenum _gles_framebuffer_texture2d_multisample(   struct gles_context *ctx,
		struct gles_framebuffer_state *fb_state,
		mali_named_list *framebuffer_object_list,
		mali_named_list *renderbuffer_object_list,
		mali_named_list *texture_object_list,
		GLenum target,
		GLenum attachment,
		GLenum textarget,
		GLuint texture,
		GLint level,
		GLsizei samples ) MALI_CHECK_RESULT;

/**
 * @brief Attaches a 2d texture to the active framebuffer
 * @param ctx
 * @param fb_state The framebuffer state struct, so we can know which framebuffer is currently bound
 * @param framebuffer_object_list The framebuffer object list; mainly required for mutex locking
 * @param renderbuffer_object_list The renderbuffer object list; used to lookup the renderbuffer to bind
 * @param texture_object_list The texture object list; we might need to lock this one. Seriously.
 * @param target Must be GL_FRAMEBUFFER
 * @param attachment Must be a valis attachment point
 * @param textarget Must be GL_TEXTURE_2D.
 * @param texture The texture id to attach.
 * @param level The mipmap level you want to attach to.
 * @note This function is a wrapper around glFramebufferTexture2D
 */
GLenum _gles_framebuffer_texture2d(   struct gles_context *ctx,
		struct gles_framebuffer_state *fb_state,
		mali_named_list *framebuffer_object_list,
		mali_named_list *renderbuffer_object_list,
		mali_named_list *texture_object_list,
		GLenum target,
		GLenum attachment,
		GLenum textarget,
		GLuint texture,
		GLint level ) MALI_CHECK_RESULT;

/**
 * @brief getter of internal fbo state
 * @param fb_state the framebuffer state
 * @param framebufefr_object_list allows you to lock the framebuffer objects
 * @param target Must be GL_FRAMEBUFFER
 * @param attachment Any legal attachment point
 * @param pname The name of the state data you want to retrieve
 * @params the output parameter where your data will be placed
 */
GLenum _gles_get_framebuffer_attachment_parameter( struct gles_framebuffer_state *fb_state,
                                                   mali_named_list* framebuffer_object_list,
                                                   GLenum target, GLenum attachment, GLenum pname, GLint *params ) MALI_CHECK_RESULT;

#if EXTENSION_DISCARD_FRAMEBUFFER
/**
 * glDiscardFramebufferEXT implementation. Discards specified framebuffer
 * attachments in the given framebuffer.
 *
 * @param ctx               Current context
 * @param fb_state          Framebuffer state
 * @param target            Must be GL_FRAMEBUFFER
 * @param count             Attachment list size
 * @param attachments       List of attachments to discard
 *
 * @return GL_NO_ERROR      if the operation was succesfull,
 *         GL_INVALID_ENUM  if the attachment list contained invalid attachments
 *         GL_INVALID_VALUE if the count is less than zero
 */
GLenum _gles_discard_framebuffer( struct gles_context *ctx,
                                  struct gles_framebuffer_state *fb_state,
                                  GLenum target,
                                  GLsizei count,
                                  const GLenum *attachments );

/**
 * Restores any discarded framebuffer attachments in the target framebuffer.
 * Any discarded attachments should be restored before trying to issue any
 * draw calls on the given framebuffer.
 *
 * @param ctx               Current context
 * @param fb_obj            Framebuffer for which the attachments should be
 *                          restored
 * @param resolve           Flag indicating whether the dirtiness resolution
 *                          should be done immediately or not.
 */
void _gles_framebuffer_restore_discarded_attachments( struct gles_context* ctx, struct gles_framebuffer_object* fb_obj, mali_bool resolve );

/**
 * Queries the framebuffer for any discarded attachments.
 *
 * @param fb_obj Framebuffer to query
 *
 * @return MALI_TRUE if the framebuffer has discarded attachments.
 */
MALI_STATIC_INLINE mali_bool _gles_framebuffer_has_discarded_attachments( struct gles_framebuffer_object* fb_obj )
{
	MALI_DEBUG_ASSERT_POINTER( fb_obj );
	return fb_obj->discarded_attachments != 0 ? MALI_TRUE : MALI_FALSE;
}

/**
 * (EGL fence support)
 *
 * Issue a fence flush. In other words, splits the frame into two halves. Invokes incremental
 * rendering on FBO #0 and if the current FBO is other than #0, flushes it. This is to make
 * sure all unsubmitted draw commands this far are added to current fence (see current_sync in EGL
 * thread state) before replacing the fence with a new one.
 *
 * @param ctx               Current context
 *
 * @return GL_NO_ERROR if successful, GL_OUT_OF_MEMORY otherwise
 */
GLenum _gles_fence_flush( struct gles_context* ctx );

#endif
#endif /* GLES_FRAMEBUFFER_OBJECT_H */

