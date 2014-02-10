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
#include "base_arch_main.h"

#include "mali_osu.h"
#include "mali_uku.h"

#include <sync/mali_external_sync.h>

mali_stream_handle _mali_base_arch_stream_create(mali_base_ctx_handle ctx)
{
	_vr_uk_stream_create_s args = {mali_uk_ctx, 0};
	mali_base_stream *stream;

	stream = _mali_sys_malloc(sizeof(mali_base_stream));
	if (NULL == stream) return NULL;

	if (_MALI_OSK_ERR_OK != _mali_uku_stream_create(&args))
	{
		_mali_sys_free(stream);
		return NULL;
	}

	stream->fd = args.fd;

	return (mali_stream_handle)stream;
}

void _mali_base_arch_stream_destroy(mali_stream_handle handle)
{
	mali_base_stream *stream = (mali_base_stream*)handle;
	_vr_uk_stream_destroy_s args = {mali_uk_ctx, stream->fd};

	_mali_uku_stream_destroy(&args);

	_mali_sys_free(stream);
}

mali_bool _mali_base_arch_fence_validate(mali_fence_handle fence)
{
	_vr_uk_fence_validate_s args = {mali_uk_ctx, -1};
	_mali_osk_errcode_t err;

	args.fd = mali_fence_fd(fence);

	err = _mali_uku_fence_validate(&args);

	if (_MALI_OSK_ERR_OK == err)
	{
		return MALI_TRUE;
	}
	else
	{
		return MALI_FALSE;
	}
}
