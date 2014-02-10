/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "egl_config_extension.h"
#if EGL_KHR_image_ENABLE
#include <mali_system.h>
#include "egl_image.h"
#include "egl_thread.h"
#include "egl_misc.h"
#include "egl_handle.h"
#include "egl_platform.h"
#include "egl_config.h"
#include "egl_surface.h"

#if EGL_ANDROID_image_native_buffer_ENABLE
#include "egl_image_android.h"
#if MALI_ANDROID_API >= 15
#include <system/window.h>
#else
#include <ui/egl/android_natives.h>
#include <ui/android_native_buffer.h>
#endif
#endif

#include "egl_image_pixmap.h"
#include "egl_image_gles.h"
#include "egl_image_vg.h"
#include "shared/mali_surface.h"
#include "shared/mali_egl_image.h"

#if MALI_USE_UNIFIED_MEMORY_PROVIDER
#include <ump/ump.h>
#if !defined(UMP_VERSION_MAJOR) || UMP_VERSION_MAJOR == 1
#include <ump/ump_ref_drv.h>
#endif
#if EGL_KHR_image_system_ENABLE
#include <shared/mali_image.h>
#endif
#endif

#if defined(HAVE_ANDROID_OS)
#include "egl_image_android.h"
#include <android/log.h>
#endif

void _egl_image_set_default_properties( egl_image_properties *prop )
{
	MALI_DEBUG_ASSERT_POINTER( prop );

	prop->num_components = 4;
	prop->min_bits[0] = 8;
	prop->min_bits[1] = 8;
	prop->min_bits[2] = 8;
	prop->min_bits[3] = 8;
	prop->image_format = EGL_NONE;
	prop->colorspace = EGL_COLORSPACE_BT_601;
	prop->colorrange = EGL_REDUCED_RANGE_KHR;
	prop->alpha_format = EGL_ALPHA_FORMAT_PRE;
	prop->miplevels = 1; /* default to only accepting 1 mipmap level */
}

EGLBoolean __egl_lock_image( egl_image *image )
{
	mali_err_code err;

	err = _mali_sys_lock_try_lock( image->lock );
	if ( MALI_ERR_NO_ERROR != err ) return EGL_FALSE;

	/* TODO: lock each non-mirrored entry */
	if ( NULL != image->image_mali )
	{
		if ( NULL != image->image_mali->pixel_buffer[0][0] )
		{
			_mali_surface_access_lock( image->image_mali->pixel_buffer[0][0] );
			_mali_surface_trigger_event( image->image_mali->pixel_buffer[0][0], image->image_mali->pixel_buffer[0][0]->mem_ref, MALI_SURFACE_EVENT_CPU_ACCESS );
			_mali_surface_access_unlock( image->image_mali->pixel_buffer[0][0] );
		}
	}

	return EGL_TRUE;
}

EGLBoolean __egl_unlock_image( egl_image *image )
{
	mali_err_code err;

	err = _mali_sys_lock_try_lock( image->lock );

	/* TODO: lock each non-mirrored entry */
	if ( NULL != image->image_mali )
	{
		if ( NULL != image->image_mali->pixel_buffer[0][0] )
		{
			_mali_surface_access_lock( image->image_mali->pixel_buffer[0][0] );
			_mali_surface_trigger_event( image->image_mali->pixel_buffer[0][0], image->image_mali->pixel_buffer[0][0]->mem_ref, MALI_SURFACE_EVENT_CPU_ACCESS_DONE );
			_mali_surface_access_unlock( image->image_mali->pixel_buffer[0][0] );
		}
	}

	if ( MALI_ERR_NO_ERROR != err )
	{
		_mali_sys_lock_unlock( image->lock );
		return EGL_TRUE;
	}
	_mali_sys_lock_unlock( image->lock );


	return EGL_FALSE;
}

void _egl_release_image( egl_image *image )
{
	EGLBoolean retval;

	retval = __egl_remove_image_handle( image );
	MALI_IGNORE( retval );

	if ( NULL != image->prop )
	{
		_mali_sys_free( image->prop );
	}

	_mali_sys_lock_destroy( image->lock );

	_mali_sys_free( image );
}

EGLBoolean _egl_destroy_image( egl_image* image, EGLBoolean force_release )
{
	MALI_DEBUG_ASSERT_POINTER( image );

	switch ( image->target )
	{
		case EGL_NATIVE_PIXMAP_KHR:
			if ( NULL != image->buffer )
			{
#if !defined(EGL_BACKEND_X11)
				__egl_platform_unmap_pixmap( MALI_STATIC_CAST(EGLNativePixmapType)(image->buffer), image );
				image->buffer = NULL;
#endif
			}
			break;
		case EGL_VG_PARENT_IMAGE_KHR:
		case EGL_GL_TEXTURE_2D_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
		case EGL_GL_RENDERBUFFER_KHR:
#if EGL_KHR_image_system_ENABLE
			__egl_platform_unmap_image_buffer( image->release_buffer );
			image->release_buffer = NULL;
#endif
			break;
#if EGL_ANDROID_image_native_buffer_ENABLE
		case EGL_NATIVE_BUFFER_ANDROID:
			if ( NULL != image->buffer )
			{
				_egl_image_unmap_ANDROID_native_buffer( image->buffer );
			}
			break;
#endif
	}

	/* unmap any mapped sessions */
	mali_image_unlock_all_sessions( image->image_mali );
	image->current_session_id = -1;

	if ( EGL_TRUE == force_release )
	{
		__egl_unlock_image( image );
	}

	if ( EGL_TRUE == __egl_lock_image( image ) )
	{
		mali_image* image_mali = image->image_mali;
		/* image is not locked, we can dereference the surfaces */
		if ( NULL != image_mali )
		{
			if ( NULL != image_mali->pixel_buffer[0][0] )
			{
				_mali_surface_access_lock( image_mali->pixel_buffer[0][0] );
				_mali_surface_trigger_event( image_mali->pixel_buffer[0][0], image->image_mali->pixel_buffer[0][0]->mem_ref, MALI_SURFACE_EVENT_CPU_ACCESS_DONE ); /* TODO: move this to a proper place */
				_mali_surface_access_unlock( image_mali->pixel_buffer[0][0] );
			}
		}
		image->image_mali = NULL;
		__egl_unlock_image( image );

#if EGL_BACKEND_X11
		image->image_mali = image_mali;
		__egl_platform_unmap_pixmap( MALI_STATIC_CAST(EGLNativePixmapType)(image->buffer), image );
		image->image_mali = NULL;
#endif
		if ( NULL != image_mali )
		{
			mali_image_deref_surfaces( image_mali );
			mali_image_deref( image_mali );
		}

		_egl_release_image( image );

		return EGL_TRUE;
	}

	return EGL_FALSE;
}

egl_image* _egl_create_image( void )
{
	egl_image *image = NULL;

	image = (egl_image *)_mali_sys_calloc( 1, sizeof(egl_image) );
	MALI_CHECK_NON_NULL( image, NULL );

	image->is_valid = EGL_TRUE;

	image->prop = (egl_image_properties*)_mali_sys_calloc( 1, sizeof(egl_image_properties) );
	if ( NULL ==  image->prop )
	{
		_mali_sys_free( image );
		return NULL;
	}

	image->current_session_id = -1;
	image->lock = _mali_sys_lock_create();
	if ( MALI_NO_HANDLE == image->lock )
	{
		_mali_sys_free( image->prop );
		_mali_sys_free( image );
		image = NULL;

		return NULL;
	}

	return image;
}

EGLBoolean _egl_image_is_sibling( egl_display *display,
                                  egl_context *context,
                                  EGLClientBuffer buffer,
                                  EGLenum target,
                                  void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	mali_named_list    *list;
	__egl_main_context *egl;
	egl_image          *curr_image;
	u32                iterator;

	MALI_IGNORE( display );
	MALI_IGNORE( context );
	MALI_IGNORE( tstate );

	egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( egl );

	list = egl->egl_images;
	MALI_DEBUG_ASSERT_POINTER( list );

	__mali_named_list_lock( list );

	curr_image = (egl_image *)__mali_named_list_iterate_begin( list, &iterator );

	while ( NULL != curr_image )
	{
		/* gles and vg sibling check is performed inside respective drivers */
		if ( target == curr_image->target )
		{
			if ( buffer == curr_image->buffer ) break;
		}
		curr_image = (egl_image *)__mali_named_list_iterate_next( list, &iterator );
	}

	__mali_named_list_unlock( list );

	return ( curr_image != NULL ? EGL_TRUE : EGL_FALSE );
}

EGLImageKHR _egl_create_image_KHR( EGLDisplay dpy,
                                   EGLContext ctx,
                                   EGLenum target,
                                   EGLClientBuffer buffer,
                                   EGLint *attr_list,
                                   void *thread_state,
                                   void *buffer_data)
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLImageKHR image_handle = EGL_NO_IMAGE_KHR;
	EGLint error = EGL_SUCCESS;
	egl_display *display = NULL;
	egl_context *context = NULL;
	egl_image   *image = NULL;
#if EGL_KHR_image_system_ENABLE
	EGLClientBuffer client_buffer = (EGLClientBuffer)NULL;
	EGLenum target_type = 0;
	EGLBoolean new_buffer = EGL_TRUE;
#endif

	/* verify display and context */
	MALI_CHECK( EGL_NO_DISPLAY != (display = __egl_get_check_display( dpy, tstate ) ), EGL_NO_IMAGE_KHR );

	switch ( target )
	{
#if	EGL_KHR_image_system_ENABLE
		case EGL_IMAGE_NAME_KHR:
			if ( EGL_NO_CONTEXT != ctx )
			{
				__egl_set_error( EGL_BAD_PARAMETER, tstate );
				return EGL_NO_IMAGE_KHR;
			}

			error = EGL_BAD_ACCESS;
			client_buffer = __egl_verify_convert_image_name_to_client_buffer( display, (EGLImageNameKHR) buffer, &target_type, &error );
			if ( NULL == client_buffer )
			{

				__egl_set_error( error, tstate );
				return EGL_NO_IMAGE_KHR;
			}

			/* calling self, this should be safe as target type below will never be EGL_IMAGE_NAME_KHR avoiding infinite loop */
			return _egl_create_image_KHR(dpy, ctx, target_type, client_buffer, attr_list, thread_state, (void *)new_buffer);
#endif
		case EGL_NATIVE_PIXMAP_KHR:
			/**
	  		* If <target> is EGL_NATIVE_PIXMAP_KHR, <dpy> must be a valid display, <ctx> must be EGL_NO_CONTEXT;
	  		* If <target> is EGL_NATIVE_PIXMAP_KHR, and <ctx> is not EGL_NO_CONTEXT, the error EGL_BAD_PARAMETER is generated.
 			*/
			if ( EGL_NO_CONTEXT != ctx )
			{
				__egl_set_error( EGL_BAD_PARAMETER, tstate );
				return EGL_NO_IMAGE_KHR;
			}
						
			image = _egl_create_image_KHR_pixmap( display, context, buffer, attr_list, tstate );
			MALI_CHECK_NON_NULL( image, EGL_NO_IMAGE_KHR );
			image->image_mali->pixel_buffer[0][0]->flags |= MALI_SURFACE_FLAG_TRACK_SURFACE;
			break;

#ifdef EGL_MALI_VG
		case EGL_VG_PARENT_IMAGE_KHR:
			MALI_CHECK( EGL_NO_CONTEXT != (context = __egl_get_check_context( ctx, dpy, tstate ) ), EGL_NO_IMAGE_KHR );
			if ( EGL_IMAGE_VG_PARENT_IMAGE & context->egl_image_caps )
			{
				image = _egl_create_image_KHR_vg( display, context, target, buffer, attr_list, tstate, buffer_data );
			}
			else
			{
				if ( EGL_OPENGL_ES_API == context->api ) error = EGL_BAD_MATCH;
				else error = EGL_BAD_PARAMETER;
			}
			break;
#endif /* EGL_MALI_VG */

#ifdef EGL_MALI_GLES
		case EGL_GL_TEXTURE_2D_KHR:
			MALI_CHECK( EGL_NO_CONTEXT != (context = __egl_get_check_context( ctx, dpy, tstate ) ), EGL_NO_IMAGE_KHR );
			if ( EGL_IMAGE_GL_TEXTURE_2D_IMAGE & context->egl_image_caps )
			{
				image = _egl_create_image_KHR_gles( display, context, target, buffer, attr_list, tstate, buffer_data );
			}
			else
			{
				if ( EGL_OPENVG_API == context->api ) error = EGL_BAD_MATCH;
				else error = EGL_BAD_PARAMETER;
			}
			break;

		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
			MALI_CHECK( EGL_NO_CONTEXT != (context = __egl_get_check_context( ctx, dpy, tstate ) ), EGL_NO_IMAGE_KHR );
			if ( EGL_IMAGE_GL_TEXTURE_CUBEMAP_IMAGE & context->egl_image_caps )
			{
				image = _egl_create_image_KHR_gles( display, context, target, buffer, attr_list, tstate, buffer_data );
			}
			else
			{
				if ( EGL_OPENVG_API == context->api ) error = EGL_BAD_MATCH;
				else error = EGL_BAD_PARAMETER;
			}
			break;

		case EGL_GL_RENDERBUFFER_KHR:
			MALI_CHECK( EGL_NO_CONTEXT != (context = __egl_get_check_context( ctx, dpy, tstate ) ), EGL_NO_IMAGE_KHR );
			if ( EGL_IMAGE_GL_RENDERBUFFER_IMAGE & context->egl_image_caps )
			{
				image = _egl_create_image_KHR_gles( display, context, target, buffer, attr_list, tstate, buffer_data );
			}
			else
			{
				if ( EGL_OPENVG_API == context->api ) error = EGL_BAD_MATCH;
				else error = EGL_BAD_PARAMETER;
			}
			break;

#if EGL_ANDROID_image_native_buffer_ENABLE
		case EGL_NATIVE_BUFFER_ANDROID:
			ctx = EGL_NO_CONTEXT;
			image = _egl_create_image_ANDROID_native_buffer( display, NULL, buffer, attr_list, tstate );
			MALI_CHECK_NON_NULL( image, EGL_NO_IMAGE_KHR );
			break;
#endif

#endif /* EGL_MALI_GLES */

		default:
			error = EGL_BAD_PARAMETER;
			break;
	}

	/* set error state if target was not found, or if target is not supported by context */
	if ( EGL_SUCCESS != error ) __egl_set_error( error, tstate );

	/* the extension specific functions sets the error state */
	MALI_CHECK_NON_NULL( image, EGL_NO_IMAGE_KHR );
		
	/* keep the display handle for later */
	image->display_handle = dpy;
	image->context_handle = ctx;
	image->target = target;

	/* insert the image into the list of EGLImages, generate a handle */
	image_handle = __egl_add_image_handle( image );
	if ( EGL_NO_IMAGE_KHR == image_handle )
	{
		_egl_destroy_image( image, EGL_FALSE );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_IMAGE_KHR;
	}
	
	if(image->image_mali->pixel_buffer[0][0]->flags & MALI_SURFACE_FLAG_TRACK_SURFACE)
	{
		_mali_surface_set_event_callback( image->image_mali->pixel_buffer[0][0], MALI_SURFACE_EVENT_UPDATE_TEXTURE,
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_update_texture_callback,
		                                  MALI_STATIC_CAST(void *)image->buffer );

		_mali_surface_set_event_callback( image->image_mali->pixel_buffer[0][0], MALI_SURFACE_EVENT_GPU_READ,
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_gpu_read_callback,
		                                  NULL );

		_mali_surface_set_event_callback( image->image_mali->pixel_buffer[0][0], MALI_SURFACE_EVENT_GPU_READ_DONE,
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_gpu_read_done_callback,
		                                  NULL );

		_mali_surface_set_event_callback( image->image_mali->pixel_buffer[0][0], MALI_SURFACE_EVENT_CPU_ACCESS,
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_cpu_access_callback,
		                                  NULL );

		_mali_surface_set_event_callback( image->image_mali->pixel_buffer[0][0], MALI_SURFACE_EVENT_CPU_ACCESS_DONE,
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_cpu_access_done_callback,
		                                  NULL );

		_mali_surface_set_event_callback( image->image_mali->pixel_buffer[0][0], MALI_SURFACE_EVENT_GPU_WRITE, 
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_gpu_write_callback, 
		                                  NULL );

		_mali_surface_set_event_callback( image->image_mali->pixel_buffer[0][0], MALI_SURFACE_EVENT_GPU_WRITE_DONE, 
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_gpu_write_done_callback, 
		                                  NULL );
	}
	return image_handle;
}

EGLImageKHR _egl_create_image_internal( EGLDisplay dpy,
                                   egl_image_properties *properties,
                                   EGLint width, EGLint height,
                                   void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLImageKHR image_handle = EGL_NO_IMAGE_KHR;
	egl_image   *image = NULL;
	mali_surface_specifier sformat;
	mali_surface_alphaformat alphaformat = MALI_SURFACE_ALPHAFORMAT_NONPRE;
	u32 miplevels = 1;
	m200_texel_format texel_format = M200_TEXEL_FORMAT_ARGB_8888;
	m200_texture_addressing_mode texel_layout = M200_TEXTURE_ADDRESSING_MODE_LINEAR;

	/* verify display and context */
	MALI_CHECK( EGL_NO_DISPLAY != __egl_get_check_display( dpy, tstate ), EGL_NO_IMAGE_KHR );
	MALI_CHECK_NON_NULL( properties, EGL_NO_IMAGE_KHR );

	image = _egl_create_image();
	if ( NULL == image )
	{
		return EGL_NO_IMAGE_KHR;
	}

	_mali_sys_memcpy( image->prop, properties, sizeof(egl_image_properties) );

	switch ( image->prop->layout )
	{
		case MALI_EGL_IMAGE_LAYOUT_LINEAR:             texel_layout = MALI_PIXEL_LAYOUT_LINEAR; break;
		case MALI_EGL_IMAGE_LAYOUT_BLOCKINTERLEAVED:   texel_layout = MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS; break;
		default: /* defalt is linear */
			break;
	}

	switch ( image->prop->alpha_format )
	{
		case EGL_ALPHA_FORMAT_PRE:     alphaformat = MALI_SURFACE_ALPHAFORMAT_PRE; break;
		case EGL_ALPHA_FORMAT_NONPRE:  alphaformat = MALI_SURFACE_ALPHAFORMAT_NONPRE; break;
		default: /* default is nonpre */
			break;
	}

	_mali_sys_memset( &sformat, 0, sizeof(mali_surface_specifier) );

	_mali_surface_specifier_ex( &sformat, width, height, 0,
								MALI_PIXEL_FORMAT_NONE, texel_format, 
								_mali_texel_layout_to_pixel_layout(texel_layout), texel_layout, 
								MALI_FALSE, MALI_FALSE, 
								MALI_SURFACE_COLORSPACE_sRGB, alphaformat, 
								MALI_FALSE );

	image->image_mali = mali_image_create( miplevels, MALI_SURFACE_FLAG_DONT_MOVE, &sformat, image->prop->image_format, tstate->main_ctx->base_ctx );
	if ( NULL == image->image_mali )
	{
		_egl_destroy_image( image, EGL_FALSE );
		return EGL_NO_IMAGE_KHR;
	}

	/* no need to pre-allocate any buffers yet, since access to the underlying buffers has to be done through the mali_egl_image API */

	image->display_handle = dpy;
	image->context_handle = EGL_NO_CONTEXT;
	image->target = EGL_NONE;
	image->buffer = (EGLClientBuffer)NULL;

	/* insert the image into the list of EGLImages, generate a handle */
	image_handle = __egl_add_image_handle( image );
	if ( EGL_NO_IMAGE_KHR == image_handle )
	{
		_egl_destroy_image( image, EGL_FALSE );
		return EGL_NO_IMAGE_KHR;
	}

	/* TODO: add surface event callbacks for GPU_TEXTURE and CPU_WRITE */

	return image_handle;
}

EGLBoolean _egl_destroy_image_KHR( EGLDisplay dpy, EGLImageKHR image, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	egl_display *display = NULL;
	egl_image *imgptr = NULL;
	EGLBoolean image_destroyed = EGL_FALSE;

	MALI_CHECK( EGL_NO_DISPLAY != (display = __egl_get_check_display( dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( NULL != (imgptr = __egl_get_check_image( image, tstate ) ), EGL_FALSE );

	/* verify that the display is the same as the one used to create the image */
	if ( imgptr->display_handle != dpy )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_FALSE;
	}

	/* Tag the image as invalid for usage in client APIs */
	imgptr->is_valid = MALI_FALSE;

	image_destroyed = _egl_destroy_image( imgptr, EGL_FALSE );
	if ( EGL_FALSE == image_destroyed )
	{
		MALI_DEBUG_PRINT( 2, ("EGLImage: eglImageDestroyKHR called when image still has references\n") );
	}

	MALI_IGNORE(display);

	return EGL_TRUE;
}

MALI_EXPORT egl_image* __egl_get_image_ptr_implicit( EGLImageKHR image_handle, EGLint usage )
{
	egl_image *image = NULL;

	image = __egl_get_image_ptr( image_handle );
	MALI_CHECK_NON_NULL( image, NULL );

	/* don't allow API side usage of image if it is tagged as destroyed */
	if ( EGL_FALSE == image->is_valid ) return NULL;

	MALI_IGNORE( usage );

	return image;
}

#if EGL_KHR_image_system_ENABLE

EGLClientNameKHR _egl_get_client_name_KHR(EGLDisplay dpy, void *thread_state)
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLClientNameKHR handle = NULL;
	PidType pid = 0;
	egl_display *display;

	MALI_CHECK( EGL_NO_DISPLAY != ( display = __egl_get_check_display( dpy, tstate ) ), EGL_NO_CLIENT_NAME_KHR );

	pid = _mali_sys_get_pid();

	display->client_handles = __egl_get_add_client_name_handles( display );
	if( !display->client_handles )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_CLIENT_NAME_KHR;
	}

	/* check if name for this client is already registered for this display, not an error if handle already exists */
	handle = __egl_get_client_name( display, pid );
	MALI_CHECK( 0 == handle, handle );

	handle = __egl_add_client_name( display, pid);
	MALI_CHECK( EGL_NO_CLIENT_NAME_KHR == handle,  handle);
	__egl_set_error( EGL_BAD_ALLOC, tstate );
	return handle;
}

MALI_STATIC EGLBoolean _egl_export_image_parse_attribute_list( const EGLint * attr_list, __egl_thread_state *tstate )
{

	EGLint *attrib = (EGLint*)attr_list;

	if ( NULL != attrib )
	{
		while ( EGL_NONE != attrib[0] )
		{
			switch ( attrib[0] )
			{
				/* no attributes allowed for now */
				default:
					__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
					return EGL_FALSE;
			}
		}
	}
	return EGL_TRUE;
}

EGLImageNameKHR _egl_export_image_KHR( EGLDisplay dpy,
                                       EGLImageKHR image,
                                       EGLClientNameKHR target_client,
                                       const EGLint *attrib_list,
                                       void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLImageNameKHR handle = EGL_NO_IMAGE_NAME_KHR;
	egl_image *imgptr = NULL;
	egl_display *display = NULL;

	MALI_CHECK( EGL_NO_DISPLAY != (display = __egl_get_check_display( dpy, tstate ) ), EGL_NO_IMAGE_NAME_KHR );
	MALI_CHECK( NULL != (imgptr = __egl_get_check_image( image, tstate ) ), EGL_NO_IMAGE_NAME_KHR );

	/* If the image was not created with the flag to be used as egl_image_system then return error */
	if ( !imgptr->use_as_system_image )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return handle;
	}

	if ( !__egl_verify_client_name_valid( display, target_client ) )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return handle;
	}

	if ( _egl_export_image_parse_attribute_list(attrib_list, tstate) )
	{
		display->shared_image_names = __egl_get_add_image_name_handles( display );
		if( !display->shared_image_names )
		{
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			return EGL_NO_IMAGE_NAME_KHR;
		}

		/* check if the same image has already been exported to the same client, not an error if already exists */
		handle = __egl_get_image_name( display, imgptr, target_client );
		MALI_CHECK( 0 == handle, handle );

		handle = __egl_add_image_name( display, imgptr, target_client);
		MALI_CHECK( EGL_NO_IMAGE_NAME_KHR == handle,  handle);
		__egl_set_error( EGL_BAD_ALLOC, tstate );
	}

	return handle;
}

EGLBoolean _egl_make_image_sharable( egl_image *image)
{
	EGLBoolean retval = EGL_FALSE;
	/* egl_image is made from texture and wants to be sharable, creating new ump memory and copying texture into it */
	mali_mem_handle mem_handle = MALI_NO_HANDLE;
	mali_shared_mem_ref *mem_ref = NULL;
	mali_shared_mem_ref *old_mem_ref = NULL;
	u32 size = 0;
	ump_handle ump = UMP_INVALID_MEMORY_HANDLE;
	mali_surface *surface = NULL;

	MALI_CHECK( ( image && image->image_mali && image->image_mali->pixel_buffer[0][0] ), retval );

	surface = image->image_mali->pixel_buffer[0][0];

	/* verify that image is not currently mapped, which means it is in use */
	if ( MALI_TRUE == mali_image_surface_is_mapped( image->image_mali, surface ) ) return retval;

	/* Create a UMP memory and copy into it only if the surface memory is not already ump memory */
	if ( UMP_INVALID_MEMORY_HANDLE == _mali_mem_get_ump_memory( surface->mem_ref->mali_memory ) )
	{
		if ( UMP_OK != ump_open() )     return retval;

		size = surface->format.pitch * surface->format.height;
		ump = ump_ref_drv_allocate( size, UMP_REF_DRV_CONSTRAINT_NONE );
		if ( UMP_INVALID_MEMORY_HANDLE == ump )     return retval;

		mem_handle = _mali_mem_wrap_ump_memory( image->image_mali->base_ctx, ump, 0 );
		mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mem_handle );
		if ( NULL == mem_ref )
		{
			_mali_mem_free( mem_handle );
			return retval;
		}

		_mali_surface_access_lock( surface );

		old_mem_ref = surface->mem_ref;
		_mali_sys_atomic_set( &mem_ref->owners, _mali_sys_atomic_get( &old_mem_ref->owners ) );
		_mali_sys_atomic_set( &mem_ref->usage, _mali_sys_atomic_get( &old_mem_ref->usage ) );

		/* Deferring the initialization of sync lock */
		mem_ref->sync_lock = old_mem_ref->sync_lock;
		mem_ref->sync_mutex = old_mem_ref->sync_mutex;
		mem_ref->sync_cond = old_mem_ref->sync_cond;

		/* copy the contents of data from old mali_memory to new mali_memory and deref the old surface */
		_mali_mem_copy( mem_handle, 0, old_mem_ref->mali_memory, surface->mem_offset, size );

		/* now we have everything from surface->mem_ref copied including references, freeing allocated data */
		_mali_mem_free( old_mem_ref->mali_memory );
		_mali_sys_free( old_mem_ref );

		/* attach new memory to the mali_surface */
		surface->mem_ref = mem_ref;
		surface->mem_offset = 0;
		_mali_surface_access_unlock( surface );
	}
	return EGL_TRUE;
}
#endif /* #if EGL_KHR_image_system_ENABLE*/

#endif /* #if EGL_KHR_image_ENABLE */
