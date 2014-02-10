/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <linux/module.h>

#include "vr_kernel_common.h"
#include "vr_osk.h"
#include "vr_ukk.h"
#include "vr_uk_types.h"
#include "vr_osk_profiling.h"
#include "vr_linux_trace.h"
#include "vr_gp.h"
#include "vr_pp.h"
#include "vr_l2_cache.h"
#include "vr_user_settings_db.h"

_vr_osk_errcode_t _vr_osk_profiling_init(vr_bool auto_start)
{
	if (VR_TRUE == auto_start)
	{
		vr_set_user_setting(_VR_UK_USER_SETTING_SW_EVENTS_ENABLE, VR_TRUE);
	}

	return _VR_OSK_ERR_OK;
}

void _vr_osk_profiling_term(void)
{
	/* Nothing to do */
}

_vr_osk_errcode_t _vr_osk_profiling_start(u32 * limit)
{
	/* Nothing to do */
	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t _vr_osk_profiling_stop(u32 *count)
{
	/* Nothing to do */
	return _VR_OSK_ERR_OK;
}

u32 _vr_osk_profiling_get_count(void)
{
	return 0;
}

_vr_osk_errcode_t _vr_osk_profiling_get_event(u32 index, u64* timestamp, u32* event_id, u32 data[5])
{
	/* Nothing to do */
	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t _vr_osk_profiling_clear(void)
{
	/* Nothing to do */
	return _VR_OSK_ERR_OK;
}

vr_bool _vr_osk_profiling_is_recording(void)
{
	return VR_FALSE;
}

vr_bool _vr_osk_profiling_have_recording(void)
{
	return VR_FALSE;
}

void _vr_osk_profiling_report_sw_counters(u32 *counters)
{
	trace_vr_sw_counters(_vr_osk_get_pid(), _vr_osk_get_tid(), NULL, counters);
}


_vr_osk_errcode_t _vr_ukk_profiling_start(_vr_uk_profiling_start_s *args)
{
	return _vr_osk_profiling_start(&args->limit);
}

_vr_osk_errcode_t _vr_ukk_profiling_add_event(_vr_uk_profiling_add_event_s *args)
{
	/* Always add process and thread identificator in the first two data elements for events from user space */
	_vr_osk_profiling_add_event(args->event_id, _vr_osk_get_pid(), _vr_osk_get_tid(), args->data[2], args->data[3], args->data[4]);

	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t _vr_ukk_profiling_stop(_vr_uk_profiling_stop_s *args)
{
	return _vr_osk_profiling_stop(&args->count);
}

_vr_osk_errcode_t _vr_ukk_profiling_get_event(_vr_uk_profiling_get_event_s *args)
{
	return _vr_osk_profiling_get_event(args->index, &args->timestamp, &args->event_id, args->data);
}

_vr_osk_errcode_t _vr_ukk_profiling_clear(_vr_uk_profiling_clear_s *args)
{
	return _vr_osk_profiling_clear();
}

_vr_osk_errcode_t _vr_ukk_sw_counters_report(_vr_uk_sw_counters_report_s *args)
{
	_vr_osk_profiling_report_sw_counters(args->counters);
	return _VR_OSK_ERR_OK;
}

/**
 * Called by gator.ko to set HW counters
 *
 * @param counter_id The counter ID.
 * @param event_id Event ID that the counter should count (HW counter value from TRM).
 * 
 * @return 1 on success, 0 on failure.
 */
int _vr_profiling_set_event(u32 counter_id, s32 event_id)
{
	if (COUNTER_VP_C0 == counter_id)
	{
		if (VR_TRUE == vr_gp_job_set_gp_counter_src0(event_id))
		{
			return 1;
		}
	}
	if (COUNTER_VP_C1 == counter_id)
	{
		if (VR_TRUE == vr_gp_job_set_gp_counter_src1(event_id))
		{
			return 1;
		}
	}
	if (COUNTER_FP0_C0 <= counter_id && COUNTER_FP3_C1 >= counter_id)
	{
		u32 core_id = (counter_id - COUNTER_FP0_C0) >> 1;
		struct vr_pp_core* pp_core = vr_pp_get_global_pp_core(core_id);

		if (NULL != pp_core)
		{
			if ((COUNTER_FP0_C0 == counter_id) || (COUNTER_FP0_C1 == counter_id))
			{
				u32 counter_src = (counter_id - COUNTER_FP0_C0) & 1;
				if (0 == counter_src)
				{
					if (VR_TRUE == vr_pp_job_set_pp_counter_src0(event_id))
					{
						return 1;
					}
				}
				else
				{
					if (VR_TRUE == vr_pp_job_set_pp_counter_src1(event_id))
					{
					VR_DEBUG_PRINT(5, ("VR PROFILING SET EVENT core 0 counter_id = %d\n",counter_id));
					return 1;
					}
				}
			}
		}
	}
	if (COUNTER_L2_C0 <= counter_id && COUNTER_L2_C1 >= counter_id)
	{
		u32 core_id = (counter_id - COUNTER_L2_C0) >> 1;
		struct vr_l2_cache_core* l2_cache_core = vr_l2_cache_core_get_glob_l2_core(core_id);

		if (NULL != l2_cache_core)
		{
			u32 counter_src = (counter_id - COUNTER_L2_C0) & 1;
			if (0 == counter_src)
			{
				VR_DEBUG_PRINT(5, ("SET EVENT L2 0 COUNTER\n"));
				if (VR_TRUE == vr_l2_cache_core_set_counter_src0(l2_cache_core, event_id))
				{
					return 1;
				}
			}
			else
			{
				VR_DEBUG_PRINT(5, ("SET EVENT L2 1 COUNTER\n"));
				if (VR_TRUE == vr_l2_cache_core_set_counter_src1(l2_cache_core, event_id))
				{
					return 1;
				}
			}
		}
	}

	return 0;
}

/**
 * Called by gator.ko to retrieve the L2 cache counter values for the first L2 cache. 
 * The L2 cache counters are unique in that they are polled by gator, rather than being
 * transmitted via the tracepoint mechanism. 
 *
 * @param src0 First L2 cache counter ID.
 * @param val0 First L2 cache counter value.
 * @param src1 Second L2 cache counter ID.
 * @param val1 Second L2 cache counter value.
 */
void _vr_profiling_get_counters(u32 *src0, u32 *val0, u32 *src1, u32 *val1)
{
	 struct vr_l2_cache_core *l2_cache = vr_l2_cache_core_get_glob_l2_core(0);
	 if (NULL != l2_cache)
	 {
		if (VR_TRUE == vr_l2_cache_lock_power_state(l2_cache))
		{
			/* It is now safe to access the L2 cache core in order to retrieve the counters */
			vr_l2_cache_core_get_counter_values(l2_cache, src0, val0, src1, val1);
		}
		vr_l2_cache_unlock_power_state(l2_cache);
	 }
}

/*
 * List of possible actions to be controlled by Streamline.
 * The following numbers are used by gator to control the frame buffer dumping and s/w counter reporting.
 * We cannot use the enums in vr_uk_types.h because they are unknown inside gator.
 */
#define FBDUMP_CONTROL_ENABLE (1)
#define FBDUMP_CONTROL_RATE (2)
#define SW_COUNTER_ENABLE (3)
#define FBDUMP_CONTROL_RESIZE_FACTOR (4)

/**
 * Called by gator to control the production of profiling information at runtime.
 */
void _vr_profiling_control(u32 action, u32 value)
{
	switch(action)
	{
	case FBDUMP_CONTROL_ENABLE:
		vr_set_user_setting(_VR_UK_USER_SETTING_COLORBUFFER_CAPTURE_ENABLED, (value == 0 ? VR_FALSE : VR_TRUE));
		break;
	case FBDUMP_CONTROL_RATE:
		vr_set_user_setting(_VR_UK_USER_SETTING_BUFFER_CAPTURE_N_FRAMES, value);
		break;
	case SW_COUNTER_ENABLE:
		vr_set_user_setting(_VR_UK_USER_SETTING_SW_COUNTER_ENABLED, value);
		break;
	case FBDUMP_CONTROL_RESIZE_FACTOR:
		vr_set_user_setting(_VR_UK_USER_SETTING_BUFFER_CAPTURE_RESIZE_FACTOR, value);
		break;
	default:
		break;	/* Ignore unimplemented actions */
	}
}

EXPORT_SYMBOL(_vr_profiling_set_event);
EXPORT_SYMBOL(_vr_profiling_get_counters);
EXPORT_SYMBOL(_vr_profiling_control);
