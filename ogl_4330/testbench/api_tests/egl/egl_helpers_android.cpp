/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

//#define _XOPEN_SOURCE 600 /* for setenv */
//#define _POSIX_C_SOURCE 199309L

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
#include <ump/ump.h>
#include <ump/ump_ref_drv.h>
#endif
#include <utils/Log.h>

#if MALI_ANDROID_API >= 15
#include <system/window.h>
#else
#include <ui/EGLUtils.h>
#include <ui/egl/android_natives.h>
#include <ui/android_native_buffer.h>
#endif

#if USE_TPI
#include <ui/FramebufferNativeWindow.h>
#include "tpi/mali_tpi.h"
#include "tpi/mali_tpi_egl.h"
#else 
#include "android/AndroidNativeWindow.cpp"
#endif
#include "egl_helpers.h"

/**
 * 'Invalid' native display
 */
#define INVALID_NATIVE_DPY_VALUE ((EGLNativeDisplayType)-1)

/**
 * Native dpys that should never be returned by egl_helper_get_native_display.
 * Mainly used to catch use-after-release.
 *
 * @note this is not the same as an INVALID display, which is a correct output
 * value of egl_helper_get_native_display
 */
#define BAD_NATIVE_DPY_VALUE ((EGLNativeDisplayType)-2)



#define SHELL_PROG "sh"
#define SHELL_PATH "/bin"
#define SHELL_ARG "-c"

/** Windowing abstraction state for Android, uses AndroidNativeWindow to represent the native window
 * */
struct egl_helper_windowing_state_T
{
	#ifndef EXCLUDE_UNIT_FRAMEWORK
	suite *test_suite; /**< Test suite pointer, for making memory allocations and fails/passes */
	#else
	int dummy;
	#endif

	#ifndef USE_TPI
	android::AndroidNativeWindow *win;
	#else
	android_native_window_t* win;
	#endif /* USE_TPI */
	EGLDisplay display;
};

#if EGL_HELPER_TEST_FBDEV_DUMP
#include <gralloc_priv.h>

static int ppm_save( const char *name, u8 *buffer, int w, int h )
{
	FILE *fp;
	int x, y;

	if ( NULL == name || NULL == buffer ) return 1;

	fp = fopen( name, "wt" );
	if ( NULL == fp ) return 1;

	fprintf( fp, "P3\n%d %d\n255\n", w, h );

	for ( y=0; y<h; y++ )
	{
		for ( x=0; x<w; x++ )
		{
			int ofs = (x + (h-y-1)*w)*4;
			unsigned int r, g, b;
			r = buffer[ofs+0];
			g = buffer[ofs+1];
			b = buffer[ofs+2];
			fprintf( fp, "%d %d %d ", r, g, b );
			if ( x > 0 && (0 == (x&7)) )
			{
				fprintf( fp, "\n" );
			}
		}
	}

	fclose( fp );

	return 0;
}

EGLBoolean egl_helper_fbdev_test( egl_helper_windowing_state *state )
{
	EGL_HELPER_ASSERT_POINTER( state );
	EGLBoolean success;
	const framebuffer_device_t *fb_device = state->win->getDevice();
	private_module_t* m = reinterpret_cast<private_module_t*>(fb_device->common.module);
	void* fb_vaddr;
	FILE *fp;
	u32 *data;
	
	m->base.lock(&m->base, m->currentBuffer, GRALLOC_USAGE_SW_READ_MASK, 
			0, 0, m->info.xres, m->info.yres, &fb_vaddr);
	
	LOGD("Buffer %x locked for dump", m->currentBuffer);

	/* dump buffer to ppm */
	char fname[256] = { 0 };
	sprintf(fname, "fbdev_dump_%s.ppm", state->test_suite->this_test->name);

	if( 0 != ppm_save( fname, reinterpret_cast<u8*>(fb_vaddr), fb_device->width, fb_device->height ) ){
		LOGD("Failed dumping framebuffer to ppm");
	}

#if 0
	sprintf( fname, "fbdev_dump_raw_%s", state->test_suite->this_test->name);

	data = reinterpret_cast<u32*>(fb_vaddr);
	fp = fopen( fname, "wr" );
	if( NULL == fp ) return EGL_FALSE;
	for(unsigned int i = 0; i < fb_device->height; i++){
		for(unsigned int j = 0; j < fb_device->width; j++){
			fprintf( fp, "%x", data[j] );
		}
		fprintf( fp, "\n" );
		data += fb_device->stride;
	}
	
	fclose( fp );
#endif


	m->base.unlock(&m->base, m->framebuffer); 

	return EGL_TRUE;
}

#endif


#if !defined(EXCLUDE_UNIT_FRAMEWORK) && !defined(EXCLUDE_EGL_SPECIFICS)
/**
 * Wrapper for pixmap specifier.
 *
 * Contains additional private data not required by the platform-independent
 * pixmap functions. Created by egl_helper_nativepixmap_prepare_data and
 * destroyed by egl_helper_nativepixmap_release_data.
 */
typedef struct egl_helper_prepared_pixmap_handle_t
{
	pixmap_spec ps;             /**< Platform-independent pixmap specifier. 
	                                 Must be the first member! */
	EGLNativePixmapType pixmap; /**< For egl_helper_nativepixmap_release_data */
		
}
egl_helper_prepared_pixmap_handle_t;
#endif /* !defined(EXCLUDE_UNIT_FRAMEWORK) && !defined(EXCLUDE_EGL_SPECIFICS) */ 

/**
 * Display abstraction
 */
#ifndef EXCLUDE_EGL_SPECIFICS
EGLBoolean egl_helper_get_native_display( suite *test_suite, EGLNativeDisplayType *ret_native_dpy, egl_helper_dpy_flags flags )
{
	EGLNativeDisplayType nativedpy;
	EGL_HELPER_ASSERT_POINTER( ret_native_dpy );
#if USE_TPI
	if(0 != (flags & EGL_HELPER_DPY_FLAG_INVALID_DPY))
	{
		nativedpy = INVALID_NATIVE_DPY_VALUE;
	}
	else
	{
		nativedpy = mali_tpi_egl_get_default_native_display();
	}
#else
	/* an invalid display, for testing purposes */
	if ( 0 != (flags & EGL_HELPER_DPY_FLAG_INVALID_DPY) )
	{
		nativedpy = INVALID_NATIVE_DPY_VALUE;
	}
	else
	{
		/* prepare a valid display */
		nativedpy = EGL_DEFAULT_DISPLAY;
	}
#endif

	*ret_native_dpy = nativedpy;

	return EGL_TRUE;
}

void egl_helper_release_native_display( suite *test_suite, EGLNativeDisplayType *inout_native_dpy )
{
	EGL_HELPER_ASSERT_POINTER( inout_native_dpy );
	EGL_HELPER_ASSERT_MSG( BAD_NATIVE_DPY_VALUE != *inout_native_dpy, ("egl_helper_release_native_display: Native display was not valid. Already released?") );

	/* No releasing need happen */ 

	*inout_native_dpy = (EGLNativeDisplayType)BAD_NATIVE_DPY_VALUE;
}
#endif /* EXCLUDE_EGL_SPECIFICS */


/**
 * Pixmap abstraction
 * @note - not ported to work without the unit framework
 */
#ifndef EXCLUDE_UNIT_FRAMEWORK 
#ifndef EXCLUDE_EGL_SPECIFICS

/* Android specific helper functions for creating and managing a native buffer */
static display_format egl_helper_android_buffer_get_format( android_native_buffer_t* native_buffer ){
	EGL_HELPER_ASSERT_POINTER( native_buffer );

	switch ( native_buffer->format ){
		case HAL_PIXEL_FORMAT_RGB_565:
			return DISPLAY_FORMAT_RGB565;
		case HAL_PIXEL_FORMAT_RGBA_8888:
			return DISPLAY_FORMAT_ARGB8888;
		default:
			return DISPLAY_FORMAT_INVALID;

	}
}


EGLBoolean egl_helper_nativepixmap_supported( display_format format )
{
	return EGL_FALSE;
}


EGLBoolean egl_helper_create_nativepixmap_internal( suite *test_suite, u32 width, u32 height, display_format format, EGLBoolean ump, EGLNativePixmapType* out_pixmap )
{

	/*pixmap not supported on ANDROID*/
	return EGL_FALSE;

}

// FIXME: what to do with non-ump pixmaps?
EGLBoolean egl_helper_create_nativepixmap_extended( suite *test_suite, u32 width, u32 height, display_format format, EGLint renderable_type, EGLNativePixmapType* pixmap )
{
	MALI_IGNORE( renderable_type );
	return egl_helper_create_nativepixmap_internal( test_suite, width, height, format, EGL_FALSE, pixmap );
}

EGLBoolean egl_helper_create_nativepixmap_ump_extended( suite *test_suite, u32 width, u32 height, display_format format, EGLint renderable_type, EGLNativePixmapType* pixmap )
{
	MALI_IGNORE( renderable_type );
	return egl_helper_create_nativepixmap_internal( test_suite, width, height, format, EGL_TRUE, pixmap );
}

EGLBoolean egl_helper_valid_nativepixmap( EGLNativePixmapType pixmap )
{
    return EGL_FALSE;
}

/** Zero for success, non-zero for failure */
int egl_helper_free_nativepixmap( EGLNativePixmapType pixmap )
{
	return 0;
}

u32 egl_helper_get_nativepixmap_width( EGLNativePixmapType pixmap )
{
	return 0;
}


u32 egl_helper_get_nativepixmap_height( EGLNativePixmapType pixmap )
{
	return 0;
}

u32 egl_helper_get_nativepixmap_bits_per_pixel( EGLNativePixmapType pixmap )
{
	return 0;
}

u32 egl_helper_get_nativepixmap_bytes_per_pixel( EGLNativePixmapType pixmap )
{
	return 0;
}

u32 egl_helper_get_nativepixmap_pitch( EGLNativePixmapType pixmap )
{
	return 0;
}


pixmap_spec *egl_helper_nativepixmap_prepare_data( suite *test_suite, EGLNativePixmapType pixmap )
{
	return NULL;
}

u16* egl_helper_nativepixmap_map_data( EGLNativePixmapType pixmap )
{
	return reinterpret_cast<u16*>(NULL);
}

void egl_helper_nativepixmap_unmap_data( EGLNativePixmapType pixmap )
{
	
}

void egl_helper_nativepixmap_release_data( pixmap_spec *ps )
{
	egl_helper_prepared_pixmap_handle_t *pixmap_info = reinterpret_cast<egl_helper_prepared_pixmap_handle_t*>(ps);

	EGL_HELPER_ASSERT_POINTER( pixmap_info );

	egl_helper_nativepixmap_unmap_data( pixmap_info->pixmap );
}

EGLBoolean egl_helper_nativepixmap_directly_renderable( void )
{
    EGLBoolean ret = EGL_FALSE;
    return ret;
}

#endif /* EXCLUDE_EGL_SPECIFICS */

/**
 * Process and threading abstraction
 */

/**
 * Parameter wrapper for threadproc wrapp
 */
typedef struct theadproc_param_wrapper_T
{
	egl_helper_threadproc *actual_threadproc;
	void *actual_param;
	suite * test_suite;
} threadproc_param_wrapper;

/**
 * Pthread Static wrapper for platform independant egl_helper_threadproc functions
 * A pointer to this function is of type void *(*)(void*).
 * Purpose is an example of how you would wrap it, and to ensure the type is all correct;
 */
static void* egl_helper_threadproc_wrapper( void *threadproc_params )
{
	threadproc_param_wrapper *params;
	void *retcode;

	EGL_HELPER_ASSERT_POINTER( threadproc_params );

	params = (threadproc_param_wrapper *)threadproc_params;

	retcode = (void*)params->actual_threadproc(params->actual_param);

	if ( NULL == params->test_suite )
	{
		/* we are not using test suite so we need to free allocated resources */
		FREE( params );
	}
	else
	{
		MALI_IGNORE( params->test_suite );
	}
	
	return retcode;
}

int egl_helper_create_process_simple()
{
	pid_t pid;

	pid = fork();

	/* The Parent - pid != 0, pid != -1 */
	return (int)pid;
}
u32 egl_helper_create_process( suite *test_suite, char* executable, char* args, char* log_basename )
{
	pid_t pid;

	MALI_IGNORE( test_suite );

	pid = fork();

	if ( -1 == pid )
	{
		/* Error! */
		return 0;
	}

	/* The child */
	if ( 0 == pid )
	{
		char buffer[1024]; /* Could alloc on heap instead */
		char stdout_logname[256];
		char stderr_logname[256];

		mali_tpi_snprintf( stdout_logname, sizeof(stdout_logname)/sizeof(stdout_logname[0]), "%s.out", log_basename );
		mali_tpi_snprintf( stderr_logname, sizeof(stderr_logname)/sizeof(stderr_logname[0]), "%s.err", log_basename );

		/* Concatenate executable and args into a string, separated by a space. Assume in current dir */
		mali_tpi_snprintf( buffer, sizeof(buffer)/sizeof(buffer[0]), "./%s %s > %s 2> %s", executable, args, stdout_logname, stderr_logname );
		execl( SHELL_PATH "/" SHELL_PROG, SHELL_PROG, SHELL_ARG, buffer, NULL );

		mali_tpi_printf( "Error: Child process continued after execl()!\n" );
		exit(1);
	}
	/* The Parent - pid != 0, pid != -1 */

	return (u32)pid;
}

int egl_helper_wait_for_process( suite *test_suite, u32 proc_handle )
{
	pid_t pid;
	int exitcode;

	MALI_IGNORE( test_suite );

	if ( 0 == proc_handle )
	{
		/* Bad handle */
		return -1;
	}

	pid = (pid_t)proc_handle;

	/* Wait and get exitcode */
	if ( pid != waitpid( pid, &exitcode, 0 ) )
	{
		/* Some fault or signal delivered */
		return -1;
	}

	return exitcode;
}

u32 egl_helper_create_thread( suite *test_suite, egl_helper_threadproc *threadproc, void *start_param )
{
	threadproc_param_wrapper *param_wrapper;
	pthread_t thread_handle; /* Type unsigned long int, so can use as handles without backing with memory */
	int ret;
	MALI_IGNORE( test_suite );
	EGL_HELPER_ASSERT_POINTER( threadproc );

	/* If not using mempools, we free this inside the wrapper threadproc */
	if ( NULL == test_suite )
	{
		param_wrapper = (threadproc_param_wrapper*)MALLOC( sizeof( threadproc_param_wrapper ) );
	}
	else
	{
		param_wrapper = (threadproc_param_wrapper*)_test_mempool_alloc( &test_suite->fixture_pool, sizeof( threadproc_param_wrapper ) );
	}
	if ( NULL == param_wrapper ) return 0;

	param_wrapper->actual_threadproc = threadproc;
	param_wrapper->actual_param = start_param;
	param_wrapper->test_suite = test_suite;

	ret = pthread_create( &thread_handle, NULL, egl_helper_threadproc_wrapper, param_wrapper );

	if ( 0 != ret )
	{
		/* If not using mempools we need to cleanup on failure */
		if ( NULL == test_suite )
		{
			FREE( param_wrapper );
		}
		return 0;
	}

	return (u32)thread_handle;
}

u32 egl_helper_wait_for_thread( suite *test_suite, u32 thread_handle )
{
	pthread_t pthread_handle; /* Type unsigned long int, so can use as handles without backing with memory */
	void *exitcode = NULL;

	MALI_IGNORE( test_suite );

	pthread_handle = (pthread_t)thread_handle;

	if ( 0 != pthread_join( pthread_handle, &exitcode ) )
	{
		/* Failure */
		return (u32)-1;
	}

	return (u32)exitcode;
}

u32 * egl_helper_mutex_create( suite *test_suite )
{
	pthread_mutex_t * mutex;

	MALI_IGNORE( test_suite );

	mutex = (pthread_mutex_t*)MALLOC(sizeof(pthread_mutex_t));
	if (mutex == NULL) return NULL;

	if (0 != pthread_mutex_init(mutex, NULL))
	{
		FREE(mutex);
		return NULL;
	}
	else return (u32 *)mutex;
}

void egl_helper_mutex_destroy( suite *test_suite, u32 * mutex )
{
	int call_result;

	MALI_IGNORE( test_suite );

	call_result = pthread_mutex_destroy((pthread_mutex_t *)mutex);
	if ( 0 != call_result) printf("Incorrect mutex use detected: pthread_mutex_destroy call failed with error code %d\n", call_result);
	FREE(mutex);
	mutex = NULL;
}

void egl_helper_mutex_lock( suite *test_suite, u32 * mutex )
{
	int call_result;

	MALI_IGNORE( test_suite );

	call_result = pthread_mutex_lock((pthread_mutex_t *)mutex);
	if ( 0 != call_result) printf("Incorrect mutex use detected: pthread_mutex_lock call failed with error code %d\n", call_result);
}

void egl_helper_mutex_unlock(  suite *test_suite, u32 * mutex )
{
	int call_result;

	MALI_IGNORE( test_suite );

	call_result = pthread_mutex_unlock((pthread_mutex_t *)mutex);
	if ( 0 != call_result) printf("Incorrect mutex use detected: pthread_mutex_unlock call failed with error code %d\n", call_result);
}

void egl_helper_usleep( u64 usec )
{
	while (usec > 999999ULL)
	{
		usleep(999999UL);
		usec -= 999999ULL;
	}

	usleep(usec);
}

u32 egl_helper_yield( void )
{
	int status;
	status = sched_yield();
	if (0 == status)
		return 0;
	else
		return 1;
}

u32* egl_helper_barrier_init(suite *test_suite, u32 number_of_threads )
{
	mali_tpi_barrier * barrier;
	u32* retval;
	int status;

	MALI_IGNORE( test_suite );

	barrier = (mali_tpi_barrier*)mali_tpi_alloc(sizeof(mali_tpi_barrier));
	if(barrier == NULL)
	{
		return NULL;
	}

	status = mali_tpi_barrier_init(barrier, number_of_threads);
	if (MALI_TPI_FALSE == status)
	{
		mali_tpi_printf("Incorrect barrier use detected: pthread_barrier_init call failed with error code %d\n", status);
		mali_tpi_free(barrier);
		retval = NULL;
	}
	else
	{
		retval = (u32*) barrier;
	}
	return retval;
}

void egl_helper_barrier_destroy( suite *test_suite, u32 *barrier )
{
	mali_tpi_bool status;

	MALI_IGNORE( test_suite );

	status = mali_tpi_barrier_destroy((mali_tpi_barrier*)barrier);
	if( MALI_TPI_FALSE == status)
	{
		mali_tpi_printf("Incorrect barrier use detected: pthread_barrier_destroy call failed with error code %d\n", status);
	}
	mali_tpi_free(barrier);
	barrier = NULL;
}

void egl_helper_barrier_wait( suite *test_suite, u32 *barrier )
{
	mali_tpi_bool status;
	MALI_IGNORE( test_suite );

	status = mali_tpi_barrier_wait((mali_tpi_barrier*)barrier);
	if( MALI_TPI_FALSE == status)
	{
		mali_tpi_printf("Incorrect barrier use detected: pthread_barrier_wait call failed\n");
	}
}

#endif /* EXCLUDE_UNIT_FRAMEWORK */

#ifndef EXCLUDE_EGL_SPECIFICS
/**
 * Windowing abstraction
 */
EGLBoolean egl_helper_windowing_init( egl_helper_windowing_state **state, suite *test_suite )
{
	EGLBoolean init = EGL_FALSE;
	EGLint major, minor;
	egl_helper_windowing_state *windowing_state;

	EGL_HELPER_ASSERT_POINTER( state );

#ifndef EXCLUDE_UNIT_FRAMEWORK
	if ( NULL != test_suite )
	{
#if USE_TPI
	    mali_tpi_init();
		mali_tpi_egl_init();
#endif
		if ( NULL != test_suite->fixture_pool.last_block )
		{
			windowing_state =( egl_helper_windowing_state*) _test_mempool_alloc( &test_suite->fixture_pool, sizeof( egl_helper_windowing_state ) );
		}
		else
		{
			/** @note this required for vg_api_tests, which don't set up the fixture pool until after EGL is setup */
			windowing_state =( egl_helper_windowing_state*) _test_mempool_alloc( test_suite->parent_pool, sizeof( egl_helper_windowing_state ) );
		}

		if ( NULL == windowing_state)
		{
			return EGL_FALSE;
		}
		windowing_state->test_suite = test_suite;
	}
	else
#endif
	{
		MALI_IGNORE( test_suite );
		windowing_state = (egl_helper_windowing_state*)MALLOC( sizeof( egl_helper_windowing_state ) );
		if ( NULL == windowing_state )
		{
			return EGL_FALSE;
		}
		MEMSET( windowing_state, 0, sizeof( egl_helper_windowing_state ) );
	}
	/* Add any other necessary state initialization here */
	
	windowing_state->win = NULL;

	/* Success - writeout */
	*state = windowing_state;

	return EGL_TRUE;
}

EGLBoolean egl_helper_windowing_deinit( egl_helper_windowing_state **state )
{
	egl_helper_windowing_state *windowing_state;
	EGL_HELPER_ASSERT_POINTER( state );

	windowing_state = *state;
	EGL_HELPER_ASSERT_POINTER( windowing_state );
	
	/* Add any necessary windowing state de-init here */

#ifndef EXCLUDE_UNIT_FRAMEWORK
#if USE_TPI
	mali_tpi_egl_shutdown();
	mali_tpi_shutdown();
#endif
	if ( NULL != windowing_state->test_suite )
	{
		/* No need to free windowing_state, since it is part of the fixture's memory pool */
		MALI_IGNORE( windowing_state );
	} 
	else
#endif
	{
		FREE( windowing_state );
	}

	/* Zero the state pointer */
	*state = NULL;

	return EGL_TRUE;
}

EGLBoolean egl_helper_windowing_set_quadrant( egl_helper_windowing_state *state, int quadrant )
{
	return EGL_FALSE;
}

EGLBoolean egl_helper_create_window( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType *win )
{
	EGLBoolean success = EGL_FALSE;
	EGLNativeWindowType window;
	
	EGL_HELPER_ASSERT_POINTER( state );
	
	suite* test_suite = state->test_suite;

#if USE_TPI
	EGLDisplay display;
	EGLint attrib_list[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE};
	EGLint num_config;
	EGLConfig config= 0;
	EGLConfig *configs;
	int i;
	EGLNativeDisplayType native_display;
	native_display = mali_tpi_egl_get_default_native_display();
	display = eglGetDisplay(native_display);
	state->display = display;
	eglInitialize(display, NULL, NULL);
	
	(void) eglChooseConfig(display, attrib_list, NULL, 0, &num_config);
	configs = (EGLConfig *)malloc(sizeof(EGLConfig) * num_config);
	if(configs == NULL)
	{
		return success;
	}
	(void) eglChooseConfig(display, attrib_list, configs, num_config, &num_config);

	for(i=0; i<num_config; i++)
	{
		EGLint eglId;

		eglGetConfigAttrib(display, configs[i], EGL_NATIVE_VISUAL_ID, &eglId);
		if(eglId == HAL_PIXEL_FORMAT_RGBA_8888)
		{
			config = configs[i];
			break;
		}
	}

	if (i > num_config)
	{
		free(configs);
		return success;
	}

	mali_tpi_egl_create_window(native_display, width, height, display, config, &window);

	free(configs);

	if (NULL == state->win)
		state->win = reinterpret_cast<android_native_window_t*>(window);
#else
	/* If a window has not already been created in current fixture, allocate with gralloc and open fbdev. */
	if ( NULL == state->win ){
		window = android_createDisplaySurface(width, height, true);		
		state->win = reinterpret_cast<android::AndroidNativeWindow*>(window);
	}
	/* Else, just allocate a buffer with gralloc */
	else{
		window = android_createDisplaySurface(width, height, false);
	}
#endif
	if ( NULL != window ){
		success = EGL_TRUE;
	}
	
	*win = window;
	return success;
}

#ifndef USE_TPI
EGLBoolean egl_helper_create_window_outside_framebuffer( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType *win )
{
	EGLBoolean success = EGL_FALSE;
	EGLNativeWindowType window;

	EGL_HELPER_ASSERT_POINTER( state );

	if ( NULL == state->win )
	{
		window = android_createDisplaySurface(width, height, false);
		state->win = reinterpret_cast<android::AndroidNativeWindow*>(window);
	}
	else
	{
		window = android_createDisplaySurface(width, height, false);
	}
	if ( NULL != window )
	{
		success = EGL_TRUE;
	}

	*win = window;
	return success;
}
#endif /* USE_TPI */

EGLBoolean egl_helper_destroy_window( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	EGL_HELPER_ASSERT_POINTER( state );

	/* If egl_helper_create_window failed, then win will be NULL, and we should
	 * not report any further error. */

	if ( NULL != win )
	{
#if USE_TPI
		mali_tpi_egl_destroy_window(mali_tpi_egl_get_default_native_display(), win);
#else
		android_destroyDisplaySurface(win);
#endif
	}
	state->win = NULL;

	return EGL_TRUE;
}

// Stuff not implemented
EGLBoolean egl_helper_create_invalid_window( egl_helper_windowing_state *state, EGLNativeWindowType *win )
{
	EGLBoolean success = EGL_FALSE;
#if USE_TPI
	success = EGL_TRUE;
	EGLDisplay display;
	EGLNativeWindowType window;
	EGLint attrib_list[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE};
	EGLint num_config;
	EGLConfig *configs;
	android_native_window_t *native_window;
	display = eglGetDisplay(mali_tpi_egl_get_default_native_display());
	eglInitialize(display, NULL, NULL);
	(void) eglChooseConfig(display, attrib_list, NULL, 0, &num_config);
	configs = (EGLConfig *)malloc(sizeof(EGLConfig) * num_config);
	if(configs==NULL)
	{
		return success;
	}
	(void) eglChooseConfig(display, attrib_list, configs, num_config, &num_config);
	mali_tpi_egl_create_window(mali_tpi_egl_get_default_native_display(), 0, 0, display, configs[0], &window);
	native_window = reinterpret_cast<android_native_window_t*>(window);
	native_window->common.magic = native_window->common.magic - 1;
	*win = window;
	free(configs);
#endif
	return success;
}

EGLBoolean egl_helper_invalidate_window( egl_helper_windowing_state *state, EGLNativeWindowType win )
{	
	return EGL_FALSE;
}

u32 egl_helper_get_window_width( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	android_native_window_t* window = reinterpret_cast<android_native_window_t *>(win);
	int width;
	int height;
#if USE_TPI
	mali_tpi_egl_get_window_dimensions(mali_tpi_egl_get_default_native_display(), win, &width,&height);
#else
	window->query( reinterpret_cast<android_native_window_t*>(win), NATIVE_WINDOW_WIDTH, &width);
#endif
	return width;
}

u32 egl_helper_get_window_height( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	android_native_window_t* window = reinterpret_cast<android_native_window_t*>(win);
	int width;
	int height;
#if USE_TPI
	mali_tpi_egl_get_window_dimensions(mali_tpi_egl_get_default_native_display(), win, &width,&height);
#else
	window->query( reinterpret_cast<android_native_window_t*>(win), NATIVE_WINDOW_HEIGHT, &height);
#endif
	return height;
}

EGLBoolean egl_helper_resize_window( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType win )
{
	EGLBoolean success = EGL_FALSE;
#if USE_TPI
	mali_tpi_bool status;
	EGLint win_format;
	android_native_window_t *window = MALI_REINTERPRET_CAST(android_native_window_t *)win;
	
	window->query( window, NATIVE_WINDOW_FORMAT, &win_format);

	EGLDisplay display;
	EGLint attrib_list[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE};
	EGLint num_config;
	EGLConfig config= 0;
	EGLConfig *configs;
	int i;
	
	display = eglGetDisplay(mali_tpi_egl_get_default_native_display());
	(void) eglChooseConfig(display, attrib_list, NULL, 0, &num_config);
	configs = (EGLConfig *)malloc(sizeof(EGLConfig) * num_config);
	if( configs==NULL )
	{
		return success;
	}
	(void) eglChooseConfig(display, attrib_list, configs, num_config, &num_config);

	for(i=0; i<num_config; i++)
	{
		EGLint eglId;
		eglGetConfigAttrib(display, configs[i], EGL_NATIVE_VISUAL_ID, &eglId);
		if(eglId == win_format)
		{
			config = configs[i];
			break;
		}
	}

	if (i > num_config)
	{
		free(configs);
		return success;
	}
	status = mali_tpi_egl_resize_window(mali_tpi_egl_get_default_native_display(), &win, width, height, display, config);
	
	if(status) success = EGL_TRUE;

	free(configs);
#endif

	return success;
}
#endif /* EXCLUDE_EGL_SPECIFICS */

FILE *egl_helper_fopen(const char *filename, const char *mode)
{
	return fopen( filename, mode );
}

u32 egl_helper_fread(void *buf, u32 size, u32 count, FILE *fp)
{
	return fread(buf, size, count, fp);
}

/* NOTE: egl_helper_physical_memory_get/release are solely provided for the mf14 memory test in the base_api_main_suite
 *       they are not meant for any other purpose at this time.
 */
u32 egl_helper_physical_memory_get( suite *test_suite, void **mapped_pointer, u32 size, u32 *phys_addr )
{
    return 0; /* NOT IMPLEMENTED */
}

/* NOTE: egl_helper_physical_memory_get/release are solely provided for the mf14 memory test in the base_api_main_suite
 *       they are not meant for any other purpose at this time.
 */
void egl_helper_physical_memory_release( suite *test_suite, void *mapped_pointer, u32 size, u32 cookie )
{
    EGL_HELPER_ASSERT_POINTER(test_suite);
    EGL_HELPER_ASSERT_POINTER(mapped_pointer);
    MALI_IGNORE(cookie);
	munmap(mapped_pointer, size);
}

unsigned long long egl_helper_get_ms(void)
{
	struct timeval tval;
	gettimeofday(&tval,NULL);
	return tval.tv_sec*1000000 + tval.tv_usec;
}

int egl_helper_initialize()
{
    return 0;
}

void egl_helper_terminate()
{
}
void egl_helper_android_set_display(egl_helper_windowing_state *state, EGLDisplay display)
{
	state->display = display;
}
void egl_helper_exit_process()
{
	exit(0);
}
