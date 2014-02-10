/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_thread.h
 * @brief Handling of thread state and data.
 */

#ifndef _EGL_THREAD_H_
#define _EGL_THREAD_H_

#include <EGL/mali_egl.h>
#include <base/mali_macros.h>
#include "egl_surface.h"
#include "egl_context.h"
#include "egl_display.h"
#include "egl_main.h"
#include "egl_sync.h"
#if MALI_EXTERNAL_SYNC == 1 
#include "mali_fence_sync.h"
#endif
/**
 * a structure containing the currently bound contexts, surfaces, displays and api
 */
typedef struct __egl_thread_state_api
{
	egl_display *display;
	egl_surface *draw_surface;
	egl_surface *read_surface;
	egl_context *context;
} __egl_thread_state_api;

/**
 * The thread state.
 * This is the data that is local to a thread.
 * A thread may have two simultaneous contexts of OpenVG and OpenGL ES.
 */
typedef struct __egl_thread_state
{
	__egl_thread_state_api *api_vg;
	__egl_thread_state_api *api_gles;
	__egl_main_context     *main_ctx;
	EGLenum                api_current;
	EGLint                 error;
	u32                    id;
	void                   *context_vg;
	void                   *context_gles;

#if MALI_ANDROID_API > 12
	mali_base_worker_handle *worker;
#endif
#if defined( EGL_SURFACE_LOCKING_ENABLED )
	mali_base_worker_handle *worker_surface_lock;
#endif

	egl_sync_node *current_sync;                   /**< The currently bound sync node. Used for fence sync chains. */
#if MALI_EXTERNAL_SYNC == 1
	struct mali_fence_sync *current_mali_fence_sync;      /**< The mali_fence_sync object, owned by EGL, used for store information of 
													 accumulated_fid and pending jobs to be added to accumlated_fid*/
#endif
} __egl_thread_state;

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Returns the current thread state api for the given thread state
 * @param tstate given thread state
 * @param api pointer to int that is either assigned EGL_OPENVG_API or EGL_OPENGL_ES_API if not NULL
 * @return an __egl_thread_state_api pointing to the current rendering api for the given thread
 */
__egl_thread_state_api* __egl_get_current_thread_state_api( __egl_thread_state *tstate, EGLenum *api ) MALI_CHECK_RESULT;

/**
 * @brief Gets the current active thread state structure.
 * @param mutex_action the mutex action to take
 * @return the current egl state for this thread or NULL on failure
 */
__egl_thread_state* __egl_get_current_thread_state( __main_mutex_action mutex_action ) MALI_CHECK_RESULT;

/**
 * @brief Gets the current active thread state structure without doing a mutex operation.
 * @return the current egl state for this thread or NULL on failure
 */
__egl_thread_state* __egl_get_current_thread_state_no_mutex( void ) MALI_CHECK_RESULT;

/**
 * @brief Tries to retrieve the current thread state
 * @return the current thread state, or NULL on failed
 * @note this uses the mutex action EGL_MAIN_MUTEX_NOP
 */
__egl_thread_state* __egl_get_check_thread_state( void ) MALI_CHECK_RESULT;

/**
 * @brief Releases the current thread state, and changes the mutex action
 * @param mutex_action the mutex action to take
 */
void __egl_release_current_thread_state( __main_mutex_action mutex_action );

/**
 * @brief Releases the currently bound thread.
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_release_thread( void ) MALI_CHECK_RESULT;

#ifdef __cplusplus
}
#endif


#endif /* _EGL_THREAD_H_ */

