/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_dump_gp_functions_frontend.h
 * Functions exposed to the NON-dumping parts of Base driver.
 * Included by mali_gp_job.c.
 *
 * This is the direction the c-files is connected:
 *  _dump_gp_functions_frontend.h  _dump_jobs_functions.h  _dump_gp_functions_backend.h
 *                |                        |                          |
 * mali_gp_job.c -> base_common_dump_gp.c -> base_common_dump_jobs.c -> base_common_dump_gp.c
 *
 */

#ifndef _BASE_COMMON_DUMP_GP_FUNCTIONS_FRONTEND_H_
#define _BASE_COMMON_DUMP_GP_FUNCTIONS_FRONTEND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <base/common/gp/base_common_gp_job.h>
#include <base/arch/base_arch_gp.h>


/** Store the memory_handle that contains the pointer_array used by the PLB.
 * This is necessary to know to be able to patch the memory before dumping when DUMP_CRASHED is enabled.*/
void _mali_common_dump_gp_set_plbu_pointer_array(mali_gp_job_handle job_handle, mali_mem_handle pointer_array_handle);

 /** Filling the job->dump_info struct */
 void _mali_common_dump_gp_pre_start(mali_gp_job *job);
/**
 * Dumping the register writes that are done during PLBU heap growth.
 */
void _mali_common_dump_gp_heap_grow(mali_gp_job *job, u32 heap_start, u32 heap_end);

/**
 * Setting dump_info pointer in TLS, so the settings can be acquired
 * when a new job is started in current callback. Dump memory if
 * DUMP_RESULT is set. If DUMP_CRASHED is set, it patch memory and
 * dump the job if it has crashed */
mali_sync_handle _mali_common_dump_gp_pre_callback(mali_gp_job * job, mali_job_completion_status completion_status);

/**
 * Setting the TLS to not running in callback.
 * Release dump_info->dump_sync so that _mali_common_dump_gp_try_start() can return
 * if waiting on job complete is enabled.
 */
void _mali_common_dump_gp_post_callback(mali_base_ctx_handle ctx, mali_sync_handle dump_sync);

/**
 * Disable dumping of specified job
 * Returns old setting
 */
mali_bool _mali_common_dump_gp_enable_dump(mali_gp_job_handle job_handle, mali_bool enable);

#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DUMP_GP_FUNCTIONS_FRONTEND_H_ */
