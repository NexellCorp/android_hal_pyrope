/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifdef EGL_SIMULATE_VSYNC
#if ((!defined _XOPEN_SOURCE) || ((_XOPEN_SOURCE - 0) < 600))
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#if ((!defined _POSIX_C_SOURCE) || ((_POSIX_C_SOURCE - 0) < 200112L))
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#endif /* EGL_SIMULATE_VSYNC */

#include <pthread.h>
#include "egl_vsync.h"
#include "egl_main.h"
/* we'll simulate the vsync now, having a thread that calls
 * the callback at given intervals (given in Hz).
 */
#ifdef EGL_SIMULATE_VSYNC
static pthread_t vsync_simulation_thread;
static mali_bool vsync_simulate = MALI_FALSE;
static u32 vsync_hertz = 50;
#endif
void __egl_vsync_callback( void );

#ifdef EGL_SIMULATE_VSYNC
void *__egl_vsync_simulation_thread( void *params )
{
	__egl_vsync_reset();
	while ( vsync_simulate )
	{
		_mali_sys_usleep( 1000000 / vsync_hertz );
		__egl_vsync_callback();
	}

	return NULL;
}

void __egl_vsync_simulation_start( u32 hertz )
{
	int rc;
	vsync_hertz = hertz;
	vsync_simulate = MALI_TRUE;
	rc = pthread_create( &vsync_simulation_thread, NULL, __egl_vsync_simulation_thread, NULL );
	MALI_IGNORE(rc);
}

void __egl_vsync_simulation_stop( void )
{
	vsync_simulate = MALI_FALSE;
	pthread_join( vsync_simulation_thread, NULL );
}
#endif


void __egl_vsync_callback( void )
{
	__egl_main_context *egl = __egl_get_main_context();

	if ( NULL == egl ) return;

	_mali_sys_mutex_lock( egl->mutex_vsync );
	egl->vsync_busy = MALI_TRUE;
	egl->vsync_ticker++;
	egl->vsync_busy = MALI_FALSE;
	_mali_sys_mutex_unlock( egl->mutex_vsync );
}

u32 __egl_vsync_get( void )
{
	__egl_main_context *egl = __egl_get_main_context();

	MALI_CHECK_NON_NULL( egl, 0 );

	while(1)
	{
		if ( egl->vsync_busy )
		{
			_mali_sys_yield();
		}
		else
		{
			return egl->vsync_ticker;
		}
	}
}

void __egl_vsync_reset( void )
{
	__egl_main_context *egl = __egl_get_main_context();

	if ( NULL == egl ) return;

	_mali_sys_mutex_lock( egl->mutex_vsync );
	egl->vsync_busy = MALI_TRUE;
	egl->vsync_ticker = 0;
	egl->vsync_busy = MALI_FALSE;
	_mali_sys_mutex_unlock( egl->mutex_vsync );
}

