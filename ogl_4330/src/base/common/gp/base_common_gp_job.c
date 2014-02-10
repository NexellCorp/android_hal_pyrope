/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_types.h>
#include <base/mali_debug.h>
#include <base/mali_memory.h>
#include <base/mali_runtime.h>
#include <base/common/tools/base_common_tools_circular_linked_list.h>
#include <base/common/base_common_sync_handle.h>
#include <base/common/base_common_context.h>
#include <base/arch/base_arch_runtime.h>
#include <base/common/gp/base_common_gp_job.h>
#include <base/common/mem/base_common_mem.h>
#include <base/arch/base_arch_gp.h>
#include <mali_gp_plbu_cmd.h>

#if MALI_INSTRUMENTED
#include "base/common/instrumented/base_common_gp_instrumented.h"
#endif /* MALI_INSTRUMENTED */

#ifdef MALI_DUMP_ENABLE
#include <base/common/dump/base_common_dump_gp_functions_frontend.h>
#include <base/common/dump/base_common_dump_jobs_functions.h>
#endif

#define GP_VS_CMDLIST_BLOCK_SIZE 1024
#define GP_PLBU_CMDLIST_BLOCK_SIZE 1024

#ifdef __SYMBIAN32__
#include <symbian_base.h>
#endif

mali_err_code _mali_base_common_gp_open(mali_base_ctx_handle ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_CHECK_NO_ERROR(_mali_base_arch_gp_open());
	MALI_SUCCESS;
}

void _mali_base_common_gp_close(mali_base_ctx_handle ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	_mali_base_arch_gp_close();
}

MALI_TEST_EXPORT
mali_gp_job_handle _mali_base_common_gp_job_new(mali_base_ctx_handle ctx)
{
	mali_gp_job * job;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	job = _mali_sys_calloc(1, sizeof(mali_gp_job));

	if (NULL != job)
	{
		job->vs_cmd_list = _mali_base_common_gp_cmdlist_create(GP_VS_CMDLIST_BLOCK_SIZE);
		if (MALI_NO_HANDLE != job->vs_cmd_list)
		{
			job->plbu_cmd_list = _mali_base_common_gp_cmdlist_create(GP_PLBU_CMDLIST_BLOCK_SIZE);
			if (MALI_NO_HANDLE != job->plbu_cmd_list)
			{
				job->autoFree = MALI_TRUE;
				job->ctx = ctx;

				job->inlined.state = MALI_GP_JOB_STATE_BUILDING;
				job->inlined.vs = &job->vs_cmd_list->inlined;
				job->inlined.plbu = &job->plbu_cmd_list->inlined;
#if MALI_INSTRUMENTED
				_mali_base_common_instrumented_gp_context_init((mali_gp_job_handle)&job->inlined);
#endif /* #if MALI_INSTRUMENTED */

				/* add a reference to base context for as long as this job exist */
				_mali_base_common_context_reference_add(ctx);

				return (mali_gp_job_handle)&job->inlined;
			}
			_mali_base_common_gp_cmdlist_done(job->vs_cmd_list);
			_mali_base_common_gp_cmdlist_destroy(job->vs_cmd_list);
		}
		_mali_sys_free(job);
	}

	return MALI_NO_HANDLE;
}

MALI_TEST_EXPORT
void _mali_base_common_gp_job_free(mali_gp_job_handle job_handle)
{
	mali_base_ctx_handle ctx;
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT(MALI_GP_JOB_STATE_BUILDING == job->inlined.state, ("Freeing a job which is not in the BUILDING state is not supported"));

	if (MALI_GP_JOB_STATE_BUILDING != job->inlined.state) return;

	/* clear state */
	_mali_base_common_gp_job_reset(job_handle);
	_mali_base_common_gp_job_free_cmdlists(job_handle);

	/* save the ctx as we have to reference it after the free call */
	ctx = job->ctx;

	/* free the memory for the job itself */
	_mali_sys_free(job);

	/* release this job's reference to the base context */
	_mali_base_common_context_reference_remove(ctx);

}

void _mali_base_common_gp_job_reset(mali_gp_job_handle job_handle)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT(MALI_GP_JOB_STATE_BUILDING == job->inlined.state, ("Resetting a job which is not in the BUILDING state is not supported"));

	if (MALI_GP_JOB_STATE_BUILDING != job->inlined.state) return;

	_mali_base_common_gp_cmdlist_reset(job->vs_cmd_list);
	_mali_base_common_gp_cmdlist_reset(job->plbu_cmd_list);

	if (NULL != job->freelist)
	{
		_mali_mem_list_free(job->freelist);
		job->freelist = MALI_NO_HANDLE;
	}

	job->frame_id = MALI_BAD_FRAME_ID;

	if (NULL != job->wait_handle)
	{
		_mali_base_arch_sys_wait_handle_trigger(job->wait_handle);
		job->wait_handle = NULL;
	}

	if (NULL != job->sync)
	{
		_mali_base_common_sync_handle_release_reference(job->sync);
		job->sync = NULL;
	}

	/* state already set to building */
	job->callback = NULL;
	job->callback_argument = NULL;
	/* keep ctx */
	job->plbu_heap = MALI_NO_HANDLE;
	/* the other fields are not used while building and initialized when the job is started */
}

void _mali_base_common_gp_job_add_mem_to_free_list(mali_gp_job_handle job_handle, mali_mem_handle mem)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);
	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT_POINTER(mem);
	MALI_DEBUG_ASSERT(MALI_GP_JOB_STATE_BUILDING == job->inlined.state, ("Adding to the memory free list of a job which is not in the BUILDING state is not supported"));

	if (MALI_GP_JOB_STATE_BUILDING != job->inlined.state) return;
	else if (NULL == job->freelist) job->freelist = mem;
	else _mali_mem_list_insert_after(job->freelist, mem);
}

void _mali_base_common_gp_job_set_callback(mali_gp_job_handle job_handle, mali_cb_gp func, void * cb_param)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);
	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT(MALI_GP_JOB_STATE_BUILDING == job->inlined.state, ("Setting a callback on a job which is not in the BUILDING state is not supported"));

	if (MALI_GP_JOB_STATE_BUILDING == job->inlined.state)
	{
		job->callback = func;
		job->callback_argument = cb_param;
	}
}

void _mali_base_common_gp_job_set_auto_free_setting(mali_gp_job_handle job_handle, mali_bool autoFree)
{
	mali_gp_job * job;
	MALI_DEBUG_ASSERT_POINTER(job_handle);

	job = mali_gp_handle_to_job(job_handle);
	if(job == NULL) return;

	MALI_DEBUG_ASSERT(MALI_GP_JOB_STATE_BUILDING == job->inlined.state, ("Changing a job which is not in the BUILDING state is not supported"));
	if (MALI_GP_JOB_STATE_BUILDING == job->inlined.state) job->autoFree = autoFree;
}

mali_bool _mali_base_common_gp_job_get_auto_free_setting(mali_gp_job_handle job_handle)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);
	MALI_DEBUG_ASSERT_POINTER(job_handle);
	if (NULL != job) return job->autoFree;
	else return MALI_FALSE;
}

mali_base_wait_handle _mali_base_common_gp_job_get_wait_handle(mali_gp_job_handle job_handle)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);
	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT(MALI_GP_JOB_STATE_BUILDING == job->inlined.state, ("Can't get the wait handle of a job which is not in the BUILDING state is not supported"));
 	if (NULL == job) return MALI_NO_HANDLE;

	if ((MALI_GP_JOB_STATE_BUILDING == job->inlined.state) && (NULL == job->wait_handle))
	{
		job->wait_handle = _mali_base_arch_sys_wait_handle_create();
	}

	return job->wait_handle;
}

mali_err_code _mali_base_common_gp_job_start(mali_gp_job_handle job_handle, mali_job_priority priority)
{
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);
	mali_err_code err;
	static u32 job_count;

	MALI_DEBUG_ASSERT_POINTER(job_handle);
	if (NULL == job_handle) MALI_ERROR(MALI_ERR_FUNCTION_FAILED);

	_mali_base_common_gp_cmdlist_done(job->vs_cmd_list);
	_mali_base_common_gp_cmdlist_done(job->plbu_cmd_list);

	job->priority = priority;

	if (MALI_NO_HANDLE != job->vs_cmd_list)
	{
		_mali_base_common_gp_cmdlist_done(job->vs_cmd_list);
		job->registers[0] = _mali_base_common_gp_cmdlist_get_start_address(job->vs_cmd_list);
		job->registers[1] = _mali_base_common_gp_cmdlist_get_end_address(job->vs_cmd_list);
	}
	else
	{
		job->registers[0] = 0;
		job->registers[1] = 0;

	}

	if (MALI_NO_HANDLE != job->plbu_cmd_list)
	{
		_mali_base_common_gp_cmdlist_done(job->plbu_cmd_list);
		job->registers[2] = _mali_base_common_gp_cmdlist_get_start_address(job->plbu_cmd_list);
		job->registers[3] = _mali_base_common_gp_cmdlist_get_end_address(job->plbu_cmd_list);
	}
	else
	{
		job->registers[2] = 0;
		job->registers[3] = 0;
	}

	if (MALI_NO_HANDLE != job->plbu_heap)
	{
#if !defined(HARDWARE_ISSUE_3251)
		job->registers[4] =  _mali_mem_heap_get_start_address(job->plbu_heap);
		job->registers[5] =  _mali_mem_heap_get_end_address_of_first_block(job->plbu_heap);
#else /* !defined(HARDWARE_ISSUE_3251) */
		job->registers[4] = _mali_mem_mali_addr_get(job->plbu_heap, 0);
		job->registers[5] = _mali_mem_mali_addr_get(job->plbu_heap, _mali_mem_size_get(job->plbu_heap));
#endif /* !defined(HARDWARE_ISSUE_3251) */
	}
	else
	{
		job->registers[4] = 0;
		job->registers[5] = 0;
	}

	#ifdef MALI_DUMP_ENABLE
	    /* Filling the job->dump_info struct */
		_mali_common_dump_gp_pre_start(job);
	#endif /* MALI_DUMP_ENABLE */

	job->inlined.state = MALI_GP_JOB_STATE_RUNNING;

	#ifdef MALI_DUMP_ENABLE
	/* Doing memory dump before starting if enabled */
	_mali_base_common_dump_pre_start_dump(job->ctx, &(job->dump_info.gp), &(job->registers[0]));
	#endif /* MALI_DUMP_ENABLE */

	err = _mali_base_arch_gp_start(job);

	switch (err)
	{
		case MALI_ERR_NO_ERROR :
			break;
		case MALI_ERR_FUNCTION_FAILED:
			/* FALL THROUGH */
		default:
			_mali_base_common_gp_job_run_postprocessing(job,  MALI_JOB_STATUS_END_UNKNOWN_ERR);
			break;
	}

	if (++job_count == 4)
	{
		job_count = 0;
		_mali_mem_new_period();
	}

	/* we report SUCCESS as long as the actual queue operations succeeded, the job start itself could still fail, but it is reported through the callback interface */
	MALI_SUCCESS;
}

void _mali_base_common_gp_job_add_to_sync_handle(mali_sync_handle sync, mali_gp_job_handle job_handle)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);
	MALI_DEBUG_ASSERT_POINTER(sync);
	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT(MALI_GP_JOB_STATE_BUILDING == job->inlined.state, ("Registration of a job which is not in the BUILDING state to a sync handle is not supported"));
	MALI_DEBUG_ASSERT(NULL == job->sync, ("Multiple sync handle registrations of a job is not supported"));

	if ((NULL != job_handle) && (MALI_GP_JOB_STATE_BUILDING == job->inlined.state) && (NULL == job->sync))
	{
		_mali_base_common_sync_handle_register_reference(sync);
		job->sync = sync;
	}
}

void _mali_base_common_gp_job_free_cmdlists(mali_gp_job_handle job_handle)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT(MALI_GP_JOB_STATE_BUILDING == job->inlined.state || MALI_GP_JOB_STATE_CALLBACK == job->inlined.state || MALI_GP_JOB_STATE_SYNCING == job->inlined.state,
	                  ("Freeing the command lists of a job which is not in the BUILDING, CALLBACK or SYNCING state is not supported"));

	if (MALI_GP_JOB_STATE_BUILDING != job->inlined.state && MALI_GP_JOB_STATE_CALLBACK != job->inlined.state && MALI_GP_JOB_STATE_SYNCING != job->inlined.state) return;

	/* call done to close mapping before destroying */
	_mali_base_common_gp_cmdlist_done(job->vs_cmd_list);
	_mali_base_common_gp_cmdlist_destroy(job->vs_cmd_list);
	job->vs_cmd_list = NULL;
	job->inlined.vs = NULL;

	/* call done to close mapping before destroying */
	_mali_base_common_gp_cmdlist_done(job->plbu_cmd_list);
	_mali_base_common_gp_cmdlist_destroy(job->plbu_cmd_list);
	job->plbu_cmd_list = NULL;
	job->inlined.plbu = NULL;
}

void _mali_base_common_gp_job_set_frame_id(mali_gp_job_handle job_handle, mali_base_frame_id frame_id)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);
	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT(MALI_GP_JOB_STATE_BUILDING == job->inlined.state, ("Adding a job which is not in the BUILDING state to a frame id is not supported"));

	job->frame_id = frame_id;
}

void _mali_base_common_gp_job_run_postprocessing(mali_gp_job * job, mali_job_completion_status completion_status)
{
#ifdef MALI_DUMP_ENABLE
	mali_base_ctx_handle ctx;
	mali_sync_handle dump_sync;
#endif /* MALI_DUMP_ENABLE */

	MALI_DEBUG_ASSERT_POINTER(job);

#ifdef MALI_DUMP_ENABLE
	ctx = job->ctx;

	/* Setting dump_info pointer in TLS, so the settings can be acquired
	 * when a new job is started in current callback. Dump memory if
	 * DUMP_RESULT is set. If DUMP_CRASHED is set, it patch memory and
	 * dump the job if it has crashed */
	dump_sync = _mali_common_dump_gp_pre_callback(job, completion_status);
	/* need to keep an extra reference until we've done the post hook */
	_mali_base_common_context_reference_add(job->ctx);
#endif /* MALI_DUMP_ENABLE */

	_mali_base_common_gp_job_run_postprocessing_job(job, completion_status);

#ifdef MALI_DUMP_ENABLE
	/* Setting the TLS to not running in callback.
	 * Release dump_info->dump_sync so that _mali_common_dump_pp_try_start() can return
	 * if waiting on job complete is enabled.*/
	_mali_common_dump_gp_post_callback(ctx, dump_sync);
	/* remove the extra reference we made */
	_mali_base_common_context_reference_remove(ctx);
#endif /* MALI_DUMP_ENABLE */
}


void _mali_base_common_gp_job_run_postprocessing_job(mali_gp_job * job, mali_job_completion_status completion_status)
{
	mali_bool override_completion = MALI_FALSE;
	mali_bool autoFree;
	mali_sync_handle sync;
	mali_base_wait_handle wait_handle;

	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT_POINTER(job->ctx);

	autoFree = job->autoFree;

	/* Since the callback function may alter elements in the job, we store these variables.*/
	wait_handle = job->wait_handle;
	job->wait_handle = NULL;
	sync = job->sync;
	job->sync = NULL;

	if (MALI_FALSE == autoFree)
	{
		job->inlined.state = MALI_GP_JOB_STATE_BUILDING;
	}
	else
	{
		job->inlined.state = MALI_GP_JOB_STATE_CALLBACK;
	}


	if (NULL != job->callback)
	{
		override_completion = !(job->callback(job->ctx, job->callback_argument, completion_status, (mali_gp_job_handle)&job->inlined));
	}

	if (NULL != sync)
	{
		_mali_base_common_sync_handle_release_reference(sync);
	}

	if (MALI_TRUE == override_completion) return;

	if (NULL != wait_handle)
	{
		_mali_base_arch_sys_wait_handle_trigger(wait_handle);
	}

	if ( MALI_FALSE != autoFree)
	{
		mali_base_ctx_handle ctx;

		/* for instrumented builds, some parts of the completion are only to be
		 * performed at the end, when there are no more duplicate jobs */
		_mali_base_common_gp_cmdlist_destroy(job->vs_cmd_list);
		job->vs_cmd_list = NULL;
		job->inlined.vs = NULL;

		_mali_base_common_gp_cmdlist_destroy(job->plbu_cmd_list);
		job->plbu_cmd_list = NULL;
		job->inlined.plbu = NULL;

		if (NULL != job->freelist)
		{
			_mali_mem_list_free(job->freelist);
			job->freelist = MALI_NO_HANDLE;
		}

		job->frame_id = MALI_BAD_FRAME_ID;

		ctx = job->ctx;
		_mali_sys_free(job);
		_mali_base_common_context_reference_remove(ctx);
	}
}

void _mali_base_common_gp_job_set_tile_heap(mali_gp_job_handle job_handle, mali_mem_handle heap)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);
	MALI_DEBUG_ASSERT_POINTER(job_handle);
	job->plbu_heap = heap;
}

void _mali_base_common_gp_job_suspended_event(mali_gp_job *job, mali_base_common_gp_job_suspend_reason reason, u32 response_cookie)
{
	MALI_DEBUG_ASSERT_POINTER(job);
	switch (reason)
	{
#if !defined(HARDWARE_ISSUE_3251)
		case MALI_BASE_COMMON_GP_JOB_SUSPEND_REASON_OOM:
		{
			u32 heap_start, heap_end;
			if (  (NULL != job->plbu_heap) && (MALI_ERR_NO_ERROR ==
					_mali_base_common_mem_heap_out_of_memory(job->plbu_heap, &heap_start, &heap_end))
			   )
			{
				/* resume job with new heap */
				#ifdef MALI_DUMP_ENABLE
					/* Dumping the register writes that are done during PLBU heap growth.*/
					_mali_common_dump_gp_heap_grow(job, heap_start, heap_end);
				#endif
				_mali_base_arch_gp_mem_request_response_new_heap(response_cookie, heap_start, heap_end);
			}
			else
			{
				#ifdef MALI_DUMP_ENABLE
					/* Dumping the register writes that are done when we do not have enough memory for PLBU heap growth.*/
					_mali_common_dump_gp_heap_grow(job, 0, 0);
				#endif
				/* no more memory available for this job, abort it */
				_mali_base_arch_gp_mem_request_response_abort(response_cookie);
			}

			break;
		}
#endif
		default:
			MALI_DEBUG_PRINT(1, ("Unknown suspension reason %d, aborting job 0x%X\n", reason, job));
			_mali_base_arch_gp_mem_request_response_abort(response_cookie);
			break;
	}
}

void _mali_base_common_gp_job_set_identity(mali_gp_job_handle job_handle, mali_base_frame_id fb_id, mali_base_flush_id flush_id)
{
	mali_gp_job* 			job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job_handle);

	job->frame_builder_id 	= fb_id;
	job->flush_id			= flush_id;
}

