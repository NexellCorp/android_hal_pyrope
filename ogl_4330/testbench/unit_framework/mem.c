/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_tpi.h"
#include "mem.h"
#include <stdlib.h>

#ifdef UNIT_TEST
#include <stdio.h>
#include <assert.h>
#endif /* UNIT_TEST */

#ifdef __SYMBIAN32__
#include "suite.h"
#endif

static const size_t default_block_size = 4096;

/* Rounds size up to fit platform alignment */
static size_t aligned_size(size_t size)
{
	return (size + MEM_PLATFORM_ALIGNMENT - 1) & ~(MEM_PLATFORM_ALIGNMENT-1);
}


/* Blocks are allocated from the underlying OS with size (block structure + alignment + block_size) */
/*@null@*/ static mempool_block *allocate_block(size_t size)
{
	size_t headersize = aligned_size(sizeof(mempool_block));

	mempool_block *block;
#ifdef __SYMBIAN32__
	MEM_CHECK(block = MALLOC(headersize + size), NULL);
#else
	MEM_CHECK(block = malloc(headersize + size), NULL);
#endif
	block->allocated = headersize + size;
	block->size = size;
	block->space_used = 0;
	block->data = ((char*)block) + headersize;

	return block;
}


/* Initializes the mempool structure with one available block */
memerr _test_mempool_init(mempool *pool, size_t block_size)
{
	mempool_block *block;

	if( NULL == pool ) return MEM_ERROR;

	if ( MALI_TPI_FALSE == mali_tpi_mutex_init(&pool->mutex) )
	{
		return MEM_ERROR;
	}

	pool->block_size = block_size ? block_size : default_block_size;
	MEM_CHECK(block = allocate_block(pool->block_size), MEM_ERROR);

	block->previous_block = 0;

	pool->last_block = block;
	pool->allocations = 0;
	pool->max_allocations = -1;

	return MEM_OK;
}


/* Frees all allocated block */
memerr _test_mempool_destroy(mempool *pool)
{
	if (!pool || !pool->last_block)
	{
		return MEM_ERROR;
	}
	else
	{
		mempool_block *block = pool->last_block;
		while (block)
		{
			mempool_block *prev = block->previous_block;
#ifdef __SYMBIAN32__
			FREE(block);
#else
			free(block);
#endif
			block = prev;
		}
		pool->last_block = 0;

		mali_tpi_mutex_term( &pool->mutex );

		return MEM_OK;
	}
}


/* Returns free space from the memory pool, allocating new blocks if necessary */
void *_test_mempool_alloc(mempool *pool, size_t size)
{
	mempool_block *lastblock;

	if (!pool)
	{
		return NULL;
	}
	mali_tpi_mutex_lock( &pool->mutex );

	lastblock = pool->last_block;
	if (!lastblock)
	{
		mali_tpi_mutex_unlock( &pool->mutex );
		return NULL;
	}

	pool->allocations++;

	if (pool->max_allocations != -1)
	{
		/* Simulate out-of-memory after N allocations */
		if (pool->allocations >= (unsigned)pool->max_allocations)
		{
			mali_tpi_mutex_unlock( &pool->mutex );
			return NULL;
		}
	}

	if (lastblock->space_used + size <= lastblock->size)
	{
		/* Can use existing block */
		void *ptr = lastblock->data + lastblock->space_used;
		lastblock->space_used += aligned_size(size);
		mali_tpi_mutex_unlock( &pool->mutex );
		return ptr;
	}
	else
	{
		if (size > pool->block_size || size > lastblock->space_used)
		{
			/* Create a new block with the exact required size, but keep using the old one for further requests */
			mempool_block *newblock = allocate_block(size);
			if (!newblock)
			{
				mali_tpi_mutex_unlock( &pool->mutex );
				return NULL;
			}
			newblock->previous_block = lastblock->previous_block;
			pool->last_block->previous_block = newblock;

			newblock->space_used = aligned_size(size);
			mali_tpi_mutex_unlock( &pool->mutex );
			return newblock->data;
		}
		else
		{
			/* Create a new default-sized block and also use it for further requests */
			mempool_block *newblock = allocate_block(pool->block_size);
			if (!newblock)
			{
				mali_tpi_mutex_unlock( &pool->mutex );
				return NULL;
			}
			newblock->previous_block = lastblock;
			pool->last_block = newblock;

			newblock->space_used = aligned_size(size);
			mali_tpi_mutex_unlock( &pool->mutex );
			return newblock->data;
		}
	}
}




#ifdef UNIT_TEST

/*
unit tests:

manual testing:
cc -Wall -g -DUNIT_TEST -DVERBOSE_TESTING -I.. -o mem mem.c && ./mem

Expected output:

Alignment (rounded up to multiples of 16):
  0 -> 0
  10 -> 16
  20 -> 32
  30 -> 32
  40 -> 48
  50 -> 64
  60 -> 64
  70 -> 80
  80 -> 80
  90 -> 96
  100 -> 112
Pool one:
 block size: 16000
  block 0x???????: size(16000) used(10016)
  block 0x???????: size(16000) used(15024)
Pool two (default):
 block size: 4096
  block 0x???????: size(4096) used(1232)
  block 0x???????: size(3500) used(3504)
Pool three:
 block size: 4096
  block 0x???????: size(4096) used(704)
  block 0x???????: size(8000) used(8000)
  block 0x???????: size(5000) used(5008)
Pool one (destroyed):
 block size: 16000
  empty
Pool two (destroyed and reinitialized):
 block size: 4096
  block 0x???????: size(4096) used(928)

 */

/** Prints contents of memory pool */
static void print_mempool(mempool *pool)
{
	mempool_block *block = pool->last_block;
	printf(" block size: %u\n", pool->block_size);
	if (!block)
	{
		printf("  empty\n");
		return;
	}

	while (block)
	{
#ifdef __SYMBIAN32__
		printf("  block %X: size(%u) used(%u)\n", block, block->size, block->space_used);
#else
		printf("  block %p: size(%u) used(%u)\n", block, block->size, block->space_used);
#endif
		block = block->previous_block;
	}
}

static int count_blocks(mempool *pool)
{
	int num = 0;
	mempool_block *block = pool->last_block;
	assert(pool);
	while (block)
	{
		++num;
		block = block->previous_block;
	}
	return num;
}

static int space_wasted(mempool *pool)
{
	mempool_block *block;
	int wasted = 0;
	assert(pool);
	block = pool->last_block;
	assert(block);
	block = block->previous_block;
	while (block)
	{
		if (block->space_used < block->size)
		{
			wasted += block->size - block->space_used;
		}
		block = block->previous_block;
	}
	return wasted;
}

#undef mem_alloc
#define mem_alloc(s) _test_mempool_alloc(&pool_two, s)

static void verbose_test()
{
	mempool pool_one;
	mempool pool_two;
	mempool pool_three;
	int i;

	_test_mempool_init(&pool_one, 16000);
	_test_mempool_init(&pool_two, 0);
	_test_mempool_init(&pool_three, 0);

/* 	_test_mempool_set_default(&pool_two); */

	_test_mempool_alloc(&pool_one, 5000);
	_test_mempool_alloc(&pool_one, 5000);
	_test_mempool_alloc(&pool_one, 5000);
	_test_mempool_alloc(&pool_one, 5000);
	_test_mempool_alloc(&pool_one, 5000);

	_test_mempool_alloc(&pool_two, 100);

	_test_mempool_alloc(&pool_three, 50);
	_test_mempool_alloc(&pool_three, 5000);
	_test_mempool_alloc(&pool_three, 125);
	_test_mempool_alloc(&pool_three, 300);
	_test_mempool_alloc(&pool_three, 8000);
	_test_mempool_alloc(&pool_three, 200);

	mem_alloc(300);
	mem_alloc(600);
	mem_alloc(3500);
	mem_alloc(200);

	printf("Alignment (rounded up to multiples of %i):\n", MEM_PLATFORM_ALIGNMENT);
	for (i = 0; i <= 100; i += 10)
	{
		printf("  %i -> %u\n", i, aligned_size(i));
	}

	printf("Pool one:\n");
	print_mempool(&pool_one);

	printf("Pool two (default):\n");
	print_mempool(&pool_two);

	printf("Pool three:\n");
	print_mempool(&pool_three);

	_test_mempool_destroy(&pool_one);
	_test_mempool_destroy(&pool_two);
	_test_mempool_init(&pool_two, 0);

	assert(NULL == _test_mempool_alloc(&pool_one, 100));
	mem_alloc(371);
	mem_alloc(529);

	printf("Pool one (destroyed):\n");
	print_mempool(&pool_one);
	printf("Pool two (destroyed and reinitialized):\n");
	print_mempool(&pool_two);
}

static int silent_test()
{
	mempool pool_one;
	mempool pool_two;
	mempool pool_three;
	int i;

	_test_mempool_init(&pool_one, 16000);
	_test_mempool_init(&pool_two, 0);
	_test_mempool_init(&pool_three, 0);

/* 	_test_mempool_set_default(&pool_two); */

	_test_mempool_alloc(&pool_one, 5000);
	_test_mempool_alloc(&pool_one, 5000);
	_test_mempool_alloc(&pool_one, 5000);
	_test_mempool_alloc(&pool_one, 5000);
	_test_mempool_alloc(&pool_one, 5000);

	_test_mempool_alloc(&pool_two, 100);

	_test_mempool_alloc(&pool_three, 50);
	_test_mempool_alloc(&pool_three, 5000);
	_test_mempool_alloc(&pool_three, 125);
	_test_mempool_alloc(&pool_three, 300);
	_test_mempool_alloc(&pool_three, 8000);
	_test_mempool_alloc(&pool_three, 200);

	mem_alloc(300);
	mem_alloc(600);
	mem_alloc(3500);
	mem_alloc(200);

	for (i = 0; i <= 1000; i += 11)
	{
		assert(aligned_size(i) - i < MEM_PLATFORM_ALIGNMENT);
		assert((aligned_size(i) % MEM_PLATFORM_ALIGNMENT) == 0);
	}

	assert(count_blocks(&pool_one) == 2);
	assert(space_wasted(&pool_one) <= 1000);

	assert(count_blocks(&pool_two) == 2);
	assert(space_wasted(&pool_two) == 0);

	assert(count_blocks(&pool_three) == 3);
	assert(space_wasted(&pool_three) == 0);

	_test_mempool_destroy(&pool_one);
	_test_mempool_destroy(&pool_two);
	_test_mempool_init(&pool_two, 0);

	assert(NULL == _test_mempool_alloc(&pool_one, 100));
	mem_alloc(371);
	mem_alloc(529);

	assert(0 == count_blocks(&pool_one));

	assert(count_blocks(&pool_two) == 1);
	assert(space_wasted(&pool_two) == 0);
	_test_mempool_destroy(&pool_two);
	_test_mempool_destroy(&pool_three);

	return 0;
}

int main(void)
{
#ifdef VERBOSE_TESTING
	silent_test();
	verbose_test();
#else
	if (silent_test())
	{
		verbose_test();
	}
	else
	{
		printf("All tests OK!\n");
	}
#endif
	return 0;
}

#endif /* UNIT_TEST */
