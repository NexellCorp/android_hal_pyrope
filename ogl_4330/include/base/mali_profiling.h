/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#ifndef _MALI_PROFILING_H
#define _MALI_PROFILING_H

#ifdef NEXELL_FEATURE_ENABLE
#include <vr_utgard_profiling_events.h>
#else
#include <mali_utgard_profiling_events.h>
#endif
#if MALI_TIMELINE_PROFILING_ENABLED || MALI_SW_COUNTERS_ENABLED

#include <mali_system.h>
#include <cinstr/mali_cinstr_common.h>
#include <base/arch/base_arch_profiling.h>
#include <mali_instrumented_plugin.h>

#endif /* MALI_TIMELINE_PROFILING_ENABLED */

#ifdef __cplusplus
extern "C" {
#endif

#if MALI_TIMELINE_PROFILING_ENABLED

/**
 * Starting system-wide gathering of profiling data.
 * Only one recording session can be started at any time, since this is system-wide.
 * @param limit The desired limit for number of events to record.
 * @return The actual limit (might be lower than requested) of number of events to record. 0 on failure.
 */
MALI_STATIC_INLINE u32 _mali_base_profiling_start(u32 limit)
{
	return _mali_base_arch_profiling_start(limit);
}

/**
 * Add/register an profiling event.
 * @param event_id ID of event to register.
 * @param data0 Custom data associated with event
 * @param data1 Custom data associated with event
 * @param data2 Custom data associated with event
 * @param data3 Custom data associated with event
 * @param data4 Custom data associated with event
 */
MALI_NOTRACE MALI_STATIC_INLINE void _mali_base_profiling_add_event(u32 event_id, u32 data0, u32 data1, u32 data2, u32 data3, u32 data4);
MALI_STATIC_INLINE void _mali_base_profiling_add_event(u32 event_id, u32 data0, u32 data1, u32 data2, u32 data3, u32 data4)
{
	/* Don't generate any events if profiling isn't turned on for this application */
	if (MALI_INSTRUMENTED_FEATURE_IS_ENABLED(CINSTR_CLIENTAPI_COMMON, CINSTR_FEATURE_TIMELINE_PROFILING))
	{
		cinstr_profiling_entry_t evt;
		evt.event_id = event_id;
		evt.data[0] = data0;
		evt.data[1] = data1;
		evt.data[2] = data2;
		evt.data[3] = data3;
		evt.data[4] = data4;

		_mali_base_arch_profiling_add_event(&evt);
	}
}

/**
 * Stop system-wide gathering of profiling data.
 * @return Number of recorded events.
 */
MALI_STATIC_INLINE u32 _mali_base_profiling_stop(void)
{
	return _mali_base_arch_profiling_stop();
}

/**
 * Retrieve recorded event.
 * @param index Index of recorded event, starting at 0.
 * @param evt Pointer to buffer receiving the event and event data.
 * @return MALI_TRUE for success, MALI_FALSE for failure.
 */
MALI_STATIC_INLINE mali_bool _mali_base_profiling_get_event(u32 index, cinstr_profiling_entry_t* evt)
{
	return _mali_base_arch_profiling_get_event(index, evt);
}

/**
 * Clear the recorded events.
 * Once all events are retrieved, the event buffer should be cleared by calling this function,
 * and thus allowing another recording session to begin.
 */
MALI_STATIC_INLINE void _mali_base_profiling_clear(void)
{
	_mali_base_arch_profiling_clear();
}

/**
 * Check if current user space process should generate events or not.
 * @return MALI_TRUE if profiling events should be turned on, otherwise MALI_FALSE
 */
MALI_STATIC_INLINE mali_bool _mali_base_profiling_get_enable_state(void)
{
	return _mali_base_arch_profiling_get_enable_state();
}

#endif /* MALI_TIMELINE_PROFILING_ENABLED */


#if MALI_SW_COUNTERS_ENABLED
/**
 * Send a set of software counters out from the driver.
 * @param counters The list of counter values to report.
 * @param num_counters Number of elements in counters array.
 */
MALI_STATIC_INLINE void _mali_base_profiling_report_sw_counters(u32 * counters, u32 num_counters)
{
	_mali_base_arch_profiling_report_sw_counters(counters, num_counters);
}

#endif /* MALI_SW_COUNTERS_ENABLED */


#if MALI_FRAMEBUFFER_DUMP_ENABLED
/**
 * Setup a static annotation channel to be used by _mali_base_profiling_annotate_write.
 *
 * @return MALI_TRUE if the channel was available and opened successfully, MALI_FALSE otherwise.
 */
MALI_STATIC_INLINE mali_bool _mali_base_profiling_annotate_setup(void)
{
	return _mali_base_arch_profiling_annotate_setup();
}

/**
 * Fully write data to the annotation channel opened by _mali_base_profiling_annotate_setup.
 * If the channel has not been opened this function does nothing.
 *
 * @param data Pointer to the data to be written.
 * @param length Number of bytes to be written.
 */
MALI_STATIC_INLINE void _mali_base_profiling_annotate_write(const void* data, u32 length)
{
	_mali_base_arch_profiling_annotate_write(data, length);
}
#endif


#ifdef __cplusplus
}
#endif


#if MALI_TIMELINE_PROFILING_FUNC_ENABLED

#include "mali_profiling_func_id.h"

#define MALI_PROFILING_ENTER_FUNC(name) _mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SINGLE_SW_ENTER_API_FUNC, 0, 0, TIMELINE_PROFILING_FUNC_ID_##name, 0, 0);		 
#define MALI_PROFILING_LEAVE_FUNC(name) _mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SINGLE_SW_LEAVE_API_FUNC, 0, 0, TIMELINE_PROFILING_FUNC_ID_##name, 0, 0);

#else /* MALI_TIMELINE_PROFILING_FUNC_ENABLED */

#define MALI_PROFILING_ENTER_FUNC(name)
#define MALI_PROFILING_LEAVE_FUNC(name)

#endif /* MALI_TIMELINE_PROFILING_FUNC_ENABLED */

#endif /*_MALI_PROFILING_H */
