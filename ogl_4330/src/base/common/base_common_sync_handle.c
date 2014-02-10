/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <base/mali_memory.h>
#include <base/mali_debug.h>
#include <base/mali_runtime.h>
#include <base/mali_macros.h>
#include <base/arch/base_arch_runtime.h>
#include "base_common_sync_handle.h"

typedef struct mali_sync_struct
{
	mali_mutex_handle       completionlock;
	u32                     references;
	mali_bool               started;
	mali_mem_handle         cleanuplist;
	mali_base_wait_handle   waithandle;
	mali_sync_cb            callback;
	void *                  callback_arg;
	mali_base_ctx_handle    ctx;
	mali_bool               core_only;
	mali_sync_handle        parent_sync_object;
} mali_sync_struct;

MALI_STATIC void _mali_sync_handle_run_completion(mali_sync_struct * sync);

mali_sync_handle _mali_base_common_sync_handle_core_new(mali_base_ctx_handle ctx)
{
	mali_sync_struct * handle = NULL;

	MALI_DEBUG_ASSERT_HANDLE(ctx);

	handle = MALI_REINTERPRET_CAST(mali_sync_struct*)_mali_sys_calloc(1, sizeof(mali_sync_struct));

	if (NULL != handle)
	{
		/*
		The wait handle is allocated upon first requested.
		This works because the completion route (which triggers
		the wait handle) is never called before a sync handle
		has started. And after a start all requests for a wait
		handle will be denied.
		*/
		handle->waithandle  = NULL;

		handle->completionlock = _mali_sys_mutex_create();

		if (MALI_NO_HANDLE != handle->completionlock)
		{
			handle->core_only = MALI_TRUE;
			handle->started = MALI_FALSE;
			handle->references = 0;
			handle->cleanuplist = NULL;
			handle->ctx = ctx;
			handle->parent_sync_object = MALI_NO_HANDLE;

			return (mali_sync_handle)handle;
		}
		_mali_sys_free(handle);
	}
	return MALI_NO_HANDLE;
}

mali_sync_handle _mali_base_common_sync_handle_new(mali_base_ctx_handle ctx)
{
	if (MALI_ERR_NO_ERROR == _mali_mem_open(ctx))
	{
		mali_sync_struct * sync;
		sync = MALI_REINTERPRET_CAST(mali_sync_struct *)_mali_base_common_sync_handle_core_new(ctx);
		if (NULL != sync)
		{
			sync->core_only = MALI_FALSE;
			return (mali_sync_handle)sync;
		}
		_mali_mem_close(ctx);
	}
	return MALI_NO_HANDLE;
}

void _mali_base_common_sync_handle_cb_function_set(mali_sync_handle handle, mali_sync_cb cbfunc, void * cbarg)
{
	mali_sync_struct * sync = MALI_REINTERPRET_CAST((mali_sync_struct*)handle);

	MALI_DEBUG_ASSERT_POINTER(sync);
	MALI_DEBUG_ASSERT( MALI_FALSE == sync->started, ("Setting callback after sync handle start is not supported"));

	sync->callback = cbfunc;
	sync->callback_arg = cbarg;
}

void _mali_base_common_sync_handle_add_mem_to_free_list(mali_sync_handle handle, mali_mem_handle mem)
{
	mali_sync_struct * sync = MALI_REINTERPRET_CAST((mali_sync_struct*)handle);
	MALI_DEBUG_ASSERT_POINTER(sync);
	MALI_DEBUG_ASSERT( MALI_FALSE == sync->started, ("Adding memory to sync handle auto-free list after calling start is not supported"));
	MALI_DEBUG_ASSERT( MALI_FALSE == sync->core_only, ("Memory free list not supported on the core sync handles, use the full version instead"));
	MALI_DEBUG_ASSERT_POINTER(mem);

	if (sync->cleanuplist) _mali_mem_list_insert_after(sync->cleanuplist, mem);
	else sync->cleanuplist = mem;
}

mali_base_wait_handle _mali_base_common_sync_handle_get_wait_handle(mali_sync_handle handle)
{
	mali_sync_struct * sync = MALI_REINTERPRET_CAST((mali_sync_struct*)handle);
	if (NULL == sync) return NULL;

	MALI_DEBUG_ASSERT( MALI_FALSE == sync->started, ("Get wait handle called on started sync object 0x%X", sync));

	if ( MALI_TRUE == sync->started) return NULL;

	/*
		Create the wait handle upon first request.
		This will prevent wait handles being created for
		sync objects which is never waited upon.
	*/

	if (NULL == sync->waithandle) sync->waithandle = _mali_base_arch_sys_wait_handle_create();

	/* if _mali_base_arch_sys_wait_handle_create failed we will return NULL to the caller */

	return sync->waithandle;
}

void _mali_base_common_sync_handle_flush(mali_sync_handle handle)
{
	mali_sync_struct * sync = MALI_REINTERPRET_CAST((mali_sync_struct*)handle);

	MALI_DEBUG_ASSERT_POINTER(sync);
	MALI_DEBUG_ASSERT( MALI_FALSE == sync->started, ("Multiple starts of a sync handle detected"));

	_mali_sys_mutex_lock(sync->completionlock);

	sync->started = MALI_TRUE;

	if (0 == sync->references)
	{
		_mali_sync_handle_run_completion(sync);
		/*
			_mali_sync_handle_run_completion unlocked the mutex before destroying
			it along with the sync object itself.
			The sync handle is now off-limits.
		*/
	}
	else _mali_sys_mutex_unlock(sync->completionlock);
}

void _mali_base_common_sync_handle_register_reference(mali_sync_handle handle)
{
	mali_sync_struct * sync = MALI_REINTERPRET_CAST((mali_sync_struct*)handle);

	MALI_DEBUG_ASSERT_POINTER(sync);
	MALI_DEBUG_ASSERT(( MALI_FALSE == sync->started)||sync->references>0 , ("Registering a reference after sync handle start is not supported"));

	_mali_sys_mutex_lock(sync->completionlock);
	++sync->references;
	_mali_sys_mutex_unlock(sync->completionlock);
}


void _mali_base_common_sync_handle_release_reference(mali_sync_handle handle)
{
	mali_sync_struct * sync = MALI_REINTERPRET_CAST((mali_sync_struct*)handle);

	MALI_DEBUG_ASSERT_POINTER(sync);
	MALI_DEBUG_ASSERT( sync->references > 0, ("Illegal ref.count of sync_handle=%p ref.count: %d", sync, sync->references ) );

	_mali_sys_mutex_lock(sync->completionlock);

	--sync->references;

	if ( (0 == sync->references) && ( MALI_TRUE == sync->started) )
	{
		_mali_sync_handle_run_completion(sync);
		/*
			_mali_sync_handle_run_completion unlocked the mutex before
			destroying it along with the sync object itself.
			The sync handle is now off-limits.
		*/
	}
	else _mali_sys_mutex_unlock(sync->completionlock);
}

MALI_STATIC void _mali_sync_handle_run_completion(mali_sync_struct * sync)
{
	mali_base_ctx_handle    ctx;
	mali_mem_handle         cleanuplist;
	mali_base_wait_handle   waithandle;
	mali_sync_cb            callback;
	void *                  callback_arg;
	mali_sync_handle        parent_sync_object;
	mali_bool               core_only;

	MALI_DEBUG_ASSERT_POINTER(sync);
	MALI_DEBUG_ASSERT( MALI_TRUE == sync->started, ("Sync handle completion called on non-started sync object"));
	MALI_DEBUG_ASSERT(0 == sync->references, ("Sync handle completion called while jobs are still running"));

	ctx = sync->ctx;
	cleanuplist = sync->cleanuplist;
	waithandle = sync->waithandle;
	callback = sync->callback;
	callback_arg = sync->callback_arg;
	parent_sync_object = sync->parent_sync_object;
	core_only = sync->core_only;

	/* destroy the sync member objects */
	_mali_sys_mutex_unlock(sync->completionlock);
	_mali_sys_mutex_destroy(sync->completionlock);

	/*
	If we have a wait handle the waitee will destroy it.
	If a requested wait handle is never waited upon we will leak it.

	The only way to work around this would be to implement a
	trigger-and-wait-with-timeout logic in the wait handle system.
	This would degrade performace too much to be feasible.
	*/

#if defined( DEBUG )
	_mali_sys_memset( sync, 0, sizeof( *sync ) );
#endif

	/* destroy the sync object itself */
	_mali_sys_free(sync);

	/* trigger the wait handle if enabled */
	if (MALI_NO_HANDLE != waithandle) _mali_base_arch_sys_wait_handle_trigger(waithandle);

	/* perform registered callback, if one is registered */
	if (NULL != callback) (*callback)(ctx, callback_arg);

	/* trigger parent sync object if attached */
	if (MALI_NO_HANDLE != parent_sync_object) _mali_base_common_sync_handle_release_reference(parent_sync_object);

	/* cleanup memory */
	if ( MALI_FALSE == core_only)
	{
		if (NULL != cleanuplist) _mali_mem_list_free(cleanuplist);
		_mali_mem_close(ctx);
	}
}

void _mali_base_common_sync_handle_add_to_sync_handle( mali_sync_handle sync_object_handle, mali_sync_handle monitored_handle)
{
	mali_sync_struct * sync, * monitored;

	MALI_DEBUG_ASSERT_HANDLE(sync_object_handle);
	MALI_DEBUG_ASSERT_HANDLE(monitored_handle);

	monitored = MALI_REINTERPRET_CAST(mali_sync_struct*)monitored_handle;
	sync = MALI_REINTERPRET_CAST(mali_sync_struct*)sync_object_handle;

	MALI_DEBUG_ASSERT(MALI_NO_HANDLE == monitored->parent_sync_object, ("The sync object is already monitored by another sync object. Can only be monitored by a single sync object"));
	MALI_DEBUG_ASSERT( MALI_FALSE == sync->started, ("Registering a new dependency for a sync object after having called start is not supported"));
	MALI_IGNORE(sync); /* unused besides the assert */
	MALI_DEBUG_ASSERT( MALI_FALSE == monitored->started, ("Registering a new dependency for a sync object after having called start is not supported"));
	monitored->parent_sync_object = sync_object_handle; /* save ref to the parent for releasing later */
	_mali_base_common_sync_handle_register_reference(sync_object_handle); /* add the reference */
}
