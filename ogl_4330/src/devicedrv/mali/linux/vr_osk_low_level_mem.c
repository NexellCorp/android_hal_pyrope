/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_osk_low_level_mem.c
 * Implementation of the OS abstraction layer for the kernel device driver
 */

/* needed to detect kernel version specific code */
#include <linux/version.h>

#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
#include <linux/shrinker.h>
#endif

#include "vr_osk.h"
#include "vr_ukk.h" /* required to hook in _vr_ukk_mem_mmap handling */
#include "vr_kernel_common.h"
#include "vr_kernel_linux.h"

static void vr_kernel_memory_vma_open(struct vm_area_struct * vma);
static void vr_kernel_memory_vma_close(struct vm_area_struct * vma);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
static int vr_kernel_memory_cpu_page_fault_handler(struct vm_area_struct *vma, struct vm_fault *vmf);
#else
static unsigned long vr_kernel_memory_cpu_page_fault_handler(struct vm_area_struct * vma, unsigned long address);
#endif


typedef struct vr_vma_usage_tracker
{
	int references;
	u32 cookie;
} vr_vma_usage_tracker;

#define INVALID_PAGE 0xffffffff

/* Linked list structure to hold details of all OS allocations in a particular
 * mapping
 */
struct AllocationList
{
	struct AllocationList *next;
	u32 offset;
	u32 physaddr;
};

typedef struct AllocationList AllocationList;

/* Private structure to store details of a mapping region returned
 * from _vr_osk_mem_mapregion_init
 */
struct MappingInfo
{
	struct vm_area_struct *vma;
	struct AllocationList *list;
	struct AllocationList *tail;
};

typedef struct MappingInfo MappingInfo;


static u32 _kernel_page_allocate(void);
static void _kernel_page_release(u32 physical_address);
static AllocationList * _allocation_list_item_get(void);
static void _allocation_list_item_release(AllocationList * item);


/* Variable declarations */
static DEFINE_SPINLOCK(allocation_list_spinlock);
static AllocationList * pre_allocated_memory = (AllocationList*) NULL ;
static int pre_allocated_memory_size_current  = 0;
#ifdef VR_OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB
	static int pre_allocated_memory_size_max      = VR_OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB * 1024 * 1024;
#else
	static int pre_allocated_memory_size_max      = 16 * 1024 * 1024; /* 6 MiB */
#endif

static struct vm_operations_struct vr_kernel_vm_ops =
{
	.open = vr_kernel_memory_vma_open,
	.close = vr_kernel_memory_vma_close,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	.fault = vr_kernel_memory_cpu_page_fault_handler
#else
	.nopfn = vr_kernel_memory_cpu_page_fault_handler
#endif
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
	#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
static int vr_mem_shrink(int nr_to_scan, gfp_t gfp_mask)
	#else
static int vr_mem_shrink(struct shrinker *shrinker, int nr_to_scan, gfp_t gfp_mask)
	#endif
#else
static int vr_mem_shrink(struct shrinker *shrinker, struct shrink_control *sc)
#endif
{
	unsigned long flags;
	AllocationList *item;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
	int nr = nr_to_scan;
#else
	int nr = sc->nr_to_scan;
#endif

	if (0 == nr)
	{
		return pre_allocated_memory_size_current / PAGE_SIZE;
	}

	if (0 == pre_allocated_memory_size_current)
	{
		/* No pages availble */
		return 0;
	}

	if (0 == spin_trylock_irqsave(&allocation_list_spinlock, flags))
	{
		/* Not able to lock. */
		return -1;
	}

	while (pre_allocated_memory && nr > 0)
	{
		item = pre_allocated_memory;
		pre_allocated_memory = item->next;

		_kernel_page_release(item->physaddr);
		_vr_osk_free(item);

		pre_allocated_memory_size_current -= PAGE_SIZE;
		--nr;
	}
	spin_unlock_irqrestore(&allocation_list_spinlock,flags);

	return pre_allocated_memory_size_current / PAGE_SIZE;
}

struct shrinker vr_mem_shrinker = {
	.shrink = vr_mem_shrink,
	.seeks = DEFAULT_SEEKS,
};

void vr_osk_low_level_mem_init(void)
{
	pre_allocated_memory = (AllocationList*) NULL ;

	register_shrinker(&vr_mem_shrinker);
}

void vr_osk_low_level_mem_term(void)
{
	unregister_shrinker(&vr_mem_shrinker);

	while ( NULL != pre_allocated_memory )
	{
		AllocationList *item;
		item = pre_allocated_memory;
		pre_allocated_memory = item->next;
		_kernel_page_release(item->physaddr);
		_vr_osk_free( item );
	}
	pre_allocated_memory_size_current  = 0;
}

static u32 _kernel_page_allocate(void)
{
	struct page *new_page;
	u32 linux_phys_addr;

	new_page = alloc_page(GFP_HIGHUSER | __GFP_ZERO | __GFP_REPEAT | __GFP_NOWARN | __GFP_COLD);

	if ( NULL == new_page )
	{
		return INVALID_PAGE;
	}

	/* Ensure page is flushed from CPU caches. */
	linux_phys_addr = dma_map_page(NULL, new_page, 0, PAGE_SIZE, DMA_BIDIRECTIONAL);

	return linux_phys_addr;
}

static void _kernel_page_release(u32 physical_address)
{
	struct page *unmap_page;

	#if 1
	dma_unmap_page(NULL, physical_address, PAGE_SIZE, DMA_BIDIRECTIONAL);
	#endif

	unmap_page = pfn_to_page( physical_address >> PAGE_SHIFT );
	VR_DEBUG_ASSERT_POINTER( unmap_page );
	__free_page( unmap_page );
}

static AllocationList * _allocation_list_item_get(void)
{
	AllocationList *item = NULL;
	unsigned long flags;

	spin_lock_irqsave(&allocation_list_spinlock,flags);
	if ( pre_allocated_memory )
	{
		item = pre_allocated_memory;
		pre_allocated_memory = pre_allocated_memory->next;
		pre_allocated_memory_size_current -= PAGE_SIZE;

		spin_unlock_irqrestore(&allocation_list_spinlock,flags);
		return item;
	}
	spin_unlock_irqrestore(&allocation_list_spinlock,flags);

	item = _vr_osk_malloc( sizeof(AllocationList) );
	if ( NULL == item)
	{
		return NULL;
	}

	item->physaddr = _kernel_page_allocate();
	if ( INVALID_PAGE == item->physaddr )
	{
		/* Non-fatal error condition, out of memory. Upper levels will handle this. */
		_vr_osk_free( item );
		return NULL;
	}
	return item;
}

static void _allocation_list_item_release(AllocationList * item)
{
	unsigned long flags;
	spin_lock_irqsave(&allocation_list_spinlock,flags);
	if ( pre_allocated_memory_size_current < pre_allocated_memory_size_max)
	{
		item->next = pre_allocated_memory;
		pre_allocated_memory = item;
		pre_allocated_memory_size_current += PAGE_SIZE;
		spin_unlock_irqrestore(&allocation_list_spinlock,flags);
		return;
	}
	spin_unlock_irqrestore(&allocation_list_spinlock,flags);

	_kernel_page_release(item->physaddr);
	_vr_osk_free( item );
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
static int vr_kernel_memory_cpu_page_fault_handler(struct vm_area_struct *vma, struct vm_fault *vmf)
#else
static unsigned long vr_kernel_memory_cpu_page_fault_handler(struct vm_area_struct * vma, unsigned long address)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	void __user * address;
	address = vmf->virtual_address;
#endif
	/*
	 * We always fail the call since all memory is pre-faulted when assigned to the process.
	 * Only the VR cores can use page faults to extend buffers.
	*/

	VR_DEBUG_PRINT(1, ("Page-fault in VR memory region caused by the CPU.\n"));
	VR_DEBUG_PRINT(1, ("Tried to access %p (process local virtual address) which is not currently mapped to any VR memory.\n", (void*)address));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	return VM_FAULT_SIGBUS;
#else
	return NOPFN_SIGBUS;
#endif
}

static void vr_kernel_memory_vma_open(struct vm_area_struct * vma)
{
	vr_vma_usage_tracker * vma_usage_tracker;
	VR_DEBUG_PRINT(4, ("Open called on vma %p\n", vma));

	vma_usage_tracker = (vr_vma_usage_tracker*)vma->vm_private_data;
	vma_usage_tracker->references++;

	return;
}

static void vr_kernel_memory_vma_close(struct vm_area_struct * vma)
{
	_vr_uk_mem_munmap_s args = {0, };
	vr_memory_allocation * descriptor;
	vr_vma_usage_tracker * vma_usage_tracker;
	VR_DEBUG_PRINT(3, ("Close called on vma %p\n", vma));

	vma_usage_tracker = (vr_vma_usage_tracker*)vma->vm_private_data;

	BUG_ON(!vma_usage_tracker);
	BUG_ON(0 == vma_usage_tracker->references);

	vma_usage_tracker->references--;

	if (0 != vma_usage_tracker->references)
	{
		VR_DEBUG_PRINT(3, ("Ignoring this close, %d references still exists\n", vma_usage_tracker->references));
		return;
	}

	/** @note args->context unused, initialized to 0.
	 * Instead, we use the memory_session from the cookie */

	descriptor = (vr_memory_allocation *)vma_usage_tracker->cookie;

	args.cookie = (u32)descriptor;
	args.mapping = descriptor->mapping;
	args.size = descriptor->size;

	_vr_ukk_mem_munmap( &args );

	/* vma_usage_tracker is free()d by _vr_osk_mem_mapregion_term().
	 * In the case of the memory engine, it is called as the release function that has been registered with the engine*/
}

void _vr_osk_mem_barrier( void )
{
	mb();
}

void _vr_osk_write_mem_barrier( void )
{
	wmb();
}

vr_io_address _vr_osk_mem_mapioregion( u32 phys, u32 size, const char *description )
{
	return (vr_io_address)ioremap_nocache(phys, size);
}

void _vr_osk_mem_unmapioregion( u32 phys, u32 size, vr_io_address virt )
{
	iounmap((void*)virt);
}

vr_io_address _vr_osk_mem_allocioregion( u32 *phys, u32 size )
{
	void * virt;
 	VR_DEBUG_ASSERT_POINTER( phys );
 	VR_DEBUG_ASSERT( 0 == (size & ~_VR_OSK_CPU_PAGE_MASK) );
 	VR_DEBUG_ASSERT( 0 != size );

	/* dma_alloc_* uses a limited region of address space. On most arch/marchs
	 * 2 to 14 MiB is available. This should be enough for the page tables, which
	 * currently is the only user of this function. */
	virt = dma_alloc_coherent(NULL, size, phys, GFP_KERNEL | GFP_DMA );

	VR_DEBUG_PRINT(3, ("Page table virt: 0x%x = dma_alloc_coherent(size:%d, phys:0x%x, )\n", virt, size, phys));

 	if ( NULL == virt )
 	{
		VR_DEBUG_PRINT(5, ("allocioregion: Failed to allocate Pagetable memory, size=0x%.8X\n", size ));
 		return 0;
 	}

	VR_DEBUG_ASSERT( 0 == (*phys & ~_VR_OSK_CPU_PAGE_MASK) );

 	return (vr_io_address)virt;
}

void _vr_osk_mem_freeioregion( u32 phys, u32 size, vr_io_address virt )
{
 	VR_DEBUG_ASSERT_POINTER( (void*)virt );
 	VR_DEBUG_ASSERT( 0 != size );
 	VR_DEBUG_ASSERT( 0 == (phys & ( (1 << PAGE_SHIFT) - 1 )) );

	dma_free_coherent(NULL, size, virt, phys);
}

_vr_osk_errcode_t inline _vr_osk_mem_reqregion( u32 phys, u32 size, const char *description )
{
#if VR_LICENSE_IS_GPL
	return _VR_OSK_ERR_OK; /* GPL driver gets the mem region for the resources registered automatically */
#else
	return ((NULL == request_mem_region(phys, size, description)) ? _VR_OSK_ERR_NOMEM : _VR_OSK_ERR_OK);
#endif
}

void inline _vr_osk_mem_unreqregion( u32 phys, u32 size )
{
#if !VR_LICENSE_IS_GPL
	release_mem_region(phys, size);
#endif
}

void inline _vr_osk_mem_iowrite32_relaxed( volatile vr_io_address addr, u32 offset, u32 val )
{
	__raw_writel(cpu_to_le32(val),((u8*)addr) + offset);
}

u32 inline _vr_osk_mem_ioread32( volatile vr_io_address addr, u32 offset )
{
	return ioread32(((u8*)addr) + offset);
}

void inline _vr_osk_mem_iowrite32( volatile vr_io_address addr, u32 offset, u32 val )
{
	iowrite32(val, ((u8*)addr) + offset);
}

void _vr_osk_cache_flushall( void )
{
	/** @note Cached memory is not currently supported in this implementation */
}

void _vr_osk_cache_ensure_uncached_range_flushed( void *uncached_mapping, u32 offset, u32 size )
{
	_vr_osk_write_mem_barrier();
}

_vr_osk_errcode_t _vr_osk_mem_mapregion_init( vr_memory_allocation * descriptor )
{
	struct vm_area_struct *vma;
	vr_vma_usage_tracker * vma_usage_tracker;
	MappingInfo *mappingInfo;

	if (NULL == descriptor) return _VR_OSK_ERR_FAULT;

	VR_DEBUG_ASSERT( 0 != (descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE) );

	vma = (struct vm_area_struct*)descriptor->process_addr_mapping_info;

	if (NULL == vma ) return _VR_OSK_ERR_FAULT;

	/* Re-write the process_addr_mapping_info */
	mappingInfo = _vr_osk_calloc( 1, sizeof(MappingInfo) );

	if ( NULL == mappingInfo ) return _VR_OSK_ERR_FAULT;

	vma_usage_tracker = _vr_osk_calloc( 1, sizeof(vr_vma_usage_tracker) );

	if (NULL == vma_usage_tracker)
	{
		VR_DEBUG_PRINT(2, ("Failed to allocate memory to track memory usage\n"));
		_vr_osk_free( mappingInfo );
		return _VR_OSK_ERR_FAULT;
	}

	mappingInfo->vma = vma;
	descriptor->process_addr_mapping_info = mappingInfo;

	/* Do the va range allocation - in this case, it was done earlier, so we copy in that information */
	descriptor->mapping = (void __user*)vma->vm_start;
	/* list member is already NULL */

	/*
	  set some bits which indicate that:
	  The memory is IO memory, meaning that no paging is to be performed and the memory should not be included in crash dumps
	  The memory is reserved, meaning that it's present and can never be paged out (see also previous entry)
	*/
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_RESERVED;
	vma->vm_flags |= VM_DONTCOPY;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &vr_kernel_vm_ops; /* Operations used on any memory system */

	vma_usage_tracker->references = 1; /* set initial reference count to be 1 as vma_open won't be called for the first mmap call */
	vma_usage_tracker->cookie = (u32)descriptor; /* cookie for munmap */

	vma->vm_private_data = vma_usage_tracker;

	return _VR_OSK_ERR_OK;
}

void _vr_osk_mem_mapregion_term( vr_memory_allocation * descriptor )
{
	struct vm_area_struct* vma;
	vr_vma_usage_tracker * vma_usage_tracker;
	MappingInfo *mappingInfo;

	if (NULL == descriptor) return;

	VR_DEBUG_ASSERT( 0 != (descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE) );

	mappingInfo = (MappingInfo *)descriptor->process_addr_mapping_info;

	VR_DEBUG_ASSERT_POINTER( mappingInfo );

	/* Linux does the right thing as part of munmap to remove the mapping
	 * All that remains is that we remove the vma_usage_tracker setup in init() */
	vma = mappingInfo->vma;

	VR_DEBUG_ASSERT_POINTER( vma );

	/* ASSERT that there are no allocations on the list. Unmap should've been
	 * called on all OS allocations. */
	VR_DEBUG_ASSERT( NULL == mappingInfo->list );

	vma_usage_tracker = vma->vm_private_data;

	/* We only get called if mem_mapregion_init succeeded */
	_vr_osk_free(vma_usage_tracker);

	_vr_osk_free( mappingInfo );
	return;
}

_vr_osk_errcode_t _vr_osk_mem_mapregion_map( vr_memory_allocation * descriptor, u32 offset, u32 *phys_addr, u32 size )
{
	struct vm_area_struct *vma;
	MappingInfo *mappingInfo;

	if (NULL == descriptor) return _VR_OSK_ERR_FAULT;

	VR_DEBUG_ASSERT_POINTER( phys_addr );

	VR_DEBUG_ASSERT( 0 != (descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE) );

	VR_DEBUG_ASSERT( 0 == (size & ~_VR_OSK_CPU_PAGE_MASK) );

	VR_DEBUG_ASSERT( 0 == (offset & ~_VR_OSK_CPU_PAGE_MASK));

	if (NULL == descriptor->mapping) return _VR_OSK_ERR_INVALID_ARGS;

	if (size > (descriptor->size - offset))
	{
		VR_DEBUG_PRINT(1,("_vr_osk_mem_mapregion_map: virtual memory area not large enough to map physical 0x%x size %x into area 0x%x at offset 0x%xr\n",
		                    *phys_addr, size, descriptor->mapping, offset));
		return _VR_OSK_ERR_FAULT;
	}

	mappingInfo = (MappingInfo *)descriptor->process_addr_mapping_info;

	VR_DEBUG_ASSERT_POINTER( mappingInfo );

	vma = mappingInfo->vma;

	if (NULL == vma ) return _VR_OSK_ERR_FAULT;

	VR_DEBUG_PRINT(7, ("Process map: mapping 0x%08X to process address 0x%08lX length 0x%08X\n", *phys_addr, (long unsigned int)(descriptor->mapping + offset), size));

	if ( VR_MEMORY_ALLOCATION_OS_ALLOCATED_PHYSADDR_MAGIC == *phys_addr )
	{
		_vr_osk_errcode_t ret;
		AllocationList *alloc_item;
		u32 linux_phys_frame_num;

		alloc_item = _allocation_list_item_get();
		if (NULL == alloc_item)
		{
			VR_DEBUG_PRINT(1, ("Failed to allocate list item\n"));
			return _VR_OSK_ERR_NOMEM;
		}

		linux_phys_frame_num = alloc_item->physaddr >> PAGE_SHIFT;

		ret = ( remap_pfn_range( vma, ((u32)descriptor->mapping) + offset, linux_phys_frame_num, size, vma->vm_page_prot) ) ? _VR_OSK_ERR_FAULT : _VR_OSK_ERR_OK;

		if ( ret != _VR_OSK_ERR_OK)
		{
			VR_PRINT_ERROR(("%s %d could not remap_pfn_range()\n", __FUNCTION__, __LINE__));
			_allocation_list_item_release(alloc_item);
			return ret;
		}

		/* Put our alloc_item into the list of allocations on success */
		if (NULL == mappingInfo->list)
		{
			mappingInfo->list = alloc_item;
		}
		else
		{
			mappingInfo->tail->next = alloc_item;
		}

		mappingInfo->tail = alloc_item;
		alloc_item->next = NULL;
		alloc_item->offset = offset;

		/* Write out new physical address on success */
		*phys_addr = alloc_item->physaddr;

		return ret;
	}

	/* Otherwise, Use the supplied physical address */

	/* ASSERT that supplied phys_addr is page aligned */
	VR_DEBUG_ASSERT( 0 == ((*phys_addr) & ~_VR_OSK_CPU_PAGE_MASK) );

	return ( remap_pfn_range( vma, ((u32)descriptor->mapping) + offset, *phys_addr >> PAGE_SHIFT, size, vma->vm_page_prot) ) ? _VR_OSK_ERR_FAULT : _VR_OSK_ERR_OK;

}

void _vr_osk_mem_mapregion_unmap( vr_memory_allocation * descriptor, u32 offset, u32 size, _vr_osk_mem_mapregion_flags_t flags )
{
	MappingInfo *mappingInfo;

   if (NULL == descriptor) return;

	VR_DEBUG_ASSERT( 0 != (descriptor->flags & VR_MEMORY_ALLOCATION_FLAG_MAP_INTO_USERSPACE) );

	VR_DEBUG_ASSERT( 0 == (size & ~_VR_OSK_CPU_PAGE_MASK) );

	VR_DEBUG_ASSERT( 0 == (offset & ~_VR_OSK_CPU_PAGE_MASK) );

	if (NULL == descriptor->mapping) return;

	if (size > (descriptor->size - offset))
	{
		VR_DEBUG_PRINT(1,("_vr_osk_mem_mapregion_unmap: virtual memory area not large enough to unmap size %x from area 0x%x at offset 0x%x\n",
							size, descriptor->mapping, offset));
		return;
	}
	mappingInfo = (MappingInfo *)descriptor->process_addr_mapping_info;

	VR_DEBUG_ASSERT_POINTER( mappingInfo );

	if ( 0 != (flags & _VR_OSK_MEM_MAPREGION_FLAG_OS_ALLOCATED_PHYSADDR) )
	{
		/* This physical RAM was allocated in _vr_osk_mem_mapregion_map and
		 * so needs to be unmapped
		 */
		while (size)
		{
			/* First find the allocation in the list of allocations */
			AllocationList *alloc = mappingInfo->list;
			AllocationList **prev = &(mappingInfo->list);

			while (NULL != alloc && alloc->offset != offset)
			{
				prev = &(alloc->next);
				alloc = alloc->next;
			}
			if (alloc == NULL) {
				VR_DEBUG_PRINT(1, ("Unmapping memory that isn't mapped\n"));
				size -= _VR_OSK_CPU_PAGE_SIZE;
				offset += _VR_OSK_CPU_PAGE_SIZE;
				continue;
			}

			*prev = alloc->next;
			_allocation_list_item_release(alloc);

			/* Move onto the next allocation */
			size -= _VR_OSK_CPU_PAGE_SIZE;
			offset += _VR_OSK_CPU_PAGE_SIZE;
		}
	}

	/* Linux does the right thing as part of munmap to remove the mapping */

	return;
}
