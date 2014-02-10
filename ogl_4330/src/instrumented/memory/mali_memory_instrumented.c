/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_memory_instrumented.h"

#include "mali_counters.h"
#include "mali_instrumented_context.h"

MALI_EXPORT
mali_err_code MALI_CHECK_RESULT _mali_memory_instrumented_init(mali_instrumented_context *ctx)
{
	_instrumented_start_group(ctx,
		"Memory Statistics",
		"Memory usage and recovery statistics for the mali driver stack",
		0);

	_instrumented_end_group(ctx);

	return MALI_ERR_NO_ERROR;
}
