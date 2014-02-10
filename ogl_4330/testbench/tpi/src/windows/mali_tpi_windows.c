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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

/* ============================================================================
	Data Structures
============================================================================ */
static mali_tpi_bool initialized = MALI_TPI_FALSE;
static CRITICAL_SECTION printf_lock;

/* ============================================================================
	Public Functions
============================================================================ */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_init( void )
{
	if( !initialized )
	{
		InitializeCriticalSection( &printf_lock );
		initialized = MALI_TPI_TRUE;
	}
	return _mali_tpi_init_internal();
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_shutdown( void )
{
	if( initialized )
	{
		DeleteCriticalSection( &printf_lock );
		initialized = MALI_TPI_FALSE;
	}
	return _mali_tpi_shutdown_internal();
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_file *mali_tpi_stdout( void )
{
	return stdout;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_file *mali_tpi_stderr( void )
{
	return stderr;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_file *mali_tpi_fopen(
	mali_tpi_dir dir,
	const char *filename,
	const char *mode )
{
	/* Windows CE doesn't have a current directory. Use the directory of
	 * the binary instead. */
	TCHAR module_name_t[MAX_PATH];
	char module_name[MAX_PATH];
	char full_filename[MAX_PATH];
	const char *prefix;
	size_t module_path_len;
	size_t i;

	GetModuleFileName(
	    NULL, module_name_t, sizeof(module_name_t) / sizeof(TCHAR) );
#ifdef _UNICODE
	WideCharToMultiByte(
	    CP_ACP, 0, module_name_t, -1, module_name,
	    sizeof(module_name), NULL, NULL);
#else
	strcpy( module_name, module_name_t );
#endif

	/* Strip the module name itself to leave the directory */
	module_path_len = strlen( module_name );
	while( (module_path_len > 0) && (module_name[module_path_len-1] != '\\') )
	{
		module_path_len--;
	}

	switch( dir )
	{
	case MALI_TPI_DIR_DATA:
		prefix = "";
		break;

	case MALI_TPI_DIR_RESULTS:
		prefix="results/";
		break;

	default:
		assert(0); /* Should never be reached */
		return NULL;
	}

	/* Put together the path, prefix and filename */
	mali_tpi_snprintf(
	    full_filename, sizeof(full_filename), "%.*s%s%s",
	    module_path_len, module_name, prefix, filename );

	/* Normalize all directory separators to backslash */
	for( i = 0; full_filename[i] != '\0'; i++ )
	{
		if( full_filename[i] == '/' )
		{
			full_filename[i] = '\\';
		}
	}

	return fopen( full_filename, mode );
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

	/* Cause the enter string to be send to the C library atomically */
	EnterCriticalSection( &printf_lock );
	ret = mali_tpi_format_string(
		(int (*)(char, void *)) fputc,
		(void *) stream, format, ap );
	LeaveCriticalSection( &printf_lock );
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
	/* GetTickCount returns milliseconds */
	return GetTickCount() * (mali_tpi_uint64) 1000000;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_sleep_status mali_tpi_sleep_ns(
	mali_tpi_uint64 interval,
	mali_tpi_bool interruptible )
{
	/* Sleep takes milliseconds. It does not support interrupt or failure. */
	Sleep( (DWORD) ((interval + 500000) / 1000000) );
	return MALI_TPI_SLEEP_STATUS_OK;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_sleep_status mali_tpi_sleep_until_ns(
	mali_tpi_uint64 timestamp,
	mali_tpi_bool interruptible )
{
	/* Windows does not have a sleep-until equivalent, so we have to turn it
	 * into a relative sleep. */
	mali_tpi_uint64 now = mali_tpi_get_time_ns();
	if( now > timestamp )
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
	*thread = CreateThread(
		NULL,                               /* Thread attributes */
		0,                                  /* Stack size (use default) */
		(LPTHREAD_START_ROUTINE)threadproc, /* Thread function pointer */
		start_param,                        /* Thread start data */
		0,                                  /* Creation flags */
		NULL );                             /* Thread ID (output) */
	if( NULL == *thread )
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
	if( WAIT_OBJECT_0 != WaitForSingleObject( *thread, INFINITE ) )
	{
		return;
	}

	if( exitcode )
	{
		GetExitCodeThread( *thread, (DWORD*)exitcode );
	}

	CloseHandle( thread );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_mutex_init(
	mali_tpi_mutex *mutex )
{
	InitializeCriticalSection( mutex );
	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_mutex_term(
	mali_tpi_mutex *mutex )
{
	DeleteCriticalSection( mutex );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_mutex_lock(
	mali_tpi_mutex *mutex )
{
	EnterCriticalSection( mutex );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL void mali_tpi_mutex_unlock(
	mali_tpi_mutex *mutex )
{
	LeaveCriticalSection( mutex );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_init(
		mali_tpi_barrier *barrier,
		u32 number_of_threads)
{
	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_destroy( mali_tpi_barrier *barrier )
{
	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_wait( mali_tpi_barrier *barrier )
{
    return MALI_TPI_TRUE;
}
