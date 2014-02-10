/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_pp_scheduler.h"
#include "vr_kernel_common.h"
#include "vr_kernel_core.h"
#include "vr_osk.h"
#include "vr_osk_list.h"
#include "vr_scheduler.h"
#include "vr_pp.h"
#include "vr_pp_job.h"
#include "vr_group.h"
#include "vr_pm.h"

#if defined(CONFIG_SYNC)
#define VR_PP_SCHEDULER_USE_DEFERRED_JOB_DELETE 1
#endif

/* Maximum of 8 PP cores (a group can only have maximum of 1 PP core) */
#define VR_MAX_NUMBER_OF_PP_GROUPS 9

static vr_bool vr_pp_scheduler_is_suspended(void);
static void vr_pp_scheduler_do_schedule(void *arg);
#if defined(VR_PP_SCHEDULER_USE_DEFERRED_JOB_DELETE)
static void vr_pp_scheduler_do_job_delete(void *arg);
#endif

static u32 pp_version = 0;

/* Physical job queue */
static _VR_OSK_LIST_HEAD_STATIC_INIT(job_queue);              /* List of physical jobs with some unscheduled work */
static u32 job_queue_depth = 0;

/* Physical groups */
static _VR_OSK_LIST_HEAD_STATIC_INIT(group_list_working);     /* List of physical groups with working jobs on the pp core */
static _VR_OSK_LIST_HEAD_STATIC_INIT(group_list_idle);        /* List of physical groups with idle jobs on the pp core */

/* Virtual job queue (VR-450 only) */
static _VR_OSK_LIST_HEAD_STATIC_INIT(virtual_job_queue);      /* List of unstarted jobs for the virtual group */
static u32 virtual_job_queue_depth = 0;

/* Virtual group (VR-450 only) */
static struct vr_group *virtual_group = NULL;                 /* Virtual group (if any) */
static vr_bool virtual_group_working = VR_FALSE;            /* Flag which indicates whether the virtual group is working or idle */

/* Number of physical cores */
static u32 num_cores = 0;

/* Variables to allow safe pausing of the scheduler */
static _vr_osk_wait_queue_t *pp_scheduler_working_wait_queue = NULL;
static u32 pause_count = 0;

static _vr_osk_lock_t *pp_scheduler_lock = NULL;
/* Contains tid of thread that locked the scheduler or 0, if not locked */
VR_DEBUG_CODE(static u32 pp_scheduler_lock_owner = 0);

static _vr_osk_wq_work_t *pp_scheduler_wq_schedule = NULL;

#if defined(VR_PP_SCHEDULER_USE_DEFERRED_JOB_DELETE)
static _vr_osk_wq_work_t *pp_scheduler_wq_job_delete = NULL;
static _vr_osk_lock_t *pp_scheduler_job_delete_lock = NULL;
static _VR_OSK_LIST_HEAD_STATIC_INIT(pp_scheduler_job_deletion_queue);
#endif

_vr_osk_errcode_t vr_pp_scheduler_initialize(void)
{
	struct vr_group *group;
	struct vr_pp_core *pp_core;
	_vr_osk_lock_flags_t lock_flags;
	u32 num_groups;
	u32 i;

#if defined(VR_UPPER_HALF_SCHEDULING)
	lock_flags = _VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_SPINLOCK_IRQ | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE;
#else
	lock_flags = _VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_SPINLOCK | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE;
#endif

	_VR_OSK_INIT_LIST_HEAD(&job_queue);
	_VR_OSK_INIT_LIST_HEAD(&group_list_working);
	_VR_OSK_INIT_LIST_HEAD(&group_list_idle);

	_VR_OSK_INIT_LIST_HEAD(&virtual_job_queue);

	pp_scheduler_lock = _vr_osk_lock_init(lock_flags, 0, _VR_OSK_LOCK_ORDER_SCHEDULER);
	if (NULL == pp_scheduler_lock)
	{
		return _VR_OSK_ERR_NOMEM;
	}

	pp_scheduler_working_wait_queue = _vr_osk_wait_queue_init();
	if (NULL == pp_scheduler_working_wait_queue)
	{
		_vr_osk_lock_term(pp_scheduler_lock);
		return _VR_OSK_ERR_NOMEM;
	}

	pp_scheduler_wq_schedule = _vr_osk_wq_create_work(vr_pp_scheduler_do_schedule, NULL);
	if (NULL == pp_scheduler_wq_schedule)
	{
		_vr_osk_wait_queue_term(pp_scheduler_working_wait_queue);
		_vr_osk_lock_term(pp_scheduler_lock);
		return _VR_OSK_ERR_NOMEM;
	}

#if defined(VR_PP_SCHEDULER_USE_DEFERRED_JOB_DELETE)
	pp_scheduler_wq_job_delete = _vr_osk_wq_create_work(vr_pp_scheduler_do_job_delete, NULL);
	if (NULL == pp_scheduler_wq_job_delete)
	{
		_vr_osk_wq_delete_work(pp_scheduler_wq_schedule);
		_vr_osk_wait_queue_term(pp_scheduler_working_wait_queue);
		_vr_osk_lock_term(pp_scheduler_lock);
		return _VR_OSK_ERR_NOMEM;
	}

	pp_scheduler_job_delete_lock = _vr_osk_lock_init(_VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_SPINLOCK_IRQ |_VR_OSK_LOCKFLAG_NONINTERRUPTABLE, 0, _VR_OSK_LOCK_ORDER_SCHEDULER_DEFERRED);
	if (NULL == pp_scheduler_job_delete_lock)
	{
		_vr_osk_wq_delete_work(pp_scheduler_wq_job_delete);
		_vr_osk_wq_delete_work(pp_scheduler_wq_schedule);
		_vr_osk_wait_queue_term(pp_scheduler_working_wait_queue);
		_vr_osk_lock_term(pp_scheduler_lock);
		return _VR_OSK_ERR_NOMEM;
	}
#endif

	num_groups = vr_group_get_glob_num_groups();

	/* Do we have a virtual group? */
	for (i = 0; i < num_groups; i++)
	{
		group = vr_group_get_glob_group(i);

		if (vr_group_is_virtual(group))
		{
			VR_DEBUG_PRINT(3, ("Found virtual group %p\n", group));

			virtual_group = group;
			break;
		}
	}

	/* Find all the available PP cores */
	for (i = 0; i < num_groups; i++)
	{
		group = vr_group_get_glob_group(i);
		pp_core = vr_group_get_pp_core(group);

		if (NULL != pp_core && !vr_group_is_virtual(group))
		{
			if (0 == pp_version)
			{
				/* Retrieve PP version from the first available PP core */
				pp_version = vr_pp_core_get_version(pp_core);
			}

			if (NULL != virtual_group)
			{
				/* Add all physical PP cores to the virtual group */
				vr_group_lock(virtual_group);
				group->state = VR_GROUP_STATE_JOINING_VIRTUAL;
				vr_group_add_group(virtual_group, group);
				vr_group_unlock(virtual_group);
			}
			else
			{
				_vr_osk_list_add(&group->pp_scheduler_list, &group_list_idle);
			}

			num_cores++;
		}
	}

	return _VR_OSK_ERR_OK;
}

void vr_pp_scheduler_terminate(void)
{
	struct vr_group *group, *temp;

	/* Delete all groups owned by scheduler */
	if (NULL != virtual_group)
	{
		vr_group_delete(virtual_group);
	}

	VR_DEBUG_ASSERT(_vr_osk_list_empty(&group_list_working));
	_VR_OSK_LIST_FOREACHENTRY(group, temp, &group_list_idle, struct vr_group, pp_scheduler_list)
	{
		vr_group_delete(group);
	}

#if defined(VR_PP_SCHEDULER_USE_DEFERRED_JOB_DELETE)
	_vr_osk_lock_term(pp_scheduler_job_delete_lock);
	_vr_osk_wq_delete_work(pp_scheduler_wq_job_delete);
#endif

	_vr_osk_wq_delete_work(pp_scheduler_wq_schedule);
	_vr_osk_wait_queue_term(pp_scheduler_working_wait_queue);
	_vr_osk_lock_term(pp_scheduler_lock);
}

VR_STATIC_INLINE void vr_pp_scheduler_lock(void)
{
	if(_VR_OSK_ERR_OK != _vr_osk_lock_wait(pp_scheduler_lock, _VR_OSK_LOCKMODE_RW))
	{
		/* Non-interruptable lock failed: this should never happen. */
		VR_DEBUG_ASSERT(0);
	}
	VR_DEBUG_PRINT(5, ("VR PP scheduler: PP scheduler lock taken\n"));
	VR_DEBUG_ASSERT(0 == pp_scheduler_lock_owner);
	VR_DEBUG_CODE(pp_scheduler_lock_owner = _vr_osk_get_tid());
}

VR_STATIC_INLINE void vr_pp_scheduler_unlock(void)
{
	VR_DEBUG_PRINT(5, ("VR PP scheduler: Releasing PP scheduler lock\n"));
	VR_DEBUG_ASSERT(_vr_osk_get_tid() == pp_scheduler_lock_owner);
	VR_DEBUG_CODE(pp_scheduler_lock_owner = 0);
	_vr_osk_lock_signal(pp_scheduler_lock, _VR_OSK_LOCKMODE_RW);
}

#ifdef DEBUG
VR_STATIC_INLINE void vr_pp_scheduler_assert_locked(void)
{
	VR_DEBUG_ASSERT(_vr_osk_get_tid() == pp_scheduler_lock_owner);
}
#define VR_ASSERT_PP_SCHEDULER_LOCKED() vr_pp_scheduler_assert_locked()
#else
#define VR_ASSERT_PP_SCHEDULER_LOCKED()
#endif

/**
 * Returns a physical job if a physical job is ready to run (no barrier present)
 */
VR_STATIC_INLINE struct vr_pp_job *vr_pp_scheduler_get_physical_job(void)
{
	VR_ASSERT_PP_SCHEDULER_LOCKED();

	if (!_vr_osk_list_empty(&job_queue))
	{
		struct vr_pp_job *job;

		VR_DEBUG_ASSERT(job_queue_depth > 0);
		job = _VR_OSK_LIST_ENTRY(job_queue.next, struct vr_pp_job, list);

		if (!vr_pp_job_has_active_barrier(job))
		{
			return job;
		}
	}

	return NULL;
}

VR_STATIC_INLINE void vr_pp_scheduler_dequeue_physical_job(struct vr_pp_job *job)
{
	VR_ASSERT_PP_SCHEDULER_LOCKED();
	VR_DEBUG_ASSERT(job_queue_depth > 0);

	/* Remove job from queue */
	if (!vr_pp_job_has_unstarted_sub_jobs(job))
	{
		/* All sub jobs have been started: remove job from queue */
		_vr_osk_list_delinit(&job->list);
	}

	--job_queue_depth;
}

/**
 * Returns a virtual job if a virtual job is ready to run (no barrier present)
 */
VR_STATIC_INLINE struct vr_pp_job *vr_pp_scheduler_get_virtual_job(void)
{
	VR_ASSERT_PP_SCHEDULER_LOCKED();
	VR_DEBUG_ASSERT_POINTER(virtual_group);

	if (!_vr_osk_list_empty(&virtual_job_queue))
	{
		struct vr_pp_job *job;

		VR_DEBUG_ASSERT(virtual_job_queue_depth > 0);
		job = _VR_OSK_LIST_ENTRY(virtual_job_queue.next, struct vr_pp_job, list);

		if (!vr_pp_job_has_active_barrier(job))
		{
			return job;
		}
	}

	return NULL;
}

VR_STATIC_INLINE void vr_pp_scheduler_dequeue_virtual_job(struct vr_pp_job *job)
{
	VR_ASSERT_PP_SCHEDULER_LOCKED();
	VR_DEBUG_ASSERT(virtual_job_queue_depth > 0);

	/* Remove job from queue */
	_vr_osk_list_delinit(&job->list);
	--virtual_job_queue_depth;
}

/**
 * Checks if the criteria is met for removing a physical core from virtual group
 */
VR_STATIC_INLINE vr_bool vr_pp_scheduler_can_move_virtual_to_physical(void)
{
	VR_ASSERT_PP_SCHEDULER_LOCKED();
	VR_DEBUG_ASSERT(NULL != virtual_group);
	VR_ASSERT_GROUP_LOCKED(virtual_group);
	/*
	 * The criteria for taking out a physical group from a virtual group are the following:
	 * - There virtual group is idle
	 * - There are currently no physical groups (idle and working)
	 * - There are physical jobs to be scheduled (without a barrier)
	 */
	return (!virtual_group_working) &&
	       _vr_osk_list_empty(&group_list_idle) &&
	       _vr_osk_list_empty(&group_list_working) &&
	       (NULL != vr_pp_scheduler_get_physical_job());
}

VR_STATIC_INLINE struct vr_group *vr_pp_scheduler_acquire_physical_group(void)
{
	VR_ASSERT_PP_SCHEDULER_LOCKED();

	if (!_vr_osk_list_empty(&group_list_idle))
	{
		VR_DEBUG_PRINT(4, ("VR PP scheduler: Acquiring physical group from idle list\n"));
		return _VR_OSK_LIST_ENTRY(group_list_idle.next, struct vr_group, pp_scheduler_list);
	}
	else if (NULL != virtual_group)
	{
		VR_ASSERT_GROUP_LOCKED(virtual_group);
		if (vr_pp_scheduler_can_move_virtual_to_physical())
		{
			VR_DEBUG_PRINT(4, ("VR PP scheduler: Acquiring physical group from virtual group\n"));
			return vr_group_acquire_group(virtual_group);
		}
	}

	return NULL;
}

static void vr_pp_scheduler_schedule(void)
{
	struct vr_group* physical_groups_to_start[VR_MAX_NUMBER_OF_PP_GROUPS-1];
	struct vr_pp_job* physical_jobs_to_start[VR_MAX_NUMBER_OF_PP_GROUPS-1];
	u32 physical_subjobs_to_start[VR_MAX_NUMBER_OF_PP_GROUPS-1];
	int num_physical_jobs_to_start = 0;
	int i;

	if (NULL != virtual_group)
	{
		/* Need to lock the virtual group because we might need to grab a physical group from it */
		vr_group_lock(virtual_group);
	}

	vr_pp_scheduler_lock();
	if (pause_count > 0)
	{
		/* Scheduler is suspended, don't schedule any jobs */
		vr_pp_scheduler_unlock();
		if (NULL != virtual_group)
		{
			vr_group_unlock(virtual_group);
		}
		return;
	}

	/* Find physical job(s) to schedule first */
	while (1)
	{
		struct vr_group *group;
		struct vr_pp_job *job;
		u32 subjob;

		job = vr_pp_scheduler_get_physical_job();
		if (NULL == job)
		{
			break; /* No job, early out */
		}

		VR_DEBUG_ASSERT(!vr_pp_job_is_virtual(job));
		VR_DEBUG_ASSERT(vr_pp_job_has_unstarted_sub_jobs(job));
		VR_DEBUG_ASSERT(1 <= vr_pp_job_get_sub_job_count(job));

		/* Acquire a physical group, either from the idle list or from the virtual group.
		 * In case the group was acquired from the virtual group, it's state will be
		 * LEAVING_VIRTUAL and must be set to IDLE before it can be used. */
		group = vr_pp_scheduler_acquire_physical_group();
		if (NULL == group)
		{
			/* Could not get a group to run the job on, early out */
			VR_DEBUG_PRINT(4, ("VR PP scheduler: No more physical groups available.\n"));
			break;
		}

		VR_DEBUG_PRINT(4, ("VR PP scheduler: Acquired physical group %p\n", group));

		/* Mark subjob as started */
		subjob = vr_pp_job_get_first_unstarted_sub_job(job);
		vr_pp_job_mark_sub_job_started(job, subjob);

		/* Remove job from queue (if we now got the last subjob) */
		vr_pp_scheduler_dequeue_physical_job(job);

		/* Move group to working list */
		_vr_osk_list_move(&(group->pp_scheduler_list), &group_list_working);

		/* Keep track of this group, so that we actually can start the job once we are done with the scheduler lock we are now holding */
		physical_groups_to_start[num_physical_jobs_to_start] = group;
		physical_jobs_to_start[num_physical_jobs_to_start] = job;
		physical_subjobs_to_start[num_physical_jobs_to_start] = subjob;
		++num_physical_jobs_to_start;

		VR_DEBUG_ASSERT(num_physical_jobs_to_start < VR_MAX_NUMBER_OF_PP_GROUPS);
	}

	/* See if we have a virtual job to schedule */
	if (NULL != virtual_group)
	{
		if (!virtual_group_working)
		{
			struct vr_pp_job *job = vr_pp_scheduler_get_virtual_job();
			if (NULL != job)
			{
				VR_DEBUG_ASSERT(vr_pp_job_is_virtual(job));
				VR_DEBUG_ASSERT(vr_pp_job_has_unstarted_sub_jobs(job));
				VR_DEBUG_ASSERT(1 == vr_pp_job_get_sub_job_count(job));

				/* Mark the one and only subjob as started */
				vr_pp_job_mark_sub_job_started(job, 0);

				/* Remove job from queue */
				vr_pp_scheduler_dequeue_virtual_job(job);

				/* Virtual group is now working */
				virtual_group_working = VR_TRUE;

				/*
				 * We no longer need the scheduler lock,
				 * but we still need the virtual lock in order to start the virtual job
				 */
				vr_pp_scheduler_unlock();

				/* Start job */
				if (_VR_OSK_ERR_OK == vr_group_start_pp_job(virtual_group, job, 0))
				{
					VR_DEBUG_PRINT(4, ("VR PP scheduler: Virtual job %u (0x%08X) part %u/%u started (from schedule)\n",
					                     vr_pp_job_get_id(job), job, 1,
					                     vr_pp_job_get_sub_job_count(job)));
				}
				else
				{
					VR_DEBUG_ASSERT(0);
				}

				/* And now we are all done with the virtual_group lock as well */
				vr_group_unlock(virtual_group);
			}
			else
			{
				/* No virtual job, release the two locks we are holding */
				vr_pp_scheduler_unlock();
				vr_group_unlock(virtual_group);
			}
		}
		else
		{
			/* Virtual core busy, release the two locks we are holding */
			vr_pp_scheduler_unlock();
			vr_group_unlock(virtual_group);
		}

	}
	else
	{
		/* There is no virtual group, release the only lock we are holding */
		vr_pp_scheduler_unlock();
	}

	/*
	 * Now we have released the scheduler lock, and we are ready to kick of the actual starting of the
	 * physical jobs.
	 * The reason we want to wait until we have released the scheduler lock is that job start actually
	 * may take quite a bit of time (quite many registers needs to be written). This will allow new jobs
	 * from user space to come in, and post processing of other PP jobs to happen at the same time as we
	 * start jobs.
	 */
	for (i = 0; i < num_physical_jobs_to_start; i++)
	{
		struct vr_group *group = physical_groups_to_start[i];
		struct vr_pp_job *job  = physical_jobs_to_start[i];
		u32 sub_job              = physical_subjobs_to_start[i];

		VR_DEBUG_ASSERT_POINTER(group);
		VR_DEBUG_ASSERT_POINTER(job);

		vr_group_lock(group);

		/* In case this group was acquired from a virtual core, update it's state to IDLE */
		group->state = VR_GROUP_STATE_IDLE;

		if (_VR_OSK_ERR_OK == vr_group_start_pp_job(group, job, sub_job))
		{
			VR_DEBUG_PRINT(4, ("VR PP scheduler: Physical job %u (0x%08X) part %u/%u started (from schedule)\n",
			                     vr_pp_job_get_id(job), job, sub_job + 1,
			                     vr_pp_job_get_sub_job_count(job)));
		}
		else
		{
			VR_DEBUG_ASSERT(0);
		}

		vr_group_unlock(group);

		/* @@@@ todo: remove the return value from vr_group_start_xx_job, since we can't fail on VR-300++ */
	}
}

static void vr_pp_scheduler_return_job_to_user(struct vr_pp_job *job, vr_bool deferred)
{
	if (VR_FALSE == vr_pp_job_use_no_notification(job))
	{
		u32 i;
		u32 sub_jobs = vr_pp_job_get_sub_job_count(job);
		vr_bool success = vr_pp_job_was_success(job);

		_vr_uk_pp_job_finished_s *jobres = job->finished_notification->result_buffer;
		_vr_osk_memset(jobres, 0, sizeof(_vr_uk_pp_job_finished_s)); /* @@@@ can be removed once we initialize all members in this struct */
		jobres->user_job_ptr = vr_pp_job_get_user_id(job);
		if (VR_TRUE == success)
		{
			jobres->status = _VR_UK_JOB_STATUS_END_SUCCESS;
		}
		else
		{
			jobres->status = _VR_UK_JOB_STATUS_END_UNKNOWN_ERR;
		}

		for (i = 0; i < sub_jobs; i++)
		{
			jobres->perf_counter0[i] = vr_pp_job_get_perf_counter_value0(job, i);
			jobres->perf_counter1[i] = vr_pp_job_get_perf_counter_value1(job, i);
		}

		vr_session_send_notification(vr_pp_job_get_session(job), job->finished_notification);
		job->finished_notification = NULL;
	}

#if defined(VR_PP_SCHEDULER_USE_DEFERRED_JOB_DELETE)
	if (VR_TRUE == deferred)
	{
		/* The deletion of the job object (releasing sync refs etc) must be done in a different context */
		_vr_osk_lock_wait(pp_scheduler_job_delete_lock, _VR_OSK_LOCKMODE_RW);

		VR_DEBUG_ASSERT(_vr_osk_list_empty(&job->list)); /* This job object should not be on any list */
		_vr_osk_list_addtail(&job->list, &pp_scheduler_job_deletion_queue);

		_vr_osk_lock_signal(pp_scheduler_job_delete_lock, _VR_OSK_LOCKMODE_RW);

		_vr_osk_wq_schedule_work(pp_scheduler_wq_job_delete);
	}
	else
	{
		vr_pp_job_delete(job);
	}
#else
	VR_DEBUG_ASSERT(VR_FALSE == deferred); /* no use cases need this in this configuration */
	vr_pp_job_delete(job);
#endif
}

static void vr_pp_scheduler_do_schedule(void *arg)
{
	VR_IGNORE(arg);

	vr_pp_scheduler_schedule();
}

#if defined(VR_PP_SCHEDULER_USE_DEFERRED_JOB_DELETE)
static void vr_pp_scheduler_do_job_delete(void *arg)
{
	_VR_OSK_LIST_HEAD_STATIC_INIT(list);
	struct vr_pp_job *job;
	struct vr_pp_job *tmp;

	VR_IGNORE(arg);

	_vr_osk_lock_wait(pp_scheduler_job_delete_lock, _VR_OSK_LOCKMODE_RW);

	/*
	 * Quickly "unhook" the jobs pending to be deleted, so we can release the lock before
	 * we start deleting the job objects (without any locks held
	 */
	_vr_osk_list_move_list(&pp_scheduler_job_deletion_queue, &list);

	_vr_osk_lock_signal(pp_scheduler_job_delete_lock, _VR_OSK_LOCKMODE_RW);

	_VR_OSK_LIST_FOREACHENTRY(job, tmp, &list, struct vr_pp_job, list)
	{
		vr_pp_job_delete(job); /* delete the job object itself */
	}
}
#endif

void vr_pp_scheduler_job_done(struct vr_group *group, struct vr_pp_job *job, u32 sub_job, vr_bool success)
{
	vr_bool job_is_done;
	vr_bool barrier_enforced = VR_FALSE;

	VR_DEBUG_PRINT(3, ("VR PP scheduler: %s job %u (0x%08X) part %u/%u completed (%s)\n",
	                     vr_pp_job_is_virtual(job) ? "Virtual" : "Physical",
	                     vr_pp_job_get_id(job),
	                     job, sub_job + 1,
	                     vr_pp_job_get_sub_job_count(job),
	                     success ? "success" : "failure"));
	VR_ASSERT_GROUP_LOCKED(group);
	vr_pp_scheduler_lock();

	vr_pp_job_mark_sub_job_completed(job, success);

	VR_DEBUG_ASSERT(vr_pp_job_is_virtual(job) == vr_group_is_virtual(group));

	job_is_done = vr_pp_job_is_complete(job);

	if (job_is_done)
	{
		struct vr_session_data *session = vr_pp_job_get_session(job);
		struct vr_pp_job *job_head;

		/* Remove job from session list */
		_vr_osk_list_del(&job->session_list);

		/* Send notification back to user space */
		VR_DEBUG_PRINT(4, ("VR PP scheduler: All parts completed for %s job %u (0x%08X)\n",
		                     vr_pp_job_is_virtual(job) ? "virtual" : "physical",
		                     vr_pp_job_get_id(job), job));
#if defined(CONFIG_SYNC)
		if (job->sync_point)
		{
			int error;
			if (success) error = 0;
			else error = -EFAULT;
			VR_DEBUG_PRINT(4, ("Sync: Signal %spoint for job %d\n",
			                     success ? "" : "failed ",
					     vr_pp_job_get_id(job)));
			vr_sync_signal_pt(job->sync_point, error);
		}
#endif

#if defined(VR_PP_SCHEDULER_USE_DEFERRED_JOB_DELETE)
		vr_pp_scheduler_return_job_to_user(job, VR_TRUE);
#else
		vr_pp_scheduler_return_job_to_user(job, VR_FALSE);
#endif

		vr_pm_core_event(VR_CORE_EVENT_PP_STOP);

		/* Resolve any barriers */
		if (!_vr_osk_list_empty(&session->job_list))
		{
			job_head = _VR_OSK_LIST_ENTRY(session->job_list.next, struct vr_pp_job, session_list);
			if (vr_pp_job_has_active_barrier(job_head))
			{
				barrier_enforced = VR_TRUE;
				vr_pp_job_barrier_enforced(job_head);
			}
		}
	}

	/* If paused, then this was the last job, so wake up sleeping workers */
	if (pause_count > 0)
	{
		/* Wake up sleeping workers. Their wake-up condition is that
		 * num_slots == num_slots_idle, so unless we are done working, no
		 * threads will actually be woken up.
		 */
		_vr_osk_wait_queue_wake_up(pp_scheduler_working_wait_queue);
		vr_pp_scheduler_unlock();
		return;
	}

	if (barrier_enforced)
	{
		/* A barrier was resolved, so schedule previously blocked jobs */
		_vr_osk_wq_schedule_work(pp_scheduler_wq_schedule);

		/* TODO: Subjob optimisation */
	}

	/* Recycle variables */
	job = NULL;
	sub_job = 0;

	if (vr_group_is_virtual(group))
	{
		/* Virtual group */

		/* Now that the virtual group is idle, check if we should reconfigure */
		struct vr_pp_job *physical_job = NULL;
		struct vr_group *physical_group = NULL;

		/* Obey the policy */
		virtual_group_working = VR_FALSE;

		if (vr_pp_scheduler_can_move_virtual_to_physical())
		{
			/* There is a runnable physical job and we can acquire a physical group */
			physical_job = vr_pp_scheduler_get_physical_job();
			VR_DEBUG_ASSERT(vr_pp_job_has_unstarted_sub_jobs(physical_job));

			/* Mark subjob as started */
			sub_job = vr_pp_job_get_first_unstarted_sub_job(physical_job);
			vr_pp_job_mark_sub_job_started(physical_job, sub_job);

			/* Remove job from queue (if we now got the last subjob) */
			vr_pp_scheduler_dequeue_physical_job(physical_job);

			/* Acquire a physical group from the virtual group
			 * It's state will be LEAVING_VIRTUAL and must be set to IDLE before it can be used */
			physical_group = vr_group_acquire_group(virtual_group);

			/* Move physical group to the working list, as we will soon start a job on it */
			_vr_osk_list_move(&(physical_group->pp_scheduler_list), &group_list_working);
		}

		/* Start the next virtual job */
		job = vr_pp_scheduler_get_virtual_job();
		if (NULL != job)
		{
			/* There is a runnable virtual job */
			VR_DEBUG_ASSERT(vr_pp_job_is_virtual(job));
			VR_DEBUG_ASSERT(vr_pp_job_has_unstarted_sub_jobs(job));
			VR_DEBUG_ASSERT(1 == vr_pp_job_get_sub_job_count(job));

			vr_pp_job_mark_sub_job_started(job, 0);

			/* Remove job from queue */
			vr_pp_scheduler_dequeue_virtual_job(job);

			/* Virtual group is now working */
			virtual_group_working = VR_TRUE;

			vr_pp_scheduler_unlock();

			/* Start job */
			if (_VR_OSK_ERR_OK == vr_group_start_pp_job(group, job, 0))
			{
				VR_DEBUG_PRINT(4, ("VR PP scheduler: Virtual job %u (0x%08X) part %u/%u started (from job_done)\n",
				                     vr_pp_job_get_id(job), job, 1,
				                     vr_pp_job_get_sub_job_count(job)));
			}
			else
			{
				VR_DEBUG_ASSERT(0);
			}
		}
		else
		{
			vr_pp_scheduler_unlock();
		}

		/* Start a physical job (if we acquired a physical group earlier) */
		if (NULL != physical_job && NULL != physical_group)
		{
			vr_group_lock(physical_group);

			/* Set the group state from LEAVING_VIRTUAL to IDLE to complete the transition */
			physical_group->state = VR_GROUP_STATE_IDLE;

			/* Start job on core */
			if (_VR_OSK_ERR_OK == vr_group_start_pp_job(physical_group, physical_job, sub_job))
			{
				VR_DEBUG_PRINT(4, ("VR PP scheduler: Physical job %u (0x%08X) part %u/%u started (from job_done)\n",
				                     vr_pp_job_get_id(physical_job), physical_job, sub_job + 1,
				                     vr_pp_job_get_sub_job_count(physical_job)));
			}
			else
			{
				/* @@@@ todo: this cant fail on VR-300+, no need to implement put back of job */
				VR_DEBUG_ASSERT(0);
			}

			vr_group_unlock(physical_group);
		}
	}
	else
	{
		/* Physical group */
		job = vr_pp_scheduler_get_physical_job();
		if (NULL != job)
		{
			/* There is a runnable physical job */
			VR_DEBUG_ASSERT(vr_pp_job_has_unstarted_sub_jobs(job));

			/* Mark subjob as started */
			sub_job = vr_pp_job_get_first_unstarted_sub_job(job);
			vr_pp_job_mark_sub_job_started(job, sub_job);

			/* Remove job from queue (if we now got the last subjob) */
			vr_pp_scheduler_dequeue_physical_job(job);

			vr_pp_scheduler_unlock();

			/* Group is already on the working list, so start the job */
			if (_VR_OSK_ERR_OK == vr_group_start_pp_job(group, job, sub_job))
			{
				VR_DEBUG_PRINT(4, ("VR PP scheduler: Physical job %u (0x%08X) part %u/%u started (from job_done)\n",
				                     vr_pp_job_get_id(job), job, sub_job + 1,
				                     vr_pp_job_get_sub_job_count(job)));
			}
			else
			{
				VR_DEBUG_ASSERT(0);
			}
		}
		else if (NULL != virtual_group)
		{
			/* Rejoin virtual group */
			/* TODO: In the future, a policy check might be useful here */

			/* We're no longer needed on the scheduler list */
			_vr_osk_list_delinit(&(group->pp_scheduler_list));

			/* Make sure no interrupts are handled for this group during
			 * the transition from physical to virtual */
			group->state = VR_GROUP_STATE_JOINING_VIRTUAL;

			vr_pp_scheduler_unlock();
			vr_group_unlock(group);

			vr_group_lock(virtual_group);
			/* TODO: Take group lock also? */
			vr_group_add_group(virtual_group, group);
			vr_group_unlock(virtual_group);

			/* We need to return from this function with the group lock held */
			/* TODO: optimise! */
			vr_group_lock(group);
		}
		else
		{
			_vr_osk_list_move(&(group->pp_scheduler_list), &group_list_idle);
			vr_pp_scheduler_unlock();
		}
	}
}

void vr_pp_scheduler_suspend(void)
{
	vr_pp_scheduler_lock();
	pause_count++; /* Increment the pause_count so that no more jobs will be scheduled */
	vr_pp_scheduler_unlock();

	/* Go to sleep. When woken up again (in vr_pp_scheduler_job_done), the
	 * vr_pp_scheduler_suspended() function will be called. This will return true
	 * iff state is idle and pause_count > 0, so if the core is active this
	 * will not do anything.
	 */
	_vr_osk_wait_queue_wait_event(pp_scheduler_working_wait_queue, vr_pp_scheduler_is_suspended);
}

void vr_pp_scheduler_resume(void)
{
	vr_pp_scheduler_lock();
	pause_count--; /* Decrement pause_count to allow scheduling again (if it reaches 0) */
	vr_pp_scheduler_unlock();
	if (0 == pause_count)
	{
		vr_pp_scheduler_schedule();
	}
}

VR_STATIC_INLINE void vr_pp_scheduler_queue_job(struct vr_pp_job *job, struct vr_session_data *session)
{
	VR_DEBUG_ASSERT_POINTER(job);

	vr_pm_core_event(VR_CORE_EVENT_PP_START);

	vr_pp_scheduler_lock();

	if (vr_pp_job_is_virtual(job))
	{
		/* Virtual job */
		virtual_job_queue_depth += 1;
		_vr_osk_list_addtail(&job->list, &virtual_job_queue);
	}
	else
	{
		job_queue_depth += vr_pp_job_get_sub_job_count(job);
		_vr_osk_list_addtail(&job->list, &job_queue);
	}

	if (vr_pp_job_has_active_barrier(job) && _vr_osk_list_empty(&session->job_list))
	{
		/* No running jobs on this session, so barrier condition already met */
		vr_pp_job_barrier_enforced(job);
	}

	/* Add job to session list */
	_vr_osk_list_addtail(&job->session_list, &session->job_list);

	VR_DEBUG_PRINT(3, ("VR PP scheduler: %s job %u (0x%08X) with %u parts queued\n",
	                     vr_pp_job_is_virtual(job) ? "Virtual" : "Physical",
	                     vr_pp_job_get_id(job), job, vr_pp_job_get_sub_job_count(job)));

	vr_pp_scheduler_unlock();
}

#if defined(CONFIG_SYNC)
static void sync_callback(struct sync_fence *fence, struct sync_fence_waiter *waiter)
{
	struct vr_pp_job *job = _VR_OSK_CONTAINER_OF(waiter, struct vr_pp_job, sync_waiter);

	/* Schedule sync_callback_work */
	_vr_osk_wq_schedule_work(job->sync_work);
}

static void sync_callback_work(void *arg)
{
	struct vr_pp_job *job = (struct vr_pp_job *)arg;
	struct vr_session_data *session;
	int err;

	VR_DEBUG_ASSERT_POINTER(job);

	session = job->session;

	/* Remove job from session pending job list */
	_vr_osk_lock_wait(session->pending_jobs_lock, _VR_OSK_LOCKMODE_RW);
	_vr_osk_list_delinit(&job->list);
	_vr_osk_lock_signal(session->pending_jobs_lock, _VR_OSK_LOCKMODE_RW);

	err = sync_fence_wait(job->pre_fence, 0);
	if (likely(0 == err))
	{
		VR_DEBUG_PRINT(3, ("VR sync: Job %d ready to run\n", vr_pp_job_get_id(job)));

		vr_pp_scheduler_queue_job(job, session);

		vr_pp_scheduler_schedule();
	}
	else
	{
		/* Fence signaled error */
		VR_DEBUG_PRINT(3, ("VR sync: Job %d abort due to sync error\n", vr_pp_job_get_id(job)));

		if (job->sync_point) vr_sync_signal_pt(job->sync_point, err);

		vr_pp_job_mark_sub_job_completed(job, VR_FALSE); /* Flagging the job as failed. */
		vr_pp_scheduler_return_job_to_user(job, VR_FALSE); /* This will also delete the job object */
	}
}
#endif

_vr_osk_errcode_t _vr_ukk_pp_start_job(void *ctx, _vr_uk_pp_start_job_s *uargs, int *fence)
{
	struct vr_session_data *session;
	struct vr_pp_job *job;
#if defined(CONFIG_SYNC)
	int post_fence = -1;
#endif

	VR_DEBUG_ASSERT_POINTER(uargs);
	VR_DEBUG_ASSERT_POINTER(ctx);

	session = (struct vr_session_data*)ctx;

	job = vr_pp_job_create(session, uargs, vr_scheduler_get_new_id());
	if (NULL == job)
	{
		VR_PRINT_ERROR(("Failed to create job!\n"));
		return _VR_OSK_ERR_NOMEM;
	}

	if (_VR_OSK_ERR_OK != vr_pp_job_check(job))
	{
		/* Not a valid job, return to user immediately */
		vr_pp_job_mark_sub_job_completed(job, VR_FALSE); /* Flagging the job as failed. */
		vr_pp_scheduler_return_job_to_user(job, VR_FALSE); /* This will also delete the job object */
		return _VR_OSK_ERR_OK; /* User is notified via a notification, so this call is ok */
	}

#if PROFILING_SKIP_PP_JOBS || PROFILING_SKIP_PP_AND_GP_JOBS
#warning PP jobs will not be executed
	vr_pp_scheduler_return_job_to_user(job, VR_FALSE);
	return _VR_OSK_ERR_OK;
#endif

#if defined(CONFIG_SYNC)
	if (_VR_PP_JOB_FLAG_FENCE & job->uargs.flags)
	{
		job->sync_point = vr_stream_create_point(job->uargs.stream);

		if (unlikely(NULL == job->sync_point))
		{
			/* Fence creation failed. */
			VR_DEBUG_PRINT(2, ("Failed to create sync point for job %d\n", vr_pp_job_get_id(job)));
			vr_pp_job_mark_sub_job_completed(job, VR_FALSE); /* Flagging the job as failed. */
			vr_pp_scheduler_return_job_to_user(job, VR_FALSE); /* This will also delete the job object */
			return _VR_OSK_ERR_OK; /* User is notified via a notification, so this call is ok */
		}

		post_fence = vr_stream_create_fence(job->sync_point);

		if (unlikely(0 > post_fence))
		{
			/* Fence creation failed. */
			/* vr_stream_create_fence already freed the sync_point */
			VR_DEBUG_PRINT(2, ("Failed to create fence for job %d\n", vr_pp_job_get_id(job)));
			vr_pp_job_mark_sub_job_completed(job, VR_FALSE); /* Flagging the job as failed. */
			vr_pp_scheduler_return_job_to_user(job, VR_FALSE); /* This will also delete the job object */
			return _VR_OSK_ERR_OK; /* User is notified via a notification, so this call is ok */
		}

		/* Grab a reference to the fence. It must be around when the
		 * job is completed, so the point can be signalled. */
		sync_fence_fdget(post_fence);

		*fence = post_fence;

		VR_DEBUG_PRINT(3, ("Sync: Created fence %d for job %d\n", post_fence, vr_pp_job_get_id(job)));
	}

	if (0 < job->uargs.fence)
	{
		int pre_fence_fd = job->uargs.fence;
		int err;

		VR_DEBUG_PRINT(2, ("Sync: Job %d waiting for fence %d\n", vr_pp_job_get_id(job), pre_fence_fd));

		job->pre_fence = sync_fence_fdget(pre_fence_fd); /* Reference will be released when job is deleted. */
		if (NULL == job->pre_fence)
		{
			VR_DEBUG_PRINT(2, ("Failed to import fence %d\n", pre_fence_fd));
			if (job->sync_point) vr_sync_signal_pt(job->sync_point, -EINVAL);
			vr_pp_job_mark_sub_job_completed(job, VR_FALSE); /* Flagging the job as failed. */
			vr_pp_scheduler_return_job_to_user(job, VR_FALSE); /* This will also delete the job object */
			return _VR_OSK_ERR_OK; /* User is notified via a notification, so this call is ok */
		}

		job->sync_work = _vr_osk_wq_create_work(sync_callback_work, (void*)job);
		if (NULL == job->sync_work)
		{
			if (job->sync_point) vr_sync_signal_pt(job->sync_point, -ENOMEM);
			vr_pp_job_mark_sub_job_completed(job, VR_FALSE); /* Flagging the job as failed. */
			vr_pp_scheduler_return_job_to_user(job, VR_FALSE); /* This will also delete the job object */
			return _VR_OSK_ERR_OK; /* User is notified via a notification, so this call is ok */
		}

		/* Add pending job to session pending job list */
		_vr_osk_lock_wait(session->pending_jobs_lock, _VR_OSK_LOCKMODE_RW);
		_vr_osk_list_addtail(&job->list, &session->pending_jobs);
		_vr_osk_lock_signal(session->pending_jobs_lock, _VR_OSK_LOCKMODE_RW);

		sync_fence_waiter_init(&job->sync_waiter, sync_callback);
		err = sync_fence_wait_async(job->pre_fence, &job->sync_waiter);

		if (0 != err)
		{
			/* No async wait started, remove job from session pending job list */
			_vr_osk_lock_wait(session->pending_jobs_lock, _VR_OSK_LOCKMODE_RW);
			_vr_osk_list_delinit(&job->list);
			_vr_osk_lock_signal(session->pending_jobs_lock, _VR_OSK_LOCKMODE_RW);
		}

		if (1 == err)
		{
			/* Fence has already signalled */
			vr_pp_scheduler_queue_job(job, session);
			if (0 == _vr_osk_list_empty(&group_list_idle)) vr_pp_scheduler_schedule();
			return _VR_OSK_ERR_OK;
		}
		else if (0 > err)
		{
			/* Sync fail */
			if (job->sync_point) vr_sync_signal_pt(job->sync_point, err);
			vr_pp_job_mark_sub_job_completed(job, VR_FALSE); /* Flagging the job as failed. */
			vr_pp_scheduler_return_job_to_user(job, VR_FALSE); /* This will also delete the job object */
			return _VR_OSK_ERR_OK; /* User is notified via a notification, so this call is ok */
		}

	}
	else
#endif /* CONFIG_SYNC */
	{
		vr_pp_scheduler_queue_job(job, session);

		if (!_vr_osk_list_empty(&group_list_idle) || !virtual_group_working)
		{
			vr_pp_scheduler_schedule();
		}
	}

	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t _vr_ukk_get_pp_number_of_cores(_vr_uk_get_pp_number_of_cores_s *args)
{
	VR_DEBUG_ASSERT_POINTER(args);
	VR_DEBUG_ASSERT_POINTER(args->ctx);
	args->number_of_cores = num_cores;
	return _VR_OSK_ERR_OK;
}

_vr_osk_errcode_t _vr_ukk_get_pp_core_version(_vr_uk_get_pp_core_version_s *args)
{
	VR_DEBUG_ASSERT_POINTER(args);
	VR_DEBUG_ASSERT_POINTER(args->ctx);
	args->version = pp_version;
	return _VR_OSK_ERR_OK;
}

void _vr_ukk_pp_job_disable_wb(_vr_uk_pp_disable_wb_s *args)
{
	struct vr_session_data *session;
	struct vr_pp_job *job;
	struct vr_pp_job *tmp;

	VR_DEBUG_ASSERT_POINTER(args);
	VR_DEBUG_ASSERT_POINTER(args->ctx);

	session = (struct vr_session_data*)args->ctx;

	/* Check queue for jobs that match */
	vr_pp_scheduler_lock();
	_VR_OSK_LIST_FOREACHENTRY(job, tmp, &job_queue, struct vr_pp_job, list)
	{
		if (vr_pp_job_get_session(job) == session &&
		    vr_pp_job_get_frame_builder_id(job) == (u32)args->fb_id &&
		    vr_pp_job_get_flush_id(job) == (u32)args->flush_id)
		{
			if (args->wbx & _VR_UK_PP_JOB_WB0)
			{
				vr_pp_job_disable_wb0(job);
			}
			if (args->wbx & _VR_UK_PP_JOB_WB1)
			{
				vr_pp_job_disable_wb1(job);
			}
			if (args->wbx & _VR_UK_PP_JOB_WB2)
			{
				vr_pp_job_disable_wb2(job);
			}
			break;
		}
	}
	vr_pp_scheduler_unlock();
}

void vr_pp_scheduler_abort_session(struct vr_session_data *session)
{
	struct vr_pp_job *job, *tmp_job;
	struct vr_group *group, *tmp_group;
	struct vr_group *groups[VR_MAX_NUMBER_OF_GROUPS];
	s32 i = 0;

	vr_pp_scheduler_lock();
	VR_DEBUG_PRINT(3, ("VR PP scheduler: Aborting all jobs from session 0x%08x\n", session));

	_VR_OSK_LIST_FOREACHENTRY(job, tmp_job, &session->job_list, struct vr_pp_job, session_list)
	{
		/* Remove job from queue (if it's not queued, list_del has no effect) */
		_vr_osk_list_delinit(&job->list);

		if (vr_pp_job_is_virtual(job))
		{
			VR_DEBUG_ASSERT(1 == vr_pp_job_get_sub_job_count(job));
			if (0 == vr_pp_job_get_first_unstarted_sub_job(job))
			{
				--virtual_job_queue_depth;
			}
		}
		else
		{
			job_queue_depth -= vr_pp_job_get_sub_job_count(job) - vr_pp_job_get_first_unstarted_sub_job(job);
		}

		/* Mark all unstarted jobs as failed */
		vr_pp_job_mark_unstarted_failed(job);

		if (vr_pp_job_is_complete(job))
		{
			_vr_osk_list_del(&job->session_list);

			/* It is safe to delete the job, since it won't land in job_done() */
			VR_DEBUG_PRINT(4, ("VR PP scheduler: Aborted PP job 0x%08x\n", job));
			vr_pp_job_delete(job);

			vr_pm_core_event(VR_CORE_EVENT_PP_STOP);
		}
		else
		{
			VR_DEBUG_PRINT(4, ("VR PP scheduler: Keeping partially started PP job 0x%08x in session\n", job));
		}
	}

	_VR_OSK_LIST_FOREACHENTRY(group, tmp_group, &group_list_working, struct vr_group, pp_scheduler_list)
	{
		groups[i++] = group;
	}

	_VR_OSK_LIST_FOREACHENTRY(group, tmp_group, &group_list_idle, struct vr_group, pp_scheduler_list)
	{
		groups[i++] = group;
	}

	vr_pp_scheduler_unlock();

	/* Abort running jobs from this session */
	while (i > 0)
	{
		vr_group_abort_session(groups[--i], session);
	}

	if (NULL != virtual_group)
	{
		vr_group_abort_session(virtual_group, session);
	}
}

static vr_bool vr_pp_scheduler_is_suspended(void)
{
	vr_bool ret;

	vr_pp_scheduler_lock();
	ret = pause_count > 0 && _vr_osk_list_empty(&group_list_working) && !virtual_group_working;
	vr_pp_scheduler_unlock();

	return ret;
}

int vr_pp_scheduler_get_queue_depth(void)
{
	return job_queue_depth;
}

#if VR_STATE_TRACKING
u32 vr_pp_scheduler_dump_state(char *buf, u32 size)
{
	int n = 0;
	struct vr_group *group;
	struct vr_group *temp;

	n += _vr_osk_snprintf(buf + n, size - n, "PP:\n");
	n += _vr_osk_snprintf(buf + n, size - n, "\tQueue is %s\n", _vr_osk_list_empty(&job_queue) ? "empty" : "not empty");
	n += _vr_osk_snprintf(buf + n, size - n, "\n");

	_VR_OSK_LIST_FOREACHENTRY(group, temp, &group_list_working, struct vr_group, pp_scheduler_list)
	{
		n += vr_group_dump_state(group, buf + n, size - n);
	}

	_VR_OSK_LIST_FOREACHENTRY(group, temp, &group_list_idle, struct vr_group, pp_scheduler_list)
	{
		n += vr_group_dump_state(group, buf + n, size - n);
	}

	if (NULL != virtual_group)
	{
		n += vr_group_dump_state(virtual_group, buf + n, size -n);
	}

	n += _vr_osk_snprintf(buf + n, size - n, "\n");
	return n;
}
#endif

/* This function is intended for power on reset of all cores.
 * No locking is done for the list iteration, which can only be safe if the
 * scheduler is paused and all cores idle. That is always the case on init and
 * power on. */
void vr_pp_scheduler_reset_all_groups(void)
{
	struct vr_group *group, *temp;

	if (NULL != virtual_group)
	{
		vr_group_reset(virtual_group);
	}

	VR_DEBUG_ASSERT(_vr_osk_list_empty(&group_list_working));

	_VR_OSK_LIST_FOREACHENTRY(group, temp, &group_list_idle, struct vr_group, pp_scheduler_list)
	{
		vr_group_reset(group);
	}
}

void vr_pp_scheduler_zap_all_active(struct vr_session_data *session)
{
	struct vr_group *group, *temp;
	struct vr_group *groups[VR_MAX_NUMBER_OF_GROUPS];
	s32 i = 0;

	if (NULL != virtual_group)
	{
		vr_group_zap_session(virtual_group, session);
	}

	vr_pp_scheduler_lock();
	_VR_OSK_LIST_FOREACHENTRY(group, temp, &group_list_working, struct vr_group, pp_scheduler_list)
	{
		groups[i++] = group;
	}
	vr_pp_scheduler_unlock();

	while (i > 0)
	{
		vr_group_zap_session(groups[--i], session);
	}
}
