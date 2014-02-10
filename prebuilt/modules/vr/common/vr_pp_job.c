/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_pp_job.h"
#include "vr_osk.h"
#include "vr_osk_list.h"
#include "vr_kernel_common.h"
#include "vr_uk_types.h"

static u32 pp_counter_src0 = VR_HW_CORE_NO_COUNTER;      /**< Performance counter 0, VR_HW_CORE_NO_COUNTER for disabled */
static u32 pp_counter_src1 = VR_HW_CORE_NO_COUNTER;      /**< Performance counter 1, VR_HW_CORE_NO_COUNTER for disabled */

struct vr_pp_job *vr_pp_job_create(struct vr_session_data *session, _vr_uk_pp_start_job_s *uargs, u32 id)
{
	struct vr_pp_job *job;
	u32 perf_counter_flag;

	job = _vr_osk_malloc(sizeof(struct vr_pp_job));
	if (NULL != job)
	{
		u32 i;

		if (0 != _vr_osk_copy_from_user(&job->uargs, uargs, sizeof(_vr_uk_pp_start_job_s)))
		{
			_vr_osk_free(job);
			return NULL;
		}

		if (job->uargs.num_cores > _VR_PP_MAX_SUB_JOBS)
		{
			VR_PRINT_ERROR(("VR PP job: Too many sub jobs specified in job object\n"));
			_vr_osk_free(job);
			return NULL;
		}

		if (!vr_pp_job_use_no_notification(job))
		{
			job->finished_notification = _vr_osk_notification_create(_VR_NOTIFICATION_PP_FINISHED, sizeof(_vr_uk_pp_job_finished_s));
			if (NULL == job->finished_notification)
			{
				_vr_osk_free(job);
				return NULL;
			}
		}
		else
		{
			job->finished_notification = NULL;
		}

		perf_counter_flag = vr_pp_job_get_perf_counter_flag(job);

		/* case when no counters came from user space
		 * so pass the debugfs / DS-5 provided global ones to the job object */
		if (!((perf_counter_flag & _VR_PERFORMANCE_COUNTER_FLAG_SRC0_ENABLE) ||
				(perf_counter_flag & _VR_PERFORMANCE_COUNTER_FLAG_SRC1_ENABLE)))
		{
			vr_pp_job_set_perf_counter_src0(job, vr_pp_job_get_pp_counter_src0());
			vr_pp_job_set_perf_counter_src1(job, vr_pp_job_get_pp_counter_src1());
		}

		_vr_osk_list_init(&job->list);
		job->session = session;
		_vr_osk_list_init(&job->session_list);
		job->id = id;

		for (i = 0; i < job->uargs.num_cores; i++)
		{
			job->perf_counter_value0[i] = 0;
			job->perf_counter_value1[i] = 0;
		}
		job->sub_jobs_num = job->uargs.num_cores ? job->uargs.num_cores : 1;
		job->sub_jobs_started = 0;
		job->sub_jobs_completed = 0;
		job->sub_job_errors = 0;
		job->pid = _vr_osk_get_pid();
		job->tid = _vr_osk_get_tid();
#if defined(CONFIG_SYNC)
		job->sync_point = NULL;
		job->pre_fence = NULL;
		job->sync_work = NULL;
#endif

		return job;
	}

	return NULL;
}

void vr_pp_job_delete(struct vr_pp_job *job)
{
#ifdef CONFIG_SYNC
	if (NULL != job->pre_fence) sync_fence_put(job->pre_fence);
	if (NULL != job->sync_point) sync_fence_put(job->sync_point->fence);
#endif
	if (NULL != job->finished_notification)
	{
		_vr_osk_notification_delete(job->finished_notification);
	}
	_vr_osk_free(job);
}

u32 vr_pp_job_get_pp_counter_src0(void)
{
	return pp_counter_src0;
}

vr_bool vr_pp_job_set_pp_counter_src0(u32 counter)
{
	pp_counter_src0 = counter;

	return VR_TRUE;
}

u32 vr_pp_job_get_pp_counter_src1(void)
{
	return pp_counter_src1;
}

vr_bool vr_pp_job_set_pp_counter_src1(u32 counter)
{
	pp_counter_src1 = counter;

	return VR_TRUE;
}
