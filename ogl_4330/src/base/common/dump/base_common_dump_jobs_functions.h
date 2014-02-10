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
 * @file base_common_dump_jobs_functions.h
 * Functions exposed to the dumping parts of Base driver.
 * Included by base_common_dump_gp.c  and base_common_dump_pp.c
 *
 * This is the direction the c-files is connected:
 *  _dump_gp_functions_frontend.h  _dump_jobs_functions.h  _dump_gp_functions_backend.h
 *                |                        |                          |
 * mali_gp_job.c -> base_common_dump_gp.c -> base_common_dump_jobs.c -> base_common_dump_gp.c
 * mali_pp_job.c -> base_common_dump_pp.c -> base_common_dump_jobs.c -> base_common_dump_pp.c
 *                |                        |                          |
 *  _dump_pp_functions_frontend.h  _dump_jobs_functions.h  _dump_pp_functions_backend.h
 */

#ifndef _BASE_COMMON_DUMP_JOBS_FUNCTIONS_H_
#define _BASE_COMMON_DUMP_JOBS_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <base/mali_types.h>
#include "base_common_dump_jobs_datatypes_frontend.h"
#include "base_common_dump_gp_datatypes_frontend.h"

/* Needed for type mali_pp_try_start_result. Casting mali_gp_try_start_result to be of same type */
#include <base/arch/base_arch_pp.h>


/**
 * Returning MALI_TRUE if the dump system is enabled.
 * It is enabled if the variables may let any job be dumped.
 */
mali_bool _mali_dump_system_is_enabled(mali_base_ctx_handle ctx);

 /**
  * Returning MALI_TRUE if we should print out frame numbers and other dumping info.
  */
mali_bool _mali_dump_system_verbose_printing_is_enabled(mali_base_ctx_handle ctx);

/**
 * Returning MALI_TRUE if we should dump jobs that crash.
 */
mali_bool _mali_dump_system_dump_crashed_is_enabled(mali_base_ctx_handle ctx);

/**
 * Open the config.txt file used for dumping of registers and commands for loading memory dumps.
 */
mali_file * _mali_base_common_dump_file_open(mali_base_ctx_handle ctx);

/**
 * Open the opened config.txt file.
 */
void _mali_base_common_dump_file_close(mali_file * file);

/**
 * If this is MALI_TRUE, the dump system should not dump registers or memory content.
 * It should then only dump information about all memory handles on the system.
 * This is for profiling of memory usage.
 */
mali_bool _mali_dump_system_mem_print_one_liners(mali_base_ctx_handle ctx);

/**
 * Returns the delimitor to use beween each field in memory dumps if
 * \a _mali_dump_system_mem_print_one_liners() is enabled.
 */
char *_mali_dump_system_mem_print_one_liners_delimiter(mali_base_ctx_handle ctx);

/**
 * Setting dump_info pointer in TLS, so the settings can be acquired when a new job
 * is started in current callback. Dump memory if DUMP_RESULT is set.
 * If DUMP_CRASHED is set, it patch memory and dump the job if it has crashed
 */
void _mali_base_common_dump_pre_callback(mali_base_ctx_handle ctx,
        mali_dump_job_info * dump_info, mali_job_completion_status completion_status,
        void * dump_reg_info);

/** Doing memory dump before starting if enabled */
void _mali_base_common_dump_pre_start_dump(mali_base_ctx_handle ctx, mali_dump_job_info * dump_info, void * registers);

void _mali_base_common_dump_tls_job_info_set(mali_base_ctx_handle ctx, mali_dump_job_info * dump_info);

/** Setting the TLS to not running in callback. Release dump_info->dump_sync */
void _mali_base_common_dump_job_post_callback(mali_base_ctx_handle ctx, mali_sync_handle dump_sync);


typedef enum mali_dump_core_type
{
	MALI_DUMP_PP,
 	MALI_DUMP_GP
}mali_dump_core_type;

/**
 * Filling the job->dump_info struct
 */
void _mali_base_common_dump_job_info_fill(mali_base_ctx_handle ctx, mali_dump_core_type core, mali_dump_job_info * info);


/**
 * Disable/enable dumping of this job (dumping is enabled by default)
 */
mali_bool _mali_base_common_dump_job_enable_dump(mali_dump_job_info * info, mali_bool enable);


#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DUMP_JOBS_FUNCTIONS_H_ */
