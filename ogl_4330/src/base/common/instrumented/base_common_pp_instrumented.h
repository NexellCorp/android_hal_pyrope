/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _BASE_COMMON_PP_INSTRUMENTED_H_
#define _BASE_COMMON_PP_INSTRUMENTED_H_

#include "base/mali_context.h"
#include "base/mali_types.h"
#include <regs/MALI200/mali200_core.h>
#include "base/instrumented/mali_pp_instrumented_internal.h"
#include <sync/mali_external_sync.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mali_base_instrumented_pp_context
{

	/* perf_counters contains the activated counters. The values contain
	 * different values, depending on if it's the original job or a duplicated
	 * job:
	 *
	 * on the original job, perf_values stores the actual values of the
	 * counters. perf_counters_count stores the number of counters to be read
	 * in total, [1-u32_max>.
	 *
	 * On the duplicated jobs, perf_values points to the location in
	 * the original job where the results should be stored. counter_id stores
	 * the ids of the hardware registers to be read. perf_counters_count stores
	 * the number of counters to be read by this job, which is either 1 or 2.*/
	u32                            *perf_counters;
	u32                             perf_counters_count;
	u32                            *perf_values[_MALI_PP_MAX_SUB_JOBS];
	u32                             counter_id[2];

#if !defined(USING_MALI200)
	u32                             l2_perf_counters_count;
	u32                             l2_counter_id[2];
	u32                             l2_perf_values[2];
	u32                             l2_perf_values_raw[2];
#endif

	mali_mem_handle					wbx_mem[MALI200_WRITEBACK_UNIT_COUNT];
} mali_base_instrumented_pp_context;

/**
 * Prepare a context by setting some fields in the PP job only included
 * in the instrumented build.
 *
 * @param job_handle                the job to init/reset instrumented context for
 */
void _mali_base_common_instrumented_pp_context_init(mali_pp_job_handle job_handle);

/**
 * Start the job with instrumentation enabled. The job will be duplicated
 * as many times as needed to fill all the perf_counters set with
 * _mali_base_common_instrumented_pp_job_set_counters.
 *
 * @param job_handle                the job to start
 * @param priority                  what priority to run the job with
 */
void _mali_base_common_instrumented_pp_job_start(
		mali_pp_job_handle job_handle,
		mali_job_priority priority,
		mali_fence_handle *fence);

/**
 * Setting the array that tells which counters to read for a job.
 * The perf_counters array tells which hardware counters on mali we will
 * readout after the job has run, and other things that can be profiled on
 * the job.
 * @note It is assumed that the perf_counters array will stay alive until
 * the job has finished running
 *
 * @param job_handle                PP job handle
 * @param perf_counters             an array with the counter indices to read
 * @param perf_counters_count       the number of counters in the perf_counters array
 * @param res                       a two dimensional array of (number-of-subjobs) rows and (number-of counters) coloumns
 *                                  - used for storing counter values
 */
MALI_TEST_IMPORT
void _mali_base_common_instrumented_pp_job_setup(mali_pp_job_handle job_handle, u32 *perf_counters, u32 perf_counters_count, u32 **res);

/**
 * Get the counters array set with pp_job_setup
 *
 * @param job_handle                PP job handle
 * @num_cores                       number of PP cores on which the job is run i.e. number of PP subjobs
 * @param perf_counters             pointer to location where array  is stored
 * @param perf_counters_count       pointer to location where number of counters is stored
 * @param res                       a pointer to an array of up to _MALI_PP_MAX_SUB_JOBS number of pointers to u32 elements;
 *                                  - on return, each element will point to an array of counter values for the corresponding sub job
 */
MALI_TEST_IMPORT
void _mali_base_common_instrumented_pp_job_get_data( mali_pp_job_handle job_handle, u32 *num_cores, u32 **perf_counters, u32 *perf_counters_count, u32 **res);

MALI_IMPORT
void _mali_base_common_instrumented_pp_job_set_wbx_mem(mali_pp_job_handle job_handle, u32 wb_unit, mali_mem_handle mem);

/**
 * Return whether or not this PP job has had any performance counters
 * enabled
 * @param job_handle                the job
 * \return MALI_TRUE if job has any performance counters enabled, MALI_FALSE if not.
 */
mali_bool _mali_base_common_instrumented_pp_is_job_instrumented(
		mali_pp_job_handle job_handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _BASE_COMMON_PP_INSTRUMENTED_H_ */
