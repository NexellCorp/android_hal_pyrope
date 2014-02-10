/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "vr_kernel_common.h"
#include "vr_kernel_memory_engine.h"
#include "vr_block_allocator.h"
#include "vr_osk.h"

#define VR_BLOCK_SIZE (256UL * 1024UL)  /* 256 kB, remember to keep the ()s */

typedef struct block_info
{
	struct block_info * next;
} block_info;

/* The structure used as the handle produced by block_allocator_allocate,
 * and removed by block_allocator_release */
typedef struct block_allocator_allocation
{
	/* The list will be released in reverse order */
	block_info *last_allocated;
	vr_allocation_engine * engine;
	vr_memory_allocation * descriptor;
	u32 start_offset;
	u32 mapping_length;
} block_allocator_allocation;


typedef struct block_allocator
{
    _vr_osk_lock_t *mutex;
	block_info * all_blocks;
	block_info * first_free;
	u32 base;
	u32 cpu_usage_adjust;
	u32 num_blocks;
} block_allocator;

VR_STATIC_INLINE u32 get_phys(block_allocator * info, block_info * block);
static vr_physical_memory_allocation_result block_allocator_allocate(void* ctx, vr_allocation_engine * engine,  vr_memory_allocation * descriptor, u32* offset, vr_physical_memory_allocation * alloc_info);
static void block_allocator_release(void * ctx, void * handle);
static vr_physical_memory_allocation_result block_allocator_allocate_page_table_block(void * ctx, vr_page_table_block * block);
static void block_allocator_release_page_table_block( vr_page_table_block *page_table_block );
static void block_allocator_destroy(vr_physical_memory_allocator * allocator);
static u32 block_allocator_stat(vr_physical_memory_allocator * allocator);

vr_physical_memory_allocator * vr_block_allocator_create(u32 base_address, u32 cpu_usage_adjust, u32 size, const char *name)
{
	vr_physical_memory_allocator * allocator;
	block_allocator * info;
	u32 usable_size;
	u32 num_blocks;

	usable_size = size & ~(VR_BLOCK_SIZE - 1);
	VR_DEBUG_PRINT(3, ("VR block allocator create for region starting at 0x%08X length 0x%08X\n", base_address, size));
	VR_DEBUG_PRINT(4, ("%d usable bytes\n", usable_size));
	num_blocks = usable_size / VR_BLOCK_SIZE;
	VR_DEBUG_PRINT(4, ("which becomes %d blocks\n", num_blocks));

	if (usable_size == 0)
	{
		VR_DEBUG_PRINT(1, ("Memory block of size %d is unusable\n", size));
		return NULL;
	}

	allocator = _vr_osk_malloc(sizeof(vr_physical_memory_allocator));
	if (NULL != allocator)
	{
		info = _vr_osk_malloc(sizeof(block_allocator));
		if (NULL != info)
		{
            info->mutex = _vr_osk_lock_init( _VR_OSK_LOCKFLAG_ORDERED, 0, _VR_OSK_LOCK_ORDER_MEM_INFO);
            if (NULL != info->mutex)
            {
        		info->all_blocks = _vr_osk_malloc(sizeof(block_info) * num_blocks);
			    if (NULL != info->all_blocks)
			    {
				    u32 i;
				    info->first_free = NULL;
				    info->num_blocks = num_blocks;

				    info->base = base_address;
				    info->cpu_usage_adjust = cpu_usage_adjust;

				    for ( i = 0; i < num_blocks; i++)
				    {
					    info->all_blocks[i].next = info->first_free;
					    info->first_free = &info->all_blocks[i];
				    }

				    allocator->allocate = block_allocator_allocate;
				    allocator->allocate_page_table_block = block_allocator_allocate_page_table_block;
				    allocator->destroy = block_allocator_destroy;
				    allocator->stat = block_allocator_stat;
				    allocator->ctx = info;
					allocator->name = name;

				    return allocator;
			    }
                _vr_osk_lock_term(info->mutex);
            }
			_vr_osk_free(info);
		}
		_vr_osk_free(allocator);
	}

	return NULL;
}

static void block_allocator_destroy(vr_physical_memory_allocator * allocator)
{
	block_allocator * info;
	VR_DEBUG_ASSERT_POINTER(allocator);
	VR_DEBUG_ASSERT_POINTER(allocator->ctx);
	info = (block_allocator*)allocator->ctx;

	_vr_osk_free(info->all_blocks);
    _vr_osk_lock_term(info->mutex);
	_vr_osk_free(info);
	_vr_osk_free(allocator);
}

VR_STATIC_INLINE u32 get_phys(block_allocator * info, block_info * block)
{
	return info->base + ((block - info->all_blocks) * VR_BLOCK_SIZE);
}

static vr_physical_memory_allocation_result block_allocator_allocate(void* ctx, vr_allocation_engine * engine, vr_memory_allocation * descriptor, u32* offset, vr_physical_memory_allocation * alloc_info)
{
	block_allocator * info;
	u32 left;
	block_info * last_allocated = NULL;
	vr_physical_memory_allocation_result result = VR_MEM_ALLOC_NONE;
	block_allocator_allocation *ret_allocation;

	VR_DEBUG_ASSERT_POINTER(ctx);
	VR_DEBUG_ASSERT_POINTER(descriptor);
	VR_DEBUG_ASSERT_POINTER(offset);
	VR_DEBUG_ASSERT_POINTER(alloc_info);

	info = (block_allocator*)ctx;
	left = descriptor->size - *offset;
	VR_DEBUG_ASSERT(0 != left);

	if (_VR_OSK_ERR_OK != _vr_osk_lock_wait(info->mutex, _VR_OSK_LOCKMODE_RW)) return VR_MEM_ALLOC_INTERNAL_FAILURE;

	ret_allocation = _vr_osk_malloc( sizeof(block_allocator_allocation) );

	if ( NULL == ret_allocation )
	{
		/* Failure; try another allocator by returning VR_MEM_ALLOC_NONE */
		_vr_osk_lock_signal(info->mutex, _VR_OSK_LOCKMODE_RW);
		return result;
	}

	ret_allocation->start_offset = *offset;
	ret_allocation->mapping_length = 0;

	while ((left > 0) && (info->first_free))
	{
		block_info * block;
		u32 phys_addr;
		u32 padding;
		u32 current_mapping_size;

		block = info->first_free;
		info->first_free = info->first_free->next;
		block->next = last_allocated;
		last_allocated = block;

		phys_addr = get_phys(info, block);

		padding = *offset & (VR_BLOCK_SIZE-1);

 		if (VR_BLOCK_SIZE - padding < left)
		{
			current_mapping_size = VR_BLOCK_SIZE - padding;
		}
		else
		{
			current_mapping_size = left;
		}

		if (_VR_OSK_ERR_OK != vr_allocation_engine_map_physical(engine, descriptor, *offset, phys_addr + padding, info->cpu_usage_adjust, current_mapping_size))
		{
			VR_DEBUG_PRINT(1, ("Mapping of physical memory  failed\n"));
			result = VR_MEM_ALLOC_INTERNAL_FAILURE;
			vr_allocation_engine_unmap_physical(engine, descriptor, ret_allocation->start_offset, ret_allocation->mapping_length, (_vr_osk_mem_mapregion_flags_t)0);

			/* release all memory back to the pool */
			while (last_allocated)
			{
				/* This relinks every block we've just allocated back into the free-list */
				block = last_allocated->next;
				last_allocated->next = info->first_free;
				info->first_free = last_allocated;
				last_allocated = block;
			}

			break;
		}

		*offset += current_mapping_size;
		left -= current_mapping_size;
		ret_allocation->mapping_length += current_mapping_size;
	}

	_vr_osk_lock_signal(info->mutex, _VR_OSK_LOCKMODE_RW);

	if (last_allocated)
	{
		if (left) result = VR_MEM_ALLOC_PARTIAL;
		else result = VR_MEM_ALLOC_FINISHED;

		/* Record all the information about this allocation */
		ret_allocation->last_allocated = last_allocated;
		ret_allocation->engine = engine;
		ret_allocation->descriptor = descriptor;

		alloc_info->ctx = info;
		alloc_info->handle = ret_allocation;
		alloc_info->release = block_allocator_release;
	}
	else
	{
		/* Free the allocation information - nothing to be passed back */
		_vr_osk_free( ret_allocation );
	}

	return result;
}

static void block_allocator_release(void * ctx, void * handle)
{
	block_allocator * info;
	block_info * block, * next;
	block_allocator_allocation *allocation;

	VR_DEBUG_ASSERT_POINTER(ctx);
	VR_DEBUG_ASSERT_POINTER(handle);

	info = (block_allocator*)ctx;
	allocation = (block_allocator_allocation*)handle;
	block = allocation->last_allocated;

	VR_DEBUG_ASSERT_POINTER(block);

	if (_VR_OSK_ERR_OK != _vr_osk_lock_wait(info->mutex, _VR_OSK_LOCKMODE_RW))
	{
		VR_DEBUG_PRINT(1, ("allocator release: Failed to get mutex\n"));
		return;
	}

	/* unmap */
	vr_allocation_engine_unmap_physical(allocation->engine, allocation->descriptor, allocation->start_offset, allocation->mapping_length, (_vr_osk_mem_mapregion_flags_t)0);

	while (block)
	{
		VR_DEBUG_ASSERT(!((block < info->all_blocks) || (block > (info->all_blocks + info->num_blocks))));

		next = block->next;

		/* relink into free-list */
		block->next = info->first_free;
		info->first_free = block;

		/* advance the loop */
		block = next;
	}

	_vr_osk_lock_signal(info->mutex, _VR_OSK_LOCKMODE_RW);

	_vr_osk_free( allocation );
}


static vr_physical_memory_allocation_result block_allocator_allocate_page_table_block(void * ctx, vr_page_table_block * block)
{
	block_allocator * info;
	vr_physical_memory_allocation_result result = VR_MEM_ALLOC_INTERNAL_FAILURE;

	VR_DEBUG_ASSERT_POINTER(ctx);
	VR_DEBUG_ASSERT_POINTER(block);
	info = (block_allocator*)ctx;

	if (_VR_OSK_ERR_OK != _vr_osk_lock_wait(info->mutex, _VR_OSK_LOCKMODE_RW)) return VR_MEM_ALLOC_INTERNAL_FAILURE;

	if (NULL != info->first_free)
	{
		void * virt;
		u32 phys;
		u32 size;
		block_info * alloc;
		alloc = info->first_free;

		phys = get_phys(info, alloc); /* Does not modify info or alloc */
		size = VR_BLOCK_SIZE; /* Must be multiple of VR_MMU_PAGE_SIZE */
		virt = _vr_osk_mem_mapioregion( phys, size, "VR block allocator page tables" );

		/* Failure of _vr_osk_mem_mapioregion will result in VR_MEM_ALLOC_INTERNAL_FAILURE,
		 * because it's unlikely another allocator will be able to map in. */

		if ( NULL != virt )
		{
			block->ctx = info; /* same as incoming ctx */
			block->handle = alloc;
			block->phys_base = phys;
			block->size = size;
			block->release = block_allocator_release_page_table_block;
			block->mapping = virt;

			info->first_free = alloc->next;

			alloc->next = NULL; /* Could potentially link many blocks together instead */

			result = VR_MEM_ALLOC_FINISHED;
		}
	}
	else result = VR_MEM_ALLOC_NONE;

	_vr_osk_lock_signal(info->mutex, _VR_OSK_LOCKMODE_RW);

	return result;
}


static void block_allocator_release_page_table_block( vr_page_table_block *page_table_block )
{
	block_allocator * info;
	block_info * block, * next;

	VR_DEBUG_ASSERT_POINTER( page_table_block );

	info = (block_allocator*)page_table_block->ctx;
	block = (block_info*)page_table_block->handle;

	VR_DEBUG_ASSERT_POINTER(info);
	VR_DEBUG_ASSERT_POINTER(block);


	if (_VR_OSK_ERR_OK != _vr_osk_lock_wait(info->mutex, _VR_OSK_LOCKMODE_RW))
	{
		VR_DEBUG_PRINT(1, ("allocator release: Failed to get mutex\n"));
		return;
	}

	/* Unmap all the physical memory at once */
	_vr_osk_mem_unmapioregion( page_table_block->phys_base, page_table_block->size, page_table_block->mapping );

	/** @note This loop handles the case where more than one block_info was linked.
	 * Probably unnecessary for page table block releasing. */
	while (block)
	{
		next = block->next;

		VR_DEBUG_ASSERT(!((block < info->all_blocks) || (block > (info->all_blocks + info->num_blocks))));

		block->next = info->first_free;
		info->first_free = block;

		block = next;
	}

	_vr_osk_lock_signal(info->mutex, _VR_OSK_LOCKMODE_RW);
}

static u32 block_allocator_stat(vr_physical_memory_allocator * allocator)
{
	block_allocator * info;
	block_info *block;
	u32 free_blocks = 0;

	VR_DEBUG_ASSERT_POINTER(allocator);

	info = (block_allocator*)allocator->ctx;
	block = info->first_free;

	while(block)
	{
		free_blocks++;
		block = block->next;
	}
	return (info->num_blocks - free_blocks) * VR_BLOCK_SIZE;
}
