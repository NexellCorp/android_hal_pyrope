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
 * @file egl_context.h
 * @brief Handles the creation/deletion and query of contexts.
 */

#ifndef _EGL_CONTEXT_H_
#define _EGL_CONTEXT_H_

/**
 * @addtogroup eglcore EGL core API
 *
 * @{
 */

/**
 * @defgroup eglcore_context EGL core context API
 *
 * @note EGL core context API
 *
 * @{
 */

#include <EGL/mali_egl.h>
#include <base/mali_macros.h>
#include "mali_instrumented_context.h"
#include "egl_config.h"
#include "egl_surface.h"
#include <shared/mali_linked_list.h>

#if EGL_KHR_image_ENABLE
typedef enum
{
	EGL_IMAGE_NONE                     = 0,
	EGL_IMAGE_VG_PARENT_IMAGE          = (1<<0),
	EGL_IMAGE_GL_TEXTURE_2D_IMAGE      = (1<<1),
	EGL_IMAGE_GL_TEXTURE_CUBEMAP_IMAGE = (1<<2),
	EGL_IMAGE_GL_TEXTURE_3D_IMAGE      = (1<<3),
	EGL_IMAGE_GL_RENDERBUFFER_IMAGE    = (1<<4)
} __egl_image_caps;
#endif /* EGL_KHR_image_ENABLE */

/**
 * @brief The internal structure representing an EGLContext
 */
typedef struct egl_context
{
	egl_config  *config;           /** Pointer to egl_config if context is current */
	egl_surface *surface;          /** Pointer to egl_surface if context is current */
	EGLenum     api;               /** Client API, either EGL_OPENGL_ES_API or EGL_OPENVG_API */
	void        *api_context;      /** Pointer to API specific context */
	EGLBoolean  gles_viewport_set; /** needed to call glViewport and glScissor on first context-bind */
	EGLint      references;        /** Number of references made to this context */
	EGLint      client_version;    /** Only set for OpenGL ES contexts, either 1 or 2 */
	EGLBoolean  robustness_enabled;          /* if robustness extension is enabled for this context */
	EGLint      reset_notification_strategy; /* reset notification strategy in case robustness is enabled for this context */
	EGLBoolean  is_current;        /** Set to EGL_TRUE if context is current to any thread */
	EGLBoolean  is_lost;           /** Set to EGL_TRUE in case of a power loss */
	EGLBoolean  is_valid;
#if EGL_KHR_image_ENABLE
	__egl_image_caps egl_image_caps;
#endif
	mali_linked_list bound_images;
} egl_context;

/**
 * @brief Binds the current client API
 * @param api wanted client API, either EGL_OPENGL_ES_API or EGL_OPENVG_API
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_bind_api( EGLenum api, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Queries the current client API
 * @param thread_state thread state pointer
 * @return EGL_NONE on failure api enum on success
 */
EGLenum _egl_query_api( void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Creates a context
 * @param __dpy the display for which to create a surface
 * @param __config the config for which to create a surface
 * @param __share_list optional context that is shared with the new context
 * @param attrib_list context attributes
 * @param thread_state thread state pointer
 * @return a context handle on success, else EGL_NO_CONTEXT
 */
EGLContext _egl_create_context( EGLDisplay __dpy, EGLConfig __config, EGLContext __share_list, const EGLint *attrib_list, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Release a context (decrement the reference count, release it when needed)
 * @param __dpy the display the context is associated with
 * @param ctx the context to destroy
 * @param tag_invalid tag the handle as invalid if EGL_TRUE
 * @param thread_state pointer to thread state
 * @return EGL_TRUE if context was destroyed
 */
EGLBoolean _egl_destroy_context_internal( EGLDisplay __dpy, egl_context *ctx, EGLBoolean tag_invalid, void *thread_state );

/**
 * @brief Destroys a context
 * @param __dpy the display for which to destroy the context
 * @param __ctx context to destroy, has to be created on the given __dpy
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean _egl_destroy_context( EGLDisplay __dpy, EGLContext __ctx, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Queries a context for a wanted attribute value
 * @param __dpy the display the context is attached to
 * @param __ctx context to query
 * @param attribute context attribute to retrieve the value for
 * @param value pointer to value represented by attribute
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean _egl_query_context( EGLDisplay __dpy, EGLContext __ctx, EGLint attribute, EGLint *value, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Make a context current for the current thread
 * @param __dpy the display for which the draw/read surface and context belongs
 * @param __draw the surface to use for drawing
 * @param __read the surface to use for reading
 * @param __ctx the egl context to use for rendering
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE else
 */
EGLBoolean _egl_make_current( EGLDisplay __dpy, EGLSurface __draw, EGLSurface __read, EGLContext __ctx, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Returns the current context for the current thread for the current API
 * @param thread_state thread state pointer
 * @return valid context handle on success, EGL_NO_CONTEXT else
 */
EGLContext _egl_get_current_context( void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Function used to release a context
 * @param ctx the context to be destroyed
 * @return reference count of context is returned,
 * zero indicating that the context was destroyed
 */
EGLint __egl_release_context( egl_context *ctx, void *thread_state );

/**
 * @brief Allocates a new egl_context struct and fills it with default values
 * @param config the config to use for the config
 * @param client_version client version when creating an OpenGL ES context
 * @return valid egl_context pointer, NULL on failure
 */
egl_context* __egl_allocate_context( egl_config *config, EGLint client_version ) MALI_CHECK_RESULT;

/**
 * @brief Remove a surface from the list of bound surfaces in the context
 *
 * @param ctx     The EGL context with the list of surfaces
 * @param surface The pointer of the surface to remove
 */
void __egl_context_unbind_bound_surface( egl_context *ctx, egl_surface *surface );

/** @} */ /* end group eglcore_context */

/** @} */ /* end group eglcore */

#endif /* _EGL_CONTEXT_H_ */

