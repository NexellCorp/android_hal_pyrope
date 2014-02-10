/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_kernel_common.h"
#include "vr_osk.h"

static _vr_osk_atomic_t vr_job_autonumber;

_vr_osk_errcode_t vr_scheduler_initialize(void)
{
	if ( _VR_OSK_ERR_OK != _vr_osk_atomic_init(&vr_job_autonumber, 0))
	{
		VR_DEBUG_PRINT(1,  ("Initialization of atomic job id counter failed.\n"));
		return _VR_OSK_ERR_FAULT;
	}

	return _VR_OSK_ERR_OK;
}

void vr_scheduler_terminate(void)
{
	_vr_osk_atomic_term(&vr_job_autonumber);
}

u32 vr_scheduler_get_new_id(void)
{
	u32 job_id = _vr_osk_atomic_inc_return(&vr_job_autonumber);
	return job_id;
}

