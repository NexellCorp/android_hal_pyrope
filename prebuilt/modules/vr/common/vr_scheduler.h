/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_SCHEDULER_H__
#define __VR_SCHEDULER_H__

#include "vr_osk.h"
#include "vr_gp_scheduler.h"
#include "vr_pp_scheduler.h"

_vr_osk_errcode_t vr_scheduler_initialize(void);
void vr_scheduler_terminate(void);

u32 vr_scheduler_get_new_id(void);

/**
 * @brief Reset all groups
 *
 * This function resets all groups known by the both the PP and GP scheuduler.
 * This must be called after the VR HW has been powered on in order to reset
 * the HW.
 */
VR_STATIC_INLINE void vr_scheduler_reset_all_groups(void)
{
	vr_gp_scheduler_reset_all_groups();
	vr_pp_scheduler_reset_all_groups();
}

/**
 * @brief Zap TLB on all active groups running \a session
 *
 * @param session Pointer to the session to zap
 */
VR_STATIC_INLINE void vr_scheduler_zap_all_active(struct vr_session_data *session)
{
	vr_gp_scheduler_zap_all_active(session);
	vr_pp_scheduler_zap_all_active(session);
}

#endif /* __VR_SCHEDULER_H__ */
