/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_DEVICE_PAUSE_RESUME_H__
#define __VR_DEVICE_PAUSE_RESUME_H__

#include "vr_osk.h"

/**
 * Pause the scheduling and power state changes of VR device driver.
 * vr_dev_resume() must always be called as soon as possible after this function
 * in order to resume normal operation of the VR driver.
 *
 * @param power_is_on Receives the power current status of VR GPU. VR_TRUE if GPU is powered on
 */
void vr_dev_pause(vr_bool *power_is_on);

/**
 * Resume scheduling and allow power changes in VR device driver.
 * This must always be called after vr_dev_pause().
 */
void vr_dev_resume(void);

#endif /* __VR_DEVICE_PAUSE_RESUME_H__ */
