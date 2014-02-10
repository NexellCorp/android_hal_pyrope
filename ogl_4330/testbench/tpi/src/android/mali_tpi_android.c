/*
 * Copyright:
 * ----------------------------------------------------------------------------
 * This confidential and proprietary software may be used only as authorized
 * by a licensing agreement from ARM Limited.
 *      (C) COPYRIGHT 2009-2012 ARM Limited, ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorized copies and
 * copies may only be made to the extent permitted by a licensing agreement
 * from ARM Limited.
 * ----------------------------------------------------------------------------
 */

/* ============================================================================
	Includes
============================================================================ */
#define MALI_TPI_API MALI_TPI_EXPORT
#include "tpi/mali_tpi.h"

#ifndef _XOPEN_SOURCE
	#define _XOPEN_SOURCE 600
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <EGL/eglplatform.h>
#include <android/log.h>

#define LOG_INFO(format, args) ((void)__android_log_vprint(ANDROID_LOG_INFO, "mali_test", format, args))
#define LOG_ERROR(format, args) ((void)__android_log_vprint(ANDROID_LOG_ERROR, "mali_test", format, args))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "mali_test", __VA_ARGS__))
#define LOG(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "mali_test", __VA_ARGS__))

#define MALI_TPI_ROOT_PATH "/data/data/com.mali.testjava/"
#define MALI_TPI_WAIT_FILENAME "egl_wait_file"

static mali_tpi_file *mali_stdout_file = NULL;
static mali_tpi_file *mali_stderr_file = NULL;


/* ============================================================================
	Public Functions
============================================================================ */

MALI_TPI_IMPL mali_tpi_bool mali_tpi_init( void )
{
	/* platform-specific setup */
	mali_tpi_bool res = _mali_tpi_init_internal();
	mali_bool wait_debugger = MALI_TRUE;

	while( wait_debugger )
	{
		mali_tpi_file *debug_file = mali_tpi_fopen(MALI_TPI_DIR_DATA, MALI_TPI_WAIT_FILENAME,"r");
		/* Does the file still exist ? */
		if( NULL != debug_file )
		{
			mali_tpi_fclose( debug_file );
			LOG(" Waiting for \"" MALI_TPI_WAIT_FILENAME "\" file to be deleted\n");
			mali_tpi_sleep_ns(1000 * 1000 * 1000, MALI_TPI_TRUE ); /* 1 second */
		
		}
		else
		{
			wait_debugger = MALI_FALSE;
		}
	}

	if( MALI_TPI_FALSE != res && NULL == mali_stdout_file )
	{
		mali_stdout_file = mali_tpi_fopen(MALI_TPI_DIR_DATA, "stdout.txt","w");
		if( NULL == mali_stdout_file )
		{
			LOG("Failed to open stdout channel\n");
			res = MALI_TPI_FALSE;
		}
	}
	if( MALI_TPI_FALSE != res && NULL == mali_stderr_file )
	{
		mali_stderr_file = mali_tpi_fopen(MALI_TPI_DIR_DATA, "stderr.txt","w");
		if( NULL == mali_stderr_file )
		{
			LOG("Failed to open stderr channel\n");
			res = MALI_TPI_FALSE;
		}
	}

	return res;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_shutdown( void )
{
	/* No platform-specific shutdown required */
	mali_tpi_bool res = MALI_TPI_TRUE;
	if( NULL != mali_stdout_file && 
	    0 != mali_tpi_fclose( mali_stdout_file ))
	{
		res = MALI_TPI_FALSE;
	}
	mali_stdout_file = NULL;
	
	if( NULL != mali_stderr_file && 
	    0 != mali_tpi_fclose( mali_stderr_file ))
	{
		res = MALI_TPI_FALSE;
	}
	mali_stderr_file = NULL;

	if( MALI_TPI_FALSE == _mali_tpi_shutdown_internal())
	{
		res = MALI_TPI_FALSE;
	}

	return res;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_file *mali_tpi_stdout( void )
{
	return mali_stdout_file;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_file *mali_tpi_stderr( void )
{
	return mali_stderr_file;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_file *mali_tpi_fopen(
	mali_tpi_dir dir,
	const char *filename,
	const char *mode )
{
	switch( dir )
	{
	case MALI_TPI_DIR_DATA:
		{
			const size_t len = strlen( MALI_TPI_ROOT_PATH ) + strlen( filename ) + 1;
			char *name;
			mali_tpi_file *ret;
			name = mali_tpi_alloc( len );
			if( NULL == name )
			{
				return NULL;
			}

			strncpy( name, MALI_TPI_ROOT_PATH, len );
			strncat( name, filename, len - strlen( name ) );
			ret = fopen( name, mode );
			mali_tpi_free( name );
			return ret;
		}
	case MALI_TPI_DIR_RESULTS:
		{
			const char prefix[] = MALI_TPI_ROOT_PATH "results/";
			const size_t len = strlen( prefix ) + strlen( filename ) + 1;
			char *name;
			mali_tpi_file *ret;

			/* Create results dir if it doesn't exist, for local use. No
			 * need to check the result, if it fails then either the dir
			 * already exists, or the fopen call will fail.
			 */
			mkdir( prefix, 0777 );

			name = mali_tpi_alloc( len );
			if( NULL == name )
			{
				return NULL;
			}

			strncpy( name, prefix, len );
			strncat( name, filename, len - strlen( name ) );
			ret = fopen( name, mode );
			mali_tpi_free( name );
			return ret;
		}

	default:
		assert(0); /* Should never be reached */
		return NULL;
	}
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL size_t mali_tpi_fread(
	void *ptr,
	size_t size,
	size_t nmemb,
	mali_tpi_file *stream )
{
	return fread( ptr, size, nmemb, stream );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL size_t mali_tpi_fwrite(
	const void *ptr,
	size_t size,
	size_t nmemb,
	mali_tpi_file *stream )
{
	return fwrite( ptr, size, nmemb, stream ); 
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_fputs(
	const char *s,
	mali_tpi_file *stream)
{
	if( mali_stdout_file == stream )
	{
		LOGI("%s\n", s);
		fputs( s, stdout );
	}
	else if( mali_stderr_file == stream )
	{
		LOG("%s\n", s);
		fputs( s, stderr );
	}

	return fputs( s, stream );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_vfprintf(
	mali_tpi_file *stream,
	const char *format,
	va_list ap )
{
	int ret;
	if( NULL == stream )
	{
		return -1;
	}

	if( mali_stdout_file == stream )
	{
		LOG_INFO( format, ap );
		flockfile( stdout );
		ret = mali_tpi_format_string(
			(int (*)(char, void *)) fputc, (void *) stdout, format, ap);
		funlockfile( stdout );
	}
	else if( mali_stderr_file == stream )
	{
		LOG_ERROR( format, ap );
		flockfile( stderr );
		ret = mali_tpi_format_string(
			(int (*)(char, void *)) fputc, (void *) stderr, format, ap);
		funlockfile( stderr );
	}

	if( ret >= 0 )
	{
		flockfile( stream );
		ret = mali_tpi_format_string(
			(int (*)(char, void *)) fputc, (void *) stream, format, ap);
		funlockfile( stream );
	}

	return ret;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_fclose( mali_tpi_file *stream )
{
	return fclose( stream );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_fflush( mali_tpi_file *stream )
{
	return fflush( stream );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL size_t mali_tpi_fsize( mali_tpi_file *stream )
{
	long int saved_pointer;
	long int stream_size;
	int  error;

	saved_pointer = ftell( stream );
	if( -1 == saved_pointer )
	{
		return 0;
	}

	error = fseek( stream, 0, SEEK_END );
	if( 0 != error )
	{
		return 0;
	}

	stream_size = ftell( stream );
	if( -1 == stream_size )
	{
		return 0;
	}
	
	fseek( stream, saved_pointer, SEEK_SET );
	return (size_t) stream_size;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void *mali_tpi_alloc( size_t size )
{
	return malloc(size);
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_free( void *ptr )
{
	free(ptr);
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_uint64 mali_tpi_get_time_ns( void )
{
	struct timespec ts;

	if( clock_gettime( CLOCK_MONOTONIC, &ts ) == -1 )
	{
		return 0;
	}
	else
	{
		return ( (mali_tpi_uint64)1000000000 ) * ts.tv_sec + ts.tv_nsec;
	}
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_sleep_status mali_tpi_sleep_ns(
	mali_tpi_uint64 interval,
	mali_tpi_bool interruptible )
{
	struct timespec target;
	const unsigned int per_s = 1000000000;

	target.tv_sec = (time_t) (interval / per_s);
	target.tv_nsec = (long) (interval % per_s);

	while( MALI_TPI_TRUE )
	{
		int status = nanosleep( &target, &target);

		switch( status )
		{
		case 0:
			return MALI_TPI_SLEEP_STATUS_OK;

		case EINTR:
			if( interruptible )
			{
				return MALI_TPI_SLEEP_STATUS_INTERRUPTED;
			}
			break;

		default:
			return MALI_TPI_SLEEP_STATUS_FAILED;
		}
		/* If we get here, it was EINTR and we need to restart */
	}
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_sleep_status mali_tpi_sleep_until_ns(
    mali_tpi_uint64 timestamp,
	mali_tpi_bool interruptible )
{
	mali_tpi_uint64 now = mali_tpi_get_time_ns();
	if( now == 0 )
	{
		return MALI_TPI_SLEEP_STATUS_FAILED;
	}
	if( now >= timestamp )
	{
		return MALI_TPI_SLEEP_STATUS_OK;
	}

	return mali_tpi_sleep_ns( timestamp - now, interruptible );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_thread_create(
	mali_tpi_thread *thread,
	mali_tpi_threadproc threadproc,
	void *start_param )
{
	int err = pthread_create( thread, NULL, threadproc, start_param );
	if( err )
	{
		return MALI_TPI_FALSE;
	}

	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_thread_wait(
	mali_tpi_thread *thread,
	void **exitcode )
{
	pthread_join( *thread , exitcode );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_mutex_init(
	mali_tpi_mutex *mutex )
{
	int err = pthread_mutex_init( mutex, NULL );
	if( err )
	{
		return MALI_TPI_FALSE;
	}

	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_mutex_term(
	mali_tpi_mutex *mutex )
{
	pthread_mutex_destroy( mutex );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_mutex_lock(
	mali_tpi_mutex *mutex )
{
	pthread_mutex_lock( mutex );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_mutex_unlock(
	mali_tpi_mutex *mutex )
{
	pthread_mutex_unlock( mutex );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_init(
		mali_tpi_barrier *barrier,
		u32 number_of_threads)
{
	int err;

    barrier->needed_threads = number_of_threads;
    barrier->called = 0;

	err = pthread_mutex_init(&barrier->mutex, NULL);
	if( err )
	{
		return MALI_TPI_FALSE;
	}

	err = pthread_cond_init(&barrier->cond, NULL);
	if( err )
	{
		return MALI_TPI_FALSE;
	}

    return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_destroy( mali_tpi_barrier *barrier )
{
	int err;

    err = pthread_mutex_destroy(&barrier->mutex);
	if( err )
	{
		return MALI_TPI_FALSE;
	}

	err = pthread_cond_destroy(&barrier->cond);
	if( err )
	{
		return MALI_TPI_FALSE;
	}

	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_wait( mali_tpi_barrier *barrier )
{

    pthread_mutex_lock(&barrier->mutex);

    barrier->called++;
    if (barrier->called == barrier->needed_threads)
    {
        barrier->called = 0;
        pthread_cond_broadcast(&barrier->cond);
    }
    else
    {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }

    pthread_mutex_unlock(&barrier->mutex);

	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_API mali_tpi_bool mali_tpi_sem_init(mali_tpi_sem *sem, u32 value)
{
	mali_tpi_bool err = MALI_TPI_TRUE;

	if (sem_init(&sem->sem, 0, value))
	{
		err = MALI_TPI_FALSE;
	}

	return err;
}

/* ------------------------------------------------------------------------- */
MALI_API void mali_tpi_sem_term(mali_tpi_sem *sem)
{
	int err = sem_destroy(&sem->sem);
	assert(err == 0);
}

/* ------------------------------------------------------------------------- */
MALI_API mali_tpi_bool mali_tpi_sem_wait(mali_tpi_sem *sem, u64 timeout_nsec)
{
	mali_tpi_bool ret = MALI_TPI_TRUE;
	int err;

	if (timeout_nsec)
	{
		struct timespec tmo;

		err = clock_gettime(CLOCK_REALTIME, &tmo);

		assert(err == 0);

		tmo.tv_sec  += timeout_nsec / 1000000000U;
		tmo.tv_nsec += timeout_nsec % 1000000000U;

		if (tmo.tv_nsec >= 1000000000)
		{
			tmo.tv_nsec -= 1000000000;
			tmo.tv_sec++;
		}

		do
		{
			err = sem_timedwait(&sem->sem, &tmo);
		} while (err == -1 && errno == EINTR);

		if (err == -1)
		{
			assert(errno == ETIMEDOUT);
			ret = MALI_TPI_FALSE;
		}

	}
	else
	{
		do
		{
			err = sem_wait(&sem->sem);
		} while (err == -1 && errno == EINTR);

		assert(err == 0);
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
MALI_API void mali_tpi_sem_post(mali_tpi_sem *sem)
{
	int err = sem_post(&sem->sem);
	assert(err == 0);
}
