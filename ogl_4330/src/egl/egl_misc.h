/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_misc.h
 * @brief Query, wait and posting functions
 */

#ifndef _EGL_MISC_H_
#define _EGL_MISC_H_

#include <EGL/mali_egl.h>
#include <base/mali_macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the internal error state
 * @param error the error code to use
 * @param thread_state thread state pointer
 */
void __egl_set_error( EGLint error, void *thread_state );
/* temp test */
/*
void __egl_set_error_temp( EGLint error, void *thread_state, const char* string, unsigned int line );
*/

/**
 * @brief Returns the current error for the current thread
 * @param thread_state thread state pointer
 * @return integer representing the error, EGL_SUCCESS on no error
 */
EGLint _egl_get_error( void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Query various egl info
 * @param dpy the display to query on
 * @param name the identifier to retrieve info for
 * @param thread_state thread state pointer
 * @return null terminated string with info
 * @note Accepted parameters are EGL_CLIENT_APIS, EGL_VENDOR, EGL_VERSION, EGL_EXTENSIONS
 */
const char* _egl_query_string( EGLDisplay __dpy, EGLint name, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Retrieves the proc address of a symbol
 * @param procname the procname to retrieve address of
 * @param thread_state thread state pointer
 * @return proc address of symbol
 */
void (* _egl_get_proc_address( const char *procname, void *thread_state ))( void );

/**
 * @brief Waits for current client API to finish rendering.
 * All rendering calls for the currently bound context, for the current rendering API,
 * made prior to eglWaitClient, are guaranteed to be executed before native rendering
 * calls made after eglWaitClient which affect the surface associated with that
 * context.
 *
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_wait_client( void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Waits for OpenGL ES to finish rendering
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean _egl_wait_GL( void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Waits for the native render to finish
 * @param engine the native engine to wait for
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean _egl_wait_native( EGLint engine, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Blits a surface to the display
 * @param __dpy the display to use
 * @param __draw surface attached to the display that will be blitted
 * @param numrects number of rectangles in the <rects> list
 * @param rects pointer to a list of 4-integer blocks defining rectangles
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_swap_buffers( EGLDisplay __dpy, EGLSurface __draw, EGLint numrects, const EGLint* rects, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Copies the buffer associated with the given surface into a native pixmap
 * @param __dpy the display for which the surface is associated with
 * @param __surface the surface to copy from
 * @param target native pixmap type destination buffer
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_copy_buffers( EGLDisplay __dpy, EGLSurface __surface, EGLNativePixmapType target, void *thread_state ) MALI_CHECK_RESULT;

/**
 * Sets the number of video frames to wait before a swap, the swap interval
 * @param __dpy the display this affects
 * @param interval the number of video frames
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 * @note this only affects the current API context for the current thread
 */
EGLBoolean _egl_swap_interval( EGLDisplay __dpy, EGLint interval, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Binds a surface as a texture for usage in OpenGL ES
 * @param __dpy the display for which the surface is associated
 * @param __surface the surface to use as a texture
 * @param buffer which buffer to use for the rendering, currently only accepted value is EGL_BACK_BUFFER
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_bind_tex_image( EGLDisplay __dpy, EGLSurface __surface, EGLint buffer, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Releases a texture
 * @param __dpy the display for which the surface is associated
 * @param __surface the surface used to create the texture
 * @param buffer the type of buffer used
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_release_tex_image( EGLDisplay __dpy, EGLSurface __surface, EGLint buffer, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Issue a fence flush on client. A fence flush always triggers incremental rendering on the default framebuffer (FBO#0),
 * and flushes the current framebuffer, if it's other than the default one.
 *
 * @param thread_state Thread state pointer
 */
EGLenum _egl_fence_flush( void *thread_state );

#ifdef NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN
void _nx_egl_platform_set_swap_n_scale( EGLDisplay __dpy, EGLSurface __draw, void *thread_state );
void __nx_egl_platform_reset_swap_n_scale( EGLDisplay __dpy, EGLSurface __draw, void *thread_state );
#endif

#ifdef __cplusplus
}
#endif

#endif /* _EGL_MISC_H_ */

