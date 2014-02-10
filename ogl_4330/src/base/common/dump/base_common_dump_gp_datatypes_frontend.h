/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_dump_gp_datatypes_frontend.h
 * Datatypes that is exposed to the non-dumping parts of Base driver.
 * Included by \a mali_gp_job.h for the \a mali_gp_job struct.
 */

#ifndef _BASE_COMMON_DUMP_GP_DATATYPES_FRONTEND_H_
#define _BASE_COMMON_DUMP_GP_DATATYPES_FRONTEND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "base_common_dump_jobs_datatypes_frontend.h"
#include <base/mali_memory.h>


typedef struct mali_dump_job_info_gp
{
	mali_dump_job_info gp;
	mali_mem_handle		pointer_array_user;
	u32					pointer_array_first_elements[4];
	void *              pointer_array_patch_backup;
} mali_dump_job_info_gp ;

#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DUMP_GP_DATATYPES_FRONTEND_H_ */
