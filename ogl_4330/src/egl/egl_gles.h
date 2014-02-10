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
 * @file egl_gles.h
 * @brief OpenGL ES spesific code.
 */

#ifndef _EGL_GLES_H
#define _EGL_GLES_H

/**
 * @addtogroup eglcore EGL core API
 *
 * @{
 */

/**
 * @defgroup eglcore_gles EGL core OpenGL ES API
 *
 * @note EGL core OpenGL ES API
 *
 * @{
 */

#include <opengles/gles_base.h>
#include <base/mali_macros.h>
#include "egl_context.h"
#include "egl_thread.h"
#include "egl_config.h"

struct egl_image;

/**
 * @brief Initialize the GLES library
 * @param egl Pointer to the EGL main context
 * @return a Mali error code
 * @note This function must not call \c __egl_get_main_context, because it is
 * called as part of initializating the main context
 */
mali_err_code __egl_gles_initialize( __egl_main_context *egl );

/**
 * @brief Free resources used by the GLES library
 * @param egl Pointer to the EGL main context
 * @note This function must not call \c __egl_get_main_context, because it is
 * called as part of shutting down the main context.
 */
void __egl_gles_shutdown( __egl_main_context *egl );

/**
 * @brief Finishes a resize of a context
 * @param context the context to finish resize for
 * @param width width in pixels
 * @param height height in pixels
 */
void __egl_gles_context_resize_finish( egl_context *context, u32 width, u32 height );

/**
 * @brief Creates a gles context
 * @param config the egl_config to use
 * @param share_list shared context
 * @param client_version wanted client versin of context
 * @param robustness_enabled if context robustness is enabled or not
 * @param reset_notification_strategy the reset notification strategy if robustness is enabled
 * @param tstate the wanted thread state
 * @return egl_context pointer or EGL_NO_CONTEXT on error.
 */
egl_context* __egl_gles_create_context( egl_config *config, egl_context *share_list, EGLint client_version, EGLBoolean robustness_enabled, EGLint reset_notification_strategy, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Releases a gles context
 * @param ctx the context to release
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_gles_release_context( egl_context *ctx, void *thread_state );

/**
 * @brief Sets the gles context not current
 * @param tstate current thread state
 * @note implicitly finishes rendering
 */
void __egl_gles_make_not_current( __egl_thread_state *tstate );

/**
 * @brief Makes a gles context current
 * @param ctx the context to make current
 * @param draw the draw surface to make current
 * @param read the read surface to make current
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_gles_make_current( egl_context *ctx, egl_surface *draw, egl_surface* read, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Removes a gles framebuilder from the context
 * @param tstate current thread state
 * @note finishes the current frame, then sets the framebuilder to NULL
 */
void __egl_gles_remove_framebuilder_from_client_ctx( __egl_thread_state *tstate );

/**
 * @brief Sets a new gles framebuilder
 * @param surface the surface which contains the frame builder
 * @param tstate the current thread state
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_gles_set_framebuilder( egl_surface *surface, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Binds a surface as an OpenGL ES texture
 * @param surface the surface to bind as a texture
 * @param tstate current thread state
 * @return EGL_TRUE on success, EGL_FALSE on failure
 * @note this currently resets the GL ES error state
 */
EGLBoolean __egl_gles_bind_tex_image( egl_surface *surface, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Releases a surface bound as an OpenGL ES texture
 * @param surface the surface to unbind as a texture
 * @param tstate current thread state
 */
void __egl_gles_unbind_tex_image( egl_surface *surface, __egl_thread_state *tstate );

/**
 * @brief Flushes GLES
 * @param flush_all Flushes all current framebuilders, i.e. both egl-fb and fbo-fb.
 * @param tstate current thread state
 */
void __egl_gles_flush( __egl_thread_state *tstate, mali_bool flush_all );

/**
 * @brief Finishes GLES
 * @param tstate current thread state
 */
void __egl_gles_finish( __egl_thread_state *tstate );

/**
 * @brief Fills in GLES data to an EGLImage
 * @param context egl context
 * @param target the kind of resource (texture, renderbuffer) to use
 * @param buffer the GLES handle to the resource to use
 * @param level the mipmap level to use
 * @param image the EGLimage to fill in
 * @return EGL_SUCCESS on success, some egl error code otherwise
 */
EGLint __egl_gles_setup_egl_image( egl_context *context, EGLenum target, EGLClientBuffer buffer, u32 level, struct egl_image *image ) MALI_CHECK_RESULT;

/**
 * @brief Binds an EGLImage as an OpenGL ES texture target
 * @param target texture target
 * @param image the EGLImage handle to bind as texture target
 */
void __egl_gles_image_target_texture_2d( GLenum target, GLeglImageOES image );

#ifdef EXTENSION_EGL_IMAGE_OES_ENABLE
#ifdef EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
#ifdef EGL_MALI_GLES
/**
 * @brief Binds an EGLImage as an OpenGL ES render buffer
 * @param target render buffer target
 * @param image the EGLImage handle to bind as render buffer target
 */
void __egl_gles_image_target_renderbuffer_storage( GLenum target, GLeglImageOES image );
#endif
#endif
#endif

/**
 * @brief Retrieves a proc address from GLES
 * @param procname procname to grab
 * @return pointer to proc
 */
void (* __egl_gles_get_proc_address( __egl_thread_state *tstate, const char *procname ))(void);

/*
 *
 */
EGLenum __egl_gles_fence_flush( __egl_thread_state *tstate );
/** @} */ /* end group eglcore_gles */
/** @} */ /* end group eglcore */

#endif /* _EGL_GLES_H */

