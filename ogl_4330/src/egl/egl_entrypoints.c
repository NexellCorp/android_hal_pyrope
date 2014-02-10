/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_entrypoints.c
 * @brief EGL API entrypoints
 * @note Please refer to the EGL spec for API documentation.
 */

#include "egl_api_trace.h"
#include <mali_system.h>
#include <EGL/eglplatform.h>
#include <EGL/mali_egl.h>
#include <EGL/mali_eglext.h>
#include "egl_entrypoints.h"
#include "egl_config_extension.h"
#include "egl_misc.h"
#include "egl_config.h"
#include "egl_thread.h"
#include "egl_display.h"
#include "egl_context.h"
#include "egl_handle.h"

#if EGL_KHR_image_ENABLE
#include "egl_image.h"
#endif

#include "egl_api_trace.h"

#if EGL_KHR_reusable_sync_ENABLE
#include "egl_sync.h"
#endif

#if EGL_KHR_lock_surface_ENABLE
#include "egl_lock_surface.h"
#endif

#if !defined( NDEBUG )
/* nexell add */
#if defined(HAVE_ANDROID_OS)
#include <android/log.h>
#include "../egl/egl_image_android.h"
#endif
#endif

#if MALI_PERFORMANCE_KIT

static int mali_measure_frame_time = -1;

#define MAX_NUM_TIMESTAMPS 2048
#define NUM_FRAMES_TO_MEASURE 100

static u32 *timestamps = NULL;
static u32 num_timestamps = 0;
static u32 framecount = 0;

static void addtimestamp(void)
{
	if ( num_timestamps >= MAX_NUM_TIMESTAMPS ) return;
	timestamps[ num_timestamps++ ] = (u32)_mali_sys_get_time_usec();
}

static void printtimestamps(void)
{
	u32 i;
	for ( i = 1; i < num_timestamps; i++ )
	{
		_mali_sys_printf( "%f\n", ((float)(timestamps[i]-timestamps[i-1]))/1000 );
	}
}
#endif

/** config **/

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglGetConfigs )( EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetConfigs);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLCONFIG_ARRAY(MALI_API_TRACE_OUT, configs, config_size);
	MALI_API_TRACE_PARAM_EGLINT(config_size);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_OUT, num_config, 1);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_get_configs( dpy, configs, config_size, num_config, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLCONFIG_ARRAY(2, configs, (num_config != NULL && retval == EGL_TRUE) ? *num_config : 0);
	MALI_API_TRACE_RETURN_EGLINT_ARRAY(4, num_config, EGL_TRUE == retval ? 1 : 0);
	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglGetConfigs);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglChooseConfig )( EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglChooseConfig);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_ATTRIB_ARRAY(attrib_list, mali_api_trace_attrib_size(attrib_list)); /* can contain pixmap ref */
	MALI_API_TRACE_PARAM_EGLCONFIG_ARRAY(MALI_API_TRACE_OUT, configs, config_size);
	MALI_API_TRACE_PARAM_EGLINT(config_size);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_OUT, num_config, 1);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_choose_config( dpy, attrib_list, configs, config_size, num_config, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLCONFIG_ARRAY(3, configs, (num_config != NULL && retval == EGL_TRUE) ? *num_config : 0);
	MALI_API_TRACE_RETURN_EGLINT_ARRAY(5, num_config, EGL_TRUE == retval ? 1 : 0);
	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglChooseConfig);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglGetConfigAttrib )( EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetConfigAttrib);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLCONFIG(config);
	MALI_API_TRACE_PARAM_EGLINT(attribute);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_OUT, value, 1);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_get_config_attrib( dpy, config, attribute, value, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLINT_ARRAY(4, value, EGL_TRUE == retval ? 1 : 0);
	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglGetConfigAttrib);
	EGL_LEAVE_FUNC();

	return retval;
}

/** misc **/

MALI_EGLAPI EGLint MALI_EGL_APIENTRY( eglGetError )( void )
{
	EGLint retval = EGL_SUCCESS;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetError);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_NOP );
	if (NULL != tstate)
	{
		retval = _egl_get_error( tstate );
		/* the call was successful, so reset the error state back to EGL_SUCCESS */
		__egl_set_error( EGL_SUCCESS, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_NOP );
	}

	MALI_API_TRACE_RETURN_EGLINT(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglGetError);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI const char * MALI_EGL_APIENTRY( eglQueryString )( EGLDisplay dpy, EGLint name )
{
	const char *retval = "";
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglQueryString);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLINT(name);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_query_string( dpy, name, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_CHAR_ARRAY(0, retval, NULL != retval ? _mali_sys_strlen(retval) + 1 : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglQueryString);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglWaitClient )( void )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglWaitClient);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_wait_client( tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglWaitClient);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglWaitGL )( void )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglWaitGL);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_wait_GL( tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglWaitGL);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglWaitNative )( EGLint engine )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglWaitNative);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLINT(engine);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_wait_native( engine, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglWaitNative);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglSwapBuffers )( EGLDisplay dpy, EGLSurface surface )
{
	EGLBoolean success = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglSwapBuffers);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLSURFACE(surface);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_SYNC_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		success = _egl_swap_buffers( dpy, surface, 0, NULL, tstate );
#if MALI_PERFORMANCE_KIT
		if (mali_measure_frame_time == -1)
		{
			mali_measure_frame_time = (int)_mali_sys_config_string_get_s64("MALI_MEASURE_FRAME_TIME", 0, 0, 1);
		}

		if (mali_measure_frame_time == 1)
		{
			if ( NULL == timestamps )
			{
				timestamps = (u32*)_mali_sys_malloc(MAX_NUM_TIMESTAMPS*sizeof(u32));
			}
			if ( NULL != timestamps )
			{
				addtimestamp();
			}
			framecount++;
			if ( framecount == NUM_FRAMES_TO_MEASURE )
			{
				printtimestamps();
			}
		}
#endif
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
	}

#ifdef NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_SYNC_LOCK );
	if (NULL != tstate)
	{
		/* FBO, render target = screen */		
		_nx_egl_platform_set_swap_n_scale( dpy, surface, tstate );
		success = _egl_swap_buffers( dpy, surface, 0, NULL, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
		/* render target = FBO */
		__nx_egl_platform_reset_swap_n_scale( dpy, surface, tstate );
	}
#endif

	MALI_API_TRACE_RETURN_EGLBOOLEAN(success);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglSwapBuffers);
	EGL_LEAVE_FUNC();

	return success;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglCopyBuffers )( EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglCopyBuffers);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLSURFACE(surface);
	MALI_API_TRACE_PARAM_EGLNATIVEPIXMAPTYPE(target);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_copy_buffers( dpy, surface, target, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglCopyBuffers);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglSwapInterval )( EGLDisplay dpy, EGLint interval )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglSwapInterval);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLINT(interval);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_swap_interval( dpy, interval, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglSwapInterval);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglBindTexImage )( EGLDisplay dpy, EGLSurface surface, EGLint buffer )
{
	EGLBoolean success = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglBindTexImage);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLSURFACE(surface);
	MALI_API_TRACE_PARAM_EGLINT(buffer);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		success = _egl_bind_tex_image( dpy, surface, buffer, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(success);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglBindTexImage);
	EGL_LEAVE_FUNC();

	return success;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglReleaseTexImage )( EGLDisplay dpy, EGLSurface surface, EGLint buffer )
{
	EGLBoolean success = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglReleaseTexImage);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLSURFACE(surface);
	MALI_API_TRACE_PARAM_EGLINT(buffer);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		success = _egl_release_tex_image( dpy, surface, buffer, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(success);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglReleaseTexImage);
	EGL_LEAVE_FUNC();

	return success;
}

/** context **/

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglBindAPI )( EGLenum api )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglBindAPI);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLENUM(api);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_bind_api( api, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglBindAPI);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLenum MALI_EGL_APIENTRY( eglQueryAPI )()
{
	EGLenum retval = EGL_NONE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglQueryAPI);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_query_api( tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLENUM(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglQueryAPI);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLContext MALI_EGL_APIENTRY( eglCreateContext )( EGLDisplay dpy, EGLConfig config, EGLContext share_list, const EGLint *attrib_list)
{
	EGLContext retval = EGL_NO_CONTEXT;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglCreateContext);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLCONFIG(config);
	MALI_API_TRACE_PARAM_EGLCONTEXT(share_list);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_IN, attrib_list, mali_api_trace_attrib_size(attrib_list));

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_create_context( dpy, config, share_list, attrib_list, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLCONTEXT(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglCreateContext);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglDestroyContext )( EGLDisplay dpy, EGLContext ctx )
{
	EGLBoolean success = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglDestroyContext);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLCONTEXT(ctx);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		success = _egl_destroy_context( dpy, ctx, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}
	MALI_API_TRACE_RETURN_EGLBOOLEAN(success);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglDestroyContext);
	EGL_LEAVE_FUNC();

	return success;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglQueryContext )( EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglQueryContext);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLCONTEXT(ctx);
	MALI_API_TRACE_PARAM_EGLINT(attribute);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_OUT, value, 1);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_query_context( dpy, ctx, attribute, value, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLINT_ARRAY(4, value, EGL_TRUE == retval ? 1 : 0);
	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglQueryContext);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglMakeCurrent )( EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx )
{
	EGLBoolean success = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglMakeCurrent);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLSURFACE(draw);
	MALI_API_TRACE_PARAM_EGLSURFACE(read);
	MALI_API_TRACE_PARAM_EGLCONTEXT(ctx);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_NOP );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		__egl_lock_surface( dpy, draw );
		if ( draw != read ) __egl_lock_surface( dpy, read );
		success = _egl_make_current( dpy, draw, read, ctx, tstate );
		__egl_unlock_surface( dpy, draw );
		if ( draw != read ) __egl_unlock_surface( dpy, read );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_NOP );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(success);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglMakeCurrent);
	EGL_LEAVE_FUNC();

	return success;
}

MALI_EGLAPI EGLContext MALI_EGL_APIENTRY( eglGetCurrentContext )( void )
{
	EGLContext retval = EGL_NO_CONTEXT;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetCurrentContext);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_get_current_context( tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLCONTEXT(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglGetCurrentContext);
	EGL_LEAVE_FUNC();

	return retval;
}

/** display **/

MALI_EGLAPI EGLDisplay MALI_EGL_APIENTRY( eglGetDisplay )( EGLNativeDisplayType display_id )
{
	EGLDisplay retval = EGL_NO_DISPLAY;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetDisplay);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLNATIVEDISPLAYTYPE(display_id);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		__egl_image_mutex_lock();
		retval = _egl_get_display( display_id, tstate );
		__egl_image_mutex_unlock();
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLDISPLAY(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglGetDisplay);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLDisplay MALI_EGL_APIENTRY( eglGetCurrentDisplay )( void )
{
	EGLDisplay retval = EGL_NO_DISPLAY;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetCurrentDisplay);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_get_current_display( tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLDISPLAY(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglGetCurrentDisplay);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglInitialize )( EGLDisplay dpy, EGLint *major, EGLint *minor )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglInitialize);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_OUT, major, 1);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_OUT, minor, 1);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		__egl_image_mutex_lock();
		retval = _egl_initialize( dpy, major, minor, tstate );
		__egl_image_mutex_unlock();
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLINT_ARRAY(2, major, EGL_TRUE == retval ? 1 : 0);
	MALI_API_TRACE_RETURN_EGLINT_ARRAY(3, minor, EGL_TRUE == retval ? 1 : 0);
	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglInitialize);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglTerminate )( EGLDisplay dpy )
{
	EGLBoolean success = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	SYNC_DEBUG( _mali_sys_printf( "\n[thread:%p] >>> %s(0x%x)\n", _mali_sys_get_tid(), MALI_FUNCTION, dpy ); );
    MALI_PROFILING_ENTER_FUNC(eglTerminate);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		__egl_image_mutex_lock();
		success = _egl_terminate( dpy, tstate );
		__egl_image_mutex_unlock();
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(success);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglTerminate);
	EGL_LEAVE_FUNC();
	SYNC_DEBUG( _mali_sys_printf( "\n[thread:%p] <<< %s\n", _mali_sys_get_tid(), MALI_FUNCTION ); );

	return success;
}

/** surface **/

MALI_EGLAPI EGLSurface MALI_EGL_APIENTRY( eglCreateWindowSurface )( EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list )
{
	EGLSurface retval = EGL_NO_SURFACE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglCreateWindowSurface);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLCONFIG(config);
	MALI_API_TRACE_PARAM_EGLNATIVEWINDOWTYPE(win);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_IN, attrib_list, mali_api_trace_attrib_size(attrib_list));

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_create_window_surface( dpy, config, win, attrib_list, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLSURFACE(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglCreateWindowSurface);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLSurface MALI_EGL_APIENTRY( eglCreatePbufferSurface )( EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
	EGLSurface surface = EGL_NO_SURFACE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglCreatePbufferSurface);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLCONFIG(config);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_IN, attrib_list, mali_api_trace_attrib_size(attrib_list));

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		surface = _egl_create_pbuffer_surface( dpy, config, attrib_list, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLSURFACE(surface);

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglCreatePbufferSurface);
	EGL_LEAVE_FUNC();

	return surface;
}

MALI_EGLAPI EGLSurface MALI_EGL_APIENTRY( eglCreatePbufferFromClientBuffer )( EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint * attrib_list )
{
	EGLSurface retval = EGL_NO_SURFACE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglCreatePbufferFromClientBuffer);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLENUM(buftype);
	MALI_API_TRACE_PARAM_EGLCLIENTBUFFER_EGLCREATEPBUFFERFROMCLIENTBUFFER(buffer, buftype);
	MALI_API_TRACE_PARAM_EGLCONFIG(config);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_IN, attrib_list, mali_api_trace_attrib_size(attrib_list));

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_create_pbuffer_from_client_buffer( dpy, buftype, buffer, config, attrib_list, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLSURFACE(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglCreatePbufferFromClientBuffer);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLSurface MALI_EGL_APIENTRY( eglCreatePixmapSurface )( EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list)
{
	EGLSurface surface = EGL_NO_SURFACE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglCreatePixmapSurface);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLCONFIG(config);
	MALI_API_TRACE_PARAM_EGLNATIVEPIXMAPTYPE(pixmap);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_IN, attrib_list, mali_api_trace_attrib_size(attrib_list));

	MALI_API_TRACE_ADD_PIXMAP_DATA(pixmap);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		surface = _egl_create_pixmap_surface( dpy, config, pixmap, attrib_list, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLSURFACE(surface);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglCreatePixmapSurface);
	EGL_LEAVE_FUNC();

	return surface;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglDestroySurface )( EGLDisplay dpy, EGLSurface surface )
{
	EGLBoolean success = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglDestroySurface);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLSURFACE(surface);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		success = _egl_destroy_surface( dpy, surface, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(success);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglDestroySurface);
	EGL_LEAVE_FUNC();

	return success;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglQuerySurface )( EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglQuerySurface);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLSURFACE(surface);
	MALI_API_TRACE_PARAM_EGLINT(attribute);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_OUT, value, 1);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_query_surface( dpy, surface, attribute, value, tstate );

		MALI_API_TRACE_RETURN_EGLINT_ARRAY(4, value, MALI_TRUE == mali_api_trace_query_surface_has_return_data(dpy, surface, attribute, tstate) ? 1 : 0);

		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglQuerySurface);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglSurfaceAttrib )( EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglSurfaceAttrib);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLSURFACE(surface);
	MALI_API_TRACE_PARAM_EGLINT(attribute);
	MALI_API_TRACE_PARAM_EGLINT(value);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_NOP );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		__egl_lock_surface( dpy, surface );
		retval = _egl_surface_attrib( dpy, surface, attribute, value, tstate );
		__egl_unlock_surface( dpy, surface );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_NOP );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglSurfaceAttrib);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLSurface MALI_EGL_APIENTRY( eglGetCurrentSurface )( EGLint readdraw )
{
	EGLSurface retval = EGL_NO_SURFACE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetCurrentSurface);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLINT(readdraw);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if (NULL != tstate)
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_get_current_surface( readdraw, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLSURFACE(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglGetCurrentSurface);
	EGL_LEAVE_FUNC();

	return retval;
}

/** thread **/
MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglReleaseThread )( void )
{
	EGLBoolean success = EGL_FALSE;

	MALI_PROFILING_ENTER_FUNC(eglReleaseThread);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();

	/* We're calling main lock / unlock here, since we don't want to create a new
	 * thread state if the current one is deleted.
	 * Calling eglReleaseThread on a released thread is not an error
	 */
	__egl_all_mutexes_lock();
	success = _egl_release_thread();

	if ( EGL_FALSE == __egl_destroy_main_context_if_threads_released() ) __egl_all_mutexes_unlock();

	MALI_API_TRACE_RETURN_EGLBOOLEAN(success);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglReleaseThread);
	EGL_LEAVE_FUNC();

	return success;
}

/** extensions **/

MALI_EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY
	   MALI_EGL_APIENTRY( eglGetProcAddress )( const char *procname )
{
	__eglMustCastToProperFunctionPointerType retval = NULL;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetProcAddress);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_CHAR_ARRAY(MALI_API_TRACE_IN, procname, NULL != procname ? _mali_sys_strlen(procname) + 1 : 0, 0);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_get_proc_address( procname, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}

	/* No need to log the return value, since we will log when/if the returned function ptr is called anyway */
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglGetProcAddress);
	EGL_LEAVE_FUNC();

	return retval;
}

#if EGL_KHR_image_ENABLE
MALI_EGLAPI EGLImageKHR MALI_EGL_APIENTRY( eglCreateImageKHR )( EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, EGLint *attr_list )
{
	EGLImageKHR retval = EGL_NO_IMAGE_KHR;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglCreateImageKHR);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLCONTEXT(ctx);
	MALI_API_TRACE_PARAM_EGLENUM(target);
	MALI_API_TRACE_PARAM_EGLCLIENTBUFFER_EGLCREATEIMAGEKHR(buffer, target);
	MALI_API_TRACE_PARAM_EGLINT_ARRAY(MALI_API_TRACE_IN, attr_list, mali_api_trace_attrib_size(attr_list));

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_IMAGE_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_create_image_KHR( dpy, ctx, target, buffer, attr_list, tstate, NULL );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_IMAGE_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLIMAGEKHR(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglCreateImageKHR);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglDestroyImageKHR )( EGLDisplay dpy, EGLImageKHR image )
{
	EGLBoolean success = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglDestroyImageKHR);
	EGL_ENTER_FUNC();
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_EGLDISPLAY(dpy);
	MALI_API_TRACE_PARAM_EGLIMAGEKHR(image);

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_IMAGE_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		success = _egl_destroy_image_KHR( dpy, image, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_IMAGE_UNLOCK );
	}

	MALI_API_TRACE_RETURN_EGLBOOLEAN(success);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(eglDestroyImageKHR);
	EGL_LEAVE_FUNC();

	return success;
}
#endif /* EGL_KHR_image_ENABLE */

#if EGL_KHR_reusable_sync_ENABLE

/*
 * NB! The EGL_KHR_reusable_sync functionality must always take the
 * EGL_MAIN_MUTEX_SYNC_LOCK. They need to take a lower granularity
 * sync lock to avoid blocking any composition layer providing
 * window buffers.
 */

MALI_EGLAPI EGLSyncKHR MALI_EGL_APIENTRY( eglCreateSyncKHR )( EGLDisplay dpy, EGLenum type, const EGLint *attrib_list )
{
	EGLSyncKHR retval = EGL_NO_SYNC_KHR;
	__egl_thread_state *tstate = NULL;

	SYNC_DEBUG( _mali_sys_printf( "\n[thread:%p] >>> %s(0x%x, 0x%x, 0x%x)\n", _mali_sys_get_tid(), MALI_FUNCTION, dpy, type, attrib_list ); );
    MALI_PROFILING_ENTER_FUNC(eglCreateSyncKHR);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_SYNC_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_create_sync_KHR( dpy, type, attrib_list, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
	}

	MALI_PROFILING_LEAVE_FUNC(eglCreateSyncKHR);
	EGL_LEAVE_FUNC();
	SYNC_DEBUG( _mali_sys_printf( "[thread:%p] <<< %s\n\n", _mali_sys_get_tid(), MALI_FUNCTION); );

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglDestroySyncKHR )( EGLDisplay dpy, EGLSyncKHR sync )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	SYNC_DEBUG( _mali_sys_printf( "\n[thread:%p] >>> %s(0x%x, 0x%x)\n", _mali_sys_get_tid(), MALI_FUNCTION, dpy, sync ); );
    MALI_PROFILING_ENTER_FUNC(eglDestroySyncKHR);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_SYNC_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_destroy_sync_KHR( dpy, sync, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
	}
	MALI_PROFILING_LEAVE_FUNC(eglDestroySyncKHR);
	EGL_LEAVE_FUNC();
	SYNC_DEBUG( _mali_sys_printf( "[thread:%p] <<< %s\n\n", _mali_sys_get_tid(), MALI_FUNCTION); );

	return retval;
}

MALI_EGLAPI EGLint MALI_EGL_APIENTRY( eglClientWaitSyncKHR )( EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout )
{
	EGLint retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	SYNC_DEBUG( _mali_sys_printf( "\n[thread:%p] >>> %s(0x%x, 0x%x, 0x%x, 0x%x)\n", _mali_sys_get_tid(), MALI_FUNCTION, dpy, sync, flags, timeout ); );

	MALI_PROFILING_ENTER_FUNC(eglClientWaitSyncKHR);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_SYNC_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_client_wait_sync_KHR( dpy, sync, flags, timeout, tstate );
		/* thread state released in _egl_client_wait_sync_KHR */
	}
	MALI_PROFILING_LEAVE_FUNC(eglClientWaitSyncKHR);
	EGL_LEAVE_FUNC();
	SYNC_DEBUG( _mali_sys_printf( "[thread:%p] <<< %s\n\n", _mali_sys_get_tid(), MALI_FUNCTION); );

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglSignalSyncKHR )( EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglSignalSyncKHR);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_SYNC_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_signal_sync_KHR( dpy, sync, mode, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
	}
	MALI_PROFILING_LEAVE_FUNC(eglSignalSyncKHR);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglGetSyncAttribKHR )( EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetSyncAttribKHR);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_SYNC_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_get_sync_attrib_KHR( dpy, sync, attribute, value, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
	}
	MALI_PROFILING_LEAVE_FUNC(eglGetSyncAttribKHR);
	EGL_LEAVE_FUNC();

	return retval;
}
#endif /* EGL_KHR_reusable_sync_ENABLE */

#if EGL_ANDROID_native_fencesync_ENABLE
MALI_EGLAPI EGLint MALI_EGL_APIENTRY( eglDupNativeFenceFDANDROID )( EGLDisplay dpy, EGLSyncKHR sync)
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state *tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglDupNativeFenceFDANDROID);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_SYNC_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_dup_native_fence_ANDROID( dpy, sync, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_SYNC_UNLOCK );
	}
	MALI_PROFILING_LEAVE_FUNC(eglDupNativeFenceFDANDROID );
	EGL_LEAVE_FUNC();

	return retval;
}
#endif /* EGL_ANDROID_native_fence_sync_ENABLE */

#if EGL_KHR_lock_surface_ENABLE
MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglLockSurfaceKHR )( EGLDisplay display, EGLSurface surface, const EGLint* attrib_list )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state* tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglLockSurfaceKHR);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_lock_surface_KHR( display, surface, attrib_list, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}
	MALI_PROFILING_LEAVE_FUNC(eglLockSurfaceKHR);
	EGL_LEAVE_FUNC();

	return retval;
}

MALI_EGLAPI EGLBoolean MALI_EGL_APIENTRY( eglUnlockSurfaceKHR )( EGLDisplay display, EGLSurface surface )
{
	EGLBoolean retval = EGL_FALSE;
	__egl_thread_state* tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglUnlockSurfaceKHR);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_unlock_surface_KHR( display, surface, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}
	MALI_PROFILING_LEAVE_FUNC(eglUnlockSurfaceKHR);
	EGL_LEAVE_FUNC();

	return retval ;
}
#endif /* EGL_KHR_lock_surface_ENABLE */

#if EGL_KHR_image_system_ENABLE
MALI_EGLAPI EGLClientNameKHR EGLAPIENTRY MALI_EGL_APIENTRY( eglGetClientNameKHR )(EGLDisplay display)
{
	EGLClientNameKHR retval = NULL;
	__egl_thread_state* tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglGetClientNameKHR);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_get_client_name_KHR( display, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}
	MALI_PROFILING_LEAVE_FUNC(eglGetClientNameKHR);
	EGL_LEAVE_FUNC();

	return retval;

}

MALI_EGLAPI EGLImageNameKHR EGLAPIENTRY MALI_EGL_APIENTRY( eglExportImageKHR )(EGLDisplay display, EGLImageKHR image, EGLClientNameKHR target_client, const EGLint *attrib_list)
{
	EGLImageNameKHR retval = NULL;
	__egl_thread_state* tstate = NULL;

	MALI_PROFILING_ENTER_FUNC(eglExportImageKHR);
	EGL_ENTER_FUNC();
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if ( NULL != tstate )
	{
		__egl_set_error( EGL_SUCCESS, tstate );
		retval = _egl_export_image_KHR( display, image, target_client, attrib_list, tstate );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
	}
	MALI_PROFILING_LEAVE_FUNC(eglExportImageKHR);
	EGL_LEAVE_FUNC();

	return retval;
}
#endif /* EGL_KHR_image_system_ENABLE */
