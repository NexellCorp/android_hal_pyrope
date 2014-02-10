/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_osk.h"
#include "vr_osk_list.h"
#include "vr_session.h"

_VR_OSK_LIST_HEAD(vr_sessions);

_vr_osk_lock_t *vr_sessions_lock;

_vr_osk_errcode_t vr_session_initialize(void)
{
	_VR_OSK_INIT_LIST_HEAD(&vr_sessions);

	vr_sessions_lock = _vr_osk_lock_init(_VR_OSK_LOCKFLAG_READERWRITER | _VR_OSK_LOCKFLAG_ORDERED, 0, _VR_OSK_LOCK_ORDER_SESSIONS);

	if (NULL == vr_sessions_lock) return _VR_OSK_ERR_NOMEM;

	return _VR_OSK_ERR_OK;
}

void vr_session_terminate(void)
{
	_vr_osk_lock_term(vr_sessions_lock);
}

void vr_session_add(struct vr_session_data *session)
{
	vr_session_lock();
	_vr_osk_list_add(&session->link, &vr_sessions);
	vr_session_unlock();
}

void vr_session_remove(struct vr_session_data *session)
{
	vr_session_lock();
	_vr_osk_list_delinit(&session->link);
	vr_session_unlock();
}
