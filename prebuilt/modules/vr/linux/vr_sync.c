/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_sync.c
 *
 */

#include <linux/seq_file.h>
#include <linux/sync.h>

#include "vr_osk.h"
#include "vr_kernel_common.h"

struct vr_sync_timeline
{
	struct sync_timeline timeline;
	atomic_t counter;
	atomic_t signalled;
};

struct vr_sync_pt
{
	struct sync_pt pt;
	u32 order;
	s32 error;
};

static inline struct vr_sync_timeline *to_vr_sync_timeline(struct sync_timeline *timeline)
{
	return container_of(timeline, struct vr_sync_timeline, timeline);
}

static inline struct vr_sync_pt *to_vr_sync_pt(struct sync_pt *pt)
{
	return container_of(pt, struct vr_sync_pt, pt);
}

static struct sync_pt *timeline_dup(struct sync_pt *pt)
{
	struct vr_sync_pt *mpt = to_vr_sync_pt(pt);
	struct vr_sync_pt *new_mpt;
	struct sync_pt *new_pt = sync_pt_create(pt->parent, sizeof(struct vr_sync_pt));

	if (!new_pt)
	{
		return NULL;
	}

	new_mpt = to_vr_sync_pt(new_pt);
	new_mpt->order = mpt->order;

	return new_pt;

}

static int timeline_has_signaled(struct sync_pt *pt)
{
	struct vr_sync_pt *mpt = to_vr_sync_pt(pt);
	struct vr_sync_timeline *mtl = to_vr_sync_timeline(pt->parent);
	long diff;

	if (0 != mpt->error)
	{
		return mpt->error;
	}

	diff = atomic_read(&mtl->signalled) - mpt->order;

	return diff >= 0;
}

static int timeline_compare(struct sync_pt *a, struct sync_pt *b)
{
	struct vr_sync_pt *ma = container_of(a, struct vr_sync_pt, pt);
	struct vr_sync_pt *mb = container_of(b, struct vr_sync_pt, pt);

	long diff = ma->order - mb->order;

	if (diff < 0)
	{
		return -1;
	}
	else if (diff == 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

static void timeline_print_tl(struct seq_file *s, struct sync_timeline *sync_timeline)
{
	struct vr_sync_timeline *mtl = to_vr_sync_timeline(sync_timeline);

	seq_printf(s, "%u, %u", atomic_read(&mtl->signalled), atomic_read(&mtl->counter));
}

static void timeline_print_pt(struct seq_file *s, struct sync_pt *sync_pt)
{
	struct vr_sync_pt *mpt = to_vr_sync_pt(sync_pt);

	seq_printf(s, "%u", mpt->order);

}

static struct sync_timeline_ops vr_timeline_ops = {
	.driver_name    = "VR",
	.dup            = timeline_dup,
	.has_signaled   = timeline_has_signaled,
	.compare        = timeline_compare,
	.print_obj      = timeline_print_tl,
	.print_pt       = timeline_print_pt
};

int vr_sync_timeline_is_ours(struct sync_timeline *timeline)
{
	return (timeline->ops == &vr_timeline_ops);
}

struct sync_timeline *vr_sync_timeline_alloc(const char * name)
{
	struct sync_timeline *tl;
	struct vr_sync_timeline *mtl;

	tl = sync_timeline_create(&vr_timeline_ops,
	                          sizeof(struct vr_sync_timeline), name);
	if (!tl)
	{
		return NULL;
	}

	/* Set the counter in our private struct */
	mtl = to_vr_sync_timeline(tl);
	atomic_set(&mtl->counter, 0);
	atomic_set(&mtl->signalled, 0);

	return tl;
}

struct sync_pt *vr_sync_pt_alloc(struct sync_timeline *parent)
{
	struct sync_pt *pt = sync_pt_create(parent, sizeof(struct vr_sync_pt));
	struct vr_sync_timeline *mtl = to_vr_sync_timeline(parent);
	struct vr_sync_pt *mpt;

	if (!pt)
	{
		return NULL;
	}

	mpt = to_vr_sync_pt(pt);
	mpt->order = atomic_inc_return(&mtl->counter);
	mpt->error = 0;

	return pt;
}

void vr_sync_signal_pt(struct sync_pt *pt, int error)
{
	struct vr_sync_pt *mpt = to_vr_sync_pt(pt);
	struct vr_sync_timeline *mtl = to_vr_sync_timeline(pt->parent);
	int signalled;
	long diff;

	if (0 != error)
	{
		VR_DEBUG_ASSERT(0 > error);
		mpt->error = error;
	}

	do {

		signalled = atomic_read(&mtl->signalled);

		diff = signalled - mpt->order;

		if (diff > 0)
		{
			/* The timeline is already at or ahead of this point. This should not happen unless userspace
			 * has been signalling fences out of order, so warn but don't violate the sync_pt API.
			 * The warning is only in debug builds to prevent a vrcious user being able to spam dmesg.
			 */
			VR_DEBUG_PRINT_ERROR(("Sync points were triggerd in a different order to allocation!\n"));
			return;
		}
	} while (atomic_cmpxchg(&mtl->signalled, signalled, mpt->order) != signalled);

	sync_timeline_signal(pt->parent);
}
