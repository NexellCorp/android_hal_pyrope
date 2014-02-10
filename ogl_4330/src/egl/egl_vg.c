/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <mali_system.h>
#include "egl_vg.h"
#include "egl_context.h"
#include "egl_misc.h"
#include "egl_handle.h"
#include "mali_instrumented_context.h"
#include "egl_config_extension.h"
#include <shared/egl_image_status.h>
#include "api_interface_vg.h"


mali_err_code __egl_vg_initialize( __egl_main_context *egl )
{
	MALI_DEBUG_ASSERT_POINTER( egl );

	if ( egl->linker->caps & _EGL_LINKER_OPENVG_BIT )
	{
		mali_err_code err = egl->linker->vg_func.initialize( &egl->vg_global_data );
		MALI_CHECK_NO_ERROR( err );
		MALI_SUCCESS;
	}

	return MALI_ERR_FUNCTION_FAILED;
}

void __egl_vg_shutdown( __egl_main_context *egl )
{
	MALI_DEBUG_ASSERT_POINTER( egl );

	if ( egl->linker->caps & _EGL_LINKER_OPENVG_BIT )
	{
		egl->linker->vg_func.shutdown( &egl->vg_global_data );
	}
}

EGLBoolean __egl_vg_context_resize_acquire( egl_context *context, u32 width, u32 height )
{
	__egl_main_context *egl = __egl_get_main_context();

	MALI_DEBUG_ASSERT_POINTER( context );

	return (EGLBoolean)egl->linker->vg_func.context_resize_acquire( context->api_context, width, height );
}

void __egl_vg_context_resize_rollback( egl_context *context, u32 width, u32 height )
{
	__egl_main_context *egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( context );

	egl->linker->vg_func.context_resize_rollback( context->api_context, width, height );
}

void __egl_vg_context_resize_finish( egl_context *context, u32 width, u32 height )
{
	__egl_main_context *egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( context );

	egl->linker->vg_func.context_resize_finish( context->api_context, width, height );
}

#if EGL_KHR_image_ENABLE
void __egl_vg_set_egl_image_caps( egl_context *context )
{
	context->egl_image_caps = EGL_IMAGE_NONE;
	if ( EGL_KHR_vg_parent_image_ENABLE ) context->egl_image_caps |= EGL_IMAGE_VG_PARENT_IMAGE;
}
#endif /* EGL_KHR_image_ENABLE */

egl_context* __egl_vg_create_context( egl_config *config,
                                      egl_context *share_list,
                                      EGLint client_version,
                                      __egl_thread_state *tstate )
{
	egl_context       *ctx = EGL_NO_CONTEXT;
	struct vg_context *shared_context = NULL;
	__egl_main_context *egl = __egl_get_main_context();

	if ( !(config->renderable_type & EGL_OPENVG_BIT) || !(egl->linker->caps & _EGL_LINKER_OPENVG_BIT) )
	{
		__egl_set_error( EGL_BAD_CONFIG, tstate );
		return EGL_NO_CONTEXT;
	}

	ctx = __egl_allocate_context( config, client_version );
	if ( NULL == ctx )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_CONTEXT;
	}

	if ( NULL != share_list ) shared_context = share_list->api_context;

	ctx->api = EGL_OPENVG_API;
	if ( NULL == egl->vg_global_context )
	{
		egl->vg_global_context = egl->linker->vg_func.create_global_context( tstate->main_ctx->base_ctx );
		if ( NULL == egl->vg_global_context )
		{
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			__egl_release_context( ctx, ( void * ) tstate );
			return EGL_NO_CONTEXT;
		}
		MALI_DEBUG_PRINT( 2, ("EGL: created OpenVG global context\n") );
	}

	ctx->api_context = egl->linker->vg_func.create_context( tstate->main_ctx->base_ctx, shared_context, egl->vg_global_context );
	if ( NULL == ctx->api_context )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		__egl_release_context( ctx, ( void * ) tstate );
		return EGL_NO_CONTEXT;
	}

#if EGL_KHR_image_ENABLE
	__egl_vg_set_egl_image_caps( ctx );
#endif

	return ctx;
}

EGLBoolean __egl_vg_release_context( egl_context *ctx )
{
	__egl_main_context *egl = __egl_get_main_context();
	MALI_CHECK_NON_NULL( ctx->api_context, EGL_TRUE );

	egl->linker->vg_func.destroy_context( (struct vg_context*)ctx->api_context );
	ctx->api_context = NULL;

	return EGL_TRUE;
}

void __egl_vg_make_not_current( __egl_thread_state *tstate )
{
	__egl_main_context *egl = __egl_get_main_context();
	if ( NULL != tstate->context_vg )
	{
		egl->linker->vg_func.flush( tstate->context_vg );
		egl->linker->vg_func.make_current( NULL, NULL, MALI_EGL_INVALID_SURFACE, 0, 0, 0, 0, VG_FALSE, VG_FALSE );
	}
}

EGLBoolean __egl_vg_make_current( egl_context *ctx, egl_surface *draw, EGLBoolean lock_client_buffer, __egl_thread_state *tstate )
{
	__egl_main_context *egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( draw );

	if ( EGL_OPENVG_API == tstate->api_current )
	{
		if ( EGL_FALSE == egl->linker->vg_func.make_current( MALI_REINTERPRET_CAST(struct vg_context *)ctx->api_context,
														draw->frame_builder,
														draw->type,
														draw->config->samples,
														draw->config->alpha_mask_size,
														draw->width, draw->height,
														(EGL_VG_ALPHA_FORMAT_PRE == draw->alpha_format ? EGL_TRUE : EGL_FALSE),
														(EGL_VG_COLORSPACE_LINEAR == draw->colorspace ? EGL_TRUE : EGL_FALSE) ) )
		{
			return EGL_FALSE;
		}

		if ( (EGLClientBuffer)NULL != draw->client_buffer )
		{
			if ( EGL_TRUE == lock_client_buffer )
			{
				/* Lock client buffer image and copy it to the back buffer */
				if ( VG_FALSE == egl->linker->vg_func.image_lock( MALI_REINTERPRET_CAST(VGImage)draw->client_buffer ) )
				{
					/* Failed to lock, exit */
					egl->linker->vg_func.make_current( NULL, NULL, MALI_EGL_INVALID_SURFACE, 0, 0, 0, 0, EGL_FALSE, EGL_FALSE );
					return EGL_FALSE;
				}
			}
		}
	}

	return EGL_TRUE;
}

void __egl_vg_remove_framebuilder_from_client_ctx( __egl_thread_state *tstate )
{
	__egl_main_context *egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( tstate );

	if ( NULL == tstate->context_vg ) return;

	/* Do not flush window surfaces, they will be updated in response to eglSwapBuffers */
	if ( EGL_NO_SURFACE != tstate->api_vg->draw_surface && MALI_EGL_WINDOW_SURFACE != tstate->api_vg->draw_surface->type )
	{
		egl->linker->vg_func.flush( tstate->context_vg );
	}

	egl->linker->vg_func.set_frame_builder( tstate->context_vg, NULL );
	egl->linker->vg_func.make_current( NULL, NULL, MALI_EGL_INVALID_SURFACE, 0, 0, 0, 0, EGL_FALSE, EGL_FALSE );
}

EGLBoolean __egl_vg_set_framebuilder( egl_surface *surface, __egl_thread_state *tstate )
{
	__egl_main_context *egl = __egl_get_main_context();

	if ( EGL_FALSE == egl->linker->vg_func.set_frame_builder( tstate->context_vg, surface->frame_builder ) )
	{
		return EGL_FALSE;
	}

	return EGL_TRUE;
}

void __egl_vg_flush( __egl_thread_state *tstate )
{
	__egl_main_context *egl = __egl_get_main_context();

	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( tstate->context_vg );

	egl->linker->vg_func.flush( tstate->context_vg );
}

void __egl_vg_finish( __egl_thread_state *tstate )
{
	__egl_main_context *egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( tstate->context_vg );

	egl->linker->vg_func.finish( tstate->context_vg );
}

#if EGL_KHR_image_ENABLE
EGLint __egl_vg_setup_egl_image( egl_context *context, VGImage buffer, struct egl_image *image )
{
	EGLint err;
	__egl_main_context *egl = __egl_get_main_context();

	MALI_DEBUG_ASSERT_POINTER( context );
	MALI_DEBUG_ASSERT_POINTER( image );

	err = egl->linker->vg_func.setup_egl_image( context->api_context, buffer, image );
	switch( err )
	{
		case VG_IMAGE_TO_EGL_IMAGE_STATUS_NO_ERROR:
			break;
		case VG_IMAGE_TO_EGL_IMAGE_STATUS_INVALID_HANDLE:
			return EGL_BAD_PARAMETER;
		case VG_IMAGE_TO_EGL_IMAGE_STATUS_CHILD_IMAGE:
			return EGL_BAD_ACCESS;
		case VG_IMAGE_TO_EGL_IMAGE_STATUS_ALREADY_SIBLING:
			return EGL_BAD_ACCESS;
		case VG_IMAGE_TO_EGL_IMAGE_STATUS_ERROR:
			return EGL_BAD_MATCH;
		default:
			return EGL_BAD_PARAMETER;
	}

	return EGL_SUCCESS;
}
#endif /* EGL_KHR_image_ENABLE */

void (* __egl_vg_get_proc_address( __egl_thread_state *tstate, const char *procname ))(void)
{
	__egl_main_context *egl = __egl_get_main_context();
	MALI_IGNORE( tstate );

	if ( egl->linker->caps & _EGL_LINKER_OPENVG_BIT )
	{
		if ( NULL == egl->linker->vg_func.get_proc_address ) return NULL;

		return egl->linker->vg_func.get_proc_address( procname );
	}

	return NULL;
}


EGLBoolean __egl_vg_post_to_pbuffer_surface( egl_surface *surface,
                                             EGLBoolean client_buffer,
                                             mali_surface *color_attachment,
                                             __egl_thread_state_api *tstate_api )
{
	EGLBoolean retval = EGL_TRUE;
	__egl_main_context *egl = __egl_get_main_context();

	if ( EGL_TRUE == client_buffer )
	{
		if ( VG_FALSE == egl->linker->vg_func.image_pbuffer_to_clientbuffer( (VGImage)surface->client_buffer, color_attachment ) )
		{
			retval = EGL_FALSE;
		}
		egl->linker->vg_func.image_unlock( (VGImage)tstate_api->draw_surface->client_buffer );
	}

	return retval;
}

egl_surface* __egl_vg_create_pbuffer_from_client_buffer( egl_display *dpy, EGLClientBuffer buffer, egl_config *config, const EGLint *attrib_list, __egl_thread_state *tstate )
{
	__egl_thread_state_api *tstate_api = NULL;
	VGboolean              is_linear = VG_FALSE, is_premultiplied = VG_FALSE;
	mali_pixel_format      pixel_format = MALI_PIXEL_FORMAT_COUNT;
	EGLint                 *cbuf_attrs = NULL;
	s32                    attr_list_length = 0;
	egl_surface            *surf = EGL_NO_SURFACE;
	s32                    width = 0, height = 0, i=0, j=0;
	__egl_main_context     *egl = __egl_get_main_context();
	mali_surface           *vg_surface = NULL;

	tstate_api = __egl_get_current_thread_state_api( tstate, NULL );
	MALI_CHECK_NON_NULL( tstate_api, NULL );

	/* If no context corresponding to the specified buftype is current,
	 * an EGL_BAD_ACCESS error is generated
	 */
	if ( NULL == tstate->api_vg || NULL == tstate->api_vg->context )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return NULL;
	}

	egl->linker->vg_func.lock_image_ptrset( tstate->api_vg->context->api_context );

	if ( VG_FALSE == egl->linker->vg_func.is_valid_image_handle( tstate->api_vg->context->api_context, (void *)buffer ) )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		egl->linker->vg_func.unlock_image_ptrset( tstate->api_vg->context->api_context );
		return NULL;
	}

#if EGL_KHR_image_ENABLE
	/* Check that clientbuffer is not already bound as eglimage */
	if ( EGL_TRUE == _egl_image_is_sibling( dpy, tstate->api_vg->context->api_context, buffer, EGL_VG_PARENT_IMAGE_KHR, tstate  ) )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		egl->linker->vg_func.unlock_image_ptrset( tstate->api_vg->context->api_context );
		return NULL;
	}
#endif /* EGL_KHR_image_ENABLE */

	/* Check that this client buffer is not already bound to a pbuffer surface */
	if ( EGL_TRUE == __egl_client_buffer_handle_exists( buffer ) )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		egl->linker->vg_func.unlock_image_ptrset( tstate->api_vg->context->api_context );
		return NULL;
	}

	/* Check if config has the OpenVG bit 	*/
	if ( 0 == ( config->renderable_type & EGL_OPENVG_BIT )  )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		egl->linker->vg_func.unlock_image_ptrset( tstate->api_vg->context->api_context );
		return NULL;
	}

	/* Check if VG image matches against the given EGLConfig. */
	if ( VG_FALSE == egl->linker->vg_func.image_match_egl_config( (VGImage)buffer,
									config->buffer_size,
									config->red_size,
									config->green_size,
									config->blue_size,
									config->alpha_size,
									&is_linear,
									&is_premultiplied,
									&width,
									&height,
									&pixel_format ) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		egl->linker->vg_func.unlock_image_ptrset( tstate->api_vg->context->api_context );
		return NULL;
	}

	vg_surface = (mali_surface*)egl->linker->vg_func.image_get_mali_surface(tstate->api_vg->context->api_context, (struct vg_image *)buffer);
	if ( NULL == vg_surface )
	{
		/* an VGImage without a surface should never exist. So refuse to continue. */
		__egl_set_error( EGL_BAD_MATCH, tstate );
		egl->linker->vg_func.unlock_image_ptrset( tstate->api_vg->context->api_context );
		return NULL;
	}

	/* Parameters are fine */
	/* Create attrib list from client buffer */
	attr_list_length = 0;
	if ( NULL != attrib_list )
	{
		while ( EGL_NONE != attrib_list[attr_list_length] )
		{
			attr_list_length+=2;
		}
	}

	/* Make space for in-attribs + the attribs which are derived from the image below and terminator
			EGL_VG_COLORSPACE
			EGL_VG_ALPHA_FORMAT,
			EGL_WIDTH
			EGL_HEIGHT
	*/
	cbuf_attrs = _mali_sys_malloc( sizeof(EGLint)*(attr_list_length + 2*4 +1) ); /* Attribute, value... EGL_NONE */
	if ( NULL == cbuf_attrs )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		_mali_sys_free( cbuf_attrs );
		egl->linker->vg_func.unlock_image_ptrset( tstate->api_vg->context->api_context );
		_mali_surface_deref( vg_surface );
		return NULL;
	}
	i = 0;
	j = 0;
	if ( NULL != attrib_list )
	{
		while ( EGL_NONE != attrib_list[i] )
		{
			switch ( attrib_list[i] )
			{
				case EGL_WIDTH :
				case EGL_HEIGHT :
				case EGL_VG_COLORSPACE :
				case EGL_VG_ALPHA_FORMAT :
					/* ignore these, they are derived from the image */
					break;
				default:
					/* Copy the other attributes */
					cbuf_attrs[j] = attrib_list[i];
					cbuf_attrs[j+1] = attrib_list[i+1];
					j+=2;
			}
			i+=2;
		}
	}

	/* Add image derived parameters */
	cbuf_attrs[j++] = EGL_WIDTH;
	cbuf_attrs[j++] = width;
	cbuf_attrs[j++] = EGL_HEIGHT;
	cbuf_attrs[j++] = height;
	cbuf_attrs[j++] = EGL_VG_COLORSPACE;
	cbuf_attrs[j++] = (VG_TRUE == is_linear) ? EGL_VG_COLORSPACE_LINEAR : EGL_VG_COLORSPACE_sRGB;
	cbuf_attrs[j++] = EGL_VG_ALPHA_FORMAT;
	cbuf_attrs[j++] = (VG_TRUE == is_premultiplied) ? EGL_VG_ALPHA_FORMAT_PRE : EGL_VG_ALPHA_FORMAT_NONPRE;
	cbuf_attrs[j++] = EGL_NONE;

	surf = __egl_create_surface( tstate, dpy, MALI_EGL_PBUFFER_SURFACE, config, (EGLNativeWindowType)NULL, (EGLNativePixmapType)NULL, cbuf_attrs, vg_surface );

	_mali_sys_free( cbuf_attrs );
	cbuf_attrs = NULL;

	if ( surf != EGL_NO_SURFACE )
	{
		egl->linker->vg_func.image_ref( tstate->api_vg->context->api_context, (struct vg_image *)buffer );
		surf->client_buffer = buffer;
	}
	_mali_surface_deref( vg_surface );

	egl->linker->vg_func.unlock_image_ptrset( tstate->api_vg->context->api_context );
	return surf;
}


void __egl_vg_destroy_pbuffer_from_client_buffer( egl_surface *surface, __egl_thread_state *tstate )
{
	if ( (EGLClientBuffer)NULL != surface->client_buffer )
	{
		__egl_main_context *egl = __egl_get_main_context();
		if ( NULL != tstate->api_vg && NULL != tstate->api_vg->context )
		{
			egl->linker->vg_func.image_deref( tstate->api_vg->context->api_context, (struct vg_image *)surface->client_buffer );
		}
		surface->client_buffer = (EGLClientBuffer)NULL;
	}
}

