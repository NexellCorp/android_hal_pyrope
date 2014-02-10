/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_SW_COUNTERS_H_
#define _GLES_SW_COUNTERS_H_

#include <base/mali_types.h>
#include <shared/m200_gp_frame_builder_inlines.h>
#include <shared/mali_sw_counters.h>

#if MALI_SW_COUNTERS_ENABLED
/**
 * @brief Return the set of software counters for the specified context.
 * @param ctx The GLES context
 * @return The counter structure, or NULL if the counters are not available.
 */
MALI_STATIC_INLINE mali_sw_counters * _gles_get_sw_counters(struct gles_context *ctx)
{
	mali_frame_builder *framebuilder = _gles_get_current_draw_frame_builder(ctx);

	MALI_DEBUG_ASSERT_POINTER( framebuilder );

	/* Locate the counter information within the framebuilder. */
	return _mali_frame_builder_get_counters(framebuilder);
}
#endif /* MALI_SW_COUNTERS_ENABLED */

#endif /* _GLES_SW_COUNTERS_H_ */
