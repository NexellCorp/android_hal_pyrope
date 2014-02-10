/*
 * The confidential and proprietary information contained in this file may
 * only be used by a person authorised under and to the extent permitted
 * by a subsisting licensing agreement from ARM Limited.
 *
 * (C) COPYRIGHT 2012 ARM Limited.
 * ALL RIGHTS RESERVED
 *
 * This entire notice must be reproduced on all copies of this file
 * and copies of this file may only be made by a person if such person is
 * permitted to do so under the terms of a subsisting license agreement
 * from ARM Limited.
 */

#include <mali_system.h>
#include "mali_osu.h"
#include "mali_uku.h"

#include <sync/mali_external_sync.h>

#include <sys/ioctl.h>
#include <errno.h>

/* Define ioctl constants if needed */
#ifndef SYNC_IOC_WAIT
#define SYNC_IOC_WAIT _IOW('>', 0, s32)
#endif

#ifndef SYNC_IOC_MERGE
struct sync_merge_data
{
	s32 fd2;
	char name[32];
	s32 fence;
};
#define SYNC_IOC_MERGE _IOWR('>', 1, struct sync_merge_data)
#endif

mali_fence_handle mali_fence_merge(const char *name, mali_fence_handle f1, mali_fence_handle f2)
{
	struct sync_merge_data data;
	int err;
	s32 fd1 = mali_fence_fd(f1);
	s32 fd2 = mali_fence_fd(f2);

	data.fd2 = fd2;

	err = ioctl(fd1, SYNC_IOC_MERGE, &data);

	if (0 > err)
	{
		MALI_DEBUG_PRINT(1, ("Failed to merge job fences; new fence not set on job\n"));
		return MALI_FENCE_INVALID_HANDLE;
	}

	mali_fence_release(f1);
	mali_fence_release(f2);

	return mali_fence_import_fd(data.fence);
}

mali_err_code mali_fence_wait(mali_fence_handle fence, u32 timeout)
{
	s32 fd = mali_fence_fd(fence);
	u32 to = timeout;

	int err = ioctl(fd, SYNC_IOC_WAIT, &to);

	switch (err)
	{
		case -ETIME:
			return MALI_ERR_TIMEOUT;
		case 0:
			mali_fence_release(fence);
			return MALI_ERR_NO_ERROR;
		default:
			return MALI_ERR_FUNCTION_FAILED;
	}
}
