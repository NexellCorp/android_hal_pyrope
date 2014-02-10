/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_mem.h
 * The common layer contains the implementation of most of the memory-functions.
 * Function-calls are passed through the frontend to common layer.
 *
 * The memory design:
 * The architecture exposes one or more memory types to the common memory subsystem.
 * Each type of memory exposed by the architecture has a freelist representing available memory
 * When a request for memory arrives the freelists of the memory types matching the request are searched.
 * If no memory is found the architecture is queried for more memory of the each compatible memory type.
 */

#ifndef _BASE_COMMON_MEM_H_
#define _BASE_COMMON_MEM_H_

#include <stddef.h>
#include <mali_system.h>
#include <base/mali_types.h>
#include <base/mali_memory_types.h>
#include <base/mali_runtime.h>
#include <base/mali_debug.h>
#include <base/common/tools/base_common_tools_circular_linked_list.h>
#include <base/common/base_common_context.h>

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
#include <ump/ump.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MALI_TEST_API
typedef struct mali_mem_stats
{
        u32 current_bytes_requested;
        u32 max_bytes_requested;
        u32 current_bytes_allocated;
        u32 max_bytes_allocated;
} mali_mem_stats;
#endif /* MALI_TEST_API */

/**
 * mali_bus_usage tells what mali cores is allowed to do
 * to the memory. It is a bit pattern with fields:
 */
typedef enum mali_mem_usage_flag
{
        MALI_PP_READ   = (1<<0),
        MALI_PP_WRITE  = (1<<1),
        MALI_GP_READ   = (1<<2),
        MALI_GP_WRITE  = (1<<3),
        MALI_CPU_READ  = (1<<4),
        MALI_CPU_WRITE = (1<<5),
        MALI_GP_L2_ALLOC   = (1<<6),  /** GP allocate mali L2 cache lines */
}mali_mem_usage_flag;

/*
	Most of these functions are just forwarded from the frontend,
	so the doxygen comments in the frontend should be referenced
	instead of duplicating them here
*/

/**
 * Get a new mali_base_ctx_handle object.
 * @param ctx The base context to scope usage to
 * @return A standard Mali error code
 */
mali_err_code _mali_base_common_mem_open(mali_base_ctx_handle ctx) MALI_CHECK_RESULT;

/**
 * Remove a reference to the memory system
 * @param ctx The base context used in the open call
 */
void _mali_base_common_mem_close(mali_base_ctx_handle ctx);

/**
 * Allocation of mali_mem
 * @param ctx Base context to scope the allocation to
 * @param size Size of allocation needed
 * @param pow2_alignment Alignment requirement
 * @param mali_access Needed capabilities
 * @return MALI_NO_HANDLE if the allocation failed, a valid handle else
 */
MALI_IMPORT mali_mem_handle _mali_base_common_mem_alloc(
                    mali_base_ctx_handle ctx,
                    u32 size,
                    u32 pow2_alignment,
                    u32 mali_access );

/**
 * Freeing of mali_mem
 * A noop if mem has the value MALI_NO_HANDLE
 * @param mem Handle to the memory to free
 */
void _mali_base_common_mem_free(mali_mem_handle mem);

/**
 * Get the sum of allocated size for all banks
 */
MALI_NOTRACE u32 _mali_base_common_mem_get_total_allocated_size(void);

/**
 * Tell memory system that a new memory tracking period has started.
 *
 * This will use historic data to calculate how much memory to hold back.
 */
void _mali_base_common_mem_new_period(void);

/**
 * Use this function to release unused memory buffers.
 * @param ctx The base context to free unused memory in
 */
void _mali_base_common_mem_free_unused_mem(mali_base_ctx_handle ctx);

/**
 * Get the address as seen by mali for mem + offset
 * @param mem Handle to the memory to get the Mali address of
 * @param offset Offset within block to calculate the address of
 * @return The Mali address calculated
 */
mali_addr _mali_base_common_mem_addr_get_full(mali_mem_handle mem, u32 offset);


/****** Get more information about current block ******/

/**
 * Get size of memory block.
 * @param mem Handle to the memory to get the size of
 * @return The size
 */
u32 _mali_base_common_mem_size_get(mali_mem_handle mem);

/**
 * Get order of memory block.
 * @param mem Handle to the memory to get the order of
 * @return The order
 */
u32 _mali_base_common_mem_order_get(mali_mem_handle mem);

/**
 * Get alignment of current memory block
 * @param mem Handle to the memory to get the alignment of
 * @return The alignment
 */
u32 _mali_base_common_mem_alignment_get(mali_mem_handle mem);

/**
 * Get the bus settings - @see mali_bus_usage
 * @param mem Handle to the memory to query
 * @return Capabilities requested for the allocation
 */
u32 _mali_base_common_mem_usage_get(mali_mem_handle mem);


/****** Using mali_mem_handle as a linked list with an user_pointer: ******/

/**
 * Adding new element to the linked list, after current element.
 * Current is allowed to be MALI_NO_HANDLE, in which case
 * new will be untouched.
 * @param current The block to add the new element after
 * @param new The new element
 * @return Current if current was not MALI_NO_HANDLE, else new
 */
mali_mem_handle _mali_base_common_mem_list_insert_after(
                    mali_mem_handle current,
                    mali_mem_handle new_mem);

/**
 * Adding new element to the linked list, before current element.
 * Current is allowed to be MALI_NO_HANDLE, in which case
 * new will be untouched.
 * @param current The block to add the new element in front of
 * @param new The new element
 * @return Always the handle new
 */
mali_mem_handle _mali_base_common_mem_list_insert_before(
                    mali_mem_handle current,
                    mali_mem_handle new_mem);

/**
 * Get next element in linked list of mali_mem_handle.
 * @param mem The block to find the next on the list for
 * @return Handle to the block following this handle, MALI_NO_HANDLE if no next exists
 */
mali_mem_handle _mali_base_common_mem_list_get_next(mali_mem_handle mem);

/**
 * Get previous element in linked list of mali_mem_handle.
 * @param mem The block to find the previous on the list for
 * @return Handle to the block preceding this handle, MALI_NO_HANDLE if no preceding exists
 */
mali_mem_handle _mali_base_common_mem_list_get_previous(mali_mem_handle mem);

/**
 * Remove element, return next in list if exist, else previous.
 * @param mem The handle to remove from list
 * @return Handle to the blocks next handle if it exists, else the value of previous
 */
mali_mem_handle _mali_base_common_mem_list_remove_item(mali_mem_handle mem);

/**
 * Free all linked memory elements in the next direction
 * @note Only walks the list in the next direction, any previous elements will be left alone
 * @param list Head of the list to free
 */
void _mali_base_common_mem_list_free(mali_mem_handle list);

/**
 * Total size of all memory in the linked list
 * @note Only walks from the list in the next direction, any previous elements will not be included
 * @param mem The head of the list to calculate the sum of
 * @return Total size of all memory blocks linked
 */
u32 _mali_base_common_mem_list_size_get(mali_mem_handle mem);

#if !defined(HARDWARE_ISSUE_3251)

/**
 * Allocate memory that can grow in size.
 * You set the expected size which will also be the
 * start size of the heap. Then the heap will grow in
 * block_size bytes, for each time it need to expand.
 * In non MMU-memory systems the mali_mem_address
 * will not be continuous between such blocks.
 * @param ctx Base context.
 * @param default_size This amount of memory will be available when function returns.
 * @param maximum_size The heap is not allowed to grow to a bigger size than this.
 * @param block_size How much to allocate, each time heap must expand. Must be a pow2 size.
 * @return Handle to a heap or MALI_NO_HANDLE if allocation failed
 */
MALI_IMPORT mali_mem_handle _mali_base_common_mem_heap_alloc(mali_base_ctx_handle ctx, u32 default_size, u32 maximum_size, u32 block_size);

/**
 * Returns the Mali address that should be set as the start
 * address for the given heap
 * @param heap The heap to get the address from
 * @return The start address of the heap, as seen by Mali
 */
u32 _mali_base_common_mem_heap_get_start_address(mali_mem_handle heap);

/**
 * Returns the Mali address that should be set as end
 * address of the first block for the given heap
 * @param heap The heap to get the address from
 * @return The end address of the first block of the heap, as seen by Mali
 */
u32 _mali_base_common_mem_heap_get_end_address_of_first_block(mali_mem_handle heap);

/**
 * Returns the Mali address that should be set as end
 * address for the given heap
 * @param heap The heap to get the address from
 * @return The end address of the heap, as seen by Mali
 */
u32 _mali_base_common_mem_heap_get_end_address(mali_mem_handle heap);

/**
 * Get blocksize, a pow2 number telling where memory may
 * be discontinuous:
 * @param heap The heap get the block size of
 * @return The block size
 */
u32 _mali_base_common_mem_heap_get_blocksize(mali_mem_handle heap);

/**
 * Query how many bytes on the heap which have been allocated.
 * @param mem Handle to the heap to query the used bytes of
 * @return Number of bytes allocated on the given heap
 */
u32 _mali_base_common_mem_heap_used_bytes_get(mali_mem_handle heap);

/**
 * Get maximum memory size allowed for this heap:
 * @param heap The heap to get the max size of
 * @return The maximum size of the heap
 */
u32 _mali_base_common_mem_heap_get_max_size(mali_mem_handle heap);

/**
 * Reset the heap.
 * Equivalent to free it and allocate it again, except that reset
 * can not return out of mem.
 * @param heap The heap to reset
 */
void _mali_base_common_mem_heap_reset(mali_mem_handle heap);

/**
 * Check if a memory handle is a heap handle
 * @param mem The memory handle to check
 * @return MALI_TRUE if a heap, MALI_FALSE if a normal allocation
 */
mali_bool _mali_base_common_mem_is_heap(mali_mem_handle mem);

/**
 * Read a 64-bit word from the given offset inside a heap
 * @note Before read64/write64 the block has to be ....
 * @param heap The heap to read from
 * @param to_offset The offset inside the heap to read from (in bytes)
 * @return The value at the given offset
 */
u64 _mali_base_common_heap_read64(mali_mem_handle heap_handle, u32 offset);

/**
 * Write a 64-bit word to the given offset inside a heap
 * @note Before read64/write64 the block has to be ....
 * @param heap The heap to write to
 * @param to_offset The offset inside the heap to write to (in bytes)
 * @param word64 The value to write
 */
void _mali_base_common_heap_write64(mali_mem_handle heap_handle, u32 offset, u64 data);

/**
 * Called when an out-of-memory has happened on a memory handle being used as a heap
 * @param mem The memory handle being used as a heap which has triggered an out-of-memory
 * @param new_heap_start Pointer to u32 to store possible new heap_start address
 * @param new_heap_end Pointer to u32 to store possible new heap end address
 * @return MALI_ERR_NO_ERROR if heap successfully expanded, new_heap_start/_end contain new heap addresses
 * MALI_ERR_OUT_OF_MEMORY if no more memory could be assigned to the heap
 */
mali_err_code _mali_base_common_mem_heap_out_of_memory(mali_mem_handle mem, u32 * new_heap_start, u32 * new_heap_end) MALI_CHECK_RESULT;

/**
 * Called when a user wants to increase the heap's size right now instead of
 * waiting for it to grow when it is known how much memory is needed.
 * @param ctx The base context
 * @param mem The memory handle being used as a heap
 * @param resize The new size the heap need to change into
 * @return MALI_ERR_NO_ERROR if heap successfully expanded,
 * MALI_ERR_OUT_OF_MEMORY if no more memory could be assigned to the heap
 */
mali_err_code _mali_base_common_mem_heap_resize( mali_base_ctx_handle ctx, mali_mem_handle mem, u32 resize) MALI_CHECK_RESULT;

/**
 * Called when a job which has a heap is finished so the heap usage can be calculated
 * @param heap_handle The heap to update
 * @param heap_addr The current heap pointer
 */
void _mali_base_common_heap_intern_set_last_heap_addr(mali_mem_handle heap_handle, u32 heap_addr);

#endif

/**
 * Create a Mali mem handle to represent externally managed memory
 * To be able to use the read/write/copy routines a mapping of the memory has to be provided.
 * @param ctx The base context to scope usage of this external memory to
 * @param phys_addr The physical address to wrap into a Mali memory handle
 * @param size The size of the external memory
 * @param mapping CPU mapping of the physical memory.
 * @param access_rights Mali access rights to the external memory
 * @return A Mali memory handle which represents the external memory
 */
mali_mem_handle _mali_base_common_mem_add_phys_mem(mali_base_ctx_handle ctx, u32 phys_addr, u32 size, void * mapping, u32 access_rights);

#if MALI_USE_DMA_BUF
mali_mem_handle _mali_base_common_mem_wrap_dma_buf(mali_base_ctx_handle ctx, int mem_fd, u32 offset);
int _mali_base_common_mem_get_dma_buf_descriptor(mali_mem_handle mem_handle);
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
mali_mem_handle _mali_base_common_mem_wrap_ump_memory(mali_base_ctx_handle ctx, ump_handle ump_mem, u32 offset, u32 access_rights);

ump_handle _mali_base_common_mem_get_ump_memory(mali_mem_handle mem_handle);
#endif


/****** Functions to print debug-info. ******/

MALI_DEBUG_CODE(

	/**
	* Print all mem-info in a mali_base_ctx_handle.
	* Legend:
	* A - Allocated
	* F - Free
	* After the usage-flag the size of the block is shown (in decimal)
	*
	* Note: When context separation is up and running we'll probably change the output slightly
 	* @param ctx The base context to print all info for
 	*/
	void _mali_base_common_mem_debug_print_all(mali_base_ctx_handle ctx);
)

#ifdef MALI_TEST_API
/**
  * Retrieve Mali memory usage statistics
  * @param stats Pointer to the struct to fill with the current stats
  */
void _mali_base_common_mem_stats_get(mali_mem_stats * stats);
#endif


#ifdef MALI_DUMP_ENABLE
/* Used for dumping content of memory to file. Useful for debugging*/
typedef enum mem_print_format
{
	MALI_MEM_PRINT_VERBOSE     =  (1<<0),
	MALI_MEM_PRINT_ONE_LINERS  =  (1<<1),
} mem_print_format;


void _mali_base_common_mem_dump_all_mem(mali_base_ctx_handle ctx, mali_file *file, u32 mem_print_format, const char * delimiter);
#endif /* #ifdef MALI_DUMP_ENABLE */

#ifdef MALI_MEMORY_PROFILING
	/* When the MALI_MEMORY_PROFILING define is enabled, all functions that allocate
		mali memory are routed through macros that store additional information
		about where the	allocation was made in the data to the returned memory_handle.
		The macros use this function to store this profiling information. */
	MALI_IMPORT mali_mem_handle  _mali_base_common_mem_set_profiling(mali_mem_handle mem,
		const char * function_name,
		const char * file_name,
		u32 line_no);
#endif /* #ifdef MALI_MEMORY_PROFILING*/

#ifdef __cplusplus
}
#endif

#endif /*_BASE_COMMON_MEM_H_*/
