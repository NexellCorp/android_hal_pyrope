/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _BASE_ARCH_PROFILING_H_
#define _BASE_ARCH_PROFILING_H_

#if MALI_TIMELINE_PROFILING_ENABLED || MALI_SW_COUNTERS_ENABLED

#include <base/mali_types.h>
#include <cinstr/mali_cinstr_common.h>

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
u32 _mali_base_arch_profiling_start(u32 limit);

/**
 * Add/register an profiling event.
 * @param evt pointer to buffer with event and event data.
 */
MALI_NOTRACE void _mali_base_arch_profiling_add_event(const cinstr_profiling_entry_t* evt);

/**
 * Stop system-wide gathering of profiling data.
 * @return Number of recorded events.
 */
u32 _mali_base_arch_profiling_stop(void);

/**
 * Retrieve recorded event.
 * @param index Index of recorded event, starting at 0.
 * @param evt Pointer to buffer receiving the event and event data.
 * @return MALI_TRUE for success, MALI_FALSE for failure.
 */
mali_bool _mali_base_arch_profiling_get_event(u32 index, cinstr_profiling_entry_t* evt);

/**
 * Clear the recorded events.
 * Once all events are retrieved, the event buffer should be cleared by calling this function,
 * and thus allowing another recording session to begin.
 */
void _mali_base_arch_profiling_clear(void);

/**
 * Check if current user space process should generate events or not.
 * @return MALI_TRUE if profiling events should be turned on, otherwise MALI_FALSE
 */
mali_bool _mali_base_arch_profiling_get_enable_state(void);

#endif /* MALI_TIMELINE_PROFILING_ENABLED */

#if MALI_SW_COUNTERS_ENABLED
/**
 * Send a set of software counters out from the driver.
 * @param counters The list of counter values to report.
 * @param num_counters Number of elements in counters array.
 */
void _mali_base_arch_profiling_report_sw_counters(u32* counters, u32 num_counters);

#endif /* MALI_SW_COUNTERS_ENABLED */

#if MALI_FRAMEBUFFER_DUMP_ENABLED
/**
 * Setup a static annotation channel to be used by _mali_base_profiling_annotate_write.
 *
 * @return MALI_TRUE if the channel was available and opened successfully, MALI_FALSE otherwise.
 */
mali_bool _mali_base_arch_profiling_annotate_setup(void);

/**
 * Fully write data to the annotation channel opened by _mali_base_profiling_annotate_setup.
 * If the channel has not been opened this function does nothing.
 *
 * @param data Pointer to the data to be written.
 * @param length Number of bytes to be written.
 */
void _mali_base_arch_profiling_annotate_write(const void* data, u32 length);
#endif


#ifdef __cplusplus
}
#endif


#endif /* _BASE_ARCH_PROFILING_H_ */
