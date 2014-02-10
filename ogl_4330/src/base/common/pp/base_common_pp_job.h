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
 * @file base_common_pp_job.h
 * The common layer contains the implementation of the pp-job-functions.
 * Function-calls are passed through the frontend to common layer.
 *
 */

#ifndef _BASE_COMMON_PP_JOB_H_
#define _BASE_COMMON_PP_JOB_H_

#include <base/mali_debug.h>
#include <base/pp/mali_pp_handle.h>
#include <base/common/tools/base_common_tools_circular_linked_list.h>
#include "sync/mali_external_sync.h"

#include "regs/MALI200/mali200_core.h"

#if MALI_INSTRUMENTED
#include "base/common/instrumented/base_common_pp_instrumented.h"
#endif

#ifdef MALI_DUMP_ENABLE
#include <base/common/dump/base_common_dump_pp_datatypes_frontend.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Definition of pp job state type.
 *
 * Used to indicate in which state a pp job is in.
 *
 * We have these states:
 * 	Pool: A job descriptor which is not currently in use
 * 	Building: A job which the user can modify and start
 * 	Running: Currently running on the hw
 * 	Postprocessing Pending: Waiting for for post processing after hw run
 * 	Callback: Running a user registered callback as part of post processing
 * 	Memory cleanup: Cleaning memory registered for auto cleanup during postprocessing.
 *
 * A job retrieved through the new_job interface originates in the Building state.
 * A job can only be modified through our public API if it's in the Building state.
 * When the user calls start on the job it switches to Queued or Running state,
 * blocking any changes to the job afterwards.
 * The other states are used to track a job's status through post processing, to
 * ease debugging / error tracking.
 *
 * Acceptable transitions between states are:
 * 	Pool: to Building
 * 	Building: to Running
 * 	Running: to Postprocessing Pending
 * 	Postprocessing Pending: to Callback
 * 	In Callback: to Memory Cleanup
 * 	Memory Cleanup: To Pool
 *
 * These rules are enforced in the public accessible functions.
 * Therefore, a user can only call i.e. free on a job which
 * is in the building state.
 * If the job has been started, a free will be denied/reported.
 * Starting of a job which has already been started
 * will also be denied/reported.
 */

typedef enum mali_pp_job_state
{
	MALI_PP_JOB_POOL                    = 0x01, /* unused, will be used by the pool system */
	MALI_PP_JOB_BUILDING                = 0x02,
	MALI_PP_JOB_RUNNING                 = 0x04,
	MALI_PP_JOB_POSTPROCESS_PENDING     = 0x05,
	MALI_PP_JOB_CALLBACK                = 0x06,
	MALI_PP_JOB_MEMORY_CLEANUP          = 0x07,
	MALI_PP_JOB_SYNCING                 = 0x08,
	MALI_PP_JOB_WAKEUP                  = 0x09
} mali_pp_job_state;

typedef struct mali_pp_registers
{
#if defined(USING_MALI450) || defined(USING_MALI400)
	mali_reg_value	frame_regs[M400_FRAME_REG_TILEBUFFER_BITS - M200_FRAME_REG_REND_LIST_ADDR + 1];
#elif defined(USING_MALI200)
	mali_reg_value	frame_regs[M200_FRAME_REG_TIEBREAK_MODE - M200_FRAME_REG_REND_LIST_ADDR + 1];
#else
#error "no supported mali core defined"
#endif
	mali_reg_value  frame_regs_addr_frame[_MALI_PP_MAX_SUB_JOBS - 1];
	mali_reg_value  frame_regs_addr_stack[_MALI_PP_MAX_SUB_JOBS - 1];
	mali_reg_value	wb0_regs[M200_WB0_REG_GLOBAL_TEST_CMP_FUNC - M200_WB0_REG_SOURCE_SELECT + 1];
	mali_reg_value	wb1_regs[M200_WB1_REG_GLOBAL_TEST_CMP_FUNC - M200_WB1_REG_SOURCE_SELECT + 1];
	mali_reg_value	wb2_regs[M200_WB2_REG_GLOBAL_TEST_CMP_FUNC - M200_WB2_REG_SOURCE_SELECT + 1];
} mali_pp_registers;

typedef struct mali_pp_job
{
	mali_job_priority               priority;
	mali_pp_job_state               state;

	mali_base_ctx_handle            ctx;
	mali_stream_handle              stream;
	mali_fence_handle               pre_fence;
	mali_cb_pp                      callback;
	void                           *callback_argument;
	mali_base_wait_handle           wait_handle;
	mali_mem_handle                 freelist;
	mali_sync_handle                sync;
	mali_base_frame_id              frame_id;

	u32                             num_cores;
	mali_pp_registers               registers;

	mali_bool                       barrier;

#if MALI_INSTRUMENTED
	mali_base_instrumented_pp_context perf_ctx;
#endif /* MALI_INSTRUMENTED */

	mali_base_frame_builder_id		frame_builder_id;
	mali_base_flush_id				flush_id;

#ifdef MALI_DUMP_ENABLE
	mali_dump_job_info dump_info;
#endif /* MALI_DUMP_ENABLE */

#if defined(USING_MALI450)
	m450_pp_job_frame_info info;
#endif /* defined(USING_MALI450) */

} mali_pp_job;

/** Initialize the PP job system. Returns a base context handle.
 * Each _mali_base_common_pp_open call must be matched with a call to _mali_base_common_pp_close.
 * It's safe to call this function multiple times.
 * @see _mali_base_common_pp_close()
 * @param ctx The base context to scope usage to
 * @return A standard Mali error code
 */
mali_err_code _mali_base_common_pp_open(mali_base_ctx_handle ctx) MALI_CHECK_RESULT;

/** Close a reference to the PP job system. Match each call to _mali_base_common_pp_open with a call to this function.
 * @param ctx The mali base context handle used in the open call
 * @see _mali_base_common_pp_open()
 */
void _mali_base_common_pp_close(mali_base_ctx_handle ctx);

/**
 * Get a new PP job struct.
 * Returns a pointer to a new mali_pp_job.
 * When you are finished with the job it needs to be released.
 * This can either be done with setting the auto_free member variable or
 * calling the _mali_base_common_pp_job_free function.
 * @param ctx The mali base context handle obtained in a open call earlier
 * @param handle Handle to a template for this job. Use MALI_NO_HANDLE if no template is to be used
 * @param num_cores number of subjobs
 * @param stream Handle of the stream to which the job belongs
 * @return Handle to a new job, or MALI_NO_HANDLE if unable create a new handle.
 */
MALI_TEST_IMPORT
mali_pp_job_handle _mali_base_common_pp_job_new(mali_base_ctx_handle ctx, u32 num_cores, mali_stream_handle stream);

/**
 * Release a PP job struct.
 * Returns the PP job struct to the base system and frees up any resources used by it.
 * @param job The job to release
 */
MALI_TEST_IMPORT
void _mali_base_common_pp_job_free(mali_pp_job_handle job_handle);

/**
 * Reset a PP job
 * Resets the PP job to same the state as when newly allocated.
 * Any attached resources will be freed.
 * @param job_handle Handle for a PP job
 */
void _mali_base_common_pp_job_reset(mali_pp_job_handle job_handle);


/**
 * Add a mali_mem_handle to a job's free-on-termination list
 * @param job Handle to the job to update
 * @param mem Handle to add to list
 */
void _mali_base_common_pp_job_add_mem_to_free_list(mali_pp_job_handle job, mali_mem_handle mem);

/**
 * Set callback for a pp job
 * @param job Handle to the job to update
 * @param func Function to set as callback func
 * @param cp_param Argument passed to callback
 */
void _mali_base_common_pp_job_set_callback(mali_pp_job_handle job, mali_cb_pp func, void * cb_param);

/**
 * Get the address of a render attachment from the write back unit of a pp job
 * @param job Handle to the job
 * @param wb_unit the Write Back unit
 * @return the memory address of the render attachment
 */
u32 _mali_base_common_pp_job_get_render_attachment_address(mali_pp_job_handle job, u32 wb_unit);

/**
 * Return true if it is possible to update a job register
 * @param job_handle Handle to the job to check
 * @return true if it possible to update a job register, false otherwise
 */
mali_bool _mali_base_common_pp_job_is_updateable(mali_pp_job_handle job_handle);

/**
 * Set a render register value in a job definition
 * @param job Handle to the job to update
 * @param regid ID of register to set
 * @param value value to assign to register
 */
void _mali_base_common_pp_job_set_common_render_reg(mali_pp_job_handle job, mali_reg_id regid, mali_reg_value value);
void _mali_base_common_pp_job_set_specific_render_reg(mali_pp_job_handle job, int core_no, mali_reg_id regid, mali_reg_value value);

/**
 * Set a barrier on the job object
 * All jobs from this process which is submitted before this job will be completed before this starts
 * @param job Handle to the job
 */
void _mali_base_common_pp_job_set_barrier(mali_pp_job_handle job);

/**
 * Get a wait handle which is trigged when the pp job has finished processing
 * Returns a handle to a wait object usable for waiting on this pp job to finish processing
 * @note This must be called before @see _mali_pp_job_start if you want to be able wait on this pp job
 * @param handle Handle to a pp job
 * @return Handle which can be used with @see _mali_wait_on_handle
 */
mali_base_wait_handle _mali_base_common_pp_job_get_wait_handle(mali_pp_job_handle job);

/**
 * Queue a PP job for execution by the system.
 * Puts the job onto the queue of jobs to be run.
 * The job's priority will decide where in the queue it will be put.
 * The job assumes ownership of the fence and will release it after use.
 * @param job Pointer to the job to put on the execution queue.
 * @param priority Priority of the job
 * @param fence Pointer to where the job's new fence handle will be stored, if not NULL
 */
void _mali_base_common_pp_job_start(mali_pp_job_handle job_handle, mali_job_priority priority, mali_fence_handle *fence);

/**
 * Set Mali fence handle on PP job
 *
 * @param job Pointer to the job to put on the execution queue.
 * @param fence Handle of the fence the job must wait for before starting
 * @return MALI_ERR_NO_ERROR if merge is successful
 */
mali_err_code _mali_base_common_pp_job_set_fence(mali_pp_job_handle job, mali_fence_handle fence);

/**
 * Attach a PP job to a sync object
 * Attaches the job to the list of jobs the sync object should wait for before firing the callback
 * @param sync Handle to the sync object to attach this job to
 * @param job Pointer to the job to put on the sync list
 */
void _mali_base_common_pp_job_add_to_sync_handle(mali_sync_handle sync, mali_pp_job_handle job);

/* interface used by arch */

/**
 * Do the post-processing for a PP job.
 * Goes through the post-processing steps for a job, which can include:
 * 	Callback
 *  Sync handle notification
 *  Memory cleanup
 * The post-processing is performed right away, it's NOT queued.
 * If postprocessing should be queued/done on another thread,
 * call this function when the actual post-processing is to be performed.
 * After post-processing is complete the job is destroyed.
 *
 * @note The job has been deleted in this call, therefore the job can't be accessed anymore.
 *
 * @param job Pointer to the job which is to go through post-processing.
 * @param completion_status Job completion status
 */
void _mali_base_common_pp_job_run_postprocessing(mali_pp_job * job, mali_job_completion_status completion_status);

/** Set the frame_id on the job.
 * The frame_id should be allocated through the function \a _mali_base_frame_id_get_new(ctx)
 * The frame_id is used when to abort jobs.
 */
void _mali_base_common_pp_set_frame_id(mali_pp_job_handle job_handle, mali_base_frame_id frame_id);

/**
 *	Set PP job identity. The identity is defined by the frame builder id and by the flush id.
 *  The frame builder ID is unique within session, while the flush id unique within frame builder.
 *
 *	@param job_handle	PP job handle
 *  @param fb_id		Frame Builder ID
 *  @param flush_id		Flush ID within frame builder context
 */
void _mali_base_common_pp_job_set_identity(mali_pp_job_handle job_handle, mali_base_frame_id fb_id, mali_base_flush_id flush_id);

/**
 * Disable Write-back unit(s) on the job with the specified frame builder and flush IDs
 *  @param fb_id		Frame Builder ID
 *  @param flush_id		Flush ID
 *  @param wbx          Which write-back units to disable
 */
void _mali_base_common_pp_job_disable_wb(u32 fb_id, u32 flush_id, mali_pp_job_wbx_flag wbx);


#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_PP_JOB_H_ */
