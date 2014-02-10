/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_pm.h"
#include "vr_kernel_common.h"
#include "vr_osk.h"
#include "vr_gp_scheduler.h"
#include "vr_pp_scheduler.h"
#include "vr_scheduler.h"
#include "vr_kernel_utilization.h"
#include "vr_group.h"

static vr_bool vr_power_on = VR_FALSE;

_vr_osk_errcode_t vr_pm_initialize(void)
{
	_vr_osk_pm_dev_enable();
	return _VR_OSK_ERR_OK;
}

void vr_pm_terminate(void)
{
	_vr_osk_pm_dev_disable();
}

void vr_pm_core_event(enum vr_core_event core_event)
{
	VR_DEBUG_ASSERT(VR_CORE_EVENT_GP_START == core_event ||
	                  VR_CORE_EVENT_PP_START == core_event ||
	                  VR_CORE_EVENT_GP_STOP  == core_event ||
	                  VR_CORE_EVENT_PP_STOP  == core_event);

	if (VR_CORE_EVENT_GP_START == core_event || VR_CORE_EVENT_PP_START == core_event)
	{
		_vr_osk_pm_dev_ref_add();
		if (vr_utilization_enabled())
		{
			vr_utilization_core_start(_vr_osk_time_get_ns());
		}
	}
	else
	{
		_vr_osk_pm_dev_ref_dec();
		if (vr_utilization_enabled())
		{
			vr_utilization_core_end(_vr_osk_time_get_ns());
		}
	}
}

/* Reset GPU after power up */
static void vr_pm_reset_gpu(void)
{
	/* Reset all L2 caches */
	vr_l2_cache_reset_all();

	/* Reset all groups */
	vr_scheduler_reset_all_groups();
}

void vr_pm_os_suspend(void)
{
	VR_DEBUG_PRINT(3, ("VR PM: OS suspend\n"));
	vr_gp_scheduler_suspend();
	vr_pp_scheduler_suspend();
	vr_group_power_off();
	vr_power_on = VR_FALSE;
}

void vr_pm_os_resume(void)
{
	VR_DEBUG_PRINT(3, ("VR PM: OS resume\n"));
	if (VR_TRUE != vr_power_on)
	{
		vr_pm_reset_gpu();
		vr_group_power_on();
	}
	vr_gp_scheduler_resume();
	vr_pp_scheduler_resume();
	vr_power_on = VR_TRUE;
}

void vr_pm_runtime_suspend(void)
{
	VR_DEBUG_PRINT(3, ("VR PM: Runtime suspend\n"));
	vr_group_power_off();
	vr_power_on = VR_FALSE;
}

void vr_pm_runtime_resume(void)
{
	VR_DEBUG_PRINT(3, ("VR PM: Runtime resume\n"));
	if (VR_TRUE != vr_power_on)
	{
		vr_pm_reset_gpu();
		vr_group_power_on();
	}
	vr_power_on = VR_TRUE;
}
