/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "base_common_gp_instrumented.h"
#include <base/arch/base_arch_mem.h>
#include <base/arch/base_arch_runtime.h>
#include <base/common/gp/base_common_gp_job.h>
#include "mali_instrumented_context.h"
#ifdef MALI_DUMP_ENABLE
#include "base/common/dump/base_common_dump_gp_functions_frontend.h"
#endif

#define MALI_INSTRUMENTED_CHECK_ALL_GP_JOBS_ARE_INSTRUMENTED 0

MALI_TEST_EXPORT
void _mali_base_common_instrumented_gp_context_init(mali_gp_job_handle job_handle)
{
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);
	mali_base_instrumented_gp_context* ctx;

	ctx = &job->perf_ctx;
	MALI_DEBUG_ASSERT_POINTER(ctx);

	ctx->perf_counters_count = 0;
	ctx->perf_counters = NULL;
	ctx->perf_values = NULL;
#if !defined(USING_MALI200)
	ctx->l2_perf_counters_count = 0;
#endif
	ctx->plbu_stack = NULL;
	ctx->pointer_array = NULL;
	ctx->num_counters_to_read = 0;
}

MALI_TEST_EXPORT
void _mali_base_common_instrumented_gp_job_set_counters(mali_gp_job_handle job_handle, u32 *perf_counters, u32 perf_counters_count)
{
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);
	mali_base_instrumented_gp_context *job_ctx = &job->perf_ctx;

	MALI_DEBUG_ASSERT_POINTER(job_ctx);
	MALI_DEBUG_ASSERT( (0 == perf_counters_count) || (NULL != perf_counters), ("Perf_counters is a null-pointer"));

	job_ctx->perf_counters = perf_counters;
	job_ctx->perf_counters_count = perf_counters_count;
	if (0 ==job_ctx->perf_counters_count)
	{
		job_ctx->perf_counters = NULL;
	}
}

MALI_TEST_EXPORT
void _mali_base_common_instrumented_gp_job_get_counters(mali_gp_job_handle job_handle, u32 **perf_counters, u32* perf_counters_count)
{
	mali_base_instrumented_gp_context *job_ctx;
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT_POINTER(perf_counters);
	MALI_DEBUG_ASSERT_POINTER(perf_counters_count);

	job_ctx = &job->perf_ctx;

	*perf_counters = job_ctx->perf_counters;
	*perf_counters_count = job_ctx->perf_counters_count;

}

MALI_STATIC mali_bool job_finished(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status job_result, mali_gp_job_handle job_handle)
{
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);

	MALI_IGNORE(ctx);
	MALI_IGNORE(cb_param);

	job->perf_ctx.job_result = job_result;
	return MALI_TRUE;
}

mali_err_code _mali_base_common_instrumented_gp_job_start(mali_gp_job_handle job_handle, mali_job_priority priority)
{
	mali_gp_job *job;
	mali_base_instrumented_gp_context *job_ctx;
	mali_mem_handle pointer_array_copy = NULL;
	mali_mem_handle plbu_stack_copy = NULL;
	mali_base_wait_handle wait_handle_copy = MALI_NO_HANDLE;
	mali_sync_handle sync_copy = MALI_NO_HANDLE;
	mali_bool autofree_copy = MALI_FALSE;
	mali_cb_gp callback_copy = NULL;
	void *callback_argument_copy = NULL;
	mali_bool must_run = MALI_TRUE;
#ifdef MALI_DUMP_ENABLE
	mali_bool revert_dump_setting = MALI_FALSE;
	mali_bool dumping_enabled = MALI_TRUE;
#endif
	u32 i = 0;

	if (NULL == _instrumented_get_context())
	{
		/* Instrumentation is not active, so we enforce normal job start behavior */
		return _mali_base_common_gp_job_start(job_handle, priority);
	}

	MALI_DEBUG_ASSERT_POINTER(job_handle);

#if MALI_INSTRUMENTED_CHECK_ALL_GP_JOBS_ARE_INSTRUMENTED
	MALI_DEBUG_ASSERT(MALI_TRUE == _mali_base_common_instrumented_gp_is_job_instrumented(job_handle), ("Job is not setup for instrumentation"));
#endif

	job = mali_gp_handle_to_job(job_handle);
	job_ctx = &job->perf_ctx;

	job_ctx->job_result = MALI_JOB_STATUS_END_UNKNOWN_ERR;

	/*
	 * If we have 2 or less counters, it might be tempting to simply run this job normally,
	 * but we need to be cautious! We never know if the next job must be run more than once,
	 * and in that case we need to be sure that the current GP job has finished, before we
	 * attempt to copy the pointer array and PLBU stack.
	 * (GP jobs can be dependent of each other, the output of one GP job can be used as input
	 * by the next. This is what the load/store pointer array commands are for.)
	 */

	if (job_ctx->perf_counters_count > 2)
	{
		/*
		 * We have to run this job several times in order to retrieve all HW counters.
		 * Take a copy of the pointer array and PLBU stack, so it can be reverted
		 * before each run.
		 */

		if (NULL == job_ctx->pointer_array || NULL == job_ctx->plbu_stack)
		{
			return MALI_ERR_FUNCTION_FAILED;
		}

		/* create a duplicate of the current pointer array */
		pointer_array_copy = _mali_mem_alloc(
			job->ctx,
			_mali_mem_size_get(job_ctx->pointer_array),
			_mali_mem_alignment_get(job_ctx->pointer_array),
			_mali_mem_usage_get(job_ctx->pointer_array));

		if (MALI_NO_HANDLE == pointer_array_copy)
		{
			return MALI_ERR_FUNCTION_FAILED;
		}

		_mali_mem_copy(
			pointer_array_copy,
			0,
			job_ctx->pointer_array,
			0,
			_mali_mem_size_get(job_ctx->pointer_array));

		/* create a duplicate of the current plbu_stack */
		plbu_stack_copy = _mali_mem_alloc(
			job->ctx,
			_mali_mem_size_get(job_ctx->plbu_stack),
			_mali_mem_alignment_get(job_ctx->plbu_stack),
			_mali_mem_usage_get(job_ctx->plbu_stack));

		if (MALI_NO_HANDLE == plbu_stack_copy)
		{
			_mali_mem_free(pointer_array_copy);
			return MALI_ERR_FUNCTION_FAILED;
		}

		_mali_mem_copy(
			plbu_stack_copy,
			0,
			job_ctx->plbu_stack,
			0,
			_mali_mem_size_get(job_ctx->plbu_stack));
	}

	/*
	 * In order to ensure that we don't start a job before the previous is finished, we
	 * simply wait for the job our self. This is not the most trivial task, since the
	 * user might already have created its own wait handle. There is also a sync handle,
	 * user callback and auto-free setting which we need to override.
	 */

	/* we will create our own wait handle for each job, and restore this when we complete it */
	wait_handle_copy = job->wait_handle;
	job->wait_handle = MALI_NO_HANDLE;

	/* Disable sync handle so we don't alert the user before we complete it */
	sync_copy = job->sync;
	job->sync = MALI_NO_HANDLE;

	/* We need the memory because we might need to run the job multiple times */
	autofree_copy = job->autoFree;
	job->autoFree = MALI_FALSE;

	/* Insert our own callback, we don't want the user to get a callback for each job we run */
	callback_copy = job->callback;
	job->callback = job_finished;
	callback_argument_copy = job->callback_argument;
	job->callback_argument = job;

	/*
	 * Run the (modified) incoming job as many times as needed
	 */
	for (i = 0; (must_run) || (i < job->perf_ctx.perf_counters_count); i += 2)
	{
		mali_err_code err;
		mali_base_wait_handle wait_handle = MALI_NO_HANDLE;

		must_run = MALI_FALSE; /* only used to ensure we do this loop atleast once */

		if (i > 0)
		{
			/* Restore the pointer array and PLBU stack (unless it is the first job) */
			_mali_mem_copy(
					job_ctx->pointer_array,
					0,
					pointer_array_copy,
					0,
					_mali_mem_size_get(job_ctx->pointer_array));

			_mali_mem_copy(
					job_ctx->plbu_stack,
					0,
					plbu_stack_copy,
					0,
					_mali_mem_size_get(job_ctx->plbu_stack));

#ifdef MALI_DUMP_ENABLE
			/* Make sure we don't dump duplicate jobs */
			if (MALI_TRUE != revert_dump_setting)
			{
				dumping_enabled = _mali_common_dump_gp_enable_dump(job_handle, MALI_FALSE);
				revert_dump_setting = MALI_TRUE;
			}
#endif
		}

		/*
		 * Setup which counters to sample
		 */
		if (job_ctx->perf_counters_count > i)
		{
			job_ctx->counter_id[0] = job_ctx->perf_counters[i];
			job_ctx->counter_result[0] = &job_ctx->perf_values[i];
			job_ctx->num_counters_to_read = 1;
			if (job_ctx->perf_counters_count > (i + 1))
			{
				job_ctx->counter_id[1] = job_ctx->perf_counters[i + 1];
				job_ctx->counter_result[1] = &job_ctx->perf_values[i + 1];
				job_ctx->num_counters_to_read = 2;
			}
		}
		else
		{
			job_ctx->num_counters_to_read = 0;
		}

		/*
		 * I don't want multiple jobs running at the same time (that will only mess things up),
		 * so I create my own wait handle.
		 */
		wait_handle = job->wait_handle = _mali_base_arch_sys_wait_handle_create();
		if (MALI_NO_HANDLE == wait_handle)
		{
			if (NULL != plbu_stack_copy)
			{
				_mali_mem_free(plbu_stack_copy);
			}
			if (NULL != pointer_array_copy)
			{
				_mali_mem_free(pointer_array_copy);
			}
			job->wait_handle = wait_handle_copy;
			job->sync = sync_copy;
			job->autoFree = autofree_copy;
			job->callback = callback_copy;
			job->callback_argument = callback_argument_copy;
#ifdef MALI_DUMP_ENABLE
			if (MALI_TRUE == revert_dump_setting)
			{
				_mali_common_dump_gp_enable_dump(job_handle, dumping_enabled);
			}
#endif
			return MALI_ERR_FUNCTION_FAILED;
		}

		/*
		 * And finally; start job
		 */
		err = _mali_base_common_gp_job_start(job_handle, priority);
		if (MALI_ERR_NO_ERROR != err)
		{
			_mali_mem_free(plbu_stack_copy);
			_mali_mem_free(pointer_array_copy);
			_mali_base_arch_sys_wait_handle_trigger(job->wait_handle);
			_mali_base_arch_sys_wait_handle_wait(job->wait_handle);
			job->wait_handle = wait_handle_copy;
			job->sync = sync_copy;
			job->autoFree = autofree_copy;
			job->callback = callback_copy;
			job->callback_argument = callback_argument_copy;
#ifdef MALI_DUMP_ENABLE
			if (MALI_TRUE == revert_dump_setting)
			{
				_mali_common_dump_gp_enable_dump(job_handle, dumping_enabled);
			}
#endif
			return err;
		}

		/*
		* Wait for the job to complete before we try to start the next.
		* Waiting for a wait handle will automatically delete it when it has been triggered.
		*/
		_mali_base_arch_sys_wait_handle_wait(wait_handle);
		if (MALI_JOB_STATUS_END_SUCCESS != job_ctx->job_result && MALI_JOB_STATUS_END_OOM != job_ctx->job_result)
		{
			break; /* no need to continue */
		}
	}

	/*
	 * Clean-up and restore
	 */
	if (NULL != plbu_stack_copy)
	{
		_mali_mem_free(plbu_stack_copy);
	}
	if (NULL != pointer_array_copy)
	{
		_mali_mem_free(pointer_array_copy);
	}
	job->wait_handle = wait_handle_copy;
	job->sync = sync_copy;
	job->autoFree = autofree_copy;
	job->callback = callback_copy;
	job->callback_argument = callback_argument_copy;
#ifdef MALI_DUMP_ENABLE
	if (MALI_TRUE == revert_dump_setting)
	{
		_mali_common_dump_gp_enable_dump(job_handle, dumping_enabled);
	}
#endif

	/*
	 * call the post processing function of the original job
	 */
	_mali_base_common_gp_job_run_postprocessing_job(job, job_ctx->job_result);

	return MALI_ERR_NO_ERROR;
}

MALI_TEST_EXPORT
u32* _mali_base_common_instrumented_gp_get_results_pointer(mali_gp_job_handle job_handle)
{
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);
	mali_base_instrumented_gp_context *job_ctx = &job->perf_ctx;
	MALI_DEBUG_ASSERT_POINTER(job_ctx);
	return job_ctx->perf_values;
}

MALI_TEST_EXPORT
void _mali_base_common_instrumented_gp_set_results_pointer(mali_gp_job_handle job_handle, u32 *res)
{
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);
	MALI_DEBUG_ASSERT_POINTER(job_handle);

	job->perf_ctx.perf_values = res;
}

MALI_TEST_EXPORT
void _mali_base_common_instrumented_gp_set_pointer_array_handle(mali_gp_job_handle job_handle, mali_mem_handle pointer_array_handle)
{
	mali_base_instrumented_gp_context *job_ctx;
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT_POINTER(pointer_array_handle);

	job_ctx = &job->perf_ctx;
	job_ctx->pointer_array = pointer_array_handle;
}

MALI_TEST_EXPORT
void _mali_base_common_instrumented_gp_set_plbu_stack_handle(mali_gp_job_handle job_handle, mali_mem_handle plbu_stack_handle)
{
	mali_base_instrumented_gp_context *job_ctx;
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT_POINTER(plbu_stack_handle);

	job_ctx = &job->perf_ctx;
	job_ctx->plbu_stack = plbu_stack_handle;
}

mali_bool _mali_base_common_instrumented_gp_is_job_instrumented(mali_gp_job_handle job_handle)
{
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);
	MALI_DEBUG_ASSERT_POINTER(job_handle);

	if (job->perf_ctx.perf_counters_count > 0)
	{
		return MALI_TRUE;
	}

	return MALI_FALSE;
}
