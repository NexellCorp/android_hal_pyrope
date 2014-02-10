/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_KERNEL_UTILIZATION_H__
#define __VR_KERNEL_UTILIZATION_H__

#include "vr_osk.h"

extern void (*vr_utilization_callback)(unsigned int);

/**
 * Initialize/start the VR GPU utilization metrics reporting.
 *
 * @return _VR_OSK_ERR_OK on success, otherwise failure.
 */
_vr_osk_errcode_t vr_utilization_init(void);

/**
 * Terminate the VR GPU utilization metrics reporting
 */
void vr_utilization_term(void);

/**
 * Check if VR utilization is enabled
 */
VR_STATIC_INLINE vr_bool vr_utilization_enabled(void)
{
	return (NULL != vr_utilization_callback);
}

/**
 * Should be called when a job is about to execute a job
 */
void vr_utilization_core_start(u64 time_now);

/**
 * Should be called to stop the utilization timer during system suspend
 */
void vr_utilization_suspend(void);

/**
 * Should be called when a job has completed executing a job
 */
void vr_utilization_core_end(u64 time_now);

#endif /* __VR_KERNEL_UTILIZATION_H__ */
