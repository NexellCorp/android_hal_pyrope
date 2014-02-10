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

#include <string.h>

#include <base/common/gp/base_common_gp_job.h>

#if MALI_INSTRUMENTED
#include <base/common/instrumented/base_common_gp_instrumented.h>
#endif /* MALI_INSTRUMENTED */

#include <base/common/mem/base_common_mem.h> /* used function: _mali_base_common_heap_intern_set_last_heap_addr */
#include <base/arch/base_arch_gp.h>
#include "mali_osu.h"
#include "mali_uku.h"

/* common backend code */
#include "base_arch_main.h"

#if MALI_PERFORMANCE_KIT
int mali_simple_hw_perf = -1;
int mali_custom_hw_perf = -1;
int mali_print_custom_hw_perf = -1;
int mali_custom_perfcount_gp_0 = -1;
int mali_custom_perfcount_gp_1 = -1;
#endif

mali_err_code _mali_base_arch_gp_open(void)
{
	return MALI_ERR_NO_ERROR;
}

void _mali_base_arch_gp_close(void)
{
	return ;
}

mali_err_code _mali_base_arch_gp_start(mali_gp_job * job)
{
	_mali_osk_errcode_t err;
	_vr_uk_gp_start_job_s call_argument;
	_mali_sys_memset(&call_argument, 0, sizeof(_vr_uk_gp_start_job_s));

    call_argument.ctx = mali_uk_ctx;
	call_argument.user_job_ptr = MALI_REINTERPRET_CAST(u32)job;
	call_argument.priority = job->priority;
    call_argument.frame_builder_id = job->frame_builder_id;
    call_argument.flush_id = job->flush_id;

	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT( (NULL != job->vs_cmd_list) || (NULL != job->plbu_cmd_list), ("Arch requested to start a GP job without any command lists"));
	MALI_DEBUG_ASSERT(sizeof(call_argument.frame_registers) == sizeof(job->registers), ("Register size mismatch for GP jobs in Base/DD interface"));

#if defined(__SYMBIAN32__)
    _mali_osu_memcpy(&call_argument.frame_registers[0], &job->registers[0], sizeof(call_argument.frame_registers));
#else
	memcpy(&call_argument.frame_registers[0], &job->registers[0], sizeof(call_argument.frame_registers));
#endif

	/* add frame builder id & flush id */
	call_argument.frame_builder_id 	= job->frame_builder_id;
	call_argument.flush_id 			= job->flush_id;

#if MALI_INSTRUMENTED
	{
		mali_base_instrumented_gp_context *perf_ctx;
		perf_ctx = &job->perf_ctx;

		MALI_DEBUG_ASSERT(perf_ctx->num_counters_to_read == 0 || perf_ctx->num_counters_to_read == 1 || perf_ctx->num_counters_to_read == 2, ("There should be either 1 or 2 counters to read here."));

		if (perf_ctx->num_counters_to_read > 0)
		{
			call_argument.perf_counter_flag = _MALI_PERFORMANCE_COUNTER_FLAG_SRC0_ENABLE;
			call_argument.perf_counter_src0 = perf_ctx->counter_id[0];
			if (perf_ctx->num_counters_to_read == 2)
			{
				call_argument.perf_counter_flag |= _MALI_PERFORMANCE_COUNTER_FLAG_SRC1_ENABLE;
				call_argument.perf_counter_src1 = perf_ctx->counter_id[1];
			}
		}
	}

#elif MALI_PERFORMANCE_KIT

	if (mali_simple_hw_perf == -1)
	{
		mali_simple_hw_perf = (int)_mali_sys_config_string_get_s64("MALI_SIMPLE_HW_PERF", 0, 0, 1);
	}

	if (mali_custom_hw_perf == -1)
	{
		mali_custom_hw_perf = (int)_mali_sys_config_string_get_s64("MALI_CUSTOM_HW_PERF", 0, 0, 1);
		if(mali_custom_hw_perf == 1)
		{
			mali_print_custom_hw_perf = (int)_mali_sys_config_string_get_s64("MALI_PRINT_CUSTOM_HW_PERF", 0, 0, 1);
		}
		else
		{
			mali_print_custom_hw_perf = 0;
		}
	}


	if (mali_simple_hw_perf == 1)
	{
		call_argument.perf_counter_flag = _MALI_PERFORMANCE_COUNTER_FLAG_SRC0_ENABLE;
		call_argument.perf_counter_src0 = 1; /* active cycle counter */
	}
	else if (mali_custom_hw_perf == 1)
	{
		/* obtain environment variables*/
		mali_custom_perfcount_gp_0 = (int)_mali_sys_config_string_get_s64("MALI_GP_PERFCOUNT0", 0, 0, 999);
		mali_custom_perfcount_gp_1 = (int)_mali_sys_config_string_get_s64("MALI_GP_PERFCOUNT1", 0, 0, 999);

		call_argument.perf_counter_flag = _MALI_PERFORMANCE_COUNTER_FLAG_SRC0_ENABLE;
		call_argument.perf_counter_src0 = mali_custom_perfcount_gp_0; /* active cycle counter */
		call_argument.perf_counter_flag |= _MALI_PERFORMANCE_COUNTER_FLAG_SRC1_ENABLE;
		call_argument.perf_counter_src1 = mali_custom_perfcount_gp_1; /* active cycle counter */

	}
#endif /* MALI_PERFORMANCE_KIT */

    err = _mali_uku_gp_start_job(&call_argument);
    MALI_CHECK(_MALI_OSK_ERR_OK == err, MALI_ERR_FUNCTION_FAILED);

	return MALI_ERR_NO_ERROR;
}

u32 _mali_base_arch_gp_get_core_nr(void)
{
    _vr_uk_get_gp_number_of_cores_s args = {NULL,0};
    args.ctx = mali_uk_ctx;
    MALI_CHECK(_MALI_OSK_ERR_OK == _mali_uku_get_gp_number_of_cores( &args ), 0);
    return args.number_of_cores;
}

u32 _mali_base_arch_gp_get_core_version(void)
{
    _vr_uk_get_gp_core_version_s args = {NULL,0};
    args.ctx = mali_uk_ctx;
    MALI_CHECK(_MALI_OSK_ERR_OK == _mali_uku_get_gp_core_version( &args ), 0);
    return args.version;
}

u32 _mali_base_arch_gp_get_product_id(void)
{
    _vr_uk_get_gp_core_version_s args = {NULL,0};
    args.ctx = mali_uk_ctx;
    MALI_CHECK(_MALI_OSK_ERR_OK == _mali_uku_get_gp_core_version( &args ), 0);
    return args.version >> 16;
}

void _mali_base_arch_gp_event_handler(u32 event_id, void * event_data)
{
	if (_VR_NOTIFICATION_GP_STALLED == event_id)
	{
		_vr_uk_gp_job_suspended_s * info;
		info = MALI_REINTERPRET_CAST(_vr_uk_gp_job_suspended_s *)event_data;
		MALI_DEBUG_PRINT(6, ("Job gp got out of memory event!\n"));
		_mali_base_common_gp_job_suspended_event(MALI_REINTERPRET_CAST(mali_gp_job*)info->user_job_ptr, (mali_base_common_gp_job_suspend_reason)info->reason, info->cookie);
	}
	else if (_VR_NOTIFICATION_GP_FINISHED == event_id)
	{
		mali_gp_job *job;
		_vr_uk_gp_job_finished_s * result;

		result = MALI_REINTERPRET_CAST(_vr_uk_gp_job_finished_s *)event_data;

		job = MALI_REINTERPRET_CAST(mali_gp_job*)result->user_job_ptr;

#if MALI_INSTRUMENTED

		{
			mali_base_instrumented_gp_context *perf_ctx;

			perf_ctx = &job->perf_ctx;

			MALI_DEBUG_ASSERT(perf_ctx->num_counters_to_read == 0 || perf_ctx->num_counters_to_read == 1 || perf_ctx->num_counters_to_read == 2, ("There should be either 1 or 2 counters to read here."));

			if (perf_ctx->num_counters_to_read > 0)
			{

				*perf_ctx->counter_result[0] = result->perf_counter0;
				if (perf_ctx->num_counters_to_read == 2)
				{
					*perf_ctx->counter_result[1] = result->perf_counter1;
				}
			}
		}

#elif MALI_PERFORMANCE_KIT

		if (mali_simple_hw_perf == 1)
		{
			_mali_sys_printf("GP cycles: %u \n", result->perf_counter0);
		}
		else if (mali_print_custom_hw_perf == 1) /* implies mali_custom_hw_perf */
		{
			_mali_sys_printf("GP counter %i: %u, ", mali_custom_perfcount_gp_0, result->perf_counter0);
			_mali_sys_printf("GP counter %i: %u \n", mali_custom_perfcount_gp_1, result->perf_counter1);
		}
#endif

#if !defined(HARDWARE_ISSUE_3251)
		if (NULL != job->plbu_heap)
		{
			_mali_base_common_heap_intern_set_last_heap_addr(job->plbu_heap, result->heap_current_addr);
		}
#endif

		MALI_DEBUG_CODE(
		switch (result->status)
		{
			case _VR_UK_JOB_STATUS_END_SUCCESS:
				MALI_DEBUG_PRINT(6, ("Mali gp job returned, OK!\n"));
				break;
			case _VR_UK_JOB_STATUS_END_OOM:
				MALI_DEBUG_PRINT(2, ("Mali gp job returned unfinished, OOM\n"));
				break;
			case _VR_UK_JOB_STATUS_END_ABORT:
				MALI_DEBUG_PRINT(2, ("Mali gp job returned unfinished, ABORTED\n"));
				break;
			case _VR_UK_JOB_STATUS_END_TIMEOUT_SW:
				MALI_DEBUG_PRINT(2, ("Mali gp job returned unfinished, TIMEOUT_SW\n"));
				break;
			case _VR_UK_JOB_STATUS_END_HANG:
				MALI_DEBUG_PRINT(2, ("Mali gp job returned unfinished, HW HANG\n"));
				break;
			case _VR_UK_JOB_STATUS_END_SEG_FAULT:
				MALI_DEBUG_PRINT(2, ("Mali gp job returned unfinished, SEG_FAULT\n"));
				break;
			case _VR_UK_JOB_STATUS_END_ILLEGAL_JOB:
				MALI_DEBUG_PRINT(2, ("Mali gp job returned unfinished, ILLEGAL JOB\n"));
				break;
			case _VR_UK_JOB_STATUS_END_UNKNOWN_ERR:
				MALI_DEBUG_PRINT(2, ("Mali gp job returned unfinished, INTERNAL DEVICEDRIVER ERR\n"));
				break;
			case _VR_UK_JOB_STATUS_END_SHUTDOWN:
				MALI_DEBUG_PRINT(2, ("Mali gp job returned unfinished, SHUTDOWN\n"));
				break;
			case _VR_UK_JOB_STATUS_END_SYSTEM_UNUSABLE:
				MALI_DEBUG_PRINT(1, ("Mali gp job returned unfinished, SYSTEM_UNUSABLE\n"));
				break;
			default:
				MALI_DEBUG_PRINT(1, ("Mali gp job returned unfinished, UNKNOWN ERR\n"));
				result->status = _VR_UK_JOB_STATUS_END_UNKNOWN_ERR;
		}

		); /* end of MALI_DEBUG_CODE */

		_mali_base_common_gp_job_run_postprocessing(job, (mali_job_completion_status)result->status);
	}
	else
	{
		MALI_DEBUG_PRINT(1, ("Job unknown event id: %d\n", event_id));
	}
}

#if !defined(HARDWARE_ISSUE_3251)
void _mali_base_arch_gp_mem_request_response_new_heap(u32 cookie, u32 new_heap_start, u32 new_heap_end)
{
	_vr_uk_gp_suspend_response_s call_argument;
    _mali_osk_errcode_t err;

    call_argument.ctx = mali_uk_ctx;
	call_argument.cookie = cookie;
	call_argument.arguments[0] = new_heap_start;
	call_argument.arguments[1] = new_heap_end;
	call_argument.code = _VRGP_JOB_RESUME_WITH_NEW_HEAP;

	err = _mali_uku_gp_suspend_response( &call_argument );
	/* we handle both success and error the same way here: we defer it to the job completion message (which might already be queued) */
	MALI_IGNORE(err);
}
#endif

void _mali_base_arch_gp_mem_request_response_abort(u32 cookie)
{
	_vr_uk_gp_suspend_response_s call_argument;
    _mali_osk_errcode_t err;

    call_argument.ctx = mali_uk_ctx;
	call_argument.cookie = cookie;
	call_argument.arguments[0] = 0;
	call_argument.arguments[1] = 0;
	call_argument.code = _VRGP_JOB_ABORT;

    err = _mali_uku_gp_suspend_response( &call_argument );
	/* we handle both success and error the same way here: we defer it to the job completion message (which might already be queued) */
	MALI_IGNORE(err);
}

