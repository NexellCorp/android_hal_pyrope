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
 * @file egl_surface.h
 * @brief Handles the creation/deletion and query of surfaces.
 */

#ifndef _EGL_SURFACE_H_
#define _EGL_SURFACE_H_

/**
 * @addtogroup eglcore EGL core API
 *
 * @{
 */

/**
 * @defgroup eglcore_surface EGL core surface API
 *
 * @note EGL core surface API
 *
 * @{
 */

#include <EGL/mali_egl.h>
#include <mali_system.h>
#include <shared/m200_gp_frame_builder.h>
#include <shared/mali_image.h>
#include <shared/mali_surface.h>
#include <shared/mali_egl_types.h>

#include "egl_config.h"
#include "egl_display.h"

#include "egl_common_memory.h"

#if defined( EGL_WORKER_THREAD_ENABLED )
#include <base/mali_worker.h>
#endif

#if MALI_INSTRUMENTED
#include <mali_instrumented_context.h>
#endif

#if MALI_ANDROID_API > 12
	#define MAX_EGL_BUFFERS 5
#else
	#define MAX_EGL_BUFFERS 3
#endif

#define EGL_SURFACE_MAX_WIDTH  MALI200_MAX_TEXTURE_SIZE
#define EGL_SURFACE_MAX_HEIGHT MALI200_MAX_TEXTURE_SIZE

struct egl_context;

typedef enum
{
	SURFACE_CAP_NONE                 = 0,
	SURFACE_CAP_DIRECT_RENDERING     = (1<<0),
	SURFACE_CAP_WRITEBACK_CONVERSION = (1<<1),
	SURFACE_CAP_PITCH                = (1<<2)
} egl_surface_caps;

typedef struct egl_buffer
{
	struct mali_surface *render_target;
	struct egl_surface *surface;
	void *data; /**< Platform specific data attached to this buffer */
	int id;
	int flags;
	int fence_fd;
#if defined( EGL_WORKER_THREAD_ENABLED )
	mali_base_worker_handle *swap_worker;
#endif
	mali_ds_consumer_handle ds_consumer_pp_render;	/**< The current's outputbuffer PP consumer, used in FB's _mali_frame_builder_set_lock_output_callback */
	mali_named_list *lock_list; /**< Per frame named list of surface locks, with egl_surface_lock_item as data. Indexed by UMP secure IDs */
} egl_buffer;

typedef struct egl_internal_render_context
{
	u32 current_position_offset;
	u32 current_varyings_offset;
	mali_mem_handle position_buffer;
	mali_mem_handle varyings_buffer;

	s32 rsw_index;

	u32 pre_index_bias;
	u32 post_index_bias;

	mali_addr rsw_base_addr;
	mali_addr vertex_array_base_addr;
	mali_bool base_addr_dirty;

} egl_internal_render_context;

typedef struct egl_frame_swap_parameters
{
	mali_ds_consumer_handle           display_consumer;   /**< The consumer blocking the buffer's output surface */
	egl_buffer*                       buffer;             /**< The buffer */
	mali_ds_error                     error_in_buffer;    /**< The async DS error state received by the activation callback */
	struct egl_frame_swap_parameters *previous;           /**< The previous consumer to release and destroy, can be NULL */
	mali_bool                         defer_release;      /**< If MALI_TRUE, we defer the release of this consumer unti the next swap */
#if MALI_INSTRUMENTED
	mali_instrumented_frame          *instrumented_frame; /**< Stores the active instrumented */
#endif
#ifdef NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN
	void* tstate;
#endif
} egl_frame_swap_parameters;

#if EGL_KHR_lock_surface_ENABLE
struct egl_lock_surface_attributes;
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
struct surface_scaling_information;
#endif

typedef enum
{
	SURFACE_LOCK_USAGE_RENDERABLE = 1,
	SURFACE_LOCK_USAGE_TEXTURE,
	SURFACE_LOCK_USAGE_CPU_WRITE,
	SURFACE_LOCK_USAGE_CPU_READ, /* not currently in use */
} egl_surface_lock_usage;

typedef struct egl_surface_lock_item
{
	egl_surface_lock_usage usage;
	egl_buffer_name buf_name;
	void *platform;
} egl_surface_lock_item;

/**
 * The internal EGLSurface type
 */
typedef struct egl_surface
{
	EGLNativeWindowType     win;                     /**< Window if window surface                       */
	EGLNativePixmapType     pixmap;                  /**< Pixmap if pixmap surface                       */
	mali_frame_builder      *frame_builder;          /**< Internal framebuilder                          */
	mali_egl_surface_type   type;                    /**< Surface type, either window, pbuffer or pixmap */
	egl_surface_caps        caps;                    /**< Surface capabilities                           */
	egl_buffer              buffer[MAX_EGL_BUFFERS]; /**< List of available buffers                      */
	struct egl_frame_swap_parameters *last_swap;     /**< The most recent swap's parameters              */

	mali_mutex_handle       lock;                    /**< EGL surface object lock                        */
	mali_surface            *internal_target;
	mali_image              *pixmap_image;

	/*mali_surface           *attachment_external;*/
	unsigned int            current_buffer;
	unsigned int            num_buffers;
	unsigned int            num_frames;
	egl_internal_render_context render_context;    /**< Render context used for preserved swap behavior */

	EGLint                references;              /**< Reference count for the surface */
	EGLBoolean            is_current;              /**< EGL_TRUE if surface is current to any thread       */
	EGLBoolean            is_null_window;          /**< EGL_TRUE if surface was created with a null window */
	EGLBoolean            is_valid;
	mali_atomic_int       do_readback;             /**< set to 1 if there is a readback needed             */
	EGLint                interval;                /**< eglSwapInterval value                              */
	EGLint                vsync_recorded;          /**< Temporary recorded vsync value                     */

	egl_display           *dpy;                    /**< Reference to display used when creating surface */
	egl_config            *config;                 /**< Reference to config used when creating surface  */
	u32                   width;                   /**< The width of the surface, as seen by the user   */
	u32                   height;                  /**< The height of the surface, as seen by the user  */

	EGLenum               alpha_format;            /**< Alpha format for OpenVG */
	EGLClientBuffer       client_buffer;           /**< Client buffer data. (VGImage) handle if pbuffer tied to a VGImage */
	EGLenum               colorspace;              /**< Color space for OpenVG                      */
	EGLint                horizontal_resolution;   /**< Horizontal dot pitch                        */
	EGLBoolean            largest_pbuffer;         /**< only for pbuffer-surfaces                   */
	EGLBoolean            mipmap_texture;          /**< true if the surface has mipmaps             */
	EGLint                mipmap_level;            /**< Mipmap level to render to                   */
	EGLint                pixel_aspect_ratio;      /**< Display aspect ratio                        */
	EGLenum               render_buffer;           /**< Render buffer                               */
	EGLenum               swap_behavior;           /**< Buffer swap behavior                        */
	EGLenum               multisample_resolve;     /**< Multisample resolve filter                  */
	EGLenum               texture_format;          /**< Format of texture : RGB, RGBA or no texture */
	EGLenum               texture_target;          /**< Type of texture : 2D or no texture          */
	EGLint                vertical_resolution;     /**< Vertical dot pitch                          */
	EGLBoolean            is_bound;                /**< EGL_TRUE if surface is bound to a texture (after a call to eglBindTexImage) */
	struct egl_context   *bound_context;           /**< Valid if is_bound is set. Holds the EGL context pointer used when bound */
	void*                 bound_texture_obj;       /**< Valid if is_bound is set. Holds the GLES texture object. */
	u32                   bound_api_version;       /**< Valid if is_bound is set. Holds the GLES API version. */
	void (*swap_func)( mali_base_ctx_handle base_ctx, EGLNativeDisplayType native_dpy, struct egl_surface *surface, struct mali_surface *target, EGLint interval);
	void (*copy_func)( mali_base_ctx_handle base_ctx, EGLNativeDisplayType native_dpy, struct egl_surface *surface, struct mali_surface *target, EGLNativePixmapType native_pixmap );

	void                  *platform;               /**< Pointer to an EGL platform-layer defined structure (different for each OS) */

#if EGL_KHR_lock_surface_ENABLE
	struct egl_lock_surface_attributes* lock_surface; /**< Attributes for lockable surfaces. */
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
	EGLBoolean                          scaling_enabled;
	struct surface_scaling_information* surface_scaling_info;
#endif
	EGLBoolean force_resize;
	EGLBoolean immediate_mode;
	u32 usage;                                     /**< cached usage bits for the frame builder */

	mali_mutex_handle jobs_mutex;
	mali_lock_handle  jobs_lock;
	s32 jobs;
	mali_named_list *lock_list; /**< Current frame named list of surface locks, with egl_surface_lock_item as data. Indexed by UMP secure IDs */
} egl_surface;

#ifdef __cplusplus
extern "C" {
#endif

/** Adds a reference for a job to the surface
 * @param surface the surface to add a job reference to
 */
void _egl_surface_job_incref( egl_surface *surface );

/** Removes a reference for a job to the surface
 * @param surface the surface to remove a job reference from
 */
void _egl_surface_job_decref( egl_surface *surface );

/** Waits all jobs to be processed on surface
 * @param surface the surface to wait for
 */
void _egl_surface_wait_for_jobs( egl_surface *surface );

/** Properly waits for any inflight rendering, all surface jobs and
 * releases all dependencies for each buffer's display_consumer 
 * @param surface 
 */
void _egl_surface_release_all_dependencies( egl_surface *surface );

/** Internal function used to create a surface of a certain type
 * @param thread_state the current thread state
 * @param dpy the display to create surface on
 * @param type the type of surface from the mali_egl_surface_type enumeration.
 * @param config the config of the surface
 * @param window the native window handle or NULL if no window
 * @param pixmap the pixmap handle or NULL of not a pixmap surface
 * @param attrib_list the attrib list as described by the EGL 1.4 specification for surface creation.
 * @param color_buffer_dst if the colour buffer is already allocated
 * @param att_dirty_flag render attachment dirty flags.
 * @return EGL_NO_SURFACE on failure, a new surface on success.
 */
egl_surface* __egl_create_surface( void *thread_state, egl_display *dpy,
                                   mali_egl_surface_type type,
                                   egl_config* config,
                                   EGLNativeWindowType window, EGLNativePixmapType pixmap,
                                   const EGLint *attrib, mali_surface* color_buffer_dst );

/**
 * @brief creates and returns a window surface
 * @param __dpy the display to use
 * @param __config the config to use
 * @param window the native window
 * @param attrib_list list of attributes
 * @param thread_state thread state pointer
 * @return valid surface handle on success, EGL_NO_SURFACE on failure
 */
EGLSurface _egl_create_window_surface( EGLDisplay __dpy, EGLConfig __config, EGLNativeWindowType window, const EGLint *attrib_list, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Creates a pbuffer surface
 * @param __dpy the display to use
 * @param __config the config to use
 * @param attrib_list list of wanted attributes
 * @param thread_state thread state pointer
 * @return valid surface handle on success, EGL_NO_SURFACE on failure
 */
EGLSurface _egl_create_pbuffer_surface( EGLDisplay __dpy, EGLConfig __config, const EGLint *attrib_list, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Creates a pbuffer from a given client buffer
 * @param __dpy the display to use
 * @param buftype the type of buffer given (only supported is EGL_OPENVG_IMAGE)
 * @param buffer the actual buffer (in this case a valid VGImage)
 * @param __config config to use
 * @param attrib_list list of wanted attributes
 * @param thread_state thread state pointer
 * @return valid surface handle on success, EGL_NO_SURFACE on failure
 */
EGLSurface _egl_create_pbuffer_from_client_buffer( EGLDisplay __dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig __config, const EGLint *attrib_list, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Creates a pixmap surface
 * @param __dpy the display to use
 * @param __config the config to use
 * @param pixmap the native pixmap to attach
 * @param attrib_list list of wanted attributes
 * @param thread_state thread state pointer
 * @return valid surface handle on success, EGL_NO_SURFACE on failure
 */
EGLSurface _egl_create_pixmap_surface( EGLDisplay __dpy, EGLConfig __config, EGLNativePixmapType pixmap, const EGLint *attrib_list, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Release a surface (not dealing with the handle for it)
 * @param surface pointer to the surface to release
 * @param thread_state pointer to thread state
 * @return 0 if deleted, else a number indicating remaining references
 */
EGLint __egl_release_surface( egl_surface *surface, void *thread_state );

/**
 * @brief Release a surface (decrement the reference count, release it when needed)
 * @param __dpy the display the surface is associated with
 * @param surface the surface to destroy
 * @param tag_invalid tag the handle as invalid if EGL_TRUE
 * @param thread_state pointer to thread state
 * @return EGL_TRUE if surface was destroyed
 */
EGLBoolean _egl_destroy_surface_internal( EGLDisplay __dpy, egl_surface *surface, EGLBoolean tag_invalid, void *thread_state );

/**
 * @brief Destroys a surface and associated data
 * @param __dpy display surface is associated with
 * @param __surface the surface to be deleted
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_destroy_surface( EGLDisplay __dpy, EGLSurface __surface, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Returns surface attribute values
 * @param __dpy the display the surface is associated with
 * @param __surface the surface which attribute values are taken from
 * @param attribute wanted attribute
 * @param value pointer to memory with attribute value
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_query_surface( EGLDisplay __dpy, EGLSurface __surface, EGLint attribute, EGLint *value, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Sets attribute values for a surface
 * @param __dpy the display the surface is associated with
 * @param __surface the surface to change attributes for
 * @param attribute which attribute to change
 * @param value the value to be replaced
 * @param thread_state thread state pointer
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_surface_attrib( EGLDisplay __dpy, EGLSurface __surface, EGLint attribute, EGLint value, void *thread_state ) MALI_CHECK_RESULT;

/**
 * @brief Returns the current surface for the current thread for the current client API
 * @param readdraw whether to retrieve the read or the draw surface of the thread state
 * @param thread_state thread state pointer
 * @return surface handle on success, EGL_FALSE on failure
 * @note readdraw can either be EGL_DRAW or EGL_READ
 */
EGLSurface _egl_get_current_surface( EGLint readdraw, void *thread_state ) MALI_CHECK_RESULT;

/**
 * Helper function for filling surface specifier with properties from EGLSurface
 * @param spec[out] 
 * @param surface
 */
MALI_STATIC_INLINE void __egl_surface_to_surface_specifier( mali_surface_specifier *spec, const egl_surface *surface )
{
	MALI_DEBUG_ASSERT_POINTER( spec );
	MALI_DEBUG_ASSERT_POINTER( surface );

	_mali_surface_specifier_ex(
		spec, 
		surface->width,
		surface->height,
		0,
		surface->config->pixel_format, 
		_mali_pixel_to_texel_format( surface->config->pixel_format ), 
		MALI_PIXEL_LAYOUT_LINEAR, 
		M200_TEXTURE_ADDRESSING_MODE_LINEAR, 
		MALI_FALSE, 
		MALI_FALSE, 
		( EGL_COLORSPACE_sRGB == surface->colorspace ) ? MALI_SURFACE_COLORSPACE_sRGB : MALI_SURFACE_COLORSPACE_lRGB, 
		( EGL_ALPHA_FORMAT_PRE == surface->alpha_format ) ? MALI_SURFACE_ALPHAFORMAT_PRE : MALI_SURFACE_ALPHAFORMAT_NONPRE,
		surface->config->alpha_size == 0 ? MALI_TRUE : MALI_FALSE );	
}

/**
 * @brief Callback executed when a surface is mapped for CPU access.
 * @param surface The mali surface accessed by CPU.
 * @param event The event leading up to this callback, typically \em MALI_SURFACE_EVENT_CPU_ACCESS.
 * @param mem_ref The mali_shared_mem_ref associated with current surface
 * @param data Custom data for the callback.
 */
void _egl_surface_cpu_access_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data );

/**
 * @brief Callback executed when a surface is ummapped.
 * @param surface The mali surface accessed by CPU.
 * @param event The event leading up to this callback, typically \em MALI_SURFACE_EVENT_CPU_ACCESS_DONE.
 * @param mem_ref The mali_shared_mem_ref associated with current surface
 * @param data Custom data for the callback.
 */
void _egl_surface_cpu_access_done_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data );

/**
 * @brief Callback executed when a surface is written by GPU, for example, renderable surface.
 * @param surface The mali surface written by GPU.
 * @param event The event leading up to this callback, typically \em MALI_SURFACE_EVENT_GPU_WRITE.
 * @param mem_ref The mali_shared_mem_ref associated with current surface
 * @param data Custom data for the callback.
 */
void _egl_surface_gpu_write_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data );

/**
 * @brief Callback executed when GPU finish writting.
 * @param surface The mali surface written by GPU.
 * @param event The event leading up to this callback, typically \em MALI_SURFACE_EVENT_GPU_WRITE_DONE.
 * @param mem_ref The mali_shared_mem_ref associated with current surface
 * @param data Custom data for the callback.
 */
void _egl_surface_gpu_write_done_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data );

/**
 * @brief Callback executed when a surface is taken in use by the GPU, for example, as texture.
 * @param surface The mali surface read by GPU.
 * @param event The event leading up to this callback, typically \em MALI_SURFACE_EVENT_GPU_READ.
 * @param mem_ref The mali_shared_mem_ref associated with current surface
 * @param data Custom data for the callback.
 */
void _egl_surface_gpu_read_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data );

/**
 * @brief Callback executed when GPU finish reading the surface.
 * @param surface The mali surface read by GPU.
 * @param event The event leading up to this callback, typically \em MALI_SURFACE_EVENT_GPU_READ_DONE.
 * @param data Custom data for the callback.
 */
void _egl_surface_gpu_read_done_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data );

/**
 * @brief Callback executed when a texture is taken in use by GPU.
 * @param surface The mali surface to be taken in use as a texture.
 * @param event The event leading up to this callback, typically \em MALI_SURFACE_EVENT_UPDATE_TEXTURE.
 * @param mem_ref The mali_shared_mem_ref associated with current surface
 * @param data Custom data for the callback.
 * @note \ref __egl_platform_update_image is called as part of this procedure, before the lock item is registered.
 * @see __egl_platform_register_lock_item
 * @see __egl_platform_update_image
 */
void _egl_surface_update_texture_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data );

#ifdef __cplusplus
}
#endif

/** @} */ /* end group eglcore_surface */

/** @} */ /* end group eglcore */


#endif /* _EGL_SURFACE_H_ */

