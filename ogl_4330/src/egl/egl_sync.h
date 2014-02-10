/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_sync.h
 * @brief EGL_KHR_{reusable,fence}_sync extension.
 */

#ifndef _EGL_SYNC_H_
#define _EGL_SYNC_H_

/**
 * @addtogroup egl_extension EGL Extensions
 *
 * @{
 */

/**
 * @defgroup egl_extension_sync EGL_KHR_{reusable,fence}_sync extension
 *
 * @note EGL_KHR_{reusable,fence}_sync extension
 *
 * @{
 */

#include <base/mali_sync_handle.h>
#include <EGL/mali_egl.h>
#include <EGL/mali_eglext.h>
#include "egl_display.h"

#if MALI_EXTERNAL_SYNC
#include <sync/mali_external_sync.h>
#endif /* MALI_EXTERNAL_SYNC */

#ifdef __cplusplus
extern "C" {
#endif

#define ENABLE_SYNC_DEBUG 0

#if ENABLE_SYNC_DEBUG
#define MALI_TID _mali_sys_get_tid()
#define PRINT_NODE(msg, x) _mali_sys_printf("[thread:%p] %s: %s egl_sync_node:%p (egl_sync:%p, mali_sync_handle:%p)\n", MALI_TID, MALI_FUNCTION, msg, x, (x)->sync, (x)->sync_handle)
#define PRINT_SYNC(msg, x) _mali_sys_printf("[thread:%p] %s: %s egl_sync:%p (egl_sync_node:%p, mali_sync_handle:%p, status %s)\n", MALI_TID, MALI_FUNCTION, msg, x, (x)->fence_chain, (x)->fence_chain ? (x)->fence_chain->sync_handle : NULL, (x)->status == EGL_UNSIGNALED_KHR ? "EGL_UNSIGNALED_KHR" : (x)->status == EGL_SIGNALED_KHR ? "EGL_SIGNALED_KHR" : "<invalid>" )
#define PRINT_NODE_REFS(msg, x, refs) _mali_sys_printf("[thread:%p] %s: %s egl_sync_node:%p (egl_sync:%p, mali_sync_handle:%p) refcount is %d\n", MALI_TID, MALI_FUNCTION, msg, x, (x)->sync, (x)->sync_handle, refs)
#define PRINT_SYNC_REFS(msg, x, refs) _mali_sys_printf("[thread:%p] %s: %s egl_sync:%p (egl_sync_node:%p, mali_sync_handle:%p, status %s), refcount is %d\n", MALI_TID, MALI_FUNCTION, msg, x, (x)->fence_chain, (x)->fence_chain ? (x)->fence_chain->sync_handle : NULL, (x)->status == EGL_UNSIGNALED_KHR ? "EGL_UNSIGNALED_KHR" : (x)->status == EGL_SIGNALED_KHR ? "EGL_SIGNALED_KHR" : "<invalid>", refs )
#define SYNC_DEBUG( X ) X
#else
#define SYNC_DEBUG( X )
#endif

struct egl_sync;
struct egl_surface;

/** Fence sync node-entry, ultimately making a fence-chain */
typedef struct egl_sync_node_t
{
	struct egl_sync_node_t      *parent;
	struct egl_sync_node_t      *child;
	struct egl_sync             *sync;
	mali_sync_handle             sync_handle;
	mali_bool                    current;
} egl_sync_node;

#if EGL_ANDROID_native_fencesync_ENABLE
typedef struct egl_native_fence_obj_t
{
	int 						fence_fd;
} egl_native_fence_obj;
#endif

/** EGLSyncKHR data */
typedef struct egl_sync
{
	EGLDisplay                   dpy;
	EGLenum                      status;
	EGLenum                      condition;
	EGLenum                      type;
	mali_lock_handle             lock;
	mali_atomic_int              references;
	EGLBoolean                   valid;
	EGLBoolean                   destroy;
	egl_sync_node               *fence_chain; /**< Chain of egl_sync_node's which is to be flushed on wait */
#if EGL_ANDROID_native_fencesync_ENABLE
	egl_native_fence_obj        *native_fence_obj; /*pointer to where android_native_fence_obj is stored*/
#endif
	mali_mutex_handle            fence_mutex; /**< Guarantees multithreaded access to the chain */
} egl_sync;

/**
 * @brief Creates a new sync
 * @param dpy display to use
 * @param type type of sync object to create
 * @param attrib_list pointer to list of attributes
 * @param thread_state pointer to thread state
 * @return Valid on EGLSyncKHR handle on success, EGL_NO_SYNC_KHR on failure
 * @note only EGL_SYNC_REUSABLE_KHR is a recognized type
 */
EGLSyncKHR _egl_create_sync_KHR( EGLDisplay dpy, EGLenum type, const EGLint *attrib_list, void *thread_state );

/**
 * @brief Destroys a sync (internal version without param checking)
 * @param sync pointer to sync object
 */
void _egl_destroy_sync( egl_sync *sync  );

/**
 * @brief Destroys a sync
 * @param dpy display sync object is attached to
 * @param sync_handle sync handle object
 * @param thread_state pointer to thread state
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_destroy_sync_KHR( EGLDisplay dpy, EGLSyncKHR sync_handle, void *thread_state );

/**
 * @brief Waits for a sync object to be in signaled state
 * @param dpy display sync object is attached to
 * @param sync_handle sync handle object
 * @param flags additional flags
 * @param timeout timeout in nanoseconds (10^-9 seconds)
 * @param thread_state pointer to thread state
 * @return condition on wait, either EGL_TIMEOUT_EXPIRED_KHR or EGL_CONDITION_SATISFIED_KHR
 */
EGLint _egl_client_wait_sync_KHR( EGLDisplay dpy, EGLSyncKHR sync_handle, EGLint flags, EGLTimeKHR timeout, void *thread_state );

/**
 * @brief Signals a sync object
 * @param dpy display sync object is attached to
 * @param sync_handle sync handle object
 * @param mode new mode for sync object
 * @param thread_state pointer to thread state
 * @return EGL_TRUE on success, EGL_FALSE on failure
 * @note will wait for any blocking wait threads if state changes from signaled to unsignaled,
 * so that signals will not be missed
 */
EGLBoolean _egl_signal_sync_KHR( EGLDisplay dpy, EGLSyncKHR sync_handle, EGLenum mode, void *thread_state );

/**
 * @brief Retrieves a sync object attribute
 * @param dpy display sync object is attached to
 * @param sync_handle sync handle object
 * @param attribute attribute to query for
 * @param value returned value for given attribute
 * @param thread_state pointer to thread state
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_get_sync_attrib_KHR( EGLDisplay dpy, EGLSyncKHR sync_handle, EGLint attribute, EGLint *value, void *thread_state );

/**
 * @brief Removes a sync_node from any fence chain, flushes
 * it's attached sync_handle and deletes allocated resources.
 * @param n The sync_node to be destroyed.
 */
void _egl_sync_destroy_sync_node( egl_sync_node *n );

/**
 * @brief interface for adding jobs to current bound surface's sync_handle
 * @return The sync_handle to current bound surface's sync_node, or MALI_NO_HANDLE if no current surface is set.
 */
MALI_IMPORT mali_sync_handle _egl_sync_get_current_sync_handle( void );

#if EGL_ANDROID_native_fencesync_ENABLE
/*
 * @brief duplicate the file descriptor stored in egl sync object
 * @param dpy display sync object is attached to
 * @param sync_handle sync handle object
 * @param thread_state pointer to thread state
 * @return new file descriptor
 * @note: there should be blocking wait in this function, if EGL_SYNC_NATIVE_FENCE_FD_ANDROID attribute
 * of sync object is EGL_NO_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID, is returned
 */
EGLint _egl_dup_native_fence_ANDROID( EGLDisplay dpy, EGLSyncKHR sync_handle, void *thread_state );
#endif
#if MALI_EXTERNAL_SYNC == 1
MALI_IMPORT void _egl_sync_notify_job_create(void *data);
MALI_EXPORT void _egl_sync_notify_job_start(mali_bool success, mali_fence_handle fence, void *data);

#endif

#ifdef __cplusplus
}
#endif

/** @} */ /* end group egl_extension_sync */

/** @} */ /* end group egl_extension */

#endif /* _EGL_SYNC_H_ */
