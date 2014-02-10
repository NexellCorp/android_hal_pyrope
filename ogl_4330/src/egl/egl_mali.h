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
 * @file egl_mali.h
 * @brief Mali / framebuilder specific code.
 */

#ifndef _EGL_MALI_H_
#define _EGL_MALI_H_

#include <base/mali_macros.h>
#include "egl_surface.h"
#include "egl_main.h"
#include "egl_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mali_surface;

/**
 * @brief Resizes an EGL Surface
 * @param surface the surface to be resized
 * @param width the new width of surface
 * @param height the new height of surface
 * @param tstate thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean __egl_mali_resize_surface( egl_surface *surface, u32 width, u32 height, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * Adds a readback on the internal target to the current frame
 * @param surface the surface to readback into
 * @param m_surf_preserve the mali_surface to read from
 * @param old_width
 * @param old_height
 * @param usage usage flags for readback
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean __egl_mali_readback_surface( egl_surface *surface, mali_surface *m_surf_preserve, u32 usage, u16 old_width, u16 old_height );

/**
 * @brief Waits for the framebuilder color attachment
 * @param tstate_api thread state api containing the surface
 * @note this function is to be used when waiting for the frame complete callback to finish
 */
void __egl_mali_busy_wait( __egl_thread_state_api *tstate_api );

/**
 * @brief Creates and returns a new frame builder
 * @param base_ctx the base context
 * @param config the config to use
 * @param num_frames the number of internal frames in the frame builder swap chain
 * @param num_bufs the number of render buffers in the swap chain
 * @param buffers array of mali_surface pointers, containing target buffers for the swap chain
 * @param type one of MALI_FRAME_BUILDER_TYPE_EGL_WINDOW, MALI_FRAME_BUILDER_TYPE_EGL_PBUFFER, MALI_FRAME_BUILDER_TYPE_EGL_PIXMAP (controls tie-break flip compared to window surfaces)
 * @return a framebuilder on success, NULL on failure
 */
mali_frame_builder *__egl_mali_create_frame_builder( mali_base_ctx_handle base_ctx,
                                                     egl_config *config,
                                                     u32 num_frames,
                                                     u32 num_bufs,
                                                     struct mali_surface** buffers,
                                                     mali_bool readback_after_flush,
													 enum mali_frame_builder_type type
                                                     ) MALI_CHECK_RESULT;

/**
 * @brief Destroys a given frame builder
 * @param frame_builder the frame builder to destroy
 */
void __egl_mali_destroy_frame_builder( egl_surface *surface );

/**
 * @brief Sets up a new frame in the framebuilder residing in the surface
 * @param surface the surface containing the frame builder
 * @param set_framebuilder if EGL_TRUE sets the current framebuilder for gles/vg
 * @param tstate current thread state
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_mali_begin_new_frame( egl_surface *surface, EGLBoolean set_framebuilder, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Handles flushing and rendering of window surfaces
 * @param surface surface to render
 * @param tstate the thread state to use
 * @param tstate_api the thread state api to use
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean __egl_mali_post_to_window_surface( egl_surface *surface, __egl_thread_state *tstate, __egl_thread_state_api *tstate_api ) MALI_CHECK_RESULT;

/**
 * @brief Renders a surface into a pixmap
 * @param surface surface to render
 * @param target pixmap to render into
 * @param remap_pixmap if EGL_TRUE, call down to platform layer to remap the pixmap, otherwise use what is currently in place
 * @param tstate the thread state to use
 * @param tstate_api the thread state api to use
 * @note depending on platform layer, this can involve blitting if not rendering directly into pixmap
 */
EGLBoolean __egl_mali_render_surface_to_pixmap( egl_surface *surface, EGLNativePixmapType target, EGLBoolean remap_pixmap, __egl_thread_state *tstate, __egl_thread_state_api *tstate_api ) MALI_CHECK_RESULT;

/**
 * @brief Renders the content of a pixmap into the surface
 * @param surface surface to render
 * @param from_pixmap if MALI_TRUE, content is read from client buffer inside surface
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean __egl_mali_render_pixmap_to_surface( egl_surface *surface ) MALI_CHECK_RESULT;

/**
 * @brief Handles flushing and rendering of pbuffer surfaces
 * @param surface surface to render
 * @param tstate the thread state to use
 * @param tstate_api the thread state api to use
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean __egl_mali_post_to_pbuffer_surface( egl_surface *surface, __egl_thread_state *tstate, __egl_thread_state_api *tstate_api ) MALI_CHECK_RESULT;


#ifdef __cplusplus
}
#endif

#endif /* _EGL_MALI_H_ */


