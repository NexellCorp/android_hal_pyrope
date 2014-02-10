/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_dump_jobs_datatypes_frontend.h
 * Datatypes that is exposed to the non-dumping parts of Base driver.
 * Included by \a mali_gp_job.h for the \a mali_gp_job struct
 * through \a base_common_dump_gp_datatypes_frontend.h
 * Included by \a mali_pp_job.h for the \a mali_pp_job struct
 * through \a base_common_dump_pp_datatypes_frontend.h
 */

#ifndef _BASE_COMMON_DUMP_JOBS_DATATYPES_FRONTEND_H_
#define _BASE_COMMON_DUMP_JOBS_DATATYPES_FRONTEND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <base/mali_types.h>
#include <base/common/base_common_sync_handle.h>

typedef struct mali_dump_job_info
{
	mali_bool             is_pp_job;
	u32                   frame_nr;
	u32                   job_nr;
	mali_base_wait_handle inline_waiter;
	mali_sync_handle      dump_sync;

	mali_bool           disable_dump;
	mali_bool           crash_dump_enable;
	mali_bool           pre_run_dump_enable;
	mali_bool           post_run_dump_enable;
	mali_bool           pre_run_dump_done;
	mali_bool           post_run_dump_done;
	char                last_mem_dump_file_name[512];
} mali_dump_job_info ;

#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DUMP_JOBS_DATATYPES_FRONTEND_H_ */
