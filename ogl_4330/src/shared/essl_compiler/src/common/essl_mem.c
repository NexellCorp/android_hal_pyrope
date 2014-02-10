/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/essl_mem.h"

#ifdef FORK_OOM

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
static essl_bool has_oomed = ESSL_FALSE;

memerr fork_oom_handler(mempool *pool)
{

	pid_t pid;
	if(has_oomed) return MEM_ERROR;
	pid = fork();
	if(pid == -1) return MEM_ERROR; /* fork failed */

	if(pid == 0)
	{
		/* child */
		has_oomed = ESSL_TRUE;
		pool->tracker->out_of_memory_encountered = ESSL_TRUE;
		return MEM_ERROR; /* go ahead and simulate OOM */
	} else {
		/* parent */
		int status = 0;
		waitpid(pid, &status, 0);
#ifdef DEBUGPRINT
		fprintf(stderr, "Failing on allocation %d\n", pool->tracker->allocations);
#endif
		if(WIFSIGNALED(status))
		{
			return MEM_ERROR;
		} else {
			return MEM_OK;
		}
	}
}

#endif



/**
 * @note portability: This must match the largest alignment restriction of the target platform. In other words, it must match the alignment of the local malloc()
 */
#ifdef  ESSL_COMPILER_ALIGNMENT_OVERRIDE
#define MEM_PLATFORM_ALIGNMENT    ESSL_COMPILER_ALIGNMENT_OVERRIDE
#else
#define MEM_PLATFORM_ALIGNMENT    4
#endif

/**
 * Default block size to use if 0 is specified when initializing a memory pool
 */
#define DEFAULT_BLOCK_SIZE   4096

/**
 * Rounds size up to fit platform alignment
 */
#define ALIGNED_SIZE(size) (((size) + MEM_PLATFORM_ALIGNMENT - 1) & ~(MEM_PLATFORM_ALIGNMENT - 1))

/**
 * Blocks are allocated from the underlying OS with size (block structure + alignment + block_size)
 */
static mempool_block *allocate_block(size_t size, mempool_tracker *tracker);



memerr _essl_mempool_init(mempool *pool, size_t block_size, mempool_tracker *tracker)
{
	mempool_block *block;

	assert(tracker != 0);

	if (block_size)
	{
		pool->block_size = block_size;
	}
	else
	{
		pool->block_size = DEFAULT_BLOCK_SIZE;
	}
#ifdef NO_MEMPOOL
	pool->block_size = 0;
#endif

	ESSL_CHECK(block = allocate_block(pool->block_size, tracker));

	block->previous_block = NULL;

	pool->last_block = block;
	pool->tracker = tracker;
	return MEM_OK;
}



void _essl_mempool_destroy(mempool *pool)
{
	if (!pool || !pool->last_block)
	{
		return;
	}
	else
	{
		size_t header_size = ALIGNED_SIZE(sizeof(struct mempool_block));
		mempool_block *block = pool->last_block;
		while (block)
		{
			mempool_block *prev = block->previous_block;
			pool->tracker->size_allocated -= (block->size + header_size);
			pool->tracker->size -= block->size;
			pool->tracker->size_used -= block->space_used;
			--pool->tracker->blocks_allocated;

			pool->tracker->free(block);
			block = prev;
		}
		pool->last_block = NULL;
		return;
	}
}



memerr _essl_mempool_clear(mempool *pool)
{
	size_t block_size = pool->block_size;
	mempool_tracker *tracker = pool->tracker;
	_essl_mempool_destroy(pool);
	ESSL_CHECK(_essl_mempool_init(pool, block_size, tracker));
	return MEM_OK;
}


void _essl_mempool_fail_alloc_handler(void)
{

}

void *_essl_mempool_alloc(mempool *pool, size_t size)
{
	const size_t aligned_size = ALIGNED_SIZE(size);
	mempool_block *lastblock;

	if(pool->tracker->out_of_memory_encountered)
	{
		return NULL;
	}

#ifdef FORK_OOM
	if(fork_oom_handler(pool) == MEM_ERROR)
	{
		return NULL;
	}

#endif


	pool->tracker->allocations++;

	if (pool->tracker->fail_on_allocation > 0)
	{
		/* Simulate out-of-memory after N allocations */
		if (pool->tracker->allocations >= (unsigned)pool->tracker->fail_on_allocation)
		{
			pool->tracker->out_of_memory_encountered = ESSL_TRUE;
			_essl_mempool_fail_alloc_handler();
			return NULL;
		}
	}

	lastblock = pool->last_block;
	if (!lastblock)
	{
		return NULL;
	}

	if (lastblock->space_used + size <= lastblock->size)
	{
		/* Can use existing block */
		void *ptr = lastblock->data + lastblock->space_used;

		lastblock->space_used += aligned_size;
		pool->tracker->size_used += aligned_size;

		memset(ptr, 0, size);
		return ptr;
	}
	else
	{
		if (size > pool->block_size || size > lastblock->space_used)
		{
			/* Create a new block with the exact required size, but keep using the old one for further requests */
			mempool_block *newblock = allocate_block(size, pool->tracker);
			if (!newblock)
			{
				return NULL;
			}

			newblock->previous_block = lastblock->previous_block;
			pool->last_block->previous_block = newblock;
			
			newblock->space_used = aligned_size;
			pool->tracker->size_used += aligned_size;


			memset(newblock->data, 0, size);
			return newblock->data;
		}
		else
		{
			/* Create a new default-sized block and also use it for further requests */
			mempool_block *newblock = allocate_block(pool->block_size, pool->tracker);
			if (!newblock)
			{
				return NULL;
			}
			newblock->previous_block = lastblock;
			pool->last_block = newblock;

			newblock->space_used = aligned_size;
			pool->tracker->size_used += aligned_size;

			memset(newblock->data, 0, size);
			return newblock->data;
		}
	}
}



mempool_tracker *_essl_mempool_get_tracker(mempool *pool)
{
	return pool->tracker;
}


void _essl_mempool_tracker_init(mempool_tracker *tracker, alloc_func alloc, free_func free)
{
	memset(tracker, 0, sizeof(*tracker));
	tracker->alloc = alloc;
	tracker->free = free;
	tracker->out_of_memory_encountered = ESSL_FALSE;
}



static mempool_block *allocate_block(size_t size, mempool_tracker *tracker)
{
	const size_t header_size = ALIGNED_SIZE(sizeof(mempool_block));
	const size_t total_size = header_size + size;
	mempool_block *block;

	block = tracker->alloc((unsigned int)total_size);
	if(block == NULL)
	{
		tracker->out_of_memory_encountered = ESSL_TRUE;
		return NULL;
	}

	block->size = size;
	block->space_used = 0;
	block->data = ((char*)block) + header_size;

	tracker->size_allocated += total_size;
	tracker->size += size;
	++tracker->blocks_allocated;
	if (tracker->size_allocated > tracker->peak_allocated)
	{
		tracker->peak_allocated = tracker->size_allocated;
	}

	return block;
}






#ifdef UNIT_TEST

/*
unit tests:

manual testing:
cc -Wall -g -DUNIT_TEST -DVERBOSE_TESTING -I.. -o mem mem.c essl_test_system.c && ./mem

Expected output:

Alignment (rounded up to multiples of 4):
  0 -> 0
  10 -> 12
  20 -> 20
  30 -> 32
  40 -> 40
  50 -> 52
  60 -> 60
  70 -> 72
  80 -> 80
  90 -> 92
  100 -> 100
Pool one:
 block size: 16000
  block 0x??????: size(16000) used(10000)
  block 0x??????: size(16000) used(15000)
Pool two:
 block size: 4096
  block 0x??????: size(4096) used(100)
Pool three:
 block size: 4096
  block 0x??????: size(4096) used(680)
  block 0x??????: size(8000) used(8000)
  block 0x??????: size(5000) used(5000)
Pool one (destroyed):
 block size: 16000
  empty
Pool two (destroyed and reinitialized):
 block size: 512
  block 0x??????: size(512) used(0)
Pool two (one empty block):
 block size: 512
  block 0x??????: size(512) used(0)
  block 0x??????: size(513) used(516)
Pool two (no empty blocks):
 block size: 512
  block 0x??????: size(512) used(512)
  block 0x??????: size(513) used(516)
Pool two (half empty block):
 block size: 512
  block 0x??????: size(512) used(256)
  block 0x??????: size(512) used(512)
  block 0x??????: size(513) used(516)
All tests OK!
 */

/** Prints contents of memory pool */
#ifdef VERBOSE_TESTING
static void print_mempool(mempool *pool)
{
	mempool_block *block = pool->last_block;
	printf(" block size: %lu\n", (long)pool->block_size);
	if (!block)
	{
		printf("  empty\n");
		return;
	}

	while (block)
	{
		printf("  block %p: size(%lu) used(%lu)\n", block, (long)block->size, (long)block->space_used);
		block = block->previous_block;
	}
}
#endif

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
#define mem_alloc(s) (pool_two.tracker->alloc(s))

static int run_test(mempool_tracker *tracker)
{
	mempool pool_one;
	mempool pool_two;
	mempool pool_three;
	int i;

	assert(_essl_mempool_init(&pool_one, 16000, tracker) == MEM_OK);
	assert(_essl_mempool_init(&pool_two, 0, tracker) == MEM_OK);
	assert(_essl_mempool_init(&pool_three, 0, tracker) == MEM_OK);

	assert(_essl_mempool_alloc(&pool_one, 5000) != NULL);
	assert(_essl_mempool_alloc(&pool_one, 5000) != NULL);
	assert(_essl_mempool_alloc(&pool_one, 5000) != NULL);
	assert(_essl_mempool_alloc(&pool_one, 5000) != NULL);
	assert(_essl_mempool_alloc(&pool_one, 5000) != NULL);

	assert(_essl_mempool_alloc(&pool_two, 100) != NULL);

	assert(_essl_mempool_alloc(&pool_three, 50) != NULL);
	assert(_essl_mempool_alloc(&pool_three, 5000) != NULL);
	assert(_essl_mempool_alloc(&pool_three, 125) != NULL);
	assert(_essl_mempool_alloc(&pool_three, 300) != NULL);
	assert(_essl_mempool_alloc(&pool_three, 8000) != NULL);
	assert(_essl_mempool_alloc(&pool_three, 200) != NULL);

	mem_alloc(300);
	mem_alloc(600);
	mem_alloc(3500);
	mem_alloc(200);

	for (i = 0; i <= 1000; i += 11)
	{
		assert(ALIGNED_SIZE(i) - i < MEM_PLATFORM_ALIGNMENT);
		assert((ALIGNED_SIZE(i) % MEM_PLATFORM_ALIGNMENT) == 0);
	}

#ifndef NO_MEMPOOL
	assert(count_blocks(&pool_one) == 2);
	assert(space_wasted(&pool_one) == 1000);

	assert(count_blocks(&pool_two) == 1);
	assert(space_wasted(&pool_two) == 0);

	assert(count_blocks(&pool_three) == 3);
	assert(space_wasted(&pool_three) == 0);
#endif

#ifdef VERBOSE_TESTING
	printf("Alignment (rounded up to multiples of %i):\n", MEM_PLATFORM_ALIGNMENT);
	for (i = 0; i <= 100; i += 10)
	{
		printf("  %i -> %lu\n", i, (long)ALIGNED_SIZE(i));
	}

	printf("Pool one:\n");
	print_mempool(&pool_one);

	printf("Pool two:\n");
	print_mempool(&pool_two);

	printf("Pool three:\n");
	print_mempool(&pool_three);
#endif

	_essl_mempool_destroy(&pool_one);
	_essl_mempool_destroy(&pool_two);
	assert(_essl_mempool_init(&pool_two, 512, tracker) == MEM_OK);

	assert(NULL == _essl_mempool_alloc(&pool_one, 100));
	mem_alloc(371);
	mem_alloc(529);

#ifndef NO_MEMPOOL
	assert(count_blocks(&pool_one) == 0);

	assert(count_blocks(&pool_two) == 1);
	assert(space_wasted(&pool_two) == 0);
#endif

#ifdef VERBOSE_TESTING
	printf("Pool one (destroyed):\n");
	print_mempool(&pool_one);
	printf("Pool two (destroyed and reinitialized):\n");
	print_mempool(&pool_two);
#endif

	assert(_essl_mempool_alloc(&pool_two, 513) != NULL);

#ifndef NO_MEMPOOL
	assert(count_blocks(&pool_two) == 2);
	assert(space_wasted(&pool_two) == 0);
#endif

#ifdef VERBOSE_TESTING
	printf("Pool two (one empty block):\n");
	print_mempool(&pool_two);
#endif

	assert(_essl_mempool_alloc(&pool_two, 512) != NULL);

#ifndef NO_MEMPOOL
	assert(count_blocks(&pool_two) == 2);
	assert(space_wasted(&pool_two) == 0);
#endif

#ifdef VERBOSE_TESTING
	printf("Pool two (no empty blocks):\n");
	print_mempool(&pool_two);
#endif

	assert(_essl_mempool_alloc(&pool_two, 256) != NULL);

#ifndef NO_MEMPOOL
	assert(count_blocks(&pool_two) == 3);
	assert(space_wasted(&pool_two) == 0);
#endif

#ifdef VERBOSE_TESTING
	printf("Pool two (half empty block):\n");
	print_mempool(&pool_two);
#endif

	/* test error handling */
	tracker->fail_on_allocation = tracker->allocations + 4;
	assert(_essl_mempool_alloc(&pool_two, 16) != NULL);
	assert(_essl_mempool_alloc(&pool_two, 16) != NULL);
	assert(_essl_mempool_alloc(&pool_three, 16) != NULL);
	assert(_essl_mempool_alloc(&pool_three, 16) == NULL);
	tracker->fail_on_allocation = 0;

	/* cleanup */
	_essl_mempool_destroy(&pool_two);
	_essl_mempool_destroy(&pool_three);

	/* more error handling*/
	_essl_mempool_destroy(&pool_three);

	assert(tracker->size_allocated == 0);
	assert(tracker->size == 0);
	assert(tracker->size_used == 0);
	assert(tracker->peak_allocated == 53192 + 6 * sizeof(mempool_block));

	return 0;
}

int main(void)
{
	mempool_tracker tracker;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);

	if (run_test(&tracker) != 0)
	{
		printf("Test FAILED!\n");
		assert(0);
		return 1;
	}

	printf("All tests OK!\n");
	return 0;
}

#endif /* UNIT_TEST */
