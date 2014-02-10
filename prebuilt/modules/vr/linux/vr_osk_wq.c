/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_osk_wq.c
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#include <linux/slab.h>	/* For memory allocation */
#include <linux/workqueue.h>
#include <linux/version.h>

#include "vr_osk.h"
#include "vr_kernel_common.h"
#include "vr_kernel_license.h"
#include "vr_kernel_linux.h"

typedef struct _vr_osk_wq_work_t_struct
{
	_vr_osk_wq_work_handler_t handler;
	void *data;
	struct work_struct work_handle;
} vr_osk_wq_work_object_t;

#if VR_LICENSE_IS_GPL
struct workqueue_struct *vr_wq = NULL;
#endif

static void _vr_osk_wq_work_func ( struct work_struct *work );

_vr_osk_errcode_t _vr_osk_wq_init(void)
{
#if VR_LICENSE_IS_GPL
	VR_DEBUG_ASSERT(NULL == vr_wq);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	vr_wq = alloc_workqueue("vr", WQ_UNBOUND, 0);
#else
	vr_wq = create_workqueue("vr");
#endif
	if(NULL == vr_wq)
	{
		VR_PRINT_ERROR(("Unable to create VR workqueue\n"));
		return _VR_OSK_ERR_FAULT;
	}
#endif

	return _VR_OSK_ERR_OK;
}

void _vr_osk_wq_flush(void)
{
#if VR_LICENSE_IS_GPL
       flush_workqueue(vr_wq);
#else
       flush_scheduled_work();
#endif
}

void _vr_osk_wq_term(void)
{
#if VR_LICENSE_IS_GPL
	VR_DEBUG_ASSERT(NULL != vr_wq);

	flush_workqueue(vr_wq);
	destroy_workqueue(vr_wq);
	vr_wq = NULL;
#else
	flush_scheduled_work();
#endif
}

_vr_osk_wq_work_t *_vr_osk_wq_create_work( _vr_osk_wq_work_handler_t handler, void *data )
{
	vr_osk_wq_work_object_t *work = kmalloc(sizeof(vr_osk_wq_work_object_t), GFP_KERNEL);

	if (NULL == work) return NULL;

	work->handler = handler;
	work->data = data;

	INIT_WORK( &work->work_handle, _vr_osk_wq_work_func );

	return work;
}

void _vr_osk_wq_delete_work( _vr_osk_wq_work_t *work )
{
	vr_osk_wq_work_object_t *work_object = (vr_osk_wq_work_object_t *)work;
	_vr_osk_wq_flush();
	kfree(work_object);
}

void _vr_osk_wq_schedule_work( _vr_osk_wq_work_t *work )
{
	vr_osk_wq_work_object_t *work_object = (vr_osk_wq_work_object_t *)work;
#if VR_LICENSE_IS_GPL
	queue_work(vr_wq, &work_object->work_handle);
#else
	schedule_work(&work_object->work_handle);
#endif
}

static void _vr_osk_wq_work_func ( struct work_struct *work )
{
	vr_osk_wq_work_object_t *work_object;

	work_object = _VR_OSK_CONTAINER_OF(work, vr_osk_wq_work_object_t, work_handle);
	work_object->handler(work_object->data);
}

