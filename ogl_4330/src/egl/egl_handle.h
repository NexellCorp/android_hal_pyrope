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
 * @file egl_handle.h
 * @brief Adding, Editing, Removing EGL handles
 * (EGLDisplay, EGLConfig, EGLSurface, EGLContext)
 */

#ifndef _EGL_HANDLE_H_
#define _EGL_HANDLE_H_

#include <EGL/mali_egl.h>
#include <base/mali_macros.h>
#include "egl_config_extension.h"

#ifdef EGL_HAS_EXTENSIONS
#include <EGL/mali_eglext.h>

#if EGL_KHR_image_ENABLE
#include "egl_image.h"
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
#include "egl_sync.h"
#endif /* EGL_KHR_reusable_sync_ENABLE */

#endif /* EGL_HAS_EXTENSIONS */

#include "egl_display.h"
#include "egl_context.h"
#include "egl_config.h"
#include "egl_surface.h"
#include "egl_thread.h"

#define HANDLE_EGL_BIT31 (1<<30)
#define HANDLE_EGL_BIT30 (1<<29)
#define HANDLE_EGL_BIT29 (1<<28)

#define HANDLE_EGL_TYPE_MASK (HANDLE_EGL_BIT31|HANDLE_EGL_BIT30|HANDLE_EGL_BIT29)

#define HANDLE_EGL_DISPLAY(handle) ((EGLDisplay)(handle))
#define HANDLE_EGL_SURFACE(handle) ((EGLSurface)(handle|HANDLE_EGL_BIT30))
#define HANDLE_EGL_CONTEXT(handle) ((EGLContext)(handle|HANDLE_EGL_BIT31))
#define HANDLE_EGL_CONFIG(handle)  ((EGLConfig)(handle|(HANDLE_EGL_BIT31|HANDLE_EGL_BIT30)))
#define HANDLE_EGL_IMAGE(handle)   ((EGLImageKHR)(handle|HANDLE_EGL_BIT29))
#if EGL_KHR_reusable_sync_ENABLE
#define HANDLE_EGL_SYNC(handle)    ((EGLSyncKHR)(handle|(HANDLE_EGL_BIT29|HANDLE_EGL_BIT30)))
#endif
#if EGL_KHR_image_system_ENABLE
#define HANDLE_EGL_CLIENT_NAME(handle)      ((EGLClientNameKHR)(handle|(HANDLE_EGL_BIT29|HANDLE_EGL_BIT31)))
#define HANDLE_EGL_IMAGE_NAME(handle)       ((EGLImageNameKHR)(handle|(HANDLE_EGL_BIT29|HANDLE_EGL_BIT30|HANDLE_EGL_BIT31)))
#endif

#define HANDLE_IS_EGL_DISPLAY(handle) (0 == ((handle) & HANDLE_EGL_TYPE_MASK))
#define HANDLE_IS_EGL_SURFACE(handle) (HANDLE_EGL_BIT30 == ((handle) & HANDLE_EGL_TYPE_MASK))
#define HANDLE_IS_EGL_CONTEXT(handle) (HANDLE_EGL_BIT31 == ((handle) & HANDLE_EGL_TYPE_MASK))
#define HANDLE_IS_EGL_CONFIG(handle)  ((HANDLE_EGL_BIT31|HANDLE_EGL_BIT30) == ((handle) & HANDLE_EGL_TYPE_MASK))
#define HANDLE_IS_EGL_IMAGE(handle)   (HANDLE_EGL_BIT29 == ((handle) & HANDLE_EGL_TYPE_MASK) )
#if EGL_KHR_reusable_sync_ENABLE
#define HANDLE_IS_EGL_SYNC(handle)    ((HANDLE_EGL_BIT29|HANDLE_EGL_BIT30) == ((handle) & HANDLE_EGL_TYPE_MASK))
#endif
#if EGL_KHR_image_system_ENABLE
#define HANDLE_IS_EGL_CLIENT_NAME(handle)      ((HANDLE_EGL_BIT29|HANDLE_EGL_BIT31) == ((handle) & HANDLE_EGL_TYPE_MASK))
#define HANDLE_IS_EGL_IMAGE_NAME(handle)       ((HANDLE_EGL_BIT29|HANDLE_EGL_BIT30|HANDLE_EGL_BIT31) == ((handle) & HANDLE_EGL_TYPE_MASK))
#endif

#define HANDLE_EGL_MASK(handle)	 ((u32)handle & ~HANDLE_EGL_TYPE_MASK)

/**
 * @brief Creates the initial handle structures
 * @param display pointer to the display which is to be initialized
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean __egl_create_handles( egl_display *display ) MALI_CHECK_RESULT;

/**
 * @brief Destroy the initial handle structures
 *        Note: This will not clean up any handles already present in the lists.
 *              The lists should be empty before calling this function.
 * @param display pointer to the display containing the handle structures
 */
void __egl_destroy_handles( egl_display* display );

/**
 * @brief Adds a display pointer to the list of display handles
 * @param display the display pointer to add
 * @return an EGLDisplay handle, EGL_NO_DISPLAY on failure
 */
EGLDisplay __egl_add_display_handle( egl_display *display ) MALI_CHECK_RESULT;

/**
 * @brief Adds a config pointer to the list of config handles attached to a display
 * @param config the config pointer to add
 * @param display_handle the handle for which the config is to be attached
 * @return an EGLConfig handle, NULL on failure
 * @note MALI_CHECK_RESULT omitted here, since this is used internally during initialization of configs
 */
EGLConfig  __egl_add_config_handle( egl_config *config, EGLDisplay display_handle );

/**
 * @brief Adds a surface pointer to the list of surface handles attached to a display
 * @param surface the surface pointer to add
 * @param display_handle the handle for which the surface is to be attached
 * @return an EGLSurface handle, EGL_NO_SURFACE on failure
 */
EGLSurface __egl_add_surface_handle( egl_surface *surface, EGLDisplay display_handle ) MALI_CHECK_RESULT;

/**
 * @brief Adds a context pointer to the list of context handles attached to a display
 * @param context the context pointer to add
 * @param display_handle the handle for which the context is to be attached
 * @return an EGLContext handle, EGL_NO_CONTEXT on failure
 */
EGLContext __egl_add_context_handle( egl_context *context, EGLDisplay display_handle ) MALI_CHECK_RESULT;

#if EGL_KHR_image_ENABLE
/**
 * @brief Adds an egl image pointer to the list of egl image pointers
 * @param image the image pointer to add
 * @return an EGLImageKHR handle, EGL_NO_IMAGE_KHR on failure
 */
EGLImageKHR __egl_add_image_handle( egl_image *image ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
/**
 * @brief Adds an egl sync object pointer to the list of egl sync objects
 * @param sync the sync object pointer to add
 * @param display_handle the handle for which the sync object is to be attached
 * @return an EGLSyncKHR handle, EGL_NO_SYNC_KHR on failure
 */
EGLSyncKHR __egl_add_sync_handle( egl_sync *sync, EGLDisplay display_handle ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_reusable_sync_ENABLE */



/**
 * @brief Removes a display handle from the list of display handles
 * @param display the display pointer to remove
 * @param free_display if EGL_TRUE then the handle will be removed, else
 * it will still be valid, but the config, surface and context handles are removed
 * @return EGL_TRUE if found and removed, EGL_FALSE if not found or failed
 * @note this does not free the actual pointer data, only the handle
 * @note this also removes all config, surface and context entries attached to the display
 */
EGLBoolean __egl_remove_display_handle( egl_display *display, EGLBoolean free_display ) MALI_CHECK_RESULT;

/**
 * @brief Removes a config handle from the list of config handles attached to a display
 * @param config the config pointer to remove
 * @param display_handle the display handle for which the config is attached to
 * @return EGL_TRUE if found and removed, EGL_FALSE if not found or failed
 */
EGLBoolean __egl_remove_config_handle( egl_config *config, EGLDisplay display_handle ) MALI_CHECK_RESULT;

/**
 * @brief Removes a surface handle from the list of surface handles attached to a display
 * @param surface the surface pointer to remove
 * @param display_handle the display handle for which the surface is attached to
 * @return EGL_TRUE if found and removed, EGL_FALSE if not found or failed
 */
EGLBoolean __egl_remove_surface_handle( egl_surface *surface, EGLDisplay display_handle ) MALI_CHECK_RESULT;

/**
 * @brief Removes a context handle from the list of context handles attached to a display
 * @param context the context pointer to remove
 * @param display_handle the display handle for which the context is attached to
 * @return EGL_TRUE if found and removed, EGL_FALSE if not found or failed
 */
EGLBoolean __egl_remove_context_handle( egl_context *context, EGLDisplay display_handle ) MALI_CHECK_RESULT;

#if EGL_KHR_image_ENABLE
/**
 * @brief Removes an image handle from the list of images handles
 * @param image the image pointer to remove
 * @return EGL_TRUE on success, EGL_FALSE if not found
 */
EGLBoolean __egl_remove_image_handle( egl_image *image ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
/**
 * @brief Removes a sync object from the list of sync objects
 * @param sync the sync objet pointer to remove
 * @param display_handle the display handle for which the sync object is attached to
 */
EGLBoolean __egl_remove_sync_handle( egl_sync *sync, EGLDisplay display_handle ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_reusable_sync_ENABLE */



/**
 * @brief Looks up a display handle, returning its data
 * @param display_handle display handle to grab data for
 * @return an egl_display pointer, NULL on failure
 */
egl_display* __egl_get_display_ptr( EGLDisplay display_handle ) MALI_CHECK_RESULT;

/**
 * @brief Looks up a config handle, returning its data
 * @param config_handle config handle to grab data for
 * @param display_handle the display handle for which the config is attached to
 * @return an egl_config pointer, NULL on failure
 */
egl_config*  __egl_get_config_ptr( EGLConfig config_handle, EGLDisplay display_handle ) MALI_CHECK_RESULT;

/**
 * @brief Looks up a surface handle, returning its data
 * @param surface_handle surface handle to grab data for
 * @param display_handle the display handle for which the surface is attached to
 * @return an egl_surface pointer, NULL on failure
 */
egl_surface* __egl_get_surface_ptr( EGLSurface surface_handle, EGLDisplay display_handle ) MALI_CHECK_RESULT;

/**
 * @brief Looks up a context handle, returning its data
 * @param context_handle context handle to grab data for
 * @param display_handle the display handle for which the context is attached to
 * @return an egl_context pointer, NULL on failure
 */
egl_context* __egl_get_context_ptr( EGLContext context_handle, EGLDisplay display_handle ) MALI_CHECK_RESULT;

#if EGL_KHR_image_ENABLE
/**
 * get image pointer from image handle
 * @param image_handle image handle
 * @return pointer to image or NULL
 */
MALI_IMPORT egl_image* __egl_get_image_ptr( EGLImageKHR image_handle ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
/**
 * @brief Looks up a sync object handle, returning its data
 * @param sync_handle the sync object handle to grab data for
 * @param display_handle the display handle for which the sync object is attached to
 * @return an egl_sync pointer, NULL on failure
 */
egl_sync* __egl_get_sync_ptr( EGLSyncKHR sync_handle, EGLDisplay display_handle ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_reusable_sync_ENABLE */




/**
 * @brief Looks up a display pointer, returning its handle
 * @param display the display pointer to look up the handle for
 * @return EGLDisplay, EGL_NO_DISPLAY on failure
 */
EGLDisplay __egl_get_display_handle( egl_display *display ) MALI_CHECK_RESULT;

/**
 * @brief Looks up a config pointer, returning its handle
 * @param config the config pointer to look up the handle for
 * @param display_handle the display handle for which the config is attached to
 * @return EGLConfig, NULL on failure
 */
EGLConfig  __egl_get_config_handle( egl_config *config, EGLDisplay display_handle ) MALI_CHECK_RESULT;

/**
 * @brief Looks up a given count of config handles
 * @param config pointer to the array of configs
 * @param display_handle the display handle for which the config is attached to
 * @param count number of config handles to retrieve
 * @return the actual number of retrieved configs
 */
EGLint __egl_get_config_handles( EGLConfig *config, EGLDisplay display_handle, EGLint count );

/**
 * @brief Searches for a config given the config id
 * @param id the config id to search for
 * @param display_handle the display handel for which the config is attached to
 * @return an EGLConfig or NULL if not found
 */
EGLConfig __egl_get_config_handle_by_id( EGLint id, EGLDisplay display_handle ) MALI_CHECK_RESULT;

/**
 * @brief Looks up a surface pointer, returning its handle
 * @param surface the surface pointer to look up the handle for
 * @param display_handle the display handle for which the surface is attached to
 * @return EGLSurface, EGL_NO_SURFACE on failure
 */
EGLSurface __egl_get_surface_handle( egl_surface *surface, EGLDisplay display_handle ) MALI_CHECK_RESULT;

/**
 * @brief Looks up a context pointer, returning its handle
 * @param context the context pointer to look up the handle for
 * @param display_handle the display handle for which the context is attached to
 * @return EGLContext, EGL_NO_CONTEXT on failure
 */
EGLContext __egl_get_context_handle( egl_context *context, EGLDisplay display_handle ) MALI_CHECK_RESULT;

#if EGL_KHR_image_ENABLE
/**
 * get handle of a image
 * @param image image pointer
 * @return image handle
 */
EGLImageKHR __egl_get_image_handle( egl_image *image ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
/**
 * @brief Looks up a sync object pointer, returning its handle
 * @param sync the sync object pointer to look up the handle for
 * @param display_handle the display handle for which the surface is attached to
 * @return EGLSyncKHR handle, EGL_NO_SYNC_KHR on failure
 */
EGLSyncKHR __egl_get_sync_handle( egl_sync *sync, EGLDisplay display_handle ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_reusable_sync_ENABLE */


/**
 * @brief Looks up a native display handle, returning its display handle
 * @param display the native display type / handle to look for
 * @return EGLDisplay, EGL_NO_DISPLAY else
 */
EGLDisplay __egl_get_native_display_handle( EGLNativeDisplayType display ) MALI_CHECK_RESULT;



/**
 * @brief Looks up the first display in the egl main context
 * @return egl_display pointer on success, NULL else
 */
egl_display* __egl_get_first_display_handle( void ) MALI_CHECK_RESULT;


/**
 * @brief Looks through all surfaces on all displays, looking for the given window
 * @param window the window to look for
 * @param EGL_TRUE if found, EGL_FALSE else
 */
EGLBoolean __egl_native_window_handle_exists( EGLNativeWindowType window ) MALI_CHECK_RESULT;

/**
 * @brief Looks through all surfaces on all displays, looking for the given pixmap
 * @param pixmap the pixmap to look for
 * @return EGL_TRUE if found, EGL_FALSE else
 */
EGLBoolean __egl_native_pixmap_handle_exists( EGLNativePixmapType pixmap ) MALI_CHECK_RESULT;

/**
 * @brief Looks through all surface on all displays, looking for the given client buffer
 * @param buffer the buffer to look for
 * @return EGL_TRUE if found, EGL_FALSE else
 */
EGLBoolean __egl_client_buffer_handle_exists( EGLClientBuffer buffer ) MALI_CHECK_RESULT;

/**
 * @brief Releases all surfaces on a given display, by calling egl_destroy_surface
 * @param display_handle the display handle to release surfaces on
 * @param tstate thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE when there are still current surfaces left
 */
EGLBoolean __egl_release_surface_handles( EGLDisplay display_handle, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Releases all contexts on a given display, by calling egl_destroy_context
 * @param display_handle the display handle to release contexts on
 * @param tstate thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE when there are still current contexts left
 */
EGLBoolean __egl_release_context_handles( EGLDisplay display_handle, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

#if EGL_KHR_image_ENABLE
/**
 * @brief Releases all EGLImages which have not been properly released by application
 * @param display_handle display to remove egl images from
 * @param force_release forces release of images if true
 */
void __egl_release_image_handles( mali_named_list * egl_images, EGLDisplay display_handle, EGLBoolean force_release );
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
/**
 * @brief Releases all sync objects which have not been properly released by application
 * @param display_handle display to remove sync objects from
 * @note This will implicitly signal the sync objects before they are released
 */
void __egl_release_sync_handles( EGLDisplay display_handle );
#endif /* EGL_KHR_reusable_sync_ENABLE */

/**
 * @brief Called after a power management event (power loss)
 * @note this will not destroy any contexts, but make them unusable
 * in eglSwapBuffers, eglCopyBuffers and eglMakeCurrent
 */
void __egl_invalidate_context_handles( void );


/**
 * @brief Checks the validity of a display handle, and returns the pointer to it
 * @param handle the display handle to check and return data for
 * @param tstate thread state pointer
 * @return an egl_display on success, NULL else
 * @note this also sets the error EGL_BAD_DISPLAY
 */
egl_display* __egl_get_check_display( EGLDisplay handle, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Checks if the given display struct is initialized
 * @param ptr the display pointer to check
 * @param tstate thread state pointer
 * @return EGL_TRUE if initialized, EGL_FALSE if not
 * @note this also sets the error EGL_NOT_INITIALIZED
 */
EGLBoolean __egl_check_display_initialized( egl_display *ptr, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Checks if the given display struct has NOT begun termination
 * @param ptr the display pointer to check
 * @param tstate thread state pointer
 * @return EGL_TRUE if termination has NOT begun, EGL_FALSE if it has
 * @note this also sets the error EGL_BAD_DISPLAY, and so should only be called
 * in the instances where it is undesirable to operate on a display that is
 * in the process of terminating.
 */
EGLBoolean __egl_check_display_not_terminating( egl_display *ptr, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Checks the validity of the given config handle, and returns a pointer to the data
 * @param handle the config handle to check and return data for
 * @param display_handle the display handle for which the config is attached
 * @param tstate thread state pointer
 * @return an egl_config on success, NULL else
 * @note this also sets the error EGL_BAD_CONFIG
 */
egl_config* __egl_get_check_config( EGLConfig handle, EGLDisplay display_handle, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Checks the validity of the given surface handle, and returns a pointer to the data
 * @param handle the surface handle to check and return data for
 * @param display_handle the display handle for which the surface is attached
 * @param tstate thread state pointer
 * @return an egl_surface on success, NULL else
 * @note this also sets the error EGL_BAD_SURFACE
 */
egl_surface* __egl_get_check_surface( EGLSurface handle, EGLDisplay display_handle, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

/**
 * @brief Checks the validity of the given context handle, and returns a pointer to the data
 * @param handle the context handle to check and return data for
 * @param display_handle the display handle for which the surface is attached
 * @param tstate thread state pointer
 * @return an egl_context on success, NULL else
 * @note this also sets the error EGL_BAD_CONTEXT
 */
egl_context* __egl_get_check_context( EGLContext handle, EGLDisplay display_handle, __egl_thread_state *tstate ) MALI_CHECK_RESULT;

#if EGL_KHR_image_ENABLE
/**
 * @brief Checks the validity of the given image handle, and returns a pointer to the data
 * @param handle the image handle to check and return data for
 * @param tstate thread state pointer
 * @return an egl_image on success, NULL else
 * @note this also sets the error EGL_BAD_PARAMETER
 */
egl_image* __egl_get_check_image( EGLImageKHR handle, __egl_thread_state *tstate ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
/**
 * @brief Checks the validity of the given sync handle, and returns a pointer to the data
 * @param handle the sync object handle to check and return data for
 * @param display_handle the display handle for which the sync object is attached
 * @param tstate thread state pointer
 * @return an egl_sync object pointer on success, NULL else
 */
egl_sync* __egl_get_check_sync( EGLSyncKHR handle, EGLDisplay display_handle, __egl_thread_state *tstate ) MALI_CHECK_RESULT;
#endif /* EGL_KHR_reusable_sync_ENABLE */

/**
 * @brief Checks if an EGLint pointer is NULL
 * @param value the value pointer to check
 * @param error the error to set if the value is NULL
 * @param tstate thread state pointer
 * @return EGL_TRUE if not NULL, EGL_FALSE if NULL
 * @note this also sets the error <error> given as parameter
 */
EGLBoolean __egl_check_null_value( EGLint *value, EGLint error, __egl_thread_state *tstate );

/**
 * @brief Locks the EGL surface object
 * @param display_handle The display handle the surface is attached to
 * @param surface_handle The surface handle to lock
 */
void __egl_lock_surface( EGLDisplay display_handle, EGLSurface surface_handle );

/**
 * @brief Unlocks the EGL surface object
 * @param display_handle The display handle the surface is attached to
 * @param surface_handle The surface handle to unlock
 */
void __egl_unlock_surface( EGLDisplay display_handle, EGLSurface surface_handle );


#endif /* _EGL_HANDLE_H_ */

