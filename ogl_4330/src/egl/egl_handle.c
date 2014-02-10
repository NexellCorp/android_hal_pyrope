/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/mali_named_list.h>
#include "egl_thread.h"
#include "egl_main.h"
#include "egl_handle.h"
#include "egl_platform.h"
#include "egl_misc.h"

/* the three msb of a handle identifies what type of handle it is
 * msb 	handle
 * 000 	EGLDisplay
 * 010 	EGLSurface
 * 100 	EGLContext
 * 110 	EGLConfig
 * 001	EGLImage
 * 011	EGLSync
 */

/* The different handle types */
typedef enum egl_handle_type
{
	EGL_HANDLE_TYPE_DISPLAY = 1,
	EGL_HANDLE_TYPE_SURFACE = 2,
	EGL_HANDLE_TYPE_CONTEXT = 3,
	EGL_HANDLE_TYPE_CONFIG  = 4,
#if EGL_KHR_image_ENABLE
	EGL_HANDLE_TYPE_IMAGE   = 5,
#endif

#if EGL_KHR_reusable_sync_ENABLE
	EGL_HANDLE_TYPE_SYNC    = 6,
#endif
} egl_handle_type;

EGLBoolean __egl_create_handles( egl_display *display )
{
	MALI_DEBUG_ASSERT_POINTER( display );

	display->config = __mali_named_list_allocate();
	display->context = __mali_named_list_allocate();
	display->surface = __mali_named_list_allocate();
	display->sync = __mali_named_list_allocate();

	if ( (NULL == display->config) || (NULL == display->context) || (NULL == display->surface) || (NULL == display->sync) )
	{
		__egl_destroy_handles( display );
		return EGL_FALSE;
	}

	return EGL_TRUE;
}

void __egl_destroy_handles( egl_display* display )
{
	MALI_DEBUG_ASSERT_POINTER( display );

	if ( NULL != display->config ) __mali_named_list_free( display->config, NULL );
	if ( NULL != display->context ) __mali_named_list_free( display->context, NULL );
	if ( NULL != display->surface ) __mali_named_list_free( display->surface, NULL );
	if ( NULL != display->sync ) __mali_named_list_free( display->sync, NULL );

	display->config  = NULL;
	display->context = NULL;
	display->surface = NULL;
	display->sync = NULL;
}

void *__egl_get_handle_ptr( u32 handle, EGLDisplay display_handle, egl_handle_type handle_type )
{
	void *ptr = NULL;
	egl_display *dpy = NULL;
	__egl_main_context *egl = __egl_get_main_context();
	mali_named_list *list = NULL;

	if ( EGL_HANDLE_TYPE_DISPLAY != handle_type )
	{
		if ( 0 == HANDLE_IS_EGL_DISPLAY( (u32)display_handle ) ) return NULL;
		dpy = __mali_named_list_get( egl->display, HANDLE_EGL_MASK(display_handle) );
	}

	switch ( handle_type )
	{
		case EGL_HANDLE_TYPE_DISPLAY:
			if ( 0 == HANDLE_IS_EGL_DISPLAY( handle ) ) return NULL;
			list = egl->display;
			break;

		case EGL_HANDLE_TYPE_SURFACE:
			MALI_CHECK_NON_NULL( dpy, NULL );
			if ( 0 == HANDLE_IS_EGL_SURFACE( handle) ) return NULL;
			list = dpy->surface;
			break;

		case EGL_HANDLE_TYPE_CONTEXT:
			MALI_CHECK_NON_NULL( dpy, NULL );
			if ( 0 == HANDLE_IS_EGL_CONTEXT( handle ) ) return NULL;
			list = dpy->context;
			break;

		case EGL_HANDLE_TYPE_CONFIG:
			MALI_CHECK_NON_NULL( dpy, NULL );
			if ( 0 == HANDLE_IS_EGL_CONFIG( handle ) ) return NULL;
			list = dpy->config;
			break;

#if EGL_KHR_image_ENABLE
		case EGL_HANDLE_TYPE_IMAGE:
			if ( 0 == HANDLE_IS_EGL_IMAGE( handle ) ) return NULL;
			list = egl->egl_images;
			break;
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
		case EGL_HANDLE_TYPE_SYNC:
			MALI_CHECK_NON_NULL( dpy, NULL );
			if ( 0 == HANDLE_IS_EGL_SYNC( handle ) ) return NULL;
			list = dpy->sync;
			break;
#endif /* EGL_KHR_reusable_sync_ENABLE */

		default:
			MALI_DEBUG_ERROR( ("Error : Unknown handle type\n") );
			return NULL;
	}

	if ( list ) ptr = __mali_named_list_get( list, HANDLE_EGL_MASK(handle) );

	return ptr;
}

egl_display *__egl_get_display_ptr( EGLDisplay handle )
{
	egl_display *ptr = __egl_get_handle_ptr( (u32)handle, EGL_NO_DISPLAY, EGL_HANDLE_TYPE_DISPLAY );

	return ptr;
}

egl_config *__egl_get_config_ptr( EGLConfig handle, EGLDisplay display_handle )
{
	egl_config *ptr = __egl_get_handle_ptr( (u32)handle, display_handle, EGL_HANDLE_TYPE_CONFIG );

	return ptr;
}

egl_surface *__egl_get_surface_ptr( EGLSurface handle, EGLDisplay display_handle )
{
	egl_surface *ptr = __egl_get_handle_ptr( (u32)handle, display_handle, EGL_HANDLE_TYPE_SURFACE );

	return ptr;
}

egl_context *__egl_get_context_ptr( EGLContext handle, EGLDisplay display_handle )
{
	egl_context *ptr = __egl_get_handle_ptr( (u32)handle, display_handle, EGL_HANDLE_TYPE_CONTEXT );

	return ptr;
}

#if EGL_KHR_image_ENABLE
MALI_EXPORT egl_image *__egl_get_image_ptr( EGLImageKHR handle )
{
	egl_image *ptr = __egl_get_handle_ptr( (u32)handle, EGL_NO_DISPLAY, EGL_HANDLE_TYPE_IMAGE );

	return ptr;
}
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
egl_sync *__egl_get_sync_ptr( EGLSyncKHR handle, EGLDisplay display_handle )
{
	egl_sync *ptr = __egl_get_handle_ptr( (u32)handle, display_handle, EGL_HANDLE_TYPE_SYNC );

	return ptr;
}
#endif /* EGL_KHR_reusable_sync_ENABLE */


u32 __egl_get_handle( void *ptr, EGLDisplay display_handle, egl_handle_type handle_type )
{
	void *data = NULL;
	u32 iterator;
	egl_display *dpy = NULL;
	mali_named_list *list = NULL;
	__egl_main_context *egl = __egl_get_main_context();

	MALI_CHECK_NON_NULL( ptr, EGL_FALSE );

    if ( EGL_HANDLE_TYPE_DISPLAY != handle_type )
    {
        if ( 0 == HANDLE_IS_EGL_DISPLAY( (u32)display_handle ) ) return 0;
        dpy = __mali_named_list_get( egl->display, HANDLE_EGL_MASK(display_handle) );
    }

	switch ( handle_type )
	{
		case EGL_HANDLE_TYPE_DISPLAY:
			list = egl->display;
			break;

		case EGL_HANDLE_TYPE_SURFACE:
			MALI_CHECK_NON_NULL( dpy, EGL_FALSE );
			list = dpy->surface;
			break;

		case EGL_HANDLE_TYPE_CONTEXT:
			MALI_CHECK_NON_NULL( dpy, EGL_FALSE );
			list = dpy->context;
			break;

		case EGL_HANDLE_TYPE_CONFIG:
			MALI_CHECK_NON_NULL( dpy, EGL_FALSE );
			list = dpy->config;
			break;

#if EGL_KHR_image_ENABLE
		case EGL_HANDLE_TYPE_IMAGE:
			list = egl->egl_images;
			break;
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
		case EGL_HANDLE_TYPE_SYNC:
			MALI_CHECK_NON_NULL( dpy, EGL_FALSE );
			list = dpy->sync;
			break;
#endif /* EGL_KHR_reusable_sync_ENABLE */

		default:
			MALI_DEBUG_ERROR( ("Error : Unknown handle type\n") );
			return EGL_FALSE;
	}

	data = __mali_named_list_iterate_begin( list, &iterator );
	while( NULL != data )
	{
		if ( ptr == data )
		{
			return iterator;
		}
		data = __mali_named_list_iterate_next( list, &iterator );
	}

	return EGL_FALSE;
}

EGLDisplay __egl_get_display_handle( egl_display *display )
{
	u32 handle;

	handle = __egl_get_handle( display, EGL_NO_DISPLAY, EGL_HANDLE_TYPE_DISPLAY );
	MALI_CHECK( 0 != handle, EGL_NO_DISPLAY );

	return HANDLE_EGL_DISPLAY(handle);
}

EGLConfig __egl_get_config_handle( egl_config *config, EGLDisplay display_handle )
{
	u32 handle;

	handle = __egl_get_handle( config, display_handle, EGL_HANDLE_TYPE_CONFIG );
	MALI_CHECK( 0 != handle, (EGLConfig)NULL );

	return HANDLE_EGL_CONFIG(handle);
}

EGLint __egl_get_config_handles( EGLConfig *config, EGLDisplay display_handle, EGLint count )
{
	u32 iterator;
	EGLint i = 0;
	void *data = NULL;
	egl_display *dpy = NULL;

	dpy = __egl_get_display_ptr( display_handle );
	MALI_CHECK_NON_NULL( dpy, i );

	data = __mali_named_list_iterate_begin( dpy->config, &iterator );
	while( (i < count) && (NULL != data) )
	{
		config[i++] = HANDLE_EGL_CONFIG(iterator);
		data = __mali_named_list_iterate_next( dpy->config, &iterator );
	}

	return i;
}

EGLConfig __egl_get_config_handle_by_id( EGLint id, EGLDisplay display_handle )
{
	egl_config *config = NULL;
	egl_display *dpy = NULL;
	u32 iterator;

	dpy = __egl_get_display_ptr( display_handle );
	MALI_CHECK_NON_NULL( dpy, (EGLConfig)NULL );

	config = (egl_config *)__mali_named_list_iterate_begin( dpy->config, &iterator );
	while ( NULL != config )
	{
		if ( id == config->config_id ) return HANDLE_EGL_CONFIG(iterator);
		config = (egl_config *)__mali_named_list_iterate_next( dpy->config, &iterator );
	}

	return (EGLConfig)NULL;
}

EGLSurface __egl_get_surface_handle( egl_surface *surface, EGLDisplay display_handle )
{
	u32 handle;

	handle = __egl_get_handle( surface, display_handle, EGL_HANDLE_TYPE_SURFACE );
	MALI_CHECK( 0 != handle, EGL_NO_SURFACE );

	return HANDLE_EGL_SURFACE(handle);
}

EGLSurface __egl_get_context_handle( egl_context *context, EGLDisplay display_handle )
{
	u32 handle;

	handle = __egl_get_handle( context, display_handle, EGL_HANDLE_TYPE_CONTEXT );
	MALI_CHECK( 0 != handle, EGL_NO_CONTEXT );

	return HANDLE_EGL_CONTEXT(handle);
}

#if EGL_KHR_image_ENABLE
EGLImageKHR __egl_get_image_handle( egl_image *image )
{
	u32 handle;

	handle = __egl_get_handle( image, EGL_NO_DISPLAY, EGL_HANDLE_TYPE_IMAGE );
	MALI_CHECK( 0 != handle, EGL_NO_IMAGE_KHR );

	return HANDLE_EGL_IMAGE(handle);
}
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
EGLSyncKHR __egl_get_sync_handle( egl_sync *sync, EGLDisplay display_handle )
{
	u32 handle;

	handle = __egl_get_handle( sync, display_handle, EGL_HANDLE_TYPE_SYNC );
	MALI_CHECK( 0 != handle, EGL_NO_SYNC_KHR );

	return HANDLE_EGL_SYNC(handle);
}
#endif /* EGL_KHR_reusable_sync_ENABLE */


u32 __egl_add_handle( void *ptr, EGLDisplay display_handle, egl_handle_type handle_type )
{
	u32 handle;
	egl_display *dpy = NULL;
	__egl_main_context *egl = __egl_get_main_context();
	mali_named_list *list = NULL;
	mali_err_code retval;

    if ( EGL_HANDLE_TYPE_DISPLAY != handle_type )
    {
        if ( 0 == HANDLE_IS_EGL_DISPLAY( (u32)display_handle ) ) return EGL_FALSE;
        dpy = __mali_named_list_get( egl->display, HANDLE_EGL_MASK(display_handle) );
    }

	switch ( handle_type )
	{
		case EGL_HANDLE_TYPE_DISPLAY:
			handle = (u32)__egl_get_display_handle( (egl_display *)ptr );
			MALI_CHECK( EGL_FALSE == handle, handle );
			list = egl->display;
			break;

		case EGL_HANDLE_TYPE_SURFACE:
			handle = (u32)__egl_get_surface_handle( (egl_surface *)ptr, display_handle );
			MALI_CHECK( EGL_FALSE == handle, handle );
			MALI_DEBUG_ASSERT_POINTER( dpy );
			list = dpy->surface;
			break;

		case EGL_HANDLE_TYPE_CONTEXT:
			handle = (u32)__egl_get_context_handle( (egl_context *)ptr, display_handle );
			MALI_CHECK( EGL_FALSE == handle, handle );
			MALI_DEBUG_ASSERT_POINTER( dpy );
			list = dpy->context;
			break;

		case EGL_HANDLE_TYPE_CONFIG:
			handle = (u32)__egl_get_config_handle( (egl_config *)ptr, display_handle );
			MALI_CHECK( EGL_FALSE == handle, handle );
			MALI_DEBUG_ASSERT_POINTER( dpy );
			list = dpy->config;
			break;

#if EGL_KHR_image_ENABLE
		case EGL_HANDLE_TYPE_IMAGE:
			handle = (u32)__egl_get_image_handle( (egl_image *)ptr );
			MALI_CHECK( EGL_FALSE == handle, handle );
			list = egl->egl_images;
			break;
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
		case EGL_HANDLE_TYPE_SYNC:
			handle = (u32)__egl_get_sync_handle( (egl_sync *)ptr, display_handle );
			MALI_CHECK( EGL_FALSE == handle, handle );
			MALI_DEBUG_ASSERT_POINTER( dpy );
			list = dpy->sync;
			break;
#endif /* EGL_KHR_reusable_sync_ENABLE */

		default:
			MALI_DEBUG_ERROR( ("Error : Unknown handle type\n") );
			return EGL_FALSE;
	}

	handle = __mali_named_list_get_unused_name( list );
	retval = __mali_named_list_insert( list, handle, ptr);
	MALI_CHECK( MALI_ERR_NO_ERROR == retval, EGL_FALSE );

	return handle;
}

EGLDisplay __egl_add_display_handle( egl_display *display )
{
	u32 dpy;
	MALI_DEBUG_ASSERT_POINTER( display );
	dpy = __egl_add_handle( display , EGL_NO_DISPLAY, EGL_HANDLE_TYPE_DISPLAY );
	MALI_CHECK( 0 != dpy, EGL_NO_DISPLAY );

	return HANDLE_EGL_DISPLAY(dpy);
}

EGLConfig __egl_add_config_handle( egl_config *config, EGLDisplay display_handle )
{
	u32 conf;
	MALI_DEBUG_ASSERT_POINTER( config );
	conf = __egl_add_handle( config, display_handle, EGL_HANDLE_TYPE_CONFIG );
	MALI_CHECK( 0 != conf, (EGLConfig)NULL );

	return HANDLE_EGL_CONFIG(conf);
}

EGLSurface __egl_add_surface_handle( egl_surface *surface, EGLDisplay display_handle )
{
	u32 surf;
	MALI_DEBUG_ASSERT_POINTER( surface );
	surf = __egl_add_handle( surface, display_handle, EGL_HANDLE_TYPE_SURFACE );
	MALI_CHECK( 0 != surf, EGL_NO_SURFACE );

	return HANDLE_EGL_SURFACE(surf);
}

EGLContext __egl_add_context_handle( egl_context *context, EGLDisplay display_handle )
{
	u32 ctx;
	MALI_DEBUG_ASSERT_POINTER( context );
	ctx = __egl_add_handle( context, display_handle, EGL_HANDLE_TYPE_CONTEXT );
	MALI_CHECK( 0 != ctx, EGL_NO_CONTEXT );

	return HANDLE_EGL_CONTEXT(ctx);
}

#if EGL_KHR_image_ENABLE
EGLImageKHR __egl_add_image_handle( egl_image *image )
{
	u32 img;
	MALI_DEBUG_ASSERT_POINTER( image );
	img = __egl_add_handle( image, EGL_NO_DISPLAY, EGL_HANDLE_TYPE_IMAGE );
	MALI_CHECK( 0 != img, EGL_NO_IMAGE_KHR );

	return HANDLE_EGL_IMAGE(img);
}
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
EGLSyncKHR __egl_add_sync_handle( egl_sync *sync, EGLDisplay display_handle )
{
	u32 sync_handle;
	MALI_DEBUG_ASSERT_POINTER( sync );
	sync_handle = __egl_add_handle( sync, display_handle, EGL_HANDLE_TYPE_SYNC );
	MALI_CHECK( 0 != sync_handle, EGL_NO_SYNC_KHR );

	return HANDLE_EGL_SYNC(sync_handle);
}
#endif /* EGL_KHR_reusable_sync_ENABLE */


EGLBoolean __egl_remove_handle( void *ptr, EGLDisplay display_handle, egl_handle_type handle_type )
{
	u32 iterator;
	void *data = NULL;
	egl_display *dpy = NULL;
	__egl_main_context *egl = __egl_get_main_context();
	mali_named_list *list = NULL;

	switch ( handle_type )
	{
		case EGL_HANDLE_TYPE_DISPLAY:
			list = egl->display;
			break;

		case EGL_HANDLE_TYPE_SURFACE:
			dpy = __egl_get_display_ptr( display_handle );
			MALI_DEBUG_ASSERT_POINTER( dpy );
			list = dpy->surface;
			break;

		case EGL_HANDLE_TYPE_CONTEXT:
			dpy = __egl_get_display_ptr( display_handle );
			MALI_DEBUG_ASSERT_POINTER( dpy );
			list = dpy->context;
			break;

#if EGL_KHR_image_ENABLE
		case EGL_HANDLE_TYPE_IMAGE:
			list = egl->egl_images;
			break;
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
		case EGL_HANDLE_TYPE_SYNC:
			dpy = __egl_get_display_ptr( display_handle );
			MALI_DEBUG_ASSERT_POINTER( dpy );
			list = dpy->sync;
			break;
#endif /* EGL_KHR_reusable_sync_ENABLE */

		default:
			MALI_DEBUG_ERROR( ("Error : Unknown handle type\n") );
			return EGL_FALSE;
	}

	data = __mali_named_list_iterate_begin( list, &iterator );
	while ( NULL != data )
	{
		if ( ptr == data )
		{
			if ( EGL_HANDLE_TYPE_DISPLAY == handle_type )
			{
				egl_display *display = (egl_display *)data;

				/* The display is only removed if all contexts and surfaces are removed first,
				 * which means that we are left with the configs to remove - so we'll do that here.
				 * We can't remove the actual display here, since we do not want to do that in all
				 * cases - but it will be removed (if needed) in the caller of this function.
				 */

				if ( NULL != display->config )
				{
					__mali_named_list_free( display->config, NULL );
					 display->config = NULL;
				}

				if ( NULL != display->context )
				{
				    __mali_named_list_free( display->context, NULL );
				    display->context = NULL;
				}

				if ( NULL != display->surface )
				{
				    __mali_named_list_free( display->surface, NULL );
				    display->surface = NULL;
				}

				if ( NULL != display->sync )
				{
					__mali_named_list_free( display->sync, NULL );
					display->sync = NULL;
				}
			}
			else
			{
				__mali_named_list_remove( list, iterator );
			}
			return EGL_TRUE;
		}
		data = __mali_named_list_iterate_next( list, &iterator );
	}

	return EGL_FALSE;
}

EGLBoolean __egl_remove_display_handle( egl_display *display, EGLBoolean free_display )
{
	u32 iterator;
	egl_display *data = NULL;
	__egl_main_context *egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( display );

	MALI_CHECK( EGL_TRUE == __egl_remove_handle( display, EGL_NO_DISPLAY, EGL_HANDLE_TYPE_DISPLAY ), EGL_FALSE );

	if ( EGL_TRUE == free_display )
	{
		/* we know that the display was found, and exists */
		data = __mali_named_list_iterate_begin( egl->display, &iterator );
		while( NULL != data )
		{
			if ( display == data )
			{
				__mali_named_list_remove( egl->display, iterator );
				return EGL_TRUE;
			}
			data = __mali_named_list_iterate_next( egl->display, &iterator );
		}
	}

	return EGL_FALSE;
}

EGLBoolean __egl_remove_surface_handle( egl_surface *surface, EGLDisplay display_handle )
{
	MALI_DEBUG_ASSERT_POINTER( surface );
	return __egl_remove_handle( surface, display_handle, EGL_HANDLE_TYPE_SURFACE );
}

EGLBoolean __egl_remove_context_handle( egl_context *context, EGLDisplay display_handle )
{
	MALI_DEBUG_ASSERT_POINTER( context );
	return __egl_remove_handle( context, display_handle, EGL_HANDLE_TYPE_CONTEXT );
}

#if EGL_KHR_image_ENABLE
EGLBoolean __egl_remove_image_handle( egl_image *image )
{
	MALI_DEBUG_ASSERT_POINTER( image );
	return __egl_remove_handle( image, EGL_NO_DISPLAY, EGL_HANDLE_TYPE_IMAGE );
}
#endif

#if EGL_KHR_reusable_sync_ENABLE
EGLBoolean __egl_remove_sync_handle( egl_sync *sync, EGLDisplay display_handle )
{
	MALI_DEBUG_ASSERT_POINTER( sync );
	return __egl_remove_handle( sync, display_handle, EGL_HANDLE_TYPE_SYNC );
}
#endif /* EGL_KHR_reusable_sync_ENABLE */

EGLDisplay __egl_get_native_display_handle( EGLNativeDisplayType display )
{
	u32 iterator;
	egl_display *data = NULL;
	__egl_main_context *egl = __egl_get_main_context();

	data = __mali_named_list_iterate_begin( egl->display, &iterator );
	while ( NULL != data )
	{
		if ( display == data->native_dpy )
		{
			return (EGLDisplay)iterator;
		}
		data = __mali_named_list_iterate_next( egl->display, &iterator );
	}

	return EGL_NO_DISPLAY;
}

egl_display* __egl_get_first_display_handle( void )
{
	u32 iterator;
	egl_display *data = NULL;
	__egl_main_context *egl = __egl_get_main_context();

	data = __mali_named_list_iterate_begin( egl->display, &iterator );

	return data;
}

EGLBoolean __egl_native_window_handle_exists( EGLNativeWindowType window )
{
	EGLBoolean retval = EGL_FALSE;
	u32 iterator_display;
	u32 iterator_surface;
	egl_display *data_display = NULL;
	egl_surface *data_surface = NULL;
	__egl_main_context *egl = __egl_get_main_context();

	if ( (EGLNativeWindowType)NULL == window ) return EGL_FALSE;

	/* step through all surfaces on all displays, looking for the given window */
	data_display = __mali_named_list_iterate_begin( egl->display, &iterator_display );
	while ( NULL != data_display )
	{
		if ( data_display->flags & EGL_DISPLAY_INITIALIZED )
		{
			data_surface = __mali_named_list_iterate_begin( data_display->surface, &iterator_surface );
			while ( NULL != data_surface )
			{
				if ( MALI_EGL_WINDOW_SURFACE == data_surface->type )
				{
					if ( window == data_surface->win ) return EGL_TRUE;
				}
				data_surface = __mali_named_list_iterate_next( data_display->surface, &iterator_surface );
			}
		}
		data_display = __mali_named_list_iterate_next( egl->display, &iterator_display );
	}

	return retval;
}

EGLBoolean __egl_native_pixmap_handle_exists( EGLNativePixmapType pixmap )
{
	EGLBoolean retval = EGL_FALSE;
	u32 iterator_display;
	u32 iterator_surface;
	egl_display *data_display = NULL;
	egl_surface *data_surface = NULL;
	__egl_main_context *egl = __egl_get_main_context();

	/* step through all surfaces on all displays, looking for the given pixmap */
	data_display = __mali_named_list_iterate_begin( egl->display, &iterator_display );
	while ( NULL != data_display )
	{
		if ( data_display->flags & EGL_DISPLAY_INITIALIZED )
		{
			data_surface = __mali_named_list_iterate_begin( data_display->surface, &iterator_surface );

			while ( NULL != data_surface )
			{
				if ( MALI_EGL_PIXMAP_SURFACE == data_surface->type )
				{
					if ( pixmap == data_surface->pixmap ) return EGL_TRUE;
				}
				data_surface = __mali_named_list_iterate_next( data_display->surface, &iterator_surface );
			}
		}
		data_display = __mali_named_list_iterate_next( egl->display, &iterator_display );
	}

	return retval;
}

EGLBoolean __egl_client_buffer_handle_exists( EGLClientBuffer buffer )
{
	EGLBoolean retval = EGL_FALSE;
#ifdef EGL_MALI_VG
	u32 iterator_display;
	u32 iterator_surface;
	egl_display *data_display = NULL;
	egl_surface *data_surface = NULL;
	__egl_main_context *egl = __egl_get_main_context();

	/* step through all surfaces on all displays, looking for the given client buffer */
	data_display = __mali_named_list_iterate_begin( egl->display, &iterator_display );
	while ( NULL != data_display )
	{
		if ( data_display->flags & EGL_DISPLAY_INITIALIZED )
		{
			data_surface = __mali_named_list_iterate_begin( data_display->surface, &iterator_surface );

			while ( NULL != data_surface )
			{
				if ( MALI_EGL_PBUFFER_SURFACE == data_surface->type )
				{
					if ( buffer == data_surface->client_buffer ) return EGL_TRUE;
				}
				data_surface = __mali_named_list_iterate_next( data_display->surface, &iterator_surface );
			}
		}
		data_display = __mali_named_list_iterate_next( egl->display, &iterator_display );
	}
#endif

	return retval;
}


EGLBoolean __egl_release_surface_handles( EGLDisplay display_handle, __egl_thread_state *tstate )
{
	egl_display *display = NULL;
	egl_surface *data = NULL;
	u32 iterator = 0;
	u32 prev_iterator = 0;
	EGLBoolean retval = EGL_TRUE;
	EGLBoolean ret = EGL_TRUE;

	display = __egl_get_display_ptr( display_handle );
	MALI_CHECK_NON_NULL( display, EGL_TRUE );
	MALI_CHECK_NON_NULL( display->surface, EGL_TRUE );
	if ( 0 == __mali_named_list_size( display->surface ) ) return EGL_TRUE;

	/* step through all the surfaces, checking if any of them are current to any thread
	 * delete the surface if it is not current to any thread and not_current is EGL_TRUE,
	 * else delete it even if it is current
	 */
	data = __mali_named_list_iterate_begin( display->surface, &iterator );
	while( NULL != data )
	{
		ret = _egl_destroy_surface_internal( display_handle, data, EGL_TRUE, tstate );
		if ( NULL == __mali_named_list_get (display->surface, iterator ) )
		{
			/* removing an entry while iterating will have undefined behavior
			 * start at the previous one, or at the beginning if there was no previous one */
			if ( 0 == prev_iterator ) data = __mali_named_list_iterate_begin( display->surface, &iterator );
			else data = __mali_named_list_iterate_next( display->surface, &prev_iterator );
		}
		else
		{
			prev_iterator = iterator;
			data = __mali_named_list_iterate_next( display->surface, &iterator );
			retval = EGL_FALSE; /* all surfaces was not removed */
		}
	}
	MALI_IGNORE( ret );

	return retval;
}


EGLBoolean __egl_release_context_handles( EGLDisplay display_handle, __egl_thread_state *tstate )
{
	egl_display *display = NULL;
	egl_context *data = NULL;
	EGLBoolean ret = EGL_TRUE;
	u32 iterator = 0;
	u32 prev_iterator = 0;
	EGLBoolean retval = EGL_TRUE;

	display = __egl_get_display_ptr( display_handle );
	MALI_CHECK_NON_NULL( display, EGL_TRUE);
	MALI_CHECK_NON_NULL( display->context, EGL_TRUE);
	if ( 0 == __mali_named_list_size( display->context ) ) return EGL_TRUE;

	/* step through all the contexts, checking if any of them are current to any thread
	 * delete the surface if it is not current to any thread and not_current is EGL_TRUE,
	 * else delete it even if it is current
	 */
	data = __mali_named_list_iterate_begin( display->context, &iterator );
	while( NULL != data )
	{
		ret = _egl_destroy_context_internal( display_handle, data, EGL_TRUE, ( void * ) tstate );
		if ( NULL == __mali_named_list_get( display->context, iterator ) )
		{
			/* removing an entry while iterating will have undefined behavior,
			 * start at the previous one, or at the beginning if there was no previous one */
			if ( 0 == prev_iterator ) data = __mali_named_list_iterate_begin( display->context, &iterator );
			else data = __mali_named_list_iterate_next( display->context, &prev_iterator );
		}
		else
		{
			prev_iterator = iterator;
			data = __mali_named_list_iterate_next( display->context, &iterator );
			retval = EGL_FALSE; /* context was not removed */
		}
	}
	MALI_IGNORE( ret );

	return retval;
}

#if EGL_KHR_image_ENABLE
void __egl_release_image_handles( mali_named_list * egl_images, EGLDisplay display_handle, EGLBoolean force_release )
{
	egl_image *image = NULL;
	u32 iterator;

	if ( NULL == egl_images ) return;

	image = __mali_named_list_iterate_begin( egl_images, &iterator );
	while ( NULL != image )
	{
		if ( (display_handle == image->display_handle) || (EGL_NO_DISPLAY == display_handle) )
		{
			if ( EGL_TRUE == _egl_destroy_image( image, force_release ) )
			{
				image = __mali_named_list_iterate_begin( egl_images, &iterator );
			}
			else
			{
				MALI_DEBUG_PRINT( 2, ("WARNING: did not properly release image\n") );
				image = __mali_named_list_iterate_next( egl_images, &iterator );
			}
		}
		else
		{
			image = __mali_named_list_iterate_next( egl_images, &iterator );
		}
	}
}
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
void __egl_release_sync_handles( EGLDisplay display_handle )
{
	egl_display *display = NULL;
	egl_sync *data = NULL;
	u32 iterator = 0;

	display = __egl_get_display_ptr( display_handle );
	if ( NULL == display ) return;
	if ( NULL == display->sync ) return;
	if ( 0 == __mali_named_list_size( display->sync ) ) return;

	/* delete all sync objects - this will implicitly signal and wait for any
	 * outstanding eglClientWaitSyncKHR */
	data = __mali_named_list_iterate_begin( display->sync, &iterator );
	while( NULL != data )
	{
		_egl_destroy_sync( data );
		/* removing an entry while iterating will have undefined behavior,
  		 * start at the previous one, or at the beginning if there was no previous one */
		data = __mali_named_list_iterate_next( display->sync, &iterator );
	}
}
#endif /* EGL_KHR_reusable_sync_ENABLE */

void __egl_invalidate_context_handles( void )
{
	u32 iterator_display;
	u32 iterator_context;
	egl_display *display = NULL;
	egl_context *context = NULL;
	__egl_main_context *egl;

	__egl_all_mutexes_lock();
	egl = __egl_get_main_context();
	if ( NULL == egl ) return;

	/* tag all contexts as lost */
	if ( NULL != egl->display )
	{
		display = __mali_named_list_iterate_begin( egl->display, &iterator_display );
		while ( NULL != display )
		{
			context = __mali_named_list_iterate_begin( display->context, &iterator_context );
			while ( NULL != context )
			{
				/* Assuming that it's the drivers responsibility to make the context not current */
				context->is_lost = EGL_TRUE;
				context = __mali_named_list_iterate_next( display->context, &iterator_context );
			}
			display = __mali_named_list_iterate_next( egl->display, &iterator_display );
		}
	}
	__egl_all_mutexes_unlock();
}

egl_display* __egl_get_check_display( EGLDisplay handle, __egl_thread_state *tstate )
{
	egl_display *ptr = NULL;
	ptr = __egl_get_display_ptr( handle );
	MALI_CHECK ( NULL == ptr, ptr );
	__egl_set_error( EGL_BAD_DISPLAY, tstate );

	return NULL;
}

EGLBoolean __egl_check_display_initialized( egl_display *ptr, __egl_thread_state *tstate )
{
	/* ASSERT that a display can be initialized, initialized and terminating,
	 * but not uninitialized and terminating */
	MALI_DEBUG_ASSERT( EGL_DISPLAY_TERMINATING != (ptr->flags & (EGL_DISPLAY_INITIALIZED | EGL_DISPLAY_TERMINATING)),
					   ("EGLDisplay was terminating, but not initialized"));
	MALI_CHECK( !(ptr->flags & EGL_DISPLAY_INITIALIZED), EGL_TRUE );
	__egl_set_error( EGL_NOT_INITIALIZED, tstate );


	return EGL_FALSE;
}

EGLBoolean __egl_check_display_not_terminating( egl_display *ptr, __egl_thread_state *tstate )
{
	/* ASSERT that a display can be initialized, initialized and terminating,
	 * but not uninitialized and terminating */
	MALI_DEBUG_ASSERT( EGL_DISPLAY_TERMINATING != (ptr->flags & (EGL_DISPLAY_INITIALIZED | EGL_DISPLAY_TERMINATING)),
					   ("EGLDisplay was terminating, but not initialized"));
	MALI_CHECK( ptr->flags & EGL_DISPLAY_TERMINATING, EGL_TRUE );
	/*__egl_set_error( EGL_BAD_DISPLAY, tstate );*/
	__egl_set_error( EGL_NOT_INITIALIZED, tstate );

	return EGL_FALSE;
}

egl_config* __egl_get_check_config( EGLConfig handle, EGLDisplay display_handle, __egl_thread_state *tstate )
{
	egl_config *ptr = NULL;
	ptr = __egl_get_config_ptr( handle, display_handle );
	MALI_CHECK( NULL == ptr, ptr );
	__egl_set_error( EGL_BAD_CONFIG, tstate );

	return NULL;
}

egl_surface* __egl_get_check_surface( EGLSurface handle, EGLDisplay display_handle, __egl_thread_state *tstate )
{
	egl_surface *ptr = NULL;
	ptr = __egl_get_surface_ptr( handle, display_handle );
	if ( NULL != ptr )
	{
		if ( EGL_TRUE == ptr->is_valid ) return ptr;
	}
	__egl_set_error( EGL_BAD_SURFACE, tstate );

	return NULL;
}

egl_context* __egl_get_check_context( EGLContext handle, EGLDisplay display_handle, __egl_thread_state *tstate )
{
	egl_context *ptr = NULL;
	ptr = __egl_get_context_ptr( handle, display_handle );
	if ( NULL != ptr )
	{
		if ( EGL_TRUE == ptr->is_valid ) return ptr;
	}	
	__egl_set_error( EGL_BAD_CONTEXT, tstate );

	return NULL;
}

#if EGL_KHR_image_ENABLE
egl_image* __egl_get_check_image( EGLImageKHR handle, __egl_thread_state *tstate )
{
	egl_image *ptr = NULL;
	ptr = __egl_get_image_ptr( handle );
	MALI_CHECK ( NULL == ptr, ptr );
	__egl_set_error( EGL_BAD_PARAMETER, tstate );

	return NULL;
}
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
egl_sync* __egl_get_check_sync( EGLSyncKHR handle, EGLDisplay display_handle, __egl_thread_state *tstate )
{
	egl_sync *ptr = NULL;
	ptr = __egl_get_sync_ptr( handle, display_handle );
	if ( ptr != NULL && ptr->valid == MALI_FALSE ) ptr = NULL;
	MALI_CHECK( NULL == ptr, ptr );
	__egl_set_error( EGL_BAD_PARAMETER, tstate );

	return NULL;
}
#endif /* EGL_KHR_reusable_sync_ENABLE */

EGLBoolean __egl_check_null_value( EGLint *value, EGLint error, __egl_thread_state *tstate )
{
	MALI_CHECK( NULL == value, EGL_TRUE );
	__egl_set_error( error, tstate );

	return EGL_FALSE;
}

void __egl_lock_surface( EGLDisplay display_handle, EGLSurface surface_handle )
{
	egl_surface *surface = NULL;

	/* early out if we do not need to look up the surface */
	if ( surface_handle == EGL_NO_SURFACE ) return;

	__egl_main_mutex_lock();
	if ( NULL != __egl_get_handle_ptr( (u32)display_handle, EGL_NO_DISPLAY, EGL_HANDLE_TYPE_DISPLAY ) )
	{
		surface = __egl_get_handle_ptr( (u32)surface_handle, display_handle, EGL_HANDLE_TYPE_SURFACE );
		if ( NULL != surface )
		{
			_mali_sys_mutex_lock( surface->lock );
		}
	}
	__egl_main_mutex_unlock();
}

void __egl_unlock_surface( EGLDisplay display_handle, EGLSurface surface_handle )
{
	egl_surface *surface = NULL;

	if ( NULL == __egl_get_handle_ptr( (u32)display_handle, EGL_NO_DISPLAY, EGL_HANDLE_TYPE_DISPLAY ) ) return;

	surface = __egl_get_handle_ptr( (u32)surface_handle, display_handle, EGL_HANDLE_TYPE_SURFACE );
	if ( NULL != surface )
	{
		_mali_sys_mutex_unlock( surface->lock );
	}
}

