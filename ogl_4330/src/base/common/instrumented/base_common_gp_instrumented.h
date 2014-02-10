/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_gp_instrumented.h
 * The common layer contains the implementation of gp instrumented functions.
 * Function-calls are passed through the frontend to common layer.
 */

#ifndef _BASE_COMMON_GP_INSTRUMENTED_H_
#define _BASE_COMMON_GP_INSTRUMENTED_H_

#include "mali_system.h"

#include "base/instrumented/mali_gp_instrumented_internal.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct mali_base_instrumented_gp_context
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
	 * the number of counters to be read by this job, which is either 1 or 2.
	 *
	 * pointer_array contains a pointer to the actual pointer array that
	 * gets used for the job. for the duplicated jobs, this handle points to a
	 * memory block copy of the original pointer array. the original pointer
	 * array is reset before each duplicate run, so that the final outcome will
	 * reflect a single run of the original job.
	 * The same applies for the plbu_stack mem_handle.
	 */
	u32 *                           perf_counters;
	u32                             perf_counters_count;
	u32 *                           perf_values;

	u32                             counter_id[2];
	u32                             num_counters_to_read;
	u32 *                           counter_result[2];

#if !defined(USING_MALI200)
	u32                             l2_perf_counters_count;
	u32                             l2_counter_id[2];
	u32                             l2_perf_values[2];
#endif

	mali_job_completion_status		job_result;

	/* PLBU stack and pointer array for copying back */
	mali_mem_handle                 plbu_stack;
	mali_mem_handle                 pointer_array;

} mali_base_instrumented_gp_context;


/**
 * Prepare a context by setting some fields in the GP job only included
 * in the instrumented build.
 *
 * @param job_handle                the job to init/reset instrumented context for
 */
MALI_TEST_IMPORT
void _mali_base_common_instrumented_gp_context_init(mali_gp_job_handle job_handle);

/**
 * Start the job with instrumentation enabled.
 *
 * The job will be duplicated as many times as needed to fill all the perf_counters set with
 * _mali_base_common_instrumented_gp_job_set_counters.
 *
 * @param job_handle                the job to start
 * @param priority                  what priority to run the job with
 */
mali_err_code _mali_base_common_instrumented_gp_job_start(
		mali_gp_job_handle job_handle,
		mali_job_priority priority);


/**
 * Setting the array that tells which counters to read for a job.
 * The perf_counters array tells which hardware counters on mali we will
 * readout after the job has run, and other things that can be profiled on
 * the job.
 * @note It is assumed that the perf_counters array will stay alive until
 * the job has finished running
 *
 * @param job_handle                gp job handle
 * @param perf_counters             an array with the counter indices to read
 * @param perf_counters_count       the number of counters in the perf_counters array
 */
MALI_TEST_IMPORT
void _mali_base_common_instrumented_gp_job_set_counters(
		mali_gp_job_handle job_handle,
		u32 *perf_counters,
		u32 perf_counters_count);

/**
 * Get the counters array set with gp_job_set_counters
 *
 * @param job_handle                gp job handle
 * @param perf_counters             pointer to location where array  is stored
 * @param perf_counters_count       pointer to location where number of counters is stored
 */
MALI_TEST_IMPORT
void _mali_base_common_instrumented_gp_job_get_counters(
		mali_gp_job_handle job_handle,
		u32 **perf_counters,
		u32 *perf_counters_count);

/**
 * Function to get the instrumented results after a job has been run.
 *
 * @param job_handle                the relevant job
 * @return                          a pointer to the results array
 */
MALI_TEST_IMPORT
u32* _mali_base_common_instrumented_gp_get_results_pointer(
		mali_gp_job_handle job_handle);

/**
 * Set the results pointer which will be filled with performance values
 *
 * @param job                       The relevant job
 * @param result                    The results to set
 */
MALI_TEST_IMPORT
void _mali_base_common_instrumented_gp_set_results_pointer(
		mali_gp_job_handle job_handle,
		u32 *res);

/**
 * Set the handle to the pointer array in the job. This is needed
 * for the instrumented job to run correctly
 * @param job                       The relevant job
 * @param pointer_array_handle      The mem_handle to the pointer array
 */
MALI_TEST_IMPORT
void _mali_base_common_instrumented_gp_set_pointer_array_handle(
		mali_gp_job_handle job_handle,
		mali_mem_handle pointer_array_handle);

/**
 * Set the handle to the plbu stack in the job. This is needed
 * for the instrumented job to run correctly
 * @param job                       The relevant job
 * @param pointer_array_handle      The mem_handle to the pointer array
 */
MALI_TEST_IMPORT
void _mali_base_common_instrumented_gp_set_plbu_stack_handle(
		mali_gp_job_handle job_handle,
		mali_mem_handle plbu_stack_handle);

/**
 * Return whether or not this GP job has had any performance counters
 * enabled
 * @param job_handle                the job
 * \return MALI_TRUE if job has any performance counters enabled, MALI_FALSE if not.
 */
mali_bool _mali_base_common_instrumented_gp_is_job_instrumented(
		mali_gp_job_handle job_handle);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _BASE_COMMON_GP_INSTRUMENTED_H_ */
