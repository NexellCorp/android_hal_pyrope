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

/* pp interface */
#include <base/common/pp/base_common_pp_job.h>
#include <base/arch/base_arch_pp.h>

#include <sync/mali_external_sync.h>

#include "mali_osu.h"
#include "mali_uku.h"
/* common backend code */
#include "base_arch_main.h"

#if MALI_PERFORMANCE_KIT
extern int mali_simple_hw_perf;
extern int mali_custom_hw_perf;
extern int mali_print_custom_hw_perf;
int mali_custom_perfcount_pp_0 = -1;
int mali_custom_perfcount_pp_1 = -1;
#endif

/* PP interface, see base_arch_pp.h */
mali_err_code _mali_base_arch_pp_open(void)
{
	MALI_SUCCESS;
}
void _mali_base_arch_pp_close(void)
{
	return;
}

#if MALI_INSTRUMENTED
MALI_STATIC void set_perf_counter_src( _vr_uk_pp_start_job_s *call_argument, mali_base_instrumented_pp_context *perf_ctx)
{
	if (perf_ctx->perf_counters_count > 0)
	{
		MALI_DEBUG_ASSERT(perf_ctx->perf_counters_count <= 2, ("If any, there should either be 1 or 2 performance counters enabled in a single job"));
		call_argument->perf_counter_flag = _MALI_PERFORMANCE_COUNTER_FLAG_SRC0_ENABLE;
		call_argument->perf_counter_src0 = perf_ctx->counter_id[0];
		if (perf_ctx->perf_counters_count > 1)
		{
			call_argument->perf_counter_flag |= _MALI_PERFORMANCE_COUNTER_FLAG_SRC1_ENABLE;
			call_argument->perf_counter_src1 = perf_ctx->counter_id[1];
		}
	}
}
#endif /* MALI_INSTRUMENTED */

mali_err_code _mali_base_arch_pp_start(mali_pp_job * job, mali_bool no_notification, mali_fence_handle *fence)
{
	_mali_osk_errcode_t err;
	_vr_uk_pp_start_job_s call_argument;
	_mali_sys_memset(&call_argument, 0, sizeof(_vr_uk_pp_start_job_s));

	call_argument.ctx = mali_uk_ctx;
	call_argument.user_job_ptr = MALI_REINTERPRET_CAST(u32)job;
	call_argument.priority = job->priority;
	call_argument.frame_builder_id = job->frame_builder_id;
	call_argument.flush_id = job->flush_id;
	call_argument.num_cores = job->num_cores;

	if (NULL != job->stream && NULL != fence)
	{
		call_argument.stream = ((mali_base_stream*)job->stream)->fd;
		call_argument.flags |= _VR_PP_JOB_FLAG_FENCE;
	}
	else call_argument.stream = -1;
	call_argument.fence = (s32)job->pre_fence;

	if (MALI_TRUE == job->barrier)
	{
		call_argument.flags |= _VR_PP_JOB_FLAG_BARRIER;
	}

	if (MALI_TRUE == no_notification)
	{
		call_argument.flags |= _VR_PP_JOB_FLAG_NO_NOTIFICATION;
	}

	MALI_DEBUG_ASSERT(sizeof(job->registers.frame_regs) <= sizeof(call_argument.frame_registers), ("Size mismatch in ioctl-interface %d > %d", sizeof(job->registers.frame_regs), sizeof(call_argument.frame_registers)));
	MALI_DEBUG_ASSERT(sizeof(job->registers.wb0_regs) <= sizeof(call_argument.wb0_registers), ("Size mismatch in ioctl-interface %d > %d", sizeof(job->registers.wb0_regs), sizeof(call_argument.wb0_registers)));
	MALI_DEBUG_ASSERT(sizeof(job->registers.wb1_regs) <= sizeof(call_argument.wb1_registers), ("Size mismatch in ioctl-interface %d > %d", sizeof(job->registers.wb1_regs), sizeof(call_argument.wb1_registers)));
	MALI_DEBUG_ASSERT(sizeof(job->registers.wb2_regs) <= sizeof(call_argument.wb2_registers), ("Size mismatch in ioctl-interface %d > %d", sizeof(job->registers.wb2_regs), sizeof(call_argument.wb2_registers)));

	/* copy the registers */
	/* frame registers */
	_mali_sys_memcpy(&call_argument.frame_registers[0], &job->registers.frame_regs[0], sizeof(job->registers.frame_regs));
	_mali_sys_memcpy(&call_argument.frame_registers_addr_frame[0], &job->registers.frame_regs_addr_frame[0], sizeof(call_argument.frame_registers_addr_frame));
	_mali_sys_memcpy(&call_argument.frame_registers_addr_stack[0], &job->registers.frame_regs_addr_stack[0], sizeof(call_argument.frame_registers_addr_stack));

	/* Mali200 also has write-back registers */
	_mali_sys_memcpy(&call_argument.wb0_registers[0], &job->registers.wb0_regs[0], sizeof(job->registers.wb0_regs));
	_mali_sys_memcpy(&call_argument.wb1_registers[0], &job->registers.wb1_regs[0], sizeof(job->registers.wb1_regs));
	_mali_sys_memcpy(&call_argument.wb2_registers[0], &job->registers.wb2_regs[0], sizeof(job->registers.wb2_regs));

#if defined(USING_MALI450)
	/* Dynamic load balancing unit registers */
	/* Tile list virtual base address */
	call_argument.dlbu_registers[0] = job->info.slave_tile_list_mali_address;
	/* Framebuffer dimensions */
	call_argument.dlbu_registers[1] = (job->info.master_x_tiles - 1) | ((job->info.master_y_tiles-1)<<16);
	/* Tile list configuration */
	call_argument.dlbu_registers[2] = job->info.binning_pow2_x | (job->info.binning_pow2_y<<16) | (job->info.size<<28);
	/* Start tile positions */
	call_argument.dlbu_registers[3] = 0 | (0<<8) | ((job->info.master_x_tiles-1)<<16) | ((job->info.master_y_tiles-1)<<24);
#endif /* defined(USING_MALI450) */

	/* add frame builder id & flush id */
	call_argument.frame_builder_id 	= job->frame_builder_id;
	call_argument.flush_id 			= job->flush_id;

#if MALI_INSTRUMENTED
	set_perf_counter_src( &call_argument, &(job->perf_ctx));
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
		call_argument.perf_counter_flag |= _MALI_PERFORMANCE_COUNTER_FLAG_SRC0_ENABLE;
		call_argument.perf_counter_src0 = 0; /* Active cycle counter */

	}
	else if (mali_custom_hw_perf == 1)
	{
		/* Obtain environment variables */
		mali_custom_perfcount_pp_0 = (int)_mali_sys_config_string_get_s64("MALI_PP_PERFCOUNT0", 0, 0, 999);
		mali_custom_perfcount_pp_1 = (int)_mali_sys_config_string_get_s64("MALI_PP_PERFCOUNT1", 0, 0, 999);
		call_argument.perf_counter_flag |= _MALI_PERFORMANCE_COUNTER_FLAG_SRC0_ENABLE;
		call_argument.perf_counter_src0 = mali_custom_perfcount_pp_0; /* Active cycle counter */
		call_argument.perf_counter_flag |= _MALI_PERFORMANCE_COUNTER_FLAG_SRC1_ENABLE;
		call_argument.perf_counter_src1 = mali_custom_perfcount_pp_1; /* Active cycle counter */

	}
#endif /* MALI_PERFORMANCE_KIT */

    err = _mali_uku_pp_start_job(&call_argument);
    MALI_CHECK(_MALI_OSK_ERR_OK == err, MALI_ERR_FUNCTION_FAILED);

	if (NULL != fence) *fence = call_argument.fence;

    return MALI_ERR_NO_ERROR;
}

u32 _mali_base_arch_pp_get_num_cores(void)
{
	static u32 num_cores_cached = (u32)-1;

	if ((u32)-1 == num_cores_cached)
	{
		_vr_uk_get_pp_number_of_cores_s args = { NULL, 0 };
		args.ctx = mali_uk_ctx;
		MALI_CHECK(_MALI_OSK_ERR_OK == _mali_uku_get_pp_number_of_cores( &args ), 0);
		#if 1 /* org, rel0 */
		num_cores_cached = args.number_of_cores;
		#else /* nexell, rel4 */
		num_cores_cached = args.number_of_enabled_cores;
		#endif
	}

	return num_cores_cached;
}

u32 _mali_base_arch_pp_get_core_version(void)
{
    _vr_uk_get_pp_core_version_s args = { NULL, 0 };
    args.ctx = mali_uk_ctx;
    MALI_CHECK(_MALI_OSK_ERR_OK == _mali_uku_get_pp_core_version( &args ), 0);
    return args.version;
}

u32 _mali_base_arch_pp_get_product_id(void)
{
    _vr_uk_get_pp_core_version_s args = { NULL, 0 };
    args.ctx = mali_uk_ctx;
    MALI_CHECK(_MALI_OSK_ERR_OK == _mali_uku_get_pp_core_version( &args ), 0);
    return args.version >> 16;
}

void _mali_base_arch_pp_event_handler(u32 event_id, void * event_data)
{
 	mali_pp_job *job;
    _vr_uk_pp_job_finished_s *result;

    result = MALI_REINTERPRET_CAST(_vr_uk_pp_job_finished_s *)event_data;
	job = MALI_REINTERPRET_CAST(mali_pp_job *)result->user_job_ptr;

	MALI_DEBUG_CODE(
			switch (result->status)
			{
			case _VR_UK_JOB_STATUS_END_SUCCESS:
			MALI_DEBUG_PRINT(6, ("Mali pp job returned, OK!\n"));
			break;
			case _VR_UK_JOB_STATUS_END_OOM:
			MALI_DEBUG_PRINT(2, ("Mali pp job returned unfinished, OOM\n"));
			break;
			case _VR_UK_JOB_STATUS_END_ABORT:
			MALI_DEBUG_PRINT(2, ("Mali pp job returned unfinished, ABORTED\n"));
			break;
			case _VR_UK_JOB_STATUS_END_TIMEOUT_SW:
			MALI_DEBUG_PRINT(2, ("Mali pp job returned unfinished, TIMEOUT_SW\n"));
			break;
			case _VR_UK_JOB_STATUS_END_HANG:
			MALI_DEBUG_PRINT(2, ("Mali pp job returned unfinished, HW HANG\n"));
			break;
			case _VR_UK_JOB_STATUS_END_SEG_FAULT:
			MALI_DEBUG_PRINT(2, ("Mali pp job returned unfinished, SEG_FAULT\n"));
			break;
			case _VR_UK_JOB_STATUS_END_ILLEGAL_JOB:
			MALI_DEBUG_PRINT(2, ("Mali pp job returned unfinished, ILLEGAL JOB\n"));
			break;
			case _VR_UK_JOB_STATUS_END_UNKNOWN_ERR:
			MALI_DEBUG_PRINT(2, ("Mali pp job returned unfinished, INTERNAL DEVICEDRIVER ERR\n"));
			break;
			case _VR_UK_JOB_STATUS_END_SHUTDOWN:
			MALI_DEBUG_PRINT(2, ("Mali pp job returned unfinished, SHUTDOWN\n"));
			break;
			case _VR_UK_JOB_STATUS_END_SYSTEM_UNUSABLE:
			MALI_DEBUG_PRINT(1, ("Mali pp job returned unfinished, SYSTEM_UNUSABLE\n"));
			break;
			default:
			MALI_DEBUG_PRINT(1, ("Mali pp job returned unfinished, UNKNOWN ERR\n"));
			result->status = _VR_UK_JOB_STATUS_END_UNKNOWN_ERR;
			}
	); /* end of MALI_DEBUG_CODE */

#if MALI_INSTRUMENTED

	if (job->perf_ctx.perf_counters_count > 0)
	{
		u32 i;

		MALI_DEBUG_ASSERT(job->perf_ctx.perf_counters_count <= 2, ("If any, there should either be 1 or 2 performance counters enabled in a single job"));

		for (i = 0; i < job->num_cores; i++)
		{
			job->perf_ctx.perf_values[i][0] = result->perf_counter0[i];
		}

		if (job->perf_ctx.perf_counters_count > 1)
		{
			for (i = 0; i < job->num_cores; i++)
			{
				job->perf_ctx.perf_values[i][1] = result->perf_counter1[i];
			}
		}

	}

#elif MALI_PERFORMANCE_KIT

	if (mali_simple_hw_perf == 1)
	{
		_mali_sys_printf("PP cycles: %u, ", result->perf_counter0);
	}
	else if (mali_print_custom_hw_perf == 1) /* implies mali_custom_hw_perf */
	{
		_mali_sys_printf("PP counter %i: %u, ", mali_custom_perfcount_pp_0, result->perf_counter0);
		_mali_sys_printf("PP counter %i: %u \n", mali_custom_perfcount_pp_1, result->perf_counter1);
	}

#endif

	_mali_base_common_pp_job_run_postprocessing(job, (mali_job_completion_status)result->status);
}

void _mali_base_arch_pp_job_disable_wb(u32 fb_id, u32 flush_id, mali_pp_job_wbx_flag wbx)
{
	_vr_uk_pp_disable_wb_s call_argument;

	call_argument.ctx = mali_uk_ctx;
	call_argument.fb_id = fb_id;
	call_argument.flush_id = flush_id;
	call_argument.wbx = (_vr_uk_pp_job_wbx_flag)wbx;

    _mali_uku_pp_job_disable_wb(&call_argument);
}
