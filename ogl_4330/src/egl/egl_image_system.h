/*
 * This confidential and proprietary software may be used only as
 * authorized by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorized
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_image_system.h
 * @brief EGLImage system extension support.
 */

#ifndef _EGL_IMAGE_SYSTEM_H_
#define _EGL_IMAGE_SYSTEM_H_
#include <EGL/egl.h>
#include "egl_config_extension.h"

/* This extension will work only with UMP support */
#if EGL_KHR_image_system_ENABLE != 0

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <mali_system.h>
#include <ump/ump.h>
#include <shared/mali_surface_specifier.h>
#include "egl_common_image_properties.h"

typedef void *EGLClientNameKHR;
typedef void *EGLImageNameKHR;

#define EGL_NO_CLIENT_NAME_KHR                 ((EGLClientNameKHR)0)
#define EGL_NO_IMAGE_NAME_KHR                  ((EGLImageNameKHR)0)
#define EGL_IMAGE_NAME_KHR                     0x31FE
#define EGL_IMAGE_USE_AS_SYSTEM_IMAGE_KHR      0x31FF
#define IMAGE_HANDLES_ENTRIES_MAX              (1 << 12)

typedef EGLClientNameKHR (* PFNEGLGETCLIENTNAMEKHRPROC) (EGLDisplay dpy);
typedef EGLImageNameKHR (* PFNEGLEXPORTIMAGEKHRPROC) (EGLDisplay dpy, EGLImageKHR image, EGLClientNameKHR target_client, const EGLint *attrib_list);

/* forward declaration */
struct egl_display;
struct egl_image;

/**
 * Structure holding globally unique egl image client name related data
 */
typedef struct egl_client_handle_entry
{
	PidType pid;
	EGLNativeDisplayType     native_dpy;
} egl_client_handle_entry;

typedef struct egl_client_handles
{
	EGLint entries;             /**< number of entries/handles registered */
	EGLint references;          /**< number of references to this shared memory */
	egl_client_handle_entry entry[IMAGE_HANDLES_ENTRIES_MAX];	/**< client name entry, keep this entry last in the structure */
} egl_client_handles;

typedef struct egl_sharable_pixmap_image_data
{
#if !defined(HAVE_ANDROID_OS) && !defined(EGL_BACKEND_X11)
	fbdev_pixmap         pixmap;
#endif
	u32                  ump_unique_secure_id;
}egl_sharable_pixmap_image_data;

typedef struct egl_image_system_buffer_properties
{
	u32 width;
	u32 height;
	mali_surface_specifier sformat;
	ump_handle ump;
	u32 ump_unique_secure_id;
	egl_image_properties prop;
	EGLClientBuffer buffer;
}egl_image_system_buffer_properties;

/**
 * Structure holding globally unique egl image name related data
 */
typedef struct egl_image_name_entry
{

	EGLNativeDisplayType     native_dpy;
	EGLClientNameKHR         target_client;     /**< target client which can share this image, also used as entry slot marker (free/occupied/neverallocated) */
	EGLenum                  target_type;
	union
	{
		egl_sharable_pixmap_image_data  pixmap;
		egl_image_system_buffer_properties buffer_properties;
	}image_data;
}egl_image_name_entry;

typedef struct egl_image_name_handles
{
	EGLint entries;            /**< number of entries/handles registered */
	EGLint references;         /**< number of references to this shared memory */
	egl_image_name_entry entry[IMAGE_HANDLES_ENTRIES_MAX]; /**< egl image name entry, keep this entry last in the structure */
}egl_image_name_handles;

/**
 * @brief get the EGLClientNameKHR which is pid as its unique identification from the exported entries
 * @param display display for the current operation
 * @param pid, unique identification of the client
 * @return EGLImageNameKHR for matching image data which is exported to target_client
 */
EGLClientNameKHR __egl_get_client_name( struct egl_display *display, PidType pid );

/**
 * @brief get the EGLImageNameKHR from the exported entries
 * @param display display for the current operation
 * @param image the data contained within the image is checked for already existing EGLImageNameKHR entries
 * @param target_client client for which the image was exported to, if image exists in the entry for client other than target_client
 *                      then it is a mismatch.
 * @return EGLImageNameKHR for matching image data which is exported to target_client
 */
EGLImageNameKHR __egl_get_image_name( struct egl_display *display, struct egl_image *image, EGLClientNameKHR target_client );

/*
 * @brief get handle for the shared memory that holds all the structs managing EGLClientNameKHR
 * @param display display for this operation.
 * @return on failure returns NULL else the mapped handle for the shared memory structs holding EGLClientNameKHR entries
 */
egl_client_handles* __egl_get_add_client_name_handles( struct egl_display *display );

/**
 * @brief get handle for the shared memory that holds all the structs managing EGLImageNameKHR
 * @param display display for this operation.
 * @return on failure returns NULL else the mapped handle for the shared memory structs holding EGLImageNameKHR entries
 */
egl_image_name_handles* __egl_get_add_image_name_handles( struct egl_display *display );

/**
 * @brief Create and register client (pid) calling this api into the client names entry
 * @param display display of the client which is also registered within the name
 * @param pid, unique identification of the client to be registered
 * @return adds the client name to the list and returns the handle to it on success, returns EGL_NO_CLIENT_NAME_KHR on failure.
 */
EGLClientNameKHR __egl_add_client_name( struct egl_display *display, PidType pid );

/**
 * Create and exports EGLImageNameKHR to the image name entries
 * @param display display for the current operation
 * @param image image whose EGLImageNameKHR to be created for.
 * @param target_client is the only client that can use the exported image name
 * @return adds the image name to the list and returns the handle to it on success, returns EGL_NO_IMAGE_NAME_KHR on failure.
 */
EGLImageNameKHR __egl_add_image_name( struct egl_display *display, struct egl_image *image, EGLClientNameKHR target_client );

/**
 * @brief verifies that the target_client is a valid EGLClientNameKHR
 * @param display display for the current operation
 * @param target_client client name which needs to be verified
 * @return EGL_TRUE if passed target_client is valid, MALI_FALSE otherwise
 */
EGLBoolean __egl_verify_client_name_valid( struct egl_display *display, EGLClientNameKHR target_client );

/**
 * @brief verify that buffer is valid EGLImageName and convert it to EGLClientBuffer usable to create image.
 * @param display display for the current operation
 * @param buffer, exported EGLImageNameKHR which will be used to create image in this client
 * @param target_type, if not NULL then gets the image target type to be created
 * @param err, if not NULL then returns the error generated if any.
 * @return on success EGLCLientBuffer is returned which is used to create egl_image, NULL on failure
 * @note the returned structure wrapping the image data is owned by egl and will be free when the image is destroyed
 */
EGLClientBuffer __egl_verify_convert_image_name_to_client_buffer( struct egl_display *display, EGLImageNameKHR buffer, EGLenum *target_type, EGLint *err );

/**
 * @brief unregisters from the client name entries
 * @param display display of the client which is unregistering
 */
void __egl_remove_client_name_handle( struct egl_display *display );

/**
 * @brief unregisters from the image_name entries
 * @param display display of the client which is unregistering
 */
void __egl_remove_image_name_handle( struct egl_display *display );
#endif /* EGL_KHR_image_system_ENABLE */

#endif /* _EGL_IMAGE_SYSTEM_H_ */
