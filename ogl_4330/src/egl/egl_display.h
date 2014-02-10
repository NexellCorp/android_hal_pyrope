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
 * @file egl_display.h
 * @brief Handling of EGLDisplay
 */

#ifndef _EGL_DISPLAY_H_
#define _EGL_DISPLAY_H_

/**
 * @addtogroup eglcore EGL core API
 *
 * @{
 */

/**
 * @defgroup eglcore_display EGL core display API
 *
 * @note EGL core display API
 *
 * @{
 */

#include "egl_config_extension.h"
#include <EGL/mali_egl.h>
#include <shared/mali_named_list.h>
#include <base/mali_macros.h>
#if EGL_KHR_image_system_ENABLE
#include "egl_image_system.h"
#endif

typedef enum {
	EGL_NATIVE_DISPLAY_ROTATE_0   = 1,
	EGL_NATIVE_DISPLAY_ROTATE_90  = 2,
	EGL_NATIVE_DISPLAY_ROTATE_180 = 3,
	EGL_NATIVE_DISPLAY_ROTATE_270 = 4,
	EGL_NATIVE_DISPLAY_FLIP_X     = 5,
	EGL_NATIVE_DISPLAY_FLIP_Y     = 6
} egl_native_display_orientation;

/** Bitwise flags that capture the state of the EGLDisplay */
typedef enum {
	EGL_DISPLAY_NONE        = 0x0, /** neither initialized, nor terminating */
	EGL_DISPLAY_INITIALIZED = 0x1, /** Initialized/not fully terminated (coexists with TERMINATING) */
	EGL_DISPLAY_TERMINATING = 0x2  /** In the process of terminating, but not fully terminated */
} egl_display_flag;

/** structure holding the platform native display format */
typedef struct egl_display_native_format
{
	u32 red_size, green_size, blue_size;        /**< Holds the red, green, blue color component sizes */
	u32 red_offset, green_offset, blue_offset;  /**< Holds the red, green, blue color offsets */
} egl_display_native_format;

struct egl_config;
/**
 * @brief represents the internal structure of an EGLDisplay handle
 */
typedef struct egl_display
{
	EGLNativeDisplayType      native_dpy;
    EGLBoolean                default_dpy;
	egl_display_native_format native_format;
	EGLint                    orientation;
	egl_display_flag          flags;
	mali_named_list           *config;
	mali_named_list           *context;
	mali_named_list           *surface;
	mali_named_list           *sync;
	EGLint                    num_configs;
	struct egl_config         *configs;
#if EGL_KHR_image_system_ENABLE
	void                      *sem_client_handles;
	egl_client_handles        *client_handles;
	void                      *sem_egl_image_names;
	egl_image_name_handles    *shared_image_names;
#endif
} egl_display;

/**
 * @brief Returns a valid display handle
 * @param display a native display type
 * @param thread_state thread state pointer
 * @return a valid display handle on success, EGL_NO_DISPLAY on failure
 */
EGLDisplay _egl_get_display( EGLNativeDisplayType display, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Return the current display for the current thread and current API
 * @param thread_state thread state pointer
 * @return valid display handle, EGL_NO_DISPLAY on failure
 * @note mali is initialized in here if this is the first time a display is successfully initialized
 */
EGLDisplay _egl_get_current_display( void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Initializes a display previously retrieved by call to eglGetDisplay
 * @param __dpy a valid display handle (retrieved by eglGetDisplay)
 * @param major if not NULL, set to the major version of EGL
 * @param minor if not NULL, set to the minor version of EGL
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean _egl_initialize( EGLDisplay __dpy, EGLint *major, EGLint *minor, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Terminates a display and marks all the attached resources for deletion
 * @param __dpy a valid display handle
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE else
 * @note If resources are no longer current to any display, and this is the
 *       last display to be terminated, then mali is shutdown here.
 *       Otherwise, mali is shutdown when the last display has context+surface
 *       unbound from it.
 */
EGLBoolean _egl_terminate( EGLDisplay __dpy, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Releases a display
 * @param dpy the display to release
 * @param free_display remove the display handle if EGL_TRUE
 */
void __egl_release_display( egl_display *dpy, EGLBoolean free_display );

/**
 * @brief Free all display and their associated contexts/surfaces
 */
void __egl_free_all_displays( void );

/** @} */ /* end group eglcore_display */

/** @} */ /* end group eglcore */

#endif /* _EGL_DISPLAY_H_ */

