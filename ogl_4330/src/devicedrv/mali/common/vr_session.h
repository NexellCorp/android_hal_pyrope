/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_SESSION_H__
#define __VR_SESSION_H__

#include "vr_mmu_page_directory.h"
#include "vr_kernel_descriptor_mapping.h"
#include "vr_osk.h"
#include "vr_osk_list.h"

struct vr_session_data
{
	_vr_osk_notification_queue_t * ioctl_queue;

#ifdef CONFIG_SYNC
	_vr_osk_list_t pending_jobs;
	_vr_osk_lock_t *pending_jobs_lock;
#endif

	_vr_osk_lock_t *memory_lock; /**< Lock protecting the vm manipulation */
	vr_descriptor_mapping * descriptor_mapping; /**< Mapping between userspace descriptors and our pointers */
	_vr_osk_list_t memory_head; /**< Track all the memory allocated in this session, for freeing on abnormal termination */

	struct vr_page_directory *page_directory; /**< MMU page directory for this session */

	_VR_OSK_LIST_HEAD(link); /**< Link for list of all sessions */

	_VR_OSK_LIST_HEAD(job_list); /**< List of all jobs on this session */
};

_vr_osk_errcode_t vr_session_initialize(void);
void vr_session_terminate(void);

/* List of all sessions. Actual list head in vr_kernel_core.c */
extern _vr_osk_list_t vr_sessions;
/* Lock to protect modification and access to the vr_sessions list */
extern _vr_osk_lock_t *vr_sessions_lock;

VR_STATIC_INLINE void vr_session_lock(void)
{
	_vr_osk_lock_wait(vr_sessions_lock, _VR_OSK_LOCKMODE_RW);
}

VR_STATIC_INLINE void vr_session_unlock(void)
{
	_vr_osk_lock_signal(vr_sessions_lock, _VR_OSK_LOCKMODE_RW);
}

void vr_session_add(struct vr_session_data *session);
void vr_session_remove(struct vr_session_data *session);
#define VR_SESSION_FOREACH(session, tmp, link) \
	_VR_OSK_LIST_FOREACHENTRY(session, tmp, &vr_sessions, struct vr_session_data, link)

VR_STATIC_INLINE struct vr_page_directory *vr_session_get_page_directory(struct vr_session_data *session)
{
	return session->page_directory;
}

VR_STATIC_INLINE void vr_session_send_notification(struct vr_session_data *session, _vr_osk_notification_t *object)
{
	_vr_osk_notification_queue_send(session->ioctl_queue, object);
}

#endif /* __VR_SESSION_H__ */
