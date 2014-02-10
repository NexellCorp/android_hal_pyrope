/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_MEM_H
#define COMMON_MEM_H

#include "common/essl_system.h"
#include "compiler.h"

/**
 * Memory pool allocator.
 * Assigns memory from allocated blocks and allocates new blocks if necessary.
*/



/**
 * Return codes for indicating whether memory allocation went well
 */
typedef unsigned int memerr;
#define MEM_ERROR 0
#define MEM_OK 1

#ifdef DEBUGPRINT
#define DEBUG_CHECK_EXPRESSIONS
#endif

/** Checks that a condition is true and returns otherwise */
#ifndef DEBUG_CHECK_EXPRESSIONS
#define ESSL_CHECK(exp) if ((exp) == 0) return 0
#else
#define ESSL_CHECK(exp) if ((exp) == 0) do { fprintf(stderr, "Null result in %s:%d\n", __FILE__, __LINE__); return 0; } while(0)

#endif

/** Special version of the ESSL_CHECK macro for varargs functions */
#ifndef DEBUG_CHECK_EXPRESSIONS
#define ESSL_CHECK_VA(args, exp) if ((exp) == 0) do { va_end(args); return 0; } while(0)
#else
#define ESSL_CHECK_VA(args, exp) if ((exp) == 0) do { va_end(args); fprintf(stderr, "Null result in %s:%d\n", __FILE__, __LINE__); return 0; } while(0)
#endif


/**
 * Memory pool tracker (will be updated by the mempools which is associated with tracker).
 */
typedef struct _tag_mempool_tracker
{
	alloc_func alloc;                /**< allocation function */
	free_func free;                  /**< deallocation function */
	size_t size_allocated;           /**< the total amount allocated from CRT/OS */
	size_t size;                     /**< the total amount available to user via the memorypools */
	size_t size_used;                /**< the total amount which is in use (given to user), including alignment */
	size_t peak_allocated;           /**< the maximum allocated at any point throughout the compilation */
	unsigned int blocks_allocated;   /**< the number of blocks allocated from the OS/CRT */
	unsigned int allocations;        /**< number of memory pool allocations (used for testing and profiling) */
	unsigned int fail_on_allocation; /**< which allocation to fail on (used for testing) */
	essl_bool out_of_memory_encountered;  /**< Dependent memory pools have returned out of memory at least once */
} mempool_tracker;


/**
 * Block of memory in a pool.
*/
typedef struct mempool_block
{
	struct mempool_block *previous_block;
	size_t size;
	size_t space_used;
	char* data;
} mempool_block;


/**
   Memory pool handle.
*/
typedef struct _tag_mempool
{
	mempool_block* last_block;
	size_t block_size;
	mempool_tracker *tracker;
} mempool;


/**
   Initialize a memory pool.
   @param pool Memory pool to initialize, provided by the user
   @param block_size Amount of user data possible in each block. If zero, the default block size is chosen.
*/
memerr _essl_mempool_init(mempool *pool, size_t block_size, mempool_tracker* tracker);


/**
   Destroys a memory pool.
   Frees all memory associated with the memory pool
*/
void _essl_mempool_destroy(mempool *pool);


/**
   Frees all memory in the pool and reinitializes it for use.
*/
memerr _essl_mempool_clear(mempool *pool);

/**
   Allocate memory from a pool
   @param pool Memory pool to allocate from
   @param size Size of memory wanted in number of chars (i.e. bytes on a platform with 8 bit chars)
   Frees all memory associated with the memory pool
   @return pointer to memory on success, 0 on failure
*/

void *_essl_mempool_alloc(mempool *pool, size_t size);

/**
 * Returns the mempool tracker used by given mempool
 */
mempool_tracker *_essl_mempool_get_tracker(mempool *pool);

/**
 * Initialize a mempool tracker.
 */
void _essl_mempool_tracker_init(mempool_tracker *tracker, alloc_func alloc, free_func free);


#endif /* COMMON_MEM_H */
