/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/mali_mem_pool.h>
#include <mali_system.h>
#include <base/mali_memory.h>

#define BLOCK_SIZE 65536
#define MAX_WASTED 4096
#define ALLOC_ALIGNMENT 64
#define BLOCKS_PER_SUPERBLOCK 128

typedef struct _mali_mmp_block {
	mali_mem_handle handle;
	mali_addr base_mali_address;
	void *mapped_address;
	u32 size;
	u32 allocated;
} mali_mmp_block;

typedef struct _mali_mmp_superblock {
	u32 n_blocks;
	struct _mali_mmp_superblock *prev;
	mali_mmp_block blocks[BLOCKS_PER_SUPERBLOCK];
} mali_mmp_superblock;

MALI_STATIC mali_mmp_superblock *_mali_mem_pool_new_superblock()
{
	mali_mmp_superblock *sblock;
	sblock = _mali_sys_malloc(sizeof(mali_mmp_superblock));
	if (sblock == NULL) return NULL;

	sblock->n_blocks = 0;
	sblock->prev = NULL;

	return sblock;
}

MALI_STATIC mali_mmp_block *_mali_mem_pool_new_block(mali_mem_pool *pool, u32 size)
{
	mali_mmp_block *block;
	if (pool->last_superblock->n_blocks == BLOCKS_PER_SUPERBLOCK)
	{
		/* New superblock */
		mali_mmp_superblock *sblock;
		sblock = _mali_mem_pool_new_superblock();
		if (sblock == NULL) return NULL;
		sblock->prev = pool->last_superblock;
		pool->last_superblock = sblock;
		MALI_DEBUG_ASSERT(pool->last_superblock->n_blocks == 0, ("Internal consistency problem: new superblock already has allocated blocks\n"));
	}

	/* Space in superblock */
	block = &pool->last_superblock->blocks[pool->last_superblock->n_blocks];

	/* To enable gp cache read alloc: Add MALI_GP_L2_ALLOC to the last bitmask here:*/
	block->handle = _mali_mem_alloc(pool->base_ctx, size, ALLOC_ALIGNMENT, MALI_PP_READ|MALI_GP_READ|MALI_GP_WRITE|MALI_CPU_WRITE);
	if (block->handle == MALI_NO_HANDLE) return NULL;

	block->base_mali_address = _mali_mem_mali_addr_get(block->handle, 0);
	block->mapped_address = NULL;
	block->size = size;
	block->allocated = 0;

	pool->last_superblock->n_blocks++;

	return block;
}

MALI_STATIC mali_bool _mali_mem_pool_map_block(mali_mmp_block *block)
{
	void *map_ptr;

	MALI_DEBUG_ASSERT(block->mapped_address == NULL, ("Double map of block in mali memory pool"));

	/* Map unallocated part of block */
	map_ptr = _mali_mem_ptr_map_area(block->handle, block->allocated, block->size - block->allocated, ALLOC_ALIGNMENT, MALI_MEM_PTR_WRITABLE|MALI_MEM_PTR_NO_PRE_UPDATE);
	if (map_ptr == NULL) return MALI_FALSE;
	block->mapped_address = map_ptr;

	return MALI_TRUE;
}

MALI_STATIC void _mali_mem_pool_unmap_block(mali_mmp_block *block)
{
	MALI_DEBUG_ASSERT(block->mapped_address != NULL, ("Unmap of unmapped block in mali memory pool"));

	_mali_mem_ptr_unmap_area(block->handle);
	block->mapped_address = NULL;
}

MALI_EXPORT mali_err_code _mali_mem_pool_init(mali_mem_pool *pool, mali_base_ctx_handle base_ctx)
{
	pool->base_ctx = base_ctx;
	pool->current_block = NULL;
	pool->map_nesting = 0;
	pool->last_superblock = NULL;

	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT void _mali_mem_pool_destroy(mali_mem_pool *pool)
{
	mali_mmp_superblock *sblock, *sblock_prev;

	if (pool->last_superblock == NULL)
	{
		/* No pool */
		return;
	}

	while (pool->map_nesting > 0)
	{
		_mali_mem_pool_unmap(pool);
	}

	for (sblock = pool->last_superblock ; sblock != NULL ;)
	{
		int i;
		for (i = (int)sblock->n_blocks - 1 ; i >= 0 ; i--)
		{
			_mali_mem_free(sblock->blocks[i].handle);
			sblock->blocks[i].handle = NULL;
		}

		sblock_prev = sblock->prev;
		_mali_sys_free(sblock);
		sblock = sblock_prev;
	}

	pool->last_superblock = NULL;
	pool->current_block = NULL;
}

MALI_EXPORT mali_err_code _mali_mem_pool_map(mali_mem_pool *pool)
{
	MALI_DEBUG_ASSERT_POINTER(pool);

	/* Map memory if not already mapped */
	if (pool->map_nesting > 0)
	{
		/* Already mapped */
		pool->map_nesting++;
		return MALI_ERR_NO_ERROR;
	}

	if ( pool->current_block == NULL )
	{
		/* Do a first-time initialization */
		pool->last_superblock = _mali_mem_pool_new_superblock();
		if (pool->last_superblock == NULL) return MALI_ERR_OUT_OF_MEMORY;
		pool->current_block = _mali_mem_pool_new_block(pool, BLOCK_SIZE);
		if (pool->current_block == NULL)
		{
			_mali_mem_pool_destroy(pool);
			return MALI_ERR_OUT_OF_MEMORY;
		}
	}

	/* Never map an empty block */
	if (pool->current_block->allocated == pool->current_block->size)
	{
		mali_mmp_block *temp_block = _mali_mem_pool_new_block( pool, BLOCK_SIZE );
		if (NULL == temp_block) return MALI_ERR_OUT_OF_MEMORY;

		pool->current_block = temp_block;
	}

	if (_mali_mem_pool_map_block(pool->current_block))
	{
		pool->map_nesting++;
		return MALI_ERR_NO_ERROR;
	} else {
		return MALI_ERR_FUNCTION_FAILED;
	}
}

MALI_EXPORT void _mali_mem_pool_unmap(mali_mem_pool *pool)
{
	mali_mmp_superblock *sblock;

	MALI_DEBUG_ASSERT(pool->map_nesting > 0, ("Unmap of unmapped mali memory pool"));

	if (--pool->map_nesting > 0)
	{
		/* Still within mapped region */
		return;
	}

	for (sblock = pool->last_superblock ; sblock != NULL ; sblock = sblock->prev)
	{
		int i;
		for (i = sblock->n_blocks-1 ; i >= 0 ; i--)
		{
			if (sblock->blocks[i].mapped_address == NULL)
			{
				/* No more mapped blocks */
				break;
			}
			_mali_mem_pool_unmap_block(&sblock->blocks[i]);
		}
	}

	/* Current block might be separate from the other mapped blocks */
	if (pool->current_block != NULL && pool->current_block->mapped_address != NULL)
	{
		_mali_mem_pool_unmap_block(pool->current_block);
	}
}

MALI_STATIC mali_mmp_block* _mem_pool_set_new_block( mali_mem_pool *pool, u32 capacity, u32 aligned_size )
{
	mali_mmp_block*	block;

	if (capacity > MAX_WASTED || aligned_size > BLOCK_SIZE)
	{
		/* Allocate dedicated block for allocation */
		block = _mali_mem_pool_new_block(pool, aligned_size);
		if (block == NULL) return NULL;
	} else {
		/* Allocate new current block */
		block = _mali_mem_pool_new_block(pool, BLOCK_SIZE);
		if (block == NULL) return NULL;
		pool->current_block = block;
	}
	_mali_mem_pool_map_block(block);

	return block;
}


MALI_EXPORT void *_mali_mem_pool_alloc(mali_mem_pool *pool, u32 size, mali_addr *mali_mem_addr)
{
	mali_mmp_block*		block			= pool->current_block;
	u32					aligned_size	= (size + (ALLOC_ALIGNMENT-1)) & ~(ALLOC_ALIGNMENT-1);
	u32					capacity		= block->size - block->allocated;

	MALI_DEBUG_ASSERT(pool->map_nesting > 0, ("Allocation from unmapped mali memory pool"));
	MALI_DEBUG_ASSERT(mali_mem_addr != NULL, ("Allocation without request for mali address"));

	if ( aligned_size > capacity )
	{
		block = _mem_pool_set_new_block( pool, capacity, aligned_size );
		if( NULL == block) return NULL;
	}

	MALI_DEBUG_ASSERT(block->size - block->allocated >= aligned_size, ("Internal block overflow in mali memory pool"));
	MALI_DEBUG_ASSERT(block->mapped_address != NULL, ("Internal unmapped block in mali memory pool"));

	{
		void*	address	= block->mapped_address;
		u32		alloced	= block->allocated;

		block->mapped_address	= (char *)address + aligned_size;
		block->allocated		= alloced + aligned_size;

		*mali_mem_addr	= block->base_mali_address + alloced;
		return address;
	}
}

MALI_EXPORT void *_mali_mem_pool_alloc_with_handle_and_offset(mali_mem_pool *pool, u32 size, mali_addr *mali_mem_addr, mali_mem_handle *handle, u32 *offset)
{
	mali_mmp_block*		block			= pool->current_block;
	u32					aligned_size	= (size + (ALLOC_ALIGNMENT-1)) & ~(ALLOC_ALIGNMENT-1);
	u32					capacity		= block->size - block->allocated;

	MALI_DEBUG_ASSERT(pool->map_nesting > 0, ("Allocation from unmapped mali memory pool"));
	MALI_DEBUG_ASSERT(mali_mem_addr != NULL, ("Allocation without request for mali address"));

	if ( aligned_size > capacity )
	{
		block = _mem_pool_set_new_block( pool, capacity, aligned_size );
		if( NULL == block) return NULL;
	}

	MALI_DEBUG_ASSERT(block->size - block->allocated >= aligned_size, ("Internal block overflow in mali memory pool"));
	MALI_DEBUG_ASSERT(block->mapped_address != NULL, ("Internal unmapped block in mali memory pool"));

	{
		void*	address	= block->mapped_address;
		u32		alloced	= block->allocated;

		block->mapped_address	= (char *)address + aligned_size;
		block->allocated		= alloced + aligned_size;

		*mali_mem_addr	= block->base_mali_address + alloced;
		*handle			= block->handle;
		*offset			= alloced;
		return address;
	}
}
