/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file ump_linux_mem.c
 *
 * This file implements the user space API of the UMP API.
 * It relies heavily on a arch backend to do the communication with the UMP device driver.
 */

#define _LARGEFILE64_SOURCE 1

#include <ump/ump.h>
#include <ump/src/ump_ioctl.h>
#include <ump/src/library/common/ump_user.h>

#include <sys/mman.h>
#include <unistd.h>

typedef uint64_t off64_t;
typedef uint64_t mali_size64;
#define UNUSED( x ) ((void)(x))

#ifdef ANDROID
/* Android does not provide a mmap64 function, it is implemented directly here using the __mmap2 syscall */
extern void *__mmap2(void *, size_t, int, int, int, size_t);

static void *mmap64( void *addr, size_t size, int prot, int flags, int fd, off64_t offset )
{
	return __mmap2(addr, size, prot, flags, fd, offset/(sysconf(_SC_PAGESIZE)));
}
#endif

__attribute__((visibility("default"))) void * ump_map(ump_handle memh, u64 offset, size_t size)
{
	ump_mem * mem = (ump_mem *)memh;
	void * addr;
	size_t page_mask = sysconf(_SC_PAGESIZE) - 1;
	mali_size64 map_offset;
	size_t map_size;
	off64_t arg;

	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != memh);
	UMP_ASSERT(mem != NULL);
	UMP_ASSERT((mem->id & UMP_LINUX_ID_MASK) == mem->id);

	if ( 0 == size )
	{
		return NULL;
	}

	/* make size a multiple of pages, correcting any rounding down we do to the offset */
	map_size = (size + (offset & page_mask) + page_mask) & ~page_mask;
	/* round down to a page boundary */
	map_offset = offset/(sysconf(_SC_PAGESIZE));

	/* 64-bit (really 52 bits) encoding: 15 bits for the ID, 37 bits for the offset */
	UMP_ASSERT( (map_offset & UMP_LINUX_OFFSET_MASK) == map_offset);
	arg = ((((u64)mem->id) << UMP_LINUX_OFFSET_BITS) | (map_offset))*sysconf(_SC_PAGESIZE);

	addr = mmap64(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, ump_fd, arg);
	if (MAP_FAILED == addr) return NULL;
	return (u8*)addr + (offset & page_mask);

}

__attribute__((visibility("default"))) void ump_unmap(ump_handle memh, void* address, size_t size)
{
	ump_mem * mem = (ump_mem *)memh;
	size_t page_mask = (sysconf(_SC_PAGESIZE)) - 1;

	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != memh);
	UMP_ASSERT(mem != NULL);
	UMP_ASSERT(0 != mem->id);
	UMP_ASSERT((mem->id & UMP_LINUX_ID_MASK) == mem->id);
	
	UNUSED(mem);

	size = ((size + ((uintptr_t)address & page_mask)) + page_mask) & ~page_mask;
	address = (void*)((uintptr_t)address & ~page_mask);
	munmap((void*)address, size);
}

