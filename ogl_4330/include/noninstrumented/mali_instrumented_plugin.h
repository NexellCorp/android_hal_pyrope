/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _MALI_INSTRUMENTED_PLUGIN_H_
#define _MALI_INSTRUMENTED_PLUGIN_H_

#include "mali_system.h"
#include "cinstr/mali_cinstr_common.h"
#ifdef NEXELL_FEATURE_ENABLE
#include "vr_utgard_counters.h"
#else
#include "mali_utgard_counters.h"
#endif

struct mali_counter;

extern mali_bool _mali_instrumented_enabled_features[CINSTR_CLIENTAPI_COUNT];

#define MALI_INSTRUMENTED_FEATURE_IS_ENABLED(api, feature) (_mali_instrumented_enabled_features[(api)] & (feature))


/**
 * Load instrumented plug-in.
 */
MALI_IMPORT void _mali_instrumented_plugin_load(void);

/**
 * Unloads the instrumented plug-in.
 */
MALI_IMPORT void _mali_instrumented_plugin_unload(void);

/**
 * Enable an instrumented feature.
 *
 * @param api The API to enable this feature for.
 * @param feature The feature to enable.
 * @return CINSTR_ERROR_SUCCESS on success, otherwise failure.
 */
MALI_IMPORT cinstr_error_t _mali_instrumented_plugin_feature_enable(cinstr_clientapi_t api, cinstr_feature_t feature);

/**
 * Report frame completed event to plug-in.
 *
 * @param type Event type to report.
 * @param frame_number The frame number of the completed frame.
 */
MALI_IMPORT void _mali_instrumented_plugin_send_event_frame_complete(cinstr_event_t type, u32 frame_number);

/**
 * Report frame completed event to plug-in.
 *
 * @param type Event type to report.
 * @param render_pass_number The number of the completed render pass (counting done per frame).
 * @param frame_number The frame number which the render pass belongs to.
 */
MALI_IMPORT void _mali_instrumented_plugin_send_event_render_pass_complete(cinstr_event_t type, u32 render_pass_number, u32 frame_number);

/**
 * Report performance counter to plug-in.
 *
 * @param source Source of counters (GP/PP0/EGL/etc).
 * @param count Number of counters (elements in counters and values array).
 * @param counters The array of the counters reported.
 * @param values Array of counter values. Matches index for index with the available counters).
 */
MALI_IMPORT void _mali_instrumented_plugin_send_event_counters(cinstr_counter_source source, u32 count, struct mali_counter* counters, s64* values);

#endif /* _MALI_INSTRUMENTED_PLUGIN_H_ */
