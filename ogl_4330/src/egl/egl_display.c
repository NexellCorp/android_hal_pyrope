/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <EGL/mali_egl.h>
#include "egl_platform.h"
#include "egl_context.h"
#include "egl_main.h"
#include "egl_misc.h"
#include "egl_thread.h"
#include "egl_handle.h"

#ifdef MALI_TEST_API
	#include <base/mali_test.h>
#endif

/* Internal Function definitions */

/* Internal function for opening mali on a display
 * Use this instead of __egl_main_open_mali() to ensure that mali resources
 * are only opened once across all displays.
 *
 * This will ASSERT if the display is initialized, since mali should already
 * be open by then.
 * If mali could not be opened on the display, then do not close mali on the
 * display. */
MALI_STATIC EGLBoolean __egl_display_open_mali( egl_display *dpy )
{
	__egl_main_context *egl;

	MALI_DEBUG_ASSERT_POINTER( dpy );
	MALI_DEBUG_ASSERT( !(dpy->flags & EGL_DISPLAY_INITIALIZED), ("Cannot open mali for an already-initialized display") );

	egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( egl );
	/* Unlikely to occur, but we check: */
	MALI_DEBUG_ASSERT( egl->displays_open < MALI_U32_MAX, ("Reference count overflow: too many display handles"));

	if ( 0 == egl->displays_open && EGL_FALSE == __egl_main_open_mali() )
	{
		/* No closing required, due to atomicity of __egl_main_open_mali(). Just return an error*/
		return EGL_FALSE;
	}

	/* Success */
	egl->displays_open++;

	return EGL_TRUE;
}

/* Internal function for closing mali on a display
 * Use this instead of __egl_main_close_mali() to ensure that mali resources
 * are only closed once all displays are closed.
 *
 * This will ASSERT if the display is initialized, because the display
 * contexts and surfaces should be released first.
 * Do not use if no displays are open */
MALI_STATIC void __egl_display_close_mali( egl_display *dpy )
{
	__egl_main_context *egl;

	MALI_DEBUG_ASSERT_POINTER( dpy );
	MALI_DEBUG_ASSERT( !(dpy->flags & EGL_DISPLAY_INITIALIZED), ("Cannot close mali on a display until it is terminated") );

	egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( egl );

	/* Check that we've not been called redundantly: */
	MALI_DEBUG_ASSERT( egl->displays_open > MALI_U32_MIN, ("Reference count underflow: closed mali when no displays were initialized"));

	/* If this is the last open display, then close mali */
	if (0 == --(egl->displays_open) )
	{
		__egl_main_close_mali();
	}
}

/* External function definitions */

EGLDisplay _egl_get_display( EGLNativeDisplayType display, void *thread_state )
{
	EGLDisplay handle;
	EGLBoolean default_dpy = EGL_FALSE;
	egl_display *dpy = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	if ( EGL_DEFAULT_DISPLAY == display ) 
	{
		display = __egl_platform_default_display();
		default_dpy = EGL_TRUE;
	}
	if ( EGL_FALSE == __egl_platform_display_valid(display) ) return EGL_NO_DISPLAY;

	/* search the display-list for an already existing display-entry */
	handle = __egl_get_native_display_handle( display );
	MALI_CHECK( EGL_NO_DISPLAY == handle, handle );

	dpy = (egl_display*)_mali_sys_calloc(1, sizeof(egl_display));
	if ( NULL == dpy )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_DISPLAY;
	}

	dpy->native_dpy = display;
	dpy->default_dpy = default_dpy;

	/* get the next available handle */
	handle = __egl_add_display_handle( dpy );
	if ( EGL_NO_DISPLAY == handle )
	{
		__egl_release_display( dpy, EGL_TRUE );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_DISPLAY;
	}

	return handle;
}

EGLDisplay _egl_get_current_display( void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api = NULL;

	tstate_api = __egl_get_current_thread_state_api( tstate, NULL );
	MALI_CHECK_NON_NULL( tstate_api, EGL_NO_DISPLAY );

	/* EGL 1.4 spec, section 3.7.4
	 * "The display for the current context in the calling thread, for the current rendering
	 * API, is returned. If there is no current context for the current rendering API,
	 * EGL_NO_DISPLAY is returned (this is not an error). "
	 */
	if ( EGL_NO_CONTEXT == tstate_api->context ) return EGL_NO_DISPLAY;

	return __egl_get_display_handle( tstate_api->display );
}

EGLBoolean _egl_initialize( EGLDisplay __dpy, EGLint *major, EGLint *minor, void *thread_state )
{
	egl_display *dpy = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );

	/* If display is unitialized, must attempt to open mali resources
	 * This must happen before platform interaction, as must init the platform
	 * layer first.
	 *
	 * __egl_display_open_mali will ASSERT if the display is initialized
	 * __egl_display_close_mali will ASSERT if the display is uninitialized
	 *
	 * These must be closed again if we fail any part of intialization, and
	 * we weren't previously initialized */
	if ( !(dpy->flags & EGL_DISPLAY_INITIALIZED)
		 && EGL_FALSE == __egl_display_open_mali( dpy ) )
	{
		/* No need to close mali resources - they were never opened */
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_FALSE;
	}

	if ( dpy->default_dpy )
	{
		/* If application is using EGL_DEFAULT_DISPLAY, which may be destroyed by eglTerminate.
		 * according to spec 3.2, eglInitialize should initialize it again, so above call 
		 * __egl_display_open_mali may get a new EGLNativeDisplayType, we need to update egl_display->native_dpy
	 	 */
		dpy->native_dpy = __egl_platform_default_display();
	}

	if ( EGL_FALSE == __egl_platform_display_valid( dpy->native_dpy ) )
	{
		/* Only close the resources if the display is not already initialized */
		if (!(dpy->flags & EGL_DISPLAY_INITIALIZED))
		{
			__egl_display_close_mali( dpy );
		}
		__egl_set_error( EGL_BAD_DISPLAY, tstate );
		return EGL_FALSE;
	}

	/* eventually initialize the windowing system */
	if ( !(dpy->flags & EGL_DISPLAY_INITIALIZED) )
	{
		if ( EGL_FALSE == __egl_platform_init_display( dpy->native_dpy, tstate->main_ctx->base_ctx ) )
		{
			__egl_display_close_mali( dpy );
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			return EGL_FALSE;
		}
		if ( EGL_FALSE == __egl_create_handles( dpy ) )
		{
			__egl_platform_deinit_display( dpy->native_dpy );
			__egl_display_close_mali( dpy );
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			return EGL_FALSE;
		}

		__egl_platform_display_get_format( dpy->native_dpy, &dpy->native_format );

		if ( EGL_FALSE == __egl_initialize_configs( __dpy ) )
		{
			__egl_destroy_handles( dpy );
			__egl_platform_deinit_display( dpy->native_dpy );
			__egl_display_close_mali( dpy );
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			return EGL_FALSE;
		}
	}

	dpy->flags |= EGL_DISPLAY_INITIALIZED ; /* tag display as initialized */
	dpy->flags &= ~(EGL_DISPLAY_TERMINATING);

	if ( NULL != major ) *major = MALI_EGL_MAJOR_VERSION;
	if ( NULL != minor ) *minor = MALI_EGL_MINOR_VERSION;

	return EGL_TRUE;
}

EGLBoolean _egl_terminate( EGLDisplay __dpy, void *thread_state )
{
	egl_display *dpy = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLBoolean released_surfaces;
	EGLBoolean released_contexts;
	EGLBoolean released_egl_images = EGL_TRUE;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );

	/* do nothing if display is not initialized */
	if ( !(dpy->flags & EGL_DISPLAY_INITIALIZED) ) return EGL_TRUE;

	/* do nothing if display is marked as terminating */
	if ( dpy->flags & EGL_DISPLAY_TERMINATING ) return EGL_TRUE;

	/* We need not check the display is valid - if it is not, must still allow
	 * termination */

	/* - only release what is not current
	 * - do not release the display, but place it in an uninitialized state
	 * - mark the display as terminating (this will be unmarked when a call to
	 *   egl_release_display is made)
	 */
	dpy->flags |= EGL_DISPLAY_TERMINATING;

	/* step through all contexts and surfaces bound to this display */
	/* remove those that are not current */
	released_surfaces = __egl_release_surface_handles( __dpy, tstate ); /* remove surfaces which are not current */
	released_contexts = __egl_release_context_handles( __dpy, tstate ); /* remove contexts which are not current */
#if EGL_KHR_image_ENABLE
	if ( NULL != tstate->main_ctx->egl_images )
	{
		released_egl_images = __mali_named_list_size( tstate->main_ctx->egl_images ) > 0 ? EGL_FALSE : EGL_TRUE;
	}
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE
	if ( __mali_named_list_size( dpy->sync ) > 0 )
	{
		MALI_DEBUG_PRINT( 2, ("WARNING: There are %i sync objects left - forced signaling and destruction\n", __mali_named_list_size( dpy->sync ) ) );
		__egl_release_sync_handles( __dpy ); /* signal and release sync objects */
	}
#endif /* EGL_KHR_reusable_sync_ENABLE */

	if ( (EGL_TRUE == released_surfaces) && (EGL_TRUE == released_contexts) && (EGL_TRUE == released_egl_images) )
	{
		EGLBoolean ret;
		/* If all surfaces, contexts and EGLImages are released, tag the display as uninitialized again */
		__egl_release_display( dpy, EGL_FALSE );
		ret = _egl_release_thread() ;
		MALI_IGNORE( ret );
	}

	return EGL_TRUE;
}

void __egl_release_display( egl_display *dpy, EGLBoolean free_display )
{
	EGLBoolean retval;
	__egl_main_context *egl = __egl_get_main_context();

	MALI_DEBUG_ASSERT_POINTER( egl );

#if EGL_KHR_image_ENABLE
	/* only release the egl images on the display that is being released */
	__egl_release_image_handles( egl->egl_images, __egl_get_display_handle( dpy ), free_display );
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_image_system_ENABLE
	__egl_remove_image_name_handle( dpy );    /* unregister client name and all the exported image names to this client */
	__egl_remove_client_name_handle( dpy );
#endif

	__egl_platform_deinit_display( dpy->native_dpy );
	retval = __egl_remove_display_handle( dpy, free_display );

	dpy->config = NULL;
	dpy->context = NULL;
	dpy->surface = NULL;
	if ( NULL != dpy->configs )
	{
		_mali_sys_free( dpy->configs );
		dpy->configs = NULL;
	}

	if ( EGL_TRUE == free_display )
	{
		_mali_sys_free( dpy );
		dpy = EGL_NO_DISPLAY;
	}
	else
	{
		/* Clear the terminating flag, and mark it as properly terminated */
		dpy->flags &= ~(EGL_DISPLAY_TERMINATING | EGL_DISPLAY_INITIALIZED);

		/* Close mali on this display */
		__egl_display_close_mali( dpy );
	}

	MALI_IGNORE(retval);
	MALI_IGNORE(egl);
}

void __egl_free_all_displays( void )
{
	u32 iterator;
	egl_display *display = NULL;
	EGLDisplay display_handle;
	EGLBoolean retval = EGL_TRUE;
	__egl_main_context *egl;
	__egl_thread_state *tstate;

	/* tstate could legally be NULL during certain termination conditions */

	/* Do nothing if we have no EGL main context */
	if ( EGL_FALSE == __egl_main_initialized() )
	{
		return;
	}

	egl = __egl_get_main_context();

	/* iterate through remaining thread and make sure contexts are set to be non current
	   but don't free the thread state as this is used in freeing surfaces and contexts later */
	if ( NULL != egl->thread )
	{
		tstate = __mali_named_list_iterate_begin( egl->thread, &iterator );
		while ( NULL != tstate )
		{
			/** NOTE: the unbinding of context here may cause mali resources to
			 * close if termination began, but did not complete. This is
			 * acceptable, since the display freeing code below will still work
			 * correctly, and closing mali after this function exits is still
			 * allowed. */
			if ( NULL != tstate->api_gles )
			{
				display_handle = __egl_get_display_handle( tstate->api_gles->display );
				retval = _egl_bind_api( EGL_OPENGL_ES_API, tstate );
				/* Only unbind resources if we've got some to unbind - otherwise this will fail and give the warning */
				if (NULL != tstate->api_gles->context
					 || NULL != tstate->api_gles->draw_surface
					 || NULL != tstate->api_gles->read_surface) 
				{
					retval = _egl_make_current( display_handle, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT, tstate );
#ifndef MALI_DEBUG_SKIP_CODE
					if ( EGL_FALSE==retval ) MALI_DEBUG_PRINT(1,("Warning: Failed to unbind a GLES display during termination"));
#endif
				}
				_mali_sys_free( tstate->api_gles );
				tstate->api_gles = NULL;
			}

			if ( NULL != tstate->api_vg )
			{
				display_handle = __egl_get_display_handle( tstate->api_vg->display );
				retval = _egl_bind_api( EGL_OPENVG_API, tstate );
				/* Only unbind resources if we've got some to unbind - otherwise this will fail and give the warning */
				if (NULL != tstate->api_vg->context
					 || NULL != tstate->api_vg->draw_surface
					 || NULL != tstate->api_vg->read_surface) 
				{
					retval = _egl_make_current( display_handle, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT, tstate );
#ifndef MALI_DEBUG_SKIP_CODE
					if ( EGL_FALSE==retval ) MALI_DEBUG_PRINT(1,("Warning: Failed to unbind a VG display during termination"));
#endif
				}
				_mali_sys_free( tstate->api_vg );
				tstate->api_vg = NULL;
			}
			__mali_named_list_remove( egl->thread, tstate->id);

			if ( NULL != tstate->current_sync ) 
			{
				_egl_sync_destroy_sync_node( tstate->current_sync );
				tstate->current_sync = NULL;
			}

#if EGL_SURFACE_LOCKING_ENABLED
			if ( MALI_BASE_WORKER_NO_HANDLE != tstate->worker_surface_lock )
			{
				_mali_base_worker_destroy( tstate->worker_surface_lock );
			}
#endif

			_mali_sys_free( tstate );

			tstate = __mali_named_list_iterate_begin( egl->thread, &iterator );
		}
	}

	display = __egl_get_first_display_handle();
	while ( NULL != display )
	{
		display_handle = __egl_get_display_handle( display );

		retval = __egl_release_surface_handles( display_handle, NULL ); /* remove surfaces */
#ifndef MALI_DEBUG_SKIP_CODE
		if ( EGL_FALSE==retval ) MALI_DEBUG_PRINT(1,("Warning: Failed to release all surface handles during termination"));
#endif

		retval = __egl_release_context_handles( display_handle, NULL ); /* remove contexts */
#ifndef MALI_DEBUG_SKIP_CODE
		if ( EGL_FALSE==retval ) MALI_DEBUG_PRINT(1,("Warning: Failed to release all context handles during termination"));
#endif

		__egl_release_display( display, EGL_TRUE ); /* remove display */
		display = __egl_get_first_display_handle();
	}

	MALI_IGNORE( retval );

}

