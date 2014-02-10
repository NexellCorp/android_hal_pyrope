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
 * @file egl_vg.h
 * @brief OpenVG spesific code.
 */

#ifndef _EGL_VG_H_
#define _EGL_VG_H_

/**
 * @addtogroup eglcore EGL core API
 *
 * @{
 */

/**
 * @defgroup eglcore_gles EGL core OpenVG API
 *
 * @note EGL core OpenVG API
 *
 * @{
 */

#include <VG/openvg.h>
#include <EGL/mali_egl.h>
#include <base/mali_macros.h>
#include "egl_context.h"
#include "egl_thread.h"
#include "egl_config.h"
#include "egl_config_extension.h"
#if EGL_KHR_image_ENABLE
#include "egl_image.h"
#endif


/**
* @brief Initialize the VG library
* @param egl Pointer to the EGL main context
* @return a Mali error code
* @note This function must not call \c __egl_get_main_context, because it is
* called as part of initializing the main context
*/
mali_err_code __egl_vg_initialize( __egl_main_context *egl );

/**
* @brief Free resources used by the VG library
* @param egl Pointer to the EGL main context
* @note This function must not call \c __egl_get_main_context, because it is
* called as part of shutting down the main context.
*/
void __egl_vg_shutdown( __egl_main_context *egl );

/**
 * @brief Acquires a resize of a vg context
 * @param context the context to resize
 * @param width new width in pixels
 * @param height new height in pixels
 * @return EGL_TRUE on success, EGL_FALSE on failure
 * @note a successful resize has to be followed by __egl_vg_context_resize_finish
 */
EGLBoolean __egl_vg_context_resize_acquire( egl_context *context, u32 width, u32 height );

/**
 * @brief Rolls back a context resize
 * @param context the context to roll back
 * @param width width in pixels
 * @param height height in pixels
 */
void __egl_vg_context_resize_rollback( egl_context *context, u32 width, u32 height );

/**
 * @brief Finishes a context resize
 * @param context the context to finish resize for
 * @param width width in pixels
 * @param height height in pixels
 */
void __egl_vg_context_resize_finish( egl_context *context, u32 width, u32 height );

/**
 * @brief Creates a vg context
 * @param config the egl_config to use
 * @param share_list the shared context
 * @param client_version client version to request
 * @param tstate the wanted thread state
 * @return an egl_context, NULL on failure
 */
egl_context* __egl_vg_create_context( egl_config *config, egl_context *share_list, EGLint client_version, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Releases a vg context
 * @param ctx the context to release
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_vg_release_context( egl_context *ctx );

/**
 * @brief Sets the vg context not current
 * @param tstate current thread state
 * @note implicitly finishes rendering
 */
void __egl_vg_make_not_current( __egl_thread_state *tstate );

/**
 * @brief Makes a vg context current
 * @param ctx the context to make current
 * @param draw the surface to make current
 * @param lock_client_buffer toggle locking of client buffer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_vg_make_current( egl_context *ctx, egl_surface *draw, EGLBoolean lock_client_buffer, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Removes a framebuilder from the context
 * @param tstate the current thread state
 * @note finishes the current frame, then sets the framebuilder to NULL
 */
void __egl_vg_remove_framebuilder_from_client_ctx( __egl_thread_state *tstate );

/**
 * @brief Sets a new vg framebuilder
 * @param surface the surface which contains the frame builder
 * @param tstate the current thread state
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_vg_set_framebuilder( egl_surface *surface, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Flushes a vg context
 * @param tstate thread state for which to flush the context
 */
void __egl_vg_flush( __egl_thread_state *tstate );

/**
 * @brief Finishes a vg context
 * @param tstate thread state for which to finish the context
 */
void __egl_vg_finish( __egl_thread_state *tstate );

/**
 * @brief Fills in VG data to an EGLImage
 * @param context egl context
 * @param buffer the VG handle to the resource to use
 * @param image the EGLimage to fill in
 * @return EGL_SUCCESS on success, some egl error code otherwise
 */
EGLint __egl_vg_setup_egl_image( egl_context *context, VGImage buffer, struct egl_image *image );

/**
 * @brief Retrieves a proc address from VG
 * @param tstate the current thread state
 * @param procname procname to grab
 * @return pointer to proc
 */
void (* __egl_vg_get_proc_address( __egl_thread_state *tstate, const char *procname ))(void);

/**
 * @brief Posts a pbuffer surface (created with eglCreatePbufferFromClientBuffer)
 * @param surface the surface to post
 * @param client_buffer client buffer to post into
 * @param color_attachment pointer to the surface  
 * @param tstate_api thread state api to use
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_vg_post_to_pbuffer_surface( egl_surface *surface,
															EGLBoolean client_buffer,
                                             mali_surface *color_attachment,
                                             __egl_thread_state_api *tstate_api );

/**
 * @brief Creates a pbuffer from a given client buffer
 * @param dpy pointer to display surface belongs to
 * @param buffer client buffer resource
 * @param config config which surface is created with
 * @param attrib_list attribute list surface is created with
 * @param tstate the current thread state
 * @return egl_surface pointer on success, NULL in failure
 * @note sets the error state on failure
 */
egl_surface* __egl_vg_create_pbuffer_from_client_buffer( egl_display *dpy,
                                                         EGLClientBuffer buffer,
                                                         egl_config *config,
                                                         const EGLint *attrib_list,
                                                         __egl_thread_state *tstate );

/**
 * @brief dereferences vg_image for surfaces that have been created with __egl_vg_create_pbuffer_from_client_buffer()
 * @param tstate the current thread state
 * @param surface pbuffer surface that have a vg_image attached
 */
void __egl_vg_destroy_pbuffer_from_client_buffer( egl_surface *surface, __egl_thread_state *tstate );

/** @} */ /* end group eglcore_vg */
/** @} */ /* end group eglcore */


#endif /* _EGL_VG_H_ */


