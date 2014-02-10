/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_KERNEL_MEMORY_ENGINE_H__
#define  __VR_KERNEL_MEMORY_ENGINE_H__

typedef void * vr_allocation_engine;

typedef enum { VR_MEM_ALLOC_FINISHED, VR_MEM_ALLOC_PARTIAL, VR_MEM_ALLOC_NONE, VR_MEM_ALLOC_INTERNAL_FAILURE } vr_physical_memory_allocation_result;

typedef struct vr_physical_memory_allocation
{
	void (*release)(void * ctx, void * handle); /**< Function to call on to release the physical memory */
	void * ctx;
	void * handle;
	struct vr_physical_memory_allocation * next;
} vr_physical_memory_allocation;

struct vr_page_table_block;

typedef struct vr_page_table_block
{
	void (*release)(struct vr_page_table_block *page_table_block);
	void * ctx;
	void * handle;
	u32 size; /**< In bytes, should be a multiple of VR_MMU_PAGE_SIZE to avoid internal fragementation */
	u32 phys_base; /**< VR physical address */
	vr_io_address mapping;
} vr_page_table_block;


/** @addtogroup _vr_osk_low_level_memory
 * @{ */

typedef enum
{
	VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE = 0x1,
	VR_MEMORY_ALLOCATION_FLAG_MAP_GUARD_PAGE     = 0x2,
} vr_memory_allocation_flag;

/**
 * Supplying this 'magic' physical address requests that the OS allocate the
 * physical address at page commit time, rather than committing a specific page
 */
#define VR_MEMORY_ALLOCATION_OS_ALLOCATED_PHYSADDR_MAGIC ((u32)(-1))

typedef struct vr_memory_allocation
{
	/* Information about the allocation */
	void * mapping; /**< CPU virtual address where the memory is mapped at */
	u32 vr_address; /**< The VR seen address of the memory allocation */
	u32 size; /**< Size of the allocation */
	u32 permission; /**< Permission settings */
	vr_memory_allocation_flag flags;
	u32 cache_settings; /* type: vr_memory_cache_settings, found in <linux/vr/vr_utgard_uk_types.h> Ump DD breaks if we include it...*/

	_vr_osk_lock_t * lock;

	/* Manager specific information pointers */
	void * vr_addr_mapping_info; /**< VR address allocation specific info */
	void * process_addr_mapping_info; /**< Mapping manager specific info */

	vr_physical_memory_allocation physical_allocation;

	_vr_osk_list_t list; /**< List for linking together memory allocations into the session's memory head */
} vr_memory_allocation;
/** @} */ /* end group _vr_osk_low_level_memory */


typedef struct vr_physical_memory_allocator
{
	vr_physical_memory_allocation_result (*allocate)(void* ctx, vr_allocation_engine * engine, vr_memory_allocation * descriptor, u32* offset, vr_physical_memory_allocation * alloc_info);
	vr_physical_memory_allocation_result (*allocate_page_table_block)(void * ctx, vr_page_table_block * block); /* VR_MEM_ALLOC_PARTIAL not allowed */
	void (*destroy)(struct vr_physical_memory_allocator * allocator);
	u32 (*stat)(struct vr_physical_memory_allocator * allocator);
	void * ctx;
	const char * name; /**< Descriptive name for use in vr_allocation_engine_report_allocators, or NULL */
	u32 alloc_order; /**< Order in which the allocations should happen */
	struct vr_physical_memory_allocator * next;
} vr_physical_memory_allocator;

typedef struct vr_kernel_mem_address_manager
{
	_vr_osk_errcode_t (*allocate)(vr_memory_allocation *); /**< Function to call to reserve an address */
	void (*release)(vr_memory_allocation *); /**< Function to call to free the address allocated */

	 /**
	  * Function called for each physical sub allocation.
	  * Called for each physical block allocated by the physical memory manager.
	  * @param[in] descriptor The memory descriptor in question
	  * @param[in] off Offset from the start of range
	  * @param[in,out] phys_addr A pointer to the physical address of the start of the
	  * physical block. When *phys_addr == VR_MEMORY_ALLOCATION_OS_ALLOCATED_PHYSADDR_MAGIC
	  * is used, this requests the function to allocate the physical page
	  * itself, and return it through the pointer provided.
	  * @param[in] size Length in bytes of the physical block
	  * @return _VR_OSK_ERR_OK on success.
	  * A value of type _vr_osk_errcode_t other than _VR_OSK_ERR_OK indicates failure.
	  * Specifically, _VR_OSK_ERR_UNSUPPORTED indicates that the function
	  * does not support allocating physical pages itself.
	  */
	 _vr_osk_errcode_t (*map_physical)(vr_memory_allocation * descriptor, u32 offset, u32 *phys_addr, u32 size);

	 /**
	  * Function called to remove a physical sub allocation.
	  * Called on error paths where one of the address managers fails.
	  *
	  * @note this is optional. For address managers where this is not
	  * implemented, the value of this member is NULL. The memory engine
	  * currently does not require the vr address manager to be able to
	  * unmap individual pages, but the process address manager must have this
	  * capability.
	  *
	  * @param[in] descriptor The memory descriptor in question
	  * @param[in] off Offset from the start of range
	  * @param[in] size Length in bytes of the physical block
	  * @param[in] flags flags to use on a per-page basis. For OS-allocated
	  * physical pages, this must include _VR_OSK_MEM_MAPREGION_FLAG_OS_ALLOCATED_PHYSADDR.
	  * @return _VR_OSK_ERR_OK on success.
	  * A value of type _vr_osk_errcode_t other than _VR_OSK_ERR_OK indicates failure.
	  */
	void (*unmap_physical)(vr_memory_allocation * descriptor, u32 offset, u32 size, _vr_osk_mem_mapregion_flags_t flags);

} vr_kernel_mem_address_manager;

vr_allocation_engine vr_allocation_engine_create(vr_kernel_mem_address_manager * vr_address_manager, vr_kernel_mem_address_manager * process_address_manager);

void vr_allocation_engine_destroy(vr_allocation_engine engine);

int vr_allocation_engine_allocate_memory(vr_allocation_engine engine, vr_memory_allocation * descriptor, vr_physical_memory_allocator * physical_provider, _vr_osk_list_t *tracking_list );
void vr_allocation_engine_release_memory(vr_allocation_engine engine, vr_memory_allocation * descriptor);

void vr_allocation_engine_release_pt1_vr_pagetables_unmap(vr_allocation_engine engine, vr_memory_allocation * descriptor);
void vr_allocation_engine_release_pt2_physical_memory_free(vr_allocation_engine engine, vr_memory_allocation * descriptor);

int vr_allocation_engine_map_physical(vr_allocation_engine engine, vr_memory_allocation * descriptor, u32 offset, u32 phys, u32 cpu_usage_adjust, u32 size);
void vr_allocation_engine_unmap_physical(vr_allocation_engine engine, vr_memory_allocation * descriptor, u32 offset, u32 size, _vr_osk_mem_mapregion_flags_t unmap_flags);

int vr_allocation_engine_allocate_page_tables(vr_allocation_engine, vr_page_table_block * descriptor, vr_physical_memory_allocator * physical_provider);

void vr_allocation_engine_report_allocators(vr_physical_memory_allocator * physical_provider);

u32 vr_allocation_engine_memory_usage(vr_physical_memory_allocator *allocator);

#endif /* __VR_KERNEL_MEMORY_ENGINE_H__ */
