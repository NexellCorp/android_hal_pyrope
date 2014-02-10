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

#ifndef _MALI_PP_HANDLE_H_
#define _MALI_PP_HANDLE_H_

#include <base/mali_types.h>
#include <base/mali_context.h>
#include <base/mali_memory.h>
#include <base/mali_sync_handle.h>
#include <mali_render_regs.h>

/**
 * Definition of the handle type used to represent a pp job
 */
typedef struct mali_pp_job_type * mali_pp_job_handle;

/**
 * Flags to indicate write-back units
 */
typedef enum mali_pp_job_wbx_flag
{
	MALI_PP_JOB_WB0 = 1,
	MALI_PP_JOB_WB1 = 2,
	MALI_PP_JOB_WB2 = 4,
} mali_pp_job_wbx_flag;

/**
 * Definition of the pp callback function
 * Called when a job has completed
 * @param ctx A mali base context handle
 * @param cb_param User defined callback parameter
 * @param completion_status Job completion status
 * @param job_handle Handle for job that is completed when this callback is called
 * @return void
 */
typedef void (*mali_cb_pp)(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status completion_status, mali_pp_job_handle job_handle);

#if defined(USING_MALI450)

typedef enum m400_tile_block_size
{
	M400_TILE_BLOCK_SIZE_128  = 0,
	M400_TILE_BLOCK_SIZE_256  = 1,
	M400_TILE_BLOCK_SIZE_512  = 2,
	M400_TILE_BLOCK_SIZE_1024 = 3,
} m400_tile_block_size;

typedef struct m450_pp_job_frame_info
{
	u32 slave_tile_list_mali_address;
	u32 master_x_tiles;
	u32 master_y_tiles;
	enum m400_tile_block_size size;
	u32 binning_pow2_x;
	u32 binning_pow2_y;
	u32 stack_size_per_core;
} m450_pp_job_frame_info;

void _mali_pp_job_450_set_frame_info(mali_pp_job_handle job_handle, m450_pp_job_frame_info * info);

#endif /* defined(USING_MALI450) */

#endif
