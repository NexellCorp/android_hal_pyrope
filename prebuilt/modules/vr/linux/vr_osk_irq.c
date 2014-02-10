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
 * @file vr_osk_irq.c
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#include <linux/slab.h>	/* For memory allocation */

#include "vr_osk.h"
#include "vr_kernel_common.h"
#include "linux/interrupt.h"

typedef struct _vr_osk_irq_t_struct
{
	u32 irqnum;
	void *data;
	_vr_osk_irq_uhandler_t uhandler;
} vr_osk_irq_object_t;

typedef irqreturn_t (*irq_handler_func_t)(int, void *, struct pt_regs *);
static irqreturn_t irq_handler_upper_half (int port_name, void* dev_id ); /* , struct pt_regs *regs*/

_vr_osk_irq_t *_vr_osk_irq_init( u32 irqnum, _vr_osk_irq_uhandler_t uhandler, void *int_data, _vr_osk_irq_trigger_t trigger_func, _vr_osk_irq_ack_t ack_func, void *probe_data, const char *description )
{
	vr_osk_irq_object_t *irq_object;

	irq_object = kmalloc(sizeof(vr_osk_irq_object_t), GFP_KERNEL);
	if (NULL == irq_object)
	{
		return NULL;
	}

	if (-1 == irqnum)
	{
		/* Probe for IRQ */
		if ( (NULL != trigger_func) && (NULL != ack_func) )
		{
			unsigned long probe_count = 3;
			_vr_osk_errcode_t err;
			int irq;

			VR_DEBUG_PRINT(2, ("Probing for irq\n"));

			do
			{
				unsigned long mask;

				mask = probe_irq_on();
				trigger_func(probe_data);

				_vr_osk_time_ubusydelay(5);

				irq = probe_irq_off(mask);
				err = ack_func(probe_data);
			}
			while (irq < 0 && (err == _VR_OSK_ERR_OK) && probe_count--);

			if (irq < 0 || (_VR_OSK_ERR_OK != err)) irqnum = -1;
			else irqnum = irq;
		}
		else irqnum = -1; /* no probe functions, fault */

		if (-1 != irqnum)
		{
			/* found an irq */
			VR_DEBUG_PRINT(2, ("Found irq %d\n", irqnum));
		}
		else
		{
			VR_DEBUG_PRINT(2, ("Probe for irq failed\n"));
		}
	}
	
	irq_object->irqnum = irqnum;
	irq_object->uhandler = uhandler;
	irq_object->data = int_data;

	if (-1 == irqnum)
	{
		VR_DEBUG_PRINT(2, ("No IRQ for core '%s' found during probe\n", description));
		kfree(irq_object);
		return NULL;
	}

	if (0 != request_irq(irqnum, irq_handler_upper_half, IRQF_SHARED, description, irq_object))
	{
		VR_DEBUG_PRINT(2, ("Unable to install IRQ handler for core '%s'\n", description));
		kfree(irq_object);
		return NULL;
	}

	return irq_object;
}

void _vr_osk_irq_term( _vr_osk_irq_t *irq )
{
	vr_osk_irq_object_t *irq_object = (vr_osk_irq_object_t *)irq;
	free_irq(irq_object->irqnum, irq_object);
	kfree(irq_object);
}


/** This function is called directly in interrupt context from the OS just after
 * the CPU get the hw-irq from vr, or other devices on the same IRQ-channel.
 * It is registered one of these function for each vr core. When an interrupt
 * arrives this function will be called equal times as registered vr cores.
 * That means that we only check one vr core in one function call, and the
 * core we check for each turn is given by the \a dev_id variable.
 * If we detect an pending interrupt on the given core, we mask the interrupt
 * out by settging the core's IRQ_MASK register to zero.
 * Then we schedule the vr_core_irq_handler_bottom_half to run as high priority
 * work queue job.
 */
static irqreturn_t irq_handler_upper_half (int port_name, void* dev_id ) /* , struct pt_regs *regs*/
{
	vr_osk_irq_object_t *irq_object = (vr_osk_irq_object_t *)dev_id;

	if (irq_object->uhandler(irq_object->data) == _VR_OSK_ERR_OK)
	{
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

