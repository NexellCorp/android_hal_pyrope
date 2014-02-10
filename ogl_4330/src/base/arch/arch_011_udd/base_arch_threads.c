/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <base/arch/base_arch_runtime.h>
#include "mali_osu.h"

/*
 * Internal wait object struct.
 * The lock tracks the state while the ref_count tracks when we can free the object
 */
typedef struct
{
	_mali_osu_lock_t * lock;
	mali_atomic_int ref_count;
} mali_wait_object;

/*
 * Internal reference remove helper.
 * If it decrements the reference to 0 then it frees the objects involved
 */
MALI_STATIC void _wait_handle_ref_release(mali_wait_object * wait_object);

/**** Spinlock functions ****/

/**
 * Create a spinlock
 */
mali_spinlock_handle _mali_base_arch_sys_spinlock_create(void)
{
	return MALI_REINTERPRET_CAST(mali_spinlock_handle)_mali_osu_lock_init(_MALI_OSU_LOCKFLAG_SPINLOCK, 0, 0);
}

/**
 * Take the given spinlock
 */
void _mali_base_arch_sys_spinlock_lock(mali_spinlock_handle spinlock)
{
	_mali_osu_errcode_t err;

	err = _mali_osu_lock_wait( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)spinlock, _MALI_OSU_LOCKMODE_RW );

	MALI_DEBUG_ASSERT( _MALI_OSU_ERR_OK == err,
					   ("Incorrect spinlock use detected: _mali_osu_lock_wait failed with error code %.8X\n", err));

	MALI_IGNORE(err);
}

/**
 * Try to take the given spinlock -- returns immediately.
 */
mali_err_code _mali_base_arch_sys_spinlock_try_lock(mali_spinlock_handle spinlock)
{
	_mali_osu_errcode_t err;
	err = _mali_osu_lock_trywait( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)spinlock, _MALI_OSU_LOCKMODE_RW );

	/* Return MALI_ERR_FUNCTION_FAILED if couldn't lock, and MALI_ERR_NO_ERROR otherwise */
	MALI_ERROR( _MALI_OSU_TRANSLATE_ERROR(err) );

	return MALI_FALSE;
}

/**
 * Release the given spinlock
 */
void _mali_base_arch_sys_spinlock_unlock(mali_spinlock_handle spinlock)
{
	_mali_osu_lock_signal( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)spinlock, _MALI_OSU_LOCKMODE_RW );
}

/**
 * Destroy the given spinlock
 */
void _mali_base_arch_sys_spinlock_destroy(mali_spinlock_handle spinlock)
{
	_mali_osu_lock_term( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)spinlock );
}

/**** Mutex functions ****/

mali_err_code _mali_base_arch_sys_mutex_auto_init(volatile mali_mutex_handle * pHandle)
{
	_mali_osu_lock_t **pplock;
	_mali_osu_errcode_t err;

	pplock = MALI_REINTERPRET_CAST(_mali_osu_lock_t **)pHandle;

	/* runtime assert that the above cast works, since pointer-to-struct-pointer
	 * types need not be interchangable */
	MALI_DEBUG_ASSERT( pHandle == (mali_mutex_handle *)pplock,
					   ("cast mali_mutex_handle* to _mali_osu_lock_t** failed\n"));

	err = _mali_osu_lock_auto_init( pplock, (_mali_osu_lock_flags_t)0, 0, 0 );

	MALI_ERROR( _MALI_OSU_TRANSLATE_ERROR(err) );
}

mali_mutex_handle _mali_base_arch_sys_mutex_create(void)
{
	return MALI_REINTERPRET_CAST(mali_mutex_handle) _mali_osu_lock_init( (_mali_osu_lock_flags_t)0, 0, 0 );
}

void _mali_base_arch_sys_mutex_destroy(mali_mutex_handle mutex)
{
	_mali_osu_lock_term( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)mutex );
}

void _mali_base_arch_sys_mutex_lock(mali_mutex_handle mutex)
{
	_mali_osu_errcode_t err;

	err = _mali_osu_lock_wait( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)mutex, _MALI_OSU_LOCKMODE_RW );
	MALI_DEBUG_ASSERT( _MALI_OSU_ERR_OK == err,
					   ("Incorrect mutex use detected: _mali_osu_lock_wait failed with error code %.8X\n", err));
	MALI_IGNORE(err);
}

mali_err_code _mali_base_arch_sys_mutex_try_lock(mali_mutex_handle mutex)
{
	_mali_osu_errcode_t err;
	err = _mali_osu_lock_trywait( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)mutex, _MALI_OSU_LOCKMODE_RW );

	/* Return MALI_ERR_FUNCTION_FAILED if couldn't lock, and MALI_ERR_NO_ERROR otherwise */
	MALI_ERROR( _MALI_OSU_TRANSLATE_ERROR(err) );
}

void _mali_base_arch_sys_mutex_unlock(mali_mutex_handle mutex)
{
	_mali_osu_lock_signal( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)mutex, _MALI_OSU_LOCKMODE_RW );
}

mali_mutex_handle _mali_base_arch_sys_mutex_static(mali_static_mutex id)
{
	if ( !(id < MALI_STATIC_MUTEX_MAX) )
	{
		return NULL;
	}
	return MALI_REINTERPRET_CAST(mali_mutex_handle) _mali_osu_lock_static( MALI_REINTERPRET_CAST(u32)id );
}

/**** Threads ****/

MALI_STATIC _mali_osu_thread_key_t _mali_base_arch_common_translate_thread_key( mali_thread_keys common_key )
{
	switch( common_key )
	{
	case MALI_THREAD_KEY_EGL_CONTEXT: return _MALI_OSU_THREAD_KEY_EGL_CONTEXT;
	case MALI_THREAD_KEY_INSTRUMENTED_ACTIVE_FRAME: return _MALI_OSU_THREAD_KEY_INSTRUMENTED_ACTIVE_FRAME;
	case MALI_THREAD_KEY_INSTRUMENTED_DATA: return _MALI_OSU_THREAD_KEY_INSTRUMENTED_DATA;
	case MALI_THREAD_KEY_DUMP_CALLBACK: return _MALI_OSU_THREAD_KEY_DUMP_CALLBACK;
	case MALI_THREAD_KEY_GLES_CONTEXT: return _MALI_OSU_THREAD_KEY_GLES_CONTEXT;
	case MALI_THREAD_KEY_VG_CONTEXT: return _MALI_OSU_THREAD_KEY_VG_CONTEXT;
	case MALI_THREAD_KEY_MALI_EGL_IMAGE: return _MALI_OSU_THREAD_KEY_MALI_EGL_IMAGE;

		default:
			MALI_DEBUG_ERROR( ("Unknown Mali Thread Key: %.8X\n", common_key) );
	}
	return _MALI_OSU_THREAD_KEY_MAX;
}

mali_err_code _mali_base_arch_sys_thread_key_set_data(mali_thread_keys key, void* value)
{
	_mali_osu_thread_key_t osu_key;
	MALI_CHECK(key < MALI_THREAD_KEY_MAX, MALI_ERR_FUNCTION_FAILED);

	osu_key = _mali_base_arch_common_translate_thread_key( key );

	MALI_ERROR( _MALI_OSU_TRANSLATE_ERROR(_mali_osu_thread_key_set_data( osu_key, value )) );
}

void* _mali_base_arch_sys_thread_key_get_data(mali_thread_keys key)
{
	_mali_osu_thread_key_t osu_key;
	if (key >= MALI_THREAD_KEY_MAX)
	{
		MALI_DEBUG_ERROR(("Invalid thread key %d specified, only values in range 0 - %d are acceptable", key, MALI_THREAD_KEY_MAX - 1));
		return NULL;
	}

	osu_key = _mali_base_arch_common_translate_thread_key( key );

	return _mali_osu_thread_key_get_data( osu_key );
}


MALI_STATIC void _wait_handle_ref_release(mali_wait_object * wait_object)
{
	MALI_DEBUG_ASSERT_POINTER(wait_object);
	if (0 == _mali_sys_atomic_dec_and_return(&wait_object->ref_count))
	{
		/* the lock is already unlocked when we get here (by design) */
		_mali_osu_lock_term(wait_object->lock);
		wait_object->lock = NULL;
		_mali_sys_free(wait_object);
	}
}

mali_base_wait_handle _mali_base_arch_sys_wait_handle_create(void)
{
	mali_wait_object * wait_object;
	_mali_osu_errcode_t err;

	wait_object = _mali_sys_malloc(sizeof(mali_wait_object));
	if (NULL == wait_object)
	{
		return MALI_NO_HANDLE;
	}

	/* Wait handles are implemented on top of a 'ANYUNLOCK' lock, which are initially
	 * locked. */

	wait_object->lock = _mali_osu_lock_init( _MALI_OSU_LOCKFLAG_ANYUNLOCK, 0, 0 );

	if (NULL == wait_object->lock)
	{
		_mali_sys_free(wait_object);
		return MALI_NO_HANDLE;
	}

	/* A refcount is used to know when to release the objects.
	 * As a wait is composed of two objects (one waiting, one signaling) it starts with the value 2
	 */
	_mali_sys_atomic_initialize(&wait_object->ref_count, 2);

	/* It is a programming error for this to fail */
	err = _mali_osu_lock_wait( wait_object->lock, _MALI_OSU_LOCKMODE_RW );
	MALI_DEBUG_ASSERT( _MALI_OSU_ERR_OK == err,
					   ("Failed to lock wait handle after creation, err=%.8X\n", err) );

	MALI_IGNORE( err );

	return MALI_REINTERPRET_CAST(mali_base_wait_handle)wait_object;

}

void _mali_base_arch_sys_wait_handle_wait(mali_base_wait_handle handle)
{
	mali_wait_object *wait_object = MALI_REINTERPRET_CAST(mali_wait_object*)handle;
	_mali_osu_errcode_t err;

	err = _mali_osu_lock_wait( wait_object->lock, _MALI_OSU_LOCKMODE_RW );
	MALI_DEBUG_ASSERT( _MALI_OSU_ERR_OK == err,
					   ("Failed to lock wait handle, err=%.8X\n", err) );
	MALI_IGNORE( err );

	_mali_osu_lock_signal( wait_object->lock, _MALI_OSU_LOCKMODE_RW );

	_wait_handle_ref_release(wait_object);
}

mali_err_code _mali_base_arch_sys_wait_handle_timed_wait(mali_base_wait_handle handle, u64 timeout)
{
	mali_wait_object *wait_object = MALI_REINTERPRET_CAST(mali_wait_object*)handle;
	_mali_osu_errcode_t err;

	err = _mali_osu_lock_timed_wait( wait_object->lock, _MALI_OSU_LOCKMODE_RW, timeout );

	if (err == _MALI_OSU_ERR_TIMEOUT)
	{
		/* timeout */
		return MALI_ERR_TIMEOUT;
	}

	MALI_DEBUG_ASSERT( _MALI_OSU_ERR_OK == err,
					   ("Failed to lock wait handle, err=%.8X\n", err) );
	MALI_IGNORE( err );

	_mali_osu_lock_signal( wait_object->lock, _MALI_OSU_LOCKMODE_RW );

	_wait_handle_ref_release(wait_object);

	return MALI_ERR_NO_ERROR;
}

void _mali_base_arch_sys_wait_handle_trigger(mali_base_wait_handle handle)
{
	mali_wait_object *wait_object = MALI_REINTERPRET_CAST(mali_wait_object*)handle;
	_mali_osu_lock_signal( wait_object->lock, _MALI_OSU_LOCKMODE_RW );
	_wait_handle_ref_release(wait_object);
}

void _mali_base_arch_sys_wait_handle_abandon(mali_base_wait_handle handle)
{
	mali_wait_object *wait_object = MALI_REINTERPRET_CAST(mali_wait_object*)handle;
	_wait_handle_ref_release(wait_object);
}

u32 _mali_base_arch_sys_thread_get_current(void)
{
	return _mali_osu_thread_get_current();
}

mali_lock_handle _mali_base_arch_sys_lock_create(void)
{
	_mali_osu_lock_t *lock;
	/* Mali Locks are OSU locks that are ANYUNLOCK-able */
	lock = _mali_osu_lock_init( _MALI_OSU_LOCKFLAG_ANYUNLOCK, 0, 0 );

	return MALI_REINTERPRET_CAST(mali_lock_handle)lock;
}

mali_err_code _mali_base_arch_sys_lock_auto_init(volatile mali_lock_handle * pHandle)
{
	_mali_osu_lock_t **pplock;
	_mali_osu_errcode_t err;

	pplock = MALI_REINTERPRET_CAST(_mali_osu_lock_t **)pHandle;

	/* runtime assert that the above cast works, since pointer-to-struct-pointer
	 * types need not be interchangable */
	MALI_DEBUG_ASSERT( pHandle == (mali_lock_handle *)pplock,
					   ("cast mali_lock_handle* to _mali_osu_lock_t** failed\n"));

	err = _mali_osu_lock_auto_init( pplock, _MALI_OSU_LOCKFLAG_ANYUNLOCK, 0, 0 );

	MALI_ERROR( _MALI_OSU_TRANSLATE_ERROR(err) );
}

void _mali_base_arch_sys_lock_destroy(mali_lock_handle lock)
{
	_mali_osu_lock_term( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)lock );
}

mali_err_code _mali_base_arch_sys_lock_timed_lock(mali_lock_handle lock, u64 timeout)
{
	_mali_osu_errcode_t err;

	err = _mali_osu_lock_timed_wait( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)lock, _MALI_OSU_LOCKMODE_RW, timeout);
	if (_MALI_OSU_ERR_TIMEOUT == err)
	{
		return MALI_ERR_TIMEOUT;
	}
	MALI_DEBUG_ASSERT( _MALI_OSU_ERR_OK == err,
					   ("Incorrect mutex use detected: _mali_osu_lock_wait failed with error code %.8X\n", err));

	MALI_SUCCESS;
}

void _mali_base_arch_sys_lock_lock(mali_lock_handle lock)
{
	_mali_osu_errcode_t err;

	err = _mali_osu_lock_wait( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)lock, _MALI_OSU_LOCKMODE_RW );
	MALI_DEBUG_ASSERT( _MALI_OSU_ERR_OK == err,
					   ("Incorrect mutex use detected: _mali_osu_lock_wait failed with error code %.8X\n", err));
	MALI_IGNORE(err);
}

mali_err_code _mali_base_arch_sys_lock_try_lock(mali_lock_handle lock)
{
	_mali_osu_errcode_t err;
	err = _mali_osu_lock_trywait( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)lock, _MALI_OSU_LOCKMODE_RW );

	/* Return MALI_ERR_FUNCTION_FAILED if couldn't lock, and MALI_ERR_NO_ERROR otherwise */
	MALI_ERROR( _MALI_OSU_TRANSLATE_ERROR(err) );
}

void _mali_base_arch_sys_lock_unlock(mali_lock_handle lock)
{
	_mali_osu_lock_signal( MALI_REINTERPRET_CAST(_mali_osu_lock_t *)lock, _MALI_OSU_LOCKMODE_RW );
}

