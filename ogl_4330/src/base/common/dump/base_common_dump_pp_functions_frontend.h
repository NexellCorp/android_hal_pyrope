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
 * @file base_common_dump_pp_functions_frontend.h
 * Functions exposed to the NON-dumping parts of Base driver.
 * Included by mali_pp_job.c.
 *
 * This is the direction the c-files is connected:
 * mali_pp_job.c -> base_common_dump_pp.c -> base_common_dump_jobs.c -> base_common_dump_pp.c
 *                |                        |                          |
 *  _dump_pp_functions_frontend.h  _dump_jobs_functions.h  _dump_pp_functions_backend.h
 */

#ifndef _BASE_COMMON_DUMP_PP_FUNCTIONS_FRONTEND_H_
#define _BASE_COMMON_DUMP_PP_FUNCTIONS_FRONTEND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <base/common/pp/base_common_pp_job.h>
#include <base/arch/base_arch_pp.h>


 /** Filling the job->dump_info struct */
 void _mali_common_dump_pp_pre_start(mali_pp_job *job);

/**
 * Setting dump_info pointer in TLS, so the settings can be acquired
 * when a new job is started in current callback. Dump memory if
 * DUMP_RESULT is set. If DUMP_CRASHED is set, it dump the job
 * with register writes to start it if the job crashes.*/
mali_sync_handle _mali_common_dump_pp_pre_callback(mali_pp_job * job, mali_job_completion_status completion_status);

/**
 * Setting the TLS to not running in callback.
 * Release dump_info->dump_sync so that _mali_common_dump_pp_try_start() can return
 * if waiting on job complete is enabled.
 */
void _mali_common_dump_pp_post_callback(mali_base_ctx_handle ctx, mali_sync_handle dump_sync);

/**
 * Disable dumping of specified job
 * Returns old setting
 */
mali_bool _mali_common_dump_pp_enable_dump(mali_pp_job_handle job_handle, mali_bool enable);

void _mali_base_common_dump_pre_start_dump(mali_base_ctx_handle ctx, mali_dump_job_info * dump_info, void * dump_reg_info);

#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DUMP_PP_FUNCTIONS_FRONTEND_H_ */
