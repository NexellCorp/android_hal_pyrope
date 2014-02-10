/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file ump.c
 *
 * Dummy implementation of the UMP API.
 */

#include <ump/ump.h>

__attribute__((visibility("default"))) ump_result ump_open(void)
{
	return UMP_ERROR;
}

__attribute__((visibility("default"))) void ump_close(void)
{
	return;
}

__attribute__((visibility("default"))) ump_handle ump_allocate_64(u64 size, ump_alloc_flags flags)
{
	return UMP_INVALID_MEMORY_HANDLE;
}

__attribute__((visibility("default"))) ump_secure_id ump_secure_id_get(const ump_handle memh)
{
	return UMP_INVALID_SECURE_ID;
}

__attribute__((visibility("default"))) ump_handle ump_from_secure_id(ump_secure_id secure_id)
{
	return UMP_INVALID_MEMORY_HANDLE;
}

__attribute__((visibility("default"))) u64 ump_size_get_64(const ump_handle memh)
{
	return 0;
}

__attribute__((visibility("default"))) int ump_cpu_msync_now(ump_handle memh, ump_cpu_msync_op op, void * address, size_t size)
{
	return 0;
}

__attribute__((visibility("default"))) ump_result ump_retain(ump_handle memh)
{
	return UMP_OK;
}

__attribute__((visibility("default"))) void ump_release(ump_handle memh)
{
	return;
}

__attribute__((visibility("default"))) void * ump_map(ump_handle memh, u64 offset, size_t size)
{
	return NULL;
}

__attribute__((visibility("default"))) void ump_unmap(ump_handle memh, void* address, size_t size)
{
	return;
}

__attribute__((visibility("default"))) ump_handle ump_import(enum ump_external_memory_type type, void * phandle, ump_alloc_flags flags)
{
	return UMP_INVALID_MEMORY_HANDLE;
}

/*
 * UMP v1 API
 */

__attribute__((visibility("default"))) ump_handle ump_ref_drv_allocate(unsigned long size, ump_alloc_constraints constraints)
{
	return UMP_INVALID_MEMORY_HANDLE;
}

__attribute__((visibility("default"))) unsigned long ump_size_get(const ump_handle memh)
{
	return 0;
}

__attribute__((visibility("default"))) ump_handle ump_handle_create_from_secure_id(ump_secure_id secure_id)
{
	return UMP_INVALID_MEMORY_HANDLE;
}

__attribute__((visibility("default"))) void ump_reference_add(ump_handle mem)
{
	return;
}

__attribute__((visibility("default"))) void ump_reference_release(ump_handle mem)
{
	return;
}

__attribute__((visibility("default"))) void * ump_mapped_pointer_get(ump_handle memh)
{
	return NULL;
}

__attribute__((visibility("default"))) void ump_mapped_pointer_release(ump_handle memh)
{
	return;
}

__attribute__((visibility("default"))) void ump_read(void * dst, ump_handle memh, unsigned long offset, unsigned long length)
{
	return;
}

__attribute__((visibility("default"))) void ump_write(ump_handle memh, unsigned long offset, const void * src, unsigned long length)
{
	return;
}

