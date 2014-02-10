/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _API_INTERFACE_VG_H_
#define _API_INTERFACE_VG_H_
/**
 * @file api_interface_vg.h
 * @brief Exported inter client API for OpenVG
 */

#include <base/mali_types.h>
#include <VG/openvg.h>

#include <shared/m200_gp_frame_builder.h>
#include <shared/mali_egl_types.h>
#include <shared/mali_egl_api_globals.h>
#include <shared/mali_surface.h>

struct vg_context;
struct vg_image;
struct egl_image;

typedef struct egl_vg_api_functions
{
	/*** API function pointers ***/
	VGboolean (*is_valid_image_handle)( struct vg_context *ctx, struct vg_image *img );
	VGImage (*get_parent)( struct vg_context *ctx, VGImage image );
	VGboolean (*image_lock)( VGImage client_buffer ); /* make current for image */
	void (*image_unlock)( VGImage client_buffer );    /* set image as not current anymore */
	void (*lock_image_ptrset)(struct vg_context *ctx); /* locks image ptrlist */
	void (*unlock_image_ptrset)(struct vg_context *ctx); /* unlocks image ptrlist */
	void (*image_ref)(struct vg_context *ctx, struct vg_image *image ); /* ref.count image */
	void (*image_deref)(struct vg_context *ctx, struct vg_image *image ); /* deref image */
	VGboolean (*image_pbuffer_to_clientbuffer)( VGImage client_buffer, mali_surface *color_attachment ); /* blit pbuffer to VGImage */
	VGboolean (*image_match_egl_config)( VGImage client_buffer,
									s32 buffer_size,
									s32 red_size,
									s32 green_size,
									s32 blue_size,
									s32 alpha_size,
									VGboolean *is_linear,
									VGboolean *is_premultiplied,
									s32	*width,
									s32 *height,
									mali_pixel_format *pixel_format );

	mali_err_code (*initialize)( egl_vg_global_data *global_data );
	void (*shutdown)( egl_vg_global_data *global_data );
	void * (*create_global_context)( mali_base_ctx_handle base_ctx );
	void (*destroy_global_context)( void* g_ctx );
	struct vg_context* (*create_context)( mali_base_ctx_handle base_ctx, struct vg_context *shared_context, void *g_ctx );
	void        (*destroy_context)( struct vg_context *ctx );

	VGboolean   (*context_resize_acquire)( struct vg_context *ctx, u32 width, u32 height );
	void        (*context_resize_rollback)( struct vg_context *ctx, u32 width, u32 height );
	void        (*context_resize_finish)( struct vg_context *ctx, u32 width, u32 height );

	VGboolean   (*make_current)(
									struct vg_context *ctx,
									mali_frame_builder *frame_builder,
									mali_egl_surface_type surface_type,
									s32 egl_samples,
									s32 egl_alpha_mask_size,
									s32 egl_surface_width,
									s32 egl_surface_height,
									s32 alpha_format,
									s32 color_space );
	void (*flush)( struct vg_context *ctx );
	void (*finish)( struct vg_context *ctx );
	void (* (*get_proc_address)( const char *procname ))( void );
	VGboolean (*set_frame_builder)( struct vg_context *ctx, mali_frame_builder *frame_builder );
	enum egl_image_from_vg_image_status (*setup_egl_image)( struct vg_context *ctx, VGImage img, struct egl_image *egl );
	mali_surface* (*image_get_mali_surface)(struct vg_context *ctx, struct vg_image *image ); /* returns internal surface, for use in pbuffers */
} egl_vg_api_functions;

/*** API function linkers  ***/
#ifdef EGL_WEAK_LINKING
#define APIENTRY_INTERFACE MALI_WEAK
APIENTRY_INTERFACE VGboolean MALI_CHECK_RESULT _vg_is_valid_image_handle( struct vg_context *ctx, struct vg_image *img );
APIENTRY_INTERFACE VGImage _vg_get_parent( struct vg_context *ctx, VGImage image );
APIENTRY_INTERFACE VGboolean _vg_image_lock( VGImage client_buffer );
APIENTRY_INTERFACE void _vg_image_unlock( VGImage client_buffer );
APIENTRY_INTERFACE void _vg_lock_image_ptrset(struct vg_context *ctx);
APIENTRY_INTERFACE void _vg_unlock_image_ptrset(struct vg_context *ctx);
APIENTRY_INTERFACE void _vg_image_ref( struct vg_context* ctx, struct vg_image* img );
APIENTRY_INTERFACE void _vg_image_deref( struct vg_context* ctx, struct vg_image* img );

APIENTRY_INTERFACE VGboolean _vg_image_pbuffer_to_clientbuffer( VGImage client_buffer, mali_surface *color_attachment );
APIENTRY_INTERFACE VGboolean _vg_image_match_egl_config( VGImage client_buffer,
									s32 buffer_size,
									s32 red_size,
									s32 green_size,
									s32 blue_size,
									s32 alpha_size,
									VGboolean *is_linear,
									VGboolean *is_premultiplied,
									s32	*width,
									s32 *height,
									mali_pixel_format *pixel_format );

APIENTRY_INTERFACE mali_err_code _vg_initialize(struct egl_vg_global_data *global_data);
APIENTRY_INTERFACE void  _vg_shutdown(struct egl_vg_global_data *global_data);
APIENTRY_INTERFACE void* _vg_create_global_context( mali_base_ctx_handle base_ctx );
APIENTRY_INTERFACE void _vg_destroy_global_context( void *g_ctx );
APIENTRY_INTERFACE struct vg_context* _vg_create_context( mali_base_ctx_handle base_ctx, struct vg_context *shared_context, void *g_ctx );
APIENTRY_INTERFACE void        _vg_destroy_context( struct vg_context *ctx );
APIENTRY_INTERFACE VGboolean   _vg_context_resize_acquire( struct vg_context *ctx, u32 width, u32 height );
APIENTRY_INTERFACE void        _vg_context_resize_rollback( struct vg_context *ctx, u32 width, u32 height );
APIENTRY_INTERFACE void        _vg_context_resize_finish( struct vg_context *ctx, u32 width, u32 height );
APIENTRY_INTERFACE VGboolean   _vg_make_current(
									struct vg_context *ctx,
									mali_frame_builder *frame_builder,
									mali_egl_surface_type surface_type,
									s32 egl_samples,
									s32 egl_alpha_mask_size,
									s32 egl_surface_width,
									s32 egl_surface_height,
									s32 alpha_format,
									s32 color_space );
APIENTRY_INTERFACE void _vg_flush( struct vg_context *ctx );
APIENTRY_INTERFACE void _vg_finish( struct vg_context *ctx );
APIENTRY_INTERFACE void (* _vg_get_proc_address( const char *procname ))( void );
APIENTRY_INTERFACE VGboolean _vg_set_frame_builder( struct vg_context *ctx, mali_frame_builder *frame_builder );
APIENTRY_INTERFACE enum egl_image_from_vg_image_status _vg_setup_egl_image( struct vg_context *ctx, VGImage img, struct egl_image *egl );
APIENTRY_INTERFACE mali_surface* _vg_image_get_mali_surface( struct vg_context *ctx, struct vg_image *image );
#undef APIENTRY_INTERFACE
#endif /* EGL_WEAK_LINKING */

#endif /* _API_INTERFACE_VG_H_ */

