/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include "mali_osu.h"
#include "mali_uku.h"
/* common backend code */
#include "base_arch_main.h"

#include <base/mali_vsync.h>

void _mali_base_arch_vsync_event_report(mali_vsync_event event)
{
	_vr_uk_vsync_event_report_s args = {0, 0};
	args.ctx = mali_uk_ctx;
	/* Convert from U to UK type.
	 * This depends on _vr_uk_vsync_event and mali_vsync_event being
	 * kept in sync.
	 */
	args.event = (_vr_uk_vsync_event)event;

	_mali_uku_vsync_event_report(&args);
}
