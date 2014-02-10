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
#include <EGL/eglplatform.h>
#include <mali_system.h>
#include "mali_counters.h"
#include "egl_common.h"
#include "egl_surface.h"
#include "egl_display.h"
#include "egl_platform.h"
#include "egl_context.h"
#include "egl_main.h"
#include "egl_mali.h"
#include "egl_misc.h"
#include "egl_extensions.h"
#include "egl_handle.h"
#ifdef EGL_USE_VSYNC
#include "egl_vsync.h"
#endif
#ifdef MALI_DUMP_ENABLE
#include <base/dump/mali_dumping.h>
#endif
#include "sw_profiling.h"

#if MALI_TIMELINE_PROFILING_ENABLED
#include <base/mali_profiling.h>
#endif

#if EGL_KHR_lock_surface_ENABLE
#include "egl_lock_surface.h"
#endif

#define EGL_MAKE_VERSION_STRING_INTERNAL(major, minor, release_name) #major "." #minor " " #release_name
#define EGL_MAKE_VERSION_STRING(major, minor, release_name) EGL_MAKE_VERSION_STRING_INTERNAL(major, minor, release_name)

#if defined(EGL_MALI_VG) && defined(EGL_MALI_GLES)
	#define EGL_CLIENT_API_STRING "OpenGL_ES OpenVG"
#elif defined(EGL_MALI_VG)
	#define EGL_CLIENT_API_STRING "OpenVG"
#elif defined(EGL_MALI_GLES)
	#define EGL_CLIENT_API_STRING "OpenGL_ES"
#else
/* no API included */
	#define EGL_CLIENT_API_STRING ""
#endif

/**
 * @brief internal function for waiting on the current client API to finish
 * @param thread_state the current thread state
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean _egl_wait_API( void *thread_state )
{
	EGLenum api;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api = __egl_get_current_thread_state_api( tstate, &api );

	if ( EGL_OPENGL_ES_API == api )
	{
#ifdef EGL_MALI_GLES
		MALI_CHECK_NON_NULL( tstate->context_gles, EGL_FALSE );
		__egl_gles_finish( tstate );
#endif
	}
	else if ( EGL_OPENVG_API == api )
	{
#ifdef EGL_MALI_VG
		MALI_CHECK_NON_NULL( tstate->context_vg, EGL_FALSE );
		__egl_vg_finish( tstate );
#endif
	}

	MALI_IGNORE( tstate_api );

	return EGL_TRUE;
}

void __egl_set_error( EGLint error, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	MALI_DEBUG_ASSERT_POINTER( tstate );

	/* temp test */
	/*if(error != EGL_SUCCESS)
		fprintf(stderr, "[VR] EGL ERROR: 0x%x\n", error);
	*/
	if ( NULL == tstate ) return;
	tstate->error = error;
}

/* temp test */
#if 0
void __egl_set_error_temp( EGLint error, void *thread_state, const char* string, unsigned int line )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	MALI_DEBUG_ASSERT_POINTER( tstate );

#if defined(HAVE_ANDROID_OS)
	ADBG(1, "[VR] EGL ERROR: %s(%d), 0x%x\n", string, line, error);
#endif
	if(error != EGL_SUCCESS)
		fprintf(stderr, "[VR] EGL ERROR: %s(%d), 0x%x\n", string, line, error);

	if ( NULL == tstate ) return;
	tstate->error = error;
}
#endif

EGLint _egl_get_error( void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	return tstate->error;
}

const char* _egl_query_string( EGLDisplay __dpy, EGLint name, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	egl_display *dpy = NULL;
	__egl_main_context *egl = __egl_get_main_context();
	static const char egl_client_apis[] = EGL_CLIENT_API_STRING;
	#ifdef NEXELL_FEATURE_ENABLE	
	#ifdef DEBUG
	static const char egl_vendor[] = "Nexell-1.11D";
	#else
	static const char egl_vendor[] = "Nexell-1.11";
	#endif
	#else
	static const char egl_vendor[] = "ARM";
	#endif
	static const char egl_version[] = EGL_MAKE_VERSION_STRING( MALI_EGL_MAJOR_VERSION, MALI_EGL_MINOR_VERSION, MALI_RELEASE_NAME );
	static const char egl_extensions[] = {
	""
	};

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), NULL );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), NULL );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

	switch (name)
	{
		case EGL_CLIENT_APIS:   return egl_client_apis;
		case EGL_VENDOR:        return egl_vendor;
		case EGL_VERSION:       return egl_version;
		case EGL_EXTENSIONS:
			if ( (egl->linker->caps & (_EGL_LINKER_OPENGL_ES_BIT|_EGL_LINKER_OPENGL_ES2_BIT)) == (_EGL_LINKER_OPENGL_ES_BIT|_EGL_LINKER_OPENGL_ES2_BIT) ) return egl_extensions_gles1_gles2;
			else if ( egl->linker->caps & _EGL_LINKER_OPENGL_ES_BIT ) return egl_extensions_gles1;
			else if ( egl->linker->caps & _EGL_LINKER_OPENGL_ES2_BIT ) return egl_extensions_gles2;
			else if ( egl->linker->caps & _EGL_LINKER_OPENVG_BIT ) return egl_extensions_gles1; /* this is not an error, either gles1 or gles2 ext list will do */
			return egl_extensions;
		default:
			__egl_set_error( EGL_BAD_PARAMETER, tstate );
	}

	return NULL;
}

/**
 * @brief Retrieves a proc address from the platform
 * @param procname procname to grab
 * @return pointer to proc
 */
void (* _egl_platform_get_proc_address(const char *procname ))(void)
{
    __egl_main_context *egl = __egl_get_main_context();
	__eglMustCastToProperFunctionPointerType funcptr = NULL;

	if ( egl->linker->caps & _EGL_LINKER_PLATFORM_BIT )
	{
		if ( NULL != egl->linker->platform_get_proc_address )
		{
            funcptr = egl->linker->platform_get_proc_address(egl->linker->handle_platform, procname);
		}
	}
	return funcptr;
}

void (* _egl_get_proc_address( const char *procname, void *thread_state ))()
{
	void (*proc)(void) = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	MALI_CHECK_NON_NULL( procname, NULL );
	MALI_IGNORE(tstate);

    proc = _egl_platform_get_proc_address( procname );
    if( proc != NULL ) return proc;

	/* Check what API procname belongs to. GLES, VG or EGL */
#if EGL_MALI_GLES
	proc = __egl_gles_get_proc_address( tstate, procname );
	if ( proc != NULL ) return proc;
#endif

#ifdef EGL_MALI_VG
	proc = __egl_vg_get_proc_address( tstate, procname );
	if ( proc != NULL ) return proc;
#endif

	proc = _egl_get_proc_address_internal( procname );

	return proc;
}

EGLBoolean _egl_wait_client( void *thread_state )
{
	EGLenum api;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api = NULL;

	tstate_api = __egl_get_current_thread_state_api( tstate, &api );
	MALI_CHECK_NON_NULL( tstate_api, EGL_TRUE );
	MALI_CHECK_NON_NULL( tstate_api->context, EGL_TRUE );
	MALI_CHECK_NON_NULL( tstate_api->context->api_context, EGL_TRUE );

	/* We need to handle pixmap flush of color data in here.
	 * EGL 1.4 spec, section 3.8
	 * "All rendering calls for the currently bound context, for the current rendering API,
	 *  made prior to eglWaitClient, are guaranteed to be executed before native rendering
	 *  calls made after eglWaitClient which affect the surface associated with that context.
	 *  Clients rendering to single buffered surfaces (e.g. pixmap surfaces) should call
	 *  eglWaitClient before accessing the native pixmap from the client."
	 */

	if ( MALI_EGL_PIXMAP_SURFACE != tstate_api->draw_surface->type ) return EGL_TRUE;

#if EGL_BACKEND_X11
	if ( EGL_FALSE == __egl_mali_render_surface_to_pixmap( tstate_api->draw_surface, tstate_api->draw_surface->pixmap, EGL_FALSE, tstate, tstate_api ) )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_FALSE;
	}
	/* begin a new frame - this will enable the acquire output callback again */
	__egl_platform_begin_new_frame( tstate_api->draw_surface );
#else
	if ( EGL_FALSE == __egl_mali_render_surface_to_pixmap( tstate_api->draw_surface, tstate_api->draw_surface->pixmap, EGL_TRUE, tstate, tstate_api ) )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_FALSE;
	}
#endif

	return EGL_TRUE;
}

EGLBoolean _egl_wait_GL( void *thread_state )
{
	EGLBoolean retval = EGL_TRUE;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	MALI_CHECK_NON_NULL( tstate->context_gles, EGL_TRUE );

#ifdef EGL_MALI_GLES
	{
		EGLenum api = EGL_NONE;

		api = _egl_query_api( tstate );
		MALI_CHECK( EGL_TRUE == _egl_bind_api( EGL_OPENGL_ES_API, tstate ), EGL_FALSE );
		retval = _egl_wait_client( thread_state );
		MALI_CHECK( EGL_TRUE == _egl_bind_api( api, tstate ), EGL_FALSE );
	}
#endif

	return retval;
}

EGLBoolean _egl_wait_native( EGLint engine, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api = NULL;

	/* if there is no current context, the function has no effect, but returns EGL_TRUE */
	tstate_api = __egl_get_current_thread_state_api( tstate, NULL );
	MALI_CHECK_NON_NULL( tstate_api, EGL_TRUE );
	MALI_CHECK( EGL_NO_CONTEXT != tstate_api->context, EGL_TRUE );

	/* if the surface does not support native rendering (pbuffers and window surfaces), return EGL_TRUE */
	MALI_DEBUG_ASSERT_POINTER( tstate_api->draw_surface );
	if ( MALI_EGL_PIXMAP_SURFACE != tstate_api->draw_surface->type ) return EGL_TRUE;

	/* check that we have a valid marking engine */
	switch( engine )
	{
		case EGL_CORE_NATIVE_ENGINE: /* the only one recognized */
			break;

		default:
			__egl_set_error( EGL_BAD_PARAMETER, tstate );
			return EGL_FALSE;
	}

	/* surface is current and a pixmap, so let the rest be up to the native layer */
	if ( EGL_FALSE == __egl_platform_wait_native( engine ) )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_FALSE;
	}

#ifdef EGL_MALI_VG
	if ( EGL_OPENVG_API == tstate->api_current )
	{
		if ( NULL != tstate->api_vg && NULL != tstate->api_vg->context )
		{
			if ( EGL_FALSE == __egl_vg_set_framebuilder( tstate->api_vg->draw_surface, tstate ) )
			{
				__egl_set_error( EGL_BAD_ALLOC, tstate );
				return EGL_FALSE;
			}
		}
	}
#endif

#ifdef EGL_MALI_GLES
	if ( EGL_OPENGL_ES_API == tstate->api_current )
	{
		if ( NULL != tstate->api_gles && NULL != tstate->api_gles->context )
		{
			if ( EGL_FALSE == __egl_gles_set_framebuilder( tstate->api_gles->draw_surface, tstate ) )
			{
				__egl_set_error( EGL_BAD_ALLOC, tstate );
				return EGL_FALSE;
			}
		}
	}
#endif

	return EGL_TRUE;
}

EGLBoolean _egl_swap_buffers( EGLDisplay __dpy, EGLSurface __draw, EGLint numrects, const EGLint* rects, void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *draw = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api = NULL;
	EGLenum api;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate), EGL_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (draw = __egl_get_check_surface( __draw, __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SINGLE_SW_EGL_SWAP_BUFFERS, 0, 0, 0, 0, 0);
#endif

	/* check if current rendering api have a context */
	tstate_api = __egl_get_current_thread_state_api( tstate, &api );

#if EGL_KHR_lock_surface_ENABLE
	if ( EGL_FALSE != __egl_lock_surface_is_locked( draw ) )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}
	if ( (draw->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) && ( MALI_EGL_WINDOW_SURFACE == draw->type) )
	{
#if EGL_BACKEND_X11
		/* acquire_buffer in case application doesn't call eglLockSurface/eglQuerySurface */
		_mali_frame_builder_acquire_output( draw->frame_builder );
		draw->swap_func( tstate->main_ctx->base_ctx, dpy->native_dpy, draw, NULL, draw->interval );
		__egl_platform_begin_new_frame( draw );
#else
		draw->swap_func( tstate->main_ctx->base_ctx, dpy->native_dpy, draw, NULL, draw->interval );
#endif

		return EGL_TRUE;
	}
#endif

	/* check if current rendering api have a context */
	if ( NULL == tstate_api || EGL_NO_CONTEXT == tstate_api->context )
	{	
		__egl_set_error( EGL_BAD_CONTEXT, tstate );
		return EGL_FALSE;
	}

	/* return EGL_CONTEXT_LOST in case of power management event */
	if ( EGL_TRUE == tstate_api->context->is_lost )
	{
		__egl_set_error( EGL_CONTEXT_LOST, tstate );
		return EGL_FALSE;
	}

	/* Check if the surface is current on this thread */
	/* EGL_KHR_lock_surface2 : This restriction does not apply to lockable surfaces; for such
	 * a surface, eglSwapBuffers and eglCopyBuffers may be called for a surface not bound to any client API context */
#if EGL_KHR_lock_surface_ENABLE
	if ( (draw->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) == 0 )
	{
#endif
		if ( tstate_api->draw_surface != draw )
		{
			__egl_set_error( EGL_BAD_SURFACE, tstate );
			return EGL_FALSE;
		}
#if EGL_KHR_lock_surface_ENABLE
	}
#endif

	/* Section 3.9 in EGL 1.4 Spec. Posting the colour buffer.
	 * If surface is a single-buffered window, pixmap or pbuffer surface, eglSwapBuffers has no effect.
	 */
	if ( MALI_EGL_PBUFFER_SURFACE == draw->type || MALI_EGL_PIXMAP_SURFACE == draw->type  || EGL_SINGLE_BUFFER == draw->render_buffer )
	{
		return EGL_TRUE;
	}

	/* skip the swap if the framebuilder is unmodified */
	if ( MALI_FALSE == _mali_frame_builder_is_modified( draw->frame_builder ) ) return EGL_TRUE;

	/* grab the vsync ticker */
#ifdef EGL_USE_VSYNC
	if ( 0 != draw->interval ) draw->vsync_recorded = __egl_vsync_get();
#endif

#if EGL_KHR_reusable_sync_ENABLE
	/*
	 * NB! We will need to unlock the sync mutex to avoid blocking any
	 * composition layer depending on the EGL_KHR_reusable_sync extension.
	 * This because a deadlock may happen if any locks are taken while we
	 * are waiting for the composition layer to provide a buffer to post
	 * into, while the composition layer waits for any locks hold at this
	 * point.
	 *
	 * This will work as long as we a) are not accessing any sync objects
	 * within the window posting code, and b) if we are not accessing any
	 * window buffers within the sync functionality. But this is strictly
	 * not needed, and thus not done.
	 *
	 * The sync lock must be locked after return of window post operation.
	 */
	__egl_sync_mutex_unlock();
#endif

	/* Pass numrects and rects to platform layer */
	__egl_platform_set_swap_region( draw, numrects, rects );

	/* API-specific flush calls are in __egl_mali_post_to_window_surface. */

	if ( EGL_FALSE == __egl_mali_post_to_window_surface( draw, tstate, tstate_api ) )
	{
#if EGL_KHR_reusable_sync_ENABLE
		__egl_sync_mutex_lock();
#endif
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_FALSE;
	}
	else
	{
		#ifdef MALI_DUMP_ENABLE
		_mali_base_common_dump_frame_counter_increment(tstate->main_ctx->base_ctx);
		#endif
	}

#if EGL_KHR_reusable_sync_ENABLE
	__egl_sync_mutex_lock();
#endif

	return EGL_TRUE;
}

EGLBoolean _egl_copy_buffers( EGLDisplay __dpy, EGLSurface __surface, EGLNativePixmapType target, void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *surface = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api = NULL;
	EGLenum api;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

	if ( EGL_FALSE == __egl_platform_pixmap_valid( target ) )
	{
		__egl_set_error( EGL_BAD_NATIVE_PIXMAP, tstate );
		return EGL_FALSE;
	}

	MALI_CHECK( EGL_NO_SURFACE != (surface = __egl_get_check_surface( __surface, __dpy, tstate ) ), EGL_FALSE );

	/* check if current rendering api have a context */
	tstate_api = __egl_get_current_thread_state_api( tstate, &api );

	if ( NULL == tstate_api || EGL_NO_CONTEXT == tstate_api->context )
	{	
		__egl_set_error( EGL_BAD_CONTEXT, tstate );
		return EGL_FALSE;
	}

	/* return EGL_CONTEXT_LOST in case of power management event */
	if ( EGL_TRUE == tstate_api->context->is_lost )
	{
		__egl_set_error( EGL_CONTEXT_LOST, tstate );
		return EGL_FALSE;
	}

	/* Check if the surface is current on this thread */
	/* EGL_KHR_lock_surface2 version2: This restriction does not apply to lockable surfaces; for such
	 * a surface, eglSwapBuffers and eglCopyBuffers may be called for a surface not bound to any client API context */
#if EGL_KHR_lock_surface_ENABLE
	if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) == 0 )
	{
#endif
		if ( tstate_api->draw_surface != surface )
		{
			__egl_set_error( EGL_BAD_SURFACE, tstate );
			return EGL_FALSE;
		}
#if EGL_KHR_lock_surface_ENABLE
	}
#endif

	/* check if target pixmap is compatible with config */
	if ( EGL_FALSE == __egl_platform_pixmap_copybuffers_compatible( dpy->native_dpy, target, surface ) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_FALSE;
	}

	/* map the given pixmap, and a blocking flush into it */
	if ( EGL_FALSE == __egl_mali_render_surface_to_pixmap( surface, target, EGL_TRUE, tstate, tstate_api ) )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_FALSE;
	}

	return EGL_TRUE;
}

EGLBoolean _egl_swap_interval( EGLDisplay __dpy, EGLint interval, void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *surface = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api = NULL;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

	/* grab the currently bound api */
	tstate_api = __egl_get_current_thread_state_api( tstate, NULL );

	if ( NULL == tstate_api || EGL_NO_CONTEXT == tstate_api->context )
	{
		__egl_set_error( EGL_BAD_CONTEXT, tstate );
		return EGL_FALSE;
	}

	surface = tstate_api->draw_surface;

	/* clamp the values */
	if ( interval < surface->config->min_swap_interval ) interval = surface->config->min_swap_interval;
	else if ( interval > surface->config->max_swap_interval ) interval = surface->config->max_swap_interval;

	tstate_api->draw_surface->interval = interval;

  return EGL_TRUE;
}

EGLBoolean _egl_bind_tex_image( EGLDisplay __dpy, EGLSurface __surface, EGLint buffer, void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *surface = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
#ifdef EGL_MALI_GLES
	__egl_thread_state_api *tstate_api = NULL;
	EGLenum api = EGL_NONE;
	egl_context *ctx;
#endif

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (surface = __egl_get_check_surface( __surface, __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

#if EGL_KHR_lock_surface_ENABLE
	if ( EGL_FALSE != __egl_lock_surface_is_locked( surface ) )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}
#endif

	/* check validity of buffer parameter */
	if ( EGL_BACK_BUFFER != buffer )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return EGL_FALSE;
	}

	/* check if buffer is bound */
	if ( EGL_TRUE == surface->is_bound )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}

#ifndef EGL_MALI_GLES
	MALI_IGNORE( surface );

	__egl_set_error( EGL_BAD_SURFACE, tstate );
	return EGL_FALSE;
#else

	/* check if surface is a pbuffer surface */
	if ( !(MALI_EGL_PBUFFER_SURFACE & surface->type) )
	{
		__egl_set_error( EGL_BAD_SURFACE, tstate );
		return EGL_FALSE;
	}

	/* check if config used to create surface supports OpenGL ES rendering */
	if ( !(EGL_OPENGL_ES_BIT & surface->config->renderable_type) && !(EGL_OPENGL_ES2_BIT & surface->config->renderable_type) )
	{
		__egl_set_error( EGL_BAD_SURFACE, tstate );
		return EGL_FALSE;
	}

	/* If eglBindTexImage is called and the surface attribute EGL_TEXTURE_FORMAT
	 * is set to EGL_NO_TEXTURE, then an EGL BAD MATCH error is returned. */
	if ( EGL_NO_TEXTURE == surface->texture_format )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_FALSE;
	}

	/* eglBindTexImage is ignored if there is no current rendering context */
	if ( EGL_NO_CONTEXT == _egl_get_current_context( tstate ) ) return EGL_TRUE;

	tstate_api = __egl_get_current_thread_state_api( tstate, &api );

	ctx = tstate_api->context;

	/* Add the surface we are about to bind as an image to the list of bound images in the context,
	 * but only if it is GLES
	 */
	if ( EGL_OPENGL_ES_API == api )
	{
		mali_err_code error;

		error = __mali_linked_list_insert_data( &ctx->bound_images, surface );

		if ( MALI_ERR_NO_ERROR != error )
		{
			return EGL_FALSE;
		}
	}

	if ( EGL_FALSE == __egl_gles_bind_tex_image( surface, tstate ) )
	{
		/* Remove the surface again */

		if ( EGL_OPENGL_ES_API == api )
		{
			__egl_context_unbind_bound_surface( ctx, surface );
		}
	}

	return EGL_TRUE;
#endif
}

EGLBoolean _egl_release_tex_image( EGLDisplay __dpy, EGLSurface __surface, EGLint buffer, void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *surface = NULL;

	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
#ifdef EGL_MALI_GLES
	EGLenum api = EGL_NONE;
    __egl_thread_state_api *tstate_api = __egl_get_current_thread_state_api( tstate, &api );
#endif

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (surface = __egl_get_check_surface( __surface, __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

	if ( EGL_BACK_BUFFER != buffer )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return EGL_FALSE;
	}

#ifndef EGL_MALI_GLES
	MALI_IGNORE( surface );

	__egl_set_error( EGL_BAD_SURFACE, tstate );
	return EGL_FALSE;
#else

	/* check if surface is a pbuffer surface */
	if ( !(MALI_EGL_PBUFFER_SURFACE & surface->type) )
	{
		__egl_set_error( EGL_BAD_SURFACE, tstate );
		return EGL_FALSE;
	}

	/* check if config used to create surface supports OpenGL ES rendering */
	if ( !(EGL_OPENGL_ES_BIT & surface->config->renderable_type) && !(EGL_OPENGL_ES2_BIT & surface->config->renderable_type) )
	{
		__egl_set_error( EGL_BAD_SURFACE, tstate );
		return EGL_FALSE;
	}

	/* EGL_BAD_MATCH if the value of EGL_TEXTURE_FORMAT is EGL_NO_TEXTURE */
	if ( EGL_NO_TEXTURE == surface->texture_format )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_FALSE;
	}

	if ( EGL_FALSE == surface->is_bound )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}

	/* verify that we have a gles context current */
	if ( tstate->api_gles == NULL )
	{
		__egl_set_error( EGL_BAD_CONTEXT, tstate );
		return EGL_FALSE;
	}

	/* Remove the bound image from the list since it is now not needed to remove it on context deletion,
	 * but only if GLES
	 */
	if ( EGL_OPENGL_ES_API == api )
	{
		__egl_context_unbind_bound_surface( tstate->api_gles->context, surface );
	}

#ifdef EGL_MALI_GLES
	__egl_gles_unbind_tex_image( surface, tstate );
#endif


	surface->is_bound = EGL_FALSE;
	MALI_IGNORE(tstate_api);

	return EGL_TRUE;
#endif
}

EGLenum _egl_fence_flush( void *thread_state )
{
	EGLenum                api = 0;
	__egl_thread_state     *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api;
	EGLenum ret = EGL_SUCCESS;

	tstate_api = __egl_get_current_thread_state_api( tstate, &api );
	MALI_IGNORE( tstate_api );

	if ( EGL_OPENGL_ES_API == api )
	{
#ifdef EGL_MALI_GLES
		if ( NULL != tstate->context_gles )
		{
			/* invoke fence flush in GLES; in some cases two flushes
			 * are needed (or: incremental render + flush) */
			ret =__egl_gles_fence_flush( tstate );
		}
#endif
	}
	else if ( EGL_OPENVG_API == api )
	{
#ifdef EGL_MALI_VG
		if ( NULL != tstate->context_vg )
		{
			/* normal flush in VG */
			__egl_vg_flush( tstate );
		}
#endif
	}
	return ret;
}
