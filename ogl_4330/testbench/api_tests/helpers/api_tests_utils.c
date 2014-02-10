/*
 * $Copyright:
 * ----------------------------------------------------------------
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 * ----------------------------------------------------------------
 * $
 */
#include "api_tests_utils.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#elif _POSIX_C_SOURCE < 200112L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#elif _XOPEN_SOURCE < 600
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "base/mali_macros.h"

void api_suite_sys_usleep(u64 usec)
{
	while (usec > 999999ULL)
	{
		usleep(999999UL);
		usec -= 999999ULL;
	}
	usleep(usec);
}

u64 api_suite_sys_get_time_usec(void)
{
	int result;
	struct timeval tod;

	result = gettimeofday(&tod, NULL);

	/* gettimeofday returns non-null on error*/
	if (0 != result) return 0;

	return (MALI_REINTERPRET_CAST(u64)tod.tv_sec) * 1000000ULL + tod.tv_usec;
}

void api_suite_sys_abort(void)
{
	abort();
}

