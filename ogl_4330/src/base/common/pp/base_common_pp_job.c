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
#include <util/mali_names.h>
#include <base/mali_runtime.h>
#include <base/common/tools/base_common_tools_circular_linked_list.h>
#include <base/common/base_common_sync_handle.h>
#include <base/common/base_common_context.h>
#include <base/arch/base_arch_runtime.h>
#include <base/arch/base_arch_pp.h>
#include "base_common_pp_job.h"
#include <sync/mali_external_sync.h>

#if MALI_INSTRUMENTED
#include "base/common/instrumented/base_common_pp_instrumented.h"
#endif

#ifdef MALI_DUMP_ENABLE
	#include <base/common/dump/base_common_dump_pp_functions_frontend.h>
#endif

#ifdef __SYMBIAN32__
#include <symbian_base.h>
#endif

#if defined(USING_MALI200)
#define LAST_FRAME_REG M200_FRAME_REG_TIEBREAK_MODE
#elif defined(USING_MALI400) || defined(USING_MALI450)
#define LAST_FRAME_REG M400_FRAME_REG_TILEBUFFER_BITS
#else
#error "no supported mali core defined"
#endif

/*
 * Startup handlers
 */
MALI_STATIC mali_err_code pp_system_initialize(mali_base_ctx_handle ctx) MALI_CHECK_RESULT;
MALI_STATIC void pp_system_terminate(mali_base_ctx_handle ctx);

MALI_STATIC mali_bool m200_check_version(void)
{
	u32 hwversion = _mali_base_arch_pp_get_core_version();

#if defined(USING_MALI400) && (MALI_HWVER == 0x0101)
	/* Accept Mali300 r0p0 if built for Mali400 r1p1 */
	u32 product = (hwversion & MALI_PRODUCT_ID_MASK) >> 16;
	u32 version = hwversion & MALI_VERSION_MASK;

	if ( (product != MALI_PRODUCT_ID_PP_MALI400 && version != MALI400_HWVER ) &&
	     (product != MALI_PRODUCT_ID_PP_MALI300 && version != MALI300_HWVER ))
#else
	if ( (hwversion & MALI_VERSION_MASK) != MALI_HWVER ||
	    ((hwversion & MALI_PRODUCT_ID_MASK) >> 16) != MALI_PRODUCT_ID_PP)
#endif
	{
#ifdef DEBUG
		const char* hwname = NULL;

		switch((hwversion & MALI_PRODUCT_ID_MASK) >> 16)
		{
		case MALI_PRODUCT_ID_PP_MALI200:
			hwname = MALI_NAME_PRODUCT_MALI200;
			break;
		case MALI_PRODUCT_ID_PP_MALI300:
			hwname = MALI_NAME_PRODUCT_MALI300;
			break;
		case MALI_PRODUCT_ID_PP_MALI400:
			hwname = MALI_NAME_PRODUCT_MALI400;
			break;
		case MALI_PRODUCT_ID_PP_MALI450:
			hwname = MALI_NAME_PRODUCT_MALI450;
			break;
		default:
			hwname = "Mali-Unknown";
			break;
		}

		MALI_DEBUG_PRINT(1, ("\n=================================================\n"));
		MALI_DEBUG_PRINT(1, ("ERROR: Hardware/software mismatch\n"));
		MALI_DEBUG_PRINT(1, ("Hardware version detected: %s r%dp%d\n",
			hwname,
			(hwversion & MALI_VERSION_MAJOR_MASK) >> 8,
			hwversion & MALI_VERSION_MINOR_MASK));
		MALI_DEBUG_PRINT(1, ("This driver was built for: %s r%dp%d\n",
			MALI_NAME_PRODUCT,
			(MALI_HWVER & MALI_VERSION_MAJOR_MASK) >> 8,
			MALI_HWVER & MALI_VERSION_MINOR_MASK));
		MALI_DEBUG_PRINT(1, ("Failing context create\n"));
		MALI_DEBUG_PRINT(1, ("=================================================\n\n"));
#endif
		return MALI_FALSE;
	}

	MALI_DEBUG_PRINT(2, ("INFO: Hardware version: %s r%dp%d\n",
		MALI_NAME_PRODUCT,
		(MALI_HWVER & MALI_VERSION_MAJOR_MASK) >> 8,
		MALI_HWVER & MALI_VERSION_MINOR_MASK));

	return MALI_TRUE;
}

mali_err_code _mali_base_common_pp_open(mali_base_ctx_handle ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	return pp_system_initialize(MALI_REINTERPRET_CAST(void*)ctx);
}

void _mali_base_common_pp_close(mali_base_ctx_handle ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	pp_system_terminate( MALI_REINTERPRET_CAST(void*)ctx );
}

MALI_STATIC void pp_reg_set(mali_pp_registers * registers, mali_reg_id regid, mali_reg_value value)
{
	u32 bank, internal_register_number;
	bank = (regid >> 6) & 0x03FF;
	internal_register_number = regid & 0x003F;

	switch (bank)
	{
		case 0:
			MALI_DEBUG_ASSERT(regid <= LAST_FRAME_REG, ("Invalid register id %d", regid));
			registers->frame_regs[internal_register_number] = value;
			break;
		case 1:
			MALI_DEBUG_ASSERT(regid <= M200_WB0_REG_GLOBAL_TEST_CMP_FUNC, ("Invalid register id %d", regid));
			registers->wb0_regs[internal_register_number] = value;
			break;
		case 2:
			MALI_DEBUG_ASSERT(regid <= M200_WB1_REG_GLOBAL_TEST_CMP_FUNC, ("Invalid register id %d", regid));
			registers->wb1_regs[internal_register_number] = value;
			break;
		case 3:
			MALI_DEBUG_ASSERT(regid <= M200_WB2_REG_GLOBAL_TEST_CMP_FUNC, ("Invalid register id %d", regid));
			registers->wb2_regs[internal_register_number] = value;
			break;
		default:
			MALI_DEBUG_ASSERT( MALI_FALSE, ("Invalid register id %d", regid));
			break;
	}
}

MALI_TEST_EXPORT
mali_pp_job_handle _mali_base_common_pp_job_new(mali_base_ctx_handle ctx, u32 num_cores, mali_stream_handle stream)
{
	mali_pp_job * job;

	job = _mali_sys_calloc(1, sizeof(mali_pp_job));
	if (NULL == job) return MALI_NO_HANDLE;

	job->state = MALI_PP_JOB_BUILDING;
	job->ctx = ctx;
	job->num_cores = num_cores;
	job->stream = stream;
	job->pre_fence = MALI_FENCE_INVALID_HANDLE;

	/* add a reference to base context for as long as this job exists */
	_mali_base_common_context_reference_add(ctx);

#if MALI_INSTRUMENTED
	_mali_base_common_instrumented_pp_context_init((mali_pp_job_handle)job);
#endif

	return (mali_pp_job_handle)job;
}

MALI_TEST_EXPORT
void _mali_base_common_pp_job_free(mali_pp_job_handle job_handle)
{
	mali_pp_job * job;
	mali_base_ctx_handle ctx;

	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT(MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state,
	                  ("Freeing a job which is not in the BUILDING, CALLBACK or SYNCING state is not supported"));

	if (MALI_PP_JOB_BUILDING != job->state && MALI_PP_JOB_CALLBACK != job->state && MALI_PP_JOB_SYNCING != job->state) return;

	_mali_base_common_pp_job_reset(job_handle);

	/* save the ctx as we have to reference it after the free call */
	ctx = job->ctx;

	/* free the job struct itself */
	_mali_sys_free(job);

	/* release this job's reference to the base context */
	_mali_base_common_context_reference_remove(ctx);

}

void _mali_base_common_pp_job_reset(mali_pp_job_handle job_handle)
{
	mali_pp_job * job;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT(MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state,
	                  ("Freeing a job which is not in the BUILDING, CALLBACK or SYNCING state is not supported"));

	if (MALI_PP_JOB_BUILDING != job->state && MALI_PP_JOB_CALLBACK != job->state && MALI_PP_JOB_SYNCING != job->state) return;

	if (NULL != job->freelist)
	{
		_mali_mem_list_free(job->freelist);
		job->freelist = MALI_NO_HANDLE;
	}

	if (NULL != job->sync)
	{
		_mali_base_common_sync_handle_release_reference(job->sync);
		job->sync = NULL;
	}

	if (NULL != job->wait_handle)
	{
		_mali_base_arch_sys_wait_handle_trigger(job->wait_handle);
		job->wait_handle = NULL;
	}

	job->stream = NULL;
	if (MALI_FENCE_INVALID_HANDLE != job->pre_fence)
	{
		mali_fence_release(job->pre_fence);
		job->pre_fence = MALI_FENCE_INVALID_HANDLE;
	}

	/* state already set */
	/* keep ctx */
	job->callback = NULL;
	job->callback_argument = 0;
	job->barrier = MALI_FALSE;
	/* the pointer to the helper functions should be kept */

	/* revert any register changes */
	_mali_sys_memset(&job->registers, 0, sizeof(job->registers));

	/* the other members will are not valid while in the building state, so we can just ignore them */
}

void _mali_base_common_pp_job_add_mem_to_free_list(mali_pp_job_handle job_handle, mali_mem_handle mem)
{
	mali_pp_job * job;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT_POINTER(mem);
	MALI_DEBUG_ASSERT(MALI_PP_JOB_BUILDING == job->state, ("Adding to the memory free list of a job which is not in the BUILDING state is not supported"));

	if (MALI_PP_JOB_BUILDING != job->state) return;
	else if (NULL == job->freelist) job->freelist = mem;
	else _mali_mem_list_insert_after(job->freelist, mem);
}

void _mali_base_common_pp_job_set_callback(mali_pp_job_handle job_handle, mali_cb_pp func, void * cb_param)
{
	mali_pp_job * job;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT(MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state,
	                  ("Setting a callback on a job which is not in the BUILDING, CALLBACK or SYNCING state is not supported"));

	if (MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state)
	{
		job->callback = func;
		job->callback_argument = cb_param;
	}
}

void _mali_base_common_pp_job_set_common_render_reg(mali_pp_job_handle job_handle, mali_reg_id regid, mali_reg_value value)
{
	mali_pp_job * job;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT(	MALI_PP_JOB_BUILDING == job->state || 
				MALI_PP_JOB_CALLBACK == job->state || 
				MALI_PP_JOB_RUNNING == job->state|| 
				MALI_PP_JOB_SYNCING == job->state,
				("Changing a render register on a job which is not in the BUILDING, CALLBACK or SYNCING state is not supported"));
 
	if (MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state)
	{
		pp_reg_set(&job->registers, regid, value);
	}
}

u32 _mali_base_common_pp_job_get_render_attachment_address(mali_pp_job_handle job_handle, u32 wb_unit)
{
	mali_pp_job* job;
	job = MALI_REINTERPRET_CAST(mali_pp_job*)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	switch(wb_unit)
	{
		case 0:
			return (u32)job->registers.wb0_regs[M200_WB0_REG_TARGET_ADDR-M200_WB0_REG_SOURCE_SELECT];
		case 1:
			return (u32)job->registers.wb1_regs[M200_WB1_REG_TARGET_ADDR-M200_WB1_REG_SOURCE_SELECT];
		case 2:
			return (u32)job->registers.wb2_regs[M200_WB2_REG_TARGET_ADDR-M200_WB2_REG_SOURCE_SELECT];
		default:
			return 0;
	}
}

mali_bool _mali_base_common_pp_job_is_updateable(mali_pp_job_handle job_handle)
{
	mali_pp_job * job;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	return (MALI_PP_JOB_BUILDING == job->state|| 
		MALI_PP_JOB_CALLBACK == job->state|| 
		MALI_PP_JOB_RUNNING == job->state|| 
		MALI_PP_JOB_SYNCING == job->state);
}

void _mali_base_common_pp_job_set_specific_render_reg(mali_pp_job_handle job_handle, int core_no, mali_reg_id regid, mali_reg_value value)
{
	mali_pp_job * job;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT(core_no >= 0 && core_no < _MALI_PP_MAX_SUB_JOBS, ("Setting register for invalid core number %d", core_no));
	MALI_DEBUG_ASSERT(MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state,
	                  ("Changing a render register on a job which is not in the BUILDING, CALLBACK or SYNCING state is not supported"));
	MALI_DEBUG_ASSERT(M200_FRAME_REG_REND_LIST_ADDR == regid || M200_FRAME_REG_FS_STACK_ADDR == regid, ("Attempting to set invalid specific register %d", regid));

	if (MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state)
	{
		if (0 == core_no)
		{
			_mali_base_common_pp_job_set_common_render_reg(job_handle, regid, value);
			return;
		}

		if (M200_FRAME_REG_REND_LIST_ADDR == regid)
		{
			job->registers.frame_regs_addr_frame[core_no - 1] = value;
		}
		else if(M200_FRAME_REG_FS_STACK_ADDR == regid)
		{
			job->registers.frame_regs_addr_stack[core_no - 1] = value;
		}
		else
		{
			return;
		}
	}
}

void _mali_base_common_pp_job_set_barrier(mali_pp_job_handle job_handle)
{
	mali_pp_job * job;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	job->barrier = MALI_TRUE;
}

mali_base_wait_handle _mali_base_common_pp_job_get_wait_handle(mali_pp_job_handle job_handle)
{
	mali_pp_job * job;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT(MALI_PP_JOB_BUILDING == job->state, ("Can't get the wait handle of a job which is not in the BUILDING state is not supported"));

	if ((MALI_PP_JOB_BUILDING == job->state) && (NULL == job->wait_handle) )
	{
		job->wait_handle = _mali_base_arch_sys_wait_handle_create();
	}

	return job->wait_handle;
}

void _mali_base_common_pp_job_start(mali_pp_job_handle job_handle, mali_job_priority priority, mali_fence_handle *fence)
{
	mali_pp_job * job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	mali_err_code err;
	mali_bool no_notification = MALI_FALSE;

	MALI_DEBUG_ASSERT_POINTER(job);
	if (NULL == job) return;

	job->priority = priority;

	#ifdef MALI_DUMP_ENABLE
	    /* Filling the job->dump_info struct */
		_mali_common_dump_pp_pre_start(job);
	#endif

	/* try to start it right away */
	MALI_DEBUG_ASSERT(
							(MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state),
							("Invalid job state transition. Can't end up in %d from %d", MALI_PP_JOB_RUNNING, job->state)
						 );
	job->state = MALI_PP_JOB_RUNNING;

#ifdef MALI_DUMP_ENABLE
	_mali_base_common_dump_pre_start_dump(job->ctx, &(job->dump_info), (void*)job);
#endif

#if !MALI_INSTRUMENTED
	/* Always use notifications in instrumented mode, since we need to get counters values back! */
	if (NULL == job->callback && NULL == job->wait_handle && NULL == job->sync)
	{
		no_notification = MALI_TRUE;
	}
#else
	no_notification = MALI_FALSE;
#endif

	err = _mali_base_arch_pp_start(job, no_notification, fence);

	switch (err)
	{
		case MALI_ERR_NO_ERROR:
			if (MALI_TRUE == no_notification)
			{
				/* The caller don't want to wait for this job,
				 * so just do the post processing (including job deletion) right now.
				 * Pretend it was successful.
				 */
				_mali_base_common_pp_job_run_postprocessing(job, MALI_JOB_STATUS_END_SUCCESS);
			}
			break;
		case MALI_ERR_FUNCTION_FAILED:
			/* FALL THROUGH */
		default:
			_mali_base_common_pp_job_run_postprocessing(job, MALI_JOB_STATUS_END_UNKNOWN_ERR);
			break;
	}
}

mali_err_code _mali_base_common_pp_job_set_fence(mali_pp_job_handle job_handle, mali_fence_handle fence)
{
	mali_pp_job *job;
	mali_fence_handle new_fence = fence;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;

	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT(MALI_PP_JOB_BUILDING == job->state, ("Invalid job state"));

	/* Merge fences if a fence is set on job already */
	if (MALI_FENCE_INVALID_HANDLE != job->pre_fence)
	{
		new_fence = mali_fence_merge("mali_merged", fence, job->pre_fence);

		if (MALI_FENCE_INVALID_HANDLE == new_fence)
		{
			MALI_DEBUG_PRINT(1, ("Failed to merge job fences; new fence not set on job\n"));
			return MALI_ERR_FUNCTION_FAILED;
		}
		mali_fence_release(fence);
		mali_fence_release(job->pre_fence);
	}

	job->pre_fence = new_fence;

	return MALI_ERR_NO_ERROR;
}

void _mali_base_common_pp_job_add_to_sync_handle(mali_sync_handle sync, mali_pp_job_handle job_handle)
{
	mali_pp_job * job;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)job_handle;
	MALI_DEBUG_ASSERT_POINTER(sync);
	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT(MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state,
	                  ("Registering of a job which is not in the BUILDING, CALLBACK or SYNCING state to a sync handle is not supported"));
	MALI_DEBUG_ASSERT(NULL == job->sync, ("Multiple sync handle registrations of a job is not supported"));

	if ( (MALI_PP_JOB_BUILDING == job->state || MALI_PP_JOB_CALLBACK == job->state || MALI_PP_JOB_SYNCING == job->state) && (NULL == job->sync))
	{
		_mali_base_common_sync_handle_register_reference(sync);
		job->sync = sync;
	}
}

void _mali_base_common_pp_job_run_postprocessing(mali_pp_job * job, mali_job_completion_status completion_status)
{
	mali_base_ctx_handle ctx;
	mali_sync_handle sync;
	mali_base_wait_handle wait_handle;
#ifdef MALI_DUMP_ENABLE
	mali_sync_handle dump_sync;
#endif

	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT_POINTER(job->ctx);

	MALI_DEBUG_PRINT(3, ("Starting PP post processing\n"));

	MALI_DEBUG_CODE(job->state = MALI_PP_JOB_CALLBACK);

	#ifdef MALI_DUMP_ENABLE
		/* Setting dump_info pointer in TLS, so the settings can be acquired
		* when a new job is started in current callback. Dump memory if
		* DUMP_RESULT is set. If DUMP_CRASHED is set, it dump the job
		* with register writes to start it if the job crashes.*/
		dump_sync = _mali_common_dump_pp_pre_callback(job, completion_status);
	#endif

	/* Since the callback function may alter elements in the job, we store these variables.*/
	ctx = job->ctx;
	wait_handle = job->wait_handle;
	job->wait_handle = NULL;
	sync = job->sync;
	job->sync = NULL;

	if (NULL != job->callback)
	{
		job->callback(job->ctx, job->callback_argument, completion_status, (mali_pp_job_handle)job);
	}

	if (NULL != sync)
	{
		MALI_DEBUG_PRINT(3, ("Triggering sync handle\n"));
		MALI_DEBUG_CODE(job->state = MALI_PP_JOB_SYNCING);
		_mali_base_common_sync_handle_release_reference(sync);
	}

	if (NULL != wait_handle)
	{
		MALI_DEBUG_PRINT(3, ("Triggering wait handle\n"));
		MALI_DEBUG_CODE(job->state = MALI_PP_JOB_WAKEUP);
		_mali_base_arch_sys_wait_handle_trigger(wait_handle);
	}
	#ifdef MALI_DUMP_ENABLE
		/* Setting the TLS to not running in callback.
		* Release dump_info->dump_sync so that _mali_common_dump_pp_try_start() can return
		* if waiting on job complete is enabled. */
		_mali_common_dump_pp_post_callback(ctx, dump_sync);
	#endif

	if (NULL != job->freelist)
	{
		MALI_DEBUG_PRINT(3, ("Doing mem free\n"));
		MALI_DEBUG_CODE(job->state = MALI_PP_JOB_MEMORY_CLEANUP);
		_mali_mem_list_free(job->freelist);
		job->freelist = NULL;
	}

	/* Release job fences */
	job->stream = NULL;
	if (MALI_FENCE_INVALID_HANDLE != job->pre_fence)
	{
		mali_fence_release(job->pre_fence);
		job->pre_fence = MALI_FENCE_INVALID_HANDLE;
	}

	/* free job object */
	MALI_DEBUG_PRINT(3, ("Freeing job\n"));
	_mali_sys_free(job);
	_mali_base_common_context_reference_remove(ctx);

}

MALI_STATIC mali_err_code pp_system_initialize(mali_base_ctx_handle ctx)
{
	mali_err_code err;

	MALI_IGNORE(ctx);

	err = _mali_base_arch_pp_open();
	if (MALI_ERR_NO_ERROR == err)
	{
		if (MALI_TRUE == m200_check_version())
		{
			MALI_SUCCESS;
		}
		else
		{
			err = MALI_ERR_FUNCTION_FAILED;
		}
		_mali_base_arch_pp_close();
	}

	MALI_ERROR(err);
}

MALI_STATIC void pp_system_terminate(mali_base_ctx_handle ctx)
{
	_mali_base_arch_pp_close();
	_mali_mem_close(ctx);
}

void _mali_base_common_pp_set_frame_id(mali_pp_job_handle job_handle, mali_base_frame_id frame_id)
{
	mali_pp_job * job = (mali_pp_job*)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);
	job->frame_id = frame_id;
}

void _mali_base_common_pp_job_set_identity(mali_pp_job_handle job_handle, mali_base_frame_id fb_id, mali_base_flush_id flush_id)
{
	mali_pp_job* 			job = (mali_pp_job*) job_handle;

	MALI_DEBUG_ASSERT_POINTER(job_handle);

	job->frame_builder_id 	= fb_id;
	job->flush_id			= flush_id;
}

void _mali_base_common_pp_job_disable_wb(u32 fb_id, u32 flush_id, mali_pp_job_wbx_flag wbx)
{
	_mali_base_arch_pp_job_disable_wb(fb_id, flush_id, wbx);
}


#if defined(USING_MALI450)

void _mali_pp_job_450_set_frame_info(mali_pp_job_handle job_handle, m450_pp_job_frame_info * info)
{
	mali_pp_job* 			job = (mali_pp_job*) job_handle;
	MALI_DEBUG_ASSERT_POINTER(job_handle);

	_mali_sys_memcpy_32(&job->info, info, sizeof(m450_pp_job_frame_info));
}

#endif /* defined(USING_MALI450) */
