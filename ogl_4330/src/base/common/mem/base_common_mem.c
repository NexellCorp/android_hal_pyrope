/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_types.h>
#include <base/mali_macros.h>
#include <base/mali_debug.h>
#include <base/mali_memory.h>
#include <base/mali_test.h>
#include <base/arch/base_arch_mem.h>
#include "base_common_mem.h"

#if MALI_USE_DMA_BUF
/* @@@@ todo: see if we can find a good solution to this Linux specific code in common problem. */
#include <unistd.h>
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
#include <ump/ump.h>
#endif

/*
	Minimum allocation size supported is 64 bytes
	log2(64) = 6
	Define alignment size and mask based on value of min order.
*/
#define MALI_MEMORY_MINIMUM_ORDER_SUPPORTED 6U
#define MALI_MEMORY_MINIMUM_ALIGNMENT_SIZE (1U << MALI_MEMORY_MINIMUM_ORDER_SUPPORTED)
#define MALI_MEMORY_MINIMUM_ALIGNEMENT_MASK (MALI_MEMORY_MINIMUM_ALIGNMENT_SIZE - 1U)

/**
 * Size of our descriptor pool
 */
#define MALI_MEMORY_DESCRIPTOR_POOL_SIZE 16

/**
 * Defintiion of the memory_mapping_data struct.
 * Tracks the host buffer when we need to use a
 * buffer access to Mali memory by the CPU.
 */
typedef struct memory_mapping_data
{
	u32 mapping_size; /**< Size of the mapping region */
	u32 mali_offset; /**< Offset into the Mali memory for region start */
	void * user_pointer; /**< The host buffer */
} memory_mapping_data;


#if !defined(HARDWARE_ISSUE_3251)
/**
 * Definition of the heap object struct
 * Used for heaps on non-MMU systems
 */
typedef struct heap_extended_data
{
	mali_mem_handle first; /**< The first handle allocated to this heap */
	mali_mem_handle last; /**< The last handle allocated to this heap */
	u32 block_size; /**< The size to use for allocations when extening the heap */
	u32 maximum_size; /**< The maximum size the heap is allowed to grow to */
	u32 current_phys_addr; /**< Mali address of first available address */
	mali_base_ctx_handle ctx; /**< The base context this heap was created in */
} heap_extended_data;
#endif

/**
 * Head of linked list of all banks registered
 */
MALI_STATIC mali_embedded_list_link memory_banks = MALI_EMBEDDED_LIST_HEAD_INIT_COMPILETIME(memory_banks);

#ifdef MALI_MEMORY_PROFILING
static mali_atomic_int local_static_alloc_nr;
#endif

static u32 current_period;

/**
 * Internal memory initialization routine
 * @param ctx The base context which caused the memory system to need initialization
 * @return A standard Mali error code
 */
MALI_STATIC mali_err_code initialize_memory_system(mali_base_ctx_handle ctx) MALI_CHECK_RESULT;
/**
 * Internal memory termination routine
 * @param ctx The base context which caused the memory system to shut down
 */
MALI_STATIC void terminate_memory_system(mali_base_ctx_handle ctx);

/**
 * Internal routine to query the arch of all available memory
 * banks and put them on the linked list of banks
 * @return A standard Mali error code
 */
MALI_STATIC mali_err_code initialize_memory_banks(void) MALI_CHECK_RESULT;

/**
 * Register a bank on the linked list
 * @param bank The bank to put on the list
 */
MALI_STATIC MALI_INLINE void register_memory_bank(mali_mem_bank* bank);

/**
 * Free all memory banks on the linked list
 */
MALI_STATIC void destroy_memory_banks(void);

/**
 * Free a memory bank
 * @param bank The bank to free
 */
MALI_STATIC void destroy_memory_bank(mali_mem_bank* bank);


/**
 *	helper function - returns the minimal fitting order for the given size.
 *	Will put order at or above the minimum.
 *	Examples, with minimum order 6:
 *		size 1-> order 6
 *		size 16 -> order 6
 *		size 17 -> order 6
 *		size 1025 -> order 10
 *		size 1024*1024*8 -> order 23
 * @param size The size to calcuate the order of
 * @return The order calculated
 */
MALI_STATIC u8 get_order(u32 size);

/**
 * Pads a size up to the nearest multiple of MALI_MEMORY_MINIMUM_ALIGNMENT_SIZE
 * @param size The size to pad
 * @return Padded size
 */
MALI_STATIC MALI_INLINE u32 pad_size(u32 size);

/**
 * Check if a memory object is of type MALI_MEM_TYPE_NORMAL
 * @param mem The descriptor to check
 * @return MALI_TRUE if a normal allocation, MALI_FALSE if not
 */
MALI_STATIC_INLINE mali_bool memory_type_is_normal_allocation(mali_mem * mem);

/**
 * Check if a memory object is of type MALI_MEM_TYPE_EXTERNAL_MEMORY
 * @param mem The descriptor to check
 * @return MALI_TRUE if an external allocation, MALI_FALSE if not
 */
MALI_STATIC_INLINE mali_bool memory_type_is_external(mali_mem * mem);



/**
 * Check if the rights of a bank matches the requested rights
 * Returns true if bank has the needed capabilities
 * @param bank_rights The rights of the bank to check
 * @param requested_rights The rights requested
 * @return MALI_TRUE if the bank has all the requested rights, MALI_FALSE if not compatible
 */
MALI_STATIC MALI_INLINE mali_bool compatible_rights(u32 bank_rights, u32 requested_rights);

/**
 * Merge contents of tail into head.
 * Tail will be removed from any lists and released back to our descriptor pool.
 * Head will the sum of both blocks on exit.
 * Upon success returns head.
 * If the merge fails the given failure_handle will be returned instead.
 * @param hint Possible optimization hint
 * @param head First block to merge
 * @param tail Second block to merge
 * @param failure_handle Which handle to return upon error
 * @return Merged handle or @a failure_handle if merge failed
*/
MALI_STATIC mali_mem * merge_mem(mali_mem * head, mali_mem * tail, mali_mem * failure_handle);

/**
 * Split memory block based on the requested alignment and size.
 * mem will be stripped of exccess memory and corrected for aligment
 * @param mem Handle to memory to split
 * @param size The size to end up with
 * @param alignment The alignment to target while splitting
 * @param mali_access The access rights of the splitted memory
 * @return A standard Mali error code
 */
MALI_STATIC mali_err_code split_mem(mali_mem * mem, u32 size, u32 alignment, u32 mali_access) MALI_CHECK_RESULT;

/**
 * Resize the given blocks
 * Will move the given number of bytes from A to B or vice versa.
 * If sizechange is negative memory from A will be moved to B, and vice versa
 * @param hint Possible optimization hint for resize
 * @param A Pointer to block A
 * @param B Pointer to block B
 * @param sizechange Signed value of number of bytes to move from one block to the other
 * @return A standard Mali error code
*/
MALI_STATIC mali_err_code resize_blocks(mali_mem * A, mali_mem * B, s32 sizechange) MALI_CHECK_RESULT;

/**
 * Size changed.
 * Update other member data based on size.
 * @param mem The memory handle which had its size was changed
 */
MALI_STATIC MALI_INLINE void size_changed(mali_mem * mem);

/**
 * Unlink a block from global list
 * Removes the given memory block from the global memory list
 * @param mem Pointer to the block to remove
 */
MALI_STATIC MALI_INLINE void unlink_mem_from_global(mali_mem * mem);

/**
 * Lock a memory bank
 * Will block until the memory bank lock is obtained
 * @param bank The bank to lock
 */
MALI_STATIC MALI_INLINE void bank_lock(mali_mem_bank * bank);

/**
 * Unlock a memory bank
 * @param bank The bank to unlock
 */
MALI_STATIC MALI_INLINE void bank_unlock(mali_mem_bank * bank);

/**
 * Debug helper to check the bank lock status
 * @note Only usage for debug asserting of lock status, might
 * experience a race condition if used for branching
 * @param bank The bank to check the lock status of
 * @return MALI_TRUE if was locked, MALI_FALSE if was unlocked
 */
MALI_DEBUG_CODE(MALI_STATIC MALI_INLINE mali_bool bank_check_lock(mali_mem_bank * bank));

MALI_DEBUG_CODE(
/**
 * Function to calculate the size of all the memory on the free lists
 * Used to assert that the calculated size in mali_mem_bank::free_list_size
 * is correct.
 */
MALI_STATIC u32 bank_calculate_free_size(mali_mem_bank* bank)
{
	mali_embedded_list_link * temp;
	mali_mem * current;
	u32 size = 0;
	u32 i;

	for (i = 0; i < (bank->order_maximum - bank->order_minimum + 1u); ++i)
	{
		/* use safe version since we're removing current from the list as we go */
		MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE(current, temp, &bank->free_list_exact[i], mali_mem, custom.link)
		{
			size += current->size;
		}

		MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE(current, temp, &bank->free_list_bigger[i], mali_mem, custom.link)
		{
			size += current->size;
		}
	}

	return size;
}
)

/**
 * Try to allocated memory from specified bank
 * Will first search exact list, then bigger list,
 * and finally try to extend the bank. If all this failed
 * NULL will be returned.
 * @param bank The bank to allocate memory from
 * @param size The size to allocate
 * @param order Calculated order of size requested
 * @param alignment The alignment needed
 * @param mali_access The minimum capabilities requested
 * @param result Pointer to a pointer to a memory allocation, NULL if allocation failed
 * @return Standard error code. OOM = out of host memory, FAILED = no Mali memory available
 */
MALI_STATIC mali_err_code bank_allocate(mali_mem_bank * bank, u32 size, u32 order, u32 alignment, u32 mali_access, mali_mem ** result) MALI_CHECK_RESULT;

/**
 * Free mem back to the bank.
 * Will perform any merges and put it on the freelist or release it back to the device driver.
 * @param bank The bank from where the memory was allocated
 * @param mem The memory to free
 */
MALI_STATIC void bank_free(mali_mem_bank * bank, mali_mem * mem);

/**
 * Remove given mem block from free list of the given bank
 * @param bank The bank to remove a memory handle from
 * @param mem The block to remove
 * @param org_size The size of the mem block when it was added to the free list.
 */
MALI_STATIC void bank_remove_from_free_list(mali_mem_bank * bank, mali_mem * mem, u32 org_size);

/**
 * Place the given mem block on the free list the given bank
 * @param bank The bank to put the memory handle on
 * @param mem The memory to put on the free list
 */
MALI_STATIC void bank_put_on_free_list(mali_mem_bank * bank, mali_mem * mem);

/**
 * Clear all memory on free list
 * @param bank The bank to free all the memory from
 */
MALI_STATIC void bank_clear_free_list(mali_mem_bank* bank);

/**
 * Clear memory on a banks freelist subpart
 * @param head The linked list to free the memory from
 */
MALI_STATIC void bank_cleanup_free_list(mali_embedded_list_link * head);

/**
 * Find block which matches the requested on the freelist based on alignment
 * @param head Head of a free list subpart
 * @param size Minimum size of the block to find
 * @param alignment The minimum alignment the allocation must have
 * @return Pointer to an allocation or NULL if no memory found
 */
MALI_STATIC mali_mem * bank_free_list_find(mali_embedded_list_link * head, u32 size, u32 alignment);

#if !defined(HARDWARE_ISSUE_3251)

/**
 * Find Mali address inside a heap
 * @param heap_handle Handle to the heap to find the address of
 * @param offset Offset inside the heap to find the address of
 * @return The address calculated
 */
MALI_STATIC mali_addr heap_mem_addr_get(mali_mem_handle heap_handle, u32 offset);

/**
 * Find the block in a heap in which a offset exists
 * @param heap Heap data to use for search
 * @param offset The offset to find
 * @param block Pointer to pointer to store found block in
 * @param offset_in_block The relative offset within the found block of the heap offset
 * @return A standard mali error code
 */
MALI_STATIC mali_err_code heap_find_block_with_offset(heap_extended_data * heap, u32 offset, mali_mem ** block, u32* offset_in_block) MALI_CHECK_RESULT;

/**
 * Returns the size of the allocations of a heap
 * @param heap The heap to find the size of
 * @return The number of allocated bytes for the heap
 */
MALI_STATIC u32 heap_alloc_size_get(mali_mem * heap);

#endif

MALI_DEBUG_CODE(
/**
 * Debug function to dump a banks memory usage map
 * @param bank The bank to dump usage map for
 */
MALI_STATIC void dump_bank_memory_usage_map(mali_mem_bank* bank);
);

#ifdef MALI_TEST_API
MALI_STATIC mali_err_code usage_tracking_init(void) MALI_CHECK_RESULT;
MALI_STATIC void usage_tracking_update(s32 size_change_request, s32 size_change_allocated);
MALI_STATIC void usage_tracking_term(void);

MALI_STATIC mali_mutex_handle allocation_stats_mutex;
MALI_STATIC u32 current_bytes_requested = 0;
MALI_STATIC u32 max_bytes_requested = 0;
MALI_STATIC u32 current_bytes_allocated = 0;
MALI_STATIC u32 max_bytes_allocated = 0;
#endif

#ifdef MALI_DUMP_ENABLE
MALI_STATIC mali_bool clear_all_allocated_memory = MALI_FALSE;
MALI_STATIC u8 clear_memory_value = 0xc1;
MALI_STATIC mali_bool dump_only_written_memory = MALI_FALSE;
#endif


/*
	Descriptor pool handling.
	We have small pool of descriptors ready for use when splitting (alloc/realloc).
	Any merges will put released descriptors back to the pool (free).
	The pool will contain maximum MALI_MEMORY_DESCRIPTOR_POOL_SIZE descriptors.
	Exccess descriptors will be freed back to the arch.
	If the pool is empty new ones will be fetched from the arch in demand.
	On init the pool is filled with MALI_MEMORY_DESCRIPTOR_POOL_SIZE descriptors.
*/

/**
 * Head of linked lsit of avaiable memory descriptors
 */
MALI_STATIC mali_embedded_list_link descriptor_pool_head = MALI_EMBEDDED_LIST_HEAD_INIT_COMPILETIME(descriptor_pool_head);

/**
 * Count of descriptors in the pool
 */
MALI_STATIC u32 descriptor_pool_count = 0;

/**
 * Mutex protecting pool manipulation
 */
MALI_STATIC mali_mutex_handle descriptor_mutex;

/**
 * Descriptor pool initialization
 * Used for filling bool with MALI_MEMORY_DESCRIPTOR_POOL_SIZE during startup and initialize the mutex
 * @return A standard Mali error code
*/
MALI_STATIC mali_err_code descriptor_pool_init(void) MALI_CHECK_RESULT;

/**
 * Descriptor pool cleanup
 * Release all descriptors and destroy mutex
 */
MALI_STATIC MALI_INLINE void descriptor_pool_term(void);

/**
 * Descriptor pool flush
 * Used for emptying pool completely during shutdown / cleanup / free_unused_memory
 */
MALI_STATIC void descriptor_pool_release_all(void);

/**
 * Get a descriptor from the pool.
 * Will return a descriptor from the pool or request a new descriptor from the arch.
 * Will return NULL only if the pool is empty and the arch could not allocate any.
 * @return Pointer to a descriptor or NULL if no descriptor could be allocated
*/
MALI_STATIC mali_mem * descriptor_pool_get(void);

/**
 * Put a descriptor back into the pool.
 * Will put back the descriptor to the pool. If the pool already contains
 * MALI_MEMORY_DESCRIPTOR_POOL_SIZE descriptors it'll be released back to the arch
 * @param descriptor The descriptor to put back into the pool
 */
MALI_STATIC void descriptor_pool_release(mali_mem * descriptor);

/**
 * Lock descriptor pool mutex
 */
MALI_STATIC MALI_INLINE void descriptor_pool_lock(void);

/**
 * Unlock descriptor pool mutex
 */
MALI_STATIC MALI_INLINE void descriptor_pool_unlock(void);


/* mali_mem linked list helper functions */

/**
 * Convert a mali_embedded_list_link * to a mali_mem *
 * @param link The memory list link to convert to a mali_mem
 * @return Pointer to the Mali memory this link represents
 */
MALI_STATIC MALI_INLINE mali_mem * mali_mem_from_link(mali_embedded_list_link * link)
{
	return MALI_EMBEDDED_LIST_GET_CONTAINER(mali_mem, memlink, link);
}
/**
 * Determine if mali_mem has a neighbouring next memory block
 * @param mem The block to check
 * @return MALI_TRUE if the block has a global next, MALI_FALSE if not.
 */
MALI_STATIC MALI_INLINE mali_bool mem_has_global_next(mali_mem * mem)
{
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == mem->memory_subtype, ("Can not use .parent on mem_type: %d\n",mem->memory_subtype));
	return _mali_embedded_list_has_next(&mem->relationship.parent->all_memory, &mem->memlink);
}

/**
 * Get neighbouring next memory block
 * mem_has_global_next has to be called to verify that the block has a global next
 * @param mem The block to find the global next for
 * @return The global next memory block
 */
MALI_STATIC MALI_INLINE mali_mem * mem_global_next(mali_mem * mem)
{
	return mali_mem_from_link(_mali_embedded_list_get_next(&mem->memlink));
}

/**
 * Determine if mali_mem has a neighbouring prev memory block
 * @param mem The block to check
 * @return MALI_TRUE if the block has a global prev, MALI_FALSE if not
 */
MALI_STATIC MALI_INLINE mali_bool mem_has_global_prev(mali_mem * mem)
{
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == mem->memory_subtype, ("Can not use .parent on mem_type: %d\n",mem->memory_subtype));
	return _mali_embedded_list_has_prev(&mem->relationship.parent->all_memory, &mem->memlink);
}

/**
 * Get neighbouring prev memory block
 * mem_has_global_prev has to be called to verify that the block has a global prev
 * @param mem The block to find the global prev for
 * @return The global prev block
 */
MALI_STATIC MALI_INLINE mali_mem * mem_global_prev(mali_mem * mem)
{
	return mali_mem_from_link(_mali_embedded_list_get_prev(&mem->memlink));
}

/**
 * Insert a mali_mem * behind given mali_mem * on the global list
 * @param head The block to insert after
 * @param new_mem The block to insert
 */
MALI_STATIC MALI_INLINE void mem_global_insert_after(mali_mem * head, mali_mem * new_mem)
{
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == head->memory_subtype, ("Can not use .parent on mem_type: %d\n", head->memory_subtype));
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == new_mem->memory_subtype, ("Can not use .parent on mem_type: %d\n", new_mem->memory_subtype));
	MALI_DEBUG_ASSERT(head->relationship.parent == new_mem->relationship.parent, ("Mixed banks on global list"));
	MALI_DEBUG_ASSERT(0 != new_mem->mali_addr, ("Inserting NULL adress on global list"));
	_mali_embedded_list_insert_after(&head->memlink, &new_mem->memlink);
}

/**
 * Insert a mali_mem * in front of given mali_mem * on the global list
 * @param head The block to insert in front of
 * @param new_mem The block insert
*/
MALI_STATIC MALI_INLINE void mem_global_insert_before(mali_mem * head, mali_mem * new_mem)
{
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == head->memory_subtype, ("Can not use .parent on mem_type: %d\n", head->memory_subtype));
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == new_mem->memory_subtype, ("Can not use .parent on mem_type: %d\n", new_mem->memory_subtype));
	MALI_DEBUG_ASSERT(head->relationship.parent == new_mem->relationship.parent, ("Mixed banks on global list"));
	MALI_DEBUG_ASSERT(0 != new_mem->mali_addr, ("Inserting NULL adress on global list"));
	_mali_embedded_list_insert_before(&head->memlink, &new_mem->memlink);
}

/**
 * Insert a mali_mem * as new tail of global linked list starting at head
 * @param head The head of the list to insert to
 * @param new_mem The block to insert as new tail
 */
MALI_STATIC MALI_INLINE void mem_global_insert_tail(mali_embedded_list_link * head, mali_mem * new_mem)
{
	_mali_embedded_list_insert_tail(head, &new_mem->memlink);
}

/**
 * Remove mem from linked list
 * @param mem The memory object to remove from the global linked list
 */
MALI_STATIC MALI_INLINE void mem_global_remove(mali_mem * mem)
{
	_mali_embedded_list_remove(&mem->memlink);
}


/*
	Create a new reference to the memory system.
	Initialize the memory system on the first reference.
*/
mali_err_code _mali_base_common_mem_open(mali_base_ctx_handle ctx)
{
	return initialize_memory_system((void*)ctx);
}

/** Shuts down the memory system */
void _mali_base_common_mem_close(mali_base_ctx_handle ctx)
{
	terminate_memory_system( (void*)ctx );
}

/*
	Allocate mali memory.
	Tries to allocate [size] bytes of memory with [mali_access] capabilities and the given [alignemnt] alignment.
	Returns a handle to the newly allocated memory or NULL if allocation failed
*/
MALI_EXPORT mali_mem_handle _mali_base_common_mem_alloc(
					mali_base_ctx_handle ctx,
					u32 requested_size,
					u32 alignment,
					u32 mali_access)
{
	mali_mem_bank * bank;
	u32 order;
	u32 size;
	mali_mem * result = NULL;

	MALI_IGNORE(ctx);

	/* Zero sized allocations not supported */
	if (0 == requested_size) return MALI_NO_HANDLE;

	/* pad size to our minimum alignment spec */
	size = pad_size(requested_size);
	order = get_order(size);
	alignment = MAX(alignment, MALI_MEMORY_MINIMUM_ALIGNMENT_SIZE);

#if MALI_FORCE_GP_L2_ALLOC
#warning "All mali memory allocations will allocate L2 cache lines when read by GP"
	mali_access |= MALI_GP_L2_ALLOC;
#endif


	/*	OPT
		Should we perform the search in a closes-match-first mode instead?
		Could be solved by a sort-by-most-specific-rights on bank init,
		then a compatible_rights match would first hit on the closest match we have
		and then continue with increasing, but un-neccesary, rights.
	*/

	/* use macro assisted list loop logic */
	/*	for each mali_mem_bank * bank in memory_banks. Linked with member link*/
	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(bank, &memory_banks, mali_mem_bank, link)
	{
		mali_err_code call_result;
		/* ignore non-capable banks */
		if ( MALI_FALSE == compatible_rights(bank->rights, (mali_access & 0xFF))) continue;
		if ( order > bank->order_maximum) continue;

		/* try to allocated from this bank */
		call_result = bank_allocate(bank, size, order, alignment, mali_access, &result);

		if (call_result == MALI_ERR_NO_ERROR)
		{
			/* found memory */
			#ifdef MALI_TEST_API
				((mali_mem*)result)->size_used_to_alloc_call = requested_size;
				usage_tracking_update(requested_size, (MALI_REINTERPRET_CAST(mali_mem*)result)->size);
			#endif

			result->cached_addr_info.mali_address = result->mali_addr;

			if (MALI_FALSE != _mali_base_arch_mem_map(result, 0, result->size, (MALI_MEM_PTR_READABLE | MALI_MEM_PTR_WRITABLE), &result->cached_addr_info.cpu_address))
			{
				_mali_sys_atomic_initialize(&result->cpu_map_ref_count, 1);
			}
			else
			{
				_mali_sys_atomic_initialize(&result->cpu_map_ref_count, 0);
				result->cached_addr_info.cpu_address = NULL;
			}

			#ifdef MALI_DUMP_ENABLE
			/* If this variable is MALI_TRUE all memory blocks will be set to clear_memory_value
			   when they are allocted. This will make it easy to see in the dumps whether a
			   memory address is written to or not.
			*/
			if ( clear_all_allocated_memory )
			{
				void * fill_ptr;
				mali_mem_handle mem;
				u32 size;
				mem = (mali_mem_handle)result;
				size = _mali_mem_size_get(mem);
				fill_ptr = _mali_mem_ptr_map_area(mem, 0, size, 0, MALI_MEM_PTR_WRITABLE);
				if ( NULL != fill_ptr )
				{
					_mali_sys_memset(fill_ptr, (u32)clear_memory_value, size);
					_mali_mem_ptr_unmap_area(mem);
				}
			}
			#endif

			_mali_sys_atomic_initialize(&result->ref_count, 1);

			break; /* done */
		}
		else if (call_result == MALI_ERR_OUT_OF_MEMORY)
		{
			/* out of host memory, abort search */
			break;
		}
		/* else: continue search */
	}

	return MALI_REINTERPRET_CAST(mali_mem_handle)result;
}

/*
	Free memory referenced by mem_handle.
	The mem_handle becomes invalid after this.
*/
void _mali_base_common_mem_free(mali_mem_handle mem_handle)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem *)mem_handle;

	if (MALI_NO_HANDLE == mem) return; /* noop if given a MALI_NO_HANDLE handle */

	if ((NULL == mem->relationship.parent) && (MALI_MEM_TYPE_NORMAL==mem->memory_subtype))
	{
		MALI_DEBUG_ASSERT( MALI_FALSE, ("Memory handle 0x%X does not belong to any memory bank!", mem));
		return;
	}

	if ( MALI_FALSE == mem->is_allocated)
	{
		MALI_DEBUG_ASSERT( MALI_FALSE, ("Double free of memory block 0x%X detected", mem));
		return;
	}

	/* first deatch it from any user created linked list */
	_mali_base_common_mem_list_remove_item(mem_handle);

	/* unmap if mapped */
	if (NULL!=mem->cached_addr_info.cpu_address)
	{
		_mali_base_arch_mem_unmap(mem);
		mem->cached_addr_info.cpu_address = NULL;
	}

	switch (mem->memory_subtype)
	{
		case MALI_MEM_TYPE_NORMAL:
			MALI_DEBUG_ASSERT(NULL != mem->relationship.parent, ("Memory handle 0x%X does not belong to any memory bank!", mem));

			/* ask the bank to free it */
			#ifdef MALI_TEST_API
				usage_tracking_update(-(s32)mem->size_used_to_alloc_call, -(s32)(mem->size));
			#endif
			bank_free(mem->relationship.parent, mem);

			/* the descriptor was freed by bank_free */
			break;

#if !defined(HARDWARE_ISSUE_3251)
		case MALI_MEM_TYPE_HEAP:
		{
			heap_extended_data * heap_data;
			MALI_DEBUG_ASSERT_POINTER(mem->relationship.heap);
			heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)mem->relationship.heap;
			_mali_mem_list_free(heap_data->first);
			_mali_sys_free(heap_data);
			descriptor_pool_release(mem);
			break;
		}
#endif

#if MALI_USE_DMA_BUF
		case MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY:
			_mali_base_arch_mem_dma_buf_release(mem);
			close(mem->relationship.dma_buf_fd);
			descriptor_pool_release(mem);
			break;
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
		case MALI_MEM_TYPE_UMP_EXTERNAL_MEMORY:
			_mali_base_arch_mem_ump_mem_release(mem);
			ump_reference_release(mem->relationship.ump_mem);
			descriptor_pool_release(mem);
			break;
#endif
		case MALI_MEM_TYPE_EXTERNAL_MEMORY:
			_mali_base_arch_release_phys_mem(mem);
			descriptor_pool_release(mem);
			break;
		case MALI_MEM_TYPE_INVALID:
		default:
			MALI_DEBUG_ASSERT( MALI_FALSE, ("Invalid memory subtype type %d given to _mali_mem_free\n", mem->memory_subtype));
			descriptor_pool_release(mem);
			break;
	}
}

u32 _mali_base_common_mem_get_total_allocated_size(void)
{
	mali_mem_bank * bank;
	u32 total_size_allocated = 0;

	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(bank, &memory_banks, mali_mem_bank, link)
	{
		total_size_allocated += bank->size_allocated;
	}

	return total_size_allocated;
}

void _mali_base_common_mem_new_period(void)
{
	mali_mem_bank * bank;

	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(bank, &memory_banks, mali_mem_bank, link)
	{
		bank_lock(bank);

		bank->holdback_size = MAX(
			MAX(
		        MAX(bank->past_size_allocated[0], bank->past_size_allocated[1]),
		        MAX(bank->past_size_allocated[2], bank->past_size_allocated[3])
			),
			MAX(
		        MAX(bank->past_size_allocated[4], bank->past_size_allocated[5]),
		        MAX(bank->past_size_allocated[6], bank->past_size_allocated[7])
			)
		);

		/* add 12.5% -- but maximum 16MB */
		bank->holdback_size += MIN(bank->holdback_size >> 3, 16*1024*1024);

		bank->past_size_allocated[++current_period % MEM_HIST_SIZE] = 0;
		bank_unlock(bank);
	}
}



/*
	Free unused memory held by memory system
*/
void _mali_base_common_mem_free_unused_mem(mali_base_ctx_handle ctx)
{
	mali_mem_bank * bank;
	MALI_IGNORE(ctx);

	/* ask each bank to emtpy their free list */
	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(bank, &memory_banks, mali_mem_bank, link)
	{
		bank_clear_free_list(bank);
	}
	/* release all descriptors */
	descriptor_pool_release_all();
}



mali_addr _mali_base_common_mem_addr_get_full(mali_mem_handle handle, u32 offset)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem *)handle;

	MALI_DEBUG_ASSERT(NULL != mem, ("Can't get mali address for a NULL handle"));
	if (NULL == mem) return 0;

	MALI_DEBUG_ASSERT( MALI_TRUE == mem->is_allocated, ("Operation on free memory block 0x%X detected", mem));

#if !defined(HARDWARE_ISSUE_3251)
	if ( _mali_base_common_mem_is_heap(handle) )
	{
		return heap_mem_addr_get(handle, offset);
	}
#endif

	MALI_DEBUG_ASSERT(mem->size >= offset, ("mem_addr_get would result in address beyond end of buffer. Handle 0x%X. Size %d < offset %d)", mem, mem->size, offset));
	if (mem->size < offset) return 0;

	return mem->mali_addr + offset;
}

/****** Get more information about current block ******/

u32 _mali_base_common_mem_size_get(mali_mem_handle mem_handle)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem *)mem_handle;

	MALI_DEBUG_ASSERT(NULL != mem, ("Can't get size of a NULL handle"));
	if (NULL == mem) return 0;

	MALI_DEBUG_ASSERT( MALI_TRUE == mem->is_allocated, ("Operation on free memory block 0x%X detected", mem));

#if !defined(HARDWARE_ISSUE_3251)
	if ( _mali_base_common_mem_is_heap(mem_handle) ) return heap_alloc_size_get(mem);
#endif
	if ( memory_type_is_normal_allocation(mem) || memory_type_is_external(mem)) return mem->size;
	MALI_DEBUG_ASSERT( MALI_FALSE, ("_mali_mem_size_get requested on unsupported memory subtype %d\n", mem->memory_subtype));
	return 0;
}

u32 _mali_base_common_mem_order_get(mali_mem_handle mem_handle)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem *)mem_handle;

	MALI_DEBUG_ASSERT(NULL != mem, ("Can't get order of a NULL handle"));
	if (NULL == mem) return 0;

	MALI_DEBUG_ASSERT( MALI_TRUE == mem->is_allocated, ("Operation on free memory block 0x%X detected", mem));

#if !defined(HARDWARE_ISSUE_3251)
	MALI_DEBUG_ASSERT(!_mali_base_common_mem_is_heap(mem_handle), ("_mali_mem_order_get called on a heap, not supported\n"));
	MALI_CHECK(!_mali_base_common_mem_is_heap(mem_handle), 0);
#endif
	MALI_DEBUG_ASSERT(memory_type_is_normal_allocation(mem), ("_mali_mem_size_get requested on unsupported memory subtype %d\n", mem->memory_subtype));
	MALI_CHECK(memory_type_is_normal_allocation(mem), 0);

	return mem->order;
}

u32 _mali_base_common_mem_alignment_get(mali_mem_handle mem_handle)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem *)mem_handle;

	MALI_DEBUG_ASSERT(NULL != mem, ("Can't get aligment for a NULL handle"));
	if (NULL == mem) return 0;

	MALI_DEBUG_ASSERT( MALI_TRUE == mem->is_allocated, ("Operation on free memory block 0x%X detected", mem));

	return mem->alignment;
}

u32 _mali_base_common_mem_usage_get(mali_mem_handle mem_handle)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem *)mem_handle;

	MALI_DEBUG_ASSERT(NULL != mem, ("Can't get memory usage of a NULL handle"));
	if (NULL == mem) return 0;

	MALI_DEBUG_ASSERT( MALI_TRUE == mem->is_allocated, ("Operation on free memory block 0x%X detected", mem));

	return (mali_mem_usage_flag)mem->effective_rights;
}


/****** Using mali_mem_handle as a linked list with an user_pointer: ******/

mali_mem_handle _mali_base_common_mem_list_insert_after(mali_mem_handle current_handle, mali_mem_handle newnext_handle)
{
	mali_mem *current = MALI_REINTERPRET_CAST(mali_mem *)current_handle, *newnext = MALI_REINTERPRET_CAST(mali_mem*)newnext_handle;

	/* if current is NULL we return the new handle */
	if (NULL == current) return newnext_handle;
	/* insertion of a NULL handle, ignored */
	if (NULL == newnext) return current_handle;

	if (( MALI_FALSE == current->is_allocated) || ( MALI_FALSE == newnext->is_allocated))
	{
		MALI_DEBUG_ASSERT( MALI_FALSE, ("Inserting unallocated memory block to user defined list"));
		return current_handle;
	}

	{
		mali_mem *last = MALI_NO_HANDLE;
		mali_mem *p = current;

		while( NULL != p )
		{
			last = p;
			p = p->custom.user.next;
		}

		MALI_DEBUG_ASSERT_POINTER( last );

		last->custom.user.next = newnext;
		newnext->custom.user.prev = last;
	}

	/* return the first of the two nodes */
	return current_handle;
}

mali_mem_handle _mali_base_common_mem_list_insert_before(mali_mem_handle current_handle, mali_mem_handle newprev_handle)
{
	mali_mem *current = MALI_REINTERPRET_CAST(mali_mem *)current_handle, *newprev = MALI_REINTERPRET_CAST(mali_mem*)newprev_handle;

	/* if current is NULL just return newprev_handle */
	if (NULL == current) return newprev_handle;
	/* insertion of a NULL handle, ignored */
	if (NULL == newprev) return current_handle;

	if (( MALI_FALSE == current->is_allocated) || ( MALI_FALSE == newprev->is_allocated))
	{
		MALI_DEBUG_ASSERT( MALI_FALSE, ("Inserting unallocated memory block to user defined list"));
		return current_handle;
	}

	newprev->custom.user.next = current;
	newprev->custom.user.prev = current->custom.user.prev;
	current->custom.user.prev = newprev;

	if (NULL != newprev->custom.user.prev) newprev->custom.user.prev->custom.user.next = newprev;

	/* return the first of the two nodes */
	return newprev_handle;
}

mali_mem_handle _mali_base_common_mem_list_get_next(mali_mem_handle mem_handle)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem * )mem_handle;

	MALI_DEBUG_ASSERT(NULL != mem, ("Can't get next for a NULL handle"));
	if (NULL == mem) return NULL;

	MALI_DEBUG_ASSERT( MALI_TRUE == mem->is_allocated, ("Trying to walk a user defined list with an unallocated memory block"));
	if ( MALI_FALSE == mem->is_allocated) return NULL;

	return MALI_REINTERPRET_CAST(mali_mem_handle)MALI_REINTERPRET_CAST(mali_mem *)mem->custom.user.next;
}

mali_mem_handle _mali_base_common_mem_list_get_previous(mali_mem_handle mem_handle)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem * )mem_handle;

	MALI_DEBUG_ASSERT(NULL != mem, ("Can't get previous for a NULL handle"));
	if (NULL == mem) return NULL;

	MALI_DEBUG_ASSERT( MALI_TRUE == mem->is_allocated, ("Trying to walk a user defined list with an unallocated memory block"));
	if ( MALI_FALSE == mem->is_allocated) return NULL;

	return MALI_REINTERPRET_CAST(mali_mem_handle)MALI_REINTERPRET_CAST(mali_mem *)mem->custom.user.prev;
}

mali_mem_handle _mali_base_common_mem_list_remove_item(mali_mem_handle mem_handle)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem * )mem_handle, *next, *prev;

	MALI_DEBUG_ASSERT(NULL != mem, ("Can't remove a NULL handle"));
	if (NULL == mem) return NULL;


	MALI_DEBUG_ASSERT( MALI_TRUE == mem->is_allocated, ("Trying remove an unallocated block from a user defined list"));
	if ( MALI_FALSE == mem->is_allocated) return NULL;

	prev = mem->custom.user.prev;
	next = mem->custom.user.next;

	if (NULL != prev) prev->custom.user.next = next;
	if (NULL != next) next->custom.user.prev = prev;

	mem->custom.user.prev = NULL;
	mem->custom.user.next = NULL;

	return (NULL != next) ? MALI_REINTERPRET_CAST(mali_mem_handle)next : MALI_REINTERPRET_CAST(mali_mem_handle)prev;
}

/* free an entire list of mali_mem's */
void _mali_base_common_mem_list_free(mali_mem_handle list_handle)
{
	mali_mem * list = MALI_REINTERPRET_CAST(mali_mem *)list_handle;

	if (NULL == list)
	{
		/* nothing to do */
		return;
	}

	/*
		We can skip checking for allocation status in the loop.
		Only allocated memory has been put on a list, so just have to check the initial block.
	*/

	MALI_DEBUG_ASSERT( MALI_TRUE == list->is_allocated, ("Trying to perform free of unallocated memory"));
	if ( MALI_FALSE == list->is_allocated) return;

	while (NULL != list)
	{
		mali_mem * item = list;
		list = list->custom.user.next;
		_mali_base_common_mem_free(MALI_REINTERPRET_CAST(mali_mem_handle)item);
	}
}

/* Return the sum of all the blocks' size on this list */
u32 _mali_base_common_mem_list_size_get(mali_mem_handle list_handle)
{
	u32 sum = 0;
	mali_mem * list = MALI_REINTERPRET_CAST(mali_mem *)list_handle;

	MALI_DEBUG_ASSERT(NULL != list, ("Can't calculate size of a NULL list"));
	if (NULL == list) return 0;

	MALI_DEBUG_ASSERT( MALI_TRUE == list->is_allocated, ("Trying to get size of invalid memory handle 0x%X", list));
	if ( MALI_FALSE == list->is_allocated) return 0;

	while (NULL != list)
	{
		sum += list->size;
		list = list->custom.user.next;
	}

	return sum;
}

/* static helper functions */

/* add a bank to our list of banks */
MALI_STATIC MALI_INLINE void register_memory_bank(mali_mem_bank* bank)
{
	MALI_DEBUG_ASSERT_POINTER(bank);
	_mali_embedded_list_insert_tail(&memory_banks, &bank->link);
}

/* meory system init call to initialize all banks reported by arch */
MALI_STATIC mali_err_code initialize_memory_banks(void)
{
	u32 i;
	u32 sets;
	u32 bufferSize;
	mali_mem_bank_info * meminfo;

	MALI_CHECK( MALI_TRUE == _mali_embedded_list_is_empty(&memory_banks), MALI_ERR_FUNCTION_FAILED);

	sets = _mali_base_arch_mem_get_num_capability_sets();
	MALI_CHECK(0 != sets, MALI_ERR_FUNCTION_FAILED); /* no memory available from arch */

	/* create a buffer to store info about each bank available from arch */
	bufferSize = sets * sizeof(mali_mem_bank_info);

	meminfo = MALI_REINTERPRET_CAST(mali_mem_bank_info*)_mali_sys_malloc(bufferSize);
	MALI_CHECK_NON_NULL(meminfo, MALI_ERR_OUT_OF_MEMORY);

	/* now retrieve the info */
	if (MALI_ERR_NO_ERROR != _mali_base_arch_mem_get_capability_sets(meminfo, bufferSize))
	{
		_mali_sys_free(meminfo);
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}

	/* create a bank object for each bank reported by arch */
	for (i = 0; i < sets; ++i)
	{
		u32 freelist_size, j;
		mali_mem_bank * bank;

		/* if found bank has too small order maximum we fail it, but try others */
		if (MALI_MEMORY_MINIMUM_ORDER_SUPPORTED > meminfo[i].maximum_order_supported) continue;

		bank = MALI_REINTERPRET_CAST(mali_mem_bank*)_mali_sys_calloc(1, sizeof(mali_mem_bank));
		if (NULL == bank)
		{
			_mali_sys_free(meminfo);
			destroy_memory_banks();
			MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
		}
		/* it's safe to register it already */
		register_memory_bank(bank);

		bank->mutex = _mali_sys_mutex_create();
		if (NULL == bank->mutex)
		{
			_mali_sys_free(meminfo);
			destroy_memory_banks();
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}

		bank->rights = meminfo[i].capabilities;
		bank->size_total = meminfo[i].size;
		bank->order_minimum = MALI_MEMORY_MINIMUM_ORDER_SUPPORTED;
		bank->order_maximum = meminfo[i].maximum_order_supported;
		bank->cache_settings = meminfo[i].cache_settings;
		freelist_size = bank->order_maximum - bank->order_minimum + 1;

		for (j = 0; j < MEM_HIST_SIZE; ++j)
		{
			bank->past_size_allocated[j] = 0;
		}

		/* allocated memory for 'freelist_size' head pointers for free lists */
		bank->free_list_exact = _mali_sys_malloc(sizeof(mali_embedded_list_link) * freelist_size);
		bank->free_list_bigger = _mali_sys_malloc(sizeof(mali_embedded_list_link) * freelist_size);

		if ( (NULL == bank->free_list_exact) || (NULL == bank->free_list_bigger) )
		{
			if (NULL != bank->free_list_exact) _mali_sys_free(bank->free_list_exact);
			if (NULL != bank->free_list_bigger) _mali_sys_free(bank->free_list_bigger);
			bank->free_list_exact = NULL;
			bank->free_list_bigger = NULL;
			_mali_sys_free(meminfo);
			destroy_memory_banks();
			MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
		}
		/* initialize all_memory head */
		MALI_EMBEDDED_LIST_HEAD_INIT_RUNTIME(&bank->all_memory);

		/* initialize free list heads */
		for (j = 0; j < freelist_size; ++j)
		{
			MALI_EMBEDDED_LIST_HEAD_INIT_RUNTIME(&bank->free_list_exact[j]);
			MALI_EMBEDDED_LIST_HEAD_INIT_RUNTIME(&bank->free_list_bigger[j]);
		}

		/* ask the backend for the allocation */
		if (MALI_ERR_NO_ERROR != _mali_base_arch_mem_init_bank(bank->cache_settings))
		{
			/* backend failed the request */
			_mali_sys_free(meminfo);
			destroy_memory_banks();
			MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
		}

	}

	_mali_sys_free(meminfo);

	if ( MALI_TRUE == _mali_embedded_list_is_empty(&memory_banks)) MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	else MALI_SUCCESS;
}

/* Clear out all mem on a freelist */
MALI_STATIC void bank_cleanup_free_list(mali_embedded_list_link * head)
{
	mali_embedded_list_link * temp;
	mali_mem * current;

	/* use safe version since we're removing current from the list as we go */
	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE(current, temp, head, mali_mem, custom.link)
	{
		_mali_embedded_list_remove(&current->custom.link);
		_mali_base_arch_mem_release_memory(current);
		unlink_mem_from_global(current);
		descriptor_pool_release(current);
	}
}

/* Free all memory on free list of bank */
MALI_STATIC void bank_clear_free_list(mali_mem_bank* bank)
{
	u32 i;
	MALI_DEBUG_ASSERT_POINTER(bank);

	bank_lock(bank);

	for (i = 0; i < (bank->order_maximum - bank->order_minimum + 1u); ++i)
	{
		if (NULL != bank->free_list_exact)
		{
			bank_cleanup_free_list(&bank->free_list_exact[i]);
		}

		if (NULL != bank->free_list_bigger)
		{
			bank_cleanup_free_list(&bank->free_list_bigger[i]);
		}
	}
	/* head pointers not released */
	bank_unlock(bank);
}

/* destroy a memory bank */
MALI_STATIC void destroy_memory_bank(mali_mem_bank* bank)
{
	MALI_DEBUG_ASSERT_POINTER(bank);

	/* first perform cleanup of freelist */
	bank_clear_free_list(bank);

	/* release freelist head pointers */
	if (NULL != bank->free_list_exact)
	{
		_mali_sys_free(bank->free_list_exact);
		bank->free_list_exact = NULL;
	}

	if (NULL != bank->free_list_bigger)
	{
		_mali_sys_free(bank->free_list_bigger);
		bank->free_list_bigger = NULL;
	}

	/* free mutex used by the bank */
	if (NULL != bank->mutex)
	{
		_mali_sys_mutex_destroy(bank->mutex);
		bank->mutex = NULL;
	}

	/* then the bank struct itself */
	_mali_sys_free(bank);

	return;
}

MALI_STATIC void destroy_memory_banks(void)
{
	mali_mem_bank * bank;
	mali_embedded_list_link * temp;
	mali_mem * mem;
	mali_mem_handle forced_free_list = MALI_NO_HANDLE;

	/* loop over all banks, use the safe loop version since we're removing the banks as we go */
	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE(bank, temp, &memory_banks, mali_mem_bank, link)
	{
		if (0 != bank->allocated_elements)
		{
			MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(mem, &bank->all_memory, mali_mem, memlink)
			{
				if ( MALI_TRUE == mem->is_allocated)
				{
					/* detach from possible user defined linked list */
					mem->custom.link.next = mem->custom.link.prev = NULL;

					forced_free_list = _mali_mem_list_insert_after(forced_free_list, (mali_mem_handle)mem);

					#if defined DEBUG
						MALI_DEBUG_PRINT(1, ("Leaked memory block\n"));
						MALI_DEBUG_PRINT(1, ("\tHandle: 0x%X\n", mem));
						MALI_DEBUG_PRINT(1, ("\tMali address: 0x%X\n", mem->mali_addr));
						#ifdef MALI_TEST_API
						MALI_DEBUG_PRINT(1, ("\tRequested size: %d\n" , mem->size_used_to_alloc_call));
						#endif
						MALI_DEBUG_PRINT(1, ("\tEffective size: %d\n", mem->size));
						MALI_DEBUG_PRINT(1, ("\tRequested rights: 0x%08x\n", mem->effective_rights));
						#ifdef MALI_MEMORY_PROFILING
							MALI_DEBUG_PRINT(1, ("\tAllocated by: %s (%s:%d)\n", mem->function_name, mem->file_name, mem->line_nr));
							MALI_DEBUG_PRINT(1, ("\tAllocation number: %d\n", mem->alloc_nr));
							MALI_DEBUG_PRINT(1, ("\tTime of allocation: %lld usecs after first allocation in application\n", mem->alloc_time));
						#endif
					#endif
				}
			}
			/* forcefully free the leaked memory */
			_mali_mem_list_free(forced_free_list);
			forced_free_list = MALI_NO_HANDLE;
		}

		_mali_embedded_list_remove(&bank->link);
		destroy_memory_bank(bank);
	}
}

MALI_STATIC mali_err_code initialize_memory_system(mali_base_ctx_handle ctx)
{
	mali_err_code err;

	MALI_IGNORE(ctx);

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	if(UMP_OK!=ump_open())
	{
		MALI_DEBUG_ERROR(("Could not open UMP memory system. Shutdown.\n"));
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}
	MALI_DEBUG_PRINT(2, ("UMP driver started successfully. \n"));
#endif

#ifdef MALI_TEST_API
	err = usage_tracking_init();
	if (MALI_ERR_NO_ERROR != err) MALI_ERROR(err);
#endif

#ifdef MALI_MEMORY_PROFILING
	_mali_sys_atomic_initialize(&local_static_alloc_nr, 0);
#endif

#ifdef MALI_DUMP_ENABLE
	/* Example: if these are among the systems environment varibles:
	   MALI_MEM_CLEAR=1 MALI_MEM_CLEAR_VALUE=0xff
	   All mali memory blocks will be filled with 0xff when they are allocated.
	   This will help us see in memory dumps which addresses that are modified and which are untouched.
	*/
	dump_only_written_memory = _mali_sys_config_string_get_bool("MALI_DUMP_MINIMUM", MALI_FALSE);
	if (dump_only_written_memory )
	{
		clear_all_allocated_memory = MALI_TRUE;
	}
	clear_all_allocated_memory= _mali_sys_config_string_get_bool("MALI_MEM_CLEAR", clear_all_allocated_memory);
	clear_memory_value = (u8)_mali_sys_config_string_get_s64("MALI_MEM_CLEAR_VALUE",clear_memory_value, 0 , MALI_U8_MAX);
#endif

	err = descriptor_pool_init();
	if (MALI_ERR_NO_ERROR == err)
	{
		err = _mali_base_arch_mem_open();
		if (MALI_ERR_NO_ERROR == err)
		{
			err = initialize_memory_banks();
			if (MALI_ERR_NO_ERROR == err)
			{
				MALI_SUCCESS;
			}
			_mali_base_arch_mem_close();
		}
		descriptor_pool_term();
	}
	#ifdef MALI_TEST_API
	usage_tracking_term();
	#endif
	MALI_ERROR(err);
}

MALI_STATIC void terminate_memory_system(mali_base_ctx_handle ctx)
{
	MALI_IGNORE(ctx);
	destroy_memory_banks();
	_mali_base_arch_mem_close();
	descriptor_pool_term();
	#ifdef MALI_TEST_API
	usage_tracking_term();
	#endif
	#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	ump_close();
	#endif
}

MALI_STATIC u8 get_order(u32 size)
{
	u8 order = 0;

	while ( 0 != (size >>= 1) ) order++;

	if (order < MALI_MEMORY_MINIMUM_ORDER_SUPPORTED) order = MALI_MEMORY_MINIMUM_ORDER_SUPPORTED;

	return order;
}

MALI_STATIC MALI_INLINE u32 pad_size(u32 size)
{
	return (size + MALI_MEMORY_MINIMUM_ALIGNEMENT_MASK) & ~MALI_MEMORY_MINIMUM_ALIGNEMENT_MASK;
}

MALI_STATIC MALI_INLINE mali_bool compatible_rights(u32 bankrights, u32 requestedrights)
{
	return ((bankrights & requestedrights) == requestedrights) ? MALI_TRUE : MALI_FALSE;
}

/**
 * Update the pow2 status flag
 * @param mem The block to update the flag for
 */
MALI_STATIC MALI_INLINE void update_pow2_flag(mali_mem * mem)
{
	MALI_DEBUG_ASSERT_POINTER(mem);
	MALI_DEBUG_ASSERT_POINTER(mem->relationship.parent);
	/* is pow2 if not larger than largest order supported by bank and size == 2^order */
	mem->is_pow2 = ( ( (1u << mem->order) == mem->size) && (mem->order <= mem->relationship.parent->order_maximum));
}

MALI_STATIC MALI_INLINE void unlink_mem_from_global(mali_mem * mem)
{
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == mem->memory_subtype, ("Can not use .parent on mem_type: %d\n",mem->memory_subtype));
	MALI_DEBUG_ASSERT(bank_check_lock(mem->relationship.parent), ("Bank not locked while unlinking from global"));
	_mali_embedded_list_remove(&mem->memlink);
}

MALI_STATIC mali_mem * merge_mem(mali_mem * head, mali_mem * tail, mali_mem * failure_handle)
{
	u32 head_org_size;
	u32 tail_org_size;

	MALI_DEBUG_ASSERT_POINTER(head);
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == head->memory_subtype, ("Can not use .parent on mem_type: %d\n",head->memory_subtype));
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == tail->memory_subtype, ("Can not use .parent on mem_type: %d\n",tail->memory_subtype));
	MALI_DEBUG_ASSERT_POINTER(head->relationship.parent);
	MALI_DEBUG_ASSERT_POINTER(tail);
	MALI_DEBUG_ASSERT_POINTER(tail->relationship.parent);
	MALI_DEBUG_ASSERT(head->relationship.parent == tail->relationship.parent, ("Attempt to do an interbank merge"));
	MALI_DEBUG_ASSERT(( MALI_FALSE == head->is_allocated) && ( MALI_FALSE == tail->is_allocated), ("Merging memory which is in use"));

	MALI_DEBUG_ASSERT(bank_check_lock(head->relationship.parent), ("Bank not locked while merge_mem\n"));

	head_org_size = head->size;
	tail_org_size = tail->size;

	/* try to perform a resize of tail block to 0 size */
	/* if it failed return the tail block */
	if (MALI_ERR_NO_ERROR != resize_blocks(head, tail, tail->size)) return failure_handle;

	bank_remove_from_free_list(head->relationship.parent, head, head_org_size);

	/* remove tail from any linked lists, both free list and global */
	bank_remove_from_free_list(tail->relationship.parent, tail, tail_org_size);
	unlink_mem_from_global(tail);

	/* tail no longer in use */
	descriptor_pool_release(tail);

	/* return merged item */
	return head;
}

MALI_STATIC void bank_free(mali_mem_bank * bank, mali_mem * mem)
{
	MALI_DEBUG_ASSERT_POINTER(bank);
	MALI_DEBUG_ASSERT_POINTER(mem);
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == mem->memory_subtype, ("Can not use .parent on mem_type: %d\n",mem->memory_subtype));
	MALI_DEBUG_ASSERT(mem->relationship.parent == bank, ("Mem 0x%X does not belong to bank 0x%X", mem, bank));

	bank_lock(bank);

	mem->is_allocated = MALI_FALSE;
	bank->allocated_elements--;
	MALI_DEBUG_ASSERT(bank->size_allocated >= mem->size, ("Incorrect allocation status"));
	bank->size_allocated -= mem->size;
	MALI_DEBUG_ASSERT(bank->size_allocated <= bank->size_total, ("Incorrect allocation status"));

	if (mem_has_global_prev(mem) && ( MALI_FALSE == mem_global_prev(mem)->is_allocated))
	{
		/* try to merge with global prev */
		mem = merge_mem(mem_global_prev(mem), mem, mem);
	}

	if (mem_has_global_next(mem) && ( MALI_FALSE == mem_global_next(mem)->is_allocated))
	{
		/* try to merge with global next */
		mem = merge_mem(mem, mem_global_next(mem), mem);
	}

	if ((bank->size_allocated + bank->free_list_size) < bank->holdback_size)
	{
		/* Hold back a certain amount, in case we need this memory again */
		bank_put_on_free_list(bank, mem);
		bank_unlock(bank);
	}
	else
	{
		/* check if block can be deallocated */
		if (MALI_TRUE == _mali_base_arch_mem_is_full_block(mem))
		{
			unlink_mem_from_global(mem);
			bank_remove_from_free_list(bank, mem, mem->size);
			bank_unlock(bank);

			_mali_base_arch_mem_release_memory(mem);
			descriptor_pool_release(mem);
		}
		else
		{
			bank_put_on_free_list(bank, mem);
			bank_unlock(bank);
		}
	}
}

MALI_STATIC mali_err_code split_mem(mali_mem * mem, u32 size, u32 alignment, u32 mali_access)
{
	u32 size_before, size_after;
	mali_mem * before = NULL, * after = NULL;

	MALI_IGNORE(mali_access);

	MALI_DEBUG_ASSERT_POINTER(mem);
	MALI_DEBUG_ASSERT(size >= MALI_MEMORY_MINIMUM_ALIGNMENT_SIZE, ("Size %d below minimum supported alloc size %d", size, MALI_MEMORY_MINIMUM_ALIGNMENT_SIZE));
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == mem->memory_subtype, ("Can not use .parent on mem_type: %d\n",mem->memory_subtype));

	MALI_DEBUG_ASSERT(bank_check_lock(mem->relationship.parent), ("Bank not locked while split_mem\n"));

	size_before = ((mem->mali_addr + (alignment - 1)) & ~(alignment - 1)) - mem->mali_addr;
	size_after = mem->size - size_before - size;

	if (0 != size_before)
	{
		before = descriptor_pool_get();
		MALI_CHECK_NON_NULL(before, MALI_ERR_OUT_OF_MEMORY);
	}

	if ( (0 != size_after) && (size_after >= MALI_MEMORY_MINIMUM_ALIGNMENT_SIZE) )
	{
		after = descriptor_pool_get();
		if (NULL == after)
		{
			if (NULL != before) descriptor_pool_release(before);
			MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
		}
	}

	if ( NULL != after )
	{
		after->relationship.parent = mem->relationship.parent;
		after->mali_addr = mem->mali_addr + mem->size;
		after->memory_subtype = mem->memory_subtype;

		/* insert into the global memory list */
		mem_global_insert_after(mem, after);

		if (MALI_ERR_NO_ERROR != resize_blocks(mem, after, -(s32)size_after))
		{
			/* failed, remove from the global list, release descriptor and indicate error */
			/* original block untouched */
			mem_global_remove(after);
			descriptor_pool_release(after);
			/* free potential preallocated before descriptor */
			if (NULL != before) descriptor_pool_release(before);
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		MALI_DEBUG_ASSERT(after->size == size_after,  ("Block size incorrect after split. Should be %d, is %d", size_after, after->size));

		/* put split off part on the free list */
		bank_put_on_free_list(after->relationship.parent, after);
	}

	if (NULL != before)
	{
		before->relationship.parent = mem->relationship.parent;
		before->mali_addr = mem->mali_addr;
		before->memory_subtype = mem->memory_subtype;

		/* insert into the global memory list */
		mem_global_insert_before(mem, before);

		if (MALI_ERR_NO_ERROR != resize_blocks(before, mem, size_before))
		{
			/* resize failed, release allocated descriptor */
			mem_global_remove(before);
			descriptor_pool_release(before);

			if (NULL != after)
			{
				/* original block touched, roll back */
				mali_mem * result;
				result = merge_mem(mem, after, NULL);
				/* the after descriptor was unlinked and freed by merge_mem function */
				/*
					If the assert is triggered the backend does not support merging back the blocks we just split.
					This should not happend if the backend is correcly implemented.
				*/
				MALI_DEBUG_ASSERT(NULL != result, ("Failed to merge back previously split memory block pair"));
				MALI_IGNORE(result);

			}
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		MALI_DEBUG_ASSERT(before->size == size_before, ("Block size incorrect after split. Should be %d, is %d", size_before, before->size));

		/* put split off part on the free list */
		bank_put_on_free_list(before->relationship.parent, before);
	}

	MALI_SUCCESS;
}

MALI_STATIC mali_mem * bank_free_list_find(mali_embedded_list_link * head, u32 size, u32 alignment)
{
	mali_mem * mem = NULL;

	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(mem, head, mali_mem, custom.link)
	{
		u32 alignment_correction = ((mem->mali_addr + (alignment - 1)) & ~(alignment - 1)) - mem->mali_addr;
		if ( mem->size >= (size + alignment_correction) ) return mem;
	}

	return NULL;
}

MALI_STATIC mali_err_code bank_allocate(mali_mem_bank * bank, u32 size, u32 order, u32 alignment, u32 mali_access, mali_mem ** result)
{
	u32 i;
	u32 order_index;
	mali_mem * found = NULL;
	mali_err_code err;

	MALI_DEBUG_ASSERT_POINTER(bank);

	order_index = order - MALI_MEMORY_MINIMUM_ORDER_SUPPORTED;

	bank_lock(bank);

	/*_mali_sys_printf("Checking bank 0x%X, having %d free list elements (%d/%d)\n", bank, bank->free_list_elements, bank->size_allocated, bank->size_total);*/

	for (i = order_index; i <= (bank->order_maximum - MALI_MEMORY_MINIMUM_ORDER_SUPPORTED); ++i)
	{
		if ( MALI_FALSE == _mali_embedded_list_is_empty(&bank->free_list_exact[i]))
		{
			found = bank_free_list_find(&bank->free_list_exact[i], size, alignment);
			if (NULL != found)
			{
				MALI_DEBUG_ASSERT(found->relationship.parent == bank, ("Found corrupted memory block on exact free list"));
				/* remove from free list */
				bank_remove_from_free_list(found->relationship.parent, found, found->size);
				break;
			}
		}

		if ( MALI_FALSE == _mali_embedded_list_is_empty(&bank->free_list_bigger[i]))
		{
			found = bank_free_list_find(&bank->free_list_bigger[i], size, alignment);
			if (NULL != found)
			{
				MALI_DEBUG_ASSERT(found->relationship.parent == bank, ("Found corrupted memory block on bigger free list"));
				/* remove from free list */
				bank_remove_from_free_list(found->relationship.parent, found, found->size);
				break;
			}
		}
	}

	if (NULL == found)
	{
		/* unlock the bank while we try to get memory from the underlying arch */
		bank_unlock(bank);

		/* We failed to find a match on the free list, try to request more memory from arch */
		/* get a descriptor for bank expansion */
		found = descriptor_pool_get();
		if (NULL == found)
		{
			MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
		}

		/* request more memory of this bank's type */
		if (MALI_ERR_NO_ERROR != _mali_base_arch_mem_get_memory(bank->cache_settings, pad_size(size + alignment - 1), found))
		{
			descriptor_pool_release(found);
			MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
		}
		/*
			If the bank was expanded, attach the new memory to the bank.
			No need to put it on the freelist.
		*/
		found->relationship.parent = bank;
		found->memory_subtype = MALI_MEM_TYPE_NORMAL;

		/* lock bank again as we add the memory we allocated to it */
		bank_lock(bank);
		mem_global_insert_tail(&bank->all_memory, found);
	}

	err = split_mem(found, size, alignment, mali_access);
	if (MALI_ERR_NO_ERROR != err)
	{
		size_changed(found);
		bank_put_on_free_list(found->relationship.parent, found);
		bank_unlock(bank);
		MALI_ERROR(err);
	}

	found->alignment = alignment;
	found->is_allocated = MALI_TRUE;
	found->effective_rights = mali_access;
	bank->allocated_elements++;
	bank->size_allocated += found->size;

	if (bank->past_size_allocated[current_period % MEM_HIST_SIZE] < bank->size_allocated)
	{
		bank->past_size_allocated[current_period % MEM_HIST_SIZE] = bank->size_allocated;
	}


	MALI_DEBUG_ASSERT(bank->size_allocated <= bank->size_total, ("Incorrect allocation status"));

	bank_unlock(bank);

	*result = found;

	MALI_SUCCESS;
}

MALI_STATIC void bank_remove_from_free_list(mali_mem_bank * bank, mali_mem * mem, u32 org_size)
{
	MALI_DEBUG_ASSERT_POINTER(bank);
	MALI_DEBUG_ASSERT_POINTER(mem);
	MALI_DEBUG_ASSERT( MALI_FALSE == mem->is_allocated, ("Allocated memory found on freelist"));

	MALI_DEBUG_ASSERT(bank_check_lock(mem->relationship.parent), ("Bank not locked while bank_remove_from_free_list\n"));

	/* handle free'd memory which is not put on any free list yet */
	if ( MALI_FALSE == _mali_embedded_list_is_empty(&mem->custom.link))
	{
		_mali_embedded_list_remove(&mem->custom.link);
		mem->custom.link.next = mem->custom.link.prev = NULL;
		bank->free_list_elements--;
		bank->free_list_size -= org_size;
		MALI_DEBUG_ASSERT(bank->free_list_size == bank_calculate_free_size(bank), ("Bank free size mismatch"));
	}
}

MALI_STATIC void bank_put_on_free_list(mali_mem_bank * bank, mali_mem * mem)
{
	MALI_DEBUG_ASSERT_POINTER(mem);
	MALI_DEBUG_ASSERT_POINTER(bank);
	MALI_DEBUG_ASSERT_POINTER(bank->free_list_exact);
	MALI_DEBUG_ASSERT_POINTER(bank->free_list_bigger);
	MALI_DEBUG_ASSERT(0 < mem->size, ("Putting zero sized block on free list"));

	MALI_DEBUG_ASSERT( MALI_FALSE == mem->is_allocated, ("Allocated memory found on freelist"));
	MALI_DEBUG_ASSERT(mem->order >= bank->order_minimum, ("Order %d smaller that minimum supported (%d) in this bank", mem->order, bank->order_minimum));

	MALI_DEBUG_ASSERT(mem->relationship.parent == bank, ("Memory not a member of this bank (0x%X != 0x%X)", mem->relationship.parent, bank));

	MALI_DEBUG_ASSERT(bank_check_lock(mem->relationship.parent), ("Bank not locked while unlinking from bank_put_on_free_list\n"));

	if ( MALI_TRUE == mem->is_pow2)
	{
		MALI_DEBUG_ASSERT(mem->order <= bank->order_maximum, ("Order %d larger than maximum supported (%d) in this bank", mem->order, bank->order_maximum));
		_mali_embedded_list_insert_tail(&bank->free_list_exact[mem->order - bank->order_minimum], &mem->custom.link);
	}
	else
	{
		u32 order_index = MIN(mem->order - bank->order_minimum, bank->order_maximum);
		_mali_embedded_list_insert_tail(&bank->free_list_bigger[order_index], &mem->custom.link);
	}

	bank->free_list_elements++;
	bank->free_list_size += mem->size;
	MALI_DEBUG_ASSERT(bank->free_list_size == bank_calculate_free_size(bank), ("Bank free size mismatch"));
}

MALI_STATIC mali_err_code descriptor_pool_init(void)
{
	/* init mutex */
	descriptor_mutex = _mali_sys_mutex_create();
	MALI_CHECK_NON_NULL(descriptor_mutex, MALI_ERR_FUNCTION_FAILED);

	for (; descriptor_pool_count < MALI_MEMORY_DESCRIPTOR_POOL_SIZE; ++descriptor_pool_count)
	{
		mali_mem * descriptor = _mali_base_arch_mem_allocate_descriptor();
		MALI_CHECK_GOTO(descriptor != NULL, dpiexit);

		_mali_embedded_list_insert_tail(&descriptor_pool_head, &descriptor->custom.link);
	}

	MALI_SUCCESS;
dpiexit:
	descriptor_pool_term();
	MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
}

MALI_STATIC MALI_INLINE void descriptor_pool_term(void)
{
	descriptor_pool_release_all();
	_mali_sys_mutex_destroy(descriptor_mutex);
}

MALI_STATIC void descriptor_pool_release_all(void)
{
	mali_mem *descriptor;
	mali_embedded_list_link *temp;

	descriptor_pool_lock();

	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE(descriptor, temp, &descriptor_pool_head, mali_mem, custom.link)
	{
		_mali_embedded_list_remove(&descriptor->custom.link);
		_mali_base_arch_mem_free_descriptor(descriptor);
	}

	descriptor_pool_count = 0;

	descriptor_pool_unlock();
}

MALI_STATIC mali_mem * descriptor_pool_get(void)
{
	mali_mem * result;
	descriptor_pool_lock();

	if (0 == descriptor_pool_count) result = _mali_base_arch_mem_allocate_descriptor();
	else
	{
		result = MALI_EMBEDDED_LIST_GET_CONTAINER(mali_mem, custom.link, _mali_embedded_list_get_next(&descriptor_pool_head));
		_mali_embedded_list_remove(&result->custom.link);
		--descriptor_pool_count;
	}
	descriptor_pool_unlock();
	return result;
}

MALI_STATIC void descriptor_pool_release(mali_mem * descriptor)
{
	MALI_DEBUG_ASSERT_POINTER(descriptor);
	MALI_DEBUG_ASSERT( MALI_TRUE == _mali_embedded_list_is_empty(&descriptor->memlink), ("Releasing descriptor which is on a global memory list"));
	MALI_DEBUG_ASSERT( MALI_TRUE == _mali_embedded_list_is_empty(&descriptor->custom.link), ("Releasing descriptor which is on a custom memory list"));

	descriptor_pool_lock();

	if (descriptor_pool_count < MALI_MEMORY_DESCRIPTOR_POOL_SIZE)
	{
		_mali_base_arch_descriptor_clear(descriptor);
		_mali_embedded_list_insert_tail(&descriptor_pool_head, &descriptor->custom.link);
		descriptor_pool_count++;
	}
	else _mali_base_arch_mem_free_descriptor(descriptor);

	descriptor_pool_unlock();
}

MALI_STATIC MALI_INLINE void descriptor_pool_lock(void)
{
	_mali_sys_mutex_lock(descriptor_mutex);
}

MALI_STATIC MALI_INLINE void descriptor_pool_unlock(void)
{
	_mali_sys_mutex_unlock(descriptor_mutex);
}

/*
	Calculate invalidated data when size field has changed
*/
MALI_STATIC MALI_INLINE void size_changed(mali_mem * mem)
{
	mem->order = get_order(mem->size);
	update_pow2_flag(mem);
}

/*
	Resizes the blocks and updates order/pow2 flags.
	The caller must move the blocks around on any free lists or whatever.
*/
MALI_STATIC mali_err_code resize_blocks(mali_mem * A, mali_mem * B, s32 sizechange)
{
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == A->memory_subtype, ("Can not use .parent on mem_type: %d\n",A->memory_subtype));
	MALI_DEBUG_ASSERT(MALI_MEM_TYPE_NORMAL == B->memory_subtype, ("Can not use .parent on mem_type: %d\n",B->memory_subtype));
	MALI_DEBUG_ASSERT(bank_check_lock(A->relationship.parent), ("Bank not locked while resize_blocks\n"));
	if ( MALI_ERR_NO_ERROR != _mali_base_arch_mem_resize_blocks(A, B, sizechange) ) return MALI_ERR_FUNCTION_FAILED;
	size_changed(A);
	size_changed(B);
	MALI_SUCCESS;
}

MALI_STATIC MALI_INLINE void bank_lock(mali_mem_bank * bank)
{
	MALI_DEBUG_ASSERT_POINTER(bank);
	if (NULL != bank->mutex) _mali_sys_mutex_lock(bank->mutex);
}

MALI_STATIC MALI_INLINE void bank_unlock(mali_mem_bank * bank)
{
	MALI_DEBUG_ASSERT_POINTER(bank);
	if (NULL != bank->mutex) _mali_sys_mutex_unlock(bank->mutex);
}

MALI_DEBUG_CODE(
MALI_STATIC MALI_INLINE mali_bool bank_check_lock(mali_mem_bank * bank)
{
	MALI_DEBUG_ASSERT_POINTER(bank);
	return (MALI_ERR_NO_ERROR != _mali_sys_mutex_try_lock(bank->mutex)) ? MALI_TRUE : MALI_FALSE;
}
)

MALI_STATIC_INLINE mali_bool memory_type_is_normal_allocation(mali_mem * mem)
{
	MALI_DEBUG_ASSERT_POINTER(mem);
	return MALI_MEM_TYPE_NORMAL == mem->memory_subtype;
}

MALI_STATIC_INLINE mali_bool memory_type_is_external(mali_mem * mem)
{
	MALI_DEBUG_ASSERT_POINTER(mem);
	return (MALI_MEM_TYPE_EXTERNAL_MEMORY == mem->memory_subtype)
	        || (MALI_MEM_TYPE_UMP_EXTERNAL_MEMORY == mem->memory_subtype)
	        || (MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY == mem->memory_subtype);
}


#if !defined(HARDWARE_ISSUE_3251)

MALI_STATIC mali_addr heap_mem_addr_get(mali_mem_handle heap_handle, u32 offset)
{
	mali_mem * heap = (mali_mem *)heap_handle;
	heap_extended_data * heap_data;
	mali_mem * block;
	u32 offset_in_block;

	MALI_DEBUG_ASSERT(_mali_base_common_mem_is_heap(heap_handle), ("Calling a heap function on a normal memory allocation"));
	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;

	if (MALI_ERR_NO_ERROR != heap_find_block_with_offset(heap_data, offset, &block, &offset_in_block)) return MALI_REINTERPRET_CAST(mali_addr)-1;

	return _mali_mem_mali_addr_get((mali_mem_handle)block, offset_in_block);
}

MALI_STATIC mali_err_code heap_find_block_with_offset(heap_extended_data * heap, u32 offset, mali_mem ** block, u32* offset_in_block)
{
	mali_mem_handle current = heap->first;

	for (current = heap->first; MALI_NO_HANDLE != current; current = _mali_mem_list_get_next(current))
	{
		if (offset >= _mali_mem_size_get(current))
		{
			offset -= _mali_mem_size_get(current);
			continue;
		}
		*block = MALI_REINTERPRET_CAST(mali_mem*)current;
		*offset_in_block = offset;
		MALI_SUCCESS;
	}

	MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
}

u64 _mali_base_common_heap_read64(mali_mem_handle heap_handle, u32 offset)
{
	mali_mem * heap;
	mali_mem * mem = NULL;
	u32 intern_offset = 0;
	mali_err_code err;
	u64 read_data;
	heap_extended_data * heap_data;

	MALI_DEBUG_ASSERT(_mali_base_common_mem_is_heap(heap_handle), ("Calling a heap function on a normal memory allocation"));
	heap = MALI_REINTERPRET_CAST(mali_mem*)heap_handle;
	MALI_DEBUG_ASSERT( MALI_TRUE == heap->is_allocated, ("Operation on free memory block 0x%X detected", heap));

	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;


	err = heap_find_block_with_offset(heap_data , offset, &mem, &intern_offset);
	if ( MALI_ERR_NO_ERROR != err )
	{
		MALI_DEBUG_ASSERT( MALI_FALSE,("Reading an illegal address: 0x%08x", offset));
		return 0;
	}

	_mali_base_arch_mem_read(&read_data, mem, intern_offset, 8);

	return read_data;

}

void _mali_base_common_heap_write64(mali_mem_handle heap_handle, u32 offset, u64 data)
{
	mali_mem * heap;
	mali_mem * mem = NULL;
	u32 intern_offset = 0;
	mali_err_code err;
	heap_extended_data * heap_data;

	MALI_DEBUG_ASSERT(_mali_base_common_mem_is_heap(heap_handle), ("Calling a heap function on a normal memory allocation"));

	heap = MALI_REINTERPRET_CAST(mali_mem*)heap_handle;
	MALI_DEBUG_ASSERT( MALI_TRUE == heap->is_allocated, ("Operation on free memory block 0x%X detected", heap));

	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;

	err = heap_find_block_with_offset(heap_data, offset, &mem, &intern_offset);
	if ( MALI_ERR_NO_ERROR != err )
	{
		MALI_DEBUG_PRINT(1, ("Writing an illegal address: 0x%08x", offset));
		return;
	}
	_mali_base_arch_mem_write(mem, intern_offset, &data, 8);
}

void _mali_base_common_heap_intern_set_last_heap_addr(mali_mem_handle heap_handle, u32 heap_addr)
{
	mali_mem * heap;
	heap_extended_data * heap_data;

	MALI_DEBUG_ASSERT_POINTER(heap_handle);
	if (!_mali_base_common_mem_is_heap(heap_handle)) return;

	heap = MALI_REINTERPRET_CAST(mali_mem*)heap_handle;

	MALI_DEBUG_ASSERT( MALI_TRUE == heap->is_allocated, ("Operation on free memory block 0x%X detected", heap));

	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;

	heap_data->current_phys_addr = heap_addr;
}



MALI_EXPORT mali_mem_handle _mali_base_common_mem_heap_alloc(mali_base_ctx_handle ctx, u32 default_size, u32 maximum_size, u32 block_size)
{
	mali_mem * heap;
	heap_extended_data * heap_data;

	heap = descriptor_pool_get();
	if (NULL == heap) return MALI_NO_HANDLE;

	heap->cached_addr_info.mali_address = 0;
	heap->cached_addr_info.cpu_address = NULL;

	/* this is a heap */
	heap->memory_subtype = MALI_MEM_TYPE_HEAP;

	heap->relationship.heap = MALI_REINTERPRET_CAST(heap_extended_data*)_mali_sys_calloc(1, sizeof(heap_extended_data));
	if (NULL == heap->relationship.heap)
	{
		descriptor_pool_release(heap);
		return MALI_NO_HANDLE;
	}

	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;

	heap_data->first = _mali_mem_alloc(ctx, default_size, 1024, MALI_GP_WRITE | MALI_PP_READ);

	if (MALI_NO_HANDLE == heap_data->first)
	{
		_mali_sys_free(heap->relationship.heap);
		descriptor_pool_release(heap);
		return MALI_NO_HANDLE;
	}

	_mali_sys_atomic_initialize(&heap->cpu_map_ref_count, 1);
	_mali_sys_atomic_initialize(&heap->ref_count, 1);

	heap->is_allocated = MALI_TRUE;
	heap->size = ((mali_mem *)heap_data->first)->size;
	heap->alignment = 1024;
	heap->effective_rights = MALI_GP_WRITE | MALI_PP_READ;
	heap_data->last = heap_data->first;
	heap_data->block_size = block_size;
	heap_data->maximum_size = maximum_size;
	heap_data->current_phys_addr = (MALI_REINTERPRET_CAST(mali_mem*)heap_data->first)->mali_addr;

	return (mali_mem_handle)heap;
}

u32 _mali_base_common_mem_heap_get_start_address(mali_mem_handle heap_handle)
{
	mali_mem * heap = MALI_REINTERPRET_CAST(mali_mem *)heap_handle;
	heap_extended_data * heap_data;
	MALI_DEBUG_ASSERT_POINTER(heap);
	if (_mali_base_common_mem_is_heap(heap_handle))
	{
		MALI_DEBUG_ASSERT( MALI_TRUE == heap->is_allocated, ("Operation on free memory block 0x%X detected", heap));
		heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;
		return _mali_mem_mali_addr_get(heap_data->first, 0);
	}
	else
	{
		return _mali_mem_mali_addr_get(heap_handle, 0);
	}
}


u32 _mali_base_common_mem_heap_get_end_address_of_first_block(mali_mem_handle heap_handle)
{
	mali_mem * heap = MALI_REINTERPRET_CAST(mali_mem *)heap_handle;
	heap_extended_data * heap_data;
	MALI_DEBUG_ASSERT_POINTER(heap);
	if(_mali_base_common_mem_is_heap(heap_handle))
	{
		MALI_DEBUG_ASSERT( MALI_TRUE == heap->is_allocated, ("Operation on free memory block 0x%X detected", heap));
		heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;
		return _mali_mem_mali_addr_get(heap_data->first, _mali_mem_size_get(heap_data->first));
	}
	else
	{
		return _mali_mem_mali_addr_get(heap_handle, _mali_mem_size_get(heap_handle));
	}
}

u32 _mali_base_common_mem_heap_get_end_address(mali_mem_handle heap_handle)
{
	mali_mem * heap = MALI_REINTERPRET_CAST(mali_mem *)heap_handle;
	heap_extended_data * heap_data;
	MALI_DEBUG_ASSERT_POINTER(heap);
	if(_mali_base_common_mem_is_heap(heap_handle))
	{
		MALI_DEBUG_ASSERT( MALI_TRUE == heap->is_allocated, ("Operation on free memory block 0x%X detected", heap));
		heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;
		return _mali_mem_mali_addr_get(heap_data->last, _mali_mem_size_get(heap_data->last));
	}
	else
	{
		return _mali_mem_mali_addr_get(heap_handle, _mali_mem_size_get(heap_handle));
	}
}

u32 _mali_base_common_mem_heap_get_blocksize(mali_mem_handle heap_handle)
{
	mali_mem * heap = MALI_REINTERPRET_CAST(mali_mem *)heap_handle;
	heap_extended_data * heap_data;
	MALI_DEBUG_ASSERT_POINTER(heap);
	MALI_DEBUG_ASSERT(_mali_base_common_mem_is_heap(heap_handle), ("Calling a heap function on a normal memory allocation"));
	MALI_DEBUG_ASSERT( MALI_TRUE == heap->is_allocated, ("Operation on free memory block 0x%X detected", heap));
	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;

	return heap_data->block_size;
}

u32 _mali_base_common_mem_heap_get_max_size(mali_mem_handle heap_handle)
{
	mali_mem * heap = MALI_REINTERPRET_CAST(mali_mem *)heap_handle;
	heap_extended_data * heap_data;
	MALI_DEBUG_ASSERT_POINTER(heap);
	MALI_DEBUG_ASSERT(_mali_base_common_mem_is_heap(heap_handle), ("Calling a heap function on a normal memory allocation"));
	MALI_DEBUG_ASSERT( MALI_TRUE == heap->is_allocated, ("Operation on free memory block 0x%X detected", heap));
	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;

	return heap_data->maximum_size;
}

void _mali_base_common_mem_heap_reset(mali_mem_handle heap_handle)
{
	mali_mem * heap = MALI_REINTERPRET_CAST(mali_mem *)heap_handle;
	mali_mem_handle list_to_free;
	heap_extended_data * heap_data;

	MALI_DEBUG_ASSERT_POINTER(heap);
	MALI_DEBUG_ASSERT(_mali_base_common_mem_is_heap(heap_handle), ("Calling a heap function on a normal memory allocation"));
	MALI_DEBUG_ASSERT( MALI_TRUE == heap->is_allocated, ("Operation on free memory block 0x%X detected", heap));
	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;

	list_to_free = _mali_mem_list_remove_item(heap_data->first);
	_mali_mem_list_free(list_to_free);

	heap->size = _mali_mem_size_get(heap_data->first);
	heap_data->last = heap_data->first;
	heap_data->current_phys_addr = (MALI_REINTERPRET_CAST(mali_mem*)heap_data->first)->mali_addr;
}

mali_bool _mali_base_common_mem_is_heap(mali_mem_handle mem_handle)
{
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem *)mem_handle;
	MALI_DEBUG_ASSERT_HANDLE(mem_handle);
	MALI_DEBUG_ASSERT( mem->is_allocated, ("Bad memory handle queried for is-heap status"));
	return mem->is_allocated && (MALI_MEM_TYPE_HEAP == mem->memory_subtype);
}

MALI_STATIC u32 heap_alloc_size_get(mali_mem * heap)
{
	MALI_DEBUG_ASSERT_POINTER(heap);
	return heap->size;
}

u32 _mali_base_common_mem_heap_used_bytes_get(mali_mem_handle heap_handle)
{
	u32 first_addr;
	u32 last_addr;
	u32 current_addr;
	mali_mem * heap;
	mali_mem *last;
	heap_extended_data * heap_data;
	MALI_DEBUG_ASSERT_HANDLE(heap_handle);
	heap = MALI_REINTERPRET_CAST(mali_mem*)heap_handle;
	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;
	last = MALI_REINTERPRET_CAST(mali_mem*)heap_data->last;
	if (NULL == last ) return 0;
	first_addr = last->mali_addr;
	last_addr  = first_addr + last->size;
	current_addr = heap_data->current_phys_addr;

	if ((current_addr >= first_addr) && (current_addr <= last_addr))
	{
		return heap->size - (last_addr-current_addr);
	}
	else
	{
		return heap->size;
	}
}

mali_err_code _mali_base_common_mem_heap_out_of_memory(mali_mem_handle mem, u32 * new_heap_start, u32 * new_heap_end)
{
	mali_mem_handle new_memory;
	mali_mem * heap = MALI_REINTERPRET_CAST(mali_mem *)mem;
	heap_extended_data * heap_data;

	MALI_CHECK(_mali_base_common_mem_is_heap(mem), MALI_ERR_OUT_OF_MEMORY); /* can't extend normal memory handles */
	MALI_CHECK_NON_NULL(new_heap_start, MALI_ERR_FUNCTION_FAILED);
	MALI_CHECK_NON_NULL(new_heap_end, MALI_ERR_FUNCTION_FAILED);
	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;

	/* check if this heap can be extended */
	MALI_CHECK(heap->size + heap_data->block_size <= heap_data->maximum_size,  MALI_ERR_OUT_OF_MEMORY); /* maximum allocation reached */

	/* try to allocate more memory */
	new_memory = _mali_mem_alloc(heap_data->ctx, heap_data->block_size, 1024, MALI_GP_WRITE | MALI_PP_READ);
	MALI_CHECK_NON_NULL(new_memory, MALI_ERR_OUT_OF_MEMORY);

	/* got more memory, update stats */
	heap->size += ((mali_mem *)new_memory)->size;

	/* link in */
	_mali_mem_list_insert_after(heap_data->last, new_memory);
	/* update last field which tracks the last block added */
	heap_data->last = new_memory;

	/* get mali addresses */
	*new_heap_start = _mali_mem_mali_addr_get(new_memory, 0);
	heap_data->current_phys_addr = *new_heap_start;
	*new_heap_end = _mali_mem_mali_addr_get(new_memory, _mali_mem_size_get(new_memory));

	MALI_DEBUG_PRINT(5, ("Extended heap: new heap [0x%X - 0x%X], size %d\n", *new_heap_start, *new_heap_end, heap->size));

	MALI_SUCCESS;
}

mali_err_code _mali_base_common_mem_heap_resize( mali_base_ctx_handle ctx, mali_mem_handle mem, u32 new_size)
{
	mali_mem * heap = MALI_REINTERPRET_CAST(mali_mem *)mem;
	heap_extended_data * heap_data;

	MALI_CHECK(_mali_base_common_mem_is_heap(mem), MALI_ERR_OUT_OF_MEMORY);
	heap_data = MALI_REINTERPRET_CAST(heap_extended_data *)heap->relationship.heap;

	MALI_DEBUG_ASSERT(new_size <= heap_data->maximum_size, ("Requested size is larger than maximum size"));
	MALI_DEBUG_ASSERT(new_size, ("Requested size is 0"));

	_mali_mem_list_free(heap_data->first);

	heap_data->first = _mali_mem_alloc(ctx, new_size, 1024, MALI_GP_WRITE | MALI_PP_READ);
	MALI_CHECK_NON_NULL(heap_data->first, MALI_ERR_OUT_OF_MEMORY);

	_mali_sys_atomic_initialize(&heap->cpu_map_ref_count, 1);
	_mali_sys_atomic_initialize(&heap->ref_count, 1);

	heap->is_allocated = MALI_TRUE;
	heap->size = ((mali_mem *)heap_data->first)->size;
	heap->alignment = 1024;
	heap->effective_rights = MALI_GP_WRITE | MALI_PP_READ;
	heap_data->last = heap_data->first;
	heap_data->current_phys_addr = (MALI_REINTERPRET_CAST(mali_mem*)heap_data->first)->mali_addr;

	MALI_DEBUG_PRINT(5, ("Resized heap. New size: %d\n", heap->size));

	MALI_SUCCESS;
}

#endif

mali_mem_handle _mali_base_common_mem_add_phys_mem(mali_base_ctx_handle ctx, u32 phys_addr, u32 size, void * mapping, u32 access_rights)
{
	mali_mem * mem = descriptor_pool_get();
	MALI_CHECK_NON_NULL(mem, MALI_NO_HANDLE);

	mem->memory_subtype = MALI_MEM_TYPE_EXTERNAL_MEMORY;
	mem->size = size;
	mem->is_allocated = MALI_TRUE;
	mem->relationship.ctx = ctx;
	mem->effective_rights = access_rights;
	size_changed(mem);

	if (MALI_ERR_NO_ERROR != _mali_base_arch_mem_add_phys_mem(mem, phys_addr, size, mapping, access_rights))
	{
		/* backend denied the phys memory usage, abort */
		descriptor_pool_release(mem);
		return MALI_NO_HANDLE;
	}

	mem->cached_addr_info.mali_address = mem->mali_addr;
	mem->cached_addr_info.cpu_address = mapping;

	_mali_sys_atomic_initialize(&mem->cpu_map_ref_count, 1);
	_mali_sys_atomic_initialize(&mem->ref_count, 1);

	return (mali_mem_handle)mem;
}

#if MALI_USE_DMA_BUF
mali_mem_handle _mali_base_common_mem_wrap_dma_buf(mali_base_ctx_handle ctx, int mem_fd, u32 offset)
{
	u32 size = 0;
	mali_mem * mem = NULL;

	MALI_IGNORE(ctx);

	if(0 > mem_fd)
	{
		return MALI_NO_HANDLE;
	}

	if (MALI_ERR_NO_ERROR != _mali_base_arch_mem_dma_buf_get_size(&size, mem_fd))
	{
		return MALI_NO_HANDLE;
	}

	if (0 == size || offset >= size)
	{
		return MALI_NO_HANDLE;
	}

	mem = descriptor_pool_get();
	if (NULL == mem)
	{
		return MALI_NO_HANDLE;
	}

	mem->memory_subtype = MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY;
	mem->is_allocated = MALI_TRUE;
	mem->relationship.dma_buf_fd = dup(mem_fd);
	mem->effective_rights = (1<<7)-1; /* all rights */
	mem->size = size;
	_mali_sys_atomic_initialize(&mem->ref_count, 1);

	/* Memory is mapped on demaind. Start map reference count at 0. */
	_mali_sys_atomic_initialize(&mem->cpu_map_ref_count, 0);

	if (MALI_ERR_NO_ERROR != _mali_base_arch_mem_dma_buf_attach(mem, mem->relationship.dma_buf_fd))
	{
		/* backend denied the phys memory usage, abort */
		close(mem->relationship.dma_buf_fd);
		descriptor_pool_release(mem);
		return MALI_NO_HANDLE;
	}

	mem->mali_addr += offset;

	mem->cached_addr_info.mali_address = mem->mali_addr;

	return MALI_REINTERPRET_CAST(mali_mem_handle)mem;
}

int _mali_base_common_mem_get_dma_buf_descriptor(mali_mem_handle mem_handle)
{
	if (MALI_NO_HANDLE != mem_handle)
	{
		mali_mem *mem = MALI_REINTERPRET_CAST(mali_mem *)mem_handle;
		if (MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY == mem->memory_subtype)
		{
			return mem->relationship.dma_buf_fd;
		}
	}

	return -1;
}
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
mali_mem_handle _mali_base_common_mem_wrap_ump_memory(mali_base_ctx_handle ctx, ump_handle ump_mem, u32 offset, u32 access_rights)
{
	u32 size=0;
	u32 ump_secure_id_u32;
	mali_mem * mem = NULL;

	MALI_IGNORE(ctx);
	if(UMP_INVALID_MEMORY_HANDLE==ump_mem) return MALI_NO_HANDLE;

	size = ump_size_get( ump_mem);
	if (0 == size)    return MALI_NO_HANDLE;
	if (size<= offset) return MALI_NO_HANDLE;

	mem = descriptor_pool_get();
	MALI_CHECK_NON_NULL(mem, MALI_NO_HANDLE);

	mem->memory_subtype = MALI_MEM_TYPE_UMP_EXTERNAL_MEMORY;
	mem->is_allocated = MALI_TRUE;
	mem->relationship.ump_mem = ump_mem;
	mem->effective_rights = access_rights;
	mem->size = size;

	_mali_sys_atomic_initialize(&mem->ref_count, 1);
	_mali_sys_atomic_initialize(&mem->cpu_map_ref_count, 1);

	ump_secure_id_u32 = (u32)ump_secure_id_get(ump_mem);
	if (MALI_ERR_NO_ERROR != _mali_base_arch_mem_ump_mem_attach(mem, ump_secure_id_u32, offset))
	{
		/* backend denied the phys memory usage, abort */
		descriptor_pool_release(mem);
		return MALI_NO_HANDLE;
	}
	mem->mali_addr += offset;
	mem->size = size-offset;

	ump_reference_add( ump_mem );

	mem->cached_addr_info.mali_address = mem->mali_addr;
	_mali_base_arch_mem_map(mem, 0, mem->size, (MALI_MEM_PTR_READABLE | MALI_MEM_PTR_WRITABLE), &mem->cached_addr_info.cpu_address);

	return MALI_REINTERPRET_CAST(mali_mem_handle)mem;
}

ump_handle _mali_base_common_mem_get_ump_memory(mali_mem_handle mem_handle)
{
	mali_mem *mem = NULL;

	if ( MALI_NO_HANDLE == mem_handle ) return UMP_INVALID_MEMORY_HANDLE;

	mem = MALI_REINTERPRET_CAST(mali_mem *)mem_handle;

	if ( MALI_MEM_TYPE_UMP_EXTERNAL_MEMORY == mem->memory_subtype ) return mem->relationship.ump_mem;

	return UMP_INVALID_MEMORY_HANDLE;
}
#endif

MALI_DEBUG_CODE(
void _mali_base_common_mem_debug_print_all(mali_base_ctx_handle ctx)
{
	mali_mem_bank * bank;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	MALI_PRINTF("- Memory usage dump -\n");
	MALI_PRINTF("-- Legend: --\n");
	MALI_PRINTF("-- \tA - allocated --\n");
	MALI_PRINTF("-- \tF - free --\n");
	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(bank, &memory_banks, mali_mem_bank, link)
	{
		dump_bank_memory_usage_map(bank);
	}
	MALI_PRINTF("- End of memory usage dump -\n");
}

MALI_STATIC void dump_bank_memory_usage_map(mali_mem_bank* bank)
{
	mali_mem * mem;
	MALI_PRINTF("-- Bank %08x --\n", bank);
	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(mem, &bank->all_memory, mali_mem, memlink)
	{
		MALI_PRINTF("\t%c[%d]", MALI_TRUE == mem->is_allocated ? 'A' : 'F', mem->size);
	}

	MALI_PRINTF("-- Bank end --\n");
}
)

#ifdef MALI_DUMP_ENABLE

MALI_STATIC void  _mali_base_common_mem_dump_mem_handle(mali_mem_handle memh, mali_file *file, u32 mem_print_format, const char *delimiter)
{
	mali_mem * mem = (mali_mem *)memh;
	u32 size, addr, i;
	u32 j = 0, k = 0;
	const char * txt[] = {"\b|","\b/","\b-", "\b\\"};

	size = mem->size;
	addr = mem->mali_addr;
	if ( 0==(mem_print_format&MALI_MEM_PRINT_ONE_LINERS) )
	{
		if ( MALI_TRUE == mem->is_allocated) _mali_sys_fprintf(file, "# MEM BLOCK - ALLOCATED\n");
		else _mali_sys_fprintf(file, "# MEM BLOCK - FREE\n");
		_mali_sys_fprintf(file, "# Addr: 0x%08x\n", addr );
		_mali_sys_fprintf(file, "# Size:   %8d\n", size );
		#ifdef MALI_TEST_API
		if ( MALI_TRUE == mem->is_allocated) _mali_sys_fprintf(file, "# RSize:  %8d\n", mem->size_used_to_alloc_call);
		#endif
		#ifdef MALI_MEMORY_PROFILING
			if (NULL != mem->function_name) {
				_mali_sys_fprintf(file, "# Function: %s\n",mem->function_name);
				_mali_sys_fprintf(file, "# File:     %s  %d\n", mem->file_name, mem->line_nr );
			} else {
				_mali_sys_fprintf(file, "# No function, file or line number information\n");
			}
			_mali_sys_fprintf(file, "# Alloc_nr: %d Alloc_time: %lld\n", mem->alloc_nr, mem->alloc_time);
		#endif
		switch (mem->memory_subtype)
		{
		case MALI_MEM_TYPE_NORMAL:
			break;
		case MALI_MEM_TYPE_HEAP:
			_mali_sys_fprintf(file, "# Is Heap memory ");
			break;
		case MALI_MEM_TYPE_EXTERNAL_MEMORY:
			_mali_sys_fprintf(file, "# Is external memory ");
			break;
		case MALI_MEM_TYPE_UMP_EXTERNAL_MEMORY:
			_mali_sys_fprintf(file, "# Is ump memory ");
			break;
		case MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY:
			_mali_sys_fprintf(file, "# Is dma-buf memory ");
			break;
		case MALI_MEM_TYPE_INVALID:
		default:
			_mali_sys_fprintf(file, "# Invalid memory type");
			break;
		}

		if ( MALI_FALSE == mem->is_allocated) return;

		if ( mem_print_format&MALI_MEM_PRINT_VERBOSE ) _mali_sys_printf(" ");

		for ( i=0 ; i<size; i+= 16)
		{
			 u32 read_buffer[4];
			 if ((mem_print_format&MALI_MEM_PRINT_VERBOSE)&& (!(k++%4096)) ) _mali_sys_printf("%s", txt[j++%4]);
			_mali_base_arch_mem_read(read_buffer, mem, i, 16);
			if (dump_only_written_memory )
			{
				u32 clear_val = clear_memory_value | clear_memory_value<<8 | clear_memory_value <<16 | clear_memory_value<<24;
				if (
						read_buffer[0] == clear_val &&
						read_buffer[1] == clear_val &&
						read_buffer[2] == clear_val &&
						read_buffer[3] == clear_val
				   ) continue;
			}
			_mali_sys_fprintf(
				file,
				"%08X: %08X %08X %08X %08X\n",
				(addr+i),
				read_buffer[0],
				read_buffer[1],
				read_buffer[2],
				read_buffer[3]
			);
		}
		if ( mem_print_format&MALI_MEM_PRINT_VERBOSE )_mali_sys_printf("\b");

	} /*if ( 0==(mem_print_format&MALI_MEM_PRINT_ONE_LINERS) )*/
	else /* Output should only be one-liners */
	{
		switch (mem->memory_subtype)
		{
			case MALI_MEM_TYPE_NORMAL:
				_mali_sys_fprintf(file, "Normal");
				break;
			case MALI_MEM_TYPE_HEAP:
				_mali_sys_fprintf(file, "HeapGP");
				break;
			case MALI_MEM_TYPE_EXTERNAL_MEMORY:
				_mali_sys_fprintf(file, "Extern");
				break;
			case MALI_MEM_TYPE_UMP_EXTERNAL_MEMORY:
				_mali_sys_fprintf(file, "UMPmem");
				break;
			case MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY:
				_mali_sys_fprintf(file, "dma-buf");
				break;
			case MALI_MEM_TYPE_INVALID:
			default:
				_mali_sys_fprintf(file, "NotSet");
				break;
		}

		_mali_sys_fprintf(file, delimiter);

		if ( MALI_TRUE == mem->is_allocated)
		{
			_mali_sys_fprintf(file, "alloced");
		}
		else
		{
			_mali_sys_fprintf(file, "freemem");
		}
		_mali_sys_fprintf(file, delimiter);

		_mali_sys_fprintf(file, "0x%08x", addr );
		_mali_sys_fprintf(file, delimiter);
		_mali_sys_fprintf(file, "%08d", size );
		_mali_sys_fprintf(file, delimiter);
		#ifdef MALI_TEST_API
		_mali_sys_fprintf(file, "%08d", mem->size_used_to_alloc_call);
		_mali_sys_fprintf(file, delimiter);
		#endif
		#ifdef MALI_MEMORY_PROFILING
			_mali_sys_fprintf(file, "%07d", mem->alloc_nr);
			_mali_sys_fprintf(file, delimiter);
			_mali_sys_fprintf(file, "%010lld", mem->alloc_time);
			_mali_sys_fprintf(file, delimiter);
			_mali_sys_fprintf(file, "%04d", mem->line_nr );
			_mali_sys_fprintf(file, delimiter);
			_mali_sys_fprintf(file, "%s",mem->function_name);
			_mali_sys_fprintf(file, delimiter);
			_mali_sys_fprintf(file, "%s", mem->file_name);
		#endif
		_mali_sys_fprintf(file, "\n");

	}
}


MALI_STATIC void dump_memory_bank(mali_file * file, mali_mem_bank * bank, u32 mem_print_format, const char * delimiter)
{
	mali_mem * mem;
	if ( 0==(mem_print_format&MALI_MEM_PRINT_ONE_LINERS) )
	{
		_mali_sys_fprintf(file, "# Bank size total: %u\n", bank->size_total );
		_mali_sys_fprintf(file, "# Bank size alloc: %u\n", bank->size_allocated );
		_mali_sys_fprintf(file, "# Bank used elements: %u\n", bank->allocated_elements );
		_mali_sys_fprintf(file, "# Bank free elements: %u\n", bank->free_list_elements );
		_mali_sys_fprintf(file, "#\n");
	}

	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(mem, &bank->all_memory, mali_mem, memlink)
	{
		_mali_base_common_mem_dump_mem_handle((mali_mem_handle)mem, file, mem_print_format, delimiter);
		if ( 0==(mem_print_format&MALI_MEM_PRINT_ONE_LINERS) ) _mali_sys_fprintf(file, "#\n");
	}
}



void _mali_base_common_mem_dump_all_mem(mali_base_ctx_handle ctx, mali_file *file, u32 mem_print_format, const char * delimiter)
{
	mali_mem_bank * bank;
	u32 bank_id        = 0;
	u32 mem_size_total = 0;
	u32 mem_size_used  = 0;
	u32 mem_nr_used    = 0;
	u32 mem_nr_free    = 0;

	MALI_IGNORE(ctx);

	if ( 0==(mem_print_format&MALI_MEM_PRINT_ONE_LINERS) )
	{
		_mali_sys_fprintf(file, "# --- DUMP ALL START ---\n");
		_mali_sys_fprintf(file, "#\n");
	}
	else
	{
		_mali_sys_fprintf(file, "MemTyp");
		_mali_sys_fprintf(file, delimiter);
		_mali_sys_fprintf(file, "MStatus");
		_mali_sys_fprintf(file, delimiter);

		_mali_sys_fprintf(file, "MaliAddres");
		_mali_sys_fprintf(file, delimiter);
		_mali_sys_fprintf(file, "SizeUsed");
		_mali_sys_fprintf(file, delimiter);
	#ifdef MALI_TEST_API
		_mali_sys_fprintf(file, "SizeReqd");
		_mali_sys_fprintf(file, delimiter);
	#endif
	#ifdef MALI_MEMORY_PROFILING
		_mali_sys_fprintf(file, "AllocNr");
		_mali_sys_fprintf(file, delimiter);
		_mali_sys_fprintf(file, "SystemTime");
		_mali_sys_fprintf(file, delimiter);
		_mali_sys_fprintf(file, "Line");
		_mali_sys_fprintf(file, delimiter);
		_mali_sys_fprintf(file, "FunctionName");
		_mali_sys_fprintf(file, delimiter);
		_mali_sys_fprintf(file, "FileName");
	#endif
		_mali_sys_fprintf(file, "\n");

	}/* if ( mem_print_format&MALI_MEM_PRINT_ONE_LINERS)*/

	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(bank, &memory_banks, mali_mem_bank, link)
	{
		bank_lock(bank);
		if ( 0==(mem_print_format&MALI_MEM_PRINT_ONE_LINERS) )
		{
			_mali_sys_fprintf(file, "# DUMP BANK NR %d\n", ++bank_id);
		}
		dump_memory_bank(file, bank, mem_print_format, delimiter);
		mem_size_total += bank->size_total;
		mem_size_used  += bank->size_allocated;
		mem_nr_used    += bank->allocated_elements;
		mem_nr_free    += bank->free_list_elements;
		bank_unlock(bank);
	}

	if ( 0==(mem_print_format&MALI_MEM_PRINT_ONE_LINERS) )
	{
		_mali_sys_fprintf(file, "#\n");
		_mali_sys_fprintf(file, "# Memory size total: %9d\n", mem_size_total );
		_mali_sys_fprintf(file, "# Memory size alloc: %9d\n", mem_size_used );
		_mali_sys_fprintf(file, "# Memory used elements:%7d\n", mem_nr_used );
		_mali_sys_fprintf(file, "# Memory free elements:%7d\n", mem_nr_free );
		_mali_sys_fprintf(file, "#\n");
		_mali_sys_fprintf(file, "# --- DUMP ALL END ---\n");
	}
}

#endif /* #ifdef MALI_DUMP_ENABLE */

#ifdef MALI_TEST_API
MALI_STATIC mali_err_code usage_tracking_init(void)
{
	allocation_stats_mutex = _mali_sys_mutex_create();
	/* NOTE: Do not use the MALI_CHECK_NON_NULL macro below as it interferes with
	 * the MALI_CHECK_NON_NULL macro failures
	 */
	if( allocation_stats_mutex == NULL )
	{
		return MALI_ERR_FUNCTION_FAILED;
	}

	MALI_DEBUG_ASSERT(0 == current_bytes_allocated, ("Error in the memory usage tracking code detected: out of sync on allocation tracking"));
	MALI_DEBUG_ASSERT(0 == current_bytes_requested, ("Error in the memory usage tracking code detected: out of sync on request tracking"));

	/* reset max to 0. The current values will always be 0 by the time we're called */
	max_bytes_allocated = 0;
	max_bytes_requested = 0;

	MALI_SUCCESS;
}

MALI_STATIC void usage_tracking_term(void)
{
	_mali_sys_mutex_destroy(allocation_stats_mutex);
	allocation_stats_mutex = MALI_NO_HANDLE;
	if (MALI_ERR_NO_ERROR != _mali_test_open()) return; /* fail to init core */
	if ( MALI_TRUE == _mali_test_is_enabled(MALI_TEST_OPTION_MALIMEM_REPORT_PEAK))
	{
		_mali_sys_printf("MALI_MEM_STATS: Peak Mali requested %d bytes\n", max_bytes_requested);
		_mali_sys_printf("MALI_MEM_STATS: Peak Mali allocated %d bytes\n", max_bytes_allocated);
	}
	_mali_test_close();
}


MALI_STATIC void usage_tracking_update(s32 size_change_requested, s32 size_change_allocated)
{
	_mali_sys_mutex_lock(allocation_stats_mutex);

	current_bytes_requested += size_change_requested;
	max_bytes_requested = MAX(max_bytes_requested, current_bytes_requested);

	current_bytes_allocated += size_change_allocated;
	max_bytes_allocated = MAX(max_bytes_allocated, current_bytes_allocated);

	_mali_sys_mutex_unlock(allocation_stats_mutex);
}

void _mali_base_common_mem_stats_get(mali_mem_stats * stats)
{
	MALI_DEBUG_ASSERT_POINTER(stats);
	_mali_sys_mutex_lock(allocation_stats_mutex);

	stats->current_bytes_requested = current_bytes_requested;
	stats->max_bytes_requested = max_bytes_requested;
	stats->current_bytes_allocated = current_bytes_allocated;
	stats->max_bytes_allocated = max_bytes_allocated;

	_mali_sys_mutex_unlock(allocation_stats_mutex);
}
#endif /* MALI_TEST_API */

#ifdef MALI_MEMORY_PROFILING
/* When the MALI_MEMORY_PROFILING define is enabled, all functions that allocate
		mali memory are routed through macros that store additional information
		about where the	allocation was made in the data to the returned memory_handle.
		The macros use this function to store this profiling information. */
MALI_EXPORT mali_mem_handle _mali_base_common_mem_set_profiling(mali_mem_handle mem_handle,
					const char * function_name,
					const char * file_name,
					u32 line_nr)
{

	static volatile u64 first_time = 0;

	u64 current_time;
	mali_mem * mem = MALI_REINTERPRET_CAST(mali_mem *)mem_handle;

	if ( MALI_NO_HANDLE == mem_handle) return mem_handle;

	current_time = _mali_sys_get_time_usec();

	mem->function_name = function_name,
	mem->file_name = file_name;
	mem->line_nr = line_nr;
	mem->alloc_nr = _mali_sys_atomic_inc_and_return(&local_static_alloc_nr);

	if (1 == mem->alloc_nr)
	{
		first_time = current_time;
		current_time = 0; /* 0 is the relative time for the first allocation */
	}
	else
	{
		while (0 == first_time)
		{
			/* Busy loop if we are not the first function call,
			   but the first_time is still 0 which indicates we have caught a race condition.
			   Loop until the other thread performs the write
			 */
			_mali_sys_yield(); /* yield the CPU a bit */
		}

		current_time -= first_time; /* covert to relative time */
	}

	mem->alloc_time = current_time;

	return mem_handle;
}
#endif /* #ifdef MALI_MEMORY_PROFILING*/
