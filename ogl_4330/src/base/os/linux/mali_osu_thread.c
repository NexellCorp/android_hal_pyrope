/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include "mali_osu.h"

#include <pthread.h>
#include <sched.h>

/* SCHED_IDLE is not defined on Android. */
#ifndef SCHED_IDLE
#define SCHED_IDLE 5
#endif

/* static array of all the thread keys which exists */
static pthread_key_t   thread_key[_MALI_OSU_THREAD_KEY_MAX] = {0, };
/* Per thread-key status of whether it has been initialized */
static mali_bool       thread_key_initialized[_MALI_OSU_THREAD_KEY_MAX] = { MALI_FALSE, };
/* Critical Section protecting thread key initialization */
static pthread_mutex_t thread_key_initialization_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Private declaration of the OSU thread type */
struct _mali_osu_thread_t_struct
{
    pthread_t handle; /* pthread_t is of type unsigned long int, so can use _mali_osu_thread_t handles without backing with memory */
};

typedef void *(*pthread_start_routine_t)(void *);

/**
 * Thread key creator
 * Called when a thread key write is detected and the key is not detected as being initialized
 * Handles race condition of first write from multiple threads comming in in parallell
 * @param key The key to create
 * @return Standard Mali error code
 */
MALI_STATIC _mali_osu_errcode_t _mali_thread_key_create( _mali_osu_thread_key_t key ) MALI_CHECK_RESULT;

MALI_STATIC _mali_osu_errcode_t _mali_thread_key_create( _mali_osu_thread_key_t key )
{
	int call_result;
	MALI_DEBUG_ASSERT_RANGE( (int)key, 0, _MALI_OSU_THREAD_KEY_MAX - 1 );

	/* key was detected uninitialized by caller, initialize it */

	/* Enter Critical Section for thread key initialization */
	call_result = pthread_mutex_lock(&thread_key_initialization_mutex);
	/* It would be a programming error for this to fail: */
	MALI_DEBUG_ASSERT( 0 == call_result,
					   ("failed to lock critical section\n") );

	/* check again with the lock held (which was originally done in _mali_osu_thread_key_set_data) */
	if ( MALI_TRUE == thread_key_initialized[key])
	{
		/* we cought a race condition, the other thread created the thread key */
		call_result = pthread_mutex_unlock(&thread_key_initialization_mutex);

		MALI_DEBUG_ASSERT(0 == call_result,
						  ("failed to unlock critical section\n"));

		return _MALI_OSU_ERR_OK;
	}

	/* no key exists and we hold the lock */
	if (0 != pthread_key_create(&thread_key[key], NULL))
	{
		/* pthread failed, cleanup and report error */
		MALI_DEBUG_ERROR(("Failed to create thread key %d", key));
		call_result = pthread_mutex_unlock(&thread_key_initialization_mutex);

		MALI_DEBUG_ASSERT(0 == call_result,
						  ("failed to unlock critical section\n"));

		return _MALI_OSU_ERR_FAULT;
	}

	/* key created, set as initialized */
	thread_key_initialized[key] = MALI_TRUE;
	/* release lock */
	call_result = pthread_mutex_unlock(&thread_key_initialization_mutex);

	MALI_DEBUG_ASSERT(0 == call_result,
					  ("failed to unlock critical section\n"));

	MALI_IGNORE( call_result );

	return _MALI_OSU_ERR_OK;
}



_mali_osu_errcode_t _mali_osu_thread_key_set_data( _mali_osu_thread_key_t key, void* value )
{
	_mali_osu_errcode_t err = _MALI_OSU_ERR_OK;
	MALI_DEBUG_ASSERT_RANGE( (int)key, 0, _MALI_OSU_THREAD_KEY_MAX - 1 );

	/** @opt We avoid entering the critical section when we already know the TLS
	 * slot was initialized */
	if ( MALI_FALSE == thread_key_initialized[key])
	{
		/* Thread synchronization will be done by _mali_thread_key_create */
		err = _mali_thread_key_create(key);
	}

	if ( _MALI_OSU_ERR_OK == err )
	{
		int call_result;
		/* It is a programming error for this to fail */
		call_result = pthread_setspecific(thread_key[key], value);
		MALI_DEBUG_ASSERT( 0 == call_result,
						   ("pthread_setspecific failed with %d\n",call_result) );
		MALI_IGNORE( call_result );
	}

	return err;
}

void* _mali_osu_thread_key_get_data( _mali_osu_thread_key_t key )
{
	MALI_DEBUG_ASSERT_RANGE( (int)key, 0, _MALI_OSU_THREAD_KEY_MAX - 1 );

	/*
		If a key has been initialized return its value.
		If it's not initialized treat the value as being NULL.
		We will never initialize a thread key during a read.
		This thread can never have written anything if the key hasn't already been initialized
	*/
	if (thread_key_initialized[key])
	{
		return pthread_getspecific(thread_key[key]);
	}
	else
	{
		return NULL;
	}
}

u32 _mali_osu_thread_get_current(void)
{
	return pthread_self();
}

_mali_osu_errcode_t _mali_osu_create_thread(_mali_osu_thread_t **thread, _mali_osu_threadproc_t *threadproc, void *start_param)
{
    int ret;
    pthread_t *pthread_handle;
    MALI_DEBUG_ASSERT_POINTER( thread );
	MALI_DEBUG_ASSERT_POINTER( threadproc );

	pthread_handle = (pthread_t *)thread; /* pthread_t is of type unsigned long int, so can use _mali_osu_thread_t handles without backing with memory */
	ret = pthread_create( pthread_handle, NULL, (pthread_start_routine_t)threadproc, start_param );
	if ( 0 != ret )
	{
		return _MALI_OSU_ERR_FAULT;
    }

    return _MALI_OSU_ERR_OK;
}

_mali_osu_errcode_t _mali_osu_wait_for_thread(_mali_osu_thread_t *thread, u32 *exitcode)
{
    u32 osu_thread_exitcode;
    void *posu_thread_exitcode = &osu_thread_exitcode; /* threads created by OSU return a 32-bit exit code */
    pthread_t thread_handle = (pthread_t)thread; /* pthread_t is of type unsigned long int, so can use _mali_osu_thread_t handles without backing with memory */

	if ( 0 != pthread_join( thread_handle, &posu_thread_exitcode ) )
	{
    	return _MALI_OSU_ERR_FAULT;
	}

    if (exitcode) *exitcode = osu_thread_exitcode;
	return _MALI_OSU_ERR_OK;
}

_mali_osu_errcode_t _mali_osu_thread_set_idle_scheduling_policy(void)
{
	struct sched_param sched_param;

	sched_param.sched_priority = 0;
	return (sched_setscheduler(0, SCHED_IDLE, &sched_param) == 0 ? _MALI_OSU_ERR_OK : _MALI_OSU_ERR_FAULT);
}
