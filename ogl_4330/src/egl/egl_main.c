/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <EGL/mali_egl.h>
#include <mali_system.h>
#include "egl_main.h"
#include "egl_platform.h"
#include "egl_thread.h"
#include "egl_handle.h"
#include "egl_config_extension.h"
#include "base/common/base_common_context.h"

#ifdef EGL_USE_VSYNC
#include "egl_vsync.h"
#endif /* EGL_USE_VSYNC */

#ifdef EGL_MALI_VG
#include "egl_vg.h"
#endif
#ifdef EGL_MALI_GLES
#include "egl_gles.h"
#endif

#if MALI_OPROFILE_SAMPLING
#include <instrumented/mali_oprofile.h>
#endif

#include "mali_instrumented_main.h"
#include "mali_instrumented_plugin.h"
#include "instrumented/apitrace/mali_trace.h"
#include "base/mali_profiling.h"

static __egl_main_context *__egl_main = NULL; /** static instance of the egl main context */
static volatile mali_mutex_handle __egl_main_mutex = MALI_NO_HANDLE;
egl_api_shared_function_ptrs egl_funcptrs;

/* internal library detach cleanup function */
#if !defined(MALI_MONOLITHIC)
MALI_STATIC
#endif
void mali_egl_cleanup_internal( void )
{
	__egl_main_context *egl;

	MALI_CHECK_GOTO(EGL_TRUE == __egl_main_initialized(), edexit);

	egl = __egl_get_main_context();
	MALI_CHECK_GOTO( NULL != egl, edexit );

#ifndef MALI_DEBUG_SKIP_CODE
	/* Warn if the application didn't terminate properly */
	if ( egl->displays_open > 0
		 || NULL != egl->base_ctx
		 || (egl->state &  (EGL_STATE_MEMOPEN |
							EGL_STATE_PPOPEN |
							EGL_STATE_GPOPEN |
							EGL_STATE_POPEN))
		 )
	{
		MALI_DEBUG_PRINT(2, ("Application did not terminate EGL correctly"));

#if defined(EGL_CLEANUP_IS_RESTRICTED) && defined(_WIN32)
		/* Also warn if we can do nothing about it */
		DEBUGMSG(1,(_T("Application did not terminate EGL correctly - cannot cleanup EGL correctly")));
#endif
	}
#endif /* MALI_DEBUG_SKIP_CODE */


#ifdef EGL_CLEANUP_IS_RESTRICTED
	/* Just free the display list - warn if you'll get leaks */
	{
		egl_display *display = __egl_get_first_display_handle();
		while ( NULL != display )
		{

#ifndef MALI_DEBUG_SKIP_CODE
			if ( NULL != display->config
				 || NULL != display->context
				 || NULL != display->surface )
			{
				MALI_DEBUG_PRINT(2,("Warning: There were unreleased resources for EGLDisplay %p during termination",__egl_get_display_handle( display ) ));
			}
#endif /* EGL_MALI_DEBUG_SKIP_CODE */
			__egl_release_display( display, EGL_TRUE ); /* really remove the display */
			display = __egl_get_first_display_handle();
		}
	}
#else  /* EGL_CLEANUP_IS_RESTRICTED */
	/* Free all displays and close Mali, if we have no restrictions on
	 * doing so */
	__egl_free_all_displays();
	__egl_main_close_mali();
#endif /* EGL_CLEANUP_IS_RESTRICTED */

	/* Thread state freed when destroying main context */

	/* destroy the main context */
	__egl_destroy_main_context();

edexit:
	if ( MALI_NO_HANDLE != __egl_main_mutex )
	{
		_mali_sys_mutex_destroy( __egl_main_mutex );
		__egl_main_mutex = MALI_NO_HANDLE;
	}
#ifdef MALI_TEST_API
	_mali_test_auto_term();
#endif
	return;
}

void __egl_main_power_event( u32 event )
{
	switch ( event )
	{
		case EGL_POWER_EVENT_SUSPEND: __egl_invalidate_context_handles(); break;
		case EGL_POWER_EVENT_RESUME:  __egl_invalidate_context_handles(); break;
		default: break;
	}
}

EGLBoolean __egl_main_initialized( void )
{
	MALI_CHECK_NON_NULL( __egl_main, EGL_FALSE );
	MALI_CHECK( EGL_STATE_INITIALIZED & __egl_main->state, EGL_FALSE );

	return EGL_TRUE;
}

MALI_STATIC void __egl_main_context_set_defaults( void )
{
	if ( _mali_sys_config_string_get_bool("MALI_NEVERBLIT", MALI_FALSE) )
	{
		__egl_main->never_blit = EGL_TRUE;
	}

	if ( _mali_sys_config_string_get_bool("MALI_FLIP_PIXMAP", MALI_FALSE ) )
	{
		__egl_main->flip_pixmap = EGL_TRUE;
	}
}

__egl_main_context* __egl_get_main_context( void )
{
	mali_err_code err;

	err = _mali_sys_mutex_auto_init( &__egl_main_mutex );
	if ( MALI_ERR_NO_ERROR != err )
	{
		MALI_DEBUG_PRINT( 0, ("Failed to autoinit egl_main_mutex\n") );
		return NULL;
	}
		
	_mali_sys_mutex_lock( __egl_main_mutex );
	if ( NULL != __egl_main )
	{
		_mali_sys_mutex_unlock( __egl_main_mutex );
		return __egl_main;
	}

	__egl_main = (__egl_main_context *) _mali_sys_calloc( 1, sizeof( __egl_main_context ) );
	MALI_CHECK_GOTO( NULL != __egl_main, egl_get_main_context_failed );

	/* init default values */
	__egl_main_context_set_defaults();

	__egl_main->main_lock = _mali_sys_lock_create();
	MALI_CHECK_GOTO( MALI_NO_HANDLE != __egl_main->main_lock, egl_get_main_context_failed );

#if EGL_KHR_reusable_sync_ENABLE
	__egl_main->sync_lock = _mali_sys_lock_create();
	MALI_CHECK_GOTO( MALI_NO_HANDLE != __egl_main->sync_lock, egl_get_main_context_failed );
#endif

	__egl_main->image_lock = _mali_sys_lock_create();
	MALI_CHECK_GOTO( MALI_NO_HANDLE != __egl_main->image_lock, egl_get_main_context_failed );

	__egl_main->mutex_vsync = _mali_sys_mutex_create();
	MALI_CHECK_GOTO( MALI_NO_HANDLE != __egl_main->mutex_vsync, egl_get_main_context_failed );

	__egl_main->display = __mali_named_list_allocate();
	MALI_CHECK_GOTO( NULL != __egl_main->display, egl_get_main_context_failed );

	__egl_main->thread = __mali_named_list_allocate();
	MALI_CHECK_GOTO( NULL != __egl_main->thread, egl_get_main_context_failed );

#if EGL_KHR_image_ENABLE
	__egl_main->egl_images = __mali_named_list_allocate();
	MALI_CHECK_GOTO( NULL != __egl_main->egl_images, egl_get_main_context_failed );
#endif

#ifdef EGL_SIMULATE_VSYNC
	__egl_vsync_simulation_start( 50 ); /* start a fake vsync thread calling the vsync callback every 1/50 seconds */
#endif
	/* set up the linker */
	__egl_main->linker = (egl_linker *)_mali_sys_calloc( 1, sizeof( egl_linker ) );
	MALI_CHECK_GOTO( NULL != __egl_main->linker, egl_get_main_context_failed );
	if ( EGL_FALSE == egl_linker_init( __egl_main->linker ) )
	{
		MALI_DEBUG_PRINT(0, ("* EGL linker failed to initialize\n"));
		goto egl_get_main_context_failed;
	}

#ifdef EGL_MALI_GLES
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == __egl_gles_initialize( __egl_main ), egl_get_main_context_failed );
#endif /* EGL_MALI_GLES */
#ifdef EGL_MALI_VG
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == __egl_vg_initialize( __egl_main ), egl_get_main_context_failed );
#endif /* EGL_MALI_VG */

#ifdef EGL_WORKER_THREAD_ENABLED
	__egl_main->worker = _mali_base_worker_create(MALI_FALSE);
	MALI_CHECK_GOTO( MALI_BASE_WORKER_NO_HANDLE != __egl_main->worker, egl_get_main_context_failed );
#endif

	/* This is about whether the __egl_main was initialized, not all the
	 * Mali resources */
	__egl_main->state |= EGL_STATE_INITIALIZED;

#if EGL_KHR_image_ENABLE
	egl_funcptrs.get_eglimage_ptr = __egl_get_image_ptr_implicit;
#else
	egl_funcptrs.get_eglimage_ptr = NULL;
#endif

#if EGL_KHR_reusable_sync_ENABLE
	egl_funcptrs.get_synchandle = _egl_sync_get_current_sync_handle;
#else
	egl_funcptrs.get_synchandle = NULL;
#endif

#if MALI_EXTERNAL_SYNC == 1
	egl_funcptrs._sync_notify_job_create = _egl_sync_notify_job_create;
	egl_funcptrs._sync_notify_job_start = _egl_sync_notify_job_start;
#endif /* MALI_EXTERNAL_SYNC == 1 */

	_mali_sys_mutex_unlock( __egl_main_mutex );
	return __egl_main;

egl_get_main_context_failed:
	__egl_destroy_main_context();
	_mali_sys_mutex_unlock( __egl_main_mutex );
	_mali_sys_mutex_destroy( __egl_main_mutex );
	__egl_main_mutex = MALI_NO_HANDLE;
	return NULL;
}

void __egl_main_mutex_lock( void )
{
	__egl_main_context *egl = NULL;

	egl = __egl_get_main_context();
	if ( NULL != egl ) 
	{
		_mali_sys_lock_lock( egl->main_lock );
	}
}

void __egl_main_mutex_unlock( void )
{
	__egl_main_context *egl = NULL;

	egl = __egl_get_main_context();
	if ( NULL != egl )
	{
		_mali_sys_lock_unlock( egl->main_lock );
	}
}

#if EGL_KHR_reusable_sync_ENABLE
void __egl_sync_mutex_lock( void )
{
	__egl_main_context *egl = NULL;

	egl = __egl_get_main_context();
	if ( NULL != egl ) 
	{
		_mali_sys_lock_lock( egl->sync_lock );
	}
}

void __egl_sync_mutex_unlock( void )
{
	__egl_main_context *egl = NULL;

	egl = __egl_get_main_context();
	if ( NULL != egl )
	{
		_mali_sys_lock_unlock( egl->sync_lock );
	}
}
#endif

void __egl_image_mutex_lock( void )
{
	__egl_main_context *egl = NULL;

	egl = __egl_get_main_context();
	if ( NULL != egl )
	{
		_mali_sys_lock_lock( egl->image_lock );
	}
}

void __egl_image_mutex_unlock( void )
{
	__egl_main_context *egl = NULL;

	egl = __egl_get_main_context();
	if ( NULL != egl )
	{
		_mali_sys_lock_unlock( egl->image_lock );
	}
}

void __egl_all_mutexes_lock( void )
{
	__egl_main_context *egl = NULL;

	egl = __egl_get_main_context();
	if ( NULL != egl ) 
	{
		_mali_sys_lock_lock( egl->main_lock );
#if EGL_KHR_reusable_sync_ENABLE
		_mali_sys_lock_lock( egl->sync_lock );
#endif
	}
}

void __egl_all_mutexes_unlock( void )
{
	__egl_main_context *egl = NULL;

	egl = __egl_get_main_context();
	/* Unlock needs to be done in opposite order to avoid lock order inconsistencies */ 
	if ( NULL != egl )
	{
		_mali_sys_lock_unlock( egl->main_lock );
#if EGL_KHR_reusable_sync_ENABLE
		_mali_sys_lock_unlock( egl->sync_lock );
#endif
	}
}

void __egl_main_mutex_action( __main_mutex_action mutex_action )
{
	switch( mutex_action )
	{
		case EGL_MAIN_MUTEX_LOCK:         __egl_main_mutex_lock(); break;
		case EGL_MAIN_MUTEX_UNLOCK:       __egl_main_mutex_unlock(); break;
		case EGL_MAIN_MUTEX_ALL_LOCK:     __egl_all_mutexes_lock(); break;
		case EGL_MAIN_MUTEX_ALL_UNLOCK:   __egl_all_mutexes_unlock(); break;
#if EGL_KHR_reusable_sync_ENABLE
		case EGL_MAIN_MUTEX_SYNC_LOCK:    __egl_sync_mutex_lock(); break;
		case EGL_MAIN_MUTEX_SYNC_UNLOCK:  __egl_sync_mutex_unlock(); break;
#endif
		case EGL_MAIN_MUTEX_IMAGE_LOCK:   __egl_image_mutex_lock(); break;
		case EGL_MAIN_MUTEX_IMAGE_UNLOCK: __egl_image_mutex_unlock(); break;
		case EGL_MAIN_MUTEX_NOP:
		default:
			break;
	}
}

#ifdef NEXELL_FEATURE_ENABLE
extern unsigned int gOpenDrvCnt;
#if defined(HAVE_ANDROID_OS)
#include "egl_image_android.h"
#include <android/log.h>
#endif
#endif
EGLBoolean __egl_main_open_mali( void )
{
	__egl_main_context *egl;

#if MALI_OPROFILE_SAMPLING
	mali_oprofile_init();
#endif

	egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( egl );

	/* initialize the base driver */
#if defined(EGL_MALI_GLES) || defined(EGL_MALI_VG)
	/* ASSERT that we've not already done this, and thus preserve atomicity */
	MALI_DEBUG_ASSERT( NULL == egl->base_ctx, ("Base context already created") );
	MALI_DEBUG_ASSERT( !(egl->state & (EGL_STATE_MEMOPEN |
									   EGL_STATE_PPOPEN |
									   EGL_STATE_GPOPEN |
									   EGL_STATE_POPEN)),
					   ("Some mali resources already open") );

	egl->base_ctx = _mali_base_context_create();
	if ( NULL == egl->base_ctx )
	{
#ifdef NEXELL_FEATURE_ENABLE
		if(gOpenDrvCnt == 500)
		{
#if defined(HAVE_ANDROID_OS)
			ALOGE("[EGL] open driver fail(try_cnt timeout:%d)\n", gOpenDrvCnt);
#else 
			printf("[EGL] open driver fail(try_cnt timeout:%d)\n", gOpenDrvCnt);
#endif			
		}
#endif
		__egl_main_close_mali();
		return EGL_FALSE;
	}
#ifdef NEXELL_FEATURE_ENABLE
#if defined(HAVE_ANDROID_OS)
	ALOGI("[EGL] open driver sucess(try_cnt:%d)\n", gOpenDrvCnt);
#else 
	printf("[EGL] open driver sucess(try_cnt:%d)\n", gOpenDrvCnt);
#endif				
#endif

	if ( MALI_ERR_NO_ERROR != _mali_mem_open( egl->base_ctx ) )
	{
		__egl_main_close_mali();
		return EGL_FALSE;
	}
	egl->state |= EGL_STATE_MEMOPEN ;

	if ( MALI_ERR_NO_ERROR != _mali_pp_open( egl->base_ctx ) )
	{
		__egl_main_close_mali();
		return EGL_FALSE;
	}
	egl->state |= EGL_STATE_PPOPEN;

	if ( MALI_ERR_NO_ERROR != _mali_gp_open( egl->base_ctx ) )
	{
		__egl_main_close_mali();
		return EGL_FALSE;
	}
	egl->state |= EGL_STATE_GPOPEN;

#endif /* defined(EGL_MALI_GLES) || defined(EGL_MALI_VG) */

#if MALI_INSTRUMENTED
   {
		if (_mali_instrumented_init() != MALI_TRUE)
		{
			__egl_main_close_mali();
			return EGL_FALSE;
		}

		egl->state |= EGL_STATE_IOPEN;
	}
#endif

#if MALI_INSTRUMENTED_PLUGIN_ENABLED
	_mali_instrumented_plugin_load();
#endif

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_START|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_START_STOP_SW_VR, 0, 0, 0, 0, 0);
#endif

	if ( EGL_TRUE != __egl_platform_initialize() )
	{
		__egl_main_close_mali();
		return EGL_FALSE;
	}
	egl->state |= EGL_STATE_POPEN;

	/* Success of opening all Mali resources */
	return EGL_TRUE;
}

void __egl_main_close_mali( void )
{
	__egl_main_context *egl;

	egl = __egl_main;

	/* __egl_main_close_mali should not be called when there is no __egl_main context: */
	MALI_DEBUG_ASSERT_POINTER( egl );

#ifdef EGL_MALI_VG
	if ( NULL != __egl_main->vg_global_context )
	{
		if ( NULL != __egl_main->linker )
		{
			__egl_main->linker->vg_func.destroy_global_context( __egl_main->vg_global_context );
			__egl_main->vg_global_context = NULL;
		}
	}
#endif

	/* Only close resources that were opened */
	if (egl->state & EGL_STATE_POPEN)
	{
		__egl_platform_terminate();
		egl->state &= ~EGL_STATE_POPEN;
	}

	/* Stop cleanup thread. */
	if (NULL != egl->base_ctx)
	{
		_mali_base_common_context_cleanup_thread_end(egl->base_ctx);
	}

#if MALI_INSTRUMENTED
	if (egl->state & EGL_STATE_IOPEN)
	{
		_mali_instrumented_term();
		egl->state &= ~EGL_STATE_IOPEN;
	}
#endif

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_START_STOP_SW_VR, 0, 0, 0, 0, 0);
#endif

#if MALI_INSTRUMENTED_PLUGIN_ENABLED
	_mali_instrumented_plugin_unload();
#endif

#if defined(EGL_MALI_GLES) || defined(EGL_MALI_VG)
	if (egl->state & EGL_STATE_GPOPEN)
	{
		_mali_gp_close( egl->base_ctx );
		egl->state &= ~EGL_STATE_GPOPEN;
	}
	if (egl->state & EGL_STATE_PPOPEN)
	{
		_mali_pp_close( egl->base_ctx );
		egl->state &= ~EGL_STATE_PPOPEN;
	}

	if (egl->state & EGL_STATE_MEMOPEN)
	{
		_mali_mem_close( egl->base_ctx );
		egl->state &= ~EGL_STATE_MEMOPEN;
	}

	if (NULL != egl->base_ctx)
	{
		_mali_base_context_destroy( egl->base_ctx );
		egl->base_ctx = NULL;
	}
#endif /* EGL_MALI_GLES || EGL_MALI_VG */

#if MALI_API_TRACE
	mali_api_trace_mark_for_deinit();
#endif

#if MALI_OPROFILE_SAMPLING
	mali_oprofile_close();
#endif

#if HAVE_ANDROID_OS
	#ifdef MALI_TEST_API
	MALI_DEBUG_PRINT(0, ("MALI_TEST: AUTO-TERM in EGL"));
	_mali_test_auto_term();
	#endif
#endif
}

EGLBoolean __egl_destroy_main_context_if_threads_released( ) 
{
	if ( NULL != __egl_main && NULL != __egl_main->thread &&
		 (0 == __mali_named_list_size( __egl_main->thread )) &&
		 (0 == __mali_named_list_size( __egl_main->display )) && 
		 (0 == __mali_named_list_size( __egl_main->egl_images )) )
	{
		__egl_destroy_main_context();
		return EGL_TRUE;
	}
	return EGL_FALSE;
}

void __egl_destroy_main_context( void )
{
	mali_err_code err;
	u32 iterator = 0;
	__egl_thread_state *tstate = NULL;

	if ( NULL == __egl_main ) return;

	/* free list of displays */
	if ( NULL != __egl_main->display )
	{
		__mali_named_list_free( __egl_main->display, NULL );
		__egl_main->display = NULL;
	}

	/* free list of threads */
	if ( NULL != __egl_main->thread )
	{
		tstate = __mali_named_list_iterate_begin( __egl_main->thread, &iterator );
		while ( NULL != tstate )
		{
			if ( NULL != tstate->api_gles ) _mali_sys_free( tstate->api_gles );
			if ( NULL != tstate->api_vg ) _mali_sys_free( tstate->api_vg );
			__mali_named_list_remove( __egl_main->thread, tstate->id );
#if MALI_ANDROID_API > 12
			if ( MALI_BASE_WORKER_NO_HANDLE != tstate->worker ) _mali_base_worker_destroy( tstate->worker );
#endif
			if ( NULL != tstate->current_sync ) 
			{
				_egl_sync_destroy_sync_node( tstate->current_sync );
				tstate->current_sync = NULL;
			}
			_mali_sys_free( tstate );
			tstate = __mali_named_list_iterate_begin( __egl_main->thread, &iterator );
		}
		__mali_named_list_free( __egl_main->thread, NULL );
		__egl_main->thread = NULL;
	}


	/* we might have the mutexes locked in case of abnormal termination (ctrl+c etc).
	 * try lock, unlock, destroy would fetch these cases
	 */
	if ( MALI_NO_HANDLE != __egl_main->main_lock )
	{
		err = _mali_sys_lock_try_lock( __egl_main->main_lock );
		_mali_sys_lock_unlock( __egl_main->main_lock );
		_mali_sys_lock_destroy( __egl_main->main_lock );
		__egl_main->main_lock = MALI_NO_HANDLE;
	}

#if EGL_KHR_reusable_sync_ENABLE
	if ( MALI_NO_HANDLE != __egl_main->sync_lock )
	{
		err = _mali_sys_lock_try_lock( __egl_main->sync_lock );
		_mali_sys_lock_unlock( __egl_main->sync_lock );
		_mali_sys_lock_destroy( __egl_main->sync_lock );
		__egl_main->sync_lock = MALI_NO_HANDLE;
	}
#endif

	if ( MALI_NO_HANDLE != __egl_main->image_lock )
	{
		err = _mali_sys_lock_try_lock( __egl_main->image_lock );
		_mali_sys_lock_unlock( __egl_main->image_lock );
		_mali_sys_lock_destroy( __egl_main->image_lock );
		__egl_main->image_lock = MALI_NO_HANDLE;
	}

	if ( NULL != __egl_main->mutex_vsync )
	{
		err = _mali_sys_mutex_try_lock ( __egl_main->mutex_vsync );
		_mali_sys_mutex_unlock( __egl_main->mutex_vsync );
		_mali_sys_mutex_destroy( __egl_main->mutex_vsync );
		__egl_main->mutex_vsync = MALI_NO_HANDLE;
	}

    if ( NULL != __egl_main->linker )
    {
#ifdef EGL_MALI_GLES
		__egl_gles_shutdown( __egl_main );
#endif /* EGL_MALI_GLES */

#ifdef EGL_MALI_VG
		__egl_vg_shutdown( __egl_main );
#endif /* EGL_MALI_VG */

		egl_linker_deinit( __egl_main->linker );
		_mali_sys_free( __egl_main->linker );
	}

#if EGL_KHR_image_ENABLE
	if ( NULL != __egl_main->egl_images ) __mali_named_list_free( __egl_main->egl_images, NULL );
#endif

#ifdef EGL_WORKER_THREAD_ENABLED
	if ( MALI_BASE_WORKER_NO_HANDLE != __egl_main->worker ) _mali_base_worker_destroy( __egl_main->worker );
#endif

	_mali_sys_free( __egl_main );
	__egl_main = NULL;
	MALI_IGNORE( err );
}

#if defined(LINUX)
void __attribute__ ((destructor)) __LINUXeglDestructor(void)
{
	mali_egl_cleanup_internal();
}
#endif /* LINUX */

#if defined(_WIN32) && !defined(MALI_MONOLITHIC)
// DLL entry point
BOOL WINAPI
DllMain(
    HANDLE hInst,
    DWORD dwReason,
    LPVOID lpvReserved
    )
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls((HMODULE) hInst);
        break;

    case DLL_PROCESS_DETACH:
	    mali_egl_cleanup_internal();
        break;

    default:
        break;
    }

    // return TRUE for success, FALSE for failure
    return TRUE;
}
#endif /* _WIN32 */
