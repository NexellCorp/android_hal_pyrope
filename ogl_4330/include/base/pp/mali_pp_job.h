/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_pp_job.h
 * PP job system.
 * All access to the mali hardware goes through a job system. Jobs are created by the user and queued for execution.
 *
 */

#ifndef _MALI_PP_JOB_H_
#define _MALI_PP_JOB_H_

#include <base/arch/base_arch_pp.h>
#include <sync/mali_external_sync.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the PP job system. Returns a base context handle.
 * Each _mali_pp_open call must be matched with a call to _mali_pp_close.
 * It's safe to call this function multiple times.
 * @see _mali_pp_close()
 * @param ctx The base context to scope the PP usage to
 * @return A standard Mali error code
 */
MALI_CHECK_RESULT MALI_STATIC_FORCE_INLINE mali_err_code _mali_pp_open(mali_base_ctx_handle ctx)
{
        MALI_CHECK_NON_NULL(ctx, MALI_ERR_FUNCTION_FAILED);
        MALI_SUCCESS;
}

/**
 * Close a reference to the PP job system. Match each call to _mali_pp_open with a call to this function.
 * @see _mali_pp_open()
 * @param ctx The mali base context handle used in the open call
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_close(mali_base_ctx_handle ctx)
{
	MALI_IGNORE(ctx);
	return;
}

/**
 * Get a new PP job struct.
 * Returns a pointer to a new mali_pp_job.
 * When you are finished with the job it needs to be released.
 * This can either be done with setting the auto_free member variable or
 * calling the _mali_pp_job_free function.
 * @param ctx The mali base context handle obtained in a open call earlier
 * @param num_cores Number of cores in a multi-core system, values in the range 1 through 4 supported
 * @param stream Handle of the stream the job belongs to
 * @return Handle to a job on success, NULL on failure
 */
MALI_STATIC_FORCE_INLINE mali_pp_job_handle _mali_pp_job_new(mali_base_ctx_handle ctx, int num_cores, mali_stream_handle stream)
{
	return _mali_base_common_pp_job_new(ctx, num_cores, stream);
}

/**
 * Release a PP job struct.
 * Returns the PP job struct to the base system and frees up any resources used by it.
 * Any attached resources will also be freed like upon a normal job completion.
 * @param job The job to release
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_free(mali_pp_job_handle job)
{
	_mali_base_common_pp_job_free(job);
}

/**
 * Reset a PP job
 * Resets the PP job to same the state as when newly allocated.
 * Any attached resources will be freed like upon a normal job completion.
 * @param job_handle Handle for a PP job
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_reset(mali_pp_job_handle job_handle)
{
	_mali_base_common_pp_job_reset(job_handle);
}

/**
 * Add a mali_mem_handle to a job's free-on-termination list
 * @param job Handle to the job to update
 * @param mem Handle to add to list
 * @return void
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_add_mem_to_free_list(mali_pp_job_handle job, mali_mem_handle mem)
{
	_mali_base_common_pp_job_add_mem_to_free_list(job, mem);
}

/**
 * Set callback for a pp job
 * @param job Handle to the job to update
 * @param func Function to set as callback func
 * @param cp_param Argument passed to callback
 * @return void
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_set_callback(mali_pp_job_handle job, mali_cb_pp func, void * cb_param)
{
	_mali_base_common_pp_job_set_callback(job, func, cb_param);
}

/**
 * Get the address of a render attachment from the write back unit of a pp job
 * @param job Handle to the job
 * @param wb_unit the Write Back unit
 * @return the memory address of the render attachment
 */
MALI_STATIC_FORCE_INLINE u32 _mali_pp_job_get_render_attachment_address(mali_pp_job_handle job, u32 wb_unit)
{
	return _mali_base_common_pp_job_get_render_attachment_address(job, wb_unit);
}

/**
 * Set a render register value in a job definition
 * @param job Handle to the job to update
 * @param regid ID of register to set
 * @param value value to assign to register
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_set_common_render_reg(mali_pp_job_handle job, mali_reg_id regid, mali_reg_value value)
{
	_mali_base_common_pp_job_set_common_render_reg(job, regid, value);
}

MALI_STATIC_FORCE_INLINE void _mali_pp_job_set_specific_render_reg(mali_pp_job_handle job, int core_no, mali_reg_id regid, mali_reg_value value)
{
	_mali_base_common_pp_job_set_specific_render_reg(job, core_no, regid, value);
}

/**
 * Set a barrier on the job object
 * All jobs from this process which is submitted before this job will be completed before this starts
 * @param job Handle to the job
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_set_barrier(mali_pp_job_handle job)
{
	_mali_base_common_pp_job_set_barrier(job);
}

/**
 * Return true if it is possible to update a register of the pp job
 * @param job Handle to the job to check
 * @return true if it is possible to update a job register, false otherwise
 */
MALI_STATIC_FORCE_INLINE mali_bool _mali_pp_job_is_updateable(mali_pp_job_handle job)
{
	return _mali_base_common_pp_job_is_updateable(job);
}

/**
 * Get a wait handle which is trigged when the pp job has finished processing
 * Returns a handle to a wait object usable for waiting on this pp job to finish processing
 * @note This must be called before @see _mali_pp_job_start if you want to be able wait on this pp job
 * @param handle Handle to a pp job
 * @return Handle which can be used with @see _mali_wait_on_handle
 */
MALI_STATIC_FORCE_INLINE mali_base_wait_handle _mali_pp_job_get_wait_handle(mali_pp_job_handle job)
{
	return _mali_base_common_pp_job_get_wait_handle(job);
}

/**
 * Queue a PP job for execution by the system.
 *
 * Puts the job onto the queue of jobs to be run.
 * The job's priority will decide where in the queue it will be put.
 *
 * If fence is not NULL, a fence, that will be signalled when the job is
 * complete, will be created and the fence handle will be stored in \a fence.
 *
 * @param job Pointer to the job to put on the execution queue.
 * @param priority Priority of the job
 * @param fence Pointer to mali_fence_handle, output fence if not NULL
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_start(mali_pp_job_handle job, mali_job_priority priority, mali_fence_handle *fence)
{
#if MALI_INSTRUMENTED
	_mali_base_common_instrumented_pp_job_start(job, priority, fence);
#else
	_mali_base_common_pp_job_start(job, priority, fence);
#endif /* #if MALI_INSTRUMENTED */
}

/**
 * Set the Mali fence the job will wait for before starting
 *
 * The job assumes ownership of the fence and will release it after use.
 *
 * @param job Handle of the job
 * @param fence Handle of fence job must wait for
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_set_fence(mali_pp_job_handle job, mali_fence_handle fence)
{
	_mali_base_common_pp_job_set_fence(job, fence);
}

/**
 * Attach a PP job to a sync object
 * Attaches the job to the list of jobs the sync object should wait for before firing the callback
 * @note The two objects must be from the same context
 * @param sync Handle to the sync object to attach this job to
 * @param job Pointer to the job to put on the sync list
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_add_to_sync_handle(mali_sync_handle sync, mali_pp_job_handle job)
{
	_mali_base_common_pp_job_add_to_sync_handle(sync, job);
}

/**
 * Function to return number of MaliPP cores available
 * @return The number of PP cores on the system
 */
MALI_STATIC_FORCE_INLINE u32 _mali_pp_get_num_cores(void)
{
	return _mali_base_arch_pp_get_num_cores();
}

/**
 * Function to return version number of MaliPP. To be used if we have several different versions
 * of MaliPP, and some may have bugfixes.
 * @return The core version
 */
MALI_STATIC_FORCE_INLINE u32 _mali_pp_get_core_product_id(void)
{
	return _mali_base_arch_pp_get_product_id();
}

/**
 * Function to return the Major and Minor fields (version) reported for the cores on the system.
 * @return The version number read from the cores
 */
MALI_STATIC_FORCE_INLINE u32 _mali_pp_get_core_version(void)
{
	return _mali_base_arch_pp_get_core_version();
}

/**
 * Set a frame_nr on the job. It can be used for aborting jobs. The frame_nr
 * can be given to the job_abort function, and it will abort all jobs in the
 * context with this frame_id.
 * @param job_handle Set frame_id on this job
 * @param frame_id   The frame_id we want to set.
 * @see _mali_base_frame_id_get_new()
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_set_frame_id(mali_pp_job_handle job_handle, mali_base_frame_id frame_id)
{
	_mali_base_common_pp_set_frame_id(job_handle, frame_id);
}

/**
 *	Set job identity, which is defined by unique frame builder id and flush id. Called from Frame Builder's
 *  @c _pp_jobs_create.
 *
 *  @param job_handle	Job handle
 *  @param fb_id		Frame Builder ID
 *  @param flush_id		Flush ID
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_set_identity(mali_pp_job_handle job_handle, mali_base_frame_id fb_id, mali_base_flush_id flush_id)
{
	_mali_base_common_pp_job_set_identity(job_handle, fb_id, flush_id);
}

/**
 * Disable Write-back unit(s) on the job with the specified frame builder and flush IDs
 *  @param fb_id		Frame Builder ID
 *  @param flush_id		Flush ID
 *  @param wbx          Which write-back units to disable
 */
MALI_STATIC_FORCE_INLINE void _mali_pp_job_disable_wb(u32 fb_id, u32 flush_id, mali_pp_job_wbx_flag wbx)
{
	_mali_base_common_pp_job_disable_wb(fb_id, flush_id, wbx);
}

#ifdef __cplusplus
}
#endif

#endif /* _MALI_PP_JOB_H_ */
