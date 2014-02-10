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
 * This file implements the user space API of the UMP API.
 */

#include <ump/ump.h>
#include <ump/src/library/common/ump_user.h>
#include <ump/src/library/common/ump_atomic.h>
#include <ump/ump_kernel_interface.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define STATIC static
#define ULONG_MAX ((unsigned long)~0UL)
#define U32_MAX UINT32_MAX

/** Reference counting of open() and close(). */
STATIC unsigned long ump_ref_count = 0;

/* Somewhat arbitrary limit for any single allocation
 */
#define UMP_LOG2_MAX_SINGLE_ALLOC	27

static pthread_mutex_t ump_lock = PTHREAD_MUTEX_INITIALIZER;

extern void open_ump_session(void);
extern void close_ump_session(void);

STATIC INLINE ump_result umpp_get_allocation_flags(ump_secure_id id, ump_alloc_flags *flags)
{
	ump_dd_handle *h = NULL;

	h = ump_dd_from_secure_id( id );
	if( h != UMP_DD_INVALID_MEMORY_HANDLE )
	{
		*flags = ump_dd_allocation_flags_get(h);
		return UMP_OK;
	}

	return UMP_ERROR;
}

STATIC INLINE ump_result umpp_retain(ump_secure_id id)
{
	ump_dd_handle *h = NULL;

	h = ump_dd_from_secure_id( id );
	if( h != UMP_DD_INVALID_MEMORY_HANDLE )
	{
		return UMP_OK;
	}

	return UMP_ERROR;
}

STATIC INLINE void umpp_release(ump_secure_id id)
{
	ump_dd_handle *h = NULL;

	h = ump_dd_from_secure_id( id );
	if( h != UMP_DD_INVALID_MEMORY_HANDLE )
	{
		ump_dd_release( h );
		ump_dd_release( h );
		return;
	}

	UMP_PRINT_WARN("umpp_release failed\n");
	return;

}

__attribute__((visibility("default"))) ump_result ump_open(void)
{
	ump_result res = UMP_OK;

	/*mutex_lock*/
	if (pthread_mutex_lock(&ump_lock))
	{
		return UMP_ERROR;
	}

	if (ump_ref_count == ULONG_MAX)
	{
		/* too many opens */
		res = UMP_ERROR;
	}
	else if (1 == ++ump_ref_count)
	{
		/* first open call, do the actual backend open */
		open_ump_session();
	}

	if (pthread_mutex_unlock(&ump_lock))
	{
		return UMP_ERROR;
	}

	return res;
}

__attribute__((visibility("default"))) void ump_close(void)
{
	int ret;

	UMP_ASSERT(0 < ump_ref_count);

	/*mutex_lock*/
	ret = pthread_mutex_lock(&ump_lock);
	/*mutex lock failed*/
	UMP_ASSERT(ret == 0);

	if (ump_ref_count && 0 == --ump_ref_count)
	{
		/* last close call, do the actual backend close */
		close_ump_session();
	}

	ret = pthread_mutex_unlock(&ump_lock);
	/*mutex unlock failed*/
	UMP_ASSERT(ret == 0);
	(void)ret;
}


__attribute__((visibility("default"))) ump_handle ump_allocate_64(u64 size, ump_alloc_flags flags)
{
	ump_dd_handle h;
	ump_mem * mem;

	UMP_ASSERT(0 == (flags & UMPP_ALLOCBITS_UNUSED));
	flags = umpp_validate_alloc_flags(flags);

	/* round up to a page size */
	size = (size + (sysconf(_SC_PAGESIZE) - 1)) & ~(sysconf(_SC_PAGESIZE) - 1);

	if( size > (1u << UMP_LOG2_MAX_SINGLE_ALLOC) )
	{
		/* Arbitrary limit to allocation size
		 */
		UMP_PRINT_WARN( "UMP allocation failed the arbitrary size constraint\n" );
		return UMP_INVALID_MEMORY_HANDLE;
	}

	h = ump_dd_allocate_64( size, flags, NULL, NULL, NULL );

	if( h != UMP_DD_INVALID_MEMORY_HANDLE )
	{
		mem = malloc(sizeof(*mem));

		if (NULL != mem)
		{
			mem->id = ump_dd_secure_id_get( h );
			mem->flags = flags;
			ump_atomic_set(&mem->refcnt, 1);

			/* used by v1 API */
			mem->mapped_mem = NULL;

			return (ump_handle)mem;
		}
		ump_dd_release( h );
	}
	return UMP_INVALID_MEMORY_HANDLE;
}

__attribute__((visibility("default"))) ump_secure_id ump_secure_id_get(const ump_handle memh)
{
	const ump_mem * mem = (const ump_mem *)memh;

	if (UMP_INVALID_MEMORY_HANDLE == memh)
	{
		return UMP_INVALID_SECURE_ID;
	}

	UMP_ASSERT(mem != NULL);
	UMP_ASSERT(mem->id != UMP_INVALID_SECURE_ID);
	UMP_ASSERT(ump_atomic_get(&mem->refcnt) != 0);

	return mem->id;
}

__attribute__((visibility("default"))) ump_handle ump_from_secure_id(ump_secure_id secure_id)
{
	ump_handle result = UMP_INVALID_MEMORY_HANDLE;
	ump_alloc_flags flags;

	if (UMP_INVALID_SECURE_ID == secure_id)
	{
		return result;
	}

	if (UMP_OK == umpp_retain(secure_id))
	{
		ump_mem * mem;

		if(UMP_OK != umpp_get_allocation_flags(secure_id, &flags))
		{
			umpp_release(secure_id);
			return UMP_INVALID_MEMORY_HANDLE;
		}

		/* handle validated and attached to this process */
		mem = malloc(sizeof(*mem));

		if (NULL != mem)
		{
			mem->id = secure_id;
			mem->flags = flags;
			ump_atomic_set(&mem->refcnt, 1);

			/* used by v1 API */
			mem->mapped_mem = NULL;

			result = (ump_handle)mem;
		}
		else
		{
			/* remove our ref */
			umpp_release(secure_id);
		}
	}

	return result;
}

__attribute__((visibility("default"))) u64 ump_size_get_64(const ump_handle memh)
{
	const ump_mem * mem = (const ump_mem *)memh;
	ump_dd_handle *h = NULL;
	u64 size = 0;

	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != mem);
	UMP_ASSERT(mem != NULL);

	h = ump_dd_from_secure_id( mem->id );
	if( h != UMP_DD_INVALID_MEMORY_HANDLE )
	{
		size = ump_dd_size_get_64( h );
		ump_dd_release( h );
	}
	return size;
}

__attribute__((visibility("default"))) int ump_cpu_msync_now(ump_handle memh, ump_cpu_msync_op op, void * address, size_t size)
{
	ump_mem * mem = (ump_mem *)memh;
	int is_cached = 1;

	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != memh);
	UMP_ASSERT(mem != NULL);
	UMP_ASSERT((UMP_MSYNC_CLEAN == op) || (UMP_MSYNC_CLEAN_AND_INVALIDATE == op) );

	if(mem->flags == UMP_CONSTRAINT_UNCACHED)
	{
		/* mapping is not cached, exit */
		return 0;
	}

	return is_cached;
}

__attribute__((visibility("default"))) ump_result ump_retain(ump_handle memh)
{
	ump_mem * mem = (ump_mem *)memh;

	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != memh);
	UMP_ASSERT(mem != NULL);

	/* check for overflow */
	while(1)
	{
		u32 refcnt = ump_atomic_get(&mem->refcnt);
		if (refcnt != U32_MAX)
		{
			if(ump_atomic_compare_and_swap(&mem->refcnt, refcnt, refcnt + 1) == refcnt)
			{
				return UMP_OK;
			}
		}
		else
		{
			return UMP_ERROR;
		}
	}
}

__attribute__((visibility("default"))) void ump_release(ump_handle memh)
{
	ump_mem * mem = (ump_mem * )memh;

	if (UMP_INVALID_MEMORY_HANDLE == memh)
	{
		/* swallow this to ease error handlers */
		return;
	}

	if (0 == ump_atomic_add( &mem->refcnt, -1 ))
	{

		if( NULL != mem->mapped_mem )
		{
			size_t size;
			/* if UMP v1 memory alocation, unmap it*/
			size = (size_t)ump_size_get_64(memh);
			/* unmap whole allocated memory */
			ump_unmap(memh, mem->mapped_mem, size);
		}

		umpp_release(mem->id);
		free(mem);
	}
}

__attribute__((visibility("default"))) ump_handle ump_import(enum ump_external_memory_type type, void * phandle, ump_alloc_flags flags)
{
	/* not supported */
	return UMP_INVALID_MEMORY_HANDLE;
}

/*
 * UMP v1 API
 */

__attribute__((visibility("default"))) ump_handle ump_ref_drv_allocate(unsigned long size, ump_alloc_constraints constraints)
{
	ump_handle memh;
	void * mapping;
	ump_alloc_flags flags = UMP_V1_API_DEFAULT_ALLOCATION_FLAGS;

	if ( (constraints & UMP_REF_DRV_CONSTRAINT_USE_CACHE) == 0 )
	{
		/* don't use cache */
		flags |= UMP_CONSTRAINT_UNCACHED;
	}

	memh = ump_allocate_64(size, flags);
	if(UMP_INVALID_MEMORY_HANDLE != memh)
	{
		mapping = ump_map(memh, 0, size);
		if(NULL != mapping)
		{
			((ump_mem *)memh)->mapped_mem = mapping;
		}
		else
		{
			ump_release(memh);
			return UMP_INVALID_MEMORY_HANDLE;
		}
	}
	return memh;
}

__attribute__((visibility("default"))) unsigned long ump_size_get(const ump_handle memh)
{
	const ump_mem * mem = (const ump_mem *)memh;
	ump_dd_handle *h = NULL;
	u64 size = 0;

	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != mem);
	UMP_ASSERT(mem != NULL);
	UMP_ASSERT_MSG( 0 != (((ump_mem*)memh)->flags & UMP_CONSTRAINT_32BIT_ADDRESSABLE), "using UMP v1 API with UMP v2 allocation" );

	h = ump_dd_from_secure_id( mem->id );
	if( h != UMP_DD_INVALID_MEMORY_HANDLE )
	{
		size = ump_dd_size_get_64( h );
		ump_dd_release( h );
	}
	return size;
}

__attribute__((visibility("default"))) ump_handle ump_handle_create_from_secure_id(ump_secure_id secure_id)
{
	ump_handle memh;
	ump_mem * mem;
	unsigned long size;
	void * mapping;

	UMP_ASSERT_MSG(UMP_INVALID_SECURE_ID != secure_id, "Secure ID is invalid");

	memh = ump_from_secure_id(secure_id);
	if (UMP_INVALID_MEMORY_HANDLE == memh)
	{
		return UMP_INVALID_MEMORY_HANDLE;
	}

	mem = (ump_mem *)memh;

	if( !(mem->flags & UMP_CONSTRAINT_32BIT_ADDRESSABLE) )
	{
		ump_release(memh);
		UMP_PRINT_WARN("Secure ID is not compatible with UMP v1 API");
		return UMP_INVALID_MEMORY_HANDLE;
	}

	size = (unsigned long)ump_size_get_64(memh);

	mapping = ump_map(memh, 0, size);
	if(NULL != mapping)
	{
		mem->mapped_mem = mapping;
	}
	else
	{
		ump_release(memh);
		return UMP_INVALID_MEMORY_HANDLE;
	}

	return memh;
}

__attribute__((visibility("default"))) void ump_reference_add(ump_handle mem)
{
	ump_retain(mem);
}

__attribute__((visibility("default"))) void ump_reference_release(ump_handle mem)
{
	ump_release(mem);
}

__attribute__((visibility("default"))) void * ump_mapped_pointer_get(ump_handle memh)
{
	ump_mem * mem = (ump_mem * )memh;

	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != memh);
	UMP_ASSERT(mem != NULL);
	UMP_ASSERT_MSG( NULL != ((ump_mem*)memh)->mapped_mem, "Error in mapping pointer (not mapped)" );
	UMP_ASSERT_MSG( 0 != (((ump_mem*)memh)->flags & UMP_CONSTRAINT_32BIT_ADDRESSABLE), "using UMP v1 API with UMP v2 allocation" );

	return mem->mapped_mem;
}

__attribute__((visibility("default"))) void ump_mapped_pointer_release(ump_handle memh)
{
	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != memh);
	UMP_ASSERT_MSG( NULL != ((ump_mem*)memh)->mapped_mem, "Error in mapping pointer (not mapped)" );
	UMP_ASSERT_MSG( 0 != (((ump_mem*)memh)->flags & UMP_CONSTRAINT_32BIT_ADDRESSABLE), "using UMP v1 API with UMP v2 allocation" );

	/* noop, cos we map in the pointer when handle is created, and unmap it when handle is destroyed */
}

__attribute__((visibility("default"))) void ump_read(void * dst, ump_handle memh, unsigned long offset, unsigned long length)
{
	ump_mem * mem = (ump_mem*)memh;

	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != memh);
	UMP_ASSERT(mem != NULL);
	UMP_ASSERT_MSG( NULL != ((ump_mem*)memh)->mapped_mem, "Error in mapping pointer (not mapped)" );
	UMP_ASSERT_MSG( 0 != (((ump_mem*)memh)->flags & UMP_CONSTRAINT_32BIT_ADDRESSABLE), "using UMP v1 API with UMP v2 allocation" );

	memcpy(dst,(char*)(mem->mapped_mem) + offset, length);
}

__attribute__((visibility("default"))) void ump_write(ump_handle memh, unsigned long offset, const void * src, unsigned long length)
{
	ump_mem * mem = (ump_mem*)memh;

	UMP_ASSERT(UMP_INVALID_MEMORY_HANDLE != memh);
	UMP_ASSERT(mem != NULL);
	UMP_ASSERT_MSG( NULL != ((ump_mem*)memh)->mapped_mem, "Error in mapping pointer (not mapped)" );
	UMP_ASSERT_MSG( 0 != (((ump_mem*)memh)->flags & UMP_CONSTRAINT_32BIT_ADDRESSABLE), "using UMP v1 API with UMP v2 allocation" );

	memcpy((char*)(mem->mapped_mem) + offset, src, length);
}
