/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#define _XOPEN_SOURCE 600 /* for setenv */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#elif _POSIX_C_SOURCE < 200112L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <pthread.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sched.h>
#undef _GNU_SOURCE
#undef __USE_GNU


#include "egl_helpers.h"
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define X_ERROR_FLUSH 1
#define X_ERROR_SYNC  2

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

/** Windowing abstraction state for FBDev
 * @note at present, this requires nothing other than the test_suite itself */
struct egl_helper_windowing_state_T
{
	#ifndef EXCLUDE_UNIT_FRAMEWORK
	suite *test_suite; /**< Test suite pointer, for making memory allocations and fails/passes */
	#else
	int dummy;
	#endif
	Display *dpy;
};

static XErrorHandler x_error_handler_old = (XErrorHandler)0;
static EGLBoolean error_set;
static Display *x_display;

typedef struct egl_helper_pixmap_attachment_t
{
	Pixmap pixmap;
	XImage *image;
} egl_helper_pixmap_attachment_t;

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
	XImage *image;
	unsigned int width;
	unsigned int height;
}
egl_helper_prepared_pixmap_handle_t;
#endif /* !defined(EXCLUDE_UNIT_FRAMEWORK) && !defined(EXCLUDE_EGL_SPECIFICS) */ 

void egl_helper_open_x_display()
{
	if ( NULL == x_display ) x_display = XOpenDisplay( NULL );
}

void egl_helper_close_x_display()
{
	if ( NULL != x_display )
	{
		XCloseDisplay( x_display );
		x_display = NULL;
	}
}


/**
 * Display abstraction
 */
#ifndef EXCLUDE_EGL_SPECIFICS
EGLBoolean egl_helper_get_native_display( suite *test_suite, EGLNativeDisplayType *ret_native_dpy, egl_helper_dpy_flags flags )
{
	EGLNativeDisplayType nativedpy;
	EGL_HELPER_ASSERT_POINTER( ret_native_dpy );

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
 * This error handler is needed to avoid X11 from simply crashing the application in case it is using an
 * invalid native resource.
 */
static int x_error_handler( Display *display, XErrorEvent *event )
{
	/*printf( "[EGL Helper] Xlib error: code %d request %d\n", event->error_code, event->request_code );*/
	error_set = EGL_TRUE;

	return 0;
}

/**
 * Initializes the error handler.
 * Should be called before any X interaction 
 */
void x_init_error_handler()
{
	error_set = EGL_FALSE;
	egl_helper_open_x_display();
	x_error_handler_old = XSetErrorHandler( x_error_handler );
}

/**
 * Deinitializes the error handler.
 * Should be called when the sequence of X calls are done.
 * Optionally flushes and syncs the X server
 */
void x_deinit_error_handler( Display *display, int flags )
{
	if ( flags & X_ERROR_FLUSH ) XFlush( display );
	if ( flags & X_ERROR_SYNC ) XSync( display, False );

	(void)XSetErrorHandler( x_error_handler_old );
}


/**
 * Pixmap abstraction
 * @note - not ported to work without the unit framework
 */
#ifndef EXCLUDE_UNIT_FRAMEWORK
#ifndef EXCLUDE_EGL_SPECIFICS

EGLBoolean egl_helper_nativepixmap_supported( display_format format )
{
	switch( format )
	{
		case DISPLAY_FORMAT_RGB565:
		case DISPLAY_FORMAT_ARGB8888:
		case DISPLAY_FORMAT_A8:
			return EGL_TRUE;

		default:
			break;
	}

	return EGL_FALSE;
}

EGLBoolean egl_helper_create_nativepixmap_internal( suite *test_suite, u32 width, u32 height, display_format format, EGLBoolean ump, EGLNativePixmapType* out_pixmap )
{
	Pixmap pixmap;
	int screen;
	int depth = 0;
	
	switch (format)
	{
		case DISPLAY_FORMAT_RGB565: depth = 16; break;
		case DISPLAY_FORMAT_ARGB1555: depth = 16; break;
		case DISPLAY_FORMAT_ARGB4444: depth = 16; break;
		case DISPLAY_FORMAT_ARGB8888: depth = 32; break;
		case DISPLAY_FORMAT_L8: depth = 8; break;
		case DISPLAY_FORMAT_AL88: depth = 16; break;
		case DISPLAY_FORMAT_A8: depth = 8; break;

		/* for bogus formats, we set depth as 0 */
		case DISPLAY_FORMAT_ARGB8885:
		case DISPLAY_FORMAT_ARGB8880:
		case DISPLAY_FORMAT_ARGB8808:
		case DISPLAY_FORMAT_ARGB8088:
		case DISPLAY_FORMAT_ARGB0888:
		case DISPLAY_FORMAT_ARGB0000:
		case DISPLAY_FORMAT_RGB560:
		case DISPLAY_FORMAT_RGB505:
		case DISPLAY_FORMAT_RGB065:
		case DISPLAY_FORMAT_RGB000:
			depth = 0;
			break;

		default: return EGL_FALSE;
	}

	x_init_error_handler();
	screen = DefaultScreen( x_display );
	pixmap = XCreatePixmap( x_display, DefaultRootWindow(x_display), width, height, depth);
	XFlush( x_display );
	XSync( x_display, False );
	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	*out_pixmap = pixmap;
	return EGL_TRUE;

}

EGLBoolean egl_helper_create_nativepixmap_extended( suite *test_suite, u32 width, u32 height, display_format format, EGLint renderable_type, EGLNativePixmapType* out_pixmap )
{
	MALI_IGNORE( renderable_type );
	return egl_helper_create_nativepixmap_internal( test_suite, width, height, format, EGL_FALSE, out_pixmap );
}

EGLBoolean egl_helper_create_nativepixmap_ump_extended( suite *test_suite, u32 width, u32 height, display_format format, EGLint renderable_type, EGLNativePixmapType* out_pixmap )
{
	MALI_IGNORE( renderable_type );
	return egl_helper_create_nativepixmap_internal( test_suite, width, height, format, EGL_TRUE, out_pixmap );
}

EGLBoolean egl_helper_valid_nativepixmap( EGLNativePixmapType pixmap )
{
    return (void*)pixmap != NULL ? EGL_TRUE : EGL_FALSE;
}

/** Zero for success, non-zero for failure */
int egl_helper_free_nativepixmap( EGLNativePixmapType pixmap )
{
	if ( NULL == (void *)pixmap ) return 0;

	x_init_error_handler();
	XFreePixmap( x_display, pixmap );
	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	return 0;
}

u32 egl_helper_get_nativepixmap_width( EGLNativePixmapType pixmap )
{
	Window root_window;
	Drawable drawable;
	int x, y;
	unsigned int width, height;
	unsigned int border_width;
	unsigned int depth;

	x_init_error_handler();
	drawable = (Drawable)pixmap;
	XGetGeometry( x_display, pixmap, &root_window, &x, &y, &width, &height, &border_width, &depth );
	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	return width;
}


u32 egl_helper_get_nativepixmap_height( EGLNativePixmapType pixmap )
{
	Window root_window;
	Drawable drawable;
	int x, y;
	unsigned int width, height;
	unsigned int border_width;
	unsigned int depth;

	x_init_error_handler();
	drawable = (Drawable)pixmap;
	XFlush( x_display );
	XGetGeometry( x_display, pixmap, &root_window, &x, &y, &width, &height, &border_width, &depth );
	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	return height;
}

u32 egl_helper_get_nativepixmap_bits_per_pixel( EGLNativePixmapType pixmap )
{
	Window root_window;
	Drawable drawable;
	int x, y;
	unsigned int width, height;
	unsigned int border_width;
	unsigned int depth;

	x_init_error_handler();
	drawable = (Drawable)pixmap;
	XFlush( x_display );
	XGetGeometry( x_display, pixmap, &root_window, &x, &y, &width, &height, &border_width, &depth );
	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	return depth;
}

u32 egl_helper_get_nativepixmap_bytes_per_pixel( EGLNativePixmapType pixmap )
{
	return egl_helper_get_nativepixmap_bits_per_pixel( pixmap ) / 8;
}

u32 egl_helper_get_nativepixmap_pitch( EGLNativePixmapType pixmap )
{
	return egl_helper_get_nativepixmap_bytes_per_pixel( pixmap ) * egl_helper_get_nativepixmap_width( pixmap );
}


pixmap_spec *egl_helper_nativepixmap_prepare_data( suite *test_suite, EGLNativePixmapType pixmap )
{
	egl_helper_prepared_pixmap_handle_t *pixmap_info = NULL;
	Pixmap xpixmap = MALI_REINTERPRET_CAST(Pixmap)pixmap;
	XImage *image;
	display_format format = DISPLAY_FORMAT_INVALID;
	Window root_window;
	Drawable drawable;
	int x_return, y_return;
	unsigned int width_return, height_return;
	unsigned int border_width_return;
	unsigned int depth_return;
	u32 red_size = 0, green_size = 0, blue_size = 0, alpha_size = 0, luminance_size = 0;
	egl_helper_pixmap_attachment_t *attachment = NULL;

	EGL_HELPER_ASSERT_POINTER( test_suite );
	EGL_HELPER_ASSERT_POINTER( (void*)xpixmap );

	x_init_error_handler();

	drawable = (Drawable)pixmap;
	XFlush( x_display );

	XGetGeometry( x_display, pixmap, &root_window, &x_return, &y_return, &width_return, &height_return, &border_width_return, &depth_return );

	switch( depth_return )
	{
		case 32:
			red_size = green_size = blue_size = alpha_size = 8;
			luminance_size = 0;
			break;

		case 24:
			red_size = green_size = blue_size = alpha_size = 8;
			luminance_size = 0;
			break;

		case 16:
			red_size = 5; green_size = 6; blue_size = 5; alpha_size = 0;
			luminance_size = 0;
			break;

		case 8:
			red_size = green_size = blue_size = 0; alpha_size = 8;
			luminance_size = 0;
			break;
	}

	format =  egl_helper_get_display_format( red_size, green_size, blue_size, alpha_size, luminance_size );
	
	if (DISPLAY_FORMAT_INVALID != format)
	{
		image = XGetImage( x_display, pixmap, 0, 0, width_return, height_return, AllPlanes, ZPixmap );
		if ( NULL != image && NULL != image->data )
		{
			pixmap_info = _test_mempool_alloc( &test_suite->fixture_pool, sizeof(*pixmap_info) );
			if ( NULL != pixmap_info )
			{
				/* Initialise pixmap specifier for platform-independent functions */
				pixmap_info->ps.test_suite = test_suite;
				pixmap_info->ps.format = format;
				pixmap_info->ps.pixels = (void *)image->data;
				pixmap_info->ps.pitch = width_return * (depth_return/8);
				pixmap_info->ps.width = width_return;
				pixmap_info->ps.height = height_return;
				pixmap_info->ps.alpha = EGL_TRUE; /* support non-maximal alpha values */
				attachment = _test_mempool_alloc( &test_suite->fixture_pool, sizeof(*attachment) );
				attachment->pixmap = pixmap;
				attachment->image = image;
				pixmap_info->ps.attachment = attachment;

				/* Store pixmap handle for egl_helper_nativepixmap_release_data */
				pixmap_info->pixmap = pixmap;
				pixmap_info->image = image;
			}
			else
			{
				printf("WARNING: pixmap_info struct is NULL in %s : %i\n", __func__, __LINE__);
			}
		}
		else
		{
			printf("WARNING: XImage is NULL in %s : %i\n", __func__, __LINE__);
		}
	}
	else
	{
		printf("WARNING: invalid format in %s : %i\n", __func__, __LINE__);
	}
	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	/* Return a pointer to the platform-independent pixmap specifier,
	   or NULL on failure */
	return NULL == pixmap_info ? NULL : &pixmap_info->ps;
}

/** WARNING: do not use this functionality for X11
 * You will depend on having to call XPutImage for the pixmap to get updated afterwards 
 */
u16* egl_helper_nativepixmap_map_data( EGLNativePixmapType pixmap )
{
	XImage *image;
	Window root_window;
	Drawable drawable;
	int x, y;
	unsigned int width, height;
	unsigned int border_width;
	unsigned int depth;

	x_init_error_handler();

	drawable = (Drawable)pixmap;
	XFlush( x_display );
	XGetGeometry( x_display, pixmap, &root_window, &x, &y, &width, &height, &border_width, &depth );
	image = XGetImage( x_display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap );

	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	return (u16*)image->data;
}

void egl_helper_nativepixmap_unmap_data( EGLNativePixmapType pixmap )
{
	XFlush( x_display );
	XSync( x_display, False );
}

void egl_helper_nativepixmap_release_data( pixmap_spec *ps )
{
	GC gc;
	egl_helper_pixmap_attachment_t *attachment = NULL;

	attachment = (egl_helper_pixmap_attachment_t *)ps->attachment;

	x_init_error_handler();

   gc = XCreateGC( x_display, attachment->pixmap, (unsigned long)0, (XGCValues *) 0 );
	XPutImage( x_display, attachment->pixmap, gc, attachment->image, 0, 0, 0, 0, ps->width, ps->height );
	XFlush( x_display );
	XFreeGC( x_display, gc );

	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );
}

EGLBoolean egl_helper_nativepixmap_directly_renderable( void )
{
	return EGL_TRUE;
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

	/* Would free the threadproc_params here, but suite mempool does this for us */

	return retcode;
}

int egl_helper_create_process_simple( )
{
	return -1;
}

void egl_helper_exit_process()
{

}

int egl_helper_create_msgq( int key, int flags )
{
	return -1;
}

int egl_helper_msgsnd(int msgq_id, const void *from, u32 size, int flags)
{
	return -1;
}

u32 egl_helper_msgrcv(int msgq_id, void *to, u32 size, long msg_id, int flags)
{
	return -1;
}

u32 egl_helper_create_process( suite *test_suite, char* executable, char* args, char* log_basename )
{
	pid_t pid;

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
	EGL_HELPER_ASSERT_POINTER( test_suite );
	EGL_HELPER_ASSERT_POINTER( threadproc );

	/* If not using mempools, would free this inside the wrapper threadproc */
	param_wrapper = (threadproc_param_wrapper*)_test_mempool_alloc( &test_suite->fixture_pool, sizeof( threadproc_param_wrapper ) );

	param_wrapper->actual_threadproc = threadproc;
	param_wrapper->actual_param = start_param;

	ret = pthread_create( &thread_handle, NULL, egl_helper_threadproc_wrapper, param_wrapper );

	if ( 0 != ret )
	{
		/* Failure - would free the param_wrapper here if not using mempools */
		return 0;
	}

	return (u32)thread_handle;
}

u32 egl_helper_wait_for_thread( suite *test_suite, u32 thread_handle )
{
	pthread_t pthread_handle; /* Type unsigned long int, so can use as handles without backing with memory */
	void *exitcode = NULL;

	EGL_HELPER_ASSERT_POINTER( test_suite );

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
	mutex = mali_tpi_alloc(sizeof(pthread_mutex_t));
	if(mutex == NULL) return NULL;

	if (0 != pthread_mutex_init(mutex, NULL))
	{
		mali_tpi_free(mutex);
		return NULL;
	}
	else return (u32 *)mutex;
}

void egl_helper_mutex_destroy( suite *test_suite, u32 * mutex )
{
	int call_result;
	call_result = pthread_mutex_destroy((pthread_mutex_t *)mutex);
	if( 0 != call_result) printf("Incorrect mutex use detected: pthread_mutex_destroy call failed with error code %d\n", call_result);
	mali_tpi_free(mutex);
	mutex = NULL;
}

void egl_helper_mutex_lock( suite *test_suite, u32 * mutex )
{
	int call_result;
	call_result = pthread_mutex_lock((pthread_mutex_t *)mutex);
	if( 0 != call_result) printf("Incorrect mutex use detected: pthread_mutex_lock call failed with error code %d\n", call_result);
}

void egl_helper_mutex_unlock(  suite *test_suite, u32 * mutex )
{
	int call_result;
	call_result = pthread_mutex_unlock((pthread_mutex_t *)mutex);
	if( 0 != call_result) printf("Incorrect mutex use detected: pthread_mutex_unlock call failed with error code %d\n", call_result);
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

/* Wrap some simple mutex and condition functions */

u32* egl_helper_barrier_init(suite *test_suite, u32 number_of_threads )
{
	pthread_barrier_t * barrier;
	u32* retval;
	int status;

	barrier = mali_tpi_alloc(sizeof(pthread_barrier_t));
	if(barrier == NULL) return NULL;
	status = pthread_barrier_init(barrier, NULL, number_of_threads);

	if (0 != status)
	{
		printf("Incorrect barrier use detected: pthread_barrier_init call failed with error code %d\n", status);
		mali_tpi_free(barrier);
		retval = NULL;
	} else
	{
		retval = (u32*) barrier;
	}
	return retval;
}

void egl_helper_barrier_destroy( suite *test_suite, u32 * barrier )
{
	int status;

	status = pthread_barrier_destroy((pthread_barrier_t *)barrier);
	if( 0 != status) printf("Incorrect barrier use detected: pthread_barrier_destroy call failed with error code %d\n", status);
	mali_tpi_free(barrier);
	barrier = NULL;
}

void egl_helper_barrier_wait( suite *test_suite, u32*barrier )
{
	int status;

	status = pthread_barrier_wait((pthread_barrier_t *)barrier);
	if( 0 != status && PTHREAD_BARRIER_SERIAL_THREAD != status)
		printf("Incorrect barrier use detected: pthread_barrier_wait call failed with error code %d\n", status);
}

#endif /* EXCLUDE_UNIT_FRAMEWORK */

#ifndef EXCLUDE_EGL_SPECIFICS
/**
 * Windowing abstraction
 */
EGLBoolean egl_helper_windowing_init( egl_helper_windowing_state **state, suite *test_suite )
{
	egl_helper_windowing_state *windowing_state;

	EGL_HELPER_ASSERT_POINTER( state );

#ifndef EXCLUDE_UNIT_FRAMEWORK
	EGL_HELPER_ASSERT_POINTER( test_suite );

	if( NULL != test_suite->fixture_pool.last_block )
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
#else
	MALI_IGNORE( test_suite );
	windowing_state = malloc( sizeof( egl_helper_windowing_state ) );
	if ( NULL == windowing_state )
	{
		return EGL_FALSE;
	}
#endif

	/* Add any other necessary state initialization here */
	windowing_state->dpy = XOpenDisplay(NULL);
	x_display = windowing_state->dpy;

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
	/* No need to free windowing_state, since it is part of the fixture's memory pool */
	MALI_IGNORE( windowing_state );
#else
	free( windowing_state );
#endif

	/* Zero the state pointer */
	*state = NULL;

	return EGL_TRUE;
}

EGLBoolean egl_helper_windowing_set_quadrant( egl_helper_windowing_state *state, int quadrant )
{
	/* TODO: move the window instead */
	return EGL_TRUE;
}

EGLBoolean egl_helper_create_window( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType *win )
{
	Window xwin;
	Display *dpy;
	int XResult = BadImplementation;
	XWindowAttributes win_attr;
	char title[256];
	char fname[256];
	pid_t pid;
	FILE *fp;

	pid = getpid();
	snprintf( fname, 256, "/proc/%i/cmdline", pid );
	fp = fopen( fname, "rt" );
	if ( NULL == fp ) return EGL_FALSE;
	fgets( title, 256, fp );
	fclose( fp );

	x_init_error_handler();

	dpy = x_display;
	state->dpy = dpy;

	xwin = XCreateSimpleWindow( dpy, DefaultRootWindow(dpy), 0, 0, width, height, 0, BlackPixel(dpy, DefaultScreen(dpy)), BlackPixel(dpy, DefaultScreen(dpy)) );

	/* uncomment to take over the desktop */
	/*xwin = DefaultRootWindow(dpy);*/

	if ( NULL == (void*)xwin ) 
	{
		x_deinit_error_handler( dpy, X_ERROR_FLUSH | X_ERROR_SYNC );
		return EGL_FALSE;
	}

	XStoreName( dpy, xwin, title );
	XMoveResizeWindow( dpy, xwin, 10, 10, width, height );
	XSelectInput( dpy, xwin, KeyPressMask | ButtonMotionMask | ExposureMask | StructureNotifyMask );

	XResult = XGetWindowAttributes( dpy, xwin, &win_attr );
	if (!XResult )
	{
		x_deinit_error_handler( dpy, X_ERROR_FLUSH | X_ERROR_SYNC );
		return EGL_FALSE;
	}

	XMapRaised( dpy, xwin );
	XFlush( dpy );

	*win = xwin;

	x_deinit_error_handler( dpy, X_ERROR_FLUSH | X_ERROR_SYNC );
	if ( EGL_TRUE == error_set ) return EGL_FALSE;

	return EGL_TRUE;
}

EGLBoolean egl_helper_create_window_advanced( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType *win )
{
	Window xwindow, root_xwindow;
	Display *xdisplay = NULL;
	Visual *visual = NULL;
	XSetWindowAttributes window_attributes;
	XSizeHints window_sizehints;
	int depth;
	int screen;
	int screen_width, screen_height;
	int xpos = 10;
	int ypos = 10;

	char title[256];
	char fname[256];
	pid_t pid;
	FILE *fp;

	pid = getpid();
	snprintf( fname, 256, "/proc/%i/cmdline", pid );
	fp = fopen( fname, "rt" );
	if ( NULL == fp ) return EGL_FALSE;
	fgets( title, 256, fp );
	fclose( fp );


	EGL_HELPER_ASSERT_POINTER( state );

	srand(time(NULL));

	xpos += rand() % 100;
	ypos += rand() % 100;

	x_init_error_handler();

	XInitThreads();

	xdisplay = XOpenDisplay( NULL );
	state->dpy = xdisplay;

	screen = DefaultScreen( xdisplay );
	screen_width = DisplayWidth( xdisplay, screen );
	screen_height = DisplayHeight( xdisplay, screen );

	depth = DefaultDepth( xdisplay, screen );
	root_xwindow = DefaultRootWindow( xdisplay );
	visual = DefaultVisual( xdisplay, screen );

	window_attributes.border_pixel = WhitePixel( xdisplay, screen );
	window_attributes.background_pixel = BlackPixel( xdisplay, screen );
	window_attributes.backing_store = NotUseful;

	xwindow = XCreateWindow( xdisplay, root_xwindow, 0, 0, width, height, 2, depth, InputOutput,
	                         visual, CWBackPixel | CWBorderPixel | CWBackingStore, &window_attributes );

	XStoreName( xdisplay, xwindow, title );

	/* Tell the server to report only keypress-related events */
	XSelectInput( xdisplay, xwindow, KeyPressMask | KeyReleaseMask );

	/* Initialize window's sizehint definition structure */
	window_sizehints.flags = PPosition | PMinSize | PMaxSize;
	window_sizehints.x = 0;
	window_sizehints.y = 0;
	window_sizehints.min_width = 0;
	window_sizehints.max_width = screen_width;
	window_sizehints.min_height = 0;
	window_sizehints.max_height = screen_height;

	/* Set the window's sizehint */
	XSetWMNormalHints( xdisplay, xwindow, &window_sizehints );

	/* Clear the window */
	XClearWindow( xdisplay, xwindow );

	/* Put the window on top of the others */
	XMapRaised( xdisplay, xwindow );

	XMoveWindow( xdisplay, xwindow, xpos, ypos );

	/* Clear event queue */
	XFlush( xdisplay );

	x_deinit_error_handler( xdisplay, X_ERROR_FLUSH | X_ERROR_SYNC );

	if ( EGL_TRUE == error_set ) return EGL_FALSE;


	*win = xwindow; /* No cast necessary - EGLNativeWindowType must be an fbdev_window here */

	return EGL_TRUE;
}

EGLBoolean egl_helper_destroy_window( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	EGL_HELPER_ASSERT_POINTER( state );

	/* state not used at present, but presented for future expansion */
	MALI_IGNORE( state );

	if ( NULL == (void*)win ) return EGL_TRUE;
	if ( (int)win == -1 ) return EGL_TRUE;

	x_init_error_handler();

	XUnmapWindow( x_display, win );
	XFlush( x_display );

	XDestroyWindow( x_display, win );
	XFlush( x_display );

	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	return EGL_TRUE;
}

EGLBoolean egl_helper_create_invalid_window( egl_helper_windowing_state *state, EGLNativeWindowType *win )
{
	EGLBoolean success = EGL_TRUE;

	*win = -1;

	return success;
}

EGLBoolean egl_helper_invalidate_window( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	EGL_HELPER_ASSERT_POINTER( state );

	/* invalidate the window by removing the references to this window in X */
	x_init_error_handler();

	XUnmapWindow( x_display, win );
	XFlush( x_display );

	XDestroyWindow( x_display, win );
	XFlush( x_display );

	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );


	return EGL_TRUE;
}

u32 egl_helper_get_window_width( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	XWindowAttributes window_attributes;
	x_init_error_handler();

	EGL_HELPER_ASSERT_POINTER( state );
	if ( NULL == (void*)win || -1 == (int)win ) 
	{
		x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );
		return 0;
	}

	XGetWindowAttributes( x_display, (Window)win, &window_attributes );
	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	return window_attributes.width;
}

u32 egl_helper_get_window_height( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	XWindowAttributes window_attributes;
	x_init_error_handler();

	EGL_HELPER_ASSERT_POINTER( state );

	if ( NULL == (void*)win || -1 == (int)win ) 
	{
		x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );
		return 0;
	}

	XGetWindowAttributes( x_display, (Window)win, &window_attributes );
	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	return window_attributes.height;
}

EGLBoolean egl_helper_resize_window( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType win )
{
	EGLBoolean success = EGL_TRUE;
	x_init_error_handler();

	EGL_HELPER_ASSERT_POINTER( state );

	XResizeWindow( x_display, (Window)win, width, height );
	XFlush( x_display );

	x_deinit_error_handler( x_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	return success;
}
#endif /* EXCLUDE_EGL_SPECIFICS */

FILE *egl_helper_fopen(const char *filename, const char *mode)
{
	return fopen( filename, mode );
}

/* NOTE: egl_helper_physical_memory_get/release are solely provided for the mf14 memory test in the base_api_main_suite
 *       they are not meant for any other purpose at this time.
 */
u32 egl_helper_physical_memory_get( suite *test_suite, void **mapped_pointer, u32 size, u32 *phys_addr )
{
	s32 fb_dev;
	void * mapped_buffer = NULL;

    EGL_HELPER_ASSERT_POINTER(test_suite);
    EGL_HELPER_ASSERT_POINTER(mapped_pointer);
    EGL_HELPER_ASSERT_POINTER(phys_addr);

	/* open the fb device */
	fb_dev = open( "/dev/fb0", O_RDWR );
	if(fb_dev >= 0)
	{
		int ioctl_result;
		struct fb_fix_screeninfo fix_info;
		struct fb_var_screeninfo var_info;
        u32 fb_size;

		ioctl_result = ioctl(fb_dev, FBIOGET_VSCREENINFO, &var_info);
        if (-1 == ioctl_result)
        {
            mali_tpi_printf("egl_helper_physical_memory_get: Could not query variable screen info\n");
            goto cleanup;
        }

		ioctl_result = ioctl(fb_dev, FBIOGET_FSCREENINFO, &fix_info);
        if (-1 == ioctl_result)
        {
            mali_tpi_printf("egl_helper_physical_memory_get: Could not query fixed screen info\n");
            goto cleanup;
        }

		if (var_info.yres_virtual < (2 * var_info.yres))
        {
            mali_tpi_printf("egl_helper_physical_memory_get: Test failure due to non-compatible direct rendering system\n");
            goto cleanup;
        }

		fb_size = (size_t)( var_info.xres * var_info.yres_virtual * ( var_info.bits_per_pixel>>3 ));
        if (size > fb_size)
        {
            mali_tpi_printf("egl_helper_physical_memory_get: frame buffer not large enough to cater for requested size\n");
            goto cleanup;
        }

		mapped_buffer = mmap((void*)0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fb_dev, 0);
        if (NULL != mapped_buffer)
        {
            *phys_addr = fix_info.smem_start;
        }
        else
        {
            mali_tpi_printf("egl_helper_physical_memory_get: mmap failed\n");
            goto cleanup;
        }
    }
    else
    {
        mali_tpi_printf("egl_helper_physical_memory_get: Could not open fbdev\n");
    }

cleanup:
    *mapped_pointer = mapped_buffer;
    if (fb_dev >= 0)
    {
		(void)close( fb_dev );
    }
    return 0; /* cookie not used */
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
