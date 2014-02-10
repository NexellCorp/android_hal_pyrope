/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_gp_job.h
 * The common layer contains the implementation of the gp-job-functions.
 * Function-calls are passed through the frontend to common layer.
 *
 */

#ifndef _BASE_COMMON_GP_JOB_H_
#define _BASE_COMMON_GP_JOB_H_

#include <mali_system.h>
#include <base/mali_types.h>

#include <base/common/tools/base_common_tools_circular_linked_list.h>
#include <base/common/gp/base_common_gp_cmdlist.h>
#include <base/mali_sync_handle.h>

#if MALI_INSTRUMENTED
#include <base/common/instrumented/base_common_gp_instrumented.h>
#endif /* MALI_INSTRUMENTED */


#ifdef MALI_DUMP_ENABLE
#include <base/common/dump/base_common_dump_gp_datatypes_frontend.h>
#endif

#define GP_VS_CMDLIST_BLOCK_SIZE 1024
#define GP_PLBU_CMDLIST_BLOCK_SIZE 1024

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The sub core id on GP
 */
typedef enum mali_gp_core_id { MALI_GP_CORE_VS = 0x01, MALI_GP_CORE_PLBU = 0x02 } mali_gp_core_id;

/**
 * The GP job struct.
 */
typedef struct mali_gp_job
{
	struct mali_gp_job_handle_type inlined;

	mali_base_ctx_handle    ctx;               /**< The base context this job was created in */
	mali_bool               autoFree;          /**< Flag indicating if the job object should be freed during postprocessing */
	mali_job_priority       priority;          /**< The jobs priority */

	mali_cb_gp              callback;          /**< Callback set to run on completion */
	void *                  callback_argument; /**< Argument to give to callback */

	mali_base_wait_handle   wait_handle;       /**< Wait handle attached to this job, when completed this object is notified */
	mali_mem_handle         freelist;          /**< Memory to free after running the job */
	mali_sync_handle        sync;              /**< Sync handle to notify of job completion */

	mali_base_frame_id      frame_id;          /**< Frame id which represents the frame multiple jobs might belong to */
	mali_mem_handle         plbu_heap;         /**< The PLBU heap which is in use for this job */

	mali_gp_cmd_list       *vs_cmd_list;       /**< The VS command list */
	mali_gp_cmd_list       *plbu_cmd_list;     /**< The PLBU command list */

	mali_addr               registers[6];      /**< Extracted info from the different sub-objects, cached here as we have multiple users of it and want to avoid duplicate how it's fetched */

#if MALI_INSTRUMENTED
	mali_base_instrumented_gp_context perf_ctx;
#endif

	mali_base_frame_builder_id		frame_builder_id;
	mali_base_flush_id				flush_id;

#ifdef MALI_DUMP_ENABLE
	mali_dump_job_info_gp dump_info;
#endif

} mali_gp_job;

/**
 * Convert a gp job handle to a mali_gp_job struct pointer.
 * @param h Handle to convert
 * @return A mali_gp_job pointer
 */
MALI_STATIC_INLINE mali_gp_job * mali_gp_handle_to_job(mali_gp_job_handle h)
{
    return (mali_gp_job*)(((u8*)h) - offsetof(mali_gp_job, inlined));
}

/**
 * Initialize the GP job system.
 * Each _mali_base_common_gp_open call must be matched with a call to _mali_base_common_gp_close.
 * It's safe to call this function multiple times.
 * @see _mali_base_common_gp_close()
 * @param ctx The base conext to scope usage to
 * @return A standard Mali error code
 */
mali_err_code _mali_base_common_gp_open(mali_base_ctx_handle ctx) MALI_CHECK_RESULT;

/**
 * Close a reference to the GP job system. Match each call to _mali_base_common_gp_open with a call to this function.
 * @param ctx The mali base context handle used in the open call
 * @see _mali_base_common_gp_open()
 */
void _mali_base_common_gp_close(mali_base_ctx_handle ctx);

/**
 * Get a new GP job struct.
 * Returns a pointer to a new mali_gp_job.
 * When you are finished with the job it needs to be released.
 * This can either be done with setting the auto_free member variable or
 * calling the _mali_base_common_gp_job_free function.
 * @param ctx The mali base context handle obtained in a open call earlier
 */
MALI_TEST_IMPORT
mali_gp_job_handle _mali_base_common_gp_job_new(mali_base_ctx_handle ctx);

/**
 * Release a GP job struct.
 * Returns the GP job struct to the base system and frees up any resources used by it.
 * @param job Pointer to a gp job struct to release
 */
MALI_TEST_IMPORT
void _mali_base_common_gp_job_free(mali_gp_job_handle job_handle);

/**
 * Reset a GP job
 * Resets the GP job to same the state as when newly allocated.
 * Any attached resources will be freed.
 * @note The plbu heap will NOT be freed
 * @param job_handle Handle for a GP job
 */
void _mali_base_common_gp_job_reset(mali_gp_job_handle job_handle);

/**
 * Add a mali_mem_handle to a job's free-on-termination list.
 * After the job has been finished and the callback has been issued, the
 * memory added to this list will be freed.
 * @param job Handle to the job to update
 * @param mem Handle to add to list
 */
void _mali_base_common_gp_job_add_mem_to_free_list(mali_gp_job_handle job, mali_mem_handle mem);

/**
 * Set callback for a gp job
 * @param job Handle to the job to update
 * @param func Function to set as callback func
 * @param cp_param Argument passed to callback
 * @return void
 */
void _mali_base_common_gp_job_set_callback(mali_gp_job_handle job, mali_cb_gp func, void * cb_param);

/**
 * Set automatic freeing setting of a GP job
 * By default a job is freed after having been run.
 * This auto free logic can be disabled if the job is to be reused.
 * @note All the other postprocessing of the job is also skipped,
 * except sync handle and wait handle usage.
 * Memory will NOT be freed, command lists will be kept, etc.
 * @note The job has to be freed using _mali_gp_job_free if not auto freed
 * @param job The job set automatic free setting for
 * @param autoFree MALI_TRUE to enable auto freeing, MALI_FALSE to disable auto freeing
 */
void _mali_base_common_gp_job_set_auto_free_setting(mali_gp_job_handle job, mali_bool autoFree);

/**
 * Get automatic freeing setting of a GP job
 * Returns the current job completion auto freeing setting.
 * @warning Potential race condition if used from multiple threads
 * @param job The job to return the auto free setting of
 * @return MALI_TRUE if auto free is enabled, MALI_FALSE if disabled
 */
mali_bool _mali_base_common_gp_job_get_auto_free_setting(mali_gp_job_handle job);

/**
 * Get a wait handle which is trigged when the gp job has finished processing
 * Returns a handle to a wait object usable for waiting on this gp job to finish processing
 * @note This must be called before @see _mali_gp_job_start if you want to be able wait on this gp job
 * @param handle Handle to a gp job
 * @return Handle which can be used with @see _mali_wait_on_handle
 */
mali_base_wait_handle _mali_base_common_gp_job_get_wait_handle(mali_gp_job_handle job);

/**
 * Queue a GP job for execution by the system.
 * Puts the job onto the queue of jobs to be run.
 * The job's priority will decide where in the queue it will be put.
 * @param job Pointer to the gp job to put on the execution queue.
 * @param priority Priority of the job
 * @return A standard Mali error code
 */
mali_err_code _mali_base_common_gp_job_start(mali_gp_job_handle job_handle, mali_job_priority priority) MALI_CHECK_RESULT;

/**
 * Attach a GP job to a sync object
 * Attaches the job to the list of jobs the sync object should wait for before firing the callback
 * @param sync Handle to the sync object to attach this job to
 * @param job Pointer to the job to put on the sync list
 */
void _mali_base_common_gp_job_add_to_sync_handle(mali_sync_handle sync, mali_gp_job_handle job);

/**
 * Free the VS and PLBU command lists of this job without resetting it.
 * @param job_handle A handle to the job to free the VS and PLBU command lists of
 */
void _mali_base_common_gp_job_free_cmdlists(mali_gp_job_handle job_handle);

/**
 * Do the post-processing for a collection of GP jobs.
 * Goes through the post-processing steps for each job, which can include:
 * 	Callback
 *  Trigger wait handle
 *  Sync handle notification
 *  Memory cleanup (if enabled)
 * The post-processing is performed right away, it's NOT queued.
 * If postprocessing should be queued/done on another thread,
 * call this function when the actual post-processing is to be performed.
 * After post-processing is complete the job is destroyed (if not autoFree is disabled).
 *
 * @param job Pointer to the job which is to go through post-processing.
 * @param completion_status Job completion status
 */
void _mali_base_common_gp_job_run_postprocessing(mali_gp_job * job, mali_job_completion_status completion_status);

/**
 * Do the post-processing for a GP job.
 * Goes through the post-processing steps for a job, which can include:
 * 	Callback
 *  Trigger wait handle
 *  Sync handle notification
 *  Memory cleanup (if enabled)
 * The post-processing is performed right away, it's NOT queued.
 * If postprocessing should be queued/done on another thread,
 * call this function when the actual post-processing is to be performed.
 * After post-processing is complete the job is destroyed (if not autoFree is disabled).
 *
 * @param job Pointer to the job which is to go through post-processing.
 * @param completion_status Job completion status
 */
void _mali_base_common_gp_job_run_postprocessing_job(mali_gp_job * job, mali_job_completion_status completion_status);

/**
 * Reason for suspend event comming from the arch backend
 */
typedef enum mali_base_common_gp_job_suspend_reason
{
	MALI_BASE_COMMON_GP_JOB_SUSPEND_REASON_OOM /**< PLBU heap OOM */
} mali_base_common_gp_job_suspend_reason;

/**
 * Notification of that a GP job has been suspended.
 * When a job has to be suspended because it has run out of heap memory
 * user space can decide if we should extend the heap or abort it.
 * In response to this function being called one of these two functions has to be called:
 * a) __mali_base_arch_gp_resume_job with new heap addresses
 * b) _mali_base_arch_gp_mem_request_response_abort to terminate the job
 * @param job Pointer to the job which has been suspended
 * @param reason The reason the job was suspended
 * @param response_cookie The cookie to use in the response call
 */
void _mali_base_common_gp_job_suspended_event(mali_gp_job *job, mali_base_common_gp_job_suspend_reason reason, u32 response_cookie);

/**
 * Setting the tile heap for the job.
 * If the job uses a heap which can be extended this function has to be called
 * @param job_handle The GP job to attach the heap to
 * @param heap The heap to attach
 * @return Normal Mali error code. MALI_ERR_NO_ERROR if binding successful, MALI_ERR_FUNCTION_FAILED if not a heap object
 */
void _mali_base_common_gp_job_set_tile_heap(mali_gp_job_handle job_handle, mali_mem_handle heap);

/**
 * Set the frame_id on the job.
 * The frame_id should be allocated through the function \a _mali_base_frame_id_get_new(ctx)
 * The frame_id is used when aborting jobs. It is also used to find out which jobs that can be merged in the gp job queue system.
 */
void _mali_base_common_gp_job_set_frame_id(mali_gp_job_handle job, mali_base_frame_id frame_id);

/**
 *	Set GP job identity. The identity is defined by the frame builder id and by the flush id.
 *  The frame builder ID is unique within session, while the flush id unique within frame builder.
 *
 *	@param job_handle	GP job handle
 *  @param fb_id		Frame Builder ID
 *  @param flush_id		Flush ID within frame builder context
 */
void _mali_base_common_gp_job_set_identity(mali_gp_job_handle job_handle, mali_base_frame_id fb_id, mali_base_flush_id flush_id);

/* Profiling function found in a separate .h file. */



#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_GP_JOB_H_ */
