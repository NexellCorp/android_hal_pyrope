/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_MEM_H
#define COMMON_MEM_H

#include <mali_tpi.h>
#ifndef WIN32_RI
	#include <mali_system.h>
#else
	#include <w32_helpers.h>
#endif

#include <stddef.h>
/**
 * Memory pool allocator.
 * Assigns memory from allocated blocks and allocates new blocks if necessary.
*/
#ifdef __cplusplus
extern "C" {
#endif


/**
 * @note portability: This must match the largest alignment restriction of the target platform. In other words, it must match the alignment of the local malloc()
*/
#ifndef MEM_PLATFORM_ALIGNMENT
#define MEM_PLATFORM_ALIGNMENT 16
#endif

/**
 * Return codes for indicating whether memory allocation went well
 */
typedef enum memerr
{
    MEM_ERROR =  0,
    MEM_OK    =  1
} memerr;

#ifndef NDEBUG
#define DEBUG_CHECK_EXPRESSIONS
#endif

/** Checks that a condition is true and returns otherwise */
#ifndef DEBUG_CHECK_EXPRESSIONS
#define MEM_CHECK(exp, ret_val) if ((exp) == 0) return (ret_val)
#else
#include <stdio.h>
#ifdef __SYMBIAN32__
void SOS_DebugPrintf(const char * format,...);
#define LOGPRINT(format...) SOS_DebugPrintf(format); printf(format)
#define MEM_CHECK(exp, ret_val) if ((exp) == 0) { LOGPRINT("Null result in %s:%d\n", __FILE__, __LINE__); return (ret_val); }
#else
#define MEM_CHECK(exp, ret_val) if ((exp) == 0) { fprintf(stderr, "Null result in %s:%d\n", __FILE__, __LINE__); return (ret_val); }
#endif
#endif

/**
 * Block of memory in a pool.
*/
typedef struct mempool_block {
	struct mempool_block *previous_block;
	size_t size;
	size_t space_used;
	size_t allocated;
	char* data;
} mempool_block;

/**
   Memory pool handle.
*/
typedef struct _tag_mempool {
	/*@null@*/ mempool_block* last_block;
	size_t block_size;
	unsigned int allocations; /* Used for testing and profiling */
	int max_allocations; /* Used for testing */
	mali_tpi_mutex mutex;
} mempool;



/**
   Initialize a memory pool.
   @param pool Memory pool to initialize, provided by the user
   @param block_size Amount of user data possible in each block
*/
memerr _test_mempool_init(/*@out@*/ mempool *pool, size_t block_size);


/**
   Destroys a memory pool.
   Frees all memory associated with the memory pool
*/
memerr _test_mempool_destroy(mempool *pool);


/**
   Allocate memory from a pool
   @param pool Memory pool to allocate from
   @param size Size of memory wanted in number of chars (i.e. bytes on a platform with 8 bit chars)
   Frees all memory associated with the memory pool
   @return pointer to memory on success, 0 on failure
*/

/*@null@*/ void *_test_mempool_alloc(mempool *pool, size_t size);

#ifdef __cplusplus
}
#endif


#endif /* COMMON_MEM_H */
