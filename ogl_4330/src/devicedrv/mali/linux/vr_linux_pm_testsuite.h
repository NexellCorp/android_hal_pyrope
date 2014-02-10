/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef __VR_LINUX_PM_TESTSUITE_H__
#define __VR_LINUX_PM_TESTSUITE_H__

#if VR_POWER_MGMT_TEST_SUITE && defined(CONFIG_PM)

typedef enum
{
        _VR_DEVICE_PMM_TIMEOUT_EVENT,
        _VR_DEVICE_PMM_JOB_SCHEDULING_EVENTS,
	_VR_DEVICE_PMM_REGISTERED_CORES,
        _VR_DEVICE_MAX_PMM_EVENTS

} _vr_device_pmm_recording_events;

extern unsigned int vr_timeout_event_recording_on;
extern unsigned int vr_job_scheduling_events_recording_on;
extern unsigned int pwr_mgmt_status_reg;
extern unsigned int is_vr_pmm_testsuite_enabled;
extern unsigned int is_vr_pmu_present;

#endif /* VR_POWER_MGMT_TEST_SUITE && defined(CONFIG_PM) */

#endif /* __VR_LINUX_PM_TESTSUITE_H__ */
