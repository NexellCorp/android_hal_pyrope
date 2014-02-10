/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_FBO_BINDINGS_H
#define GLES_FBO_BINDINGS_H

#include "gles_base.h"
#include <shared/mali_linked_list.h>

struct gles_framebuffer_object;
struct gles_framebuffer_attachment;
struct mali_render_attachment;
struct mali_surface;
struct mali_frame_builder;


/** @brief Allocates and initializes a mali_linked_list and returns it. This list will hold gles_fbo_binding nodes.
 *  @return A new mali_linked_list if succesful, NULL if not
 */
MALI_CHECK_RESULT mali_linked_list *_gles_fbo_bindings_alloc( void );

/** @brief Frees the list and its contents. The list must be empty when this function is called.
 *  This is typically called when the texture/renderbuffer object is free'd.
 *  @param list The list to free and update fbos
 */
void _gles_fbo_bindings_free( mali_linked_list *list );

/** @brief Goes through the linked list and updates all entries' surface pointer, and also calls _gles_fbo_bindings_flag_completeness_dirty.
 *  @note It is an error to call this function if the surface attached to the FBO has not actually changed.
 *  @param list The linked list of gles_fbo_bindings to update
 */
void _gles_fbo_bindings_surface_changed( mali_linked_list *list );

/** @brief Goes through the linked list and updates all entries' fbo to completeness_dirty = MALI_TRUE and the corresponding attachment_points completeness_dirty = MALI_TRUE
 *  This function is called every time the object holding this list is constraint modified and is used to notify the FBOs that this happened.
 *  @param list The linked list of gles_fbo_bindings to update
 */
void _gles_fbo_bindings_flag_completeness_dirty( mali_linked_list *list );

/** @brief Adds a new fbo binding to the list
 *  @param list The list to add the connection to
 *  @param fbo The pointer to the fbo this list is to remember
 *  @param attachment_point The attachment point in the FBO to bind to
 *  @return may run out of memory with a MALI_ERR_OUT_OF_MEMORY, otherwise MALI_ERR_NO_ERROR
 */
MALI_CHECK_RESULT mali_err_code _gles_fbo_bindings_add_binding( mali_linked_list *list, struct gles_framebuffer_object *fbo,  struct gles_framebuffer_attachment* attachment_point );

/** @brief Removes a specified fbo binding in the linked list. This will also flag the given FBO and attachment point with completeness_dirty=TRUE.
 *  This function is called when the texture/renderbuffer is detached from the FBO in question
 *  It is explicitly not called when the texture/renderbuffer is deleted, as at that point there are no more refcounts to the object
 *  and thus also no more fbos requiring their connections to be severed.
 *  @param list The list to remove the connection from
 *  @param fbo The fbo to remove from this list (only where the fbo and attachment pointer is matching will anything be removed)
 *  @param attachment_point The attachment point on the FBO to remove
 */
void _gles_fbo_bindings_remove_binding( mali_linked_list *list, struct gles_framebuffer_object *fbo,  struct gles_framebuffer_attachment* attachment_point );

#endif
