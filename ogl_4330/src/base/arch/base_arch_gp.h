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
 * @file base_arch_gp.h
 */

#ifndef _BASE_ARCH_GP_H_
#define _BASE_ARCH_GP_H_

#include <base/mali_types.h>
#include <base/common/gp/base_common_gp_job.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Initialize the arch GP system. Multiple concurent users should be
 * supported, so reopening should just add another reference.
 * Upon first open it should perform the needed interaction with the
 * HW/DD to perpare it for handling jobs.
 * @return Initialization status.
 */
mali_err_code _mali_base_arch_gp_open(void) MALI_CHECK_RESULT;

/**
 * Termintate the arch GP system. Multiple concurent users should be
 * supported, so termination should only occure when the last
 * reference is removed.
 */
void _mali_base_arch_gp_close(void);

/**
 * Request for starting a job. An existing job could be returned for
 * requeuing if the new job has a higher priority than a previous
 * start-requested job which the hw hasn't actually started to process.
 * @param job Info about a job to start. Includes priority info used
 * to selectively swap it with existing jobs.
 * @param requeued_job Pointer to a job info struct pointer. Should point
 * to a NULL pointer if no job has been requeued.
 * @return No error if job was started, including a possible requeued job.
 * Return error if job can't be submitted now and should be queued for
 * later
 */
mali_err_code _mali_base_arch_gp_start(mali_gp_job * job) MALI_CHECK_RESULT;

/**
 * Get the number of GP cores on the system.
 * @return The number for GP cores available on the system.
 */
u32 _mali_base_arch_gp_get_core_nr(void);

/**
 * Get the GP core version number.
 * Only supported if @see _mali_base_arch_gp_get_core_nr returned non-zero.
 * @note If multiple cores exists on the system of different hw versions
 * the lowest version number will be returned.
 * @return The hardware version for the GP cores.
 */
u32 _mali_base_arch_gp_get_core_version(void);

/**
 * Function to return version number of MaliGP. To be used if we have several different versions
 * of MaliGP, and some may have bugfixes.
 * @return The core version
 */
u32 _mali_base_arch_gp_get_product_id(void);

/**
 * Resume a job which has been suspended over heap out of memory with a new heap
 * @param cookie The cookie identifying the job which has been suspended
 * @param new_heap_start New heap start
 * @param new_heap_end New heap end
 */
void _mali_base_arch_gp_mem_request_response_new_heap(u32 cookie, u32 new_heap_start, u32 new_heap_end);

/**
 * Abort a job which has been suspended
 * @param cookie The cookie identifying the job which has been suspended
 */
void _mali_base_arch_gp_mem_request_response_abort(u32 cookie);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _BASE_ARCH_GP_H_ */
