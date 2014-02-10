/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_dump_jobs_functions_frontend.h
 * Functions exposed to the NON-dumping parts of Base driver.
 * Included by base_common_context.c  to open and close the dump system.
 */

#ifndef _BASE_COMMON_DUMP_JOBS_FUNCTIONS_FRONTEND_H_
#define _BASE_COMMON_DUMP_JOBS_FUNCTIONS_FRONTEND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <base/mali_types.h>

/**
 * Open the dump system. This function must be called prior to any other dump function calls.
 * Returning MALI_TRUE if initialization was successful.
 * Open and close is not reference counted, and should only be done by when Base driver opens closes.
 */
mali_err_code _mali_dump_system_open(mali_base_ctx_handle ctx);

/**
 * Closes the dump system. Calling other dumping functions after this function has been called will lead to segmentation faults.
 * Open and close is not reference counted, and should only be done by when Base driver opens closes.
 */
void _mali_dump_system_close(mali_base_ctx_handle ctx);


#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DUMP_JOBS_FUNCTIONS_FRONTEND_H_ */

