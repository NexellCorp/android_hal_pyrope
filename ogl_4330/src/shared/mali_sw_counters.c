/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/mali_sw_counters.h>
#include <base/mali_profiling.h>

MALI_EXPORT struct mali_sw_counters* _mali_sw_counters_alloc()
{
	/* just return an NULL'ed memory block. 
	 * No special initialization needed. */
	return _mali_sys_calloc( 1, sizeof( mali_sw_counters ) );
}


MALI_EXPORT void _mali_sw_counters_free(struct mali_sw_counters* counters)
{
	_mali_sys_free(counters);
}

MALI_EXPORT void _mali_sw_counters_dump(struct mali_sw_counters* counters)
{
	u32* counter_list;

	MALI_DEBUG_ASSERT_POINTER(counters);

	counter_list = counters->counter;

	_mali_base_profiling_report_sw_counters(counter_list, sizeof(counters->counter) / sizeof(counters->counter[0]));

}


MALI_EXPORT void _mali_sw_counters_reset(struct mali_sw_counters* counters)
{
	MALI_DEBUG_ASSERT_POINTER(counters);
	_mali_sys_memset( counters, 0, sizeof( mali_sw_counters ) );
}


