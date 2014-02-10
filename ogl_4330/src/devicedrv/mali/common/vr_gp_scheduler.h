/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_GP_SCHEDULER_H__
#define __VR_GP_SCHEDULER_H__

#include "vr_osk.h"
#include "vr_gp_job.h"
#include "vr_group.h"

_vr_osk_errcode_t vr_gp_scheduler_initialize(void);
void vr_gp_scheduler_terminate(void);

void vr_gp_scheduler_job_done(struct vr_group *group, struct vr_gp_job *job, vr_bool success);
void vr_gp_scheduler_oom(struct vr_group *group, struct vr_gp_job *job);
void vr_gp_scheduler_abort_session(struct vr_session_data *session);
u32 vr_gp_scheduler_dump_state(char *buf, u32 size);

void vr_gp_scheduler_suspend(void);
void vr_gp_scheduler_resume(void);

/**
 * @brief Reset all groups
 *
 * This function resets all groups known by the GP scheuduler. This must be
 * called after the VR HW has been powered on in order to reset the HW.
 */
void vr_gp_scheduler_reset_all_groups(void);

/**
 * @brief Zap TLB on all groups with \a session active
 *
 * The scheculer will zap the session on all groups it owns.
 */
void vr_gp_scheduler_zap_all_active(struct vr_session_data *session);

#endif /* __VR_GP_SCHEDULER_H__ */
