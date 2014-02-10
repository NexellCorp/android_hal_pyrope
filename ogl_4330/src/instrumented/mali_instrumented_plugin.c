/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_instrumented_plugin.h"
#include "mali_system.h"
#include <cinstr/mali_cinstr_common.h>
#include <cinstr/mali_cinstr_version.h>
#ifdef NEXELL_FEATURE_ENABLE
#include "vr_utgard_counters.h"
#else
#include "mali_utgard_counters.h"
#endif
#include <base/mali_profiling.h>
#include <base/mali_runtime_stdarg.h>
#include <mali_instrumented_context.h>
#include <mali_counters.h>
#include <mali_egl_instrumented_types.h>
#include <mali_gles_instrumented_types.h>
#include <mali_vg_instrumented_types.h>



static struct cinstr_driver_info cinstr_core_get_driver_info(void);
static cinstr_error_t cinstr_core_feature_enable(cinstr_clientapi_t api, cinstr_feature_t feature);
static cinstr_error_t cinstr_core_feature_disable(cinstr_clientapi_t api, cinstr_feature_t feature);
static u32 cinstr_core_get_number_of_counters(void);
static cinstr_error_t cinstr_core_get_counters(u32* ids, u32 size);
static cinstr_error_t cinstr_core_counter_enable(u32 id);
static cinstr_error_t cinstr_core_counter_disable(u32 id);

mali_bool _mali_instrumented_enabled_features[CINSTR_CLIENTAPI_COUNT] = { 0, };


#if MALI_TIMELINE_PROFILING_ENABLED
static u32 cinstr_core_profiling_start(u32 limit);
static u32 cinstr_core_profiling_get_events(cinstr_profiling_entry_t* entries, u32 count);
static u32 cinstr_core_profiling_stop(void);
static cinstr_error_t cinstr_core_profiling_clear(void);
#endif


static mali_library* plugin_library_handle = NULL;

static const cinstr_core_instance core_inst =
{
	0 |
#if MALI_TIMELINE_PROFILING_ENABLED
	CINSTR_FEATURE_TIMELINE_PROFILING | 
#endif
#if MALI_INSTRUMENTED
	CINSTR_FEATURE_FRAME_COMPLETE | CINSTR_FEATURE_COUNTERS_HW_CORE | CINSTR_FEATURE_COUNTERS_SW_ALL |
#endif
	0, /* features_available */

    cinstr_core_get_driver_info, /* cinstr_core_get_driver_info */
	cinstr_core_feature_enable, /* feature_enable */
	cinstr_core_feature_disable, /* feature_disable */

#if MALI_TIMELINE_PROFILING_ENABLED
	cinstr_core_profiling_start, /* profiling_start */
	cinstr_core_profiling_stop, /* profiling_stop */
	cinstr_core_profiling_get_events, /* profiling_get_events */
	cinstr_core_profiling_clear, /* profiling_clear */
#else
	NULL, /* profiling_start */
	NULL, /* profiling_stop */
	NULL, /* profiling_get_events */
	NULL, /* profiling_clear */
#endif

	cinstr_core_get_number_of_counters, /* get_number_of_counters */
	cinstr_core_get_counters, /* get_counters */
	cinstr_core_counter_enable, /* counter_enable */
	cinstr_core_counter_disable, /* counter_disable */

	NULL, /* cinstr_core_feature_configure_hw_counters */
	NULL, /* configure_job_dump */
};

static cinstr_plugin_instance plugin_inst =
{
	NULL, /* get_info */
	NULL, /* handle_event */
	NULL, /* term */
};

static const cinstr_core_data core_data =
{
	VR_CINSTR_VERSION_MAKE(MALI_MODULE_CINSTR_MAJOR, MALI_MODULE_CINSTR_MINOR), /* version */
	&core_inst, /* core */
	&plugin_inst, /* plugin */
};





/* ------------ helpers for the code below -------------- */

#define _MALI_INSTRUMENTED_BAD_ID 0xFFFFFFFF
#if VR_CINSTR_EGL_FIRST_COUNTER != 0
#error "VR_CINSTR_EGL_FIRST_COUNTER not equal to zero, range tests on u32 id are not complete"
#endif

static u32 convert_cinstr_id_to_native_id(u32 id)
{
	if (id == VR_CINSTR_PP_JOB_COUNT)
	{
		return CID_M200_JOB_COUNT_CORE_0;
	}
	else if (id == VR_CINSTR_GP_JOB_COUNT)
	{
		return CID_MGP2_JOB_COUNT;
	}
	else if ( id <= VR_CINSTR_EGL_LAST_COUNTER)
	{
		return id - VR_CINSTR_COUNTER_SOURCE_EGL + MALI_EGL_COUNTER_OFFSET;
	}
	else if (id >= VR_CINSTR_GLES_FIRST_COUNTER && id <= VR_CINSTR_GLES_LAST_COUNTER)
	{
		return id - VR_CINSTR_COUNTER_SOURCE_OPENGLES + MALI_GLES_COUNTER_OFFSET;
	}
	else if (id >= VR_CINSTR_VG_FIRST_COUNTER && id <= VR_CINSTR_VG_LAST_COUNTER)
	{
		return id - VR_CINSTR_COUNTER_SOURCE_OPENVG + MALI_VG_COUNTER_OFFSET;
	}
	else if (id >= VR_CINSTR_GP_FIRST_COUNTER && id <= VR_CINSTR_GP_LAST_COUNTER)
	{
		return id - VR_CINSTR_COUNTER_SOURCE_GP + MALI_MGP2_COUNTER_OFFSET;
	}
	else if (id >= VR_CINSTR_PP_FIRST_COUNTER && id <= VR_CINSTR_PP_LAST_COUNTER)
	{
		return id - VR_CINSTR_COUNTER_SOURCE_PP + MALI_M200_COUNTER_OFFSET;
	}
	else
	{
		return _MALI_INSTRUMENTED_BAD_ID;
	}
}

static u32 convert_native_id_to_cinstr_id(u32 id)
{
	if (id >= CID_M200_JOB_COUNT_CORE_0 && id <= CID_M200_JOB_COUNT_CORE_7)
	{
		return VR_CINSTR_PP_JOB_COUNT;
	}
	else if (id == CID_MGP2_JOB_COUNT)
	{
		return VR_CINSTR_GP_JOB_COUNT;
	}
	else if (id >= INST_EGL_BLIT_TIME && id <= INST_EGL_BLIT_TIME)
	{
		return id - MALI_EGL_COUNTER_OFFSET + VR_CINSTR_COUNTER_SOURCE_EGL;
	}
	else if (id >= INST_GL_DRAW_ELEMENTS_CALLS && id <= INST_GL_POINTS_COUNT)
	{
		return id - MALI_GLES_COUNTER_OFFSET + VR_CINSTR_COUNTER_SOURCE_OPENGLES;
	}
	else if (id >= VG_MASK_COUNTER && id <= VG_RSW_COUNTER)
	{
		return id - MALI_VG_COUNTER_OFFSET + VR_CINSTR_COUNTER_SOURCE_OPENVG;
	}
	else if (id >= CID_MGP2_CYCLE_COUNTER_DEPRECATED && id <= CID_MGP2_ACTIVE_CYCLES_PLBU_TILE_ITERATOR)
	{
		return id - MALI_MGP2_COUNTER_OFFSET + VR_CINSTR_COUNTER_SOURCE_GP;
	}
	else if (id >= CID_M200_ACTIVE_CLOCK_CYCLES_COUNT && id <= CID_M200_PROGRAM_CACHE_MISS_COUNT)
	{
		return id - MALI_M200_COUNTER_OFFSET + VR_CINSTR_COUNTER_SOURCE_PP;
	}
	else
	{
		return _MALI_INSTRUMENTED_BAD_ID;
	}
}









/* ------------- this is what the mali driver calls ---------------- */

MALI_EXPORT void _mali_instrumented_plugin_load(void)
{
#ifdef MALI_INSTRUMENTED_PLUGIN
	plugin_library_handle = _mali_sys_library_load(MALI_STRINGIZE_MACRO(MALI_INSTRUMENTED_PLUGIN));

	if (NULL != plugin_library_handle)
	{
		u32 retval;
		if (MALI_TRUE == _mali_sys_library_init(plugin_library_handle, (void*)&core_data, &retval))
		{
			if ((cinstr_error_t)retval == CINSTR_ERROR_SUCCESS)
			{
				return; /* success */
			}
		}

		/* failed to init plugin, do cleanup */
		_mali_sys_library_unload(plugin_library_handle);
		plugin_library_handle = NULL;
	}
#endif
}

MALI_EXPORT void _mali_instrumented_plugin_unload(void)
{
#ifdef MALI_INSTRUMENTED_PLUGIN
	if (NULL != plugin_library_handle)
	{
		if (NULL != plugin_inst.term)
		{
			(void)plugin_inst.term();
		}
		_mali_sys_library_unload(plugin_library_handle);
		plugin_library_handle = NULL;
	}
#endif
}


MALI_EXPORT cinstr_error_t _mali_instrumented_plugin_feature_enable(cinstr_clientapi_t api, cinstr_feature_t feature)
{
	if (api == CINSTR_CLIENTAPI_ALL)
	{
		/* special case, where we should enable this for all supported APIs */
		int i;
		for (i = 0; i < CINSTR_CLIENTAPI_COUNT; i++)
		{
			_mali_instrumented_enabled_features[i] |= feature;
		}
		return CINSTR_ERROR_SUCCESS;
	}
	else if (api > 0 && api < CINSTR_CLIENTAPI_COUNT)
	{
		_mali_instrumented_enabled_features[api] |= feature;
		return CINSTR_ERROR_SUCCESS;
	}

	return CINSTR_ERROR_UNKNOWN;
}


static void send_event(struct cinstr_event_data *evt, ...)
{
#ifdef MALI_INSTRUMENTED_PLUGIN
	if (NULL != plugin_library_handle)
	{
		if (NULL != plugin_inst.handle_event)
		{
			va_list vp;
			va_start(vp, evt);
			(void)plugin_inst.handle_event(evt, vp);
			va_end(vp);
		}
	}
#endif
}


MALI_EXPORT void _mali_instrumented_plugin_send_event_frame_complete(cinstr_event_t type, u32 frame_number)
{
	struct cinstr_event_data evt;

	evt.type = type;
	evt.payload.frame_complete.frame_number = frame_number;

	send_event(&evt);
}

MALI_EXPORT void _mali_instrumented_plugin_send_event_render_pass_complete(cinstr_event_t type, u32 render_pass_number, u32 frame_number)
{
	struct cinstr_event_data evt;

	evt.type = type;
	evt.payload.render_pass_complete.render_pass_number = render_pass_number;
	evt.payload.render_pass_complete.frame_number = frame_number;

	send_event(&evt);
}



MALI_EXPORT void _mali_instrumented_plugin_send_event_counters(cinstr_counter_source source, u32 count, struct mali_counter* counters, s64* values)
{
	struct cinstr_event_data evt;
	u32 i;
	u32* counters_cinstr;

	if (source == VR_CINSTR_COUNTER_SOURCE_GP || source == VR_CINSTR_COUNTER_SOURCE_PP)
	{
		evt.type = CINSTR_EVENT_COUNTERS_HW;
	}
	else
	{
		evt.type = CINSTR_EVENT_COUNTERS_SW;
	}

	counters_cinstr = _mali_sys_malloc(sizeof(u32) * count);
	if (NULL == counters_cinstr)
	{
		return;
	}

	for (i = 0; i < count; i++)
	{
		counters_cinstr[i] = convert_native_id_to_cinstr_id(counters[i].info->id);
	}

	evt.payload.perfcount.source = source;
	evt.payload.perfcount.count = count;
	evt.payload.perfcount.counters = counters_cinstr;
	evt.payload.perfcount.values = values;

	send_event(&evt);

	_mali_sys_free(counters_cinstr);
}





/* ------------ this is what the plugin calls ----------- */

static struct cinstr_driver_info cinstr_core_get_driver_info(void)
{
	struct cinstr_driver_info retval;

	retval.sw_version = 0; /* Encoding not specified yet */

#if defined(USING_MALI200)
	retval.hw_model = CINSTR_MODEL_MALI_200;
	retval.hw_version = ((MALI200_HWVER >> 8) << 16) | (MALI200_HWVER & 0xFF);
#elif defined(USING_MALI400)
	retval.hw_model = CINSTR_MODEL_MALI_400MP;
	retval.hw_version = ((MALI400_HWVER >> 8) << 16) | (MALI400_HWVER & 0xFF);
#elif defined(USING_MALI450)
	retval.hw_model = CINSTR_MODEL_MALI_450MP;
	retval.hw_version = ((MALI450_HWVER >> 8) << 16) | (MALI450_HWVER & 0xFF);
#else
#error "Unknown Mali GPU"
#endif

	return retval;
}



static cinstr_error_t cinstr_core_feature_enable(cinstr_clientapi_t api, cinstr_feature_t feature)
{
	if (api == CINSTR_CLIENTAPI_ALL)
	{
		/* special case, where we should enable this for all supported APIs */
		int i;
		for (i = 0; i < CINSTR_CLIENTAPI_COUNT; i++)
		{
			_mali_instrumented_enabled_features[i] |= feature;
		}
		return CINSTR_ERROR_SUCCESS;
	}
	else if (api > 0 && api < CINSTR_CLIENTAPI_COUNT)
	{
		_mali_instrumented_enabled_features[api] |= feature;
		return CINSTR_ERROR_SUCCESS;
	}

	return CINSTR_ERROR_UNKNOWN;
}

static cinstr_error_t cinstr_core_feature_disable(cinstr_clientapi_t api, cinstr_feature_t feature)
{
	if (api > 0 && api < CINSTR_CLIENTAPI_COUNT)
	{
		_mali_instrumented_enabled_features[api] &= ~feature;
		return CINSTR_ERROR_SUCCESS;
	}

	return CINSTR_ERROR_UNKNOWN;
}

static u32 cinstr_core_get_number_of_counters(void)
{
	mali_instrumented_context *ictx = _instrumented_get_context();

	if (NULL == ictx)
	{
		return 0;
	}

	return ictx->egl_num_counters + ictx->gles_num_counters + ictx->vg_num_counters + ictx->gp_num_counters + ictx->pp_num_counters[0];
}

static cinstr_error_t cinstr_core_get_counters(u32* ids, u32 size)
{
	mali_instrumented_context *ictx = _instrumented_get_context();
	u32 ii;     /* input index */
	u32 oi = 0; /* output index */

	if (NULL == ids || cinstr_core_get_number_of_counters() != size || NULL == ictx)
	{
		return CINSTR_ERROR_UNKNOWN;
	}

	for (ii = 0; ii < ictx->egl_num_counters; ii++)
	{
		ids[oi] = convert_native_id_to_cinstr_id(ictx->counters[ictx->egl_start_index + ii].info->id);
		oi++;
	}

	for (ii = 0; ii < ictx->gles_num_counters; ii++)
	{
		ids[oi] = convert_native_id_to_cinstr_id(ictx->counters[ictx->gles_start_index + ii].info->id);
		oi++;
	}

	for (ii = 0; ii < ictx->vg_num_counters; ii++)
	{
		ids[oi] = convert_native_id_to_cinstr_id(ictx->counters[ictx->vg_start_index + ii].info->id);
		oi++;
	}

	for (ii = 0; ii < ictx->gp_num_counters; ii++)
	{
		ids[oi] = convert_native_id_to_cinstr_id(ictx->counters[ictx->gp_start_index + ii].info->id);
		oi++;
	}

	for (ii = 0; ii < ictx->pp_num_counters[0]; ii++)
	{
		ids[oi] = convert_native_id_to_cinstr_id(ictx->counters[ictx->pp_start_index[0] + ii].info->id);
		oi++;
	}

	return CINSTR_ERROR_SUCCESS;
}

static cinstr_error_t cinstr_core_counter_change_native(u32 native_id, mali_bool enable)
{
	mali_instrumented_context *ictx = _instrumented_get_context();
	void* index_ptr;
	unsigned int index;

	index_ptr = __mali_named_list_get(ictx->counter_map, native_id);
	if (NULL == index_ptr)
	{
		return CINSTR_ERROR_UNKNOWN;
	}
	
	index = (MALI_REINTERPRET_CAST(u32)index_ptr) - 1;

	if (MALI_TRUE == enable)
	{
		if (_instrumented_activate_counters(ictx, &index, 1) != MALI_ERR_NO_ERROR)
		{
			return CINSTR_ERROR_UNKNOWN;
		}
	}
	else
	{
		if (_instrumented_deactivate_counters(ictx, &index, 1) != MALI_ERR_NO_ERROR)
		{
			return CINSTR_ERROR_UNKNOWN;
		}
	}

	return CINSTR_ERROR_SUCCESS;
}


static cinstr_error_t cinstr_core_counter_change(u32 id, mali_bool enable)
{
	u32 native_id;

	native_id = convert_cinstr_id_to_native_id(id);
	if (native_id == _MALI_INSTRUMENTED_BAD_ID)
	{
		return CINSTR_ERROR_UNKNOWN;
	}

	if (id >= VR_CINSTR_PP_FIRST_COUNTER && id <= VR_CINSTR_PP_LAST_COUNTER)
	{
		/*
		 * Each PP counters are internally registered as up to 4 different counters with 4 different ID,
		 * thus we need to enable this PP counter for all jobs (job is also refered to as core in some contexts).
		 * It doesn't make much sense to have different PP counters enabled for different PP jobs anyway.
		 */
		int i;
		for (i = 0; i < MALI_MAX_PP_SPLIT_COUNT; i++)
		{
			u32 cur_native_id;
			cinstr_error_t ret;

			if (id == VR_CINSTR_PP_JOB_COUNT)
			{
				cur_native_id = native_id + i; /* These counter not a part of the HW counters, so handled differently */
			}
			else
			{
				cur_native_id = native_id + (i * M200_MULTI_CORE_OFFSET);
			}
			ret = cinstr_core_counter_change_native(cur_native_id, enable);
			if (CINSTR_ERROR_SUCCESS != ret)
			{
				return ret;
			}
		}

		return CINSTR_ERROR_SUCCESS;
	}
	else
	{
		return cinstr_core_counter_change_native(native_id, enable);
	}
}

static cinstr_error_t cinstr_core_counter_enable(u32 id)
{
	return cinstr_core_counter_change(id, MALI_TRUE);
}

static cinstr_error_t cinstr_core_counter_disable(u32 id)
{
	return cinstr_core_counter_change(id, MALI_FALSE);
}



#if MALI_TIMELINE_PROFILING_ENABLED

static u32 cinstr_core_profiling_start(u32 limit)
{
	return _mali_base_profiling_start(limit);
}

static u32 cinstr_core_profiling_stop(void)
{
	return _mali_base_profiling_stop();
}

static u32 cinstr_core_profiling_get_events(cinstr_profiling_entry_t* entries, u32 count)
{
	u32 i;
	
	for (i = 0; i < count; i++)
	{
		if (_mali_base_profiling_get_event(i, entries + i) != MALI_TRUE)
		{
			break;
		}
	}

	return i; /* return the count actually retrieved */
}

static cinstr_error_t cinstr_core_profiling_clear(void)
{
	_mali_base_profiling_clear();
	return CINSTR_ERROR_SUCCESS;
}

#endif
