/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_gp_scheduler.h"
#include "vr_kernel_common.h"
#include "vr_osk.h"
#include "vr_osk_list.h"
#include "vr_scheduler.h"
#include "vr_gp.h"
#include "vr_gp_job.h"
#include "vr_group.h"
#include "vr_pm.h"

enum vr_gp_slot_state
{
	VR_GP_SLOT_STATE_IDLE,
	VR_GP_SLOT_STATE_WORKING,
};

/* A render slot is an entity which jobs can be scheduled onto */
struct vr_gp_slot
{
	struct vr_group *group;
	/*
	 * We keep track of the state here as well as in the group object
	 * so we don't need to take the group lock so often (and also avoid clutter with the working lock)
	 */
	enum vr_gp_slot_state state;
	u32 returned_cookie;
};

static u32 gp_version = 0;
static _VR_OSK_LIST_HEAD(job_queue);                          /* List of jobs with some unscheduled work */
static struct vr_gp_slot slot;

/* Variables to allow safe pausing of the scheduler */
static _vr_osk_wait_queue_t *gp_scheduler_working_wait_queue = NULL;
static u32 pause_count = 0;

static vr_bool vr_gp_scheduler_is_suspended(void);

static _vr_osk_lock_t *gp_scheduler_lock = NULL;
/* Contains tid of thread that locked the scheduler or 0, if not locked */

_vr_osk_errcode_t vr_gp_scheduler_initialize(void)
{
	u32 num_groups;
	u32 i;

	_VR_OSK_INIT_LIST_HEAD(&job_queue);

	gp_scheduler_lock = _vr_osk_lock_init(_VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_SPINLOCK | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE, 0, _VR_OSK_LOCK_ORDER_SCHEDULER);
	if (NULL == gp_scheduler_lock)
	{
		return _VR_OSK_ERR_NOMEM;
	}

	gp_scheduler_working_wait_queue = _vr_osk_wait_queue_init();
	if (NULL == gp_scheduler_working_wait_queue)
	{
		_vr_osk_lock_term(gp_scheduler_lock);
		return _VR_OSK_ERR_NOMEM;
	}

	/* Find all the available GP cores */
	num_groups = vr_group_get_glob_num_groups();
	for (i = 0; i < num_groups; i++)
	{
		struct vr_group *group = vr_group_get_glob_group(i);

		struct vr_gp_core *gp_core = vr_group_get_gp_core(group);
		if (NULL != gp_core)
		{
			if (0 == gp_version)
			{
				/* Retrieve GP version */
				gp_version = vr_gp_core_get_version(gp_core);
			}
			slot.group = group;
			slot.state = VR_GP_SLOT_STATE_IDLE;
			break; /* There is only one GP, no point in looking for more */
		}
	}

	return _VR_OSK_ERR_OK;
}

void vr_gp_scheduler_terminate(void)
{
	VR_DEBUG_ASSERT(VR_GP_SLOT_STATE_IDLE == slot.state);
	VR_DEBUG_ASSERT_POINTER(slot.group);
	vr_group_delete(slot.group);

	_vr_osk_wait_queue_term(gp_scheduler_working_wait_queue);
	_vr_osk_lock_term(gp_scheduler_lock);
}

VR_STATIC_INLINE void vr_gp_scheduler_lock(void)
{
	if(_VR_OSK_ERR_OK != _vr_osk_lock_wait(gp_scheduler_lock, _VR_OSK_LOCKMODE_RW))
	{
		/* Non-interruptable lock failed: this should never happen. */
		VR_DEBUG_ASSERT(0);
	}
	VR_DEBUG_PRINT(5, ("VR GP scheduler: GP scheduler lock taken\n"));
}

VR_STATIC_INLINE void vr_gp_scheduler_unlock(void)
{
	VR_DEBUG_PRINT(5, ("VR GP scheduler: Releasing GP scheduler lock\n"));
	_vr_osk_lock_signal(gp_scheduler_lock, _VR_OSK_LOCKMODE_RW);
}

#ifdef DEBUG
VR_STATIC_INLINE void vr_gp_scheduler_assert_locked(void)
{
	VR_DEBUG_ASSERT_LOCK_HELD(gp_scheduler_lock);
}
#define VR_ASSERT_GP_SCHEDULER_LOCKED() vr_gp_scheduler_assert_locked()
#else
#define VR_ASSERT_GP_SCHEDULER_LOCKED()
#endif

static void vr_gp_scheduler_schedule(void)
{
	struct vr_gp_job *job;

	vr_gp_scheduler_lock();

	if (0 < pause_count || VR_GP_SLOT_STATE_IDLE != slot.state || _vr_osk_list_empty(&job_queue))
	{
		VR_DEBUG_PRINT(4, ("VR GP scheduler: Nothing to schedule (paused=%u, idle slots=%u)\n",
		                     pause_count, VR_GP_SLOT_STATE_IDLE == slot.state ? 1 : 0));
		vr_gp_scheduler_unlock();
		return; /* Nothing to do, so early out */
	}

	/* Get (and remove) next job in queue */
	job = _VR_OSK_LIST_ENTRY(job_queue.next, struct vr_gp_job, list);
	_vr_osk_list_del(&job->list);

	/* Mark slot as busy */
	slot.state = VR_GP_SLOT_STATE_WORKING;

	vr_gp_scheduler_unlock();

	VR_DEBUG_PRINT(3, ("VR GP scheduler: Starting job %u (0x%08X)\n", vr_gp_job_get_id(job), job));

	vr_group_lock(slot.group);

	if (_VR_OSK_ERR_OK != vr_group_start_gp_job(slot.group, job))
	{
		VR_DEBUG_PRINT(3, ("VR GP scheduler: Failed to start GP job\n"));
		VR_DEBUG_ASSERT(0); /* @@@@ todo: this cant fail on VR-300+, no need to implement put back of job */
	}

	vr_group_unlock(slot.group);
}

/* @@@@ todo: pass the job in as a param to this function, so that we don't have to take the scheduler lock again */
static void vr_gp_scheduler_schedule_on_group(struct vr_group *group)
{
	struct vr_gp_job *job;

	VR_DEBUG_ASSERT_LOCK_HELD(group->lock);
	VR_DEBUG_ASSERT_LOCK_HELD(gp_scheduler_lock);

	if (0 < pause_count || VR_GP_SLOT_STATE_IDLE != slot.state || _vr_osk_list_empty(&job_queue))
	{
		vr_gp_scheduler_unlock();
		VR_DEBUG_PRINT(4, ("VR GP scheduler: Nothing to schedule (paused=%u, idle slots=%u)\n",
		                     pause_count, VR_GP_SLOT_STATE_IDLE == slot.state ? 1 : 0));
		return; /* Nothing to do, so early out */
	}

	/* Get (and remove) next job in queue */
	job = _VR_OSK_LIST_ENTRY(job_queue.next, struct vr_gp_job, list);
	_vr_osk_list_del(&job->list);

	/* Mark slot as busy */
	slot.state = VR_GP_SLOT_STATE_WORKING;

	vr_gp_scheduler_unlock();

	VR_DEBUG_PRINT(3, ("VR GP scheduler: Starting job %u (0x%08X)\n", vr_gp_job_get_id(job), job));

	if (_VR_OSK_ERR_OK != vr_group_start_gp_job(slot.group, job))
	{
		VR_DEBUG_PRINT(3, ("VR GP scheduler: Failed to start GP job\n"));
		VR_DEBUG_ASSERT(0); /* @@@@ todo: this cant fail on VR-300+, no need to implement put back of job */
	}
}

static void vr_gp_scheduler_return_job_to_user(struct vr_gp_job *job, vr_bool success)
{
	_vr_uk_gp_job_finished_s *jobres = job->finished_notification->result_buffer;
	_vr_osk_memset(jobres, 0, sizeof(_vr_uk_gp_job_finished_s)); /* @@@@ can be removed once we initialize all members in this struct */
	jobres->user_job_ptr = vr_gp_job_get_user_id(job);
	if (VR_TRUE == success)
	{
		jobres->status = _VR_UK_JOB_STATUS_END_SUCCESS;
	}
	else
	{
		jobres->status = _VR_UK_JOB_STATUS_END_UNKNOWN_ERR;
	}

	jobres->heap_current_addr = vr_gp_job_get_current_heap_addr(job);
	jobres->perf_counter0 = vr_gp_job_get_perf_counter_value0(job);
	jobres->perf_counter1 = vr_gp_job_get_perf_counter_value1(job);

	vr_session_send_notification(vr_gp_job_get_session(job), job->finished_notification);
	job->finished_notification = NULL;

	vr_gp_job_delete(job);
}

void vr_gp_scheduler_job_done(struct vr_group *group, struct vr_gp_job *job, vr_bool success)
{
	VR_DEBUG_PRINT(3, ("VR GP scheduler: Job %u (0x%08X) completed (%s)\n", vr_gp_job_get_id(job), job, success ? "success" : "failure"));

	vr_gp_scheduler_return_job_to_user(job, success);

	vr_gp_scheduler_lock();

	/* Mark slot as idle again */
	slot.state = VR_GP_SLOT_STATE_IDLE;

	/* If paused, then this was the last job, so wake up sleeping workers */
	if (pause_count > 0)
	{
		_vr_osk_wait_queue_wake_up(gp_scheduler_working_wait_queue);
	}

	vr_gp_scheduler_schedule_on_group(group);

	/* It is ok to do this after schedule, since START/STOP is simply ++ and -- anyways */
	vr_pm_core_event(VR_CORE_EVENT_GP_STOP);
}

void vr_gp_scheduler_oom(struct vr_group *group, struct vr_gp_job *job)
{
	_vr_uk_gp_job_suspended_s * jobres;
	_vr_osk_notification_t * notification;

	vr_gp_scheduler_lock();

	notification = job->oom_notification;
	job->oom_notification = NULL;
	slot.returned_cookie = vr_gp_job_get_id(job);

	jobres = (_vr_uk_gp_job_suspended_s *)notification->result_buffer;
	jobres->user_job_ptr = vr_gp_job_get_user_id(job);
	jobres->cookie = vr_gp_job_get_id(job);

	vr_gp_scheduler_unlock();

	jobres->reason = _VRGP_JOB_SUSPENDED_OUT_OF_MEMORY;

	vr_session_send_notification(vr_gp_job_get_session(job), notification);

	/*
	* If this function failed, then we could return the job to user space right away,
	* but there is a job timer anyway that will do that eventually.
	* This is not exactly a common case anyway.
	*/
}

void vr_gp_scheduler_suspend(void)
{
	vr_gp_scheduler_lock();
	pause_count++; /* Increment the pause_count so that no more jobs will be scheduled */
	vr_gp_scheduler_unlock();

	_vr_osk_wait_queue_wait_event(gp_scheduler_working_wait_queue, vr_gp_scheduler_is_suspended);
}

void vr_gp_scheduler_resume(void)
{
	vr_gp_scheduler_lock();
	pause_count--; /* Decrement pause_count to allow scheduling again (if it reaches 0) */
	vr_gp_scheduler_unlock();
	if (0 == pause_count)
	{
		vr_gp_scheduler_schedule();
	}
}

_vr_osk_errcode_t _vr_ukk_gp_start_job(void *ctx, _vr_uk_gp_start_job_s *uargs)
{
	struct vr_session_data *session;
	struct vr_gp_job *job;

	VR_DEBUG_ASSERT_POINTER(uargs);
	VR_DEBUG_ASSERT_POINTER(ctx);

	session = (struct vr_session_data*)ctx;

	job = vr_gp_job_create(session, uargs, vr_scheduler_get_new_id());
	if (NULL == job)
	{
		return _VR_OSK_ERR_NOMEM;
	}

#if PROFILING_SKIP_PP_AND_GP_JOBS
#warning GP jobs will not be executed
	vr_gp_scheduler_return_job_to_user(job, VR_TRUE);
	return _VR_OSK_ERR_OK;
#endif

	vr_pm_core_event(VR_CORE_EVENT_GP_START);

	vr_gp_scheduler_lock();
	_vr_osk_list_addtail(&job->list, &job_queue);
	vr_gp_scheduler_unlock();

	VR_DEBUG_PRINT(3, ("VR GP scheduler: Job %u (0x%08X) queued\n", vr_gp_job_get_id(job), job));

	vr_gp_scheduler_schedule();

	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t _vr_ukk_get_gp_number_of_cores(_vr_uk_get_gp_number_of_cores_s *args)
{
	VR_DEBUG_ASSERT_POINTER(args);
	VR_CHECK_NON_NULL(args->ctx, _VR_OSK_ERR_INVALID_ARGS);
	args->number_of_cores = 1;
	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t _vr_ukk_get_gp_core_version(_vr_uk_get_gp_core_version_s *args)
{
	VR_DEBUG_ASSERT_POINTER(args);
	VR_CHECK_NON_NULL(args->ctx, _VR_OSK_ERR_INVALID_ARGS);
	args->version = gp_version;
	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t _vr_ukk_gp_suspend_response(_vr_uk_gp_suspend_response_s *args)
{
	struct vr_session_data *session;
	struct vr_gp_job *resumed_job;
	_vr_osk_notification_t *new_notification = 0;

	VR_DEBUG_ASSERT_POINTER(args);

	if (NULL == args->ctx)
	{
		return _VR_OSK_ERR_INVALID_ARGS;
	}

	session = (struct vr_session_data*)args->ctx;
	if (NULL == session)
	{
		return _VR_OSK_ERR_FAULT;
	}

	if (_VRGP_JOB_RESUME_WITH_NEW_HEAP == args->code)
	{
		new_notification = _vr_osk_notification_create(_VR_NOTIFICATION_GP_STALLED, sizeof(_vr_uk_gp_job_suspended_s));

		if (NULL == new_notification)
		{
			VR_PRINT_ERROR(("VR GP scheduler: Failed to allocate notification object. Will abort GP job.\n"));
			vr_group_lock(slot.group);
			vr_group_abort_gp_job(slot.group, args->cookie);
			vr_group_unlock(slot.group);
			return _VR_OSK_ERR_FAULT;
		}
	}

	vr_group_lock(slot.group);

	if (_VRGP_JOB_RESUME_WITH_NEW_HEAP == args->code)
	{
		VR_DEBUG_PRINT(3, ("VR GP scheduler: Resuming job %u with new heap; 0x%08X - 0x%08X\n", args->cookie, args->arguments[0], args->arguments[1]));

		resumed_job = vr_group_resume_gp_with_new_heap(slot.group, args->cookie, args->arguments[0], args->arguments[1]);
		if (NULL != resumed_job)
		{
			/* @@@@ todo: move this and other notification handling into the job object itself */
			resumed_job->oom_notification = new_notification;
			vr_group_unlock(slot.group);
			return _VR_OSK_ERR_OK;
		}
		else
		{
			vr_group_unlock(slot.group);
			_vr_osk_notification_delete(new_notification);
			return _VR_OSK_ERR_FAULT;
		}
	}

	VR_DEBUG_PRINT(3, ("VR GP scheduler: Aborting job %u, no new heap provided\n", args->cookie));
	vr_group_abort_gp_job(slot.group, args->cookie);
	vr_group_unlock(slot.group);
	return _VR_OSK_ERR_OK;
}

void vr_gp_scheduler_abort_session(struct vr_session_data *session)
{
	struct vr_gp_job *job, *tmp;

	vr_gp_scheduler_lock();
	VR_DEBUG_PRINT(3, ("VR GP scheduler: Aborting all jobs from session 0x%08x\n", session));

	/* Check queue for jobs and remove */
	_VR_OSK_LIST_FOREACHENTRY(job, tmp, &job_queue, struct vr_gp_job, list)
	{
		if (vr_gp_job_get_session(job) == session)
		{
			VR_DEBUG_PRINT(4, ("VR GP scheduler: Removing GP job 0x%08x from queue\n", job));
			_vr_osk_list_del(&(job->list));
			vr_gp_job_delete(job);

			vr_pm_core_event(VR_CORE_EVENT_GP_STOP);
		}
	}

	vr_gp_scheduler_unlock();

	vr_group_abort_session(slot.group, session);
}

static vr_bool vr_gp_scheduler_is_suspended(void)
{
	vr_bool ret;

	vr_gp_scheduler_lock();
	ret = pause_count > 0 && slot.state == VR_GP_SLOT_STATE_IDLE;
	vr_gp_scheduler_unlock();

	return ret;
}


#if VR_STATE_TRACKING
u32 vr_gp_scheduler_dump_state(char *buf, u32 size)
{
	int n = 0;

	n += _vr_osk_snprintf(buf + n, size - n, "GP\n");
	n += _vr_osk_snprintf(buf + n, size - n, "\tQueue is %s\n", _vr_osk_list_empty(&job_queue) ? "empty" : "not empty");

	n += vr_group_dump_state(slot.group, buf + n, size - n);
	n += _vr_osk_snprintf(buf + n, size - n, "\n");

	return n;
}
#endif

void vr_gp_scheduler_reset_all_groups(void)
{
	if (NULL != slot.group)
	{
		vr_group_reset(slot.group);
	}
}

void vr_gp_scheduler_zap_all_active(struct vr_session_data *session)
{
	if (NULL != slot.group)
	{
		vr_group_zap_session(slot.group, session);
	}
}
