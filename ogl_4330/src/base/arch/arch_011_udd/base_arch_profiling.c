/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <base/arch/base_arch_profiling.h>
#include <base/arch/base_arch_runtime.h>
#include "mali_osu.h"
#include "mali_uku.h"
/* common backend code */
#include "base_arch_main.h"

#if MALI_TIMELINE_PROFILING_ENABLED
u32 _mali_base_arch_profiling_start(u32 limit)
{
	_vr_uk_profiling_start_s args;
	args.ctx = mali_uk_ctx;
	args.limit = limit;
	return (_mali_uku_profiling_start(&args) == _MALI_OSK_ERR_OK) ? args.limit : 0;
}


MALI_NOTRACE void _mali_base_arch_profiling_add_event(const cinstr_profiling_entry_t* evt)
{
	_vr_uk_profiling_add_event_s args;
	args.ctx = mali_uk_ctx;
	args.event_id = evt->event_id;
	args.data[0] = evt->data[0];
	args.data[1] = evt->data[1];
	args.data[2] = evt->data[2];
	args.data[3] = evt->data[3];
	args.data[4] = evt->data[4];

	_mali_uku_profiling_add_event(&args);
}


u32 _mali_base_arch_profiling_stop(void)
{
	_vr_uk_profiling_stop_s args;
	args.ctx = mali_uk_ctx;
	args.count = 0;

	if (_mali_uku_profiling_stop(&args) == _MALI_OSK_ERR_OK)
	{
		return args.count;
	}

	return 0;
}


mali_bool _mali_base_arch_profiling_get_event(u32 index, cinstr_profiling_entry_t* evt)
{
	_vr_uk_profiling_get_event_s args;
	args.ctx = mali_uk_ctx;
	args.index = index;

	if (_mali_uku_profiling_get_event(&args) == _MALI_OSK_ERR_OK)
	{
		evt->timestamp = args.timestamp;
		evt->event_id = args.event_id;
		evt->data[0] = args.data[0];
		evt->data[1] = args.data[1];
		evt->data[2] = args.data[2];
		evt->data[3] = args.data[3];
		evt->data[4] = args.data[4];
		return MALI_TRUE;
	}

	return MALI_FALSE;
}


void _mali_base_arch_profiling_clear(void)
{
	_vr_uk_profiling_clear_s args = { NULL, };
	args.ctx = mali_uk_ctx;

	_mali_uku_profiling_clear(&args);
}


mali_bool _mali_base_arch_profiling_get_enable_state(void)
{
	return (0 == _mali_base_arch_get_setting(_VR_UK_USER_SETTING_SW_EVENTS_ENABLE) )? MALI_FALSE : MALI_TRUE;
}

#endif /* MALI_TIMELINE_PROFILING_ENABLED */

#if MALI_SW_COUNTERS_ENABLED
void _mali_base_arch_profiling_report_sw_counters(u32 * counters, u32 num_counters)
{
	_vr_uk_sw_counters_report_s args = {NULL, NULL, 0};
	args.ctx = mali_uk_ctx;
	args.counters = counters;
	args.num_counters = num_counters;

	_mali_uku_profiling_report_sw_counters(&args);
}
#endif /* MALI_SW_COUNTERS_ENABLED */

#if MALI_FRAMEBUFFER_DUMP_ENABLED
mali_bool _mali_base_arch_profiling_annotate_setup(void)
{
	return _mali_osu_annotate_setup();
}

void _mali_base_arch_profiling_annotate_write(const void* data, u32 length)
{
	_mali_osu_annotate_write(data, length);
}
#endif
