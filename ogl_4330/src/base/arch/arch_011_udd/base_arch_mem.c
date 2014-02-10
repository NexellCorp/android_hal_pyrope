/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/* base backends can access OS and library functions directly */
#include <mali_system.h>

#include <stdlib.h>
#include <string.h>

 /* common util functions */
#include <base/common/tools/base_common_tools_circular_linked_list.h>

/* common memory types */
#include <base/mali_memory.h>
#include <base/mali_byteorder.h>
/* base memory interface */
#include <base/common/mem/base_common_mem.h>
/* the arch memory interface we're implementing */
#include <base/arch/base_arch_mem.h>

#include "mali_osu.h"
#include "mali_uku.h"

/* common backend code */
#include "base_arch_main.h"

#if MALI_USE_DMA_BUF
/* @@@@ todo: see if we can find a good solution to this Linux specific code in common problem. */
#include <sys/mman.h>
#endif

/* Minimum allocation size */
#define SLOT_SIZE (256UL * 1024UL)


/**
 * Memory initialization routine
 * Adds a reference to the file descriptor
 * Initializes memory variables
 * @return Standard mali_err_code
 */
MALI_STATIC mali_err_code memory_initialize(mali_base_ctx_handle ctx) MALI_CHECK_RESULT;

/**
 * Memory termination routine
 * Cleans up memory variables
 * Removes the reference to the device driver file descriptor
 */
MALI_STATIC void memory_terminate(mali_base_ctx_handle ctx);

MALI_STATIC mali_err_code backend_mmu_get_memory(u32 cache_settings, u32 minimum_block_size, mali_mem * descriptor) MALI_CHECK_RESULT;
MALI_STATIC void backend_mmu_release_memory(mali_mem * descriptor);

MALI_STATIC mali_err_code backend_mmu_map_external_memory(arch_mem * descriptor, u32 phys_addr, u32 size, u32 access_rights) MALI_CHECK_RESULT;
MALI_STATIC void backend_mmu_unmap_external_memory(arch_mem * descriptor);

/* helpers for MMU implementation */

/* The virtual address range are implemented in base_arch_virtual_mem.c */
/**
 * Create the virtual address allocators
 *
 * @return Standard Mali error code
 */
mali_err_code mali_virtual_memory_area_create(void);

/**
 * Destroy the virtual address allocators
 */
void mali_virtual_memory_area_destroy(void);

/**
 * Allocate a virtual address range
 * Allocates a range inside the virtual address space and sets it as in use
 * @param descriptor The descriptor to set the virtual addresses for
 * @param minimum_block_size Minimum size requested, a larger area might be allocated
 * @return Standard Mali error code
 */
mali_err_code mali_mmu_virtual_address_range_allocate(arch_mem *descriptor, u32 minimum_block_size);

/**
 * Free virtual address range
 * Frees the virtual address range described by the descriptor
 * @param descriptor Descriptor describing the virtual memory range to free
 */
void mali_mmu_virtual_address_range_free(arch_mem *descriptor);

/**
 * Allocate physical memory for descriptor
 * Allocate physical memory for the virtual range described by the descriptor
 * Only size should be allocated, not whole virtual size
 * @param descriptor The descriptor to assign physical memory to
 * @param cache_settings of type _memory_cache_settings
 * @return Standard Mali error code
 */
MALI_STATIC mali_err_code backend_mmu_physical_memory_allocate(arch_mem * descriptor, u32 cache_settings) MALI_CHECK_RESULT;

/**
 * Free physical memory
 * Frees the physical memory assigned to the descriptor
 * @param descriptor Descriptor describing the physical memory to free
 */
MALI_STATIC void backend_mmu_physical_memory_free(arch_mem * descriptor);


/* memory interface, see base_arch_mem.h */
mali_err_code _mali_base_arch_mem_open(void)
{
	return memory_initialize((mali_base_ctx_handle)NULL);
}

void _mali_base_arch_mem_close(void)
{
	memory_terminate((mali_base_ctx_handle)NULL);
}

u32 _mali_base_arch_mem_get_num_capability_sets(void)
{
	u32 count = 0;
	_vr_mem_info * element;

	/* no locking needed since the list is static after init */

	element = _mali_base_arch_get_mem_info();

	while (NULL != element)
	{
		count++;
		element = element->next;
	}

	return count;
}

mali_err_code _mali_base_arch_mem_get_capability_sets(void * buffer, u32 size_of_buffer)
{
	int i;
	u32 minimum_buffer_size;
	mali_mem_bank_info * output;
	_vr_mem_info * element;

	minimum_buffer_size = _mali_base_arch_mem_get_num_capability_sets() * sizeof(mali_mem_bank_info);

	/* check the buffer */
	MALI_CHECK_NON_NULL(buffer, MALI_ERR_FUNCTION_FAILED);
	MALI_CHECK(size_of_buffer >= minimum_buffer_size, MALI_ERR_FUNCTION_FAILED);

	/* loop over the elements, update each's output variable */
	for (i = 0, element = _mali_base_arch_get_mem_info(), output = (mali_mem_bank_info *)buffer; NULL != element; element = element->next, ++output, ++i)
	{
		output->capabilities = element->flags;
		output->size = element->size;
		output->maximum_order_supported = element->maximum_order_supported;
		output->cache_settings = element->identifier;
	}

	MALI_SUCCESS;
}

mali_bool _mali_base_arch_mem_map(mali_mem * base_mem, u32 offset_in_mem, u32 size, u32 flags, void** cpu_ptr)
{
	arch_mem * mem;
	u32 must_have = ((flags & MALI_MEM_PTR_READABLE) ? MALI_CPU_READ : 0) | ((flags & MALI_MEM_PTR_WRITABLE) ? MALI_CPU_WRITE : 0 );

	MALI_DEBUG_PRINT(6, ("Trying to map memory\n"));

	/* check input */
	if (NULL == base_mem) return MALI_FALSE; /* bad memory handle */
	if (NULL == cpu_ptr) return MALI_FALSE; /* bad output pointer */

	/* get the arch descriptor */
	mem = arch_mem_from_mali_mem(base_mem);

	/* check that the memory has been mapped by the device driver */
	if (NULL == mem->mapping)
	{
		MALI_DEBUG_PRINT(3, ("Memory not mapped, direct not available\n"));
		switch (base_mem->memory_subtype)
		{
#if MALI_USE_DMA_BUF
			case MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY:

				if (MALI_ERR_NO_ERROR != _mali_base_arch_mem_dma_buf_map(base_mem))
				{
					MALI_DEBUG_ERROR(("Failed to map dma-buf memory\n"));
					return MALI_FALSE; /* not mapped */
				}
				break;
#endif
			default:
				return MALI_FALSE; /* not mapped */
				break;
		}
	}

	/* all bits requested must be present in what the device driver reported */
	if ( (mem->flags & must_have) != must_have )
	{
		MALI_DEBUG_PRINT(3, ("Memory mapped, but access has to be cached\n"));
		return MALI_FALSE; /* not supported request */
	}

	/* check the requested mapping parameters */
	if (0 != (flags & ~(MALI_MEM_PTR_READABLE | MALI_MEM_PTR_WRITABLE | MALI_MEM_PTR_NO_PRE_UPDATE))) return MALI_FALSE; /* invalid flags */
	if (size > mem->embedded_mali_mem.size) return MALI_FALSE; /* request too large */
	if (offset_in_mem > mem->embedded_mali_mem.size) return MALI_FALSE; /* request will start after the end */
	if ((offset_in_mem + size) > mem->embedded_mali_mem.size) return MALI_FALSE; /* request extends beyond the end */

	/* mapping OK, set pointer */
	*cpu_ptr = MALI_REINTERPRET_CAST(void*)((MALI_REINTERPRET_CAST(u32)mem->mapping) + offset_in_mem);

	return MALI_TRUE;
}


void _mali_base_arch_mem_unmap(mali_mem * mem)
{
	switch (mem->memory_subtype)
	{
#if MALI_USE_DMA_BUF
		case MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY:
			_mali_base_arch_mem_dma_buf_unmap(mem);
			break;
#endif
		default:
			/* No cleanup needed */
			break;
	}
}

mali_mem * _mali_base_arch_mem_allocate_descriptor(void)
{
	arch_mem * mem;
	mem = _mali_sys_calloc(1, sizeof(arch_mem));
	if (NULL == mem) return NULL;
	else return &mem->embedded_mali_mem;
}

void _mali_base_arch_mem_free_descriptor(mali_mem * descriptor)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);
	MALI_DEBUG_ASSERT_POINTER(descriptor);
	if (NULL != descriptor) _mali_sys_free(mem);
}

void _mali_base_arch_descriptor_clear(mali_mem * descriptor)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);
	MALI_DEBUG_ASSERT_POINTER(descriptor);
	if (NULL != descriptor) _mali_sys_memset(mem, 0, sizeof(arch_mem));
}

mali_err_code _mali_base_arch_mem_init_bank(u32 cache_settings)
{
	MALI_IGNORE(cache_settings);
	MALI_SUCCESS;
}

mali_err_code _mali_base_arch_mem_get_memory(u32 cache_settings, u32 minimum_block_size, mali_mem * descriptor)
{
	return backend_mmu_get_memory(cache_settings, minimum_block_size, descriptor);
}

void _mali_base_arch_mem_release_memory(mali_mem * descriptor)
{
	MALI_DEBUG_ASSERT(0 != ((arch_mem*)descriptor)->cookie, ("about to release a block with a zero \
				cookie, memory corruption or incorrect API usage detected\n"));

	backend_mmu_release_memory(descriptor);
}

mali_err_code _mali_base_arch_mem_resize_blocks(mali_mem * A, mali_mem * B, s32 size)
{
	/* all the work of resizing blocks is done in userspace */
	arch_mem * memA = arch_mem_from_mali_mem(A);
	arch_mem * memB = arch_mem_from_mali_mem(B);

	/* check that we have valid pointers */
	MALI_DEBUG_ASSERT_POINTER(memA);
	MALI_DEBUG_ASSERT_POINTER(memB);
	MALI_DEBUG_ASSERT(memA != memB, ("Arch asked to resize memory where the two arguments identify the same block"));

	if (size == 0)
	{
		MALI_DEBUG_PRINT(3, ("Arch asked to perform a zero sized resize"));
		MALI_SUCCESS; /* nothing to do */
	}

	/* only resize blocks with the same cookie, or when at least one of them is a non-bound descriptor (when cookie is zero) */
	if ((0 != memA->cookie) &&  (0 != memB->cookie) && (memB->cookie != memA->cookie)) return MALI_ERR_FUNCTION_FAILED;

	/* check for blank descriptors, use size == 0 to test */
	if (0 == A->size)
	{
		/* assert that A is unbound */
		MALI_DEBUG_ASSERT((0 == memA->cookie), ("Arch asked to resize a zero sized block which is in the bound state"));
		/* and that B is bound */
		MALI_DEBUG_ASSERT((0 != memB->cookie), ("Arch asked to resize two unbound blocks"));

		/* Insert block infront of B */
		memA->cookie = memB->cookie;
		memA->cookie_backend = memB->cookie_backend;
		memA->mapping = memB->mapping;
		memA->flags = memB->flags;
		A->mali_addr = B->mali_addr;
	}
	else if (0 == B->size)
	{
		/* assert that B is unbound */
		MALI_DEBUG_ASSERT( (0 == memB->cookie), ("Arch asked to resize a zero sized block which is in the bound state"));
		/* and that A is bound */
		MALI_DEBUG_ASSERT( (0 != memA->cookie), ("Arch asked to resize two unbound blocks"));

		/* Insert block behind A */
		memB->cookie = memA->cookie;
		memB->cookie_backend = memA->cookie_backend;
		memB->mapping = MALI_REINTERPRET_CAST(void*)((MALI_REINTERPRET_CAST(u32)memA->mapping) + A->size);
		memB->flags = memA->flags;
		B->mali_addr = A->mali_addr + A->size;
	}
    else
    {
        /* both a and b are bound - they must have the same cookie_backend */
		MALI_DEBUG_ASSERT((memA->cookie_backend == memB->cookie_backend), ("Arch asked to resize bound blocks that have different cookie_backend"));
    }

	/* check for resize direction */
	if (size < 0)
	{
		/* A shrinks, B expands */
		/* B can't move its start address if it's a head block */
		if( MALI_TRUE == memB->is_head_of_block ) return MALI_ERR_FUNCTION_FAILED;

		/* if A is a tail block we have a special case */
		if ( MALI_TRUE == memA->is_tail_of_block)
		{
			/* A can only be resized if a zero sized block has been inserted behind it */
			MALI_DEBUG_ASSERT(0 == B->size, ("Resize with a non-zero sized block is not supported for head blocks"));
			/* transfer tail status to B */
			memA->is_tail_of_block = MALI_FALSE;
			memB->is_tail_of_block = MALI_TRUE;

		}
	}
	else
	{
		/* A expands, B shrinks */
		/* A can't be resized if it's a tail block */
		if ( MALI_TRUE == memA->is_tail_of_block ) return MALI_ERR_FUNCTION_FAILED;

		/* if B is a head block we have special case */
		if ( MALI_TRUE == memB->is_head_of_block)
		{
			/* B can only be resized if a zero sized block has been inserted in front of it */
			MALI_DEBUG_ASSERT(0 == A->size, ("Resize with a non-zero sized block is not supported for head blocks"));
			/* transfter head status to A */
			memB->is_head_of_block = MALI_FALSE;
			memA->is_head_of_block = MALI_TRUE;
		}
	}

	/* update size and mali address */
	A->size += size;
	B->mali_addr += size;
	B->size -= size;
	memB->mapping = MALI_REINTERPRET_CAST(void*)((MALI_REINTERPRET_CAST(u32)memB->mapping) + size);

	if (0 == A->size) memB->is_head_of_block = memA->is_head_of_block;
	if (0 == B->size) memA->is_tail_of_block = memB->is_tail_of_block;

	MALI_SUCCESS;
}

mali_bool _mali_base_arch_mem_is_full_block(mali_mem * descriptor)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);

	/* check input */
	MALI_CHECK_NON_NULL(descriptor, MALI_FALSE);

	/* release the memory if it's a complete big-block */
	return (( MALI_TRUE == mem->is_head_of_block) && ( MALI_TRUE == mem->is_tail_of_block));
}

#if MALI_USE_DMA_BUF
mali_err_code _mali_base_arch_mem_dma_buf_get_size(u32 *size, u32 mem_fd)
{
	_vr_uk_dma_buf_get_size_s call_arguments;

	MALI_DEBUG_ASSERT_POINTER(size);

	call_arguments.ctx = mali_uk_ctx;
	call_arguments.mem_fd = mem_fd;

	if (_MALI_OSK_ERR_OK != _mali_uku_dma_buf_get_size(&call_arguments))
	{
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}

	*size = call_arguments.size;

	MALI_SUCCESS;
}

mali_err_code _mali_base_arch_mem_dma_buf_attach(mali_mem * descriptor, u32 mem_fd)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);
	u32 minimum_block_size;
	u32 physical_size = mem->embedded_mali_mem.size; /* Need to store this size now, because mali_mmu_virtual_address_range_allocate() will actually change it! */
	_vr_uk_attach_dma_buf_s call_arguments;

	MALI_DEBUG_ASSERT_POINTER(descriptor);

	mem->is_head_of_block = mem->is_tail_of_block = MALI_TRUE;
	mem->flags = mem->embedded_mali_mem.effective_rights;
	mem->mapping_size = physical_size;

	minimum_block_size = mem->embedded_mali_mem.size;

#if HARDWARE_ISSUE_9427
	minimum_block_size += MALI_PAGE_SIZE;
#endif

	/* first allocate a virtual address range */
	if (MALI_ERR_NO_ERROR != mali_mmu_virtual_address_range_allocate(mem, minimum_block_size))
	{
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* ok, map into mali's mmu tables. Does NOT map into cpu space.*/
	call_arguments.ctx          = mali_uk_ctx;
	call_arguments.mem_fd       = mem_fd;
	call_arguments.vr_address = mem->embedded_mali_mem.mali_addr;
	call_arguments.size         = physical_size;
	call_arguments.rights       = mem->flags;
#if HARDWARE_ISSUE_9427
	call_arguments.flags        = _VR_MAP_EXTERNAL_MAP_GUARD_PAGE;
#else
	call_arguments.flags        = 0;
#endif
	call_arguments.cookie       = 0; /* return DD-cookie */

	if(_MALI_OSK_ERR_OK != _mali_uku_attach_dma_buf(&call_arguments))
	{
		mali_mmu_virtual_address_range_free(mem);
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* Memory block mapped, ready to be used */
	mem->cookie_backend = call_arguments.cookie;

	return MALI_ERR_NO_ERROR;
}

void _mali_base_arch_mem_dma_buf_release(mali_mem * descriptor)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);
	_vr_uk_release_dma_buf_s call_arguments;
	u32 ref;

	MALI_DEBUG_ASSERT_POINTER(descriptor);

	ref = _mali_sys_atomic_get(&descriptor->cpu_map_ref_count);
	if (0 != ref)
	{
		_mali_base_arch_mem_dma_buf_unmap(descriptor);
	}

	call_arguments.ctx = mali_uk_ctx;
	call_arguments.cookie = MALI_REINTERPRET_CAST(u32)mem->cookie_backend;

	if (_MALI_OSK_ERR_OK != _mali_uku_release_dma_buf(&call_arguments))
	{
		MALI_DEBUG_ERROR(("failed to release external dma_buf memory\n"));
	}

	mali_mmu_virtual_address_range_free(mem);
	mem->mapping = NULL;
}

mali_err_code _mali_base_arch_mem_dma_buf_map(mali_mem *descriptor)
{
	arch_mem *mem = arch_mem_from_mali_mem(descriptor);
	int mem_fd = descriptor->relationship.dma_buf_fd;

	mem->mapping = mmap(0, mem->mapping_size, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, 0);
	if (MAP_FAILED == mem->mapping)
	{
		MALI_DEBUG_ERROR(("failed to map dma-buf memory\n"));
		return MALI_ERR_FUNCTION_FAILED;
	}

	return MALI_ERR_NO_ERROR;
}

void _mali_base_arch_mem_dma_buf_unmap(mali_mem *descriptor)
{
	arch_mem *mem = arch_mem_from_mali_mem(descriptor);

	descriptor->cached_addr_info.cpu_address = NULL;

	munmap(mem->mapping, mem->mapping_size);
	mem->mapping = NULL;
}
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
mali_err_code _mali_base_arch_mem_ump_mem_attach(mali_mem * descriptor, ump_secure_id secure_id, u32 offset)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);
	u32 minimum_block_size;
	u32 physical_size = mem->embedded_mali_mem.size; /* Need to store this size now, because mali_mmu_virtual_address_range_allocate() will actually change it! */
	_vr_uk_attach_ump_mem_s call_arguments;

	MALI_DEBUG_ASSERT_POINTER(descriptor);

	if ( offset&3 )
	{
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}

	mem->is_head_of_block = mem->is_tail_of_block = MALI_TRUE;
	mem->flags = mem->embedded_mali_mem.effective_rights;

	mem->mapping = ump_mapped_pointer_get( mem->embedded_mali_mem.relationship.ump_mem);
	if ( NULL ==mem->mapping )
	{
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}

	minimum_block_size = mem->embedded_mali_mem.size;

#if HARDWARE_ISSUE_9427
	minimum_block_size += MALI_PAGE_SIZE;
#endif

	/* first allocate a virtual address range */
	MALI_CHECK_NO_ERROR(mali_mmu_virtual_address_range_allocate(mem, minimum_block_size));

	/* ok, map into mali's mmu tables. Does NOT map into cpu space.*/
	call_arguments.ctx          = mali_uk_ctx;
	call_arguments.secure_id    = secure_id;
	call_arguments.vr_address = mem->embedded_mali_mem.mali_addr;
	call_arguments.size         = physical_size; /* Maps the size of the ump, not of the vma, which could be much bigger */
	call_arguments.rights       = mem->flags;
#if HARDWARE_ISSUE_9427
	call_arguments.flags        = _VR_MAP_EXTERNAL_MAP_GUARD_PAGE;
#else
	call_arguments.flags        = 0;
#endif
	call_arguments.cookie       = 0; /* return DD-cookie */

	if( _MALI_OSK_ERR_OK != _mali_uku_attach_ump_mem( &call_arguments ) )
	{
		/* failed, release virtual address range */
		mali_mmu_virtual_address_range_free(mem);
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}

	/* Memory block mapped, ready to be used */
	mem->cookie_backend = call_arguments.cookie;

	/* Adding the offset specified by the user */
	mem->mapping = MALI_REINTERPRET_CAST(void*)((MALI_REINTERPRET_CAST(u32)mem->mapping) + offset);

	MALI_SUCCESS;
}

void _mali_base_arch_mem_ump_mem_release(mali_mem * descriptor)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);
    _vr_uk_release_ump_mem_s call_arguments;

	MALI_DEBUG_ASSERT_POINTER(descriptor);

	ump_mapped_pointer_release( mem->embedded_mali_mem.relationship.ump_mem);

    call_arguments.ctx = mali_uk_ctx;
    call_arguments.cookie = MALI_REINTERPRET_CAST(u32)mem->cookie_backend;

    if (_MALI_OSK_ERR_OK != _mali_uku_release_ump_mem( &call_arguments ))
    {
        MALI_DEBUG_ERROR( ("failed to unmap external UMP memory\n") );
    }

	mali_mmu_virtual_address_range_free(mem);
	mem->mapping = NULL;
}
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 */

mali_err_code _mali_base_arch_mem_add_phys_mem(mali_mem * descriptor, u32 phys_addr, u32 size, void * mapping, u32 access_rights)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);
	MALI_DEBUG_ASSERT_POINTER(descriptor);

	mem->is_head_of_block = mem->is_tail_of_block = MALI_TRUE;

	MALI_CHECK_NO_ERROR(backend_mmu_map_external_memory(mem, phys_addr, size, access_rights));
	/* update mapping after the map call needed as the shared code sets the mapping as well */
	mem->flags = access_rights;
	mem->mapping = mapping;
	mem->embedded_mali_mem.size = size;

	MALI_SUCCESS;
}

void _mali_base_arch_release_phys_mem(mali_mem * descriptor)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);
	MALI_DEBUG_ASSERT_POINTER(descriptor);
	backend_mmu_unmap_external_memory(mem);
}

MALI_STATIC mali_err_code memory_initialize(mali_base_ctx_handle ctx)
{
	MALI_IGNORE(ctx);

	MALI_CHECK_NO_ERROR(_mali_base_arch_open());

	if (MALI_ERR_NO_ERROR != mali_virtual_memory_area_create())
	{
		_mali_base_arch_close();
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}
	MALI_SUCCESS;
}

MALI_STATIC void memory_terminate(mali_base_ctx_handle ctx)
{
	MALI_IGNORE(ctx);
	mali_virtual_memory_area_destroy();
	_mali_base_arch_close();
}

MALI_STATIC mali_err_code backend_mmu_get_memory(u32 cache_settings, u32 minimum_block_size, mali_mem * descriptor)
{
	/* temporary descriptor used during allocation to be able to leave descriptor untouched on error */
	arch_mem temp_descriptor;
	mali_err_code err;
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);

	/* input validation */
	MALI_CHECK_NON_NULL(descriptor, MALI_ERR_FUNCTION_FAILED);

	/* change block size to an acceptable size */
	if (minimum_block_size < SLOT_SIZE) minimum_block_size = SLOT_SIZE; /* round up to at least one block */
	else minimum_block_size = (minimum_block_size + SLOT_SIZE - 1) & ~(SLOT_SIZE-1); /* align to block size */

	MALI_CHECK(minimum_block_size != 0, MALI_ERR_FUNCTION_FAILED); /* caught the u32 wrapping when rounding up */

	/* copy input to temp descriptor */

#if defined(__SYMBIAN32__)
    _mali_osu_memcpy(&temp_descriptor, mem, sizeof(temp_descriptor));
#else
	memcpy(&temp_descriptor, mem, sizeof(temp_descriptor));
#endif

	/* get a virtual address space */
	err = mali_mmu_virtual_address_range_allocate(&temp_descriptor, minimum_block_size);

	if (MALI_ERR_NO_ERROR == err)
	{
		/* assign physical memory to the virtual area */

		u32 diff = temp_descriptor.embedded_mali_mem.size - minimum_block_size;
		if (diff > 0)
		{
			/* can add guard pages around */
			MALI_DEBUG_PRINT(4, ("%d guard pages in front and %d behind\n", diff / 8192, diff / 8192));

			temp_descriptor.embedded_mali_mem.mali_addr += diff / 2;
			temp_descriptor.embedded_mali_mem.size = minimum_block_size;
		}
		else
		{
			MALI_DEBUG_PRINT(4, ("No guard pages possible\n"));
		}

		err = backend_mmu_physical_memory_allocate(&temp_descriptor, cache_settings);
		if (MALI_ERR_NO_ERROR == err)
		{
			/* all ok, copy temp_descriptor to output descriptor*/
#if defined(__SYMBIAN32__)
            _mali_osu_memcpy(mem, &temp_descriptor, sizeof(temp_descriptor));
#else
			memcpy(mem, &temp_descriptor, sizeof(temp_descriptor));
#endif
			MALI_SUCCESS;
		}
		/* physical alloc failed, release virtual area */
		mali_mmu_virtual_address_range_free(&temp_descriptor);
	}

	/* an error occured */
	MALI_ERROR(err);
}

MALI_STATIC void backend_mmu_release_memory(mali_mem * descriptor)
{
	arch_mem * mem = arch_mem_from_mali_mem(descriptor);

	MALI_DEBUG_ASSERT_POINTER(descriptor);
	MALI_DEBUG_ASSERT_POINTER((void*)mem->cookie);
	MALI_DEBUG_ASSERT( ( MALI_TRUE == mem->is_head_of_block) && ( MALI_TRUE == mem->is_tail_of_block), ("Releasing memory block which is not a big block!"));
	MALI_DEBUG_ASSERT( ((MALI_REINTERPRET_CAST(u32)mem->mapping) & 4095) == 0, ("Freeing a non page aligned memory block"));

	backend_mmu_physical_memory_free(mem);
	mali_mmu_virtual_address_range_free(mem);
}

MALI_STATIC mali_err_code backend_mmu_physical_memory_allocate(arch_mem * descriptor, u32 cache_settings)
{
    _vr_uk_mem_mmap_s args = { 0, 0, 0, 0, 0, 0, 0, 0 };

    args.ctx = mali_uk_ctx;
    args.phys_addr = descriptor->embedded_mali_mem.mali_addr;
    args.size = descriptor->embedded_mali_mem.size;

    args.cache_settings = cache_settings;

    MALI_CHECK( _MALI_OSK_ERR_OK == _mali_uku_mem_mmap( &args ), MALI_ERR_FUNCTION_FAILED );

    descriptor->mapping = args.mapping;
	MALI_DEBUG_ASSERT( ((MALI_REINTERPRET_CAST(u32)descriptor->mapping) & 4095) == 0, ("Allocated a non page aligned memory block 0x%x?", descriptor->mapping));
	descriptor->cookie_backend = (u32)args.cookie;
	descriptor->flags = MALI_PP_READ | MALI_PP_WRITE | MALI_GP_READ | MALI_GP_WRITE | MALI_CPU_READ | MALI_CPU_WRITE;
	MALI_SUCCESS;
}

MALI_STATIC void backend_mmu_physical_memory_free(arch_mem * descriptor)
{
    _vr_uk_mem_munmap_s args = { 0, 0, 0, 0 };

    MALI_DEBUG_ASSERT_POINTER(descriptor->mapping);

    args.ctx = mali_uk_ctx;
    args.mapping = descriptor->mapping;
    args.size = descriptor->embedded_mali_mem.size;
	args.cookie = descriptor->cookie_backend;

    if (_MALI_OSK_ERR_OK != _mali_uku_mem_munmap( &args ))
    {
        MALI_DEBUG_ERROR( ("munmap failed to free physical memory\n") );
    }

	descriptor->mapping = NULL;
}

MALI_STATIC mali_err_code backend_mmu_map_external_memory(arch_mem * descriptor, u32 phys_addr, u32 size, u32 access_rights)
{
	u32 minimum_block_size = size; /* size should be the same as mem->embedded_mali_mem.size */
	u32 physical_size = size;
	_vr_uk_map_external_mem_s call_arguments = {0, 0, 0, 0, 0, 0, 0};

#if HARDWARE_ISSUE_9427
	minimum_block_size += MALI_PAGE_SIZE;
#endif

	/* change block size to an acceptable size */
	if (minimum_block_size < SLOT_SIZE) minimum_block_size = SLOT_SIZE; /* round up to at least one block */
	else minimum_block_size = (minimum_block_size + SLOT_SIZE - 1) & ~(SLOT_SIZE-1); /* align to block size */

	/* first allocate a virtual address range */
	MALI_CHECK_NO_ERROR(mali_mmu_virtual_address_range_allocate(descriptor, minimum_block_size));

	/* ok, do the actual mapping */
	call_arguments.ctx          = mali_uk_ctx;
	call_arguments.phys_addr    = phys_addr;
	call_arguments.size         = physical_size;
	call_arguments.vr_address = descriptor->embedded_mali_mem.mali_addr;
	call_arguments.rights       = access_rights;
#if HARDWARE_ISSUE_9427
	call_arguments.flags        = _VR_MAP_EXTERNAL_MAP_GUARD_PAGE;
#else
	call_arguments.flags        = 0;
#endif
	call_arguments.cookie       = 0;

	if (_MALI_OSK_ERR_OK != _mali_uku_map_external_mem( &call_arguments ))
	{
		/* call failed, release virtual address range */
		mali_mmu_virtual_address_range_free(descriptor);
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}

	/* Memory block mapped, ready to be used */
	descriptor->cookie_backend = call_arguments.cookie;
	MALI_SUCCESS;

}

MALI_STATIC void backend_mmu_unmap_external_memory(arch_mem * descriptor)
{
    _vr_uk_unmap_external_mem_s call_arguments;

    call_arguments.ctx = mali_uk_ctx;
    call_arguments.cookie = MALI_REINTERPRET_CAST(u32)descriptor->cookie_backend;

	mali_mmu_virtual_address_range_free(descriptor);
	descriptor->mapping = NULL;
	if (_MALI_OSK_ERR_OK != _mali_uku_unmap_external_mem( &call_arguments ))
    {
        MALI_DEBUG_ERROR( ("failed to unmap external memory\n") );
    }
}

MALI_EXPORT u32 _mali_base_arch_mmu_dump_size_get(void)
{
	_vr_uk_query_mmu_page_table_dump_size_s call_data = { NULL, 0 };

	call_data.ctx = mali_uk_ctx;

	MALI_CHECK( _MALI_OSK_ERR_OK == _mali_uku_query_mmu_page_table_dump_size( &call_data ),
				0 );

    return call_data.size;
}

MALI_EXPORT mali_err_code _mali_base_arch_mmu_dump_get(mali_mmu_dump * dump)
{
    _vr_uk_dump_mmu_page_table_s call_data = { NULL, 0, NULL, 0, NULL, 0, NULL };
    _mali_osk_errcode_t err;

	MALI_DEBUG_ASSERT_POINTER(dump);
	MALI_DEBUG_ASSERT_POINTER(dump->storage_buffer);

	call_data.size = dump->storage_buffer_size;
	call_data.buffer = dump->storage_buffer;

	call_data.ctx = mali_uk_ctx;
    err = _mali_uku_dump_mmu_page_table( &call_data );
    if (_MALI_OSK_ERR_OK == err)
    {
	    MALI_DEBUG_ASSERT(0 == (call_data.register_writes_size % 8), ("Invalid size of register write buffer, not a multiple of 8\n"));
	    MALI_DEBUG_ASSERT(0 == (call_data.page_table_dump_size%(4096+4)), ("Bad page table size\n"));

	    dump->num_registers = call_data.register_writes_size / 8;
	    dump->register_writes = call_data.register_writes;
	    dump->num_pages = call_data.page_table_dump_size / (4096+4);
	    dump->pages = call_data.page_table_dump;
	    MALI_SUCCESS;
    } else if (_MALI_OSK_ERR_NOMEM == err)
    {
	    MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
    }
    else
    {
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
    }
}
