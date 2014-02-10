/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <mali_system.h>
#include <EGL/mali_egl.h>
#include "egl_thread.h"
#include "egl_handle.h"
#include "egl_context.h"
#include "egl_surface.h"

static const mali_thread_keys __egl_threads_key = MALI_THREAD_KEY_EGL_CONTEXT;

__egl_thread_state_api* __egl_get_current_thread_state_api( __egl_thread_state *tstate, EGLenum *api )
{
	__egl_thread_state_api *tstate_api = NULL;
	switch ( tstate->api_current )
	{
		case EGL_OPENGL_ES_API:
			tstate_api = tstate->api_gles;
			if( NULL != api ) *api = EGL_OPENGL_ES_API;
			break;

		case EGL_OPENVG_API:
			tstate_api = tstate->api_vg;
			if ( NULL != api ) *api = EGL_OPENVG_API;
			break;

		default:
			tstate_api = NULL;
			if ( NULL != api ) *api = EGL_NONE;
			break;
	}

	return tstate_api;
}

__egl_thread_state* __egl_get_current_thread_state( __main_mutex_action mutex_action )
{
	__egl_thread_state *tstate = (__egl_thread_state *)_mali_sys_thread_key_get_data( __egl_threads_key );

	if ( NULL == tstate )
	{
		mali_err_code err;
		__egl_main_context *egl;

		egl = __egl_get_main_context();
		MALI_CHECK( NULL != egl, NULL );

		tstate = (__egl_thread_state *)_mali_sys_calloc( 1, sizeof( __egl_thread_state ) );
		MALI_CHECK_NON_NULL( tstate, NULL );
#ifdef EGL_MALI_GLES
		tstate->api_current = EGL_OPENGL_ES_API;
#else
		tstate->api_current = EGL_NONE;
#endif
		tstate->api_vg = NULL;
		tstate->api_gles = NULL;
		tstate->error = EGL_SUCCESS;
		tstate->main_ctx = egl;
		tstate->id = _mali_sys_thread_get_current(); /* store the thread id */
		tstate->context_vg = NULL;
		tstate->context_gles = NULL;
		tstate->current_sync = NULL;
#if MALI_EXTERNAL_SYNC
		tstate->current_mali_fence_sync = NULL;
#endif

#if EGL_SURFACE_LOCKING_ENABLED
		tstate->worker_surface_lock = _mali_base_worker_create(MALI_FALSE);
		if ( MALI_BASE_WORKER_NO_HANDLE == tstate->worker_surface_lock )
		{
			_mali_sys_free( tstate );
			return NULL;
		}
#endif /* EGL_SURFACE_LOCKING_ENABLED */


#if MALI_ANDROID_API > 12
		tstate->worker = _mali_base_worker_create(MALI_FALSE);
		if ( MALI_BASE_WORKER_NO_HANDLE == tstate->worker )
		{
#if EGL_SURFACE_LOCKING_ENABLED
			_mali_base_worker_destroy( tstate->worker_surface_lock );
#endif
			_mali_sys_free( tstate );
			return NULL;
		}
#endif

		__mali_named_list_lock( egl->thread );
		err = __mali_named_list_insert( egl->thread, tstate->id, tstate );
		/* The above might fail if a thread using the same id has existed before
		 * This will happen whenever a thread is terminated without calling eglReleaseThread,
		 * leaving the entry in egl->thread intact, but the TLS data NULL
		 */
		if (MALI_ERR_FUNCTION_FAILED == err )
		{
			EGLBoolean retval = EGL_FALSE;
			EGLDisplay display_handle = EGL_NO_DISPLAY;
			__egl_thread_state *tstate_old = __mali_named_list_get( egl->thread, tstate->id );

			if( NULL != tstate_old )
			{
				if ( NULL != tstate_old->api_gles )
				{
					/* un-make any current gles context */
					display_handle = __egl_get_display_handle( tstate_old->api_gles->display );
					retval = _egl_bind_api( EGL_OPENGL_ES_API, tstate_old );
					if ( (NULL != tstate_old->api_gles->context || NULL != tstate_old->api_gles->draw_surface || NULL != tstate_old->api_gles->read_surface) && EGL_TRUE == retval )
					{
						retval = _egl_make_current( display_handle, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT, tstate_old );
					}
					_mali_sys_free( tstate_old->api_gles );
					tstate_old->api_gles = NULL;
				}
				if ( NULL != tstate_old->api_vg )
				{
					/* un-make any current vg context */
					display_handle = __egl_get_display_handle( tstate_old->api_vg->display );
					retval = _egl_bind_api( EGL_OPENVG_API, tstate_old );
					if ( (NULL != tstate_old->api_vg->context || NULL != tstate_old->api_vg->draw_surface || NULL != tstate_old->api_vg->read_surface) && EGL_TRUE == retval )
					{
						retval = _egl_make_current( display_handle, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT, tstate_old );
					}
					_mali_sys_free( tstate_old->api_vg );
					tstate_old->api_vg = NULL;
				}
				if ( NULL != tstate_old->current_sync ) 
				{
					_egl_sync_destroy_sync_node( tstate_old->current_sync );
					tstate_old->current_sync = NULL;
				}
#if MALI_EXTERNAL_SYNC
				if ( NULL != tstate_old->current_mali_fence_sync )
				{
					_mali_fence_sync_free( tstate_old->current_mali_fence_sync );
					tstate_old->current_mali_fence_sync= NULL;
				}
#endif
				_mali_sys_free( tstate_old );
				tstate_old = NULL;
			}
			MALI_IGNORE( retval );
			MALI_IGNORE( display_handle );

			err = __mali_named_list_set( egl->thread, tstate->id, tstate );
		}
		__mali_named_list_unlock( egl->thread );

		if ( MALI_ERR_NO_ERROR != err )
		{
#if MALI_ANDROID_API > 12
			if ( MALI_BASE_WORKER_NO_HANDLE != tstate->worker ) _mali_base_worker_destroy( tstate->worker );
#endif
			_mali_sys_free( tstate );
			return NULL;
		}

		err = _mali_sys_thread_key_set_data( __egl_threads_key, (void *)tstate );
		if ( MALI_ERR_NO_ERROR != err )
		{
#if MALI_ANDROID_API > 12
			if ( MALI_BASE_WORKER_NO_HANDLE != tstate->worker ) _mali_base_worker_destroy( tstate->worker );
#endif
			_mali_sys_free( tstate );
			return NULL;
		}
	}
	else if ( NULL == tstate->current_sync && NULL != tstate->main_ctx->base_ctx )
	{

		tstate->current_sync = (egl_sync_node*)_mali_sys_malloc( sizeof( *tstate->current_sync ) );
		if ( NULL == tstate->current_sync )
		{
			return NULL;
		}

		tstate->current_sync->parent = NULL;
		tstate->current_sync->child = NULL;
		tstate->current_sync->sync = NULL;
		tstate->current_sync->current = MALI_TRUE;

		tstate->current_sync->sync_handle = _mali_sync_handle_new(tstate->main_ctx->base_ctx);
		if( MALI_NO_HANDLE == tstate->current_sync->sync_handle )
		{
			_mali_sys_free( tstate->current_sync );
			tstate->current_sync = NULL;
			return NULL;
		}
	}
#if MALI_EXTERNAL_SYNC
	if( NULL == tstate->current_mali_fence_sync )
	{
		tstate->current_mali_fence_sync = _mali_fence_sync_alloc();
	}
#endif
	__egl_main_mutex_action( mutex_action );

	return tstate;
}

__egl_thread_state* __egl_get_current_thread_state_no_mutex( void )
{
	__egl_thread_state *tstate = (__egl_thread_state *)_mali_sys_thread_key_get_data( __egl_threads_key );
	if( NULL != tstate ) return tstate;

	return __egl_get_current_thread_state( EGL_MAIN_MUTEX_NOP );
}

__egl_thread_state* __egl_get_check_thread_state(void)
{
	return __egl_get_current_thread_state_no_mutex();
}

void __egl_release_current_thread_state( __main_mutex_action mutex_action )
{
	__egl_main_mutex_action( mutex_action );
}

EGLBoolean _egl_release_thread( void )
{
	EGLDisplay display_handle;
	EGLBoolean retval = EGL_TRUE;
	__egl_main_context *egl = NULL;
	mali_err_code err;

	__egl_thread_state *tstate = (__egl_thread_state *)_mali_sys_thread_key_get_data( __egl_threads_key );
	MALI_CHECK_NON_NULL( tstate, EGL_TRUE ); /* thread already released */

	egl = tstate->main_ctx;

	/* release any bound context */
	if ( NULL != tstate->api_gles )
	{
		display_handle = __egl_get_display_handle( tstate->api_gles->display );
		if ( EGL_FALSE == _egl_bind_api( EGL_OPENGL_ES_API, tstate ) ) return EGL_FALSE;
		retval = _egl_make_current( display_handle, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT, tstate );
		_mali_sys_free( tstate->api_gles );
		tstate->api_gles = NULL;
		tstate->context_gles = NULL;
	}
	if ( NULL != tstate->api_vg )
	{
		display_handle = __egl_get_display_handle( tstate->api_vg->display );
		if ( EGL_FALSE == _egl_bind_api( EGL_OPENVG_API, tstate ) ) return EGL_FALSE;
		retval = _egl_make_current( display_handle, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT, tstate );
		_mali_sys_free( tstate->api_vg );
		tstate->api_vg = NULL;
		tstate->context_vg = NULL;
	}

	/* release the thread state data */

	if( NULL != tstate->current_sync )
	{
		_egl_sync_destroy_sync_node( tstate->current_sync );
		tstate->current_sync = NULL;
	}
#if MALI_EXTERNAL_SYNC
    if( NULL != tstate->current_mali_fence_sync )
	{
		_mali_fence_sync_free( tstate->current_mali_fence_sync );
		tstate->current_mali_fence_sync = NULL;
	}
#endif

	__mali_named_list_lock( egl->thread );
	err = _mali_sys_thread_key_set_data( __egl_threads_key, (void *)NULL );
	__mali_named_list_remove( egl->thread, tstate->id );

#if MALI_ANDROID_API > 12
	if ( MALI_BASE_WORKER_NO_HANDLE != tstate->worker ) _mali_base_worker_destroy( tstate->worker );
#endif


#if defined( EGL_SURFACE_LOCKING_ENABLED )
	if ( MALI_BASE_WORKER_NO_HANDLE != tstate->worker_surface_lock )
	{
		_mali_base_worker_destroy( tstate->worker_surface_lock );
	}
#endif

	_mali_sys_free( tstate );
	__mali_named_list_unlock( egl->thread );

	MALI_IGNORE( err );
	MALI_IGNORE( retval );

	return EGL_TRUE;
}

