/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "vr_kernel_common.h"
#include "vr_kernel_memory_engine.h"
#include "vr_osk.h"
#include "vr_osk_list.h"

typedef struct memory_engine
{
	vr_kernel_mem_address_manager * vr_address;
	vr_kernel_mem_address_manager * process_address;
} memory_engine;

vr_allocation_engine vr_allocation_engine_create(vr_kernel_mem_address_manager * vr_address_manager, vr_kernel_mem_address_manager * process_address_manager)
{
	memory_engine * engine;

	/* VR Address Manager need not support unmap_physical */
	VR_DEBUG_ASSERT_POINTER(vr_address_manager);
	VR_DEBUG_ASSERT_POINTER(vr_address_manager->allocate);
	VR_DEBUG_ASSERT_POINTER(vr_address_manager->release);
	VR_DEBUG_ASSERT_POINTER(vr_address_manager->map_physical);

	/* Process Address Manager must support unmap_physical for OS allocation
	 * error path handling */
	VR_DEBUG_ASSERT_POINTER(process_address_manager);
	VR_DEBUG_ASSERT_POINTER(process_address_manager->allocate);
	VR_DEBUG_ASSERT_POINTER(process_address_manager->release);
	VR_DEBUG_ASSERT_POINTER(process_address_manager->map_physical);
	VR_DEBUG_ASSERT_POINTER(process_address_manager->unmap_physical);


	engine = (memory_engine*)_vr_osk_malloc(sizeof(memory_engine));
	if (NULL == engine) return NULL;

	engine->vr_address = vr_address_manager;
	engine->process_address = process_address_manager;

	return (vr_allocation_engine)engine;
}

void vr_allocation_engine_destroy(vr_allocation_engine engine)
{
	VR_DEBUG_ASSERT_POINTER(engine);
	_vr_osk_free(engine);
}

_vr_osk_errcode_t vr_allocation_engine_allocate_memory(vr_allocation_engine mem_engine, vr_memory_allocation * descriptor, vr_physical_memory_allocator * physical_allocators, _vr_osk_list_t *tracking_list )
{
	memory_engine * engine = (memory_engine*)mem_engine;

	VR_DEBUG_ASSERT_POINTER(engine);
	VR_DEBUG_ASSERT_POINTER(descriptor);
	VR_DEBUG_ASSERT_POINTER(physical_allocators);
	/* ASSERT that the list member has been initialized, even if it won't be
	 * used for tracking. We need it to be initialized to see if we need to
	 * delete it from a list in the release function. */
	VR_DEBUG_ASSERT( NULL != descriptor->list.next && NULL != descriptor->list.prev );

	if (_VR_OSK_ERR_OK == engine->vr_address->allocate(descriptor))
	{
		_vr_osk_errcode_t res = _VR_OSK_ERR_OK;
		if ( descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE )
		{
			res = engine->process_address->allocate(descriptor);
		}
		if ( _VR_OSK_ERR_OK == res )
		{
			/* address space setup OK, commit physical memory to the allocation */
			vr_physical_memory_allocator * active_allocator = physical_allocators;
			struct vr_physical_memory_allocation * active_allocation_tracker = &descriptor->physical_allocation;
			u32 offset = 0;

			while ( NULL != active_allocator )
			{
				switch (active_allocator->allocate(active_allocator->ctx, mem_engine, descriptor, &offset, active_allocation_tracker))
				{
					case VR_MEM_ALLOC_FINISHED:
						if ( NULL != tracking_list )
						{
							/* Insert into the memory session list */
							/* ASSERT that it is not already part of a list */
							VR_DEBUG_ASSERT( _vr_osk_list_empty( &descriptor->list ) );
							_vr_osk_list_add( &descriptor->list, tracking_list );
						}

						VR_SUCCESS; /* all done */
					case VR_MEM_ALLOC_NONE:
						/* reuse current active_allocation_tracker */
						VR_DEBUG_PRINT( 4, ("Memory Engine Allocate: No allocation on %s, resorting to %s\n",
											  ( active_allocator->name ) ? active_allocator->name : "UNNAMED",
											  ( active_allocator->next ) ? (( active_allocator->next->name )? active_allocator->next->name : "UNNAMED") : "NONE") );
						active_allocator = active_allocator->next;
						break;
					case VR_MEM_ALLOC_PARTIAL:
						if (NULL != active_allocator->next)
						{
							/* need a new allocation tracker */
							active_allocation_tracker->next = _vr_osk_calloc(1, sizeof(vr_physical_memory_allocation));
							if (NULL != active_allocation_tracker->next)
							{
								active_allocation_tracker = active_allocation_tracker->next;
								VR_DEBUG_PRINT( 2, ("Memory Engine Allocate: Partial allocation on %s, resorting to %s\n",
													  ( active_allocator->name ) ? active_allocator->name : "UNNAMED",
													  ( active_allocator->next ) ? (( active_allocator->next->name )? active_allocator->next->name : "UNNAMED") : "NONE") );
								active_allocator = active_allocator->next;
								break;
							}
						}
					   /* FALL THROUGH */
					case VR_MEM_ALLOC_INTERNAL_FAILURE:
					   active_allocator = NULL; /* end the while loop */
					   break;
				}
			}

			VR_PRINT(("Memory allocate failed, could not allocate size %d kB.\n", descriptor->size/1024));

			/* allocation failure, start cleanup */
			/* loop over any potential partial allocations */
			active_allocation_tracker = &descriptor->physical_allocation;
			while (NULL != active_allocation_tracker)
			{
				/* handle blank trackers which will show up during failure */
				if (NULL != active_allocation_tracker->release)
				{
					active_allocation_tracker->release(active_allocation_tracker->ctx, active_allocation_tracker->handle);
				}
				active_allocation_tracker = active_allocation_tracker->next;
			}

			/* free the allocation tracker objects themselves, skipping the tracker stored inside the descriptor itself */
			for ( active_allocation_tracker = descriptor->physical_allocation.next; active_allocation_tracker != NULL; )
			{
				void * buf = active_allocation_tracker;
				active_allocation_tracker = active_allocation_tracker->next;
				_vr_osk_free(buf);
			}

			/* release the address spaces */

			if ( descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE )
			{
				engine->process_address->release(descriptor);
			}
		}
		engine->vr_address->release(descriptor);
	}

	VR_ERROR(_VR_OSK_ERR_FAULT);
}

void vr_allocation_engine_release_memory(vr_allocation_engine mem_engine, vr_memory_allocation * descriptor)
{
	vr_allocation_engine_release_pt1_vr_pagetables_unmap(mem_engine, descriptor);
	vr_allocation_engine_release_pt2_physical_memory_free(mem_engine, descriptor);
}

void vr_allocation_engine_release_pt1_vr_pagetables_unmap(vr_allocation_engine mem_engine, vr_memory_allocation * descriptor)
{
	memory_engine * engine = (memory_engine*)mem_engine;

	VR_DEBUG_ASSERT_POINTER(engine);
	VR_DEBUG_ASSERT_POINTER(descriptor);

	/* Calling: vr_address_manager_release()  */
	/* This function is allowed to be called several times, and it only does the release on the first call. */
	engine->vr_address->release(descriptor);
}

void vr_allocation_engine_release_pt2_physical_memory_free(vr_allocation_engine mem_engine, vr_memory_allocation * descriptor)
{
	memory_engine * engine = (memory_engine*)mem_engine;
	vr_physical_memory_allocation * active_allocation_tracker;

	/* Remove this from a tracking list in session_data->memory_head */
	if ( ! _vr_osk_list_empty( &descriptor->list ) )
	{
		_vr_osk_list_del( &descriptor->list );
		/* Clear the list for debug mode, catch use-after-free */
		VR_DEBUG_CODE( descriptor->list.next = descriptor->list.prev = NULL; )
	}

	active_allocation_tracker = &descriptor->physical_allocation;
	while (NULL != active_allocation_tracker)
	{
		VR_DEBUG_ASSERT_POINTER(active_allocation_tracker->release);
		active_allocation_tracker->release(active_allocation_tracker->ctx, active_allocation_tracker->handle);
		active_allocation_tracker = active_allocation_tracker->next;
	}

	/* free the allocation tracker objects themselves, skipping the tracker stored inside the descriptor itself */
	for ( active_allocation_tracker = descriptor->physical_allocation.next; active_allocation_tracker != NULL; )
	{
		void * buf = active_allocation_tracker;
		active_allocation_tracker = active_allocation_tracker->next;
		_vr_osk_free(buf);
	}

	if ( descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE )
	{
		engine->process_address->release(descriptor);
	}
}

_vr_osk_errcode_t vr_allocation_engine_map_physical(vr_allocation_engine mem_engine, vr_memory_allocation * descriptor, u32 offset, u32 phys, u32 cpu_usage_adjust, u32 size)
{
	_vr_osk_errcode_t err;
	memory_engine * engine = (memory_engine*)mem_engine;
	_vr_osk_mem_mapregion_flags_t unmap_flags = (_vr_osk_mem_mapregion_flags_t)0;

	VR_DEBUG_ASSERT_POINTER(engine);
	VR_DEBUG_ASSERT_POINTER(descriptor);

	VR_DEBUG_PRINT(7, ("Mapping phys 0x%08X length 0x%08X at offset 0x%08X\n", phys, size, offset));

	VR_DEBUG_ASSERT_POINTER(engine->vr_address);
	VR_DEBUG_ASSERT_POINTER(engine->vr_address->map_physical);

	/* Handle process address manager first, because we may need them to
	 * allocate the physical page */
	if ( descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE )
	{
		/* Handle OS-allocated specially, since an adjustment may be required */
		if ( VR_MEMORY_ALLOCATION_OS_ALLOCATED_PHYSADDR_MAGIC == phys )
		{
			VR_DEBUG_ASSERT( _VR_OSK_CPU_PAGE_SIZE == size );

			/* Set flags to use on error path */
			unmap_flags |= _VR_OSK_MEM_MAPREGION_FLAG_OS_ALLOCATED_PHYSADDR;

			err = engine->process_address->map_physical(descriptor, offset, &phys, size);
			/* Adjust for cpu physical address to vr physical address */
			phys -= cpu_usage_adjust;
		}
		else
		{
			u32 cpu_phys;
			/* Adjust vr physical address to cpu physical address */
			cpu_phys = phys + cpu_usage_adjust;
			err = engine->process_address->map_physical(descriptor, offset, &cpu_phys, size);
		}

		if ( _VR_OSK_ERR_OK != err )
		{
			VR_DEBUG_PRINT(2, ("Map failed: %s %d\n", __FUNCTION__, __LINE__));
			VR_ERROR( err );
		}
	}

	VR_DEBUG_PRINT(7, ("Mapping phys 0x%08X length 0x%08X at offset 0x%08X to CPUVA 0x%08X\n", phys, size, offset, (u32)(descriptor->mapping) + offset));

	/* VR address manager must use the physical address - no point in asking
	 * it to allocate another one for us */
	VR_DEBUG_ASSERT( VR_MEMORY_ALLOCATION_OS_ALLOCATED_PHYSADDR_MAGIC != phys );

	err = engine->vr_address->map_physical(descriptor, offset, &phys, size);

	if ( _VR_OSK_ERR_OK != err )
	{
		if ( descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE )
		{
			VR_DEBUG_PRINT( 2, ("Process address manager succeeded, but VR Address manager failed for phys=0x%08X size=0x%08X, offset=0x%08X. Will unmap.\n", phys, size, offset));
			engine->process_address->unmap_physical(descriptor, offset, size, unmap_flags);
		}
		VR_DEBUG_PRINT(2, ("Map vr failed: %s %d\n", __FUNCTION__, __LINE__));
		VR_ERROR( err );
	}

	VR_SUCCESS;
}

void vr_allocation_engine_unmap_physical(vr_allocation_engine mem_engine, vr_memory_allocation * descriptor, u32 offset, u32 size, _vr_osk_mem_mapregion_flags_t unmap_flags )
{
	memory_engine * engine = (memory_engine*)mem_engine;

	VR_DEBUG_ASSERT_POINTER(engine);
	VR_DEBUG_ASSERT_POINTER(descriptor);

	VR_DEBUG_PRINT(7, ("UnMapping length 0x%08X at offset 0x%08X\n", size, offset));

	VR_DEBUG_ASSERT_POINTER(engine->vr_address);
	VR_DEBUG_ASSERT_POINTER(engine->process_address);

	if ( descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE )
	{
		/* Mandetory for process_address manager to have an unmap function*/
		engine->process_address->unmap_physical( descriptor, offset, size, unmap_flags );
	}

	/* Optional for vr_address manager to have an unmap function*/
	if ( NULL != engine->vr_address->unmap_physical )
	{
		engine->vr_address->unmap_physical( descriptor, offset, size, unmap_flags );
	}
}


_vr_osk_errcode_t vr_allocation_engine_allocate_page_tables(vr_allocation_engine engine, vr_page_table_block * descriptor, vr_physical_memory_allocator * physical_provider)
{
	vr_physical_memory_allocator * active_allocator = physical_provider;

	VR_DEBUG_ASSERT_POINTER(descriptor);
	VR_DEBUG_ASSERT_POINTER(physical_provider);

	while ( NULL != active_allocator )
	{
		switch (active_allocator->allocate_page_table_block(active_allocator->ctx, descriptor))
		{
			case VR_MEM_ALLOC_FINISHED:
				VR_SUCCESS; /* all done */
			case VR_MEM_ALLOC_NONE:
				/* try next */
				VR_DEBUG_PRINT( 2, ("Memory Engine Allocate PageTables: No allocation on %s, resorting to %s\n",
									  ( active_allocator->name ) ? active_allocator->name : "UNNAMED",
									  ( active_allocator->next ) ? (( active_allocator->next->name )? active_allocator->next->name : "UNNAMED") : "NONE") );
				active_allocator = active_allocator->next;
				break;
			case VR_MEM_ALLOC_PARTIAL:
				VR_DEBUG_PRINT(1, ("Invalid return value from allocate_page_table_block call: VR_MEM_ALLOC_PARTIAL\n"));
				/* FALL THROUGH */
			case VR_MEM_ALLOC_INTERNAL_FAILURE:
				VR_DEBUG_PRINT(1, ("Aborting due to allocation failure\n"));
				active_allocator = NULL; /* end the while loop */
				break;
		}
	}

	VR_ERROR(_VR_OSK_ERR_FAULT);
}


void vr_allocation_engine_report_allocators( vr_physical_memory_allocator * physical_provider )
{
	vr_physical_memory_allocator * active_allocator = physical_provider;
	VR_DEBUG_ASSERT_POINTER(physical_provider);

	VR_DEBUG_PRINT( 1, ("VR memory allocators will be used in this order of preference (lowest numbered first) :\n"));
	while ( NULL != active_allocator )
	{
		if ( NULL != active_allocator->name )
		{
			VR_DEBUG_PRINT( 1, ("\t%d: %s\n", active_allocator->alloc_order, active_allocator->name) );
		}
		else
		{
			VR_DEBUG_PRINT( 1, ("\t%d: (UNNAMED ALLOCATOR)\n", active_allocator->alloc_order) );
		}
		active_allocator = active_allocator->next;
	}

}

u32 vr_allocation_engine_memory_usage(vr_physical_memory_allocator *allocator)
{
	u32 sum = 0;
	while(NULL != allocator)
	{
		/* Only count allocators that have set up a stat function. */
		if(allocator->stat)
			sum += allocator->stat(allocator);

		allocator = allocator->next;
	}

	return sum;
}
