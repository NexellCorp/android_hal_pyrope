/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007, 2009-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_dump_gp_functions_backend.h
 * Functions exposed to the dumping parts of Base driver.
 * Included by mali_common_dump_jobs.c.
 *
 * This is the direction the c-files is connected:
 *  _dump_gp_functions_frontend.h  _dump_jobs_functions.h  _dump_gp_functions_backend.h
 *                |                        |                          |
 * mali_gp_job.c -> base_common_dump_gp.c -> base_common_dump_jobs.c -> base_common_dump_gp.c
 *
 */

#ifndef _BASE_COMMON_DUMP_GP_FUNCTIONS_BACKEND_H_
#define _BASE_COMMON_DUMP_GP_FUNCTIONS_BACKEND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <base/common/gp/base_common_gp_job.h>

/**
 * Writes all registers in the job to the dump file, and load command to load the corresponding memory dump.
 * This makes it possible to analyze or simulate what the mali core actually did in the job.
 */
void _mali_common_dump_gp_regs(mali_base_ctx_handle ctx, mali_file *file_regs, u32 * dump_reg_info);

/** This is used when we try to dump the gp_job after it crashed.
 * To be able to do that we must patch the pointer array so it becomes as it was before job start.
 * In this process the crashed pointer array is restored before in the unpatching which is done
 * after the dumping has been performed.
 * The algorithm used will not work if several GP jobs are used after eachother on the same frame
 * without resetting the pointer_array before starting the job.
 * This is because we for speed reasons only take a backup of the first part of the array before
 * the job starts, and when crash occure tries to recreate the whole buffer based on these values.
 */
void _mali_common_dump_gp_patch_pointer_array(mali_dump_job_info * dump_info);

/** Function restores the memory pathced by function
 * _mali_common_dump_gp_patch_pointer_array() */
void _mali_common_dump_gp_unpatch_pointer_array(mali_dump_job_info * dump_info);


#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DUMP_GP_FUNCTIONS_BACKEND_H_ */
