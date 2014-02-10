/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _API_INTERFACE_GLES_H_
#define _API_INTERFACE_GLES_H_
/**
 * @file api_interface_gles.h
 * @brief Exported inter client API for OpenGL ES
 */

#include <base/mali_types.h>
#include <opengles/gles_base.h>

#include <shared/m200_gp_frame_builder.h>
#include <shared/mali_surface.h>
#include <shared/egl_image_status.h>
#include <shared/mali_egl_types.h>
#include <shared/mali_egl_api_globals.h>
#include "api_interface_egl.h"

struct gles_context;
struct egl_image;
struct egl_gles_surface_capabilities
{
	u32 red_size, green_size, blue_size, alpha_size;
	u32 depth_size, stencil_size;
	u32 samples, sample_buffers;
	mali_egl_surface_type surface_type;
};
	/*** API function pointers ***/
typedef struct egl_gles_api_functions
{
	struct gles_context* (*create_context)( mali_base_ctx_handle base_ctx, struct gles_context *share_ctx, egl_api_shared_function_ptrs *funcptrs, int robust_access );
	void (*delete_context)( struct gles_context *ctx );
	mali_err_code (*initialize)( egl_gles_global_data *global_data );
	void (*shutdown)( egl_gles_global_data *global_data );
	mali_err_code (*make_current)( struct gles_context *ctx );
	GLenum (*finish)( struct gles_context *ctx );
	GLenum (*flush)( struct gles_context *ctx, mali_bool flush_all );
	mali_err_code (*set_draw_frame_builder)( struct gles_context *ctx, struct mali_frame_builder *frame_builder, struct egl_gles_surface_capabilities);
	mali_err_code (*set_read_frame_builder)( struct gles_context *ctx, struct mali_frame_builder *frame_builder, mali_egl_surface_type surface_type );
	GLenum (*viewport)( struct gles_context *ctx, GLint x, GLint y, GLsizei width, GLsizei height );
	void (*update_viewport_state)( struct gles_context *ctx );
	GLenum (*scissor)( struct gles_context *ctx, GLint x, GLint y, GLsizei width, GLsizei height );
	GLenum (*get_error)( struct gles_context *ctx );
	GLenum (*copy_tex_image_2d)( struct gles_context *ctx, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border );
	GLenum (*bind_tex_image)(struct gles_context *ctx, GLenum target, GLenum internalformat, mali_bool egl_mipmap_texture, mali_surface *surface, void** out_texture_obj );
	void (*unbind_tex_image)( struct gles_context *ctx, void* texture_obj );
	void (* (*get_proc_address)( const char *procname ))( void );
	enum egl_image_from_gles_surface_status (*setup_egl_image_from_texture)( struct gles_context *ctx, enum egl_image_gles_target egl_target, GLuint name, u32 level, struct egl_image *image );
	enum egl_image_from_gles_surface_status (*setup_egl_image_from_renderbuffer)( struct gles_context *ctx, GLuint name, struct egl_image *image );
	void (*glEGLImageTargetTexture2DOES)( GLenum target, GLeglImageOES image );
	void (*glEGLImageTargetRenderbufferStorageOES)( GLenum target, GLeglImageOES image );
	GLenum (*fence_flush)( struct gles_context *ctx );
} egl_gles_api_functions;

/*** API function linkers  ***/

#ifdef EGL_WEAK_LINKING
#define APIENTRY_INTERFACE MALI_WEAK
APIENTRY_INTERFACE struct gles_context *_gles1_create_context( mali_base_ctx_handle base_ctx, struct gles_context *share_ctx, egl_api_shared_function_ptrs *funcptrs, int robust_access );
APIENTRY_INTERFACE void  _gles1_delete_context( struct gles_context *ctx );
APIENTRY_INTERFACE struct gles_context *_gles2_create_context( mali_base_ctx_handle base_ctx, struct gles_context *share_ctx, egl_api_shared_function_ptrs *funcptrs, int robust_access );
APIENTRY_INTERFACE void  _gles2_delete_context( struct gles_context *ctx );
APIENTRY_INTERFACE mali_err_code _gles_initialize(struct egl_gles_global_data *global_data)
APIENTRY_INTERFACE void  _gles_shutdown(struct egl_gles_global_data *global_data);
APIENTRY_INTERFACE mali_err_code _gles_make_current( struct gles_context *ctx );
APIENTRY_INTERFACE GLenum  _gles_finish( struct gles_context *ctx );
APIENTRY_INTERFACE GLenum _gles_flush( struct gles_context *ctx, mali_bool flush_all );
APIENTRY_INTERFACE mali_err_code  _gles_set_draw_frame_builder( struct gles_context *ctx, struct mali_frame_builder *frame_builder, mali_egl_surface_type surface_type, u32 red_bits, u32 green_bits, u32 blue_bits, u32 alpha_bits, u32 depth_bits, u32 stencil_bits, u32 num_samples, u32 sample_buffers );
APIENTRY_INTERFACE mali_err_code  _gles_set_read_frame_builder( struct gles_context *ctx, struct mali_frame_builder *frame_builder, mali_egl_surface_type surface_type );
APIENTRY_INTERFACE GLenum _gles1_viewport_for_egl( struct gles_context *ctx, GLint x, GLint y, GLint width, GLint height );
APIENTRY_INTERFACE GLenum _gles2_viewport_for_egl( struct gles_context *ctx, GLint x, GLint y, GLint width, GLint height );
APIENTRY_INTERFACE void _gles1_update_viewport_state_for_egl( struct gles_context *ctx );
APIENTRY_INTERFACE void _gles2_update_viewport_state_for_egl( struct gles_context *ctx );
APIENTRY_INTERFACE GLenum _gles1_egl_image_target_texture_2d( struct gles_context *ctx, GLenum target, GLeglImageOES image );
APIENTRY_INTERFACE GLenum _gles2_egl_image_target_texture_2d( struct gles_context *ctx, GLenum target, GLeglImageOES image );
APIENTRY_INTERFACE void (* _gles1_get_proc_address(const char *procname))( void );
APIENTRY_INTERFACE void (* _gles2_get_proc_address(const char *procname))( void );
APIENTRY_INTERFACE enum egl_image_from_gles_surface_status _gles_setup_egl_image_from_texture( struct gles_context *ctx, enum egl_image_gles_target egl_target, GLuint name, u32 level, struct egl_image *image );
APIENTRY_INTERFACE enum egl_image_from_gles_surface_status _gles_setup_egl_image_from_renderbuffer( struct gles_context *ctx, GLuint name, struct egl_image *image );
APIENTRY_INTERFACE GLenum _gles_fence_flush( struct gles_context *ctx );
#undef APIENTRY_INTERFACE
#endif /* EGL_WEAK_LINKING */

#endif /* _API_INTERFACE_GLES_H_ */

