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
 * @file mali_gp_job.h
 * The GP job system.
 * All access to the mali hardware goes through a job system. Jobs are created
 * by the user and queued for execution. The Vertex Shader (VS) and the Polygon
 * List Builder Unit (PLBU), use a common job definition. A job can contain
 * commmand lists for both the PLBU and the VS. If a VS and a PLBU job depend
 * on each other they must be added to a single GP job handle so they are
 * handled together by the lower systems.
 */

#ifndef _MALI_GP_HANDLE_H_
#define _MALI_GP_HANDLE_H_

#include <base/mali_types.h>
#include <base/mali_macros.h>
#include <base/mali_context.h>
#include <base/mali_memory.h>
#include <base/mali_sync_handle.h>

/**
 * Definition of GP job state type.
 *
 * Used to indicate in which state a GP job is in.
 *
 * We have these states:
 *      Pool: A job descriptor which is not currently in use
 *      Building: A job which the user can modify and start
 *  Separate states for each subqueue a job can be a part of:
 *       Queued: Queued for running on the hw. (VS or PLBU)
 *       Running: Currently running on the hw. (VS or PLBU)
 *      Postprocessing Pending: Waiting for for post processing after hw run (on both cores if requested)
 *      Callback: Running a user registered callback as part of post processing
 *      Memory cleanup: Cleaning memory registered for auto cleanup during postprocessing.
 *
 * A job retrieved through the new_job interface originates in the Building state.
 * A job can only be modified through our public API if it's in the Building state.
 * When the user calls start on the job it switches to Queued or Running state (for each core requested),
 * blocking any changes to the job afterwards.
 * The other states are used to track a job's status through post processing, to
 * ease debugging / error tracking.
 *
 * Acceptable transitions between states are:
 *      Pool: to Building
 *      Building: to Running, to Queued
 *      Queued: to Running (per core)
 *      Running: to Queued, to Postprocessing Pending (per core)
 *      Postprocessing Pending: to Callback
 *      In Callback: to Memory Cleanup
 *      Memory Cleanup: To Pool
 *
 * These rules are enforced in the public accessible functions.
 * Therefore, a user can only call i.e. free on a job which
 * is in the building state.
 * If the job has been started, a free will be denied/reported.
 * Starting of a job which has already been started
 * will also be denied/reported.
 */

/**
 * Enum defintion for job state tracking
 * Used to track the state a job is in to prohibit
 * manipulation of the job after the job has been started
 */
typedef enum mali_gp_job_state
{
        MALI_GP_JOB_STATE_POOL                 = 0x00000001,
        MALI_GP_JOB_STATE_BUILDING             = 0x00000002,
        MALI_GP_JOB_STATE_RUNNING              = 0x00000004,
        MALI_GP_JOB_STATE_POSTPROCESS_PENDING  = 0x00000005,
        MALI_GP_JOB_STATE_CALLBACK             = 0x00000006,
        MALI_GP_JOB_STATE_MEMORY_CLEANUP       = 0x00000007,
        MALI_GP_JOB_STATE_SYNCING              = 0x00000008,
        MALI_GP_JOB_STATE_WAKEUP               = 0x00000009
} mali_gp_job_state;

/*
 * Info about a command list needed to inline command writes
 */
typedef struct mali_gp_cmd_list_inline
{
	u64 * mapping;
	u32 mapping_left;

	MALI_DEBUG_CODE
	(
	u64 * reserved_mapping;
	u32   reserved_count;
	)

} *mali_gp_cmd_list_handle;

/**
 * Type definition of a GP job handle
 * This type is used to represent a GP job and is used in
 * all function manipulating a GP job.
 */
typedef struct mali_gp_job_handle_type
{
	/* debug only? */
	mali_gp_job_state       state;             /**< The state this job is in */

	mali_gp_cmd_list_handle vs;
	mali_gp_cmd_list_handle plbu;
} * mali_gp_job_handle;

/**
 * Definition of the gp callback function
 * Called when the job has completed.
 * Registered using _mali_gp_job_set_callback on a job
 * @param ctx The Mali base context handle the job was created in
 * @param cb_param User defined callback parameter
 * @param completion_status Job completion status
 * @param job_handle Handle for job that is completed when this callback is called
 * @return True if the callback successfully completed the job and memory can be reclaimed.
 *         False if the job was not completed and the job should be kept alive
 */
typedef mali_bool (*mali_cb_gp)(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status completion_status, mali_gp_job_handle job_handle);

#endif
