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
 * @file base_arch_mem.h
 */

#ifndef _BASE_ARCH_MEM_H_
#define _BASE_ARCH_MEM_H_

#include <base/mali_types.h>
#include <base/common/mem/base_common_mem.h>
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
#include <ump/ump.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Definition of the MMU dump struct.
 * The dump struct tracks a storage buffer where the actual dump will be stored.
 * The size of the buffer can be queried by calling _mali_base_arch_mmu_dump_size_get.
 * This function have to be called just before taking the dump as the size will vary.
 * With a dump struct representing such a buffer the function _mali_base_arch_mmu_dump_get can be called.
 * If the buffer is too small the function will return an error and the buffer should be resized and the attemt retried.
 * On success the fields num_registers and num_pages will contain the number of registers and pages in the dump.
 * The pointers register_writes and pages will point into the storage_buffer to where they are stored.
 * How they are stored is documented for the struct members themselves.
 */
typedef struct mali_mmu_dump
{
	u32 storage_buffer_size; /**< Size of the buffer to dump to, must at least be what @see _mali_base_arch_mmu_dump_size_get returned */
	void * storage_buffer; /**< Pointer to the buffer to dump to */

	u32 num_registers; /**< Number of register writes dumped */
	/**
	 * Pointer to the registers dumped.
	 * Each register write is stored as two 32-bit words.
	 * The first word is the register address, the second
	 * word is the value written. So two register writes
	 * would be stored like this:
	 * 0x0000: 0x02000008 # address of the first register write
	 * 0x0004: 0x00000000 # value written
	 * 0x0008: 0x02000004 # address of the second/last register write
	 * 0x000C: 0xCD800000 # value written
	 *
	 */
	u32 * register_writes;

	u32 num_pages; /**< Number of pages dumped */
	/**
	 * Pointer to the pages dumped.
	 * Each page is stored as a 4 byte physical address of the page, then 4kB of page data, 4100 bytes in total.
	 * All the pages are stored back-to-back. So the @see num_pages variable must be used to know when to stop.
	 * Two pages would be stored like this:
	 * 0x0000: 0x98001000 # physical address of the first page
	 * 0x0004-0x1003: The first page
	 * 0x1004: 0x98004000 # physical address of the second page
	 * 0x1008-0x2007: The second page
	 */
	u32 * pages;
} mali_mmu_dump;

/**
 * Initialize the arch memory system. Multiple concurent users should be
 * supported, so reopening should just add another reference.
 * Upon first open it should perform the needed interaction with the
 * HW/DD to perpare it for memory requests.
 * @return Initialization status.
 */
mali_err_code _mali_base_arch_mem_open(void) MALI_CHECK_RESULT;

/**
 * Termintate the arch memory system. Multiple concurent users should be
 * supported, so termination should only occure when the last
 * reference is removed.
 */
void _mali_base_arch_mem_close(void);

/**
 * Read data from mali memory into CPU/host memory.
 * @param to Pointer to CPU/host memory to write to
 * @param from_mali The Mali memory block to read from
 * @param from_offset Offset in Mali memory to start read from
 * @param size Number of bytes to read
 */
void _mali_base_arch_mem_read(void * to, mali_mem * from_mali, u32 from_offset, u32 size);


/**
 * Read data from mali memory into CPU/host memory.
 * @param to Pointer to CPU/host memory to write to
 * @param from_mali The Mali memory block to read from
 * @param from_offset Offset in Mali memory to start read from
 * @param size Number of bytes to read
 * @param typesize Size of entity to read in bytes
 */
void _mali_base_arch_mem_read_mali_to_cpu(void * to, mali_mem * from_mali, u32 from_offset, u32 size, u32 typesize);

/**
 * Function to write data to Mali memory from CPU/host memory.
 * @param to_mali Destination Mali memory block
 * @param to_offset Offset in the Mali memory block to start writing to
 * @param from Pointer to CPU/host memory to write into Mali memory
 * @param size Number of bytes to write
 */
void _mali_base_arch_mem_write(mali_mem * to_mali, u32 to_offset, const void* from, u32 size);

/**
 * Function to write data to Mali memory from CPU/host memory in endian aware manner.
 *
 * @param to_mali Destination Mali memory block
 * @param to_offset Offset in the Mali memory block to start writing to
 * @param from Pointer to CPU/host memory to write into Mali memory
 * @param size Number of bytes to write
 * @param typesize Size of one written entity in bytes
 */
void _mali_base_arch_mem_write_cpu_to_mali(mali_mem * to_mali, u32 to_offset, const void* from, u32 size, u32 typesize);

/**
 * A variant of _mali_base_arch_mem_write_cpu_to_mali function that should be used whenever size is known at compile time
 * and it is guaranteed that both source(including offset) and destination are 4-byte aligned. These assumptions
 * allow the compiler to perform additional optimizations.
 *
 * @param to_mali Destination Mali memory block
 * @param to_offset Offset in the Mali memory block to start writing to
 * @param from Pointer to CPU/host memory to write into Mali memory
 * @param size Number of bytes to write
 * @param typesize Size of one written entity in bytes
 */
void _mali_base_arch_mem_write_cpu_to_mali_32(mali_mem * to_mali, u32 to_offset, const void* from, u32 size, u32 typesize);

/**
 * Copy memory from a block of mali memory to another one.
 * The copy function does not support overlapping mem regions.
 * @param to_mali Destination Mali memory block
 * @param to_offset Offset inside the destination block to start writing
 * @param from_mali Source Mali memory block
 * @param from_offset Offset inside the source block to start reading from
 * @param size Number of bytes to copy
 */
void _mali_base_arch_mem_copy(mali_mem * to_mali, u32 to_offset, mali_mem * from_mali, u32 from_offset, u32 size);


/**
 * Get the number of capability_sets the get_capability_sets will return.
 * Necessary to know how much memory we must allocate for this info.
 * @note Called only by Base Common
 * @return The number of capability sets which exists
 */
u32 _mali_base_arch_mem_get_num_capability_sets(void);

/**
 * Writes capabillity structs to the given buffer.
 * These tell which memory banks we can use, and their settings.
 * @note Called only by Base Common
 * @param buffer The buffer to write capability info to
 * @param size_of_buffer Size of the buffer to write to
 * @return MALI_ERR_FUNCTION_FAILED if buffer too small, MALI_ERR_NO_ERROR if successful
 */
mali_err_code _mali_base_arch_mem_get_capability_sets(void * buffer, u32 size_of_buffer) MALI_CHECK_RESULT;

/**
 * Map a area to a pointer.
 * Only one mapping per block at the time is supported.
 * @see mali_mem_ptr_flag for the possible optimizations and access restrictions
 * @param mem The Mali memory handle to operate on
 * @param offset_in_mem Offset inside the Mali memory block where the mapping should start
 * @param size Size of mapping
 * @param flag Requested minimum capabilities for the buffer (read/write), and optimization hints (do not preload existing contents)
 * @param cpu_ptr Pointer to a void * to store pointer to mapping
 * @return MALI_TRUE if mapping available, MALI_FALSE if read/write interface has to be used
 */
mali_bool _mali_base_arch_mem_map(mali_mem * mem, u32 offset_in_mem, u32 size, u32 flags, void** cpu_ptr);

/**
 * Unmap area.
 * Removes any mappings to the memory described by the given mali memory handle
 * @param mem The Mali memory handle to operate on
 */
void _mali_base_arch_mem_unmap(mali_mem * mem);

/**
 * Allocate a blank memory descriptor
 * Create and return a blank mali memory descriptor.
 * This could contain arch dependant information wrapped around it if needed by the backend
 * @note Called only by Base Common
 * @return Pointer to a memory descriptor, or NULL if out of memory
 */
mali_mem * _mali_base_arch_mem_allocate_descriptor(void);

/**
 * Free a descriptor.
 * The descriptor is not referencing any memory
 * @note Used by Mem_Common
 * @param descriptor The descriptor to free
 */
void _mali_base_arch_mem_free_descriptor(mali_mem * descriptor);

/**
 * Clear a memory descriptor
 * Called when a descriptor is no longer describing any memory and should be reset to a blank state
 * @note Called only by Base Common
 * @param descriptor The descriptor to clear
 */
void _mali_base_arch_descriptor_clear(mali_mem * descriptor);

/**
 * Initialize a memory bank
 * Called once during startup by Base common to perform possible arch specific operations needed prepare a bank for use.
 * @note Called only by Base Common
 * @param cache_settings
 * @return Standard Mali error code
 */
mali_err_code _mali_base_arch_mem_init_bank(u32 cache_settings) MALI_CHECK_RESULT;

/**
 * Request for memory
 * Called by Base when no more memory exists on its freelist
 * @note Called only by Base Common
 * @note The backend is allowed to return larger memory blocks than requested, no upper limit exists.
 * @note Passing in a block size of 0 (zero) means that architecture should return a block suitable for handling multiple small allocations.
 * @param cache_settings
 * @param minimum_block_size Minimum size of the allocation
 * @param descriptor The descriptor to use for the allocation
 */
mali_err_code _mali_base_arch_mem_get_memory(u32 cache_settings, u32 minimum_block_size, mali_mem * descriptor) MALI_CHECK_RESULT;

/**
 * Only called during memory system shutdown to free memory on the freelist and and any detected memory leaks
 * @note Called only by Base Common
 * @param mem Memory descriptor of memory to release
 */
void _mali_base_arch_mem_release_memory(mali_mem * mem);

/**
 * Resize neighbouring memory blocks A and B.
 * Resize neighbouring blocks as a pair, memory removed from one is added to the other.
 * Hint gives a hint about why the move is performed. The arch can deny a resize.
 * @note Either A or B can be blank descriptor. In that case the backend should insert
 * it infront if A is blank, or behind if B is blank.
 * After insertion the normal resizing operations is performed.
 * @note Called only by Base Common
 * @param A Block describing memory
 * @param B Block describing memory
 * @param size The size of the memory to be transfered, sign gives direction of transfer
 * @note Negative means that A should become smaler and B larger, positive the opposite.
 * @return Standard Mali error code
 */
mali_err_code _mali_base_arch_mem_resize_blocks(mali_mem * A, mali_mem * B, s32 size) MALI_CHECK_RESULT;

/**
 * Check if a descriptor represents a full block. This is necessary in order to
 * deallocate it.
 * @note Called only by Base Common
 * @param mem The memory to check
 */
mali_bool _mali_base_arch_mem_is_full_block(mali_mem * mem) MALI_CHECK_RESULT;


#if MALI_USE_DMA_BUF
/**
 * Get size of dma_buf.
 * @param pointer to u32 in which size size of dma-buf will be returned
 * @param mem_fd dma_buf descriptor
 * @return Standard Mali error code
 */
mali_err_code _mali_base_arch_mem_dma_buf_get_size(u32 *size, u32 mem_fd);

/**
 * dma_buf memory attach.
 * @param descriptor Mali memory descriptor the dma_buf memory is to be represented by
 * @param mem_fd dma_buf descriptor
 * @return Standard Mali error code
 */
mali_err_code _mali_base_arch_mem_dma_buf_attach(mali_mem * descriptor, u32 mem_fd);

/**
 * dma_buf memory release.
 * Releases a previously attached dma_buf memory block
 * @param descriptor Mali memory descriptor representing the dma_buf memory block
 */
void _mali_base_arch_mem_dma_buf_release(mali_mem * descriptor);

/**
 * dma_buf memory map
 *
 * Create CPU mapping of dma-buf memory
 *
 * @param descriptor Mali memory descriptor the dma_buf memory is to be represented by
 * @return Standard Mali error code
 */
mali_err_code _mali_base_arch_mem_dma_buf_map(mali_mem *descriptor);

/**
 * dma_buf memory unmap
 *
 * Removes CPU mapping of dma-buf memory
 *
 * @param descriptor Mali memory descriptor the dma_buf memory is to be represented by
 */
void _mali_base_arch_mem_dma_buf_unmap(mali_mem *descriptor);
#endif


#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
/**
 * UMP memory attach.
 * @param descriptor Mali memory descriptor the UMP memory is to be represented by
 * @param secure_id UMP secure ID to map
 * @param offset Offset into UMP memory to attach
 * @return Standard Mali error code
 */
mali_err_code _mali_base_arch_mem_ump_mem_attach(mali_mem * descriptor, ump_secure_id secure_id, u32 offset) MALI_CHECK_RESULT;
/**
 * UMP memory release.
 * Releases a previously attached UMP memory block
 * @param descriptor Mali memory descriptor representing the UMP memory block
 */
void _mali_base_arch_mem_ump_mem_release(mali_mem * descriptor);
#endif

/**
 * Create a Mali mem handle to represent externally managed memory.
 * @param phys_addr The physical address to wrap into a Mali memory handle
 * @param size The size of the external memory
 * @param mapping Mapping of the physical memory, if it exists
 * @param access_rights Rights to memory to assign
 * @return Standard Mali error code
 */
mali_err_code _mali_base_arch_mem_add_phys_mem(mali_mem * descriptor, u32 phys_addr, u32 size, void * mapping, u32 access_rights) MALI_CHECK_RESULT;

/**
 * Release the externally managed memory represented by a descriptor
 * @param descrptor The descriptor representing externally managed memory
 */
void _mali_base_arch_release_phys_mem(mali_mem * descriptor);

/**
 * Query the size needed for an mmu dump
 * Returns the size needed for a dump of the MMU configuration.
 * Returns 0 if the system uses no MMU.
 * @return Size of the buffer needed for a dump or 0 if no MMU to dump is present
 */
MALI_IMPORT u32 _mali_base_arch_mmu_dump_size_get(void);

/**
 * Dump current MMU data
 * Dumps the needed register setup for the MMU core(s) and the currently active page tables.
 * @param dump Pointer to the struct to store the dump in
 * @return MALI_ERR_NO_ERROR on success, MALI_ERR_OUT_OF_MEMORY if the storage pool is too small and should be increased and MALI_ERR_FUNCTION_FAILED if the call failed in any other way.
 */
MALI_IMPORT mali_err_code _mali_base_arch_mmu_dump_get(mali_mmu_dump * dump);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_BASE_ARCH_MEM_H_ */
