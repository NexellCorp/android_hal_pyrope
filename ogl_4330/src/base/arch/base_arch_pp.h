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
 * @file base_arch_pp.h
 */

#ifndef _BASE_ARCH_PP_H_
#define _BASE_ARCH_PP_H_

#include <base/mali_types.h>
#include "base/common/pp/base_common_pp_job.h"
#include <sync/mali_external_sync.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Initialize the arch PP system. Multiple concurent users should be
 * supported, so reopening should just add another reference.
 * Upon first open it should perform the needed interaction with the
 * HW/DD to perpare it for handling jobs.
 * @return Mali err code indicating initialization status.
 */
mali_err_code _mali_base_arch_pp_open(void) MALI_CHECK_RESULT;

/**
 * Termintate the arch PP system. Multiple concurent users should be
 * supported, so termination should only occure when the last
 * reference is removed.
 */
void _mali_base_arch_pp_close(void);

mali_err_code _mali_base_arch_pp_start(mali_pp_job *job, mali_bool no_notification, mali_fence_handle *fence) MALI_CHECK_RESULT;

/**
 * Get the number of PP cores on the system.
 * @return The number for PP cores available on the system.
 */
u32 _mali_base_arch_pp_get_num_cores(void);

/**
 * Get the PP core version number.
 * Only supported if @see _mali_base_arch_pp_get_num_cores returned non-zero.
 * @note If multiple cores exists on the system of different hw versions
 * the lowest version number will be returned.
 * @return The hardware version for the PP cores.
 */
u32 _mali_base_arch_pp_get_core_version(void);

/**
 * Function to return version number of MaliPP. To be used if we have several different versions
 * of MaliPP, and some may have bugfixes.
 * @return The core version
 */
u32 _mali_base_arch_pp_get_product_id(void);

/**
 * Disable Write-back unit(s) on the job with the specified frame builder and flush IDs
 *  @param fb_id		Frame Builder ID
 *  @param flush_id		Flush ID
 *  @param wbx          Which write-back units to disable
 */
void _mali_base_arch_pp_job_disable_wb(u32 fb_id, u32 flush_id, mali_pp_job_wbx_flag wbx);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_BASE_ARCH_PP_H_ */
