/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_kernel_common.h"
#include "vr_group.h"
#include "vr_osk.h"
#include "vr_l2_cache.h"
#include "vr_gp.h"
#include "vr_pp.h"
#include "vr_mmu.h"
#include "vr_dlbu.h"
#include "vr_broadcast.h"
#include "vr_gp_scheduler.h"
#include "vr_pp_scheduler.h"
#include "vr_kernel_core.h"
#include "vr_osk_profiling.h"

static void vr_group_bottom_half_mmu(void *data);
static void vr_group_bottom_half_gp(void *data);
static void vr_group_bottom_half_pp(void *data);

static void vr_group_timeout(void *data);
static void vr_group_reset_pp(struct vr_group *group);

#if defined(CONFIG_VR400_PROFILING)
static void vr_group_report_l2_cache_counters_per_core(struct vr_group *group, u32 core_num);
#endif /* #if defined(CONFIG_VR400_PROFILING) */

/*
 * The group object is the most important object in the device driver,
 * and acts as the center of many HW operations.
 * The reason for this is that operations on the MMU will affect all
 * cores connected to this MMU (a group is defined by the MMU and the
 * cores which are connected to this).
 * The group lock is thus the most important lock, followed by the
 * GP and PP scheduler locks. They must be taken in the following
 * order:
 * GP/PP lock first, then group lock(s).
 */

static struct vr_group *vr_global_groups[VR_MAX_NUMBER_OF_GROUPS];
static u32 vr_global_num_groups = 0;

enum vr_group_activate_pd_status
{
	VR_GROUP_ACTIVATE_PD_STATUS_FAILED,
	VR_GROUP_ACTIVATE_PD_STATUS_OK_KEPT_PD,
	VR_GROUP_ACTIVATE_PD_STATUS_OK_SWITCHED_PD,
};

/* local helper functions */
static enum vr_group_activate_pd_status vr_group_activate_page_directory(struct vr_group *group, struct vr_session_data *session);
static void vr_group_deactivate_page_directory(struct vr_group *group, struct vr_session_data *session);
static void vr_group_remove_session_if_unused(struct vr_group *group, struct vr_session_data *session);
static void vr_group_recovery_reset(struct vr_group *group);
static void vr_group_mmu_page_fault(struct vr_group *group);

static void vr_group_post_process_job_pp(struct vr_group *group);
static void vr_group_post_process_job_gp(struct vr_group *group, vr_bool suspend);

void vr_group_lock(struct vr_group *group)
{
	if(_VR_OSK_ERR_OK != _vr_osk_lock_wait(group->lock, _VR_OSK_LOCKMODE_RW))
	{
		/* Non-interruptable lock failed: this should never happen. */
		VR_DEBUG_ASSERT(0);
	}
	VR_DEBUG_PRINT(5, ("VR group: Group lock taken 0x%08X\n", group));
}

void vr_group_unlock(struct vr_group *group)
{
	VR_DEBUG_PRINT(5, ("VR group: Releasing group lock 0x%08X\n", group));
	_vr_osk_lock_signal(group->lock, _VR_OSK_LOCKMODE_RW);
}

#ifdef DEBUG
void vr_group_assert_locked(struct vr_group *group)
{
	VR_DEBUG_ASSERT_LOCK_HELD(group->lock);
}
#endif


struct vr_group *vr_group_create(struct vr_l2_cache_core *core, struct vr_dlbu_core *dlbu, struct vr_bcast_unit *bcast)
{
	struct vr_group *group = NULL;
	_vr_osk_lock_flags_t lock_flags;

#if defined(VR_UPPER_HALF_SCHEDULING)
	lock_flags = _VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_SPINLOCK_IRQ | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE;
#else
	lock_flags = _VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_SPINLOCK | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE;
#endif

	if (vr_global_num_groups >= VR_MAX_NUMBER_OF_GROUPS)
	{
		VR_PRINT_ERROR(("VR group: Too many group objects created\n"));
		return NULL;
	}

	group = _vr_osk_calloc(1, sizeof(struct vr_group));
	if (NULL != group)
	{
		group->timeout_timer = _vr_osk_timer_init();

		if (NULL != group->timeout_timer)
		{
			_vr_osk_lock_order_t order;
			_vr_osk_timer_setcallback(group->timeout_timer, vr_group_timeout, (void *)group);

			if (NULL != dlbu)
			{
				order = _VR_OSK_LOCK_ORDER_GROUP_VIRTUAL;
			}
			else
			{
				order = _VR_OSK_LOCK_ORDER_GROUP;
			}

			group->lock = _vr_osk_lock_init(lock_flags, 0, order);
			if (NULL != group->lock)
			{
				group->l2_cache_core[0] = core;
				group->session = NULL;
				group->page_dir_ref_count = 0;
				group->power_is_on = VR_TRUE;
				group->state = VR_GROUP_STATE_IDLE;
				_vr_osk_list_init(&group->group_list);
				_vr_osk_list_init(&group->pp_scheduler_list);
				group->parent_group = NULL;
				group->l2_cache_core_ref_count[0] = 0;
				group->l2_cache_core_ref_count[1] = 0;
				group->bcast_core = bcast;
				group->dlbu_core = dlbu;

				vr_global_groups[vr_global_num_groups] = group;
				vr_global_num_groups++;

				return group;
			}
            _vr_osk_timer_term(group->timeout_timer);
		}
		_vr_osk_free(group);
	}

	return NULL;
}

_vr_osk_errcode_t vr_group_add_mmu_core(struct vr_group *group, struct vr_mmu_core* mmu_core)
{
	/* This group object now owns the MMU core object */
	group->mmu= mmu_core;
	group->bottom_half_work_mmu = _vr_osk_wq_create_work(vr_group_bottom_half_mmu, group);
	if (NULL == group->bottom_half_work_mmu)
	{
		return _VR_OSK_ERR_FAULT;
	}
	return _VR_OSK_ERR_OK;
}

void vr_group_remove_mmu_core(struct vr_group *group)
{
	/* This group object no longer owns the MMU core object */
	group->mmu = NULL;
	if (NULL != group->bottom_half_work_mmu)
	{
		_vr_osk_wq_delete_work(group->bottom_half_work_mmu);
	}
}

_vr_osk_errcode_t vr_group_add_gp_core(struct vr_group *group, struct vr_gp_core* gp_core)
{
	/* This group object now owns the GP core object */
	group->gp_core = gp_core;
	group->bottom_half_work_gp = _vr_osk_wq_create_work(vr_group_bottom_half_gp, group);
	if (NULL == group->bottom_half_work_gp)
	{
		return _VR_OSK_ERR_FAULT;
	}
	return _VR_OSK_ERR_OK;
}

void vr_group_remove_gp_core(struct vr_group *group)
{
	/* This group object no longer owns the GP core object */
	group->gp_core = NULL;
	if (NULL != group->bottom_half_work_gp)
	{
		_vr_osk_wq_delete_work(group->bottom_half_work_gp);
	}
}

_vr_osk_errcode_t vr_group_add_pp_core(struct vr_group *group, struct vr_pp_core* pp_core)
{
	/* This group object now owns the PP core object */
	group->pp_core = pp_core;
	group->bottom_half_work_pp = _vr_osk_wq_create_work(vr_group_bottom_half_pp, group);
	if (NULL == group->bottom_half_work_pp)
	{
		return _VR_OSK_ERR_FAULT;
	}
	return _VR_OSK_ERR_OK;
}

void vr_group_remove_pp_core(struct vr_group *group)
{
	/* This group object no longer owns the PP core object */
	group->pp_core = NULL;
	if (NULL != group->bottom_half_work_pp)
	{
		_vr_osk_wq_delete_work(group->bottom_half_work_pp);
	}
}

void vr_group_delete(struct vr_group *group)
{
	u32 i;

	VR_DEBUG_PRINT(4, ("Deleting group %p\n", group));

	VR_DEBUG_ASSERT(NULL == group->parent_group);

	/* Delete the resources that this group owns */
	if (NULL != group->gp_core)
	{
		vr_gp_delete(group->gp_core);
	}

	if (NULL != group->pp_core)
	{
		vr_pp_delete(group->pp_core);
	}

	if (NULL != group->mmu)
	{
		vr_mmu_delete(group->mmu);
	}

	if (vr_group_is_virtual(group))
	{
		/* Remove all groups from virtual group */
		struct vr_group *child;
		struct vr_group *temp;

		_VR_OSK_LIST_FOREACHENTRY(child, temp, &group->group_list, struct vr_group, group_list)
		{
			child->parent_group = NULL;
			vr_group_delete(child);
		}

		vr_dlbu_delete(group->dlbu_core);

		if (NULL != group->bcast_core)
		{
			vr_bcast_unit_delete(group->bcast_core);
		}
	}

	for (i = 0; i < VR_MAX_NUMBER_OF_GROUPS; i++)
	{
		if (vr_global_groups[i] == group)
		{
			vr_global_groups[i] = NULL;
			vr_global_num_groups--;

			if (i != vr_global_num_groups)
			{
				/* We removed a group from the middle of the array -- move the last
				 * group to the current position to close the gap */
				vr_global_groups[i] = vr_global_groups[vr_global_num_groups];
				vr_global_groups[vr_global_num_groups] = NULL;
			}

			break;
		}
	}

	if (NULL != group->timeout_timer)
	{
		_vr_osk_timer_del(group->timeout_timer);
		_vr_osk_timer_term(group->timeout_timer);
	}

	if (NULL != group->bottom_half_work_mmu)
	{
		_vr_osk_wq_delete_work(group->bottom_half_work_mmu);
	}

	if (NULL != group->bottom_half_work_gp)
	{
		_vr_osk_wq_delete_work(group->bottom_half_work_gp);
	}

	if (NULL != group->bottom_half_work_pp)
	{
		_vr_osk_wq_delete_work(group->bottom_half_work_pp);
	}

	_vr_osk_lock_term(group->lock);

	_vr_osk_free(group);
}

VR_DEBUG_CODE(static void vr_group_print_virtual(struct vr_group *vgroup)
{
	u32 i;
	struct vr_group *group;
	struct vr_group *temp;

	VR_DEBUG_PRINT(4, ("Virtual group %p\n", vgroup));
	VR_DEBUG_PRINT(4, ("l2_cache_core[0] = %p, ref = %d\n", vgroup->l2_cache_core[0], vgroup->l2_cache_core_ref_count[0]));
	VR_DEBUG_PRINT(4, ("l2_cache_core[1] = %p, ref = %d\n", vgroup->l2_cache_core[1], vgroup->l2_cache_core_ref_count[1]));

	i = 0;
	_VR_OSK_LIST_FOREACHENTRY(group, temp, &vgroup->group_list, struct vr_group, group_list)
	{
		VR_DEBUG_PRINT(4, ("[%d] %p, l2_cache_core[0] = %p\n", i, group, group->l2_cache_core[0]));
		i++;
	}
})

/**
 * @brief Add child group to virtual group parent
 *
 * Before calling this function, child must have it's state set to JOINING_VIRTUAL
 * to ensure it's not touched during the transition period. When this function returns,
 * child's state will be IN_VIRTUAL.
 */
void vr_group_add_group(struct vr_group *parent, struct vr_group *child)
{
	vr_bool found;
	u32 i;

	VR_DEBUG_PRINT(3, ("Adding group %p to virtual group %p\n", child, parent));

	VR_ASSERT_GROUP_LOCKED(parent);

	VR_DEBUG_ASSERT(vr_group_is_virtual(parent));
	VR_DEBUG_ASSERT(!vr_group_is_virtual(child));
	VR_DEBUG_ASSERT(NULL == child->parent_group);
	VR_DEBUG_ASSERT(VR_GROUP_STATE_JOINING_VIRTUAL == child->state);

	_vr_osk_list_addtail(&child->group_list, &parent->group_list);

	child->state = VR_GROUP_STATE_IN_VIRTUAL;
	child->parent_group = parent;

	VR_DEBUG_ASSERT_POINTER(child->l2_cache_core[0]);

	VR_DEBUG_PRINT(4, ("parent->l2_cache_core: [0] = %p, [1] = %p\n", parent->l2_cache_core[0], parent->l2_cache_core[1]));
	VR_DEBUG_PRINT(4, ("child->l2_cache_core: [0] = %p, [1] = %p\n", child->l2_cache_core[0], child->l2_cache_core[1]));

	/* Keep track of the L2 cache cores of child groups */
	found = VR_FALSE;
	for (i = 0; i < 2; i++)
	{
		if (parent->l2_cache_core[i] == child->l2_cache_core[0])
		{
			VR_DEBUG_ASSERT(parent->l2_cache_core_ref_count[i] > 0);
			parent->l2_cache_core_ref_count[i]++;
			found = VR_TRUE;
		}
	}

	if (!found)
	{
		/* First time we see this L2 cache, add it to our list */
		i = (NULL == parent->l2_cache_core[0]) ? 0 : 1;

		VR_DEBUG_PRINT(4, ("First time we see l2_cache %p. Adding to [%d] = %p\n", child->l2_cache_core[0], i, parent->l2_cache_core[i]));

		VR_DEBUG_ASSERT(NULL == parent->l2_cache_core[i]);

		parent->l2_cache_core[i] = child->l2_cache_core[0];
		parent->l2_cache_core_ref_count[i]++;
	}

	/* Update Broadcast Unit and DLBU */
	vr_bcast_add_group(parent->bcast_core, child);
	vr_dlbu_add_group(parent->dlbu_core, child);

	/* Update MMU */
	VR_DEBUG_ASSERT(0 == child->page_dir_ref_count);
	if (parent->session == child->session)
	{
		vr_mmu_zap_tlb(child->mmu);
	}
	else
	{
		child->session = NULL;

		if (NULL == parent->session)
		{
			vr_mmu_activate_empty_page_directory(child->mmu);
		}
		else
		{

			vr_bool activate_success = vr_mmu_activate_page_directory(child->mmu,
			        vr_session_get_page_directory(parent->session));
			VR_DEBUG_ASSERT(activate_success);
			VR_IGNORE(activate_success);
		}
	}
	child->session = NULL;

	/* Start job on child when parent is active */
	if (NULL != parent->pp_running_job)
	{
		struct vr_pp_job *job = parent->pp_running_job;
		VR_DEBUG_PRINT(3, ("Group %x joining running job %d on virtual group %x\n",
		                     child, vr_pp_job_get_id(job), parent));
		VR_DEBUG_ASSERT(VR_GROUP_STATE_WORKING == parent->state);
		vr_pp_job_start(child->pp_core, job, vr_pp_core_get_id(child->pp_core), VR_TRUE);

		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE|
		                              VR_PROFILING_MAKE_EVENT_CHANNEL_PP(vr_pp_core_get_id(child->pp_core))|
		                              VR_PROFILING_EVENT_REASON_SINGLE_HW_FLUSH,
		                              vr_pp_job_get_frame_builder_id(job), vr_pp_job_get_flush_id(job), 0, 0, 0);

		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_START|
		                              VR_PROFILING_MAKE_EVENT_CHANNEL_PP(vr_pp_core_get_id(child->pp_core))|
		                              VR_PROFILING_EVENT_REASON_START_STOP_HW_VIRTUAL,
		                              vr_pp_job_get_pid(job), vr_pp_job_get_tid(job), 0, 0, 0);
	}

	VR_DEBUG_CODE(vr_group_print_virtual(parent);)
}

/**
 * @brief Remove child group from virtual group parent
 *
 * After the child is removed, it's state will be LEAVING_VIRTUAL and must be set
 * to IDLE before it can be used.
 */
void vr_group_remove_group(struct vr_group *parent, struct vr_group *child)
{
	u32 i;

	VR_ASSERT_GROUP_LOCKED(parent);

	VR_DEBUG_PRINT(3, ("Removing group %p from virtual group %p\n", child, parent));

	VR_DEBUG_ASSERT(vr_group_is_virtual(parent));
	VR_DEBUG_ASSERT(!vr_group_is_virtual(child));
	VR_DEBUG_ASSERT(parent == child->parent_group);
	VR_DEBUG_ASSERT(VR_GROUP_STATE_IN_VIRTUAL == child->state);
	/* Removing groups while running is not yet supported. */
	VR_DEBUG_ASSERT(VR_GROUP_STATE_IDLE == parent->state);

	vr_group_lock(child);

	/* Update Broadcast Unit and DLBU */
	vr_bcast_remove_group(parent->bcast_core, child);
	vr_dlbu_remove_group(parent->dlbu_core, child);

	_vr_osk_list_delinit(&child->group_list);

	child->session = parent->session;
	child->parent_group = NULL;
	child->state = VR_GROUP_STATE_LEAVING_VIRTUAL;

	/* Keep track of the L2 cache cores of child groups */
	i = (child->l2_cache_core[0] == parent->l2_cache_core[0]) ? 0 : 1;

	VR_DEBUG_ASSERT(child->l2_cache_core[0] == parent->l2_cache_core[i]);

	parent->l2_cache_core_ref_count[i]--;

	if (parent->l2_cache_core_ref_count[i] == 0)
	{
		parent->l2_cache_core[i] = NULL;
	}

	VR_DEBUG_CODE(vr_group_print_virtual(parent));

	vr_group_unlock(child);
}

struct vr_group *vr_group_acquire_group(struct vr_group *parent)
{
	struct vr_group *child;

	VR_ASSERT_GROUP_LOCKED(parent);

	VR_DEBUG_ASSERT(vr_group_is_virtual(parent));
	VR_DEBUG_ASSERT(!_vr_osk_list_empty(&parent->group_list));

	child = _VR_OSK_LIST_ENTRY(parent->group_list.prev, struct vr_group, group_list);

	vr_group_remove_group(parent, child);

	return child;
}

void vr_group_reset(struct vr_group *group)
{
	/*
	 * This function should not be used to abort jobs,
	 * currently only called during insmod and PM resume
	 */
	VR_DEBUG_ASSERT(NULL == group->gp_running_job);
	VR_DEBUG_ASSERT(NULL == group->pp_running_job);

	vr_group_lock(group);

	group->session = NULL;

	if (NULL != group->mmu)
	{
		vr_mmu_reset(group->mmu);
	}

	if (NULL != group->gp_core)
	{
		vr_gp_reset(group->gp_core);
	}

	if (NULL != group->pp_core)
	{
		vr_group_reset_pp(group);
	}

	vr_group_unlock(group);
}

struct vr_gp_core* vr_group_get_gp_core(struct vr_group *group)
{
	return group->gp_core;
}

struct vr_pp_core* vr_group_get_pp_core(struct vr_group *group)
{
	return group->pp_core;
}

_vr_osk_errcode_t vr_group_start_gp_job(struct vr_group *group, struct vr_gp_job *job)
{
	struct vr_session_data *session;
	enum vr_group_activate_pd_status activate_status;

	VR_ASSERT_GROUP_LOCKED(group);
	VR_DEBUG_ASSERT(VR_GROUP_STATE_IDLE == group->state);

	session = vr_gp_job_get_session(job);

	if (NULL != group->l2_cache_core[0])
	{
		vr_l2_cache_invalidate_all_conditional(group->l2_cache_core[0], vr_gp_job_get_id(job));
	}

	activate_status = vr_group_activate_page_directory(group, session);
	if (VR_GROUP_ACTIVATE_PD_STATUS_FAILED != activate_status)
	{
		/* if session is NOT kept Zapping is done as part of session switch */
		if (VR_GROUP_ACTIVATE_PD_STATUS_OK_KEPT_PD == activate_status)
		{
			vr_mmu_zap_tlb_without_stall(group->mmu);
		}
		vr_gp_job_start(group->gp_core, job);

		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE |
				VR_PROFILING_MAKE_EVENT_CHANNEL_GP(0) |
				VR_PROFILING_EVENT_REASON_SINGLE_HW_FLUSH,
		        vr_gp_job_get_frame_builder_id(job), vr_gp_job_get_flush_id(job), 0, 0, 0);
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_START |
				VR_PROFILING_MAKE_EVENT_CHANNEL_GP(0),
				vr_gp_job_get_pid(job), vr_gp_job_get_tid(job), 0, 0, 0);
#if defined(CONFIG_VR400_PROFILING)
		if ((VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src0(group->l2_cache_core[0])) &&
				(VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src1(group->l2_cache_core[0])))
			vr_group_report_l2_cache_counters_per_core(group, 0);
#endif /* #if defined(CONFIG_VR400_PROFILING) */

		group->gp_running_job = job;
		group->state = VR_GROUP_STATE_WORKING;

		/* Setup the timeout timer value and save the job id for the job running on the gp core */
		_vr_osk_timer_mod(group->timeout_timer, _vr_osk_time_mstoticks(vr_max_job_runtime));

		return _VR_OSK_ERR_OK;
	}

	return _VR_OSK_ERR_FAULT;
}

_vr_osk_errcode_t vr_group_start_pp_job(struct vr_group *group, struct vr_pp_job *job, u32 sub_job)
{
	struct vr_session_data *session;
	enum vr_group_activate_pd_status activate_status;

	VR_ASSERT_GROUP_LOCKED(group);
	VR_DEBUG_ASSERT(VR_GROUP_STATE_IDLE == group->state);

	session = vr_pp_job_get_session(job);

	if (NULL != group->l2_cache_core[0])
	{
		vr_l2_cache_invalidate_all_conditional(group->l2_cache_core[0], vr_pp_job_get_id(job));
	}

	if (NULL != group->l2_cache_core[1])
	{
		vr_l2_cache_invalidate_all_conditional(group->l2_cache_core[1], vr_pp_job_get_id(job));
	}

	activate_status = vr_group_activate_page_directory(group, session);
	if (VR_GROUP_ACTIVATE_PD_STATUS_FAILED != activate_status)
	{
		/* if session is NOT kept Zapping is done as part of session switch */
		if (VR_GROUP_ACTIVATE_PD_STATUS_OK_KEPT_PD == activate_status)
		{
			VR_DEBUG_PRINT(3, ("PP starting job PD_Switch 0 Flush 1 Zap 1\n"));
			vr_mmu_zap_tlb_without_stall(group->mmu);
		}

		if (vr_group_is_virtual(group))
		{
			struct vr_group *child;
			struct vr_group *temp;
			u32 core_num = 0;

			/* Configure DLBU for the job */
			vr_dlbu_config_job(group->dlbu_core, job);

			/* Write stack address for each child group */
			_VR_OSK_LIST_FOREACHENTRY(child, temp, &group->group_list, struct vr_group, group_list)
			{
				vr_pp_write_addr_stack(child->pp_core, job);
				core_num++;
			}
		}

		vr_pp_job_start(group->pp_core, job, sub_job, VR_FALSE);

		/* if the group is virtual, loop through physical groups which belong to this group
		 * and call profiling events for its cores as virtual */
		if (VR_TRUE == vr_group_is_virtual(group))
		{
			struct vr_group *child;
			struct vr_group *temp;

			_VR_OSK_LIST_FOREACHENTRY(child, temp, &group->group_list, struct vr_group, group_list)
			{
				_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE|
				                              VR_PROFILING_MAKE_EVENT_CHANNEL_PP(vr_pp_core_get_id(child->pp_core))|
				                              VR_PROFILING_EVENT_REASON_SINGLE_HW_FLUSH,
				                              vr_pp_job_get_frame_builder_id(job), vr_pp_job_get_flush_id(job), 0, 0, 0);

				_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_START|
				                              VR_PROFILING_MAKE_EVENT_CHANNEL_PP(vr_pp_core_get_id(child->pp_core))|
				                              VR_PROFILING_EVENT_REASON_START_STOP_HW_VIRTUAL,
				                              vr_pp_job_get_pid(job), vr_pp_job_get_tid(job), 0, 0, 0);
			}
#if defined(CONFIG_VR400_PROFILING)
			if (0 != group->l2_cache_core_ref_count[0])
			{
				if ((VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src0(group->l2_cache_core[0])) &&
									(VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src1(group->l2_cache_core[0])))
				{
					vr_group_report_l2_cache_counters_per_core(group, vr_l2_cache_get_id(group->l2_cache_core[0]));
				}
			}
			if (0 != group->l2_cache_core_ref_count[1])
			{
				if ((VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src0(group->l2_cache_core[1])) &&
									(VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src1(group->l2_cache_core[1])))
				{
					vr_group_report_l2_cache_counters_per_core(group, vr_l2_cache_get_id(group->l2_cache_core[1]));
				}
			}
#endif /* #if defined(CONFIG_VR400_PROFILING) */
		}
		else /* group is physical - call profiling events for physical cores */
		{
			_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE|
			                              VR_PROFILING_MAKE_EVENT_CHANNEL_PP(vr_pp_core_get_id(group->pp_core))|
			                              VR_PROFILING_EVENT_REASON_SINGLE_HW_FLUSH,
			                              vr_pp_job_get_frame_builder_id(job), vr_pp_job_get_flush_id(job), 0, 0, 0);

			_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_START|
			                              VR_PROFILING_MAKE_EVENT_CHANNEL_PP(vr_pp_core_get_id(group->pp_core))|
			                              VR_PROFILING_EVENT_REASON_START_STOP_HW_PHYSICAL,
			                              vr_pp_job_get_pid(job), vr_pp_job_get_tid(job), 0, 0, 0);
#if defined(CONFIG_VR400_PROFILING)
			if ((VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src0(group->l2_cache_core[0])) &&
					(VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src1(group->l2_cache_core[0])))
			{
				vr_group_report_l2_cache_counters_per_core(group, vr_l2_cache_get_id(group->l2_cache_core[0]));
			}
#endif /* #if defined(CONFIG_VR400_PROFILING) */
		}
		group->pp_running_job = job;
		group->pp_running_sub_job = sub_job;
		group->state = VR_GROUP_STATE_WORKING;

		/* Setup the timeout timer value and save the job id for the job running on the pp core */
		_vr_osk_timer_mod(group->timeout_timer, _vr_osk_time_mstoticks(vr_max_job_runtime));

		return _VR_OSK_ERR_OK;
	}

	return _VR_OSK_ERR_FAULT;
}

struct vr_gp_job *vr_group_resume_gp_with_new_heap(struct vr_group *group, u32 job_id, u32 start_addr, u32 end_addr)
{
	VR_ASSERT_GROUP_LOCKED(group);

	if (group->state != VR_GROUP_STATE_OOM ||
	    vr_gp_job_get_id(group->gp_running_job) != job_id)
	{
		return NULL; /* Illegal request or job has already been aborted */
	}

	if (NULL != group->l2_cache_core[0])
	{
		vr_l2_cache_invalidate_all_force(group->l2_cache_core[0]);
	}

	vr_mmu_zap_tlb_without_stall(group->mmu);

	vr_gp_resume_with_new_heap(group->gp_core, start_addr, end_addr);

	_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_RESUME|VR_PROFILING_MAKE_EVENT_CHANNEL_GP(0), 0, 0, 0, 0, 0);

	group->state = VR_GROUP_STATE_WORKING;

	return group->gp_running_job;
}

static void vr_group_reset_pp(struct vr_group *group)
{
	struct vr_group *child;
	struct vr_group *temp;

	/* TODO: If we *know* that the group is idle, this could be faster. */

	vr_pp_reset_async(group->pp_core);

	if (!vr_group_is_virtual(group) || NULL == group->pp_running_job)
	{
		/* This is a physical group or an idle virtual group -- simply wait for
		 * the reset to complete. */
		vr_pp_reset_wait(group->pp_core);
	}
	else /* virtual group */
	{
		/* Loop through all members of this virtual group and wait until they
		 * are done resetting.
		 */
		_VR_OSK_LIST_FOREACHENTRY(child, temp, &group->group_list, struct vr_group, group_list)
		{
			vr_pp_reset_wait(child->pp_core);
		}
	}
}

static void vr_group_complete_pp(struct vr_group *group, vr_bool success)
{
	struct vr_pp_job *pp_job_to_return;
	u32 pp_sub_job_to_return;

	VR_DEBUG_ASSERT_POINTER(group->pp_core);
	VR_DEBUG_ASSERT_POINTER(group->pp_running_job);
	VR_ASSERT_GROUP_LOCKED(group);

	vr_group_post_process_job_pp(group);

	vr_pp_reset_async(group->pp_core);

	pp_job_to_return = group->pp_running_job;
	pp_sub_job_to_return = group->pp_running_sub_job;
	group->state = VR_GROUP_STATE_IDLE;
	group->pp_running_job = NULL;

	vr_group_deactivate_page_directory(group, group->session);

	if (_VR_OSK_ERR_OK != vr_pp_reset_wait(group->pp_core))
	{
		VR_DEBUG_PRINT(3, ("VR group: Failed to reset PP, need to reset entire group\n"));

		vr_group_recovery_reset(group);
	}

	vr_pp_scheduler_job_done(group, pp_job_to_return, pp_sub_job_to_return, success);
}

static void vr_group_complete_gp(struct vr_group *group, vr_bool success)
{
	struct vr_gp_job *gp_job_to_return;

	VR_DEBUG_ASSERT_POINTER(group->gp_core);
	VR_DEBUG_ASSERT_POINTER(group->gp_running_job);
	VR_ASSERT_GROUP_LOCKED(group);

	vr_group_post_process_job_gp(group, VR_FALSE);

	vr_gp_reset_async(group->gp_core);

	gp_job_to_return = group->gp_running_job;
	group->state = VR_GROUP_STATE_IDLE;
	group->gp_running_job = NULL;

	vr_group_deactivate_page_directory(group, group->session);

	if (_VR_OSK_ERR_OK != vr_gp_reset_wait(group->gp_core))
	{
		VR_DEBUG_PRINT(3, ("VR group: Failed to reset GP, need to reset entire group\n"));

		vr_group_recovery_reset(group);
	}

	vr_gp_scheduler_job_done(group, gp_job_to_return, success);
}

void vr_group_abort_gp_job(struct vr_group *group, u32 job_id)
{
	VR_ASSERT_GROUP_LOCKED(group);

	if (group->state == VR_GROUP_STATE_IDLE ||
	    vr_gp_job_get_id(group->gp_running_job) != job_id)
	{
		return; /* No need to cancel or job has already been aborted or completed */
	}

	vr_group_complete_gp(group, VR_FALSE);
}

static void vr_group_abort_pp_job(struct vr_group *group, u32 job_id)
{
	VR_ASSERT_GROUP_LOCKED(group);

	if (group->state == VR_GROUP_STATE_IDLE ||
	    vr_pp_job_get_id(group->pp_running_job) != job_id)
	{
		return; /* No need to cancel or job has already been aborted or completed */
	}

	vr_group_complete_pp(group, VR_FALSE);
}

void vr_group_abort_session(struct vr_group *group, struct vr_session_data *session)
{
	struct vr_gp_job *gp_job;
	struct vr_pp_job *pp_job;
	u32 gp_job_id = 0;
	u32 pp_job_id = 0;
	vr_bool abort_pp = VR_FALSE;
	vr_bool abort_gp = VR_FALSE;

	vr_group_lock(group);

	if (vr_group_is_in_virtual(group))
	{
		/* Group is member of a virtual group, don't touch it! */
		vr_group_unlock(group);
		return;
	}

	gp_job = group->gp_running_job;
	pp_job = group->pp_running_job;

	if ((NULL != gp_job) && (vr_gp_job_get_session(gp_job) == session))
	{
		VR_DEBUG_PRINT(4, ("Aborting GP job 0x%08x from session 0x%08x\n", gp_job, session));

		gp_job_id = vr_gp_job_get_id(gp_job);
		abort_gp = VR_TRUE;
	}

	if ((NULL != pp_job) && (vr_pp_job_get_session(pp_job) == session))
	{
		VR_DEBUG_PRINT(4, ("VR group: Aborting PP job 0x%08x from session 0x%08x\n", pp_job, session));

		pp_job_id = vr_pp_job_get_id(pp_job);
		abort_pp = VR_TRUE;
	}

	if (abort_gp)
	{
		vr_group_abort_gp_job(group, gp_job_id);
	}
	if (abort_pp)
	{
		vr_group_abort_pp_job(group, pp_job_id);
	}

	vr_group_remove_session_if_unused(group, session);

	vr_group_unlock(group);
}

struct vr_group *vr_group_get_glob_group(u32 index)
{
	if(vr_global_num_groups > index)
	{
		return vr_global_groups[index];
	}

	return NULL;
}

u32 vr_group_get_glob_num_groups(void)
{
	return vr_global_num_groups;
}

static enum vr_group_activate_pd_status vr_group_activate_page_directory(struct vr_group *group, struct vr_session_data *session)
{
	enum vr_group_activate_pd_status retval;
	VR_ASSERT_GROUP_LOCKED(group);

	VR_DEBUG_PRINT(5, ("VR group: Activating page directory 0x%08X from session 0x%08X on group 0x%08X\n", vr_session_get_page_directory(session), session, group));
	VR_DEBUG_ASSERT(0 <= group->page_dir_ref_count);

	if (0 != group->page_dir_ref_count)
	{
		if (group->session != session)
		{
			VR_DEBUG_PRINT(4, ("VR group: Activating session FAILED: 0x%08x on group 0x%08X. Existing session: 0x%08x\n", session, group, group->session));
			return VR_GROUP_ACTIVATE_PD_STATUS_FAILED;
		}
		else
		{
			VR_DEBUG_PRINT(4, ("VR group: Activating session already activated: 0x%08x on group 0x%08X. New Ref: %d\n", session, group, 1+group->page_dir_ref_count));
			retval = VR_GROUP_ACTIVATE_PD_STATUS_OK_KEPT_PD;

		}
	}
	else
	{
		/* There might be another session here, but it is ok to overwrite it since group->page_dir_ref_count==0 */
		if (group->session != session)
		{
			vr_bool activate_success;
			VR_DEBUG_PRINT(5, ("VR group: Activate session: %08x previous: %08x on group 0x%08X. Ref: %d\n", session, group->session, group, 1+group->page_dir_ref_count));

			activate_success = vr_mmu_activate_page_directory(group->mmu, vr_session_get_page_directory(session));
			VR_DEBUG_ASSERT(activate_success);
			if ( VR_FALSE== activate_success ) return VR_GROUP_ACTIVATE_PD_STATUS_FAILED;
			group->session = session;
			retval = VR_GROUP_ACTIVATE_PD_STATUS_OK_SWITCHED_PD;
		}
		else
		{
			VR_DEBUG_PRINT(4, ("VR group: Activate existing session 0x%08X on group 0x%08X. Ref: %d\n", session->page_directory, group, 1+group->page_dir_ref_count));
			retval = VR_GROUP_ACTIVATE_PD_STATUS_OK_KEPT_PD;
		}
	}

	group->page_dir_ref_count++;
	return retval;
}

static void vr_group_deactivate_page_directory(struct vr_group *group, struct vr_session_data *session)
{
	VR_ASSERT_GROUP_LOCKED(group);

	VR_DEBUG_ASSERT(0 < group->page_dir_ref_count);
	VR_DEBUG_ASSERT(session == group->session);

	group->page_dir_ref_count--;

	/* As an optimization, the MMU still points to the group->session even if (0 == group->page_dir_ref_count),
	   and we do not call vr_mmu_activate_empty_page_directory(group->mmu); */
	VR_DEBUG_ASSERT(0 <= group->page_dir_ref_count);
}

static void vr_group_remove_session_if_unused(struct vr_group *group, struct vr_session_data *session)
{
	VR_ASSERT_GROUP_LOCKED(group);

	if (0 == group->page_dir_ref_count)
	{
		VR_DEBUG_ASSERT(VR_GROUP_STATE_WORKING != group->state);

		if (group->session == session)
		{
			VR_DEBUG_ASSERT(VR_TRUE == group->power_is_on);
			VR_DEBUG_PRINT(3, ("VR group: Deactivating unused session 0x%08X on group %08X\n", session, group));
			vr_mmu_activate_empty_page_directory(group->mmu);
			group->session = NULL;
		}
	}
}

void vr_group_power_on(void)
{
	int i;
	for (i = 0; i < vr_global_num_groups; i++)
	{
		struct vr_group *group = vr_global_groups[i];
		vr_group_lock(group);
		VR_DEBUG_ASSERT(VR_GROUP_STATE_IDLE == group->state);
		group->power_is_on = VR_TRUE;

		if (NULL != group->l2_cache_core[0])
		{
			vr_l2_cache_power_is_enabled_set(group->l2_cache_core[0], VR_TRUE);
		}

		if (NULL != group->l2_cache_core[1])
		{
			vr_l2_cache_power_is_enabled_set(group->l2_cache_core[1], VR_TRUE);
		}

		vr_group_unlock(group);
	}
	VR_DEBUG_PRINT(4,("group: POWER ON\n"));
}

vr_bool vr_group_power_is_on(struct vr_group *group)
{
	VR_ASSERT_GROUP_LOCKED(group);
	return group->power_is_on;
}

void vr_group_power_off(void)
{
	int i;
	/* It is necessary to set group->session = NULL; so that the powered off MMU is not written to on map /unmap */
	/* It is necessary to set group->power_is_on=VR_FALSE so that pending bottom_halves does not access powered off cores. */
	for (i = 0; i < vr_global_num_groups; i++)
	{
		struct vr_group *group = vr_global_groups[i];
		vr_group_lock(group);
		VR_DEBUG_ASSERT(VR_GROUP_STATE_IDLE == group->state);
		group->session = NULL;
		group->power_is_on = VR_FALSE;

		if (NULL != group->l2_cache_core[0])
		{
			vr_l2_cache_power_is_enabled_set(group->l2_cache_core[0], VR_FALSE);
		}

		if (NULL != group->l2_cache_core[1])
		{
			vr_l2_cache_power_is_enabled_set(group->l2_cache_core[1], VR_FALSE);
		}

		vr_group_unlock(group);
	}
	VR_DEBUG_PRINT(4,("group: POWER OFF\n"));
}


static void vr_group_recovery_reset(struct vr_group *group)
{
	VR_ASSERT_GROUP_LOCKED(group);

	/* Stop cores, bus stop */
	if (NULL != group->pp_core)
	{
		vr_pp_stop_bus(group->pp_core);
	}
	else
	{
		vr_gp_stop_bus(group->gp_core);
	}

	/* Flush MMU and clear page fault (if any) */
	vr_mmu_activate_fault_flush_page_directory(group->mmu);
	vr_mmu_page_fault_done(group->mmu);

	/* Wait for cores to stop bus, then do a hard reset on them */
	if (NULL != group->pp_core)
	{
		if (vr_group_is_virtual(group))
		{
			struct vr_group *child, *temp;

			_VR_OSK_LIST_FOREACHENTRY(child, temp, &group->group_list, struct vr_group, group_list)
			{
				vr_pp_stop_bus_wait(child->pp_core);
				vr_pp_hard_reset(child->pp_core);
			}
		}
		else
		{
			vr_pp_stop_bus_wait(group->pp_core);
			vr_pp_hard_reset(group->pp_core);
		}
	}
	else
	{
		vr_gp_stop_bus_wait(group->gp_core);
		vr_gp_hard_reset(group->gp_core);
	}

	/* Reset MMU */
	vr_mmu_reset(group->mmu);
	group->session = NULL;
}

#if VR_STATE_TRACKING
u32 vr_group_dump_state(struct vr_group *group, char *buf, u32 size)
{
	int n = 0;

	n += _vr_osk_snprintf(buf + n, size - n, "Group: %p\n", group);
	n += _vr_osk_snprintf(buf + n, size - n, "\tstate: %d\n", group->state);
	if (group->gp_core)
	{
		n += vr_gp_dump_state(group->gp_core, buf + n, size - n);
		n += _vr_osk_snprintf(buf + n, size - n, "\tGP job: %p\n", group->gp_running_job);
	}
	if (group->pp_core)
	{
		n += vr_pp_dump_state(group->pp_core, buf + n, size - n);
		n += _vr_osk_snprintf(buf + n, size - n, "\tPP job: %p, subjob %d \n",
		                        group->pp_running_job, group->pp_running_sub_job);
	}

	return n;
}
#endif

static void vr_group_mmu_page_fault(struct vr_group *group)
{
	VR_ASSERT_GROUP_LOCKED(group);

	if (NULL != group->pp_core)
	{
		struct vr_pp_job *pp_job_to_return;
		u32 pp_sub_job_to_return;

		VR_DEBUG_ASSERT_POINTER(group->pp_running_job);

		vr_group_post_process_job_pp(group);

		pp_job_to_return = group->pp_running_job;
		pp_sub_job_to_return = group->pp_running_sub_job;
		group->state = VR_GROUP_STATE_IDLE;
		group->pp_running_job = NULL;

		vr_group_deactivate_page_directory(group, group->session);

		vr_group_recovery_reset(group); /* This will also clear the page fault itself */

		vr_pp_scheduler_job_done(group, pp_job_to_return, pp_sub_job_to_return, VR_FALSE);
	}
	else
	{
		struct vr_gp_job *gp_job_to_return;

		VR_DEBUG_ASSERT_POINTER(group->gp_running_job);

		vr_group_post_process_job_gp(group, VR_FALSE);

		gp_job_to_return = group->gp_running_job;
		group->state = VR_GROUP_STATE_IDLE;
		group->gp_running_job = NULL;

		vr_group_deactivate_page_directory(group, group->session);

		vr_group_recovery_reset(group); /* This will also clear the page fault itself */

		vr_gp_scheduler_job_done(group, gp_job_to_return, VR_FALSE);
	}
}

_vr_osk_errcode_t vr_group_upper_half_mmu(void * data)
{
	struct vr_group *group = (struct vr_group *)data;
	struct vr_mmu_core *mmu = group->mmu;
	u32 int_stat;

	VR_DEBUG_ASSERT_POINTER(mmu);

	/* Check if it was our device which caused the interrupt (we could be sharing the IRQ line) */
	int_stat = vr_mmu_get_int_status(mmu);
	if (0 != int_stat)
	{
		struct vr_group *parent = group->parent_group;

		/* page fault or bus error, we thread them both in the same way */
		vr_mmu_mask_all_interrupts(mmu);
		if (NULL == parent)
		{
			_vr_osk_wq_schedule_work(group->bottom_half_work_mmu);
		}
		else
		{
			_vr_osk_wq_schedule_work(parent->bottom_half_work_mmu);
		}
		return _VR_OSK_ERR_OK;
	}

	return _VR_OSK_ERR_FAULT;
}

static void vr_group_bottom_half_mmu(void * data)
{
	struct vr_group *group = (struct vr_group *)data;
	struct vr_mmu_core *mmu = group->mmu;
	u32 rawstat;
	u32 status;

	VR_DEBUG_ASSERT_POINTER(mmu);

	vr_group_lock(group);

	/* TODO: Remove some of these asserts? Will we ever end up in
	 * "physical" bottom half for a member of the virtual group? */
	VR_DEBUG_ASSERT(NULL == group->parent_group);
	VR_DEBUG_ASSERT(!vr_group_is_in_virtual(group));

	if ( VR_FALSE == vr_group_power_is_on(group) )
	{
		VR_PRINT_ERROR(("Interrupt bottom half of %s when core is OFF.", mmu->hw_core.description));
		vr_group_unlock(group);
		return;
	}

	rawstat = vr_mmu_get_rawstat(mmu);
	status = vr_mmu_get_status(mmu);

	VR_DEBUG_PRINT(4, ("VR MMU: Bottom half, interrupt 0x%08X, status 0x%08X\n", rawstat, status));

	if (rawstat & (VR_MMU_INTERRUPT_PAGE_FAULT | VR_MMU_INTERRUPT_READ_BUS_ERROR))
	{
		/* An actual page fault has occurred. */
		u32 fault_address = vr_mmu_get_page_fault_addr(mmu);
		VR_DEBUG_PRINT(2,("VR MMU: Page fault detected at 0x%x from bus id %d of type %s on %s\n",
		                 (void*)fault_address,
		                 (status >> 6) & 0x1F,
		                 (status & 32) ? "write" : "read",
		                 mmu->hw_core.description));
		VR_IGNORE(fault_address);

		vr_group_mmu_page_fault(group);
	}

	vr_group_unlock(group);
}

_vr_osk_errcode_t vr_group_upper_half_gp(void *data)
{
	struct vr_group *group = (struct vr_group *)data;
	struct vr_gp_core *core = group->gp_core;
	u32 irq_readout;

	irq_readout = vr_gp_get_int_stat(core);

	if (VRGP2_REG_VAL_IRQ_MASK_NONE != irq_readout)
	{
		/* Mask out all IRQs from this core until IRQ is handled */
		vr_gp_mask_all_interrupts(core);

		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE|VR_PROFILING_MAKE_EVENT_CHANNEL_GP(0)|VR_PROFILING_EVENT_REASON_SINGLE_HW_INTERRUPT, irq_readout, 0, 0, 0, 0);

		/* We do need to handle this in a bottom half */
		_vr_osk_wq_schedule_work(group->bottom_half_work_gp);
		return _VR_OSK_ERR_OK;
	}

	return _VR_OSK_ERR_FAULT;
}

//added by nexell
#include <asm/io.h>
#include <linux/clk.h>
#include <linux/delay.h>	/* mdelay */

#include <asm/mach/time.h>
#include <asm/mach/irq.h>
#include <asm/smp_twd.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#define PHY_BASEADDR_VR				(0xC0070000)
#define PHY_BASEADDR_VR_GP			(0xC0070000 + 0x0)
#define VR_PM_DBG PM_DBGOUT


static void vr_group_bottom_half_gp(void *data)
{
	struct vr_group *group = (struct vr_group *)data;
	u32 irq_readout;
	u32 irq_errors;

	_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_START|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF, 0, _vr_osk_get_tid(), VR_PROFILING_MAKE_EVENT_DATA_CORE_GP(0), 0, 0);

	vr_group_lock(group);

	if ( VR_FALSE == vr_group_power_is_on(group) )
	{
		VR_PRINT_ERROR(("VR group: Interrupt bottom half of %s when core is OFF.", vr_gp_get_hw_core_desc(group->gp_core)));
		vr_group_unlock(group);
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF, 0, _vr_osk_get_tid(), 0, 0, 0);
		return;
	}

	irq_readout = vr_gp_read_rawstat(group->gp_core);

	VR_DEBUG_PRINT(4, ("VR group: GP bottom half IRQ 0x%08X from core %s\n", irq_readout, vr_gp_get_hw_core_desc(group->gp_core)));

	if (irq_readout & (VRGP2_REG_VAL_IRQ_VS_END_CMD_LST|VRGP2_REG_VAL_IRQ_PLBU_END_CMD_LST))
	{
		u32 core_status = vr_gp_read_core_status(group->gp_core);
		if (0 == (core_status & VRGP2_REG_VAL_STATUS_MASK_ACTIVE))
		{
			VR_DEBUG_PRINT(4, ("VR group: GP job completed, calling group handler\n"));
			group->core_timed_out = VR_FALSE;
			_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
			                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
			                              VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF,
			                              0, _vr_osk_get_tid(), 0, 0, 0);
			vr_group_complete_gp(group, VR_TRUE);
			vr_group_unlock(group);
			return;
		}
	}

	/*
	 * Now lets look at the possible error cases (IRQ indicating error or timeout)
	 * END_CMD_LST, HANG and PLBU_OOM interrupts are not considered error.
	 */
	irq_errors = irq_readout & ~(VRGP2_REG_VAL_IRQ_VS_END_CMD_LST|VRGP2_REG_VAL_IRQ_PLBU_END_CMD_LST|VRGP2_REG_VAL_IRQ_HANG|VRGP2_REG_VAL_IRQ_PLBU_OUT_OF_MEM);

	/* added by nexell */
	if(irq_readout & VRGP2_REG_VAL_IRQ_AXI_BUS_ERROR)
	{		
		vr_hw_core_register_write(&group->gp_core->hw_core, VRGP2_REG_ADDR_MGMT_INT_CLEAR, VRGP2_REG_VAL_IRQ_AXI_BUS_ERROR);
		group->core_timed_out = VR_FALSE;
		VR_PM_DBG("ignore VRGP2_REG_VAL_IRQ_AXI_BUS_ERROR\n");
		
		#if 0
		{
			/* temp test */ 	
			u32 phys_addr_page, map_size;
			void *mem_mapped;
			//mask vr400 GP2 AXI_BUS_ERROR
			phys_addr_page = PHY_BASEADDR_VR_GP + 0x2C;
			map_size	   = sizeof(u32) * 2;
			mem_mapped = ioremap_nocache(phys_addr_page, map_size);
			if (NULL != mem_mapped)
			{
				unsigned int temp32 = ioread32(((u8*)mem_mapped));
				VR_PM_DBG("=> read GP2 INT Mask, addr(0x%x, 0x%x)\n", (int)mem_mapped, temp32);
				temp32 = ioread32(((u8*)mem_mapped + 4));
				VR_PM_DBG("=> read GP2 INT Mask, addr(0x%x, 0x%x)\n", (int)mem_mapped + 4, temp32);
				iounmap(mem_mapped);
			}
		}
		{
			/* temp test */ 	
			u32 phys_addr_page, map_size;
			void *mem_mapped;
			//mask vr400 GP2 AXI_BUS_ERROR
			phys_addr_page = PHY_BASEADDR_VR_GP + 0x6C;
			map_size	   = sizeof(u32);
			mem_mapped = ioremap_nocache(phys_addr_page, map_size);
			if (NULL != mem_mapped)
			{
				unsigned int temp32 = ioread32(((u8*)mem_mapped));
				VR_PM_DBG("=> read GP2 Version, addr(0x%x, 0x%x)\n", (int)mem_mapped, temp32);
				iounmap(mem_mapped);
			}
		}		
		#endif

		vr_group_complete_gp(group, VR_FALSE);		
		vr_group_unlock(group);
		return;
	}
	
	if (0 != irq_errors)
	{
		VR_PRINT_ERROR(("VR group: Unknown interrupt 0x%08X from core %s, aborting job\n", irq_readout, vr_gp_get_hw_core_desc(group->gp_core)));
		group->core_timed_out = VR_FALSE;
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
		                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
		                              VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF,
		                              0, _vr_osk_get_tid(), 0, 0, 0);

		vr_group_complete_gp(group, VR_FALSE);
		vr_group_unlock(group);
		return;
	}
	else if (group->core_timed_out) /* SW timeout */
	{
		group->core_timed_out = VR_FALSE;
		if (!_vr_osk_timer_pending(group->timeout_timer) && NULL != group->gp_running_job)
		{
			VR_PRINT(("VR group: Job %d timed out\n", vr_gp_job_get_id(group->gp_running_job)));
			vr_group_complete_gp(group, VR_FALSE);
			vr_group_unlock(group);
			return;
		}
	}
	else if (irq_readout & VRGP2_REG_VAL_IRQ_PLBU_OUT_OF_MEM)
	{
		/* GP wants more memory in order to continue. */
		VR_DEBUG_PRINT(3, ("VR group: PLBU needs more heap memory\n"));

		group->state = VR_GROUP_STATE_OOM;
		vr_group_unlock(group); /* Nothing to do on the HW side, so just release group lock right away */
		vr_gp_scheduler_oom(group, group->gp_running_job);
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF, 0, _vr_osk_get_tid(), 0, 0, 0);
		return;
	}

	/*
	 * The only way to get here is if we only got one of two needed END_CMD_LST
	 * interrupts. Enable all but not the complete interrupt that has been
	 * received and continue to run.
	 */
	vr_gp_enable_interrupts(group->gp_core, irq_readout & (VRGP2_REG_VAL_IRQ_PLBU_END_CMD_LST|VRGP2_REG_VAL_IRQ_VS_END_CMD_LST));
	vr_group_unlock(group);

	_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF, 0, _vr_osk_get_tid(), 0, 0, 0);
}

static void vr_group_post_process_job_gp(struct vr_group *group, vr_bool suspend)
{
	/* Stop the timeout timer. */
	_vr_osk_timer_del_async(group->timeout_timer);

	if (NULL == group->gp_running_job)
	{
		/* Nothing to do */
		return;
	}

	vr_gp_update_performance_counters(group->gp_core, group->gp_running_job, suspend);

#if defined(CONFIG_VR400_PROFILING)
	if (suspend)
	{
		/* @@@@ todo: test this case and see if it is still working*/
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_SUSPEND|VR_PROFILING_MAKE_EVENT_CHANNEL_GP(0),
		                              vr_gp_job_get_perf_counter_value0(group->gp_running_job),
		                              vr_gp_job_get_perf_counter_value1(group->gp_running_job),
		                              vr_gp_job_get_perf_counter_src0(group->gp_running_job) | (vr_gp_job_get_perf_counter_src1(group->gp_running_job) << 8),
		                              0, 0);
	}
	else
	{
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP|VR_PROFILING_MAKE_EVENT_CHANNEL_GP(0),
		                              vr_gp_job_get_perf_counter_value0(group->gp_running_job),
		                              vr_gp_job_get_perf_counter_value1(group->gp_running_job),
		                              vr_gp_job_get_perf_counter_src0(group->gp_running_job) | (vr_gp_job_get_perf_counter_src1(group->gp_running_job) << 8),
		                              0, 0);

		if ((VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src0(group->l2_cache_core[0])) &&
				(VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src1(group->l2_cache_core[0])))
			vr_group_report_l2_cache_counters_per_core(group, 0);
	}
#endif

	vr_gp_job_set_current_heap_addr(group->gp_running_job,
	                                  vr_gp_read_plbu_alloc_start_addr(group->gp_core));
}

_vr_osk_errcode_t vr_group_upper_half_pp(void *data)
{
	struct vr_group *group = (struct vr_group *)data;
	struct vr_pp_core *core = group->pp_core;
	u32 irq_readout;

	/*
	 * For VR-450 there is one particular case we need to watch out for:
	 *
	 * Criteria 1) this function call can be due to a shared interrupt,
	 * and not necessary because this core signaled an interrupt.
	 * Criteria 2) this core is a part of a virtual group, and thus it should
	 * not do any post processing.
	 * Criteria 3) this core has actually indicated that is has completed by
	 * having set raw_stat/int_stat registers to != 0
	 *
	 * If all this criteria is meet, then we could incorrectly start post
	 * processing on the wrong group object (this should only happen on the
	 * parent group)
	 */
#if !defined(VR_UPPER_HALF_SCHEDULING)
	if (vr_group_is_in_virtual(group))
	{
		/*
		 * This check is done without the group lock held, which could lead to
		 * a potential race. This is however ok, since we will safely re-check
		 * this with the group lock held at a later stage. This is just an
		 * early out which will strongly benefit shared IRQ systems.
		 */
		return _VR_OSK_ERR_OK;
	}
#endif

	irq_readout = vr_pp_get_int_stat(core);
	if (VR200_REG_VAL_IRQ_MASK_NONE != irq_readout)
	{
		/* Mask out all IRQs from this core until IRQ is handled */
		vr_pp_mask_all_interrupts(core);

#if defined(CONFIG_VR400_PROFILING)
		/* Currently no support for this interrupt event for the virtual PP core */
		if (!vr_group_is_virtual(group))
		{
			_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE |
			                              VR_PROFILING_MAKE_EVENT_CHANNEL_PP(core->core_id) |
			                              VR_PROFILING_EVENT_REASON_SINGLE_HW_INTERRUPT,
			                              irq_readout, 0, 0, 0, 0);
		}
#endif

#if defined(VR_UPPER_HALF_SCHEDULING)
		if (irq_readout & VR200_REG_VAL_IRQ_END_OF_FRAME)
		{
			_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_START |
			                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
			                              VR_PROFILING_EVENT_REASON_START_STOP_SW_UPPER_HALF,
			                              0, 0, VR_PROFILING_MAKE_EVENT_DATA_CORE_PP(core->core_id), 0, 0);

			VR_DEBUG_PRINT(3, ("VR PP: Job completed, calling group handler from upper half\n"));

			vr_group_lock(group);

			/* Read int stat again */
			irq_readout = vr_pp_read_rawstat(core);
			if (!(irq_readout & VR200_REG_VAL_IRQ_END_OF_FRAME))
			{
				/* There was nothing to do */
				vr_pp_enable_interrupts(core);
				vr_group_unlock(group);
				return _VR_OSK_ERR_OK;
			}

			if (vr_group_is_in_virtual(group))
			{
				/* We're member of a virtual group, so interrupt should be handled by the virtual group */
				vr_pp_enable_interrupts(core);
				vr_group_unlock(group);
				_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
				                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
				                              VR_PROFILING_EVENT_REASON_START_STOP_SW_UPPER_HALF,
				                              0, 0, VR_PROFILING_MAKE_EVENT_DATA_CORE_PP(core->core_id), 0, 0);
				return _VR_OSK_ERR_FAULT;
			}

			_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
			                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
			                              VR_PROFILING_EVENT_REASON_START_STOP_SW_UPPER_HALF,
			                              0, 0, VR_PROFILING_MAKE_EVENT_DATA_CORE_PP(core->core_id), 0, 0);

			vr_group_complete_pp(group, VR_TRUE);
			/* No need to enable interrupts again, since the core will be reset while completing the job */

			vr_group_unlock(group);

			return _VR_OSK_ERR_OK;
		}
#endif

		/* We do need to handle this in a bottom half */
		_vr_osk_wq_schedule_work(group->bottom_half_work_pp);
		return _VR_OSK_ERR_OK;
	}

	return _VR_OSK_ERR_FAULT;
}

static void vr_group_bottom_half_pp(void *data)
{
	struct vr_group *group = (struct vr_group *)data;
	struct vr_pp_core *core = group->pp_core;
	u32 irq_readout;
	u32 irq_errors;

	_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_START |
	                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
	                              VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF,
	                              0, _vr_osk_get_tid(), VR_PROFILING_MAKE_EVENT_DATA_CORE_PP(core->core_id), 0, 0);

	vr_group_lock(group);

	if (vr_group_is_in_virtual(group))
	{
		/* We're member of a virtual group, so interrupt should be handled by the virtual group */
		vr_pp_enable_interrupts(core);
		vr_group_unlock(group);
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
		                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
		                              VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF,
		                              0, _vr_osk_get_tid(), 0, 0, 0);
		return;
	}

	if ( VR_FALSE == vr_group_power_is_on(group) )
	{
		VR_PRINT_ERROR(("Interrupt bottom half of %s when core is OFF.", vr_pp_get_hw_core_desc(core)));
		vr_group_unlock(group);
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
		                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
		                              VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF,
		                              0, _vr_osk_get_tid(), 0, 0, 0);
		return;
	}

	irq_readout = vr_pp_read_rawstat(group->pp_core);

	VR_DEBUG_PRINT(4, ("VR PP: Bottom half IRQ 0x%08X from core %s\n", irq_readout, vr_pp_get_hw_core_desc(group->pp_core)));

	if (irq_readout & VR200_REG_VAL_IRQ_END_OF_FRAME)
	{
		VR_DEBUG_PRINT(3, ("VR PP: Job completed, calling group handler\n"));
		group->core_timed_out = VR_FALSE;
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
		                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
		                              VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF,
		                              0, _vr_osk_get_tid(), 0, 0, 0);
		vr_group_complete_pp(group, VR_TRUE);
		vr_group_unlock(group);
		return;
	}

	/*
	 * Now lets look at the possible error cases (IRQ indicating error or timeout)
	 * END_OF_FRAME and HANG interrupts are not considered error.
	 */
	irq_errors = irq_readout & ~(VR200_REG_VAL_IRQ_END_OF_FRAME|VR200_REG_VAL_IRQ_HANG);
	if (0 != irq_errors)
	{
		VR_PRINT_ERROR(("VR PP: Unknown interrupt 0x%08X from core %s, aborting job\n",
		                  irq_readout, vr_pp_get_hw_core_desc(group->pp_core)));
		group->core_timed_out = VR_FALSE;
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
		                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
		                              VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF,
		                              0, _vr_osk_get_tid(), 0, 0, 0);
		vr_group_complete_pp(group, VR_FALSE);
		vr_group_unlock(group);
		return;
	}
	else if (group->core_timed_out) /* SW timeout */
	{
		group->core_timed_out = VR_FALSE;
		_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
		                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
		                              VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF,
		                              0, _vr_osk_get_tid(), 0, 0, 0);
		if (!_vr_osk_timer_pending(group->timeout_timer) && NULL != group->pp_running_job)
		{
			VR_PRINT(("VR PP: Job %d timed out on core %s\n",
			            vr_pp_job_get_id(group->pp_running_job), vr_pp_get_hw_core_desc(core)));
			vr_group_complete_pp(group, VR_FALSE);
			vr_group_unlock(group);
		}
		else
		{
			vr_group_unlock(group);
		}
		return;
	}

	/*
	 * We should never get here, re-enable interrupts and continue
	 */
	if (0 == irq_readout)
	{
		VR_DEBUG_PRINT(3, ("VR group: No interrupt found on core %s\n",
		                    vr_pp_get_hw_core_desc(group->pp_core)));
	}
	else
	{
		VR_PRINT_ERROR(("VR group: Unhandled PP interrupt 0x%08X on %s\n", irq_readout,
		                    vr_pp_get_hw_core_desc(group->pp_core)));
	}
	vr_pp_enable_interrupts(core);
	vr_group_unlock(group);

	_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP |
	                              VR_PROFILING_EVENT_CHANNEL_SOFTWARE |
	                              VR_PROFILING_EVENT_REASON_START_STOP_SW_BOTTOM_HALF,
	                              0, _vr_osk_get_tid(), 0, 0, 0);
}

static void vr_group_post_process_job_pp(struct vr_group *group)
{
	VR_ASSERT_GROUP_LOCKED(group);

	/* Stop the timeout timer. */
	_vr_osk_timer_del_async(group->timeout_timer);

	/*todo add stop SW counters profiling*/

	if (NULL != group->pp_running_job)
	{
		if (VR_TRUE == vr_group_is_virtual(group))
		{
			struct vr_group *child;
			struct vr_group *temp;

			/* update performance counters from each physical pp core within this virtual group */
			_VR_OSK_LIST_FOREACHENTRY(child, temp, &group->group_list, struct vr_group, group_list)
			{
				vr_pp_update_performance_counters(child->pp_core, group->pp_running_job, group->pp_running_sub_job);
			}

#if defined(CONFIG_VR400_PROFILING)
			/* send profiling data per physical core */
			_VR_OSK_LIST_FOREACHENTRY(child, temp, &group->group_list, struct vr_group, group_list)
			{
				_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP|
				                              VR_PROFILING_MAKE_EVENT_CHANNEL_PP(vr_pp_core_get_id(child->pp_core))|
				                              VR_PROFILING_EVENT_REASON_START_STOP_HW_VIRTUAL,
				                              vr_pp_job_get_perf_counter_value0(group->pp_running_job, vr_pp_core_get_id(child->pp_core)),
				                              vr_pp_job_get_perf_counter_value1(group->pp_running_job, vr_pp_core_get_id(child->pp_core)),
				                              vr_pp_job_get_perf_counter_src0(group->pp_running_job) | (vr_pp_job_get_perf_counter_src1(group->pp_running_job) << 8),
				                              0, 0);
			}
			if (0 != group->l2_cache_core_ref_count[0])
			{
				if ((VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src0(group->l2_cache_core[0])) &&
									(VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src1(group->l2_cache_core[0])))
				{
					vr_group_report_l2_cache_counters_per_core(group, vr_l2_cache_get_id(group->l2_cache_core[0]));
				}
			}
			if (0 != group->l2_cache_core_ref_count[1])
			{
				if ((VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src0(group->l2_cache_core[1])) &&
									(VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src1(group->l2_cache_core[1])))
				{
					vr_group_report_l2_cache_counters_per_core(group, vr_l2_cache_get_id(group->l2_cache_core[1]));
				}
			}

#endif
		}
		else
		{
			/* update performance counters for a physical group's pp core */
			vr_pp_update_performance_counters(group->pp_core, group->pp_running_job, group->pp_running_sub_job);

#if defined(CONFIG_VR400_PROFILING)
			_vr_osk_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP|
			                              VR_PROFILING_MAKE_EVENT_CHANNEL_PP(vr_pp_core_get_id(group->pp_core))|
			                              VR_PROFILING_EVENT_REASON_START_STOP_HW_PHYSICAL,
			                              vr_pp_job_get_perf_counter_value0(group->pp_running_job, group->pp_running_sub_job),
			                              vr_pp_job_get_perf_counter_value1(group->pp_running_job, group->pp_running_sub_job),
			                              vr_pp_job_get_perf_counter_src0(group->pp_running_job) | (vr_pp_job_get_perf_counter_src1(group->pp_running_job) << 8),
			                              0, 0);
			if ((VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src0(group->l2_cache_core[0])) &&
					(VR_HW_CORE_NO_COUNTER != vr_l2_cache_core_get_counter_src1(group->l2_cache_core[0])))
			{
				vr_group_report_l2_cache_counters_per_core(group, vr_l2_cache_get_id(group->l2_cache_core[0]));
			}
#endif
		}
	}
}

static void vr_group_timeout(void *data)
{
	struct vr_group *group = (struct vr_group *)data;

	group->core_timed_out = VR_TRUE;

	if (NULL != group->gp_core)
	{
		VR_DEBUG_PRINT(2, ("VR group: TIMEOUT on %s\n", vr_gp_get_hw_core_desc(group->gp_core)));
		_vr_osk_wq_schedule_work(group->bottom_half_work_gp);
	}
	else
	{
		VR_DEBUG_PRINT(2, ("VR group: TIMEOUT on %s\n", vr_pp_get_hw_core_desc(group->pp_core)));
		_vr_osk_wq_schedule_work(group->bottom_half_work_pp);
	}
}

void vr_group_zap_session(struct vr_group *group, struct vr_session_data *session)
{
	VR_DEBUG_ASSERT_POINTER(group);
	VR_DEBUG_ASSERT_POINTER(session);

	/* Early out - safe even if mutex is not held */
	if (group->session != session) return;

	vr_group_lock(group);

	vr_group_remove_session_if_unused(group, session);

	if (group->session == session)
	{
		/* The Zap also does the stall and disable_stall */
		vr_bool zap_success = vr_mmu_zap_tlb(group->mmu);
		if (VR_TRUE != zap_success)
		{
			VR_DEBUG_PRINT(2, ("VR memory unmap failed. Doing pagefault handling.\n"));
			vr_group_mmu_page_fault(group);
		}
	}

	vr_group_unlock(group);
}

#if defined(CONFIG_VR400_PROFILING)
static void vr_group_report_l2_cache_counters_per_core(struct vr_group *group, u32 core_num)
{
	u32 source0 = 0;
	u32 value0 = 0;
	u32 source1 = 0;
	u32 value1 = 0;
	u32 profiling_channel = 0;

	switch(core_num)
	{
		case 0:	profiling_channel = VR_PROFILING_EVENT_TYPE_SINGLE |
				VR_PROFILING_EVENT_CHANNEL_GPU |
				VR_PROFILING_EVENT_REASON_SINGLE_GPU_L20_COUNTERS;
				break;
		case 1: profiling_channel = VR_PROFILING_EVENT_TYPE_SINGLE |
				VR_PROFILING_EVENT_CHANNEL_GPU |
				VR_PROFILING_EVENT_REASON_SINGLE_GPU_L21_COUNTERS;
				break;
		case 2: profiling_channel = VR_PROFILING_EVENT_TYPE_SINGLE |
				VR_PROFILING_EVENT_CHANNEL_GPU |
				VR_PROFILING_EVENT_REASON_SINGLE_GPU_L22_COUNTERS;
				break;
		default: profiling_channel = VR_PROFILING_EVENT_TYPE_SINGLE |
				VR_PROFILING_EVENT_CHANNEL_GPU |
				VR_PROFILING_EVENT_REASON_SINGLE_GPU_L20_COUNTERS;
				break;
	}

	if (0 == core_num)
	{
		vr_l2_cache_core_get_counter_values(group->l2_cache_core[0], &source0, &value0, &source1, &value1);
	}
	if (1 == core_num)
	{
		if (1 == vr_l2_cache_get_id(group->l2_cache_core[0]))
		{
			vr_l2_cache_core_get_counter_values(group->l2_cache_core[0], &source0, &value0, &source1, &value1);
		}
		else if (1 == vr_l2_cache_get_id(group->l2_cache_core[1]))
		{
			vr_l2_cache_core_get_counter_values(group->l2_cache_core[1], &source0, &value0, &source1, &value1);
		}
	}
	if (2 == core_num)
	{
		if (2 == vr_l2_cache_get_id(group->l2_cache_core[0]))
		{
			vr_l2_cache_core_get_counter_values(group->l2_cache_core[0], &source0, &value0, &source1, &value1);
		}
		else if (2 == vr_l2_cache_get_id(group->l2_cache_core[1]))
		{
			vr_l2_cache_core_get_counter_values(group->l2_cache_core[1], &source0, &value0, &source1, &value1);
		}
	}

	_vr_osk_profiling_add_event(profiling_channel, source1 << 8 | source0, value0, value1, 0, 0);
}
#endif /* #if defined(CONFIG_VR400_PROFILING) */
