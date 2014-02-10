/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include "mali_fence_sync.h"

#if MALI_EXTERNAL_SYNC != 1
#error "This file should only be built if MALI_EXTERNAL_SYNC is enabled"
#endif

struct mali_fence_sync
{

	mali_fence_handle accumulated_sync;   /* all flushes within this frame that have been started */
	u32               pending_jobs;       /* the number of flushes not yet started */

	mali_mutex_handle mutex;              /* protects the above two members. Must be taken before accessing them */	
	mali_lock_handle  semaphore;          /* held while pending jobs > 0. For optimized waiting. */	

};



struct mali_fence_sync* _mali_fence_sync_alloc( void )
{
	struct mali_fence_sync* retval = _mali_sys_malloc(sizeof(struct mali_fence_sync));
	if(!retval) return NULL;

	retval->accumulated_sync = MALI_FENCE_INVALID_HANDLE;
	retval->pending_jobs = 0;
	retval->mutex = _mali_sys_mutex_create();
	if(retval->mutex == MALI_NO_HANDLE) 
	{
		_mali_sys_free(retval);
		return NULL;
	}
	retval->semaphore = _mali_sys_lock_create();
	if(retval->semaphore == MALI_NO_HANDLE) 
	{
		_mali_sys_mutex_destroy( retval->mutex );	
		_mali_sys_free(retval);
		return NULL;
	}

	return retval;
}

void _mali_fence_sync_free( struct mali_fence_sync* sync )
{
	MALI_DEBUG_ASSERT_POINTER(sync);
	MALI_DEBUG_ASSERT(sync->pending_jobs == 0, ("No pending jobs can exist before the object is freed"));

	if(MALI_FENCE_INVALID_HANDLE != sync->accumulated_sync)
	{
		mali_fence_release( sync->accumulated_sync );
	}

	_mali_sys_mutex_destroy( sync->mutex );	
	_mali_sys_lock_destroy( sync->semaphore );	
	_mali_sys_free(sync);
}

MALI_STATIC void _mali_fence_sync_lock( struct mali_fence_sync* sync )
{
	MALI_DEBUG_ASSERT_POINTER(sync);
	_mali_sys_mutex_lock( sync->mutex );	
}

MALI_STATIC void _mali_fence_sync_unlock( struct mali_fence_sync* sync )
{
	MALI_DEBUG_ASSERT_POINTER(sync);
	_mali_sys_mutex_unlock( sync->mutex );	
}

void _mali_fence_sync_notify_job_create( struct mali_fence_sync* sync )
{
	MALI_DEBUG_ASSERT_POINTER(sync);
	_mali_fence_sync_lock(sync);
	if(sync->pending_jobs == 0) 
	{
	    _mali_sys_lock_lock( sync->semaphore );
	}
	sync->pending_jobs++;
	_mali_fence_sync_unlock(sync);
}

void _mali_fence_sync_notify_job_start( struct mali_fence_sync* sync, mali_bool success, mali_fence_handle fence)
{
	mali_fence_handle merged_fence;
	MALI_DEBUG_ASSERT_POINTER(sync);
	_mali_fence_sync_lock(sync);

	MALI_DEBUG_ASSERT( success  || !(fence != MALI_FENCE_INVALID_HANDLE), ("if the job failed, the fence should be MALI_FENCE_INVALID_HANDLE"));
	MALI_DEBUG_ASSERT( !success || (fence != MALI_FENCE_INVALID_HANDLE), ("if the job succeeded, the fence should not be MALI_FENCE_INVALID_HANDLE"));
	MALI_DEBUG_ASSERT( sync->pending_jobs > 0, ("starting a job that was never created") );

	sync->pending_jobs--;
	if(sync->pending_jobs == 0) 
	{
	    _mali_sys_lock_unlock( sync->semaphore );
	}

	/* merge together the sync handles */
	if(MALI_FENCE_INVALID_HANDLE != fence)
	{
		/* we need to dup the fence, since we don't have ownership and merge will invalidate it */
		fence = mali_fence_dup(fence);
		MALI_DEBUG_ASSERT(fence != MALI_FENCE_INVALID_HANDLE, ("Failed to dup fence!"));

		if( sync->accumulated_sync == MALI_FENCE_INVALID_HANDLE )
		{
			sync->accumulated_sync = fence;
		}
		else
		{
			merged_fence = mali_fence_merge("ACCUMULATED FENCE SYNC HANDLE", sync->accumulated_sync, fence); 

			if(merged_fence != MALI_FENCE_INVALID_HANDLE)
			{
				sync->accumulated_sync = merged_fence;
			}
			else
			{
				/* we are not able to handle this failure decently ( actually, the error should never be hit )
				 * close everything and reset sync->accuumulated_sync to initial state
				 */
				MALI_DEBUG_ASSERT( merged_fence != MALI_FENCE_INVALID_HANDLE, ("Failed to merge fence!"));
				mali_fence_release( sync->accumulated_sync );
				mali_fence_release( fence );
				sync->accumulated_sync = MALI_FENCE_INVALID_HANDLE;
			}
		}
	}

	_mali_fence_sync_unlock(sync);

}

mali_fence_handle _mali_fence_sync_get_fence_handle( struct mali_fence_sync* sync )
{
	mali_fence_handle retval;
	MALI_DEBUG_ASSERT_POINTER(sync);

	/* wait until all pending jobs are 0, which is done by taking and releasing the semaphore */
	_mali_sys_lock_lock( sync->semaphore );
	_mali_sys_lock_unlock( sync->semaphore );

	_mali_fence_sync_lock( sync );
	
	if( sync->accumulated_sync == MALI_FENCE_INVALID_HANDLE )
	{
		_mali_fence_sync_unlock(sync);
		return MALI_FENCE_INVALID_HANDLE;
	}
	retval = mali_fence_dup(sync->accumulated_sync);	
	
	_mali_fence_sync_unlock(sync);

	return retval;
}
