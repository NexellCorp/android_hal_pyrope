/*
 * This confidential and proprietary software may be used only as
 * authorized by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "m200_gp_frame_builder_inlines.h"
#include "m200_gp_frame_builder_dependency.h"
#include "m200_frame.h"

mali_internal_frame* _mali_frame_alloc( mali_base_ctx_handle base_ctx,
		mali_frame_builder *frame_builder)
{
	mali_bool init_succeeded = MALI_TRUE;
	mali_err_code err;

	mali_internal_frame *frame = (mali_internal_frame *) _mali_sys_calloc( 1, sizeof( mali_internal_frame ) );
	if( frame == NULL )
	{
		return NULL;
	}

	frame->pool_mapped = MALI_FALSE;
	err = _mali_mem_pool_init(&frame->frame_pool, base_ctx);
	if(err != MALI_ERR_NO_ERROR)
	{
		_mali_sys_free(frame);
		return NULL;
	}

#if MALI_INSTRUMENTED
	frame->instrumented_frame = NULL;
#endif

	frame->frame_builder = frame_builder;

	/* initialize the error state pointer */
	_mali_sys_atomic_set( &(frame->completion_status), (u32)MALI_JOB_STATUS_END_SUCCESS );

	/* the tile list allocations depend on screen size, so their initial allocation is postponed until
	 * _mali_frame_builder_use()
	 */
	frame->tilelists            = NULL;
	frame->plbu_heap_held       = NULL;
	frame->vpbox.left = frame->vpbox.right = frame->vpbox.top = frame->vpbox.bottom = 0.f;
	frame->cow_flavour          = FRAME_COW_REALLOC;

	/* create the mutex for protecting the frame state */
	frame->mutex = _mali_sys_mutex_create();
	if( MALI_NO_HANDLE == frame->mutex ) init_succeeded = MALI_FALSE;

	frame->lock = _mali_sys_lock_create();
	if( MALI_NO_HANDLE == frame->lock ) init_succeeded = MALI_FALSE;

#if MALI_SW_COUNTERS_ENABLED
	/*
	 * Allocate memory for the software counter store.  
	 */
	frame->sw_counters = _mali_sw_counters_alloc();
	if(NULL == frame->sw_counters) init_succeeded = MALI_FALSE;
#endif

	frame->ds_consumer_pp_render =  mali_ds_consumer_allocate(base_ctx, frame, _mali_frame_builder_frame_dependency_activated, _mali_frame_builder_frame_dependency_release );
	if( MALI_NO_HANDLE == frame->ds_consumer_pp_render )
	{
		init_succeeded = MALI_FALSE;
	} else {
		mali_ds_consumer_set_callback_replace_resource(frame->ds_consumer_pp_render, _mali_frame_builder_surface_do_copy_on_write);
	}

	/* PP job allocated during flush */
	frame->curr_pp_split_count = frame_builder->curr_pp_split_count;
	frame->pp_job = MALI_NO_HANDLE;

	/* allocate the frame's gp job. */
	frame->gp_job = _mali_gp_job_new( base_ctx );
	if( MALI_NO_HANDLE == frame->gp_job ) init_succeeded = MALI_FALSE;

	frame->frame_mem       = MALI_NO_HANDLE;
	frame->gp_job_mem_list = MALI_NO_HANDLE;

	/* fragment shader stack parameters */
	frame->fs_stack.start = 0;
	frame->fs_stack.grow  = 0;

	/* fragment shader stack allocated during render flush */
	frame->fs_stack.fs_stack_mem = MALI_NO_HANDLE;

	/* create an array for user-supplied callbacks */
	frame->callback_list = NULL;
	err = _mali_frame_callback_list_set_room(frame, INITIAL_LIST_ROOM);
	if (MALI_ERR_NO_ERROR != err) init_succeeded = MALI_FALSE;
	frame->callback_list_size = 0;
	frame->have_non_deferred_callbacks = MALI_FALSE;

	frame->ds_consumer_gp_job = mali_ds_consumer_allocate(
			base_ctx,
			frame,
			_mali_frame_builder_activate_gp_consumer_callback /*Activation function */,
			NULL
			);

	if ( MALI_NO_HANDLE == frame->ds_consumer_gp_job )
	{
		init_succeeded = MALI_FALSE;
	}
	frame->current_plbu_ds_resource = MALI_NO_HANDLE;

	frame->gp_context_stack = _mali_mem_alloc( base_ctx, GP_CONTEXT_STACK_SIZE, 64, MALI_GP_READ | MALI_GP_WRITE );
	if( MALI_NO_HANDLE == frame->gp_context_stack ) init_succeeded = MALI_FALSE;

	frame->reset_on_finish              = MALI_FALSE;
	frame->plbu_heap_reset_on_job_start = MALI_FALSE;
	frame->readback_first_drawcall	    = MALI_TRUE;
	frame->state                        = FRAME_UNMODIFIED;
	frame->num_flushes_since_reset      = 0;
	frame->cb_func_lock_output          = NULL;
	frame->cb_func_acquire_output       = NULL;
	frame->cb_func_complete_output      = NULL;

#if MALI_EXTERNAL_SYNC
	frame->cb_func_pp_job_start         = NULL;

	frame->stream = mali_stream_create(base_ctx);
	if (NULL == frame->stream) init_succeeded = MALI_FALSE;

#endif /* MALI_EXTERNAL_SYNC */

	frame->frame_id = MALI_BAD_FRAME_ID;

	_mali_sys_memset(&frame->order_synch, 0, sizeof(frame->order_synch));

	/* create the mutex for serializing the callbacks and the main thread */
	frame->order_synch.frame_order_mutex = _mali_sys_mutex_create();
	if( MALI_NO_HANDLE == frame->order_synch.frame_order_mutex ) init_succeeded = MALI_FALSE;

	/* create surfacetracking block */
	frame->surfacetracking = _mali_surfacetracking_alloc();
	if( NULL == frame->surfacetracking) init_succeeded = MALI_FALSE;

	if(init_succeeded) return frame;
	else
	{
		_mali_frame_free(frame);
		return NULL;
	}
}

void _mali_frame_free( mali_internal_frame *frame )
{
	MALI_DEBUG_ASSERT_POINTER( frame );

	/* If we are here, then we are here for only two reasons:
	   - cleanup in case we were unable to build a complete frame builder => No rendering is possible
	   - cleanup in case we destroy a valid framebuilder => we have to wait for _all_ frames to finish
	   rendering _before_ deleting a single one.
	   In both cases we should never be in the rendering state and no-one should be using this frame
	   from a different thread.
	 */
	MALI_DEBUG_ASSERT( frame->state != FRAME_RENDERING, ("Invalid frame state.") );
	if ( MALI_NO_HANDLE != frame->mutex )
	{
		/* This is a necessary but not sufficient condition to fulfill: If we cannot
		   lock the mutex, something really bad happened, e.g. someone is modifying the
		   frame when we are deleting a FB. */
		MALI_DEBUG_ASSERT( (MALI_ERR_NO_ERROR == _mali_sys_mutex_try_lock(frame->mutex)) &&
				(_mali_sys_mutex_unlock(frame->mutex),1) , ("frame->mutex is locked!") );
	}

	if ( MALI_NO_HANDLE != frame->ds_consumer_gp_job )
	{
		mali_ds_consumer_release_set_mode(frame->ds_consumer_gp_job, MALI_DS_RELEASE_ALL);
		mali_ds_consumer_set_callback_release( frame->ds_consumer_gp_job, NULL );
		mali_ds_consumer_release_all_connections( frame->ds_consumer_gp_job );
		mali_ds_consumer_free( frame->ds_consumer_gp_job );
	}

	if ( NULL != frame->ds_consumer_pp_render )
	{
		mali_ds_consumer_release_set_mode(frame->ds_consumer_pp_render, MALI_DS_RELEASE_ALL);
		mali_ds_consumer_set_callback_release( frame->ds_consumer_pp_render, NULL );
		mali_ds_consumer_release_all_connections( frame->ds_consumer_pp_render );
		mali_ds_consumer_free(frame->ds_consumer_pp_render);
	}

#if MALI_SW_COUNTERS_ENABLED
	if( NULL != frame->sw_counters)
	{
		_mali_sw_counters_free(frame->sw_counters);
		frame->sw_counters = NULL;
	}
#endif

	/* execute any pending callbacks before freeing the frame */
	_mali_frame_builder_frame_execute_callbacks( frame );
	_mali_mem_pool_destroy( &frame->frame_pool );

#if MALI_EXTERNAL_SYNC
	if (NULL != frame->stream)
	{
		mali_stream_destroy(frame->stream);
		frame->stream = NULL;
	}
#endif /* MALI_EXTERNAL_SYNC */

	/* free all allocated memory */
	_mali_mem_list_free( frame->frame_mem );
	_mali_mem_list_free( frame->gp_job_mem_list );
	_mali_mem_free( frame->fs_stack.fs_stack_mem );

	if(frame->tilelists)
	{
		_mali_tilelist_free(frame->tilelists);
		frame->tilelists = NULL;
	}

	_mali_mem_free( frame->gp_context_stack );

	/* free the projobs */
	_mali_projob_reset(frame);	

	/* free gp job */
	if( MALI_NO_HANDLE != frame->gp_job ) _mali_gp_job_free( frame->gp_job );

	/* free list of callbacks */
	if( frame->callback_list != NULL )
	{
		_mali_sys_free( frame->callback_list );
		frame->callback_list = NULL;
		frame->callback_list_room = 0;
		frame->callback_list_size = 0;
		frame->have_non_deferred_callbacks = MALI_FALSE;
	}

	if ( MALI_NO_HANDLE != frame->lock )
	{
		_mali_sys_lock_destroy( frame->lock );
		frame->lock = MALI_NO_HANDLE;
	}

	/* unlock and free the mutex */
	if( MALI_NO_HANDLE != frame->mutex )
	{
		_mali_sys_mutex_destroy( frame->mutex );
	}

	/* Free the serialization mutex */
	/* We should reach this line with order_synch.frame_order_mutex unlocked */
	if( MALI_NO_HANDLE != frame->order_synch.frame_order_mutex )
	{
		_mali_sys_mutex_destroy( frame->order_synch.frame_order_mutex );
		frame->order_synch.frame_order_mutex = MALI_NO_HANDLE;
	}

	/* free the surfacetracking */
	if( NULL != frame->surfacetracking) 
	{
		_mali_surfacetracking_free(frame->surfacetracking);
		frame->surfacetracking = NULL;
	}

	/* free the frame itself */
	_mali_sys_free( frame );
}

void _mali_frame_reset( mali_internal_frame *frame )
{
	MALI_DEBUG_ASSERT_POINTER( frame );

	MALI_DEBUG_ASSERT(frame->state != FRAME_RENDERING, ("You can't clean up a frame that is in the state rendering; doing so would pull memory out from beneath the HW. "));

#if MALI_EXTERNAL_SYNC
	frame->cb_func_pp_job_start = NULL;
#endif /* MALI_EXTERNAL_SYNC */

	/* execute any frame callbacks */
	_mali_frame_builder_frame_execute_callbacks( frame );

	/* Remove all dependencies on this frame */
	mali_ds_consumer_release_set_mode(frame->ds_consumer_pp_render, MALI_DS_RELEASE_ALL);
	mali_ds_consumer_release_all_connections(frame->ds_consumer_pp_render);

	_mali_sys_mutex_lock( frame->mutex );

	/* destroy the mem pool memory. It's now intialized, but unmapped and empty */
	_mali_mem_pool_destroy(&frame->frame_pool);
	frame->pool_mapped = MALI_FALSE;

	/* free user allocated memory */
	_mali_mem_list_free( frame->frame_mem );
	frame->frame_mem = MALI_NO_HANDLE;

	/* free GP related user allocated memory */
	_mali_mem_list_free( frame->gp_job_mem_list );
	frame->gp_job_mem_list = MALI_NO_HANDLE;

	_mali_mem_free( frame->fs_stack.fs_stack_mem );
	frame->fs_stack.fs_stack_mem = MALI_NO_HANDLE;

	/* reset gp job */
	if( MALI_NO_HANDLE != frame->gp_job ) _mali_gp_job_reset( frame->gp_job );

	/* reset the projobs */
	_mali_projob_reset(frame);	

	/* the frame is now empty */
	frame->state                     = FRAME_UNMODIFIED;
	frame->num_flushes_since_reset   = 0;
	frame->readback_first_drawcall	 = MALI_TRUE;

	frame->current_plbu_ds_resource = MALI_NO_HANDLE;
	frame->plbu_heap_held = NULL;

	frame->job_inited = 0;

#if MALI_SW_COUNTERS_ENABLED
	_mali_sw_counters_reset( frame->sw_counters );
#endif

	_mali_surfacetracking_reset(frame->surfacetracking, MALI_SURFACE_TRACKING_READ | MALI_SURFACE_TRACKING_WRITE);

	/* If we missed the PP activation callback due to an OOM during flush, we must cleanup a pending CoW too */
	if ( FRAME_COW_DEEP_COPY_PENDING == frame->cow_flavour )
	{
		_mali_shared_mem_ref_owner_deref(frame->cow_desc.src_mem_ref);
		frame->cow_flavour = FRAME_COW_REALLOC;
	}

	_mali_sys_mutex_unlock( frame->mutex );
}

void _mali_frame_wait( mali_internal_frame *frame )
{
	MALI_DEBUG_ASSERT_POINTER( frame );

	/* wait for all mali hw processing of the frame to finish and the callbacks to complete */
	_mali_frame_wait_and_take_mutex( frame );

	/* done waiting - unlock the mutex and return */
	_mali_sys_mutex_unlock( frame->mutex );
}

/**
 * Helper function which if necessary waits until the frame is not in the rendering state before returning.
 * @note frame->mutex must be locked before calling, and will be locked upon return
 */
MALI_STATIC void _mali_frame_ensure_not_rendering( mali_internal_frame *frame )
{
#if MALI_TIMELINE_PROFILING_ENABLED
	mali_bool waited = MALI_FALSE;
#endif

	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT( MALI_ERR_NO_ERROR != _mali_sys_mutex_try_lock(frame->mutex),
			("frame->mutex not locked by caller!") );

	/* NOTE: in the code below:
	 * between unlocking frame->lock and locking frame->mutex it is possible that another thread starts
	 * rendering. Thus we need to test the frame state in a loop.
	 */

#if MALI_TIMELINE_PROFILING_ENABLED
	if (frame->state == FRAME_RENDERING)
	{
		waited = MALI_TRUE;
		_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SUSPEND|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_FB_IFRAME_WAIT, 0, 0, frame->frame_builder->identifier, (u32)frame, 0);
	}
#endif

	do
	{
		/* Mali is still working on this frame, and the callback needs the mutex - unlock */
		_mali_sys_mutex_unlock(frame->mutex);

		/* wait for any in-flight rendering jobs to complete */
		_mali_sys_lock_lock(frame->lock);

		/* release the lock */
		_mali_sys_lock_unlock(frame->lock);

		/* now grab the mutex again (and keep it) */
		_mali_sys_mutex_lock(frame->mutex);
	} while ( frame->state == FRAME_RENDERING );

#if MALI_TIMELINE_PROFILING_ENABLED
	if (MALI_TRUE == waited)
	{
		_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_RESUME|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_FB_IFRAME_WAIT, 0, 0, frame->frame_builder->identifier, (u32)frame, 0);
	}
#endif

	MALI_DEBUG_ASSERT( NULL == frame->order_synch.release_on_finish , ("Err: The new and clean frame has an action pending.") );
	MALI_DEBUG_ASSERT( frame->state != FRAME_RENDERING, ("Invalid frame state") );

}

void _mali_frame_wait_and_take_mutex( mali_internal_frame *frame )
{
	MALI_DEBUG_ASSERT_POINTER( frame );

	_mali_sys_mutex_lock(frame->mutex);
	_mali_frame_ensure_not_rendering(frame);
}
