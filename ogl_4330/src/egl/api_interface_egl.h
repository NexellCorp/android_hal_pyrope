/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _API_INTERFACE_EGL_H_
#define _API_INTERFACE_EGL_H_
/**
 * @file api_interface_egl.h
 * @brief Exported inter client API
 */


#include <base/mali_sync_handle.h>
#include <EGL/mali_egl.h>
#include <EGL/mali_eglext.h>
#if MALI_EXTERNAL_SYNC == 1
#include "sync/mali_external_sync.h"
#endif
struct egl_image;

typedef struct
{

	struct egl_image *(*get_eglimage_ptr)( EGLImageKHR image_handle, EGLint usage); /**< A pointer to a function to 
																						 retrieve an EGL image from. */

	mali_sync_handle (*get_synchandle)( void ); /**< A pointer to a function to retrieve current flush's sync handle. 
												     All flushes must be added to this! */
#if MALI_EXTERNAL_SYNC == 1
	void (*_sync_notify_job_create)(void *data);  /**<A pointer to a function to increase the number of flushes not yet started
												   this should be called during pp job creatation */
	void (*_sync_notify_job_start)(mali_bool success, mali_fence_handle fence, void *data); /**< A pointer to a function to merge the fence of current flush into accumulated_sync
																							 this should be called during pp job start*/
#endif
	
} egl_api_shared_function_ptrs;

#endif /* _API_INTERFACE_EGL_H_ */

