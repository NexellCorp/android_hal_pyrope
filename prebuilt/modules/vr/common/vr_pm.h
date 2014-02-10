/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_PM_H__
#define __VR_PM_H__

#include "vr_osk.h"

enum vr_core_event
{
	VR_CORE_EVENT_GP_START,
	VR_CORE_EVENT_GP_STOP,
	VR_CORE_EVENT_PP_START,
	VR_CORE_EVENT_PP_STOP
};

_vr_osk_errcode_t vr_pm_initialize(void);
void vr_pm_terminate(void);

void vr_pm_core_event(enum vr_core_event core_event);

/* Callback functions registered for the runtime PMM system */
void vr_pm_os_suspend(void);
void vr_pm_os_resume(void);
void vr_pm_runtime_suspend(void);
void vr_pm_runtime_resume(void);


#endif /* __VR_PM_H__ */
