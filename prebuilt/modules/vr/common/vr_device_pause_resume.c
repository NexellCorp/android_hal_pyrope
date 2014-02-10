/**
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_device_pause_resume.c
 * Implementation of the VR pause/resume functionality
 */

#include "vr_gp_scheduler.h"
#include "vr_pp_scheduler.h"
#include "vr_group.h"

void vr_dev_pause(vr_bool *power_is_on)
{
	vr_bool power_is_on_tmp;

	/* Locking the current power state - so it will not switch from being ON to OFF, but it might remain OFF */
	power_is_on_tmp = _vr_osk_pm_dev_ref_add_no_power_on();
	if (NULL != power_is_on)
	{
		*power_is_on = power_is_on_tmp;
	}

	vr_gp_scheduler_suspend();
	vr_pp_scheduler_suspend();
}

void vr_dev_resume(void)
{
	vr_gp_scheduler_resume();
	vr_pp_scheduler_resume();

	/* Release our PM reference, as it is now safe to turn of the GPU again */
	_vr_osk_pm_dev_ref_dec_no_power_on();
}
