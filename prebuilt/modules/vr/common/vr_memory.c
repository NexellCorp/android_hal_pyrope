/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_kernel_common.h"
#include "vr_kernel_descriptor_mapping.h"
#include "vr_mem_validation.h"
#include "vr_memory.h"
#include "vr_mmu_page_directory.h"
#include "vr_kernel_memory_engine.h"
#include "vr_block_allocator.h"
#include "vr_kernel_mem_os.h"
#include "vr_session.h"
#include "vr_l2_cache.h"
#include "vr_scheduler.h"
#if defined(CONFIG_VR400_UMP)
#include "ump_kernel_interface.h"
#endif

/* kernel side OS functions and user-kernel interface */
#include "vr_osk.h"
#include "vr_osk_vr.h"
#include "vr_ukk.h"
#include "vr_osk_list.h"
#include "vr_osk_bitops.h"

/**
 * Per-session memory descriptor mapping table sizes
 */
#define VR_MEM_DESCRIPTORS_INIT 64
#define VR_MEM_DESCRIPTORS_MAX 65536

typedef struct dedicated_memory_info
{
	u32 base;
	u32 size;
	struct dedicated_memory_info * next;
} dedicated_memory_info;

/* types used for external_memory and ump_memory physical memory allocators, which are using the vr_allocation_engine */
#if defined(CONFIG_VR400_UMP)
typedef struct ump_mem_allocation
{
	vr_allocation_engine * engine;
	vr_memory_allocation * descriptor;
	u32 initial_offset;
	u32 size_allocated;
	ump_dd_handle ump_mem;
} ump_mem_allocation ;
#endif

typedef struct external_mem_allocation
{
	vr_allocation_engine * engine;
	vr_memory_allocation * descriptor;
	u32 initial_offset;
	u32 size;
} external_mem_allocation;

/**
 * @brief Internal function for unmapping memory
 *
 * Worker function for unmapping memory from a user-process. We assume that the
 * session/descriptor's lock was obtained before entry. For example, the
 * wrapper _vr_ukk_mem_munmap() will lock the descriptor, then call this
 * function to do the actual unmapping. vr_memory_core_session_end() could
 * also call this directly (depending on compilation options), having locked
 * the descriptor.
 *
 * This function will fail if it is unable to put the MMU in stall mode (which
 * might be the case if a page fault is also being processed).
 *
 * @param args see _vr_uk_mem_munmap_s in "vr_utgard_uk_types.h"
 * @return _VR_OSK_ERR_OK on success, otherwise a suitable _vr_osk_errcode_t on failure.
 */
static _vr_osk_errcode_t _vr_ukk_mem_munmap_internal( _vr_uk_mem_munmap_s *args );

#if defined(CONFIG_VR400_UMP)
static void ump_memory_release(void * ctx, void * handle);
static vr_physical_memory_allocation_result ump_memory_commit(void* ctx, vr_allocation_engine * engine, vr_memory_allocation * descriptor, u32* offset, vr_physical_memory_allocation * alloc_info);
#endif /* CONFIG_VR400_UMP */


static void external_memory_release(void * ctx, void * handle);
static vr_physical_memory_allocation_result external_memory_commit(void* ctx, vr_allocation_engine * engine, vr_memory_allocation * descriptor, u32* offset, vr_physical_memory_allocation * alloc_info);


/* nop functions */

/* vr address manager needs to allocate page tables on allocate, write to page table(s) on map, write to page table(s) and release page tables on release */
static _vr_osk_errcode_t  vr_address_manager_allocate(vr_memory_allocation * descriptor); /* validates the range, allocates memory for the page tables if needed */
static _vr_osk_errcode_t  vr_address_manager_map(vr_memory_allocation * descriptor, u32 offset, u32 *phys_addr, u32 size);
static void vr_address_manager_release(vr_memory_allocation * descriptor);

/* MMU variables */

typedef struct vr_mmu_page_table_allocation
{
	_vr_osk_list_t list;
	u32 * usage_map;
	u32 usage_count;
	u32 num_pages;
	vr_page_table_block pages;
} vr_mmu_page_table_allocation;

typedef struct vr_mmu_page_table_allocations
{
	_vr_osk_lock_t *lock;
	_vr_osk_list_t partial;
	_vr_osk_list_t full;
	/* we never hold on to a empty allocation */
} vr_mmu_page_table_allocations;

static vr_kernel_mem_address_manager vr_address_manager =
{
	vr_address_manager_allocate, /* allocate */
	vr_address_manager_release,  /* release */
	vr_address_manager_map,      /* map_physical */
	NULL                           /* unmap_physical not present*/
};

/* the mmu page table cache */
static struct vr_mmu_page_table_allocations page_table_cache;


static vr_kernel_mem_address_manager process_address_manager =
{
	_vr_osk_mem_mapregion_init,  /* allocate */
	_vr_osk_mem_mapregion_term,  /* release */
	_vr_osk_mem_mapregion_map,   /* map_physical */
	_vr_osk_mem_mapregion_unmap  /* unmap_physical */
};

static _vr_osk_errcode_t vr_mmu_page_table_cache_create(void);
static void vr_mmu_page_table_cache_destroy(void);

static vr_allocation_engine memory_engine = NULL;
static vr_physical_memory_allocator * physical_memory_allocators = NULL;

static dedicated_memory_info * mem_region_registrations = NULL;

vr_allocation_engine vr_mem_get_memory_engine(void)
{
	return memory_engine;
}

/* called during module init */
_vr_osk_errcode_t vr_memory_initialize(void)
{
	_vr_osk_errcode_t err;

	VR_DEBUG_PRINT(2, ("Memory system initializing\n"));

	err = vr_mmu_page_table_cache_create();
	if(_VR_OSK_ERR_OK != err)
	{
		VR_ERROR(err);
	}

	memory_engine = vr_allocation_engine_create(&vr_address_manager, &process_address_manager);
	VR_CHECK_NON_NULL( memory_engine, _VR_OSK_ERR_FAULT);

	VR_SUCCESS;
}

/* called if/when our module is unloaded */
void vr_memory_terminate(void)
{
	VR_DEBUG_PRINT(2, ("Memory system terminating\n"));

	vr_mmu_page_table_cache_destroy();

	while ( NULL != mem_region_registrations)
	{
		dedicated_memory_info * m;
		m = mem_region_registrations;
		mem_region_registrations = m->next;
		_vr_osk_mem_unreqregion(m->base, m->size);
		_vr_osk_free(m);
	}

	while ( NULL != physical_memory_allocators)
	{
		vr_physical_memory_allocator * m;
		m = physical_memory_allocators;
		physical_memory_allocators = m->next;
		m->destroy(m);
	}

	if (NULL != memory_engine)
	{
		vr_allocation_engine_destroy(memory_engine);
		memory_engine = NULL;
	}
}

_vr_osk_errcode_t vr_memory_session_begin(struct vr_session_data * session_data)
{
	VR_DEBUG_PRINT(5, ("Memory session begin\n"));

	/* create descriptor mapping table */
	session_data->descriptor_mapping = vr_descriptor_mapping_create(VR_MEM_DESCRIPTORS_INIT, VR_MEM_DESCRIPTORS_MAX);

	if (NULL == session_data->descriptor_mapping)
	{
		VR_ERROR(_VR_OSK_ERR_NOMEM);
	}

	session_data->memory_lock = _vr_osk_lock_init( _VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_ONELOCK
	                                | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE, 0, _VR_OSK_LOCK_ORDER_MEM_SESSION);
	if (NULL == session_data->memory_lock)
	{
		vr_descriptor_mapping_destroy(session_data->descriptor_mapping);
		_vr_osk_free(session_data);
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}

	/* Init the session's memory allocation list */
	_VR_OSK_INIT_LIST_HEAD( &session_data->memory_head );

	VR_DEBUG_PRINT(5, ("MMU session begin: success\n"));
	VR_SUCCESS;
}

static void descriptor_table_cleanup_callback(int descriptor_id, void* map_target)
{
	vr_memory_allocation * descriptor;

	descriptor = (vr_memory_allocation*)map_target;

	VR_DEBUG_PRINT(3, ("Cleanup of descriptor %d mapping to 0x%x in descriptor table\n", descriptor_id, map_target));
	VR_DEBUG_ASSERT(descriptor);

	vr_allocation_engine_release_memory(memory_engine, descriptor);
	_vr_osk_free(descriptor);
}

void vr_memory_session_end(struct vr_session_data *session_data)
{
	_vr_osk_errcode_t err = _VR_OSK_ERR_BUSY;

	VR_DEBUG_PRINT(3, ("MMU session end\n"));

	if (NULL == session_data)
	{
		VR_DEBUG_PRINT(1, ("No session data found during session end\n"));
		return;
	}

	while (err == _VR_OSK_ERR_BUSY)
	{
		/* Lock the session so we can modify the memory list */
		_vr_osk_lock_wait( session_data->memory_lock, _VR_OSK_LOCKMODE_RW );
		err = _VR_OSK_ERR_OK;

		/* Free all memory engine allocations */
		if (!_vr_osk_list_empty(&session_data->memory_head))
		{
			vr_memory_allocation *descriptor;
			vr_memory_allocation *temp;
			_vr_uk_mem_munmap_s unmap_args;

			VR_DEBUG_PRINT(1, ("Memory found on session usage list during session termination\n"));

			unmap_args.ctx = session_data;

			/* use the 'safe' list iterator, since freeing removes the active block from the list we're iterating */
			_VR_OSK_LIST_FOREACHENTRY(descriptor, temp, &session_data->memory_head, vr_memory_allocation, list)
			{
				VR_DEBUG_PRINT(4, ("Freeing block with vr address 0x%x size %d mapped in user space at 0x%x\n",
							descriptor->vr_address, descriptor->size, descriptor->size, descriptor->mapping)
						);
				/* ASSERT that the descriptor's lock references the correct thing */
				VR_DEBUG_ASSERT(  descriptor->lock == session_data->memory_lock );
				/* Therefore, we have already locked the descriptor */

				unmap_args.size = descriptor->size;
				unmap_args.mapping = descriptor->mapping;
				unmap_args.cookie = (u32)descriptor;

				/*
					* This removes the descriptor from the list, and frees the descriptor
					*
					* Does not handle the _VR_OSK_SPECIFIC_INDIRECT_MMAP case, since
					* the only OS we are aware of that requires indirect MMAP also has
					* implicit mmap cleanup.
					*/
				err = _vr_ukk_mem_munmap_internal( &unmap_args );

				if (err == _VR_OSK_ERR_BUSY)
				{
					_vr_osk_lock_signal( session_data->memory_lock, _VR_OSK_LOCKMODE_RW );
					/*
						* Reason for this;
						* We where unable to stall the MMU, probably because we are in page fault handling.
						* Sleep for a while with the session lock released, then try again.
						* Abnormal termination of programs with running VR jobs is a normal reason for this.
						*/
					_vr_osk_time_ubusydelay(10);
					break; /* Will jump back into: "while (err == _VR_OSK_ERR_BUSY)" */
				}
			}
		}
	}
	/* Assert that we really did free everything */
	VR_DEBUG_ASSERT( _vr_osk_list_empty(&session_data->memory_head) );

	if (NULL != session_data->descriptor_mapping)
	{
		vr_descriptor_mapping_call_for_each(session_data->descriptor_mapping, descriptor_table_cleanup_callback);
		vr_descriptor_mapping_destroy(session_data->descriptor_mapping);
		session_data->descriptor_mapping = NULL;
	}

	_vr_osk_lock_signal( session_data->memory_lock, _VR_OSK_LOCKMODE_RW );

	/**
	 * @note Could the VMA close handler mean that we use the session data after it was freed?
	 * In which case, would need to refcount the session data, and free on VMA close
	 */

	/* Free the lock */
	_vr_osk_lock_term( session_data->memory_lock );

	return;
}

_vr_osk_errcode_t vr_memory_core_resource_os_memory(u32 size)
{
	vr_physical_memory_allocator * allocator;
	vr_physical_memory_allocator ** next_allocator_list;

	u32 alloc_order = 1; /* OS memory has second priority */

	allocator = vr_os_allocator_create(size, 0 /* cpu_usage_adjust */, "Shared VR GPU memory");
	if (NULL == allocator)
	{
		VR_DEBUG_PRINT(1, ("Failed to create OS memory allocator\n"));
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}

	allocator->alloc_order = alloc_order;

	/* link in the allocator: insertion into ordered list
	 * resources of the same alloc_order will be Last-in-first */
	next_allocator_list = &physical_memory_allocators;

	while (NULL != *next_allocator_list &&
			(*next_allocator_list)->alloc_order < alloc_order )
	{
		next_allocator_list = &((*next_allocator_list)->next);
	}

	allocator->next = (*next_allocator_list);
	(*next_allocator_list) = allocator;

	VR_SUCCESS;
}

_vr_osk_errcode_t vr_memory_core_resource_dedicated_memory(u32 start, u32 size)
{
	vr_physical_memory_allocator * allocator;
	vr_physical_memory_allocator ** next_allocator_list;
	dedicated_memory_info * cleanup_data;

	u32 alloc_order = 0; /* Dedicated range has first priority */

	/* do the low level linux operation first */

	/* Request ownership of the memory */
	if (_VR_OSK_ERR_OK != _vr_osk_mem_reqregion(start, size, "Dedicated VR GPU memory"))
	{
		VR_DEBUG_PRINT(1, ("Failed to request memory region for frame buffer (0x%08X - 0x%08X)\n", start, start + size - 1));
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}

	/* create generic block allocator object to handle it */
	allocator = vr_block_allocator_create(start, 0 /* cpu_usage_adjust */, size, "Dedicated VR GPU memory");

	if (NULL == allocator)
	{
		VR_DEBUG_PRINT(1, ("Memory bank registration failed\n"));
		_vr_osk_mem_unreqregion(start, size);
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}

	/* save low level cleanup info */
	allocator->alloc_order = alloc_order;

	cleanup_data = _vr_osk_malloc(sizeof(dedicated_memory_info));

	if (NULL == cleanup_data)
	{
		_vr_osk_mem_unreqregion(start, size);
		allocator->destroy(allocator);
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}

	cleanup_data->base = start;
	cleanup_data->size = size;

	cleanup_data->next = mem_region_registrations;
	mem_region_registrations = cleanup_data;

	/* link in the allocator: insertion into ordered list
	 * resources of the same alloc_order will be Last-in-first */
	next_allocator_list = &physical_memory_allocators;

	while ( NULL != *next_allocator_list &&
			(*next_allocator_list)->alloc_order < alloc_order )
	{
		next_allocator_list = &((*next_allocator_list)->next);
	}

	allocator->next = (*next_allocator_list);
	(*next_allocator_list) = allocator;

	VR_SUCCESS;
}

#if defined(CONFIG_VR400_UMP)
static vr_physical_memory_allocation_result ump_memory_commit(void* ctx, vr_allocation_engine * engine, vr_memory_allocation * descriptor, u32* offset, vr_physical_memory_allocation * alloc_info)
{
	ump_dd_handle ump_mem;
	u32 nr_blocks;
	u32 i;
	ump_dd_physical_block * ump_blocks;
	ump_mem_allocation *ret_allocation;

	VR_DEBUG_ASSERT_POINTER(ctx);
	VR_DEBUG_ASSERT_POINTER(engine);
	VR_DEBUG_ASSERT_POINTER(descriptor);
	VR_DEBUG_ASSERT_POINTER(alloc_info);

	ret_allocation = _vr_osk_malloc( sizeof( ump_mem_allocation ) );
	if ( NULL==ret_allocation ) return VR_MEM_ALLOC_INTERNAL_FAILURE;

	ump_mem = (ump_dd_handle)ctx;

	VR_DEBUG_PRINT(4, ("In ump_memory_commit\n"));

	nr_blocks = ump_dd_phys_block_count_get(ump_mem);

	VR_DEBUG_PRINT(4, ("Have %d blocks\n", nr_blocks));

	if (nr_blocks == 0)
	{
		VR_DEBUG_PRINT(1, ("No block count\n"));
		_vr_osk_free( ret_allocation );
		return VR_MEM_ALLOC_INTERNAL_FAILURE;
	}

	ump_blocks = _vr_osk_malloc(sizeof(*ump_blocks)*nr_blocks );
	if ( NULL==ump_blocks )
	{
		_vr_osk_free( ret_allocation );
		return VR_MEM_ALLOC_INTERNAL_FAILURE;
	}

	if (UMP_DD_INVALID == ump_dd_phys_blocks_get(ump_mem, ump_blocks, nr_blocks))
	{
		_vr_osk_free(ump_blocks);
		_vr_osk_free( ret_allocation );
		return VR_MEM_ALLOC_INTERNAL_FAILURE;
	}

	/* Store away the initial offset for unmapping purposes */
	ret_allocation->initial_offset = *offset;

	for(i=0; i<nr_blocks; ++i)
	{
		VR_DEBUG_PRINT(4, ("Mapping in 0x%08x size %d\n", ump_blocks[i].addr , ump_blocks[i].size));
		if (_VR_OSK_ERR_OK != vr_allocation_engine_map_physical(engine, descriptor, *offset, ump_blocks[i].addr , 0, ump_blocks[i].size ))
		{
			u32 size_allocated = *offset - ret_allocation->initial_offset;
			VR_DEBUG_PRINT(1, ("Mapping of external memory failed\n"));

			/* unmap all previous blocks (if any) */
			vr_allocation_engine_unmap_physical(engine, descriptor, ret_allocation->initial_offset, size_allocated, (_vr_osk_mem_mapregion_flags_t)0 );

			_vr_osk_free(ump_blocks);
			_vr_osk_free(ret_allocation);
			return VR_MEM_ALLOC_INTERNAL_FAILURE;
		}
		*offset += ump_blocks[i].size;
	}

	if (descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_GUARD_PAGE)
	{
		/* Map in an extra virtual guard page at the end of the VMA */
		VR_DEBUG_PRINT(4, ("Mapping in extra guard page\n"));
		if (_VR_OSK_ERR_OK != vr_allocation_engine_map_physical(engine, descriptor, *offset, ump_blocks[0].addr , 0, _VR_OSK_VR_PAGE_SIZE ))
		{
			u32 size_allocated = *offset - ret_allocation->initial_offset;
			VR_DEBUG_PRINT(1, ("Mapping of external memory (guard page) failed\n"));

			/* unmap all previous blocks (if any) */
			vr_allocation_engine_unmap_physical(engine, descriptor, ret_allocation->initial_offset, size_allocated, (_vr_osk_mem_mapregion_flags_t)0 );

			_vr_osk_free(ump_blocks);
			_vr_osk_free(ret_allocation);
			return VR_MEM_ALLOC_INTERNAL_FAILURE;
		}
		*offset += _VR_OSK_VR_PAGE_SIZE;
	}

	_vr_osk_free( ump_blocks );

	ret_allocation->engine = engine;
	ret_allocation->descriptor = descriptor;
	ret_allocation->ump_mem = ump_mem;
	ret_allocation->size_allocated = *offset - ret_allocation->initial_offset;

	alloc_info->ctx = NULL;
	alloc_info->handle = ret_allocation;
	alloc_info->next = NULL;
	alloc_info->release = ump_memory_release;

	return VR_MEM_ALLOC_FINISHED;
}

static void ump_memory_release(void * ctx, void * handle)
{
	ump_dd_handle ump_mem;
	ump_mem_allocation *allocation;

	allocation = (ump_mem_allocation *)handle;

	VR_DEBUG_ASSERT_POINTER( allocation );

	ump_mem = allocation->ump_mem;

	VR_DEBUG_ASSERT(UMP_DD_HANDLE_INVALID!=ump_mem);

	/* At present, this is a no-op. But, it allows the vr_address_manager to
	 * do unmapping of a subrange in future. */
	vr_allocation_engine_unmap_physical( allocation->engine,
										   allocation->descriptor,
										   allocation->initial_offset,
										   allocation->size_allocated,
										   (_vr_osk_mem_mapregion_flags_t)0
										   );
	_vr_osk_free( allocation );


	ump_dd_reference_release(ump_mem) ;
	return;
}

_vr_osk_errcode_t _vr_ukk_attach_ump_mem( _vr_uk_attach_ump_mem_s *args )
{
	ump_dd_handle ump_mem;
	vr_physical_memory_allocator external_memory_allocator;
	struct vr_session_data *session_data;
	vr_memory_allocation * descriptor;
	int md;

  	VR_DEBUG_ASSERT_POINTER(args);
  	VR_CHECK_NON_NULL(args->ctx, _VR_OSK_ERR_INVALID_ARGS);

	session_data = (struct vr_session_data *)args->ctx;
	VR_CHECK_NON_NULL(session_data, _VR_OSK_ERR_INVALID_ARGS);

	/* check arguments */
	/* NULL might be a valid VR address */
	if ( ! args->size) VR_ERROR(_VR_OSK_ERR_INVALID_ARGS);

	/* size must be a multiple of the system page size */
	if ( args->size % _VR_OSK_VR_PAGE_SIZE ) VR_ERROR(_VR_OSK_ERR_INVALID_ARGS);

	VR_DEBUG_PRINT(3,
	                 ("Requested to map ump memory with secure id %d into virtual memory 0x%08X, size 0x%08X\n",
	                  args->secure_id, args->vr_address, args->size));

	ump_mem = ump_dd_handle_create_from_secure_id( (int)args->secure_id ) ;

	if ( UMP_DD_HANDLE_INVALID==ump_mem ) VR_ERROR(_VR_OSK_ERR_FAULT);

	descriptor = _vr_osk_calloc(1, sizeof(vr_memory_allocation));
	if (NULL == descriptor)
	{
		ump_dd_reference_release(ump_mem);
		VR_ERROR(_VR_OSK_ERR_NOMEM);
	}

	descriptor->size = args->size;
	descriptor->mapping = NULL;
	descriptor->vr_address = args->vr_address;
	descriptor->vr_addr_mapping_info = (void*)session_data;
	descriptor->process_addr_mapping_info = NULL; /* do not map to process address space */
	descriptor->cache_settings = (u32) VR_CACHE_STANDARD;
	descriptor->lock = session_data->memory_lock;

	if (args->flags & _VR_MAP_EXTERNAL_MAP_GUARD_PAGE)
	{
		descriptor->flags = VR_MEMORY_ALLOCATION_FLAG_MAP_GUARD_PAGE;
	}
	_vr_osk_list_init( &descriptor->list );

	if (_VR_OSK_ERR_OK != vr_descriptor_mapping_allocate_mapping(session_data->descriptor_mapping, descriptor, &md))
	{
		ump_dd_reference_release(ump_mem);
		_vr_osk_free(descriptor);
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}

	external_memory_allocator.allocate = ump_memory_commit;
	external_memory_allocator.allocate_page_table_block = NULL;
	external_memory_allocator.ctx = ump_mem;
	external_memory_allocator.name = "UMP Memory";
	external_memory_allocator.next = NULL;

	_vr_osk_lock_wait(session_data->memory_lock, _VR_OSK_LOCKMODE_RW);

	if (_VR_OSK_ERR_OK != vr_allocation_engine_allocate_memory(memory_engine, descriptor, &external_memory_allocator, NULL))
	{
		_vr_osk_lock_signal(session_data->memory_lock, _VR_OSK_LOCKMODE_RW);
		vr_descriptor_mapping_free(session_data->descriptor_mapping, md);
		ump_dd_reference_release(ump_mem);
		_vr_osk_free(descriptor);
		VR_ERROR(_VR_OSK_ERR_NOMEM);
	}

	_vr_osk_lock_signal(session_data->memory_lock, _VR_OSK_LOCKMODE_RW);

	args->cookie = md;

	VR_DEBUG_PRINT(5,("Returning from UMP attach\n"));

	/* All OK */
	VR_SUCCESS;
}


_vr_osk_errcode_t _vr_ukk_release_ump_mem( _vr_uk_release_ump_mem_s *args )
{
	vr_memory_allocation * descriptor;
	struct vr_session_data *session_data;

	VR_DEBUG_ASSERT_POINTER(args);
	VR_CHECK_NON_NULL(args->ctx, _VR_OSK_ERR_INVALID_ARGS);

	session_data = (struct vr_session_data *)args->ctx;
	VR_CHECK_NON_NULL(session_data, _VR_OSK_ERR_INVALID_ARGS);

	if (_VR_OSK_ERR_OK != vr_descriptor_mapping_get(session_data->descriptor_mapping, args->cookie, (void**)&descriptor))
	{
		VR_DEBUG_PRINT(1, ("Invalid memory descriptor %d used to release ump memory\n", args->cookie));
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}

	descriptor = vr_descriptor_mapping_free(session_data->descriptor_mapping, args->cookie);

	if (NULL != descriptor)
	{
		_vr_osk_lock_wait( session_data->memory_lock, _VR_OSK_LOCKMODE_RW );

		vr_allocation_engine_release_memory(memory_engine, descriptor);

		_vr_osk_lock_signal( session_data->memory_lock, _VR_OSK_LOCKMODE_RW );

		_vr_osk_free(descriptor);
	}

	VR_SUCCESS;

}
#endif /* CONFIG_VR400_UMP */


static vr_physical_memory_allocation_result external_memory_commit(void* ctx, vr_allocation_engine * engine, vr_memory_allocation * descriptor, u32* offset, vr_physical_memory_allocation * alloc_info)
{
	u32 * data;
	external_mem_allocation * ret_allocation;

	VR_DEBUG_ASSERT_POINTER(ctx);
	VR_DEBUG_ASSERT_POINTER(engine);
	VR_DEBUG_ASSERT_POINTER(descriptor);
	VR_DEBUG_ASSERT_POINTER(alloc_info);

	ret_allocation = _vr_osk_malloc( sizeof(external_mem_allocation) );

	if ( NULL == ret_allocation )
	{
		return VR_MEM_ALLOC_INTERNAL_FAILURE;
	}

	data = (u32*)ctx;

	ret_allocation->engine = engine;
	ret_allocation->descriptor = descriptor;
	ret_allocation->initial_offset = *offset;

	alloc_info->ctx = NULL;
	alloc_info->handle = ret_allocation;
	alloc_info->next = NULL;
	alloc_info->release = external_memory_release;

	VR_DEBUG_PRINT(5, ("External map: mapping phys 0x%08X at vr virtual address 0x%08X staring at offset 0x%08X length 0x%08X\n", data[0], descriptor->vr_address, *offset, data[1]));

	if (_VR_OSK_ERR_OK != vr_allocation_engine_map_physical(engine, descriptor, *offset, data[0], 0, data[1]))
	{
		VR_DEBUG_PRINT(1, ("Mapping of external memory failed\n"));
		_vr_osk_free(ret_allocation);
		return VR_MEM_ALLOC_INTERNAL_FAILURE;
	}
	*offset += data[1];

	if (descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_GUARD_PAGE)
	{
		/* Map in an extra virtual guard page at the end of the VMA */
		VR_DEBUG_PRINT(4, ("Mapping in extra guard page\n"));
		if (_VR_OSK_ERR_OK != vr_allocation_engine_map_physical(engine, descriptor, *offset, data[0], 0, _VR_OSK_VR_PAGE_SIZE))
		{
			u32 size_allocated = *offset - ret_allocation->initial_offset;
			VR_DEBUG_PRINT(1, ("Mapping of external memory (guard page) failed\n"));

			/* unmap what we previously mapped */
			vr_allocation_engine_unmap_physical(engine, descriptor, ret_allocation->initial_offset, size_allocated, (_vr_osk_mem_mapregion_flags_t)0 );
			_vr_osk_free(ret_allocation);
			return VR_MEM_ALLOC_INTERNAL_FAILURE;
		}
		*offset += _VR_OSK_VR_PAGE_SIZE;
	}

	ret_allocation->size = *offset - ret_allocation->initial_offset;

	return VR_MEM_ALLOC_FINISHED;
}

static void external_memory_release(void * ctx, void * handle)
{
	external_mem_allocation * allocation;

	allocation = (external_mem_allocation *) handle;
	VR_DEBUG_ASSERT_POINTER( allocation );

	/* At present, this is a no-op. But, it allows the vr_address_manager to
	 * do unmapping of a subrange in future. */

	vr_allocation_engine_unmap_physical( allocation->engine,
										   allocation->descriptor,
										   allocation->initial_offset,
										   allocation->size,
										   (_vr_osk_mem_mapregion_flags_t)0
										   );

	_vr_osk_free( allocation );

	return;
}

_vr_osk_errcode_t _vr_ukk_map_external_mem( _vr_uk_map_external_mem_s *args )
{
	vr_physical_memory_allocator external_memory_allocator;
	struct vr_session_data *session_data;
	u32 info[2];
	vr_memory_allocation * descriptor;
	int md;

	VR_DEBUG_ASSERT_POINTER(args);
	VR_CHECK_NON_NULL(args->ctx, _VR_OSK_ERR_INVALID_ARGS);

	session_data = (struct vr_session_data *)args->ctx;
	VR_CHECK_NON_NULL(session_data, _VR_OSK_ERR_INVALID_ARGS);

	external_memory_allocator.allocate = external_memory_commit;
	external_memory_allocator.allocate_page_table_block = NULL;
	external_memory_allocator.ctx = &info[0];
	external_memory_allocator.name = "External Memory";
	external_memory_allocator.next = NULL;

	/* check arguments */
	/* NULL might be a valid VR address */
	if ( ! args->size) VR_ERROR(_VR_OSK_ERR_INVALID_ARGS);

	/* size must be a multiple of the system page size */
	if ( args->size % _VR_OSK_VR_PAGE_SIZE ) VR_ERROR(_VR_OSK_ERR_INVALID_ARGS);

	VR_DEBUG_PRINT(3,
	        ("Requested to map physical memory 0x%x-0x%x into virtual memory 0x%x\n",
	        (void*)args->phys_addr,
	        (void*)(args->phys_addr + args->size -1),
	        (void*)args->vr_address)
	);

	/* Validate the vr physical range */
	if (_VR_OSK_ERR_OK != vr_mem_validation_check(args->phys_addr, args->size))
	{
		return _VR_OSK_ERR_FAULT;
	}

	info[0] = args->phys_addr;
	info[1] = args->size;

	descriptor = _vr_osk_calloc(1, sizeof(vr_memory_allocation));
	if (NULL == descriptor) VR_ERROR(_VR_OSK_ERR_NOMEM);

	descriptor->size = args->size;
	descriptor->mapping = NULL;
	descriptor->vr_address = args->vr_address;
	descriptor->vr_addr_mapping_info = (void*)session_data;
	descriptor->process_addr_mapping_info = NULL; /* do not map to process address space */
	descriptor->cache_settings = (u32)VR_CACHE_STANDARD;
	descriptor->lock = session_data->memory_lock;
	if (args->flags & _VR_MAP_EXTERNAL_MAP_GUARD_PAGE)
	{
		descriptor->flags = VR_MEMORY_ALLOCATION_FLAG_MAP_GUARD_PAGE;
	}
	_vr_osk_list_init( &descriptor->list );

	_vr_osk_lock_wait(session_data->memory_lock, _VR_OSK_LOCKMODE_RW);

	if (_VR_OSK_ERR_OK != vr_allocation_engine_allocate_memory(memory_engine, descriptor, &external_memory_allocator, NULL))
	{
		_vr_osk_lock_signal(session_data->memory_lock, _VR_OSK_LOCKMODE_RW);
		_vr_osk_free(descriptor);
		VR_ERROR(_VR_OSK_ERR_NOMEM);
	}

	_vr_osk_lock_signal(session_data->memory_lock, _VR_OSK_LOCKMODE_RW);

	if (_VR_OSK_ERR_OK != vr_descriptor_mapping_allocate_mapping(session_data->descriptor_mapping, descriptor, &md))
	{
		vr_allocation_engine_release_memory(memory_engine, descriptor);
		_vr_osk_free(descriptor);
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}

	args->cookie = md;

	VR_DEBUG_PRINT(5,("Returning from range_map_external_memory\n"));

	/* All OK */
	VR_SUCCESS;
}


_vr_osk_errcode_t _vr_ukk_unmap_external_mem( _vr_uk_unmap_external_mem_s *args )
{
	vr_memory_allocation * descriptor;
	void* old_value;
	struct vr_session_data *session_data;

	VR_DEBUG_ASSERT_POINTER(args);
	VR_CHECK_NON_NULL(args->ctx, _VR_OSK_ERR_INVALID_ARGS);

	session_data = (struct vr_session_data *)args->ctx;
	VR_CHECK_NON_NULL(session_data, _VR_OSK_ERR_INVALID_ARGS);

	if (_VR_OSK_ERR_OK != vr_descriptor_mapping_get(session_data->descriptor_mapping, args->cookie, (void**)&descriptor))
	{
		VR_DEBUG_PRINT(1, ("Invalid memory descriptor %d used to unmap external memory\n", args->cookie));
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}

	old_value = vr_descriptor_mapping_free(session_data->descriptor_mapping, args->cookie);

	if (NULL != old_value)
	{
		_vr_osk_lock_wait( session_data->memory_lock, _VR_OSK_LOCKMODE_RW );

		vr_allocation_engine_release_memory(memory_engine, descriptor);

		_vr_osk_lock_signal( session_data->memory_lock, _VR_OSK_LOCKMODE_RW );

		_vr_osk_free(descriptor);
	}

	VR_SUCCESS;
}

_vr_osk_errcode_t _vr_ukk_init_mem( _vr_uk_init_mem_s *args )
{
	VR_DEBUG_ASSERT_POINTER(args);
	VR_CHECK_NON_NULL(args->ctx, _VR_OSK_ERR_INVALID_ARGS);

	args->memory_size = 2 * 1024 * 1024 * 1024UL; /* 2GB address space */
	args->vr_address_base = 1 * 1024 * 1024 * 1024UL; /* staring at 1GB, causing this layout: (0-1GB unused)(1GB-3G usage by VR)(3G-4G unused) */
	VR_SUCCESS;
}

_vr_osk_errcode_t _vr_ukk_term_mem( _vr_uk_term_mem_s *args )
{
	VR_DEBUG_ASSERT_POINTER(args);
	VR_CHECK_NON_NULL(args->ctx, _VR_OSK_ERR_INVALID_ARGS);
	VR_SUCCESS;
}

static _vr_osk_errcode_t vr_address_manager_allocate(vr_memory_allocation * descriptor)
{
	struct vr_session_data *session_data;
	u32 actual_size;

	VR_DEBUG_ASSERT_POINTER(descriptor);

	session_data = (struct vr_session_data *)descriptor->vr_addr_mapping_info;

	actual_size = descriptor->size;

	if (descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_GUARD_PAGE)
	{
		actual_size += _VR_OSK_VR_PAGE_SIZE;
	}

	return vr_mmu_pagedir_map(session_data->page_directory, descriptor->vr_address, actual_size);
}

static void vr_address_manager_release(vr_memory_allocation * descriptor)
{
	const u32 illegal_vr_address = 0xffffffff;
	struct vr_session_data *session_data;
	VR_DEBUG_ASSERT_POINTER(descriptor);

	/* It is allowed to call this function several times on the same descriptor.
	   When memory is released we set the illegal_vr_address so we can early out here. */
	if ( illegal_vr_address == descriptor->vr_address) return;

	session_data = (struct vr_session_data *)descriptor->vr_addr_mapping_info;
	vr_mmu_pagedir_unmap(session_data->page_directory, descriptor->vr_address, descriptor->size);

	descriptor->vr_address = illegal_vr_address ;
}

static _vr_osk_errcode_t vr_address_manager_map(vr_memory_allocation * descriptor, u32 offset, u32 *phys_addr, u32 size)
{
	struct vr_session_data *session_data;
	u32 vr_address;

	VR_DEBUG_ASSERT_POINTER(descriptor);
	VR_DEBUG_ASSERT_POINTER(phys_addr);

	session_data = (struct vr_session_data *)descriptor->vr_addr_mapping_info;
	VR_DEBUG_ASSERT_POINTER(session_data);

	vr_address = descriptor->vr_address + offset;

	VR_DEBUG_PRINT(7, ("VR map: mapping 0x%08X to VR address 0x%08X length 0x%08X\n", *phys_addr, vr_address, size));

	vr_mmu_pagedir_update(session_data->page_directory, vr_address, *phys_addr, size, descriptor->cache_settings);

	VR_SUCCESS;
}

/* This handler registered to vr_mmap for MMU builds */
_vr_osk_errcode_t _vr_ukk_mem_mmap( _vr_uk_mem_mmap_s *args )
{
	struct vr_session_data *session_data;
	vr_memory_allocation * descriptor;

	/* validate input */
	if (NULL == args) { VR_DEBUG_PRINT(3,("vr_ukk_mem_mmap: args was NULL\n")); VR_ERROR(_VR_OSK_ERR_INVALID_ARGS); }

	/* Unpack arguments */
	session_data = (struct vr_session_data *)args->ctx;

	/* validate input */
	if (NULL == session_data) { VR_DEBUG_PRINT(3,("vr_ukk_mem_mmap: session data was NULL\n")); VR_ERROR(_VR_OSK_ERR_FAULT); }

	descriptor = (vr_memory_allocation*) _vr_osk_calloc( 1, sizeof(vr_memory_allocation) );
	if (NULL == descriptor) { VR_DEBUG_PRINT(3,("vr_ukk_mem_mmap: descriptor was NULL\n")); VR_ERROR(_VR_OSK_ERR_NOMEM); }

	descriptor->size = args->size;
	descriptor->vr_address = args->phys_addr;
	descriptor->vr_addr_mapping_info = (void*)session_data;

	descriptor->process_addr_mapping_info = args->ukk_private; /* save to be used during physical manager callback */
	descriptor->flags = VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE;
	descriptor->cache_settings = (u32) args->cache_settings ;
	descriptor->lock = session_data->memory_lock;
	_vr_osk_list_init( &descriptor->list );

	_vr_osk_lock_wait(session_data->memory_lock, _VR_OSK_LOCKMODE_RW);

	if (0 == vr_allocation_engine_allocate_memory(memory_engine, descriptor, physical_memory_allocators, &session_data->memory_head))
	{
		/* We do not FLUSH nor TLB_ZAP on MMAP, since we do both of those on job start*/
	   	_vr_osk_lock_signal(session_data->memory_lock, _VR_OSK_LOCKMODE_RW);

		args->mapping = descriptor->mapping;
		args->cookie = (u32)descriptor;

		VR_DEBUG_PRINT(7, ("MMAP OK\n"));
		VR_SUCCESS;
	}
	else
	{
	   	_vr_osk_lock_signal(session_data->memory_lock, _VR_OSK_LOCKMODE_RW);
		/* OOM, but not a fatal error */
		VR_DEBUG_PRINT(4, ("Memory allocation failure, OOM\n"));
		_vr_osk_free(descriptor);
		/* Linux will free the CPU address allocation, userspace client the VR address allocation */
		VR_ERROR(_VR_OSK_ERR_FAULT);
	}
}

static _vr_osk_errcode_t _vr_ukk_mem_munmap_internal( _vr_uk_mem_munmap_s *args )
{
	struct vr_session_data *session_data;
	vr_memory_allocation * descriptor;

	descriptor = (vr_memory_allocation *)args->cookie;
	VR_DEBUG_ASSERT_POINTER(descriptor);

	/** @note args->context unused; we use the memory_session from the cookie */
	/* args->mapping and args->size are also discarded. They are only necessary
	   for certain do_munmap implementations. However, they could be used to check the
	   descriptor at this point. */

	session_data = (struct vr_session_data *)descriptor->vr_addr_mapping_info;
	VR_DEBUG_ASSERT_POINTER(session_data);

	/* Unmapping the memory from the vr virtual address space.
	   It is allowed to call this function severeal times, which might happen if zapping below fails. */
	vr_allocation_engine_release_pt1_vr_pagetables_unmap(memory_engine, descriptor);

#ifdef VR_UNMAP_FLUSH_ALL_VR_L2
	{
		u32 i;
		u32 number_of_l2_ccores = vr_l2_cache_core_get_glob_num_l2_cores();
		for (i = 0; i < number_of_l2_ccores; i++)
		{
			struct vr_l2_cache_core *core;
			core = vr_l2_cache_core_get_glob_l2_core(i);
			if (vr_l2_cache_power_is_enabled_get(core) )
			{
				vr_l2_cache_invalidate_all_force(core);
			}
		}
	}
#endif

	vr_scheduler_zap_all_active(session_data);

	/* Removes the descriptor from the session's memory list, releases physical memory, releases descriptor */
	vr_allocation_engine_release_pt2_physical_memory_free(memory_engine, descriptor);

	_vr_osk_free(descriptor);

	return _VR_OSK_ERR_OK;
}

/* Handler for unmapping memory for MMU builds */
_vr_osk_errcode_t _vr_ukk_mem_munmap( _vr_uk_mem_munmap_s *args )
{
	vr_memory_allocation * descriptor;
	_vr_osk_lock_t *descriptor_lock;
	_vr_osk_errcode_t err;

	descriptor = (vr_memory_allocation *)args->cookie;
	VR_DEBUG_ASSERT_POINTER(descriptor);

	/** @note args->context unused; we use the memory_session from the cookie */
	/* args->mapping and args->size are also discarded. They are only necessary
	for certain do_munmap implementations. However, they could be used to check the
	descriptor at this point. */

	VR_DEBUG_ASSERT_POINTER((struct vr_session_data *)descriptor->vr_addr_mapping_info);

	descriptor_lock = descriptor->lock; /* should point to the session data lock... */

	err = _VR_OSK_ERR_BUSY;
	while (err == _VR_OSK_ERR_BUSY)
	{
		if (descriptor_lock)
		{
			_vr_osk_lock_wait( descriptor_lock, _VR_OSK_LOCKMODE_RW );
		}

		err = _vr_ukk_mem_munmap_internal( args );

		if (descriptor_lock)
		{
			_vr_osk_lock_signal( descriptor_lock, _VR_OSK_LOCKMODE_RW );
		}

		if (err == _VR_OSK_ERR_BUSY)
		{
			/*
			 * Reason for this;
			 * We where unable to stall the MMU, probably because we are in page fault handling.
			 * Sleep for a while with the session lock released, then try again.
			 * Abnormal termination of programs with running VR jobs is a normal reason for this.
			 */
			_vr_osk_time_ubusydelay(10);
		}
	}

	return err;
}

u32 _vr_ukk_report_memory_usage(void)
{
	return vr_allocation_engine_memory_usage(physical_memory_allocators);
}

_vr_osk_errcode_t vr_mmu_get_table_page(u32 *table_page, vr_io_address *mapping)
{
	_vr_osk_lock_wait(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);

	if (!_vr_osk_list_empty(&page_table_cache.partial))
	{
		vr_mmu_page_table_allocation * alloc = _VR_OSK_LIST_ENTRY(page_table_cache.partial.next, vr_mmu_page_table_allocation, list);
		int page_number = _vr_osk_find_first_zero_bit(alloc->usage_map, alloc->num_pages);
		VR_DEBUG_PRINT(6, ("Partial page table allocation found, using page offset %d\n", page_number));
		_vr_osk_set_nonatomic_bit(page_number, alloc->usage_map);
		alloc->usage_count++;
		if (alloc->num_pages == alloc->usage_count)
		{
			/* full, move alloc to full list*/
			_vr_osk_list_move(&alloc->list, &page_table_cache.full);
		}
		_vr_osk_lock_signal(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);

		*table_page = (VR_MMU_PAGE_SIZE * page_number) + alloc->pages.phys_base;
		*mapping =  (vr_io_address)((VR_MMU_PAGE_SIZE * page_number) + (u32)alloc->pages.mapping);
		VR_DEBUG_PRINT(4, ("Page table allocated for VA=0x%08X, VRPA=0x%08X\n", *mapping, *table_page ));
		VR_SUCCESS;
	}
	else
	{
		vr_mmu_page_table_allocation * alloc;
		/* no free pages, allocate a new one */

		alloc = (vr_mmu_page_table_allocation *)_vr_osk_calloc(1, sizeof(vr_mmu_page_table_allocation));
		if (NULL == alloc)
		{
			_vr_osk_lock_signal(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);
			*table_page = VR_INVALID_PAGE;
			VR_ERROR(_VR_OSK_ERR_NOMEM);
		}

		_VR_OSK_INIT_LIST_HEAD(&alloc->list);

		if (_VR_OSK_ERR_OK != vr_allocation_engine_allocate_page_tables(memory_engine, &alloc->pages, physical_memory_allocators))
		{
			VR_DEBUG_PRINT(1, ("No more memory for page tables\n"));
			_vr_osk_free(alloc);
			_vr_osk_lock_signal(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);
			*table_page = VR_INVALID_PAGE;
			*mapping = NULL;
			VR_ERROR(_VR_OSK_ERR_NOMEM);
		}

		/* create the usage map */
		alloc->num_pages = alloc->pages.size / VR_MMU_PAGE_SIZE;
		alloc->usage_count = 1;
		VR_DEBUG_PRINT(3, ("New page table cache expansion, %d pages in new cache allocation\n", alloc->num_pages));
		alloc->usage_map = _vr_osk_calloc(1, ((alloc->num_pages + BITS_PER_LONG - 1) & ~(BITS_PER_LONG-1) / BITS_PER_LONG) * sizeof(unsigned long));
		if (NULL == alloc->usage_map)
		{
			VR_DEBUG_PRINT(1, ("Failed to allocate memory to describe MMU page table cache usage\n"));
			alloc->pages.release(&alloc->pages);
			_vr_osk_free(alloc);
			_vr_osk_lock_signal(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);
			*table_page = VR_INVALID_PAGE;
			*mapping = NULL;
			VR_ERROR(_VR_OSK_ERR_NOMEM);
		}

		_vr_osk_set_nonatomic_bit(0, alloc->usage_map);

		if (alloc->num_pages > 1)
		{
			_vr_osk_list_add(&alloc->list, &page_table_cache.partial);
		}
		else
		{
			_vr_osk_list_add(&alloc->list, &page_table_cache.full);
		}

		_vr_osk_lock_signal(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);
		*table_page = alloc->pages.phys_base; /* return the first page */
		*mapping = alloc->pages.mapping; /* Mapping for first page */
		VR_DEBUG_PRINT(4, ("Page table allocated: VA=0x%08X, VRPA=0x%08X\n", *mapping, *table_page ));
		VR_SUCCESS;
	}
}

void vr_mmu_release_table_page(u32 pa)
{
	vr_mmu_page_table_allocation * alloc, * temp_alloc;

	VR_DEBUG_PRINT_IF(1, pa & 4095, ("Bad page address 0x%x given to vr_mmu_release_table_page\n", (void*)pa));

	VR_DEBUG_PRINT(4, ("Releasing table page 0x%08X to the cache\n", pa));

   	_vr_osk_lock_wait(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);

	/* find the entry this address belongs to */
	/* first check the partial list */
	_VR_OSK_LIST_FOREACHENTRY(alloc, temp_alloc, &page_table_cache.partial, vr_mmu_page_table_allocation, list)
	{
		u32 start = alloc->pages.phys_base;
		u32 last = start + (alloc->num_pages - 1) * VR_MMU_PAGE_SIZE;
		if (pa >= start && pa <= last)
		{
			VR_DEBUG_ASSERT(0 != _vr_osk_test_bit((pa - start)/VR_MMU_PAGE_SIZE, alloc->usage_map));
			_vr_osk_clear_nonatomic_bit((pa - start)/VR_MMU_PAGE_SIZE, alloc->usage_map);
			alloc->usage_count--;

			_vr_osk_memset((void*)( ((u32)alloc->pages.mapping) + (pa - start) ), 0, VR_MMU_PAGE_SIZE);

			if (0 == alloc->usage_count)
			{
				/* empty, release whole page alloc */
				_vr_osk_list_del(&alloc->list);
				alloc->pages.release(&alloc->pages);
				_vr_osk_free(alloc->usage_map);
				_vr_osk_free(alloc);
			}
		   	_vr_osk_lock_signal(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);
			VR_DEBUG_PRINT(4, ("(partial list)Released table page 0x%08X to the cache\n", pa));
			return;
		}
	}

	/* the check the full list */
	_VR_OSK_LIST_FOREACHENTRY(alloc, temp_alloc, &page_table_cache.full, vr_mmu_page_table_allocation, list)
	{
		u32 start = alloc->pages.phys_base;
		u32 last = start + (alloc->num_pages - 1) * VR_MMU_PAGE_SIZE;
		if (pa >= start && pa <= last)
		{
			_vr_osk_clear_nonatomic_bit((pa - start)/VR_MMU_PAGE_SIZE, alloc->usage_map);
			alloc->usage_count--;

			_vr_osk_memset((void*)( ((u32)alloc->pages.mapping) + (pa - start) ), 0, VR_MMU_PAGE_SIZE);


			if (0 == alloc->usage_count)
			{
				/* empty, release whole page alloc */
				_vr_osk_list_del(&alloc->list);
				alloc->pages.release(&alloc->pages);
				_vr_osk_free(alloc->usage_map);
				_vr_osk_free(alloc);
			}
			else
			{
				/* transfer to partial list */
				_vr_osk_list_move(&alloc->list, &page_table_cache.partial);
			}

		   	_vr_osk_lock_signal(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);
			VR_DEBUG_PRINT(4, ("(full list)Released table page 0x%08X to the cache\n", pa));
			return;
		}
	}

	VR_DEBUG_PRINT(1, ("pa 0x%x not found in the page table cache\n", (void*)pa));

   	_vr_osk_lock_signal(page_table_cache.lock, _VR_OSK_LOCKMODE_RW);
}

static _vr_osk_errcode_t vr_mmu_page_table_cache_create(void)
{
	page_table_cache.lock = _vr_osk_lock_init( _VR_OSK_LOCKFLAG_ORDERED | _VR_OSK_LOCKFLAG_ONELOCK
	                            | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE, 0, _VR_OSK_LOCK_ORDER_MEM_PT_CACHE);
	VR_CHECK_NON_NULL( page_table_cache.lock, _VR_OSK_ERR_FAULT );
	_VR_OSK_INIT_LIST_HEAD(&page_table_cache.partial);
	_VR_OSK_INIT_LIST_HEAD(&page_table_cache.full);
	VR_SUCCESS;
}

static void vr_mmu_page_table_cache_destroy(void)
{
	vr_mmu_page_table_allocation * alloc, *temp;

	_VR_OSK_LIST_FOREACHENTRY(alloc, temp, &page_table_cache.partial, vr_mmu_page_table_allocation, list)
	{
		VR_DEBUG_PRINT_IF(1, 0 != alloc->usage_count, ("Destroying page table cache while pages are tagged as in use. %d allocations still marked as in use.\n", alloc->usage_count));
		_vr_osk_list_del(&alloc->list);
		alloc->pages.release(&alloc->pages);
		_vr_osk_free(alloc->usage_map);
		_vr_osk_free(alloc);
	}

	VR_DEBUG_PRINT_IF(1, !_vr_osk_list_empty(&page_table_cache.full), ("Page table cache full list contains one or more elements \n"));

	_VR_OSK_LIST_FOREACHENTRY(alloc, temp, &page_table_cache.full, vr_mmu_page_table_allocation, list)
	{
		VR_DEBUG_PRINT(1, ("Destroy alloc 0x%08X with usage count %d\n", (u32)alloc, alloc->usage_count));
		_vr_osk_list_del(&alloc->list);
		alloc->pages.release(&alloc->pages);
		_vr_osk_free(alloc->usage_map);
		_vr_osk_free(alloc);
	}

	_vr_osk_lock_term(page_table_cache.lock);
}
