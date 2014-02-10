/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/m200_incremental_rendering.h>
#include <shared/m200_gp_frame_builder_inlines.h>
#include "egl_config_extension.h"
#if EGL_KHR_reusable_sync_ENABLE
#include <mali_system.h>
#include "egl_sync.h"
#include "egl_thread.h"
#include "egl_misc.h"
#include "egl_handle.h"
#include "egl_common.h"

#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
#include "egl_resource_profiling.h"
#endif

#if EGL_ANDROID_native_fencesync_ENABLE
#include <utils/Log.h>
#if MALI_EXTERNAL_SYNC == 1
#include "sync/mali_external_sync.h"
#include "mali_fence_sync.h"
#else
#error "MALI_EXTERNAL_SYNC option must be enabled for EGL Android native fence sync extension!"
#endif
#endif

MALI_STATIC egl_sync* _egl_create_sync( void )
{
	egl_sync *sync = NULL;

	sync = (egl_sync *)_mali_sys_calloc( 1, sizeof(egl_sync) );
	MALI_CHECK_NON_NULL( sync, NULL );

	sync->lock = _mali_sys_lock_create();
	if ( MALI_NO_HANDLE == sync->lock )
	{
		_mali_sys_free( sync );
		return NULL;
	}
	_mali_sys_lock_lock( sync->lock );
	_mali_sys_atomic_initialize( &sync->references, 1 );

	sync->status = EGL_UNSIGNALED_KHR,
	sync->condition = EGL_NONE;
	sync->dpy = EGL_NO_DISPLAY;
	sync->valid = MALI_TRUE;
	sync->destroy = MALI_FALSE;
	sync->fence_chain = NULL;
	sync->fence_mutex = MALI_NO_HANDLE;
#if EGL_ANDROID_native_fencesync_ENABLE
	sync->native_fence_obj = NULL;
#endif

	SYNC_DEBUG( _mali_sys_printf( "sync created ", sync ) );

	return sync;
}

MALI_STATIC void _egl_addref_sync( egl_sync *sync )
{
	MALI_DEBUG_ASSERT_POINTER( sync );
	_mali_sys_atomic_inc( &sync->references );
}

MALI_STATIC void _egl_deref_sync( egl_sync *sync )
{
	MALI_DEBUG_ASSERT_POINTER( sync );

	SYNC_DEBUG( _mali_sys_printf( "[thread:%p] %s: decref egl_sync:%p\n", _mali_sys_get_tid(), MALI_FUNCTION, sync ); );

	MALI_DEBUG_ASSERT( _mali_sys_atomic_get( &sync->references ) > 0, ("Reference counting issue with sync %p", sync ) );

	if ( _mali_sys_atomic_dec_and_return( &sync->references ) == 0 )
	{
		MALI_DEBUG_ASSERT( MALI_FALSE == sync->valid, ("Sync %p is still valid", sync ) );
#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
		if( EGL_SYNC_RESOURCE_PROFILING2_TYPE != sync->type) _mali_sys_lock_destroy( sync->lock );
#else
		_mali_sys_lock_destroy( sync->lock );
#endif
		SYNC_DEBUG( PRINT_SYNC( "deleting ", sync ); );
		_mali_sys_free( sync );
	}
}

MALI_STATIC void _egl_blocking_wait_sync( egl_sync *sync )
{
	MALI_DEBUG_ASSERT_POINTER( sync );

	while ( _mali_sys_atomic_get( &sync->references ) > 1 ) _mali_sys_yield();
}

MALI_STATIC egl_sync* _egl_create_sync_reusable( const EGLint *attrib_list, __egl_thread_state *tstate )
{
	egl_sync *sync = NULL;

	/* attribute list must be null or empty */
	if ( (NULL != attrib_list) && (EGL_NONE != attrib_list[0]) )
	{
		__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
		return NULL;
	}

	sync = _egl_create_sync();
	if ( NULL == sync )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}
	sync->type = EGL_SYNC_REUSABLE_KHR;

	return sync;
}

static void __egl_sync_fence_cb( mali_base_ctx_handle ctx, void* param )
{
	egl_sync *sync = (egl_sync *) param;
	MALI_IGNORE(ctx);

	SYNC_DEBUG( _mali_sys_printf("[thread:%p] %s: entering callback, sync = egl_sync:%p\n", _mali_sys_get_tid(), MALI_FUNCTION, sync ); );

	if ( !sync ) return;

	SYNC_DEBUG( PRINT_SYNC( "sync ", sync); );

	if ( sync->status == EGL_UNSIGNALED_KHR )
	{
		_mali_sys_lock_unlock( sync->lock );
		sync->status = EGL_SIGNALED_KHR;
	}

	if ( sync->destroy )
	{
		_egl_blocking_wait_sync( sync );
		sync->valid = EGL_FALSE;
		_egl_deref_sync( sync );
	}
	SYNC_DEBUG( _mali_sys_printf("[thread:%p] %s: exit callback\n", _mali_sys_get_tid(), MALI_FUNCTION); );
}

MALI_STATIC EGLBoolean _egl_insert_sync_node_to_fence_chain( __egl_thread_state *tstate, egl_sync *sync )
{
	egl_sync_node *new_sync_node = NULL;
	egl_sync_node *old_sync_node = tstate->current_sync;

	MALI_DEBUG_ASSERT_POINTER(old_sync_node);

	/* Create new sync_node with corresponding sync_handle */
	new_sync_node = (egl_sync_node*)_mali_sys_malloc( sizeof( *new_sync_node ) );
	if ( NULL == new_sync_node )
	{
		return EGL_FALSE;
	}

	new_sync_node->sync_handle = _mali_sync_handle_new(tstate->main_ctx->base_ctx);

	if( MALI_NO_HANDLE == new_sync_node->sync_handle )
	{
		_mali_sys_free( new_sync_node );
		return EGL_FALSE;
	}

	MALI_DEBUG_ASSERT( sync->fence_chain == NULL, ("No sync handle should be set up at this point") );

	/* Insert the current_sync into fence_chain of the sync_node.
	 * Replace the current_sync with the newly created one.
	 * Make the new sync_handle depend on the old one. */
	old_sync_node->current = MALI_FALSE;
	old_sync_node->sync = sync;
	old_sync_node->child = new_sync_node;

	new_sync_node->current = MALI_TRUE;
	new_sync_node->parent = old_sync_node;
	new_sync_node->child = NULL;
	new_sync_node->sync = NULL;

	sync->fence_chain = old_sync_node;

	_mali_sync_handle_add_to_sync_handle( new_sync_node->sync_handle, old_sync_node->sync_handle );

	SYNC_DEBUG( PRINT_NODE( "submitting implicit fence ", old_sync_node ) );
	_egl_fence_flush( tstate );

	tstate->current_sync = new_sync_node;
	SYNC_DEBUG( PRINT_NODE( "new implicit fence is now ", new_sync_node ) );

	return EGL_TRUE;
}

MALI_STATIC EGLBoolean  _egl_create_fence( __egl_thread_state *tstate, egl_sync *sync )
{
	EGLBoolean retval = EGL_TRUE;
	__egl_thread_state_api *tstate_api = NULL;
	EGLenum api;

	tstate_api = __egl_get_current_thread_state_api( tstate, &api );
	if ( ( NULL == tstate_api ) || ( NULL == tstate_api->draw_surface ) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_FALSE;
	}

	if ( NULL == tstate_api->draw_surface->frame_builder )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_FALSE;
	}

	if ( !_egl_insert_sync_node_to_fence_chain ( tstate, sync ) )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_FALSE;
	}

	return EGL_TRUE;
}


MALI_STATIC egl_sync* _egl_create_sync_fence( const EGLint *attrib_list, __egl_thread_state *tstate )
{
	egl_sync *sync = NULL;
	__egl_thread_state_api *tstate_api = NULL;
	EGLenum api;

	/* attribute list must be null or empty */
	if ( (NULL != attrib_list) && (EGL_NONE != attrib_list[0]) )
	{
		__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
		return NULL;
	}

	tstate_api = __egl_get_current_thread_state_api( tstate, &api );
	if ( ( NULL == tstate_api ) || ( NULL == tstate_api->draw_surface ) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return NULL;
	}

	sync = _egl_create_sync();
	if ( NULL == sync )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

	sync->fence_mutex = _mali_sys_mutex_create();
	if ( MALI_NO_HANDLE == sync->fence_mutex )
	{
		_egl_destroy_sync( sync );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

	sync->condition = EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR;
	sync->type = EGL_SYNC_FENCE_KHR;

	if( !_egl_create_fence( tstate, sync))
	{
		_egl_destroy_sync( sync );
		return NULL;
	}

	return sync;
}

void _egl_sync_flush_chain( egl_sync_node *n )
{
	mali_sync_handle local;

	MALI_DEBUG_ASSERT_POINTER( n );

	local = n->sync_handle;

	SYNC_DEBUG( PRINT_NODE( "flushing chaing starting from ", n ) );

	/* Flushes the sync_handle and it's parent recursively.
	 * Then removes it's occurence from the chain as we
	 * might have flushed in the middle of a bigger chain.
	 * Finally free the sync_node if it is not current. If it
	 * is current it will be freed by __egl_surface_release */
	if ( NULL != n->sync && MALI_NO_HANDLE != local )
	{
		n->sync_handle = MALI_NO_HANDLE;
		_mali_sync_handle_cb_function_set( local, __egl_sync_fence_cb, n->sync );
		_mali_sync_handle_flush(local);
	}

	if ( NULL != n->parent )
	{
		_egl_sync_flush_chain( n->parent );
		n->parent = NULL;
	}

	if ( n->child )
	{
		n->child->parent = NULL;
		n->child = NULL;
	}

	if ( MALI_FALSE == n->current )
	{
		if ( n->sync ) n->sync->fence_chain = NULL;
		_mali_sys_free( n );
	}
}
#if EGL_ANDROID_native_fencesync_ENABLE
EGLBoolean _egl_close_native_fence_object ( egl_sync * sync)
{
	EGLBoolean ret = EGL_TRUE;

	MALI_DEBUG_ASSERT_POINTER( sync );
	MALI_DEBUG_ASSERT_POINTER( sync->native_fence_obj );

	if (sync->native_fence_obj->fence_fd >= 0 )
	{
		mali_fence_release ( mali_fence_import_fd ( sync->native_fence_obj->fence_fd) );
	}
	_mali_sys_free( sync->native_fence_obj );

	sync->native_fence_obj = NULL;

	return ret;
}

MALI_STATIC EGLint _egl_client_wait_native_fence_obj ( egl_sync *sync, u64 timeout_ms )
{
	mali_err_code merr = MALI_ERR_NO_ERROR;
	EGLint retval = EGL_TIMEOUT_EXPIRED_KHR;
	MALI_DEBUG_ASSERT_POINTER( sync );
	MALI_DEBUG_ASSERT_POINTER( sync->native_fence_obj );

	if ( sync->native_fence_obj->fence_fd == EGL_NO_NATIVE_FENCE_FD_ANDROID )
	{
		/*no fence to wait for, mark the sync status as SIGNALED
		  and return without error*/
	    sync->status = EGL_SIGNALED_KHR;
	    _mali_sys_lock_unlock ( sync->lock );
		_egl_deref_sync ( sync );
		return EGL_CONDITION_SATISFIED_KHR;
	}
	/* query the current signal status*/
	sync->status = ( MALI_ERR_NO_ERROR ==  mali_fence_wait( mali_fence_import_fd (sync->native_fence_obj->fence_fd ), 0 ))?
		EGL_SIGNALED_KHR: EGL_UNSIGNALED_KHR;

	if ( sync->status == EGL_SIGNALED_KHR )
	{
		/*fence_fd will be invalidated inside mali_fence_wait if condition satisfied*/
		sync->native_fence_obj->fence_fd = EGL_NO_NATIVE_FENCE_FD_ANDROID;
		_mali_sys_lock_unlock( sync->lock );
		_egl_deref_sync ( sync );
		return EGL_CONDITION_SATISFIED_KHR;
	}
	/*wait till timeout or signaled*/
	merr = mali_fence_wait( mali_fence_import_fd (sync->native_fence_obj->fence_fd ), timeout_ms );
	if ( MALI_ERR_NO_ERROR == merr )
	{
		/*fence_fd will be invalidated inside mali_fence_wait if condition satisfied*/
		sync->native_fence_obj->fence_fd = EGL_NO_NATIVE_FENCE_FD_ANDROID;
		sync->status = EGL_SIGNALED_KHR;
		_mali_sys_lock_unlock ( sync->lock );
		retval = EGL_CONDITION_SATISFIED_KHR;
	}

	_egl_deref_sync ( sync );
	return retval;
}

MALI_STATIC egl_sync* _egl_create_android_native_fence_sync( const EGLint *attrib_list, __egl_thread_state *tstate )
{
	egl_sync *sync = NULL;
	__egl_thread_state_api *tstate_api = NULL;
	EGLint *attrib = ( EGLint *)attrib_list;
	EGLenum api;
	EGLint fence_fd = EGL_NO_NATIVE_FENCE_FD_ANDROID;
	mali_bool done = MALI_FALSE;
	mali_err_code err = MALI_ERR_NO_ERROR;
	mali_fence_handle fence_handle = MALI_FENCE_INVALID_HANDLE;

	if( NULL != attrib )
	{
		while ( done != MALI_TRUE )
		{
			switch ( attrib[0] )
			{
				case EGL_SYNC_NATIVE_FENCE_FD_ANDROID:
					fence_fd = attrib[1];
					break;
				case EGL_NONE:
					done = MALI_TRUE;
				   	break;
				default:
					__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
					return NULL;
			}
			attrib += 2;
		}
	}

	tstate_api = __egl_get_current_thread_state_api( tstate, &api );

	if ( ( NULL == tstate_api ) || ( NULL == tstate_api->draw_surface ) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return NULL;
	}
	sync = _egl_create_sync();

	if( NULL == sync )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

	/*prepare native fence object structure*/
	egl_native_fence_obj *native_fence_obj = ( egl_native_fence_obj*)_mali_sys_malloc(sizeof( egl_native_fence_obj ));
	if ( native_fence_obj == NULL )
	{
		_egl_destroy_sync( sync );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}
	native_fence_obj->fence_fd = EGL_NO_NATIVE_FENCE_FD_ANDROID;
	sync->native_fence_obj = native_fence_obj;

	/* sync object with attribuite EGL_SYNC_NATIVE_FENCE_FD_ANDROID set to non-EGL_NO_NATIVE_FENCE_FD_ANDROID*/
	if( fence_fd != EGL_NO_NATIVE_FENCE_FD_ANDROID )
	{
	    sync->condition = EGL_SYNC_NATIVE_FENCE_SIGNALED_ANDROID;
		sync->status = EGL_UNSIGNALED_KHR;
		sync->native_fence_obj->fence_fd = fence_fd;
		sync->type = EGL_SYNC_NATIVE_FENCE_ANDROID;
		sync->fence_chain = NULL;

		return sync;
	}

	/*sync object with attirbuite EGL_SYNC_NATIVE_FENCE_FD_ANDROID set to EGL_NO_NATIVE_FENCE_FD_ANDROID*/
	sync->condition = EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR;
	sync->type = EGL_SYNC_NATIVE_FENCE_ANDROID;

	if( EGL_SUCCESS != _egl_fence_flush ( tstate))
	{
		_egl_destroy_sync( sync );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

#if MALI_EXTERNAL_SYNC == 1
	fence_handle = _mali_fence_sync_get_fence_handle ( tstate->current_mali_fence_sync );
	MALI_DEBUG_PRINT(3, ("%s to get valid accumulated fid from mali_fence_sync!\n",
				(fence_handle !=MALI_FENCE_INVALID_HANDLE)? "succeeded" : "Failed" ));
	if ( fence_handle != MALI_FENCE_INVALID_HANDLE)
	{
		sync->native_fence_obj->fence_fd = mali_fence_fd( fence_handle );
	}
#endif
	return sync;
}
#endif
EGLSyncKHR _egl_create_sync_KHR( EGLDisplay dpy, EGLenum type, const EGLint *attrib_list, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state*)thread_state;
	EGLSyncKHR sync_handle = EGL_NO_SYNC_KHR;
	egl_sync *sync = NULL;

	MALI_CHECK( EGL_NO_DISPLAY != __egl_get_check_display( dpy, tstate ), EGL_NO_SYNC_KHR );

	switch ( type )
	{
#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
		case EGL_SYNC_RESOURCE_PROFILING2_TYPE:
			sync = _egl_create_sync_resource_profiling( attrib_list, tstate );
			break;
#endif
		case EGL_SYNC_REUSABLE_KHR:
			sync = _egl_create_sync_reusable( attrib_list, tstate );
			break;
		case EGL_SYNC_FENCE_KHR:
			if ( dpy != _egl_get_current_display( tstate ) )
			{
				__egl_set_error( EGL_BAD_MATCH, tstate );
				return EGL_NO_SYNC_KHR;
			}
			sync = _egl_create_sync_fence( attrib_list, tstate );
			break;
#if EGL_ANDROID_native_fencesync_ENABLE
		case EGL_SYNC_NATIVE_FENCE_ANDROID:
			if ( dpy != _egl_get_current_display( tstate ) )
			{
				__egl_set_error ( EGL_BAD_MATCH, tstate );
				return EGL_NO_SYNC_KHR;
			}
			sync = _egl_create_android_native_fence_sync ( attrib_list, tstate );
			break;
#endif
		default:
			__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
			break;
	}

	MALI_CHECK_NON_NULL( sync, EGL_NO_SYNC_KHR );

	sync->dpy = dpy;
	sync_handle = __egl_add_sync_handle( sync, dpy );
	if ( EGL_NO_SYNC_KHR == sync_handle )
	{
		_mali_sys_lock_unlock( sync->lock );
		_egl_deref_sync( sync );
		__egl_set_error( EGL_BAD_ALLOC, tstate );

		return EGL_NO_SYNC_KHR;
	}

	SYNC_DEBUG( _mali_sys_printf( "[thread:%p] %s: egl_sync:%p created\n", _mali_sys_get_tid(), MALI_FUNCTION, sync ); );

	return sync_handle;
}

void _egl_sync_destroy_sync_node( egl_sync_node *n )
{
	MALI_DEBUG_ASSERT_POINTER( n );

	/* If fence_chain is in a chain we need to take it out */
	if ( n->child )	n->child->parent = n->parent;
	if ( n->parent ) n->parent->child = n->child;
	if ( n->sync ) n->sync->fence_chain = NULL;

	/* ... and flush the sync_handle if it exists */
	if ( MALI_NO_HANDLE != n->sync_handle )
	{
		mali_sync_handle local = n->sync_handle;
		_mali_sync_handle_cb_function_set( local, __egl_sync_fence_cb, n->sync );
		_mali_sync_handle_flush(local);
	}

	_mali_sys_free( n );
}

void _egl_destroy_sync( egl_sync *sync )
{
	MALI_DEBUG_ASSERT_POINTER( sync );

	if ( MALI_NO_HANDLE != sync->fence_mutex )
	{
		_mali_sys_mutex_destroy( sync->fence_mutex );
		sync->fence_mutex = MALI_NO_HANDLE;
	}
	switch ( sync->type )
	{
		case EGL_SYNC_FENCE_KHR:
			{
				if ( EGL_UNSIGNALED_KHR == sync->status )
				{
					/* Destroy this sync in the call back from the frame builder
					 * and destroy the sync_node only if not current */
					sync->destroy = EGL_TRUE;

					if ( sync->fence_chain && MALI_TRUE != sync->fence_chain->current )
					{
						_egl_sync_destroy_sync_node( sync->fence_chain );
					}

					return;
				}
			}
			break;
		case EGL_SYNC_REUSABLE_KHR:
			{
				if ( EGL_UNSIGNALED_KHR == sync->status ) _mali_sys_lock_unlock( sync->lock );
				sync->status = EGL_SIGNALED_KHR;

			}
			break;
#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
		case  EGL_SYNC_RESOURCE_PROFILING2_TYPE :
			{
				_egl_destroy_sync_resource_profiling( sync );
			}
			break;
#endif

#if EGL_ANDROID_native_fencesync_ENABLE
		case EGL_SYNC_NATIVE_FENCE_ANDROID:
			{
				_egl_blocking_wait_sync( sync );

				if( sync->status == EGL_UNSIGNALED_KHR )
				{
					_mali_sys_lock_unlock ( sync->lock );
				}
				_egl_close_native_fence_object( sync );

				sync->valid = MALI_FALSE;

				_egl_deref_sync ( sync );

				return;
			}
			break;
#endif
		default:
			break;
	}

	/* wait for blocking waits to get signaled before destroying */
	_egl_blocking_wait_sync( sync );
	sync->valid = MALI_FALSE;

	_egl_deref_sync( sync );
}

EGLBoolean _egl_destroy_sync_KHR( EGLDisplay dpy, EGLSyncKHR sync_handle, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state*)thread_state;
	egl_sync *sync = NULL;


	MALI_CHECK( EGL_NO_DISPLAY != __egl_get_check_display( dpy, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_NO_SYNC_KHR != (sync = __egl_get_check_sync( sync_handle, dpy, tstate ) ), EGL_FALSE );

	SYNC_DEBUG( _mali_sys_printf("[thread:%p] %s: enter, egl_sync:%p\n", _mali_sys_get_tid(), MALI_FUNCTION, sync) );

	if ( EGL_NO_DISPLAY != sync->dpy )
	{
		EGLBoolean retval = __egl_remove_sync_handle( sync, sync->dpy );
		MALI_IGNORE( retval );
	}

	_egl_destroy_sync( sync );

	SYNC_DEBUG( _mali_sys_printf("[thread:%p] %s: exit\n", _mali_sys_get_tid(), MALI_FUNCTION) );

	return EGL_TRUE;
}

EGLint _egl_client_wait_sync_KHR( EGLDisplay dpy, EGLSyncKHR sync_handle, EGLint flags, EGLTimeKHR timeout, void *thread_state )
{
	EGLint retval = EGL_TIMEOUT_EXPIRED_KHR;
	mali_err_code merr = MALI_ERR_NO_ERROR;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	egl_sync *sync = NULL;
	u64 timeout_ms = 0;

	/* Verify display handle, even if we don't use it */
	if ( EGL_NO_DISPLAY == __egl_get_check_display( dpy, tstate ) )
	{
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
		return EGL_FALSE;
	}

	sync = __egl_get_check_sync( sync_handle, dpy, tstate );
	if ( EGL_NO_SYNC_KHR == sync )
	{
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
		return EGL_FALSE;
	}

	SYNC_DEBUG( PRINT_SYNC( "waiting for ", sync ) );

	if ( EGL_FOREVER_KHR != timeout )
	{
		/* supplied time is in nanoseconds  - convert to microseconds */
		timeout_ms = (timeout < 1000) ? 1 : (timeout/1000);
	}
#if EGL_ANDROID_native_fencesync_ENABLE
	/* sync condition satisfied by signal status of native fence object*/
	if ( sync->type  == EGL_SYNC_NATIVE_FENCE_ANDROID )
	{
		if ( timeout == EGL_FOREVER_KHR )
			timeout_ms = -1;

		/* addref the sync object before we unlock the main mutex */
	    _egl_addref_sync( sync );

	    /* release the main lock, since we potentially can wait forever in here */
	    __egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );

		return _egl_client_wait_native_fence_obj( sync , timeout_ms );
	}
#endif

#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
	if( EGL_SYNC_RESOURCE_PROFILING2_TYPE != sync->type )
#endif
	{
		if ( 0 == timeout )
		{
			/* Just return the status of the sync */
			if ( EGL_SIGNALED_KHR == sync->status )	retval = EGL_CONDITION_SATISFIED_KHR;
			__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
			return retval;
		}
	}

	/* early out if the sync has been signaled already */
	if ( EGL_SIGNALED_KHR == sync->status )
	{
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
		return EGL_CONDITION_SATISFIED_KHR;
	}

	/**
	 * "Accepted in the <flags> parameter of eglClientWaitSyncKHR:
	 *  EGL_SYNC_FLUSH_COMMANDS_BIT_KHR 0x0001"
	 */
	if ( flags & ~EGL_SYNC_FLUSH_COMMANDS_BIT_KHR )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
		return EGL_FALSE;
	}

	/* addref the sync object before we unlock the main mutex */
	_egl_addref_sync( sync );

	/* release the main lock, since we potentially can wait forever in here */
	__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );

#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
	if ( EGL_SYNC_RESOURCE_PROFILING2_TYPE == sync->type )
	{
		/**
		 * If the value of <timeout> is zero, then eglClientWaitSyncKHR simply
		 * tests the current status of <sync>.
		 */
		if ( 0 == timeout )
		{
			if ( EGL_SIGNALED_KHR == _egl_sync_status_resource_profiling( sync ) )
			{
				retval = EGL_CONDITION_SATISFIED_KHR;
			}
		}
		else
		{
			retval = _egl_client_wait_sync_resource_profiling( sync, timeout_ms );
			if ( EGL_FALSE == retval )
			{
				__egl_set_error( EGL_BAD_PARAMETER, tstate );
			}
		}

		_egl_deref_sync( sync );

		return retval;
	}
#endif

	/* Fence sync handles must be handled separately,
	 * by flushing the fence sync and setting up the fence callback
	 * on a fence, this is what will actually lower the sync semaphore.
	 * Notably, We need to flush all parents as we might have a chain of
	 * fences.
	 * NB! We might have already flushed the this sync's fence_chain by
	 * a later fence, in that case sync->fence_chain is NULL. No wait needed. */
	if ( sync->type == EGL_SYNC_FENCE_KHR )
	{
		/* this lock serializes simultaneous waits on the same sync */
		_mali_sys_mutex_lock( sync->fence_mutex );

		if ( sync->fence_chain )
		{
			SYNC_DEBUG( PRINT_NODE( "sync node is ", sync->fence_chain ) );

			/* serialize acess to fence chain (shared amongst threads) */
			__egl_main_mutex_action( EGL_MAIN_MUTEX_SYNC_LOCK );

			_egl_sync_flush_chain( sync->fence_chain );

			__egl_main_mutex_action( EGL_MAIN_MUTEX_SYNC_UNLOCK );
		}

		_mali_sys_mutex_unlock( sync->fence_mutex );
	}

	if ( EGL_FOREVER_KHR == timeout )
	{
		_mali_sys_lock_lock( sync->lock );
	}
	else
	{
		merr = _mali_sys_lock_timed_lock( sync->lock, timeout_ms );
	}

	if ( MALI_ERR_NO_ERROR == merr )
	{
		retval = EGL_CONDITION_SATISFIED_KHR;
		_mali_sys_lock_unlock( sync->lock );
	}
	else
	{
		retval = EGL_TIMEOUT_EXPIRED_KHR;
	}

	_egl_deref_sync( sync );

	return retval;
}

EGLBoolean _egl_signal_sync_KHR( EGLDisplay dpy, EGLSyncKHR sync_handle, EGLenum mode, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	egl_sync *sync = NULL;

	MALI_CHECK( EGL_NO_DISPLAY != __egl_get_check_display( dpy, tstate ) , EGL_FALSE );
	MALI_CHECK( EGL_NO_SYNC_KHR != (sync = __egl_get_check_sync( sync_handle, dpy, tstate ) ), EGL_FALSE );

	if ( EGL_SYNC_REUSABLE_KHR != sync->type )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_FALSE;
	}

	if ( sync->status == mode ) return EGL_TRUE; /* setting same status */

	if ( (EGL_SIGNALED_KHR != mode) && (EGL_UNSIGNALED_KHR != mode) )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return EGL_FALSE;
	}

	if ( mode == EGL_UNSIGNALED_KHR )
	{
		/* make sure all waits have returned before setting unsignaled mode again
		 * should happen very rarely - only if signals are issued straight after each other */
		_egl_blocking_wait_sync( sync );
		_mali_sys_lock_lock( sync->lock );
	}
	else _mali_sys_lock_unlock( sync->lock );

	sync->status = mode;

	return EGL_TRUE;
}

EGLBoolean _egl_get_sync_attrib_KHR( EGLDisplay dpy, EGLSyncKHR sync_handle, EGLint attribute, EGLint *value, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	egl_sync *sync = NULL;

	MALI_CHECK( EGL_NO_DISPLAY != __egl_get_check_display( dpy, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_NO_SYNC_KHR != (sync = __egl_get_check_sync( sync_handle, dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_null_value( value, EGL_BAD_PARAMETER, tstate ), EGL_FALSE );

	switch ( attribute )
	{
		case EGL_SYNC_TYPE_KHR:   *value = (EGLint)sync->type;    break;
#if EGL_FEATURE_RESOURCE_PROFILING_ENABLE
		case EGL_SYNC_STATUS_KHR:
			*value = EGL_SYNC_RESOURCE_PROFILING2_TYPE == sync->type ? _egl_sync_status_resource_profiling( sync ) : (EGLint)sync->status;
			break;
		case EGL_SYNC_CONDITION_KHR:
			if ( EGL_SYNC_FENCE_KHR == sync->type || EGL_SYNC_RESOURCE_PROFILING2_TYPE == sync->type
#if EGL_ANDROID_native_fencesync_ENABLE
     			|| EGL_SYNC_NATIVE_FENCE_ANDROID == sync->type
#endif
			   )
			{
				*value = (EGLint)sync->condition;
				break;
			} /* fall through */
#else
		case EGL_SYNC_STATUS_KHR:
			*value = (EGLint)sync->status;
			break;
		case EGL_SYNC_CONDITION_KHR:
			if ( EGL_SYNC_FENCE_KHR == sync->type
#if EGL_ANDROID_native_fencesync_ENABLE
				|| EGL_SYNC_NATIVE_FENCE_ANDROID == sync->type
#endif
		       )
			{
				*value = (EGLint)sync->condition;
				break;
			} /* fall through */
#endif
		default:
			__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
			return EGL_FALSE;
	}

	return EGL_TRUE;
}
#if EGL_ANDROID_native_fencesync_ENABLE
EGLint _egl_dup_native_fence_ANDROID( EGLDisplay dpy, EGLSyncKHR sync_handle, void *thread_state )
{
	/* FIXME: Implement this function */
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	egl_sync *sync = NULL;
	int fd = EGL_NO_NATIVE_FENCE_FD_ANDROID;

	MALI_CHECK( EGL_NO_DISPLAY != __egl_get_check_display( dpy, tstate ) , EGL_FALSE );
	MALI_CHECK( EGL_NO_SYNC_KHR != (sync = __egl_get_check_sync( sync_handle, dpy, tstate ) ), EGL_FALSE );


	if( sync->type != EGL_SYNC_NATIVE_FENCE_ANDROID )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return EGL_NO_NATIVE_FENCE_FD_ANDROID;
	}

	if( sync->native_fence_obj->fence_fd == EGL_NO_NATIVE_FENCE_FD_ANDROID )
	{
		/* nothing to be wait on*/
		 __egl_set_error( EGL_BAD_PARAMETER, tstate );
		return EGL_NO_NATIVE_FENCE_FD_ANDROID;
	}

	MALI_DEBUG_ASSERT( sync->native_fence_obj->fence_fd != EGL_NO_NATIVE_FENCE_FD_ANDROID , ("There must be 'fence_fd' other than EGL_NO_NATIVE_FENCE_FD_ANDROID provided at this point!"));

	fd = mali_fence_dup( mali_fence_import_fd( sync->native_fence_obj->fence_fd) );

	return (EGLint ) fd;
}
#endif

#if MALI_EXTERNAL_SYNC == 1
MALI_EXPORT void _egl_sync_notify_job_start( mali_bool success, mali_fence_handle fence, void *data)
{
	struct mali_fence_sync *fence_sync = ( struct mali_fence_sync *)data;
	MALI_DEBUG_ASSERT_POINTER( fence_sync );
	_mali_fence_sync_notify_job_start( fence_sync, success, fence );
}
MALI_EXPORT void _egl_sync_notify_job_create(void *data)
{
	struct mali_fence_sync *fence_sync;
	__egl_thread_state *tstate = NULL;
	mali_frame_builder *frame_builder = (mali_frame_builder*) data;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_NOP );

	MALI_DEBUG_ASSERT_POINTER( tstate );

	fence_sync = tstate->current_mali_fence_sync;

	MALI_DEBUG_ASSERT_POINTER( fence_sync );

	_mali_fence_sync_notify_job_create( fence_sync );

	_mali_frame_builder_set_fence_sync_data( frame_builder, (void *)fence_sync );

	__egl_release_current_thread_state( EGL_MAIN_MUTEX_NOP );
}
#endif

MALI_EXPORT mali_sync_handle _egl_sync_get_current_sync_handle( void )
{
	egl_sync_node *sync_node;
	__egl_thread_state *tstate = NULL;

	/* It is safe to avoid grabbing mutexes as long as this
	 * function is called internally. */
	tstate  = __egl_get_current_thread_state( EGL_MAIN_MUTEX_NOP );

	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( tstate->current_sync );

	sync_node = tstate->current_sync;

	__egl_release_current_thread_state( EGL_MAIN_MUTEX_NOP );

	return sync_node->sync_handle;
}

#endif /* EGL_KHR_reusable_sync_ENABLE */

