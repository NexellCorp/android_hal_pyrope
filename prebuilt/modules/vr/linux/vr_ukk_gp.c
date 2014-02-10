/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <linux/fs.h>       /* file system operations */
#include <asm/uaccess.h>    /* user space access */

#include "vr_ukk.h"
#include "vr_osk.h"
#include "vr_kernel_common.h"
#include "vr_session.h"
#include "vr_ukk_wrappers.h"

int gp_start_job_wrapper(struct vr_session_data *session_data, _vr_uk_gp_start_job_s __user *uargs)
{
	_vr_osk_errcode_t err;

	VR_CHECK_NON_NULL(uargs, -EINVAL);
	VR_CHECK_NON_NULL(session_data, -EINVAL);

	err = _vr_ukk_gp_start_job(session_data, uargs);
	if (_VR_OSK_ERR_OK != err) return map_errcode(err);

	return 0;
}

int gp_get_core_version_wrapper(struct vr_session_data *session_data, _vr_uk_get_gp_core_version_s __user *uargs)
{
    _vr_uk_get_gp_core_version_s kargs;
    _vr_osk_errcode_t err;

    VR_CHECK_NON_NULL(uargs, -EINVAL);
    VR_CHECK_NON_NULL(session_data, -EINVAL);

    kargs.ctx = session_data;
    err =  _vr_ukk_get_gp_core_version(&kargs);
    if (_VR_OSK_ERR_OK != err) return map_errcode(err);

	/* no known transactions to roll-back */

    if (0 != put_user(kargs.version, &uargs->version)) return -EFAULT;

    return 0;
}

int gp_suspend_response_wrapper(struct vr_session_data *session_data, _vr_uk_gp_suspend_response_s __user *uargs)
{
    _vr_uk_gp_suspend_response_s kargs;
    _vr_osk_errcode_t err;

    VR_CHECK_NON_NULL(uargs, -EINVAL);
    VR_CHECK_NON_NULL(session_data, -EINVAL);

    if (0 != copy_from_user(&kargs, uargs, sizeof(_vr_uk_gp_suspend_response_s))) return -EFAULT;

    kargs.ctx = session_data;
    err = _vr_ukk_gp_suspend_response(&kargs);
    if (_VR_OSK_ERR_OK != err) return map_errcode(err);

    if (0 != put_user(kargs.cookie, &uargs->cookie)) return -EFAULT;

    /* no known transactions to roll-back */
    return 0;
}

int gp_get_number_of_cores_wrapper(struct vr_session_data *session_data, _vr_uk_get_gp_number_of_cores_s __user *uargs)
{
    _vr_uk_get_gp_number_of_cores_s kargs;
    _vr_osk_errcode_t err;

    VR_CHECK_NON_NULL(uargs, -EINVAL);
    VR_CHECK_NON_NULL(session_data, -EINVAL);

    kargs.ctx = session_data;
    err = _vr_ukk_get_gp_number_of_cores(&kargs);
    if (_VR_OSK_ERR_OK != err) return map_errcode(err);

	/* no known transactions to roll-back */

    if (0 != put_user(kargs.number_of_cores, &uargs->number_of_cores)) return -EFAULT;

    return 0;
}
