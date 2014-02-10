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
 * @file base_common_ds_context.h
 * Functions exposed to the NON-dumping parts of Base driver.
 * Included by base_common_context.c  to open and close the dump system.
 */

#ifndef _BASE_COMMON_DS_CONTEXT_H_
#define _BASE_COMMON_DS_CONTEXT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <base/mali_types.h>

/**
  * Open the dependency_system.
  * Returning MALI_TRUE if initialization was successful.
  * Open and close is not reference counted, and should only be done by when Base driver open/close.
  * @param ctx Handle to the base context
  * @return An error code of type mali_error_code
  */
mali_err_code mali_common_dependency_system_open(mali_base_ctx_handle ctx);

/**
 * Closes the dependency_system.
 * Open and close is not reference counted, and should only be done by when Base driver open/close.
 * @param ctx Handle to the base context
 */
void mali_common_dependency_system_close(mali_base_ctx_handle ctx);

#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DS_CONTEXT_H_ */

