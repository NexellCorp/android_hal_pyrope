/**
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_osk.h"
#include "vr_kernel_common.h"
#include "vr_uk_types.h"
#include "vr_user_settings_db.h"
#include "vr_session.h"

static u32 vr_user_settings[_VR_UK_USER_SETTING_MAX];
const char *_vr_uk_user_setting_descriptions[] = _VR_UK_USER_SETTING_DESCRIPTIONS;

static void vr_user_settings_notify(_vr_uk_user_setting_t setting, u32 value)
{
	struct vr_session_data *session, *tmp;

	vr_session_lock();
	VR_SESSION_FOREACH(session, tmp, link)
	{
		_vr_osk_notification_t *notobj = _vr_osk_notification_create(_VR_NOTIFICATION_SETTINGS_CHANGED, sizeof(_vr_uk_settings_changed_s));
		_vr_uk_settings_changed_s *data = notobj->result_buffer;
		data->setting = setting;
		data->value = value;

		vr_session_send_notification(session, notobj);
	}
	vr_session_unlock();
}

void vr_set_user_setting(_vr_uk_user_setting_t setting, u32 value)
{
	vr_bool notify = VR_FALSE;

	VR_DEBUG_ASSERT(setting < _VR_UK_USER_SETTING_MAX && setting >= 0);

	if (vr_user_settings[setting] != value)
	{
		notify = VR_TRUE;
	}

	vr_user_settings[setting] = value;

	if (notify)
	{
		vr_user_settings_notify(setting, value);
	}
}

u32 vr_get_user_setting(_vr_uk_user_setting_t setting)
{
	VR_DEBUG_ASSERT(setting < _VR_UK_USER_SETTING_MAX && setting >= 0);

	return vr_user_settings[setting];
}

_vr_osk_errcode_t _vr_ukk_get_user_setting(_vr_uk_get_user_setting_s *args)
{
	_vr_uk_user_setting_t setting;
	VR_DEBUG_ASSERT_POINTER(args);

	setting = args->setting;

	if (0 <= setting && _VR_UK_USER_SETTING_MAX > setting)
	{
		args->value = vr_user_settings[setting];
		return _VR_OSK_ERR_OK;
	}
	else
	{
		return _VR_OSK_ERR_INVALID_ARGS;
	}
}

_vr_osk_errcode_t _vr_ukk_get_user_settings(_vr_uk_get_user_settings_s *args)
{
	VR_DEBUG_ASSERT_POINTER(args);

	_vr_osk_memcpy(args->settings, vr_user_settings, sizeof(vr_user_settings));

	return _VR_OSK_ERR_OK;
}
