/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#if ((!defined _XOPEN_SOURCE) || ((_XOPEN_SOURCE - 0) < 600))
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#elif _POSIX_C_SOURCE < 200112L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <mali_system.h>
#include <base/mali_profiling.h>
#include "mali_osu.h"
#include "mali_spinlock.h"

#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

/* enable lock contention events -- timeline profiling must enabled too. */
#define MALI_DEBUG_LOCK_CONTENTION_EVENTS 0

/**
 * @file mali_osu_locks.c
 * File implements the user side of the OS interface
 */

/** @opt Most of the time, we use the plain mutex type of osu_lock, and so
 * only require the flags and mutex members. This costs 2 extra DWORDS, but
 * most of the time we don't use those DWORDS.
 * Therefore, ANY_UNLOCK type osu_locks can be implemented as a second
 * structure containing the member _mali_osu_lock_t lock_t, plus the extra
 * state required. Then, we use &container->lock_t when passing out of the
 * OSU api, and CONTAINER_OF() when passing back in to recover the original
 * structure. */

/** Private declaration of the OSU lock type */
struct _mali_osu_lock_t_struct
{
	/** At present, only two types of mutex, so we store this information as
	 * the flags supplied at init time */
	_mali_osu_lock_flags_t flags;

	/* a lock struct represents *either* a mutex or a spinlock */
	union {
		pthread_mutex_t mutex; /**< Used in both plain and ANY_UNLOCK osu_locks */
		mali_spinlock_t spinlock; /**< Only used if _MALI_OSU_LOCKFLAG_SPINLOCK is passed to init */
	} l;

	/* Extra State for ANY_UNLOCK osu_locks. These are UNINITIALIZED when
	 * flags does not contain _MALI_OSU_LOCKFLAG_ANYUNLOCK: */
	pthread_cond_t condition;  /**< The condition object to use while blocking */
	mali_bool state;  /**< The boolean which indicates the event's state */

	PidType owner; /**< tid of lock owner */

	MALI_DEBUG_CODE(
				  /** debug checking of locks */
				  _mali_osu_lock_mode_t locked_as;
	) /* MALI_DEBUG_CODE */
};

/* Provide two statically initialized locks */
MALI_STATIC _mali_osu_lock_t _mali_osu_static_locks[] =
{
	{
		_MALI_OSU_LOCKFLAG_STATIC,
		{(pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER},
		PTHREAD_COND_INITIALIZER,
		MALI_FALSE,
		0,
		MALI_DEBUG_CODE( _MALI_OSU_LOCKMODE_UNDEF )
	},
	{
		_MALI_OSU_LOCKFLAG_STATIC,
		{(pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER},
		PTHREAD_COND_INITIALIZER,
		MALI_FALSE,
		0,
		MALI_DEBUG_CODE( _MALI_OSU_LOCKMODE_UNDEF )
	},
	{
		_MALI_OSU_LOCKFLAG_STATIC,
		{(pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER},
		PTHREAD_COND_INITIALIZER,
		MALI_FALSE,
		0,
		MALI_DEBUG_CODE( _MALI_OSU_LOCKMODE_UNDEF )
	},
	{
		_MALI_OSU_LOCKFLAG_STATIC,
		{(pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER},
		PTHREAD_COND_INITIALIZER,
		MALI_FALSE,
		0,
		MALI_DEBUG_CODE( _MALI_OSU_LOCKMODE_UNDEF )
	},
};

/* Critical section for auto_init */
MALI_STATIC pthread_mutex_t	static_auto_init_mutex = PTHREAD_MUTEX_INITIALIZER;

#if MALI_DEBUG_LOCK_CONTENTION_EVENTS
MALI_STATIC void got_lock(_mali_osu_lock_t *lock, mali_bool contended)
{
	lock->owner = _mali_sys_get_tid();

	if (contended)
	{
		_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE
		                             | VR_PROFILING_EVENT_CHANNEL_SOFTWARE
		                             | VR_PROFILING_EVENT_REASON_SINGLE_LOCK_CONTENDED,
		                             0, 0, lock->owner, 0, 0);
	}
}
MALI_STATIC void lock_contention(_mali_osu_lock_t *lock)
{
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE
	                             | VR_PROFILING_EVENT_CHANNEL_SOFTWARE
				     | VR_PROFILING_EVENT_REASON_SINGLE_LOCK_CONTENDED,
				     0, 0, lock->owner, 0, 0);
}
#endif

_mali_osu_errcode_t _mali_osu_lock_auto_init( _mali_osu_lock_t **pplock, _mali_osu_lock_flags_t flags, u32 initial, u32 order )
{
	int call_result;
	/* Validate parameters: */
	MALI_DEBUG_ASSERT_POINTER( pplock );

	/** @opt We don't lock the Critical Section or do anything if this is already non-null */
	if ( NULL != *pplock)
	{
		return _MALI_OSU_ERR_OK;
	}

	/* We MIGHT need to initialize it, lock the Critical Section and check again */
	call_result = pthread_mutex_lock(&static_auto_init_mutex);
	/* It would be a programming error for this to fail: */
	MALI_DEBUG_ASSERT( 0 == call_result,
					   ("failed to lock critical section\n") );

	if ( NULL != *pplock )
	{
		/*
			We caught a race condition to initialize this osu_lock.
			The other thread won the race, so the osu_lock is now initialized.
		*/
		call_result = pthread_mutex_unlock(&static_auto_init_mutex);

		MALI_DEBUG_ASSERT(0 == call_result,
						  ("failed to unlock critical section\n"));

		return _MALI_OSU_ERR_OK;
	}

	/* We're the first thread in: initialize the osu_lock */
	*pplock = _mali_osu_lock_init( flags, initial, order );

	if ( NULL == *pplock )
	{
		/* osu_lock creation failed */
		call_result = pthread_mutex_unlock(&static_auto_init_mutex);
		MALI_DEBUG_ASSERT(0 == call_result,
						  ("failed to unlock critical section\n"));

		return _MALI_OSU_ERR_FAULT;
	}


	/* osu_lock created OK */
	call_result = pthread_mutex_unlock(&static_auto_init_mutex);

	MALI_DEBUG_ASSERT(0 == call_result,
					  ("failed to unlock critical section\n"));

	MALI_IGNORE( call_result );

	return _MALI_OSU_ERR_OK;
}

_mali_osu_lock_t *_mali_osu_lock_init( _mali_osu_lock_flags_t flags, u32 initial, u32 order )
{
	_mali_osu_lock_t * lock;
	pthread_mutexattr_t mutex_attributes;
	mali_bool is_spinlock;

	MALI_IGNORE(order); /* order isn't implemented yet, for now callers should set it to zero. */

	/* Validate parameters: */
	/* Flags acceptable */
	MALI_DEBUG_ASSERT( 0 == ( flags & _MALI_OSU_LOCKFLAG_STATIC),
					   ("incorrect flags or trying to initialise a statically initialized lock, %.8X\n", flags) );

    /* Parameter initial SBZ - for future expansion */
	MALI_DEBUG_ASSERT( 0 == initial,
					   ("initial must be zero\n") );

	is_spinlock = 0 != (flags & _MALI_OSU_LOCKFLAG_SPINLOCK);

	if ( !is_spinlock )
	{
		if (0 != pthread_mutexattr_init(&mutex_attributes))
		{
			return NULL;
		}

#if MALI_DEBUG_EXTENDED_MUTEX_LOCK_CHECKING
#define MALI_PTHREADS_MUTEX_TYPE PTHREAD_MUTEX_ERRORCHECK
#else
#define MALI_PTHREADS_MUTEX_TYPE PTHREAD_MUTEX_DEFAULT
#endif

		if (0 != pthread_mutexattr_settype(&mutex_attributes, MALI_PTHREADS_MUTEX_TYPE))
		{
			/** Return NULL on failure */
			pthread_mutexattr_destroy(&mutex_attributes);
			return NULL;

		}
	}

#undef MALI_PTHREADS_MUTEX_TYPE

	/** @opt use containing structures for the ANY_UNLOCK type, to
	 * save 2 DWORDS when not in use */
	lock = _mali_sys_malloc( sizeof(_mali_osu_lock_t) );

	/** Return NULL on failure */
	if( NULL == lock )
	{
		if ( !is_spinlock ) {
			pthread_mutexattr_destroy(&mutex_attributes);
		}
		return NULL;
	}

	if ( is_spinlock )
	{
		if ( 0 != mali_spinlock_init( &lock->l.spinlock ) )
		{
			_mali_sys_free( lock );
			return NULL;
		}
	}
	else
	{
		if (0 != pthread_mutex_init( &lock->l.mutex, &mutex_attributes ))
		{
			pthread_mutexattr_destroy(&mutex_attributes);
			_mali_sys_free( lock );
			return NULL;
		}

		/* done with the mutexattr object */
		pthread_mutexattr_destroy(&mutex_attributes);
	}

	/* ANY_UNLOCK type */
	if ( !is_spinlock && (flags & _MALI_OSU_LOCKFLAG_ANYUNLOCK ))
	{
		if (0 != pthread_cond_init( &lock->condition, NULL ))
		{
			/* cleanup */
			pthread_mutex_destroy( &lock->l.mutex );
			_mali_sys_free( lock );
			return NULL;
		}
		lock->state = MALI_FALSE; /* mark as unlocked by default */
	}

	lock->flags = flags;

	/** Debug lock checking */
	MALI_DEBUG_CODE( lock->locked_as = _MALI_OSU_LOCKMODE_UNDEF	);

	return lock;
}

_mali_osu_errcode_t _mali_osu_lock_timed_wait( _mali_osu_lock_t *lock, _mali_osu_lock_mode_t mode, u64 timeout)
{
	/* absolute time specifier */
	struct timespec ts;
	struct timeval tv;

	/* Parameter validation */
	MALI_DEBUG_ASSERT_POINTER( lock );

	MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_RW == mode,
					   ("unrecognised mode, %.8X\n", mode) );
	MALI_DEBUG_ASSERT( _MALI_OSU_LOCKFLAG_ANYUNLOCK == lock->flags, ("Timed operations only implemented for ANYUNLOCK type locks"));

	/* calculate the realtime timeout value */

	if (0 != gettimeofday(&tv, NULL))
	{
		MALI_DEBUG_PRINT(1,("Could not get the current realtime value to calculate the absolute value for a timed mutex lock with a timeout"));
		return _MALI_OSU_ERR_FAULT;
	}

	tv.tv_usec += timeout;

#define MALI_USECS_PER_SECOND 1000000LL
#define MALI_NANOSECS_PER_USEC 1000LL

	/* did we overflow a second in the usec part? */
	while (tv.tv_usec >= MALI_USECS_PER_SECOND)
	{
		tv.tv_usec -= MALI_USECS_PER_SECOND;
		tv.tv_sec++;
	}

	/* copy to the correct struct */
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = (tv.tv_usec * MALI_NANOSECS_PER_USEC);

#undef MALI_USECS_PER_SECOND
#undef MALI_NANOSECS_PER_USEC

	/* lock the mutex protecting access to the state field */
	pthread_mutex_lock( &lock->l.mutex );
	/* loop while locked (state is MALI_TRUE) */
	/* pthread_cond_timedwait unlocks the mutex, wait, and locks the mutex once unblocked (either due to the event or the timeout) */
	while ( MALI_TRUE == lock->state )
	{
		int res;
		res = pthread_cond_timedwait( &lock->condition, &lock->l.mutex, &ts );
		if (0 == res) continue; /* test the state variable again (loop condition) */
		else if (ETIMEDOUT == res)
		{
			/* timeout, need to clean up and return the correct error code */
			pthread_mutex_unlock(&lock->l.mutex);
			return _MALI_OSU_ERR_TIMEOUT;
		}
		else
		{
			MALI_DEBUG_PRINT(1, ("Unexpected return from pthread_cond_timedwait 0x%08X\n", res));

			pthread_mutex_unlock(&lock->l.mutex);
			return _MALI_OSU_ERR_FAULT;
		}
	
	}

	/* DEBUG tracking of previously locked state - occurs while lock is obtained */
	MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_UNDEF == lock->locked_as,
			("This lock was already locked\n") );
	MALI_DEBUG_CODE( lock->locked_as = mode );

	/* the state is MALI_FALSE (unlocked), so we set it to MALI_TRUE to indicate that it's locked and can return knowing that we own the lock */
	lock->state = MALI_TRUE;
	/* final unlock of the mutex */
	pthread_mutex_unlock(&lock->l.mutex);

	return _MALI_OSU_ERR_OK;

}

_mali_osu_errcode_t _mali_osu_lock_wait( _mali_osu_lock_t *lock, _mali_osu_lock_mode_t mode)
{
#if MALI_DEBUG_LOCK_CONTENTION_EVENTS
	mali_bool contention = MALI_FALSE;
#endif

	/* Parameter validation */
	MALI_DEBUG_ASSERT_POINTER( lock );

	MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_RW == mode,
					   ("unrecognised mode, %.8X\n", mode) );

	/** @note since only one flag can be set, we use a switch statement here.
	 * Otherwise, MUST add an enum into the _mali_osu_lock_t to store the
	 * implemented lock type */
	switch ( lock->flags )
	{
    case _MALI_OSU_LOCKFLAG_STATIC:
    case _MALI_OSU_LOCKFLAG_DEFAULT:
		/* Usual Mutex type */
		{
			int call_result;
#if MALI_DEBUG_LOCK_CONTENTION_EVENTS
			call_result = pthread_mutex_trylock( &lock->l.mutex );

			if (0 != call_result)
			{
				lock_contention(lock);
				contention = MALI_TRUE;
				call_result = pthread_mutex_lock( &lock->l.mutex );
				got_lock(lock, contention);
			}
			else
			{
				got_lock(lock, MALI_FALSE);
			}
#else
			call_result = pthread_mutex_lock( &lock->l.mutex );
#endif

			MALI_DEBUG_ASSERT( 0 == call_result,
							   ("pthread_mutex_lock call failed with error code %d (%s)\n", call_result,
							    call_result==EBUSY?"EBUSY - mutex already locked":
								call_result==EAGAIN?"EAGAIN - maximum number of recursive locks exceeded":
								call_result==EDEADLK?"EDEADLK - self deadlock. current thread already owns the mutex":
								call_result==EINVAL?"EINVAL - mutex protected, or not initialized":
								"Unknown error code"
							   )
							 );
			MALI_IGNORE( call_result );
		}

		/* DEBUG tracking of previously locked state - occurs while lock is obtained */
		MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_UNDEF == lock->locked_as,
						   ("This lock was already locked\n") );
		MALI_DEBUG_CODE( lock->locked_as = mode );
		break;

	case _MALI_OSU_LOCKFLAG_ANYUNLOCK:
		/** @note Use of bitflags in a case statement ONLY works because this
		 * is the ONLY flag that is supported */

		/* lock the mutex protecting access to the state field */
		pthread_mutex_lock( &lock->l.mutex );
		/* loop while locked (state is MALI_TRUE) */
		/* pthread_cond_wait unlocks the mutex, wait, and locks the mutex once unblocked */
#if MALI_DEBUG_LOCK_CONTENTION_EVENTS
		if (MALI_TRUE == lock->state)
		{
			lock_contention(lock);
			contention = MALI_TRUE;
		}
#endif
		while ( MALI_TRUE == lock->state ) pthread_cond_wait( &lock->condition, &lock->l.mutex );
#if MALI_DEBUG_LOCK_CONTENTION_EVENTS
		got_lock(lock, contention);
#endif

		/* DEBUG tracking of previously locked state - occurs while lock is obtained */
		MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_UNDEF == lock->locked_as,
						   ("This lock was already locked\n") );
		MALI_DEBUG_CODE( lock->locked_as = mode );

		/* the state is MALI_FALSE (unlocked), so we set it to MALI_TRUE to indicate that it's locked and can return knowing that we own the lock */
		lock->state = MALI_TRUE;
		/* final unlock of the mutex */
		pthread_mutex_unlock(&lock->l.mutex);
		break;

	case _MALI_OSU_LOCKFLAG_SPINLOCK:
		/* spin lock */
		mali_spinlock_wait( &lock->l.spinlock );
		break;

	default:
		MALI_DEBUG_ERROR( ("lock has incorrect flags==%.8X\n", lock->flags) );
		break;
	}

	return _MALI_OSU_ERR_OK;
}

_mali_osu_errcode_t _mali_osu_lock_trywait( _mali_osu_lock_t *lock, _mali_osu_lock_mode_t mode)
{
	_mali_osu_errcode_t err = _MALI_OSU_ERR_FAULT;
	/* Parameter validation */
	MALI_DEBUG_ASSERT_POINTER( lock );

	MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_RW == mode,
					   ("unrecognised mode, %.8X\n", mode) );

	/** @note since only one flag can be set, we use a switch statement here.
	 * Otherwise, MUST add an enum into the _mali_osu_lock_t to store the
	 * implemented lock type */
	switch ( lock->flags )
	{
    case _MALI_OSU_LOCKFLAG_STATIC:
    case _MALI_OSU_LOCKFLAG_DEFAULT:
		/* Usual Mutex type */
		{
			/* This is not subject to MALI_CHECK - overriding the result would cause a programming error */
			if ( 0 == pthread_mutex_trylock( &lock->l.mutex ) )
			{
				err = _MALI_OSU_ERR_OK;

				/* DEBUG tracking of previously locked state - occurs while lock is obtained */
				MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_UNDEF == lock->locked_as
								   || mode == lock->locked_as,
								   ("tried as mode==%.8X, but was locked as %.8X\n", mode, lock->locked_as) );
				MALI_DEBUG_CODE( lock->locked_as = mode );
			}
		}
		break;

	case _MALI_OSU_LOCKFLAG_ANYUNLOCK:
		/** @note Use of bitflags in a case statement ONLY works because this
		 * is the ONLY flag that is supported */

		/* lock the mutex protecting access to the state field */
		pthread_mutex_lock(&lock->l.mutex);

		if ( MALI_FALSE == lock->state)
		{
			/* unlocked, take the lock */
			lock->state = MALI_TRUE;
			err = _MALI_OSU_ERR_OK;
		}

		/* DEBUG tracking of previously locked state - occurs while lock is obtained */
		/* Can do this regardless of whether we obtained ANYUNLOCK: */


		MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_UNDEF == lock->locked_as
						   || mode == lock->locked_as,
						   ("tried as mode==%.8X, but was locked as %.8X\n", mode, lock->locked_as) );
		/* If we were already locked, this does no harm, because of the above assert: */
		MALI_DEBUG_CODE( lock->locked_as = mode );

		pthread_mutex_unlock(&lock->l.mutex);
		break;

	case _MALI_OSU_LOCKFLAG_SPINLOCK:
		/* Spin lock type */
		MALI_DEBUG_ERROR( ("trywait() is not implemented for spinlocks\n") );
		break;

	default:
		MALI_DEBUG_ERROR( ("lock has incorrect flags==%.8X\n", lock->flags) );
		break;
	}

	return err;
}


void _mali_osu_lock_signal( _mali_osu_lock_t *lock, _mali_osu_lock_mode_t mode )
{
	/* Parameter validation */
	MALI_DEBUG_ASSERT_POINTER( lock );

	MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_RW == mode,
					   ("unrecognised mode, %.8X\n", mode) );

	/** @note since only one flag can be set, we use a switch statement here.
	 * Otherwise, MUST add an enum into the _mali_osu_lock_t to store the
	 * implemented lock type */
	switch ( lock->flags )
	{
	case _MALI_OSU_LOCKFLAG_STATIC:
	case _MALI_OSU_LOCKFLAG_DEFAULT:
		/* Usual Mutex type */

		/* DEBUG tracking of previously locked state - occurs while lock is obtained */
		MALI_DEBUG_ASSERT( mode == lock->locked_as,
						   ("This lock was locked as==%.8X, but tried to unlock as mode==%.8X\n", lock->locked_as, mode));
		MALI_DEBUG_CODE( lock->locked_as = _MALI_OSU_LOCKMODE_UNDEF );

		{
			int call_result;
			call_result = pthread_mutex_unlock( &lock->l.mutex );
			MALI_DEBUG_ASSERT( 0 == call_result,
							   ("pthread_mutex_lock call failed with error code %d\n", call_result));
			MALI_IGNORE( call_result );
		}
		break;

	case _MALI_OSU_LOCKFLAG_ANYUNLOCK:
		/** @note Use of bitflags in a case statement ONLY works because this
		 * is the ONLY flag that is supported */

		pthread_mutex_lock(&lock->l.mutex);
		MALI_DEBUG_ASSERT( MALI_TRUE == lock->state, ("Unlocking a _mali_osu_lock_t %p which is not locked\n", lock));

		/* DEBUG tracking of previously locked state - occurs while lock is obtained */
		MALI_DEBUG_ASSERT( mode == lock->locked_as,
						   ("This lock was locked as==%.8X, but tried to unlock as %.8X\n", lock->locked_as, mode ));
		MALI_DEBUG_CODE( lock->locked_as = _MALI_OSU_LOCKMODE_UNDEF );

		/* mark as unlocked */
		lock->state = MALI_FALSE;

		/* signal the condition, only wake a single thread */
		pthread_cond_signal(&lock->condition);

		pthread_mutex_unlock(&lock->l.mutex);
		break;

	case _MALI_OSU_LOCKFLAG_SPINLOCK:
		mali_spinlock_signal( &lock->l.spinlock );
		break;

	default:
		MALI_DEBUG_ERROR( ("lock has incorrect flags==%.8X\n", lock->flags) );
		break;
	}
}

void _mali_osu_lock_term( _mali_osu_lock_t *lock )
{
	int call_result;
	MALI_DEBUG_ASSERT_POINTER( lock );

	/** Debug lock checking: */
	/* Lock is signalled on terminate - not a guarantee, since we could be locked immediately beforehand */
	MALI_DEBUG_ASSERT( _MALI_OSU_LOCKMODE_UNDEF == lock->locked_as,
					   ("cannot terminate held lock\n") );

	if ( lock->flags & _MALI_OSU_LOCKFLAG_SPINLOCK )
	{
		mali_spinlock_destroy( &lock->l.spinlock );
	}
	else {
		call_result = pthread_mutex_destroy( &lock->l.mutex );
		MALI_DEBUG_ASSERT( 0 == call_result,
				("Incorrect mutex use detected: pthread_mutex_destroy call failed with error code %d\n", call_result) );

		/* Destroy extra state for ANY_UNLOCK type osu_locks */
		if ( lock->flags & _MALI_OSU_LOCKFLAG_ANYUNLOCK )
		{
			MALI_DEBUG_ASSERT( MALI_FALSE == lock->state, ("terminate called on locked object %p\n", lock));
			call_result = pthread_cond_destroy(&lock->condition);
			MALI_DEBUG_ASSERT( 0 == call_result,
					("Incorrect condition-variable use detected: pthread_cond_destroy call failed with error code %d\n", call_result) );
		}

		MALI_IGNORE(call_result);
	}

	_mali_sys_free( lock );
}

_mali_osu_lock_t *_mali_osu_lock_static( u32 nr )
{
    MALI_DEBUG_ASSERT( nr < MALI_OSU_STATIC_LOCK_COUNT,
                       ("provided static lock index (%d) out of bounds (0 < nr < %d)\n", nr, MALI_OSU_STATIC_LOCK_COUNT) );
    return &_mali_osu_static_locks[nr];
}
