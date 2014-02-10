/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_memory.h
 * File contains functions controlling memory used by the mali cores.
 * All interaction with Mali memory through a handle mali_mem_handle.
 * mali_mem_handle can be seen as an object, which users do not know the internals of.
 *
 * Typical usage can be:
 * 		Open a reference to the memory system using @see _mali_mem_open
 * 		Allocate mali memory, using @see _mali_mem_alloc
 * 		Use functions @see _mali_mem_read and @see _mali_mem_write to read and write from mali memory.
 * 		When done, free mali_memory with @see _mali_mem_free
 * 		Run @see _mali_mem_close to remove the reference created with _mali_mem_open
 */

#ifndef _MALI_MEMORY_H_
#define _MALI_MEMORY_H_

#include <base/mali_types.h>
#include <base/mali_memory_types.h>
#include <base/mali_macros.h>
#include <base/mali_context.h>
#include <arch/base_arch_mem_inline.h>
#include <base/common/mem/base_common_mem.h>
#include <base/arch/base_arch_mem.h>
#include <mali_system.h>

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
#include <ump/ump.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* When memory profiling is enabled these functions are routed
* through macros to get file, lineno and function information.
* The macros must be undefined in this file, since the macro
* will use this original version
*/
#ifdef _mali_mem_alloc
#undef _mali_mem_alloc
#endif /*_mali_mem_alloc */

#ifdef _mali_mem_heap_alloc
#undef _mali_mem_heap_alloc
#endif /*_mali_mem_heap_alloc */

/**
 * Get a new mali_base_ctx_handle object.
 * @param ctx The base context to scope usage to
 * @return A standard Mali error code
 */
MALI_CHECK_RESULT MALI_STATIC_FORCE_INLINE mali_err_code _mali_mem_open(mali_base_ctx_handle ctx)
{
        MALI_IGNORE(ctx);
        MALI_SUCCESS;
}
/**
 * De-init of memory system
 * @param ctx The base context used in the open call
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_close(mali_base_ctx_handle ctx)
{
        MALI_IGNORE(ctx);
        return;
}

/**
 * Allocation of mali_mem
 * @param ctx The Base context to bind the allocation to
 * @param size Size in bytes to allocate
 * @param pow2_alignment Minimum alignment
 * @param mali_access Access/usage for the memory needed/to limit to
 * @return A valid Mali memory handle or MALI_NO_HANDLE if unable to satisfy request
 */
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_alloc(
		mali_base_ctx_handle ctx,
		u32 size,
		u32 pow2_alignment,
		u32 mali_access)
{
        return _mali_base_common_mem_alloc(ctx, size, pow2_alignment, mali_access);
}

/**
 * Freeing of mali_mem
 * A noop if mem has the value MALI_NO_HANDLE
 * @param mem Handle to the memory to free
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_free(mali_mem_handle mem)
{
        _mali_base_common_mem_free(mem);
}

/**
 * Add reference to mali memory
 *
 * Freshly allocated Mali memory has a reference count of 1.
 *
 * @param memh Handle to the memory to reference
 */
MALI_STATIC_INLINE void _mali_mem_addref(mali_mem_handle memh)
{
	MALI_DEBUG_ASSERT_POINTER(memh);
	_mali_sys_atomic_inc(&((mali_mem*)memh)->ref_count);
}

/**
 * Get current reference count of memory
 *
 * @param memh Handle to the memory
 */
MALI_STATIC_INLINE int _mali_mem_get_ref_count(mali_mem_handle memh)
{
	MALI_DEBUG_ASSERT_POINTER(memh);
	return _mali_sys_atomic_get(&((mali_mem*)memh)->ref_count);
}

/**
 * Release reference to Mali memory
 *
 * Will free memory if reference count reaches 0.
 *
 * @param memh Handle to memory
 */
MALI_STATIC_INLINE void _mali_mem_deref(mali_mem_handle memh)
{
	u32 ref_count = 0;
	mali_mem *mem = (mali_mem*)memh;

	MALI_DEBUG_ASSERT_POINTER(mem);
	MALI_DEBUG_ASSERT(0 < _mali_sys_atomic_get(&mem->ref_count), ("inconsistent ref count"));

	ref_count = _mali_sys_atomic_dec_and_return(&mem->ref_count);

	/* check if this was the last reference */
	if (0 == ref_count)
	{
		/* free mali mem */
		_mali_mem_free(memh);
	}
}

/**
 * Get the total allocated size for all banks
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_get_total_allocated_size(void)
{
        return _mali_base_common_mem_get_total_allocated_size();
}

/**
 * Indicate to the memory sub system that a new period has started.
 *
 * This is used by the memory system to decide how much memory to hold back by
 * looking at memory usage in previous periods.
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_new_period(void)
{
        _mali_base_common_mem_new_period();
}

/**
 * Use this function to release unused memory buffers which exists in a Base context
 * @param ctx Base context to operate on
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_free_unused_mem(mali_base_ctx_handle ctx)
{
        _mali_base_common_mem_free_unused_mem(ctx);
}

MALI_STATIC_FORCE_INLINE mali_addr _mali_mem_mali_addr_get_full(mali_mem_handle mem, u32 offset)
{
        return _mali_base_common_mem_addr_get_full(mem, offset);
}

/**
 * Get the address as seen by mali for mem + offset
 * @param mem The memory handle to return the memory address of
 * @param offset Relative offset inside the buffer
 * @return A Mali memory address
 */
MALI_STATIC_FORCE_INLINE mali_addr _mali_mem_mali_addr_get(mali_mem_handle mem, u32 offset)
{
	if (!mem->mali_address) return  _mali_mem_mali_addr_get_full(mem, offset);
	else return mem->mali_address + offset;
}

/****** Get more information about current block ******/

/**
 * Get size of memory block.
 * @note This can return a higher value than what was requested through _mali_mem_alloc
 * @note If memory was allocated as a heap, it returns how much of the heap that has been used.
 * @param mem The block to operate on
 * @return The size in bytes of the memory block
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_size_get(mali_mem_handle mem)
{
        return _mali_base_common_mem_size_get(mem);
}

/**
 * Get order of memory block.
 * @note Order is the Log2 of the size
 * @note You can not read out order of a heap. The return value will be 0 for heaps.
 * @param mem The block to operate on
 * @return The log2 of the size in bytes of the memory block
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_order_get(mali_mem_handle mem)
{
        return _mali_base_common_mem_order_get(mem);
}

/**
 * Get alignment of current memory block
 * Get the alignment used during the _mali_mem_alloc request for the block.
 * @note The actual alignment could be better than returned
 * @param mem The block to operate on
 * @return The alignment used during _mali_mem_alloc
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_alignment_get(mali_mem_handle mem)
{
        return _mali_base_common_mem_alignment_get(mem);
}

/**
 * Get the usage settings used for _mali_mem_alloc - @see mali_mem_usage_flag
 * @note Returns the requested usage flags, not what the memory is actually capable to
 * @param mem The block to operate on
 * @return The usage flags used for allocation of the memory block
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_usage_get(mali_mem_handle mem)
{
        return _mali_base_common_mem_usage_get(mem);
}

/****** Functions to read, write, and copy from mali memory. ******/

/**
 * Read data from mali memory into CPU/host memory.
 * @param to Pointer to CPU/host memory to write to
 * @param from_mali The Mali memory block to read from
 * @param from_offset Offset in Mali memory to start read from
 * @param size Number of bytes to read
 * @note We currently do not support this for heaps
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_read(
		void* to,
		mali_mem_handle from_mali,
		u32 from_offset,
		u32 size)
{
        _mali_base_arch_mem_read(to, (mali_mem*)from_mali, from_offset, size);
}

/**
 * Read data from mali memory into CPU/host memory.
 * @param to Pointer to CPU/host memory to write to
 * @param from_mali The Mali memory block to read from
 * @param from_offset Offset in Mali memory to start read from
 * @param size Number of bytes to read
 * @param typesize Size of single entity to read
 * @note We currently do not support this for heaps
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_read_mali_to_cpu(
		void* to,
		mali_mem_handle from_mali,
		u32 from_offset,
		u32 size,
		u32 typesize)
{
        _mali_base_arch_mem_read_mali_to_cpu(to, (mali_mem*)from_mali, from_offset, size, typesize);
}

/**
 * Function to write data to Mali memory from CPU/host memory.
 * @param to_mali Destination Mali memory block
 * @param to_offset Offset in the Mali memory block to start writing to
 * @param from Pointer to CPU/host memory to write into Mali memory
 * @param size Number of bytes to write
 * @note We currently do not support this for heaps
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_write(
		mali_mem_handle to_mali,
		u32 to_offset,
		const void* from,
		u32 size)
{
        _mali_base_arch_mem_write((mali_mem*)to_mali, to_offset, from, size);
}

/**
 * Function to write data to Mali memory from CPU/host memory in endian aware manner.
 * @param to_mali Destination Mali memory block
 * @param to_offset Offset in the Mali memory block to start writing to
 * @param from Pointer to CPU/host memory to write into Mali memory
 * @param size Number of bytes to write
 * @param typesize Size of one written entity in bytes
 * @note We currently do not support this for heaps
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_write_cpu_to_mali(
		mali_mem_handle to_mali,
		u32 to_offset,
		const void* from,
		u32 size,
		u32 typesize)
{
	MALI_DEBUG_ASSERT((typesize) != 0, ("typesize == 0"));
	_mali_base_arch_mem_write_cpu_to_mali((mali_mem*)to_mali, to_offset, from, size, typesize);
}

/**
 * Copy memory from a block of mali memory to another one.
 * The copy function does not support overlapping mem regions.
 * @param to_mali Destination Mali memory block
 * @param to_offset Offset inside the destination block to start writing
 * @param from_mali Source Mali memory block
 * @param from_offset Offset inside the source block to start reading from
 * @param size Number of bytes to copy
 * @note We currently do not support this for heaps
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_copy(
		mali_mem_handle to_mali,
		u32 to_offset,
		mali_mem_handle from_mali,
		u32 from_offset,
		u32 size)
{
	_mali_base_arch_mem_copy((mali_mem*)to_mali, to_offset, (mali_mem*)from_mali, from_offset, size);
}

/**
 * A variant of _mali_mem_write_cpu_to_mali function that should be used whenever size is known at compile time
 * and it is guaranteed that both source and destination are 4-byte aligned (including offset). Such assumptions
 * allows the compiler to perform additional optimizations.
 *
 * @param to_mali Destination Mali memory block
 * @param to_offset Offset in the Mali memory block to start writing to
 * @param from Pointer to CPU/host memory to write into Mali memory
 * @param size Number of bytes to write
 * @param typesize Size of one written entity in bytes
 * @note We currently do not support this for heaps
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_write_cpu_to_mali_32(
       mali_mem_handle to_mali,
       u32 to_offset,
       const void* from,
       u32 size,
       u32 typesize)
{
   MALI_DEBUG_ASSERT((typesize) != 0, ("typesize == 0"));

  _mali_base_arch_mem_write_cpu_to_mali_32((mali_mem*)to_mali, to_offset, from, size, typesize);
}


/****** Functions to access Mali memory directly through a pointer. ******/

/**
 * Map a area to a pointer.
 * Only one mapping per block at the time is supported.
 * The alignment variable will align the returned pointer if a host-buffer is needed to represent the Mali memory.
 * If direct access is allowed the alignment will be what the mali-memory address has, and the parameter will be ignored.
 * @see mali_mem_ptr_flag for the possible optimizations and access restrictions
 * @param mem The Mali memory handle to operate on
 * @param offset_in_mem Offset inside the Mali memory block where the mapping should start
 * @param size Size of mapping
 * @param align_pow2 Alignment used if a CPU/host memory buffer is needed to represent the mapping
 * @param flag Requested minimum capabilities for the buffer (read/write), and optimization hints (do not preload existing contents)
 * @return Pointer to memory which is accessible by the CPU in the modes specified or NULL if not able to perform the mapping
 * @note We do not support this for heaps
 */
MALI_STATIC_FORCE_INLINE void * _mali_mem_ptr_map_area(
		mali_mem_handle handle,
		u32 offset_in_mem,
		u32 size,
		u32 align_pow2,
		u32 flag)
{
	u32 ref;
	mali_mem *mem = (mali_mem*)handle;

	MALI_IGNORE(size);
	MALI_IGNORE(align_pow2);
	MALI_IGNORE(flag);
	
	ref = _mali_sys_atomic_inc_and_return(&mem->cpu_map_ref_count);
	if (1 == ref)
	{
		/* Memory is not mapped on CPU, do the mapping now. */
		MALI_DEBUG_ASSERT(!handle->cpu_address, ("Unmapped memory has CPU pointer"));

		if (MALI_FALSE == _mali_base_arch_mem_map(mem, 0, mem->size,
		                          flag | MALI_MEM_PTR_READABLE | MALI_MEM_PTR_WRITABLE,
					  &handle->cpu_address))
		{
			MALI_DEBUG_ASSERT(0, ("Failed to map memory"));
			return NULL;
		}
	}

	MALI_DEBUG_ASSERT_POINTER(handle->cpu_address);

	return (void*)(((u32)handle->cpu_address)+ offset_in_mem);
}

/**
 * Unmap area. If you free the mali_mem before unmapping the area,
 * the contents of the pointer will not be written back, and the
 * pointer itself will be unmapped.
 * @param mem The Mali memory handle to operate on
 * @note We do not support this for heaps
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_ptr_unmap_area(mali_mem_handle memh)
{
	mali_mem *mem = (mali_mem*)memh;
	u32 ref;

	ref = _mali_sys_atomic_dec_and_return(&mem->cpu_map_ref_count);

	if (0 == ref)
	{
		_mali_base_arch_mem_unmap(mem);
	}

	return;
}

/****** Using mali_mem_handle as a linked list with an user_pointer: ******/

/**
 * Adding a new element to the linked list, after current element.
 * @param current The item to insert the new item after, can be NULL
 * @param item_to_insert The item to be inserted
 * @return The current argument if it was non-NULL, else the item_to_insert argument.
 */
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_list_insert_after(mali_mem_handle current, mali_mem_handle item_to_insert)
{
        return _mali_base_common_mem_list_insert_after(current, item_to_insert);
}

/**
 * Adding new element to the linked list, before current element.
 * @param current The item to insert the new item before
 * @param item_to_insert The item to be inserted
 * @return The item_to_insert argument
 */
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_list_insert_before(mali_mem_handle current, mali_mem_handle item_to_insert)
{
        return _mali_base_common_mem_list_insert_before(current, item_to_insert);
}

/**
 * Get the next element in the linked list of mali_mem_handles.
 * @param mem The handle to get the next of
 * @return Handle to the next memory block if any, NULL if no next exists
 */
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_list_get_next(mali_mem_handle mem)
{
        return _mali_base_common_mem_list_get_next(mem);
}

/**
 * Get the previous element in the linked list of mali_mem_handles.
 * @param mem the handle to get the previous of
 * @return Handle to the next memory block if any, NULL if no next exists
 */
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_list_get_previous(mali_mem_handle mem)
{
        return _mali_base_common_mem_list_get_previous(mem);
}

/**
 * Remove element, return next in list if exist, else previous.
 * @param mem The item to remove
 * @return Next in list if it exist, else previous if it exitst, or NULL if no items linked
 */
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_list_remove_item(mali_mem_handle mem)
{
        return _mali_base_common_mem_list_remove_item(mem);
}

/**
 * Free all linked memory elements starting from the given list and all it the next chain.
 * @note Only the next and all in that direction will be freed, any previous element will be untouched!
 * @param list Handle to the Mali memory block which is the head of a list of blocks to free
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_list_free(mali_mem_handle list)
{
        _mali_base_common_mem_list_free(list);
}

/**
 * Total sum of the size of all the memory blocks in the linked list if the next-direction
 * @param mem
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_list_size_get(mali_mem_handle mem)
{
        return _mali_base_common_mem_list_size_get(mem);
}

#if !defined(HARDWARE_ISSUE_3251)
/****** Functions to work with heap memory ******/

/**
 * Allocate memory that can grow in size.
 * You set the expected size which will also be the
 * start size of the heap. Then the heap will grow in
 * block_size bytes, for each time it need to expand.
 * @param ctx Base context.
 * @param default_size This amount of memory will be available when function returns.
 * @param maximum_size The heap is not allowed to grow to a bigger size than this.
 * @param block_size How much to allocate, each time heap must expand. Must be a pow2 size.
 */
/* coverity[+alloc] */
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_heap_alloc(
		mali_base_ctx_handle ctx,
		u32 default_size,
		u32 maximum_size,
		u32 block_size)
{
        return _mali_base_common_mem_heap_alloc(ctx, default_size, maximum_size, block_size);
}

/**
 * Returns the physical address that should be set as the start
 * address for the given heap
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_heap_get_start_address(mali_mem_handle heap)
{
        return _mali_base_common_mem_heap_get_start_address(heap);
}

/**
 * Returns the physical address that should be set as end of first block
 * address for the given heap
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_heap_get_end_address_of_first_block(mali_mem_handle heap)
{
        return _mali_base_common_mem_heap_get_end_address_of_first_block(heap);
}

/**
 * Returns the physical address that should be set as end
 * address for the given heap
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_heap_get_end_address(mali_mem_handle heap)
{
        return _mali_base_common_mem_heap_get_end_address(heap);
}

/**
 * Get blocksize, a pow2 number telling where memory may
 * be discontinuous:
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_heap_get_blocksize(mali_mem_handle mem)
{
        return _mali_base_common_mem_heap_get_blocksize(mem);
}

/**
 * Get maximum memory size allowed for this heap:
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_heap_get_max_size(mali_mem_handle mem)
{
        return _mali_base_common_mem_heap_get_max_size(mem);
}

/**
 * Reset the heap.
 * Equivalent to free it and allocate it again, except that reset
 * can not return out of mem.
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_heap_reset(mali_mem_handle mem)
{
        _mali_base_common_mem_heap_reset(mem);
}

/**
 * Query how many bytes on the heap have been allocated.
 * @param mem Handle to the heap to query the used bytes of
 * @return Number of bytes allocated on the given heap
 */
MALI_STATIC_FORCE_INLINE u32 _mali_mem_heap_used_bytes_get(mali_mem_handle mem)
{
        return _mali_base_common_mem_heap_used_bytes_get(mem);
}

/**
 * Returns true if mali_memory is of heap type.
 */
MALI_STATIC_FORCE_INLINE mali_bool _mali_mem_is_heap(mali_mem_handle mem)
{
        return _mali_base_common_mem_is_heap(mem);
}

/**
 * Read a 64-bit word from the given offset inside a heap
 * @param heap The heap to read from
 * @param to_offset The offset inside the heap to read from (in bytes)
 * @return The value at the given offset
 */
MALI_STATIC_FORCE_INLINE u64 _mali_mem_heap_read64(mali_mem_handle heap, u32 offset)
{
        return _mali_base_common_heap_read64(heap, offset);
}

/**
 * Write a 64-bit word to the given offset inside a heap
 * @param heap The heap to write to
 * @param to_offset The offset inside the heap to write to (in bytes)
 * @param word64 The value to write
 */
MALI_STATIC_FORCE_INLINE void _mali_mem_heap_write64(mali_mem_handle heap, u32 to_offset, u64 word64)
{
        _mali_base_common_heap_write64(heap, to_offset, word64);
}

/**
 * Resize a heap (increase its size) without waiting for it to grow dynamically.
 * @param ctx the base context
 * @param heap The heap whose size should be increased
 * @param new_size The new size. 
 * @return MALI_ERROR_NONE if successful, MALI_ERROR_OUT_OF_MEMORY if we fail
 * to grow the heap to the requested size.
 */
MALI_STATIC_FORCE_INLINE mali_err_code _mali_mem_heap_resize( mali_base_ctx_handle ctx, mali_mem_handle heap, u32 new_size)
{
	return _mali_base_common_mem_heap_resize(ctx, heap, new_size);
}

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
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_add_phys_mem(mali_base_ctx_handle ctx, u32 phys_addr, u32 size, void * mapping, u32 access_rights)
{
        return _mali_base_common_mem_add_phys_mem(ctx, phys_addr, size, mapping, access_rights);
}

#if MALI_USE_DMA_BUF
/**
 * Create a Mali mem handle to represent an external memory given by a dma_buf descriptor
 * This makes it possible to map an externally allocated memory into memory accessible by mali cores.
 * @param ctx The base context to scope usage of this external memory to
 * @param mem_fd Descriptor to a dma_buf exported memory
 * @return A Mali memory handle which represents the external memory
 */
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_wrap_dma_buf(mali_base_ctx_handle ctx, int mem_fd, u32 offset)
{
	return _mali_base_common_mem_wrap_dma_buf(ctx, mem_fd, offset);
}

/**
 * Return a dma_buf descriptor given a Mali mem handle
 * @param mem_handle Handle to a dma_buf wrapper mali memory handle
 * @return A dma_buf descriptor for which the mali memory was wrapped
 * @note In case the mali memory does not link to dma_buf return value will be -1
 */
MALI_STATIC_FORCE_INLINE int _mali_mem_get_dma_buf_descriptor(mali_mem_handle mem_handle)
{
	return _mali_base_common_mem_get_dma_buf_descriptor(mem_handle);
}

#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
/**
 * Create a Mali mem handle to represent an external memory given by a Unified Memory Provider
 * This makes it possible to map an externally allocated memory into memory accessible by mali cores.
 * @param ctx The base context to scope usage of this external memory to
 * @param mem Handle to a ump allocated memory
 * @param offset Offset into external memory
 * @return A Mali memory handle which represents the external memory
 */
MALI_STATIC_FORCE_INLINE mali_mem_handle _mali_mem_wrap_ump_memory(mali_base_ctx_handle ctx, ump_handle mem, u32 offset)
{
        u32 access_rights = (1<<7)-1; /* all rights - OPT: we may change the interface to let user select this */
        return _mali_base_common_mem_wrap_ump_memory(ctx, mem, offset, access_rights );
}

/**
 * Return a UMP handle given a Mali mem handle
 * This will return the ump of a wrapped mali handle
 * @param mem_handle Handle to a ump wrapper mali memory handle
 * @return A UMP handle for which the mali memory was wrapped
 * @note In case the mali memory does not link to UMP return value will be UMP_INVALID_MEMORY_HANDLE
 */
MALI_STATIC_FORCE_INLINE ump_handle _mali_mem_get_ump_memory(mali_mem_handle mem_handle)
{
        return _mali_base_common_mem_get_ump_memory(mem_handle);
}

#endif

/**
 * Compare two mali_mem_handles
 *
 * @note Only UMP and dma_buf is currently supported
 *
 * @return negative number if first argument is smaller, 0 if equal, and a positive number if greater
 */
MALI_STATIC_FORCE_INLINE int _mali_mem_relative_compare(mali_mem_handle handle1, mali_mem_handle handle2)
{
	mali_mem *mem1 = (mali_mem*)handle1;
	mali_mem *mem2 = (mali_mem*)handle2;
	int typediff = mem1->memory_subtype < mem2->memory_subtype;

	if(0 == typediff)
	{
		switch (mem1->memory_subtype)
		{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
			case MALI_MEM_TYPE_UMP_EXTERNAL_MEMORY:
				return ump_secure_id_get(mem1->relationship.ump_mem) - ump_secure_id_get(mem2->relationship.ump_mem);
#endif
#if MALI_USE_DMA_BUF
			case MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY:
                                /* NOTE: this can be used to check whether two
                                 * dma buf handles are the same, but it is
                                 * _NOT_ guaranteed to return a correct
                                 * relative comparison since file descriptors
                                 * are not guaranteed to monotonically
                                 * increase. */
                                return mem1->relationship.dma_buf_fd - mem2->relationship.dma_buf_fd;
#endif
			case MALI_MEM_TYPE_EXTERNAL_MEMORY: /* Fall through */
			case MALI_MEM_TYPE_NORMAL: /* Fall through */
			case MALI_MEM_TYPE_HEAP: 
				return 0;
			default: /* Fall through */
				MALI_DEBUG_ASSERT(0, ("Memory type is not comparable"));
				return 0;
		}
	}
	else
	{
		return typediff;
	}
}

/****** Functions to print debug-info. ******/

MALI_DEBUG_CODE(
	/**
	 * Print all mem-info in a mali_base_ctx_handle.
	 * @param ctx The Base context to dump the memory info for
	 *
	 * Legend:
	 * A - Allocated
	 * F - Free
	 * After the usage-flag the size of the block is shown (in decimal)
	 *
	 * NOTE When context separation is up and running we'll probably change the output slightly
	 */
	MALI_STATIC_FORCE_INLINE void _mali_mem_debug_print_all(mali_base_ctx_handle ctx)
	{
        	_mali_base_common_mem_debug_print_all(ctx);
	}
)

#ifdef MALI_TEST_API
/**
* Retreive Mali memory usage statistics
* @param stats Pointer to the struct to fill with the current stats
*/
MALI_STATIC_FORCE_INLINE void _mali_mem_stats_get(mali_mem_stats * stats)
{
        _mali_base_common_mem_stats_get(stats);
}

#endif /* MALI_TEST_API */

#ifdef MALI_MEMORY_PROFILING
#include <base/common/mem/base_common_mem.h>
/**
 * If MALI_MEMORY_PROFILING is defined we substitute the frontend functions in the
 * Base driver for these functions with macros:
 * mali_mem_handle _mali_mem_alloc( mali_base_ctx_handle ctx, u32 size, u32 pow2_alignment, mali_mem_usage_flag mali_access );
 * mali_mem_handle _mali_mem_heap_alloc( mali_base_ctx_handle ctx, u32 default_size, u32 maximum_size, u32 block_size );
 * The macro runs the same function as the frontend layer would have done for these
 * functions, but in addition it runs the function:
 * \a _mali_base_common_mem_set_profiling(..)
 * That function stores the function name, file name and line number of the place
 * the function was used. With this system enabled, it is possible to know where
 * each allocated memory-block have been allocated, and thereby easily see what
 * the differend memory blocks contain in e.g. memory-dumps.
 */

/**
 * Function used to set function name, file name and line number stored in the
 * memory handler object. This is usefull in macros for storing where the
 * memory handler has been allocated.
 * @param mem The memory handle to store the data below in.
 * @param function_name Pointer to the function name to store
 * @param file_name     Pointer to the file name  to store
 * @param line_nr       Line number to store
 * @return The same mali_mem_handle as the input parameter
 */

extern mali_mem_handle _mali_base_common_mem_set_profiling(
				mali_mem_handle mem,
				const char * function_name,
				const char * file_name,
				u32 line_nr);

#define _mali_mem_alloc( ctx, size, pow2_alignment, mali_access) \
		(\
			_mali_base_common_mem_set_profiling(\
					_mali_base_common_mem_alloc((ctx),(size),(pow2_alignment), (mali_access)),\
					MALI_FUNCTION,\
					__FILE__,\
					__LINE__)\
		)

#if !defined(HARDWARE_ISSUE_3251)
#define _mali_mem_heap_alloc(ctx, default_size, maximum_size, block_size)\
		(\
			_mali_base_common_mem_set_profiling(\
				_mali_base_common_mem_heap_alloc((ctx), (default_size), (maximum_size), (block_size)),\
						MALI_FUNCTION,\
						__FILE__,\
						__LINE__)\
		)
#endif /* !defined(HARDWARE_ISSUE_3251) || HARDWARE_ISSUE_3251 == 0 */

#endif /* #ifdef MALI_MEMORY_PROFILING */


#ifdef __cplusplus
}
#endif

#endif /*_MALI_MEMORY_H_ */
