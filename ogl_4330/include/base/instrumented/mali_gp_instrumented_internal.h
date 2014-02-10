/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _MALI_BASE_GP_INSTRUMENTED_H_
#define _MALI_BASE_GP_INSTRUMENTED_H_

#include "mali_system.h"
#include "base/gp/mali_gp_handle.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Init/reset the instrumented context for the specified GP job
 *
 * \param job_handle GP job to init/reset instrumented context for
 */
MALI_IMPORT
void _mali_instrumented_gp_context_init(mali_gp_job_handle job_handle);

/**
 * Set the array that tells which counters to read for a job.
 *
 * The perf_counters array tells which counters on mali we will readout after
 * the job has run.
 *
 * \param job_handle gp job handle
 * \param perf_counters an array with the counter indices to read
 * \param perf_counters_count the number of counters in the perf_counters array
 */
MALI_IMPORT
void _mali_instrumented_gp_job_set_counters(
		mali_gp_job_handle job_handle,
		u32 *perf_counters,
		u32 perf_counters_count);

/**
 * Get the array that tells which counters to read / has been read for a job.
 *
 * The perf_counters array tells which counters on mali we will readout after
 * the job has run.
 *
 * \param job_handle gp job handle
 * \param perf_counters returns the array with the counter indices to read
 * \param perf_counters_count returns the number of counters in the perf_counters array
 */
MALI_IMPORT
void _mali_instrumented_gp_job_get_counters(
		mali_gp_job_handle job_handle,
		u32 **perf_counters,
		u32 *perf_counters_count);

/**
 * Set the instrumented results before the job has been run.
 * @param job_handle job to set results pointer on
 * @param res an array of u32 values that will be filled with results when
 *            the job is run
 */
MALI_IMPORT
void _mali_instrumented_gp_set_results_pointer(
		mali_gp_job_handle job_handle,
		u32 *res);

/**
 * Get the instrumented results after a job has been run.
 *
 * \param job_handle job to get results from
 * \return an array of u32 values with the results, in the same order as
 *         the counters from _mali_instrumented_gp_job_set_counters.
 */
MALI_IMPORT
u32* _mali_instrumented_gp_get_results_pointer(
		mali_gp_job_handle job_handle);

/**
 * Set pointer array on instrumented gp job.
 *
 * The instrumented job keeps a mali_mem_handle reference to the pointer
 * array allocated by the frame builder, so that it can overwrite it after
 * each run of the job.
 *
 * \param job_handle job to set pointer array on
 * \param pointer_array_handle the pointer array
 */
MALI_IMPORT
void _mali_instrumented_gp_set_pointer_array_handle(
		mali_gp_job_handle job_handle,
		mali_mem_handle pointer_array_handle);

/**
 * Set plbu stack on instrumented gp job.
 *
 * The instrumented job keeps a mali_mem_handle reference to the plbu
 * stack allocated by the frame builder, so that it can overwrite it after
 * each run of the job.
 *
 * \param job_handle job to set plbu stack on
 * \param plbu_stack_handle the plbu stack
 */
MALI_IMPORT
void _mali_instrumented_gp_set_plbu_stack_handle(
		mali_gp_job_handle job_handle,
		mali_mem_handle plbu_stack_handle);

/**
 * Find out whether a GP job is instrumented.
 *
 * \param job_handle GP job to check for instrumentation
 * \return MALI_TRUE if job is instrumented, MALI_FALSE if not.
 */
MALI_IMPORT
mali_bool _mali_instrumented_gp_is_job_instrumented(
		mali_gp_job_handle job_handle);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MALI_BASE_GP_INSTRUMENTED_H_ */
