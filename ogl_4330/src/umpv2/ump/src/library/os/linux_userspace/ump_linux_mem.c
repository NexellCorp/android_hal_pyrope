/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
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
#include <ump/src/library/common/ump_user.h>
#include <ump/ump_kernel_interface.h>

#include <sys/mman.h>
#include <unistd.h>

#ifdef ANDROID
/* Android does not provide a mmap64 function, it is implemented directly here using the __mmap2 syscall */
extern void *__mmap2(void *, size_t, int, int, int, size_t);

static void *mmap64( void *addr, size_t size, int prot, int flags, int fd, off64_t offset )
{
	return __mmap2(addr, size, prot, flags, fd, (offset/sysconf(_SC_PAGESIZE)));
}
#endif

__attribute__((visibility("default"))) void * ump_map(ump_handle memh, u64 offset, size_t size)
{
	void * ret = NULL;

	/* Assertion expected in debug builds for a bad handle
	 */
	UMP_ASSERT( memh != NULL);

	{
		ump_mem * mem = (ump_mem *)memh;
		ump_dd_handle dd_h;
		
		UMP_ASSERT( mem->id != UMP_INVALID_SECURE_ID );
		
		dd_h = ump_dd_from_secure_id( (ump_secure_id) mem->id );

		if( dd_h != UMP_DD_INVALID_MEMORY_HANDLE )
		{
			u64 pCount;
			const ump_dd_physical_block_64 *pArray;
			
			ump_dd_phys_blocks_get_64( dd_h, &pCount, &pArray );

			UMP_ASSERT( (offset + size) > offset );
			UMP_ASSERT( (offset + size) <= pArray[0].size );
			
			ret = (void*) (uintptr_t) (pArray[0].addr + offset);
		}
	}
	return ret;
}

__attribute__((visibility("default"))) void ump_unmap(ump_handle memh, void* address, size_t size)
{
	/* Assertion expected in debug builds for a bad handle
	 */
	UMP_ASSERT( memh != NULL);

	{
		ump_mem * mem = (ump_mem *)memh;
		ump_dd_handle dd_h;
		
		UMP_ASSERT( mem->id != UMP_INVALID_SECURE_ID );
		
		dd_h = ump_dd_from_secure_id( (ump_secure_id) mem->id );

		if( dd_h != UMP_DD_INVALID_MEMORY_HANDLE )
		{
			ump_dd_release( dd_h );
			ump_dd_release( dd_h );
		}
	}
}
