/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_kernel_utilization.h"
#include "vr_osk.h"
#include "vr_osk_vr.h"
#include "vr_kernel_common.h"

/* Define how often to calculate and report GPU utilization, in milliseconds */
static _vr_osk_lock_t *time_data_lock;

static _vr_osk_atomic_t num_running_cores;

static u64 period_start_time = 0;
static u64 work_start_time = 0;
static u64 accumulated_work_time = 0;

static _vr_osk_timer_t *utilization_timer = NULL;
static vr_bool timer_running = VR_FALSE;

static u32 last_utilization = 0 ;

static u32 vr_utilization_timeout = 1000;
void (*vr_utilization_callback)(unsigned int) = NULL;

static void calculate_gpu_utilization(void* arg)
{
	u64 time_now;
	u64 time_period;
	u32 leading_zeroes;
	u32 shift_val;
	u32 work_norvrzed;
	u32 period_norvrzed;
	u32 utilization;

	_vr_osk_lock_wait(time_data_lock, _VR_OSK_LOCKMODE_RW);

	if (accumulated_work_time == 0 && work_start_time == 0)
	{
		/* Don't reschedule timer, this will be started if new work arrives */
		timer_running = VR_FALSE;

		_vr_osk_lock_signal(time_data_lock, _VR_OSK_LOCKMODE_RW);

		/* No work done for this period, report zero usage */
		if (NULL != vr_utilization_callback)
		{
			vr_utilization_callback(0);
		}

		return;
	}

	time_now = _vr_osk_time_get_ns();
	time_period = time_now - period_start_time;

	/* If we are currently busy, update working period up to now */
	if (work_start_time != 0)
	{
		accumulated_work_time += (time_now - work_start_time);
		work_start_time = time_now;
	}

	/*
	 * We have two 64-bit values, a dividend and a divisor.
	 * To avoid dependencies to a 64-bit divider, we shift down the two values
	 * equally first.
	 * We shift the dividend up and possibly the divisor down, making the result X in 256.
	 */

	/* Shift the 64-bit values down so they fit inside a 32-bit integer */
	leading_zeroes = _vr_osk_clz((u32)(time_period >> 32));
	shift_val = 32 - leading_zeroes;
	work_norvrzed = (u32)(accumulated_work_time >> shift_val);
	period_norvrzed = (u32)(time_period >> shift_val);

	/*
	 * Now, we should report the usage in parts of 256
	 * this means we must shift up the dividend or down the divisor by 8
	 * (we could do a combination, but we just use one for simplicity,
	 * but the end result should be good enough anyway)
	 */
	if (period_norvrzed > 0x00FFFFFF)
	{
		/* The divisor is so big that it is safe to shift it down */
		period_norvrzed >>= 8;
	}
	else
	{
		/*
		 * The divisor is so small that we can shift up the dividend, without loosing any data.
		 * (dividend is always smaller than the divisor)
		 */
		work_norvrzed <<= 8;
	}

	utilization = work_norvrzed / period_norvrzed;

	last_utilization = utilization;

	accumulated_work_time = 0;
	period_start_time = time_now; /* starting a new period */

	_vr_osk_lock_signal(time_data_lock, _VR_OSK_LOCKMODE_RW);

	_vr_osk_timer_add(utilization_timer, _vr_osk_time_mstoticks(vr_utilization_timeout));

	if (NULL != vr_utilization_callback)
	{
		vr_utilization_callback(utilization);
	}
}

_vr_osk_errcode_t vr_utilization_init(void)
{
#if USING_GPU_UTILIZATION
	struct _vr_osk_device_data data;
	if (_VR_OSK_ERR_OK == _vr_osk_device_data_get(&data))
	{
		/* Use device specific settings (if defined) */
		if (0 != data.utilization_interval)
		{
			vr_utilization_timeout = data.utilization_interval;
		}
		if (NULL != data.utilization_handler)
		{
			vr_utilization_callback = data.utilization_handler;
		}
	}
#endif

	if (NULL != vr_utilization_callback)
	{
		VR_DEBUG_PRINT(2, ("VR GPU Utilization: Utilization handler installed with interval %u\n", vr_utilization_timeout));
	}
	else
	{
		VR_DEBUG_PRINT(2, ("VR GPU Utilization: No utilization handler installed\n"));
	}

	time_data_lock = _vr_osk_lock_init(_VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_SPINLOCK_IRQ |
	                                     _VR_OSK_LOCKFLAG_NONINTERRUPTABLE, 0, _VR_OSK_LOCK_ORDER_UTILIZATION);

	if (NULL == time_data_lock)
	{
		return _VR_OSK_ERR_FAULT;
	}

	_vr_osk_atomic_init(&num_running_cores, 0);

	utilization_timer = _vr_osk_timer_init();
	if (NULL == utilization_timer)
	{
		_vr_osk_lock_term(time_data_lock);
		return _VR_OSK_ERR_FAULT;
	}
	_vr_osk_timer_setcallback(utilization_timer, calculate_gpu_utilization, NULL);

	return _VR_OSK_ERR_OK;
}

void vr_utilization_suspend(void)
{
	if (NULL != utilization_timer)
	{
		_vr_osk_timer_del(utilization_timer);
		timer_running = VR_FALSE;
	}
}

void vr_utilization_term(void)
{
	if (NULL != utilization_timer)
	{
		_vr_osk_timer_del(utilization_timer);
		timer_running = VR_FALSE;
		_vr_osk_timer_term(utilization_timer);
		utilization_timer = NULL;
	}

	_vr_osk_atomic_term(&num_running_cores);

	_vr_osk_lock_term(time_data_lock);
}

void vr_utilization_core_start(u64 time_now)
{
	if (_vr_osk_atomic_inc_return(&num_running_cores) == 1)
	{
		/*
		 * We went from zero cores working, to one core working,
		 * we now consider the entire GPU for being busy
		 */

		_vr_osk_lock_wait(time_data_lock, _VR_OSK_LOCKMODE_RW);

		if (time_now < period_start_time)
		{
			/*
			 * This might happen if the calculate_gpu_utilization() was able
			 * to run between the sampling of time_now and us grabbing the lock above
			 */
			time_now = period_start_time;
		}

		work_start_time = time_now;
		if (timer_running != VR_TRUE)
		{
			timer_running = VR_TRUE;
			period_start_time = work_start_time; /* starting a new period */

			_vr_osk_lock_signal(time_data_lock, _VR_OSK_LOCKMODE_RW);

			_vr_osk_timer_add(utilization_timer, _vr_osk_time_mstoticks(vr_utilization_timeout));
		}
		else
		{
			_vr_osk_lock_signal(time_data_lock, _VR_OSK_LOCKMODE_RW);
		}
	}
}

void vr_utilization_core_end(u64 time_now)
{
	if (_vr_osk_atomic_dec_return(&num_running_cores) == 0)
	{
		/*
		 * No more cores are working, so accumulate the time we was busy.
		 */
		_vr_osk_lock_wait(time_data_lock, _VR_OSK_LOCKMODE_RW);

		if (time_now < work_start_time)
		{
			/*
			 * This might happen if the calculate_gpu_utilization() was able
			 * to run between the sampling of time_now and us grabbing the lock above
			 */
			time_now = work_start_time;
		}

		accumulated_work_time += (time_now - work_start_time);
		work_start_time = 0;

		_vr_osk_lock_signal(time_data_lock, _VR_OSK_LOCKMODE_RW);
	}
}

u32 _vr_ukk_utilization_gp_pp(void)
{
	return last_utilization;
}

u32 _vr_ukk_utilization_gp(void)
{
	return last_utilization;
}

u32 _vr_ukk_utilization_pp(void)
{
	return last_utilization;
}
