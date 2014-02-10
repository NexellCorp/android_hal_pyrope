/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_counters.h"

#include "mali_egl_instrumented_types.h"
#include "mali_instrumented_context_types.h"

MALI_EXPORT
mali_err_code _egl_instrumentation_init(mali_instrumented_context* ctx)
{
	_instrumented_register_counter_egl_begin(ctx);

	_instrumented_start_group(ctx, "EGL", "EGL stats, including number of calls, timing information, etc.", 0);
		_instrumented_start_group(ctx, "Timing", "Time spent in various parts of EGL", 0);
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_EGL_BLIT_TIME,
				"Blitting", "Time spent blitting the framebuffer from video memory to framebuffer",
				"us", MALI_SCALE_NONE ));
		_instrumented_end_group(ctx);
	_instrumented_end_group(ctx);

	_instrumented_register_counter_egl_end(ctx);

	MALI_SUCCESS;
}

