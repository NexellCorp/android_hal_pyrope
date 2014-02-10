/*
 * Copyright:
 * ----------------------------------------------------------------------------
 * This confidential and proprietary software may be used only as authorized
 * by a licensing agreement from ARM Limited.
 *      (C) COPYRIGHT 2009-2010, 2012 ARM Limited, ALL RIGHTS RESERVED
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
#include <sys/time.h>

/* ============================================================================
	Public Functions
============================================================================ */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_init(void)
{
	/* No platform-specific setup required */
	return _mali_tpi_init_internal();
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_shutdown(void)
{
	/* No platform-specific shutdown required */
	return _mali_tpi_shutdown_internal();
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_file *mali_tpi_stdout(void)
{
	return stdout;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_file *mali_tpi_stderr(void)
{
	return stderr;
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
		return fopen( filename, mode );

	case MALI_TPI_DIR_RESULTS:
		{
			const char prefix[] = "results/";
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
	mali_tpi_file *stream )
{
	return fputs( s, stream );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_vfprintf(
	mali_tpi_file *stream,
	const char *format,
	va_list ap )
{
	int ret;

	flockfile( stream );
	ret = mali_tpi_format_string(
		(int (*)(char, void *))fputc, (void *) stream, format, ap );
	funlockfile( stream );
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
	return malloc( size );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_free( void *ptr )
{
	free( ptr );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_uint64 mali_tpi_get_time_ns( void )
{
	struct timeval tv;
	uint64_t time_ns;
	gettimeofday(&tv, NULL);

	time_ns  = ((mali_tpi_uint64) tv.tv_sec)  * 1000000000;
	time_ns += ((mali_tpi_uint64) tv.tv_usec) * 1000;
	return time_ns;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_sleep_status mali_tpi_sleep_ns(
	mali_tpi_uint64 interval,
	mali_tpi_bool interruptible )
{
	if( !interruptible )
	{
		/* Restarting is only reliable when doing absolute timing. Convert
		 * to an absolute time.
		 */
		mali_tpi_uint64 now = mali_tpi_get_time_ns();
		if( 0 == now )
		{
			return MALI_TPI_SLEEP_STATUS_FAILED;
		}
		return mali_tpi_sleep_until_ns( interval + now, interruptible );
	}
	else
	{
		struct timespec target;
		const unsigned int per_s = 1000000000;
		int status;

		target.tv_sec = (time_t) (interval / per_s);
		target.tv_nsec = (long)  (interval % per_s);

		status = nanosleep( &target, NULL );
		switch( status )
		{
		case 0:
			return MALI_TPI_SLEEP_STATUS_OK;

		case EINTR:
			return MALI_TPI_SLEEP_STATUS_INTERRUPTED;

		default:
			return MALI_TPI_SLEEP_STATUS_FAILED;
		}
	}
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_sleep_status mali_tpi_sleep_until_ns(
	mali_tpi_uint64 timestamp,
	mali_tpi_bool interruptible )
{
	struct timespec target;
	const unsigned int per_s = 1000000000;

	target.tv_sec = (time_t) (timestamp / per_s);
	target.tv_nsec = (long)  (timestamp % per_s);

	while( MALI_TPI_TRUE )
	{
		int status = nanosleep( &target, NULL );
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

#if 0
/* ------------------------------------------------------------------------- */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_init(
		mali_tpi_barrier *barrier,
		u32 number_of_threads)
{
	/* No support for barrier attributes yet: parameter two. Use default ones. */
	int err = pthread_barrier_init( barrier, NULL, number_of_threads );

	if( err != 0 )
	{
		return MALI_TPI_FALSE;
	}

	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_destroy( mali_tpi_barrier *barrier )
{
	int err = pthread_barrier_destroy( barrier );

	if( err != 0 )
	{
		return MALI_TPI_FALSE;
	}

	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_wait( mali_tpi_barrier *barrier )
{
    /* Synchronization point */
    int err = pthread_barrier_wait( barrier );

    if ( (err != 0) && (err != PTHREAD_BARRIER_SERIAL_THREAD) )
	{
		return MALI_TPI_FALSE;
	}

	return MALI_TPI_TRUE;
}
#endif
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

