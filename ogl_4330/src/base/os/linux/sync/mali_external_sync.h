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
 * @file mali_external_sync.h
 *
 * Mali interface to external sync object API.
 */

#ifndef _MALI_EXTERNAL_SYNC_H_
#define _MALI_EXTERNAL_SYNC_H_

#include <base/mali_types.h>
#include <base/mali_macros.h>
#include <base/common/base_common_context.h>

#include <unistd.h>
#include <errno.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mali_base_stream
{
	u32 fd;
} mali_base_stream;

/**
 * Definition of the stream object type
 */
typedef struct mali_base_stream * mali_stream_handle;

/**
 * Definition of the fence object type
 *
 * This wraps sync object fence file descriptor.
 */
typedef s32 mali_fence_handle;

#define MALI_FENCE_INVALID_HANDLE -1

#if MALI_EXTERNAL_SYNC

/* Include after type definitions */
#include <base/arch/base_arch_external_sync.h>

/** Create a Mali stream
 *
 * @param ctx the base context to assosiate the sream with
 * @return Handle to the new stream
 */
MALI_STATIC_FORCE_INLINE mali_stream_handle mali_stream_create(mali_base_ctx_handle ctx)
{
	return _mali_base_arch_stream_create(ctx);
}

/**
 * @brief Destroy a stream
 *
 * @param stream Handle of the stream to destroy
 */
MALI_STATIC_FORCE_INLINE void mali_stream_destroy(mali_stream_handle stream)
{
	_mali_base_arch_stream_destroy(stream);
}

/**
 * @brief Check validity of fence
 *
 * @param fence Handle of fence to check
 * @return MALI_TRUE if valid, otherwise MALI_FALSE
 */
MALI_STATIC_FORCE_INLINE mali_bool mali_fence_is_valid(mali_fence_handle fence)
{
	return _mali_base_arch_fence_validate(fence);
}

/**
 * @brief Import a sync fence file descriptor
 *
 * @param fd the file descriptor to import
 * @return Handle to Mali fence
 */
MALI_STATIC_FORCE_INLINE mali_fence_handle mali_fence_import_fd(int fd)
{
	MALI_DEBUG_ASSERT(MALI_FENCE_INVALID_HANDLE < fd, ("Invalid fence handle"));

	return (mali_fence_handle)fd;
}

/**
 * @brief Get file descriptor of mali fence
 *
 * TODO: decide on fd ownership/dup on fd extract?
 *
 * @param fence Handle of fence
 * @return File descriptor representing the fence
 */
MALI_STATIC_FORCE_INLINE int mali_fence_fd(mali_fence_handle fence)
{
	return (int)fence;
}

/**
 * @brief Release a mali_fence
 *
 * @param fence Handle of the fence to release
 */
MALI_STATIC_FORCE_INLINE void mali_fence_release(mali_fence_handle fence)
{
	int fd = (int)fence;

	MALI_DEBUG_ASSERT(MALI_FENCE_INVALID_HANDLE < fd, ("Invalid fence handle"));

	close(fd);
}

/**
 * @brief Duplicate a mali_fence
 *
 * @param fence handle of the fence to duplicate
 */
MALI_STATIC_FORCE_INLINE mali_fence_handle mali_fence_dup(mali_fence_handle fence)
{
	int fd = (int)fence;
	int newfd;

	MALI_DEBUG_ASSERT(MALI_FENCE_INVALID_HANDLE < fd, ("Invalid fence handle"));

	newfd = dup(fd);

	if (0 > newfd)
	{
		MALI_DEBUG_PRINT(1, ("Failed to duplicate fence fd %d, errno %s\n", fd, strerror(errno)));
		return MALI_FENCE_INVALID_HANDLE;
	}

	return (mali_fence_handle)newfd;
}

/**
 * @brief Wait for a sync fence
 *
 * Call will block until fence is signalled, or timeout is reached.
 *
 * Assumes ownership of fence and will release it after use.
 *
 * A negative value for timout will wait forever.
 *
 * @param fence Handle of the fence to wait for
 * @param timeout Timeout in ms
 * @return Will return MALI_ERR_NO_ERROR if fence is signalled
 */
mali_err_code mali_fence_wait(mali_fence_handle fence, u32 timeout);

/**
 * @brief Merge two sync fences
 *
 * The two input fences are not valid after a successful merge.
 *
 * @param name Name of the new fence
 * @param f1 First fence to merge
 * @param f2 Second fence to merge
 * @return new fence that will be signalled once both f1 and f2 is signalled or MALI_FENCE_INVALID_HANDLE
 */
mali_fence_handle mali_fence_merge(const char *name, mali_fence_handle f1, mali_fence_handle f2);

#else /* MALI_EXTERNAL_SYNC */

MALI_STATIC_FORCE_INLINE void mali_fence_release(mali_fence_handle fence)
{
	MALI_IGNORE(fence);
}

MALI_STATIC_FORCE_INLINE mali_fence_handle mali_fence_merge(const char *name, mali_fence_handle f1, mali_fence_handle f2)
{
	MALI_IGNORE(name);
	MALI_IGNORE(f1);
	MALI_IGNORE(f2);
	return MALI_FENCE_INVALID_HANDLE;
}

#endif /* MALI_EXTERNAL_SYNC */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MALI_EXTERNAL_SYNC_H_ */
