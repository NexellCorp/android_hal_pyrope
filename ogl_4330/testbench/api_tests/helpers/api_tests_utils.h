/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef API_TESTS_UTILS_H
#define API_TESTS_UTILS_H

/*
	API tests should provide replacement functions for all _mali_sys_ calls. Ideally
	there shouldn't be any calls to _mali_sys_ functions in API tests.
*/

#include "base/mali_types.h"

#ifdef __SYMBIAN32__
#define API_SUITE_SYS_FPRINTF _mali_sys_fprintf
#else
#define API_SUITE_SYS_FPRINTF fprintf
#endif

void 	api_suite_sys_usleep(u64 usec);
u64 	api_suite_sys_get_time_usec(void);


void 	api_suite_sys_abort(void);

#endif

