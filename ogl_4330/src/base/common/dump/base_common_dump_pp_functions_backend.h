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
 * @file base_common_dump_pp_functions_backend.h
 * Functions exposed to the dumping parts of Base driver.
 * Included by mali_common_dump_jobs.c.
 *
 * This is the direction the c-files is connected:
 * mali_pp_job.c -> base_common_dump_pp.c -> base_common_dump_jobs.c -> base_common_dump_pp.c
 *                |                        |                          |
 *  _dump_pp_functions_frontend.h  _dump_jobs_functions.h  _dump_pp_functions_backend.h
 */

#ifndef _BASE_COMMON_DUMP_PP_FUNCTIONS_BACKEND_H_
#define _BASE_COMMON_DUMP_PP_FUNCTIONS_BACKEND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <base/common/pp/base_common_pp_job.h>

/**
 * Writes all registers in the job to the dump file, and load command to load the corresponding memory dump.
 * This makes it possible to analyze or simulate what the mali core actually did in the job.
 */
void _mali_common_dump_pp_regs(mali_base_ctx_handle ctx, mali_file *file_regs, mali_pp_job * job, u32 subjob_nr);


#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DUMP_PP_FUNCTIONS_BACKEND_H_ */
