/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_image.h
 * @brief EGLImage extension functionality
 */

#ifndef _EGL_IMAGE_H_
#define _EGL_IMAGE_H_

/**
 * @addtogroup eglcore EGL core API
 *
 * @{
 */

/**
 * @defgroup eglcore_image EGL core EGLImageKHR API
 *
 * @note EGL core EGLImageKHR API
 *
 * @{
 */

#include <EGL/mali_egl.h>
#include <EGL/mali_eglext.h>
#include <shared/mali_image.h>
#include <shared/m200_td.h>
#include "egl_surface.h"
#include "egl_context.h"
#include "egl_common_image_properties.h"

/** EGLImage data */
typedef struct egl_image
{
	EGLenum                      target;
	EGLClientBuffer              buffer;
	EGLDisplay                   display_handle;
	EGLContext                   context_handle;
	EGLBoolean                   is_pixmap;
	EGLBoolean                   is_valid;
	egl_image_properties        *prop;
	mali_lock_handle             lock;
	mali_image                  *image_mali;
	s32                          current_session_id;
	egl_surface_lock_item        lock_item; /* a single lock item for CPU access to this EGLImage TODO: need to be per plane*/
	void                        *data; /* Pointer to an EGL platform-layer defined structure (different for each OS), for Symbian, it's heap pointer */
#if EGL_KHR_image_system_ENABLE
	EGLBoolean                   use_as_system_image;
    void                        *image_buffer;    /* image buffer component used to create shared egl_image */
	void                        *release_buffer;  /* buffer to be released when destroying this image */
#endif
} egl_image;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief fills in default values for egl_image_properties
 * @param prop pointer to egl_image_properties
 */
void _egl_image_set_default_properties( egl_image_properties *prop );

/**
 * @brief Locks an EGLImage for external access
 * @param image pointer to image for which to lock
 * @return EGL_TRUE if image was successfully locked, EGL_FALSE on failure or if the image is already locked
 */
EGLBoolean __egl_lock_image( egl_image *image );

/**
 * @brief Unlocks an EGLImage for external access
 * @param image pointer to image for which to unlock
 * @return EGL_TRUE if image was successfully unlocked, EGL_FALSE on failure or if the image was not locked
 */
EGLBoolean __egl_unlock_image( egl_image *image );

/**
 * Destroys an EGLImage, given the actual pointer
 * @param image image to destroy
 * @param force_release forces references to be released if EGL_TRUE
 * @return EGL_TRUE if image was destroyed, EGL_FALSE if it was not destroyed
 */
EGLBoolean _egl_destroy_image ( egl_image *image, EGLBoolean force_release );

/**
 * Creates a bare egl_image
 * @return pointer to egl_image, NULL in case of out of memory
 */
egl_image* _egl_create_image( void );

/**
 * Checks if a given buffer is an EGLImage sibling
 * @param display display to use
 * @param context context to use
 * @param buffer the buffer to check for siblings
 * @param thread_state currentt thread state
 * @return EGL_TRUE if image is sibling, EGL_FALSE if not
 */
EGLBoolean _egl_image_is_sibling( egl_display *display,
                                  egl_context *context,
                                  EGLClientBuffer buffer,
                                  EGLenum target,
                                  void *thread_state );

/**
 * Creates an EGLImage
 * @param dpy display to use
 * @param ctx context to use
 * @param target what type of image to create
 * @param buffer client buffer
 * @param attr_list attribute list
 * @param thread_state current thread state
 * @param buffer_data, any extra data needed to pass, if not needed pass NULL
 * @return EGL_NO_IMAGE_KHR on error, valid EGLImage else
 */
EGLImageKHR _egl_create_image_KHR( EGLDisplay dpy,
                                   EGLContext ctx,
                                   EGLenum target,
                                   EGLClientBuffer buffer,
                                   EGLint *attr_list,
                                   void *thread_state,
                                   void *buffer_data);

/**
 * Creates an EGLImage
 * @param dpy the display to use
 * @param properties the properties to use for the new image
 * @param width width of image
 * @param height height of image
 * @param thread_state current thread state
 * @return EGL_NO_IMAGE_KHR on error, valid EGLImage else
 * @note called internally from within mali_egl_image API
 */
EGLImageKHR _egl_create_image_internal( EGLDisplay dpy,
                                   egl_image_properties *properties,
                                   EGLint width, EGLint height,
                                   void *thread_state );
/**
 * Destroys an EGLImage
 * @param dpy display to use
 * @param image image to destroy
 * @param thread_state current thread state
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean _egl_destroy_image_KHR( EGLDisplay dpy, EGLImageKHR image, void *thread_state );

/**
 * Helper function which retrieves the pointer to an egl_image given an EGLImageKHR handle
 * @param image_handle handle to retrieve pointer for
 * @param usage image usage bits to check
 * @return egl_image pointer on success, NULL on failure
 * @note this function is called from the client API when an EGLImage is used,
 *       and will implicitly allocate any buffers which have not yet been allocated
 */
MALI_IMPORT egl_image* __egl_get_image_ptr_implicit( EGLImageKHR image_handle, EGLint usage ) MALI_CHECK_RESULT;

#if EGL_KHR_image_system_ENABLE
/**
 * Creates and returns a unique name (handle) for a client.
 * @param dpy display to use
 * @param thread_state current thread_state
 * @return valid EGLClientNameKHR success, NULL otherwise
 */
EGLClientNameKHR _egl_get_client_name_KHR(EGLDisplay dpy, void *thread_state);

/**
 * Creates and returns a unique name (handle) for an EGLImage that can be used by target client to create image
 * @param dpy   display to use
 * @param image EGLImage to be exported
 * @param target_client target_client that can use the exported EGLImage
 * @attrib_list  attribute list
 * @param thread_state current thread state
 * @return valid EGLImageNameKHR usable by target_client to create image on success, NULL otherwise
 */
EGLImageNameKHR _egl_export_image_KHR( EGLDisplay dpy,
                                  EGLImageKHR image,
                                  EGLClientNameKHR target_client,
                                  const EGLint * attrib_list,
                                  void *thread_state );

/**
 * Checks if this image has all mali_surfaces with UMP memory, if not creates UMP memory and copies the from non ump mali_surface
 * into the ump memory including all references and locks. After the copy the non ump memory is freed.
 * @param image, egl image to be made sharable
 * @note locking/unlocking is for surface->access_lock is done in this function when it is required to change memory references
 */
EGLBoolean _egl_make_image_sharable( egl_image *image);
#endif

#ifdef __cplusplus
}
#endif

/** @} */ /* end group eglcore_image */

/** @} */ /* end group eglcore */

#endif /* _EGL_IMAGE_H_ */
