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
 * @file gles_context.h
 * @brief Defines the structures that build up the gles-context.
 */

#ifndef _GLES_CONTEXT_H_
#define _GLES_CONTEXT_H_

#include <mali_system.h>
#include "gles_base.h"
#include "gles_config.h"
#include "gles_ftype.h"
#include "gles_state.h"
#include "gles_texture_object.h"
#include <shared/mali_mem_pool.h>

#include <shared/m200_gp_frame_builder.h>
#include <shared/mali_egl_types.h>
#include <shared/mali_named_list.h>
#include <shared/mali_egl_api_globals.h>
#include "egl/api_interface_gles.h"

struct egl_image;
struct gles_share_lists;
struct gles_vtable;
struct gles_framebuffer_object;
struct mali_rsw_raster;

/* Convenience macros: There are no point in paying for a runtime check of API
 *                     version is active if only one are included in a build */
#if MALI_USE_GLES_2
 #define IF_API_IS_GLES1(api_version) if (GLES_API_VERSION_1 == api_version)
#else
 #define IF_API_IS_GLES1(api_version)
#endif

#if MALI_USE_GLES_1
 #define IF_API_IS_GLES2(api_version) if (GLES_API_VERSION_2 == api_version)
#else
 #define IF_API_IS_GLES2(api_version)
#endif

/** the structure holding the entire gles context */
typedef struct gles_context
{
	mali_base_ctx_handle base_ctx;                 /**< Holds the base driver context */

	enum gles_api_version api_version;             /**< The OpenGL ES API version this context
	                                                    was created for*/
	const struct gles_vtable *vtable;              /**< The vtable containing function pointers
	                                                    to the second layer functions for this
	                                                    context. The vtable is initialized based
	                                                    on the API version */

	gles_state state;                              /**< Holds the OpenGL ES state */
	
	u32 *bit_buffer;							   /**< A reusable buffer where each bit indicates
														 a used u16 index. This buffer should always 
														 be zeroed between usage */

	gles_texture_object *default_texture_object[GLES_TEXTURE_TARGET_COUNT];
	                                               /**< Default texture objects, all named 0 */
	struct gles_framebuffer_object *default_framebuffer_object;
	                                               /* Default FBO, named 0 */
	struct gles_share_lists *share_lists;          /**< Set of share lists that handles sharing
	                                                    of objects between different contexts */

	struct gles_gb_context *gb_ctx;                /**< The geometry backend (GB) context */
	struct gles_fb_context *fb_ctx;                /**< The fragment backend (FB) context */
	struct mali_rsw_raster *rsw_raster;            /**< The template rasterisation RSWs */

#ifdef USING_SHADER_GENERATOR
	struct gles_sg_context *sg_ctx;                /**< A pointer to the shader generator
	                                                   context,for variants that use this */
#endif
	egl_api_shared_function_ptrs *egl_funcptrs;    /**< EGL function pointers accessible from APIs. */
	mali_mem_pool *frame_pool;                     /**< A pointer to the current memory pool */
	GLubyte renderer[32];
#if EXTENSION_ROBUSTNESS_EXT_ENABLE
	GLenum robust_strategy;
#endif

	struct mali_frame_builder *texture_frame_builder; /**< The frame-builder used to swizzle textures in hw */

#if defined( NEXELL_LEAPFROG_FEATURE_VIEWPORT_SCALE_EN ) || defined( NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN )  /* nexell for leapfrog */
	unsigned int scale_surf_width;
	unsigned int scale_surf_height;	
	unsigned int sacle_surf_pixel_byte_size;
#endif
} gles_context;

/**
 * @brief Prepares for rendering, including setting up the frame pool
 * @param ctx A pointer to the gles_context to set as the current OpenGL ES context
 * @return An error code
 */
MALI_CHECK_RESULT mali_err_code _gles_begin_frame(gles_context *ctx);

/**
 * @brief Resets (clean) a frame (typically after an error has occured, or after a complete clear)
 *
 * This will reset the current frame builder and re-initialize the frame pool
 *
 * @param ctx A pointer to the gles_context to set as the current OpenGL ES context 
 * @return An error code
 */
MALI_CHECK_RESULT mali_err_code _gles_clean_frame(gles_context *ctx);

/**
 * @brief Resets a frame (typically after an error has occured, or after a complete clear)
 *
 * This will reset the current frame builder and re-initialize the frame pool
 *
 * @param ctx A pointer to the gles_context to set as the current OpenGL ES context
 * @return An error code
 */
MALI_CHECK_RESULT mali_err_code _gles_reset_frame(gles_context *ctx);

/**
 * @brief Checks if there have been any errors during rendering, and updates the GLES error state if so.
 * @param ctx A pointer to the gles_context to check errors for
 */
void _gles_check_for_rendering_errors(gles_context *ctx);

/**
 * @brief Prepares for a drawcall.
 *
 * This function should be called before any memory is allocated from the frame pool,
 * and before the frame builder is used.
 *
 * Each _gles_drawcall_begin should have a matching _gles_drawcall_end
 *
 * @param ctx A pointer to the gles_context to set as the current OpenGL ES context
 * @return An error code
 */
mali_err_code _gles_drawcall_begin(gles_context *ctx);

/**
 * @brief Finalizes a draw call
 *
 * This function should be called every time a drawcall has been completed.
 * Each _gles_drawcall_begin should have a matching _gles_drawcall_end
 *
 * @param ctx A pointer to the gles_context to set as the current OpenGL ES context
 */
void _gles_drawcall_end(gles_context *ctx);

/**
 * @brief Returns a pointer to the current GLES context
 *
 * @pre There must be a current context, otherwise the return value is undefined.
 * @return The pointer to the current context
 */
struct gles_context *_gles_get_context(void);

/**
 * @brief Sets the supplied gles_context as the current OpenGL ES context.
 *
 * @param ctx A pointer to the gles_context to set as the current OpenGL ES context
 * @return An error if something failed
 */
MALI_CHECK_RESULT mali_err_code _gles_make_current( struct gles_context *ctx );

/**
 * @brief Performs initializations once per process per GLES version.
 *
 * @param global_data A pointer to the GLES global data held by EGL
 * @return A Mali error code
 *
 * @note This function is called once for each GLES version present, so it
 * must be able to cope with resources in \a global_data already being
 * allocated.
 */
mali_err_code _gles_initialize(egl_gles_global_data *global_data);

/**
 * @brief Shut down the initializations performed by _gles_initialize
 *
 * @param global_data A pointer to the GLES global data held by EGL
 *
 * @note This function may be called even if _gles_initialize failed.
 * @note This function will be called once for each GLES version present,
 * so it must be able to cope with the resources in \a global_data having
 * already been freed.
 */
void _gles_shutdown(egl_gles_global_data *global_data);

/**
 * @brief Sets the write/draw frame builder. This framebuilder will be used for all operations, except
 *        for copying (glCopyTex[Sub]Image2D) and pixel reading (glReadPixels). For these operations
 *        the read frame builder will be used.
 * @note The write/draw and read frame builders can be the same frame builder, in which case that one is
 *       used for all operations.
 * @note It is _not_ legal to send a NULL frame_builder into this function.
 *
 * @param ctx A pointer to the gles context to initialize with the write/draw frame builder.
 * @param frame_builder A pointer to the write/draw frame builder
 * @param surface_capabilities A set of parameters describing the capabilities of the defautl surface.
 *                             See src/egl/api_interface_gles.h for details.
 */
MALI_CHECK_RESULT mali_err_code _gles_set_draw_frame_builder( gles_context *ctx, mali_frame_builder *frame_builder,
                                                              struct egl_gles_surface_capabilities);

/**
 * @brief Sets the read frame builder. This framebuilder will be used for copying (glCopyTex[Sub]Image2D)
 *        and pixel reading (glReadPixels). For all other operations the write frame builder will be used.
 * @note The read and write/draw frame builders can be the same frame builder, in which case that one is
 *       used for all operations.
 * @note It is _not_ legal to send a NULL frame_builder into this function.
 *
 * @param ctx A pointer to the gles context to initialize with the read frame builder.
 * @param frame_builder A pointer to the read frame builder
 */
MALI_CHECK_RESULT mali_err_code _gles_set_read_frame_builder( gles_context *ctx, mali_frame_builder *frame_builder, mali_egl_surface_type surface_type );

/**
 * @brief Un-binds buffers for a context
 * @param ctx The context to unbind buffer objects from
 */
void _gles_internal_unbind_buffer_objects(gles_context *ctx);

/**
 * @brief Builds the GL_RENDERER string.
 * @param renderer The string to fill with the name of the renderer. Note that this must have been
 *                 pre-allocated before calling this function
 * @param max_len The size of the area allocated in the renderer string (maximum string length)
 */
void _gles_create_renderer_string( char *renderer, int max_len );

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
/**
 * @brief Return the current status of the graphics reset state
 * @param ctx The context
 */
GLenum _gles_get_graphics_reset_status_ext( gles_context *ctx );
#endif
#endif /* _GLES_CONTEXT_H_ */
