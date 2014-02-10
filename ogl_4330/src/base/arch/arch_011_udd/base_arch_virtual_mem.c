/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <arch/base_arch_mem_inline.h>
#include <mali_system.h>

 /* common util functions */
#include <base/common/tools/base_common_tools_circular_linked_list.h>

/* the arch memory interface we're implementing */
#include <base/arch/base_arch_mem.h>

/**
 * Buddy system slot struct
 */
typedef struct virtual_memory_area_slot
{
	mali_embedded_list_link link; /**< Link used when in use and or on a free list */
	u32 misc; /** Flags describing the state fo this slot */
} virtual_memory_area_slot;

/**
 * Virtual memory area struct
 */
typedef struct virtual_memory_area
{
	u32 min_order; /**< Minimum order for allocations supported */
	u32 max_order; /**< Maximum order for allocations supported */
	u32 size; /**< Total size of area */
	u32 slot_size; /**< Minimum allocation size */
	mali_addr start_addr; /**< Mali address of start */
	virtual_memory_area_slot * slots; /**< Pointer to N slots based on the number of slots needed for this system */
	mali_embedded_list_link * freelist; /**< Pointer to X linked list used as freelist. Based om min_order->max_order range */
	mali_mutex_handle lock; /**< Lock protecting access to the freelist and slots list */
} virtual_memory_area;

/**
 * Buddy system buddy status bits
 */
enum MISC_SHIFT { MISC_SHIFT_FREE = 0, MISC_SHIFT_ORDER = 1, MISC_SHIFT_TOPLEVEL = 6 };
enum MISC_MASK { MISC_MASK_FREE = 0x01, MISC_MASK_ORDER = 0x1F, MISC_MASK_TOPLEVEL = 0x01F };

/**
 * Function to calculate the order needed to represent size
 * Find the lowest order where (1 << order ) >= size
 * If a bank is given the order will never be below it's minimum limit
 * @param size The size to find the order needed for
 * @return The lowest order satisfying (1 << order) >= size
 */
MALI_STATIC_INLINE u32 order_needed_for_size(u32 size);

/**
 * Function to find the maximum order which fits within a size
 * Finds the highest order where (1 << order) <= size
 * @param size The size to find the order for
 * @return The highest order satisfying (1 << order) <= size
 */
MALI_STATIC_INLINE u32 maximum_order_which_fits(u32 size);

/**
 * Set a memory slot's free status
 * @param slot The slot to set the state for
 * @param state The state to set
 */
MALI_STATIC_INLINE void set_slot_free(virtual_memory_area_slot * slot, int state);

/**
 * Set a memory slot's order
 * @param slot The slot to set the order for
 * @param order The order to set
 */
MALI_STATIC_INLINE void set_slot_order(virtual_memory_area_slot * slot, u32 order);

/**
 * Get a memory slot's order
 * @param slot The slot to get the order for
 * @return The order this slot exists on
 */
MALI_STATIC_INLINE u32 get_slot_order(virtual_memory_area_slot * slot);

/**
 * Tag a slot as being a toplevel slot.
 * A toplevel slot has no buddy and no parent
 * @param slot The slot to tag as being toplevel
 */
MALI_STATIC_INLINE void set_slot_toplevel(virtual_memory_area_slot * slot, u32 level);

/**
 * Check if a slot is a toplevel slot
 * @param slot The slot to check
 * @return 1 if toplevel, 0 else
 */
MALI_STATIC_INLINE u32 get_slot_toplevel(virtual_memory_area_slot * slot);

/**
 * Checks if the given slot is a buddy at the given order and that it's free
 * @param slot The slot to check
 * @param order The order to check against
 * @return 0 if not valid, else 1
 */
MALI_STATIC_INLINE int slot_is_valid_buddy(virtual_memory_area *area, virtual_memory_area_slot *slot, u32 order);

/*
 The buddy system uses the following rules to quickly find a slot's buddy
 and parent (slot representing this slot at a higher order level):
 - Given a slot with index i the slot's buddy is at index i ^ ( 1 << order)
 - Given a slot with index i the slot's parent is at i & ~(1 << order)
*/

/**
 * Get a slot's buddy
 * @param slot The slot to find the buddy for
 * @param order The order to operate on
 * @return Pointer to the buddy slot
 */
MALI_STATIC_INLINE virtual_memory_area_slot * slot_get_buddy(virtual_memory_area *area, virtual_memory_area_slot *slot, u32 order);

/**
 * Get a slot's parent
 * @param slot The slot to find the parent for
 * @param order The order to operate on
 * @return Pointer to the parent slot
 */
MALI_STATIC_INLINE virtual_memory_area_slot * slot_get_parent(virtual_memory_area *area, virtual_memory_area_slot *slot, u32 order);

/**
 * Get the mali seen address of the memory described by the slot
 * @param slot The memory slot to return the address of
 * @return The mali seen address of the memory slot
 */
MALI_STATIC_INLINE u32 slot_mali_addr_get(virtual_memory_area *area, virtual_memory_area_slot * slot);

/**
 * Get the size of the memory described by the given slot
 * @param slot The memory slot to return the size of
 * @return The size of the memory slot described by the object
 */
MALI_STATIC_INLINE u32 slot_size_get(virtual_memory_area_slot * slot);

/**
 * Virtual memory area assigned to this process
 */
MALI_STATIC virtual_memory_area mali_normal_memory_area = {0, 0, 0, 0, 0, 0, 0, 0};
MALI_STATIC virtual_memory_area mali_external_memory_area = {0, 0, 0, 0, 0, 0, 0, 0};

MALI_STATIC_INLINE u32 order_needed_for_size(u32 size)
{
        u32 order = 0;
        u32 powsize = 1;
        while (powsize < size)
        {
                powsize <<= 1;
                order++;
        }
        if (order < 12) order = 12;

        return order;
}

MALI_STATIC_INLINE u32 maximum_order_which_fits(u32 size)
{
        u32 order = 0;
        u32 powsize = 1;
        while (powsize < size)
        {
                powsize <<= 1;
                if (powsize > size) break;
                order++;
        }

        return order;
}

MALI_STATIC_INLINE u32 get_slot_free(virtual_memory_area_slot * slot)
{
	return (slot->misc >> MISC_SHIFT_FREE) & MISC_MASK_FREE;
}

MALI_STATIC_INLINE void set_slot_free(virtual_memory_area_slot * slot, int state)
{
	if (state) slot->misc |= (MISC_MASK_FREE << MISC_SHIFT_FREE);
	else slot->misc &= ~(MISC_MASK_FREE << MISC_SHIFT_FREE);
}

MALI_STATIC_INLINE void set_slot_order(virtual_memory_area_slot * slot, u32 order)
{
	slot->misc &= ~(MISC_MASK_ORDER << MISC_SHIFT_ORDER);
	slot->misc |= ((order & MISC_MASK_ORDER) << MISC_SHIFT_ORDER);
}

MALI_STATIC_INLINE u32 get_slot_order(virtual_memory_area_slot * slot)
{
	return (slot->misc >> MISC_SHIFT_ORDER) & MISC_MASK_ORDER;
}

MALI_STATIC_INLINE void set_slot_toplevel(virtual_memory_area_slot * slot, u32 level)
{
	slot->misc |= ((level & MISC_MASK_TOPLEVEL) << MISC_SHIFT_TOPLEVEL);
}

MALI_STATIC_INLINE u32 get_slot_toplevel(virtual_memory_area_slot * slot)
{
	return (slot->misc >> MISC_SHIFT_TOPLEVEL) & MISC_MASK_TOPLEVEL;
}

MALI_STATIC_INLINE u32 slot_get_offset(virtual_memory_area *area, virtual_memory_area_slot * slot)
{
    /* Return offset of slot record from the start of the slot records. */

	MALI_DEBUG_ASSERT(slot >= area->slots, ("Slot 0x%X before start of slot records 0x%X\n", slot, area->slots));
	MALI_DEBUG_ASSERT(slot < area->slots + (area->size / area->slot_size), ("Slot 0x%X beyond end of slots 0x%X\n", slot, area->slots + (area->size / area->slot_size)));
	MALI_DEBUG_ASSERT((MALI_REINTERPRET_CAST(u32)(slot - area->slots)) <= (area->size / area->slot_size), ("Bad offset for slot 0x%X\n", slot));
	return slot - area->slots;
}

MALI_STATIC_INLINE virtual_memory_area_slot * slot_get_buddy(virtual_memory_area *area, virtual_memory_area_slot *slot, u32 order)
{
	return slot + ( (slot_get_offset(area, slot) ^ (1 << order)) - slot_get_offset(area, slot));
}

MALI_STATIC_INLINE virtual_memory_area_slot *slot_get_parent(virtual_memory_area *area, virtual_memory_area_slot *slot, u32 order)
{
	return slot + ((slot_get_offset(area, slot) & ~(1 << order)) - slot_get_offset(area, slot));
}

MALI_STATIC_INLINE u32 slot_mali_addr_get(virtual_memory_area *area, virtual_memory_area_slot * slot)
{
	if (NULL != slot) return area->start_addr + area->slot_size * slot_get_offset(area, slot);
	else return 0;
}

MALI_STATIC_INLINE u32 slot_size_get(virtual_memory_area_slot * slot)
{
	MALI_DEBUG_ASSERT_POINTER(slot);
	return 1 << get_slot_order(slot);
}

MALI_STATIC_INLINE int slot_is_valid_buddy(virtual_memory_area *area, virtual_memory_area_slot *slot, u32 order)
{
	MALI_DEBUG_ASSERT(slot >= area->slots, ("Slot 0x%X before start of slot records 0x%X\n", slot, area->slots));
	MALI_DEBUG_ASSERT(slot < area->slots + (area->size / area->slot_size), ("Slot 0x%X beyond end of slots 0x%X\n", slot, area->slots + (area->size / area->slot_size)));
	MALI_DEBUG_ASSERT((MALI_REINTERPRET_CAST(u32)(slot - area->slots)) <= (area->size / area->slot_size), ("Bad offset for slot 0x%X\n", slot));
	if (get_slot_free(slot) && (get_slot_order(slot) == order)) return 1;
	else return 0;
}

MALI_STATIC mali_err_code virtual_memory_area_create(virtual_memory_area *area, u32 start, u32 size, u32 slot_size)
{
	unsigned int i, offset,left;

	area->lock = _mali_sys_mutex_create();
	MALI_CHECK_NON_NULL(area->lock, MALI_ERR_FUNCTION_FAILED);

	area->start_addr = start;
	area->size = size;
	area->slot_size = slot_size;
	area->min_order = order_needed_for_size(slot_size);
	area->max_order = maximum_order_which_fits(area->size);

	area->slots = _mali_sys_calloc((area->size / slot_size), sizeof(virtual_memory_area_slot));
	if (NULL == area->slots)
	{
		_mali_sys_mutex_destroy(area->lock);
		MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
	}

	area->freelist = _mali_sys_calloc((area->max_order - area->min_order + 1), sizeof(struct mali_embedded_list_link));
	if (NULL == area->freelist)
	{
		_mali_sys_free(area->slots);
		_mali_sys_mutex_destroy(area->lock);
		MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
	}

	for (i = 0; i < (area->max_order - area->min_order + 1); i++) MALI_EMBEDDED_LIST_HEAD_INIT_RUNTIME(&area->freelist[i]);

	/* init slot info */
	for (offset = 0, left = area->size; offset < (area->size / slot_size); /* updated inside the body */)
	{
		u32 slot_order;
		virtual_memory_area_slot * slot;

		/* the maximum order which fits in the remaining area */
		slot_order = maximum_order_which_fits(left);

		/* find the slot pointer */
		slot = &area->slots[offset];

		/* tag the slot as being toplevel */
		set_slot_toplevel(slot, slot_order);

		/* tag it as being free */
		set_slot_free(slot, 1);

		/* set the order */
		set_slot_order(slot, slot_order);

		_mali_embedded_list_insert_tail(area->freelist + (slot_order - area->min_order), &slot->link);

		left -= (1 << slot_order);
		offset += ((1 << slot_order) / slot_size);
	}

	MALI_SUCCESS;
}

MALI_STATIC void virtual_memory_area_destroy(virtual_memory_area *area)
{
	_mali_sys_mutex_destroy(area->lock);
	area->lock = MALI_NO_HANDLE;

	_mali_sys_free(area->slots);
	area->slots = NULL;

	_mali_sys_free(area->freelist);
	area->freelist = NULL;
}

MALI_STATIC virtual_memory_area_slot *backend_mmu_virtual_address_range_allocate(virtual_memory_area *area, u32 minimum_block_size)
{
	virtual_memory_area_slot *slot = NULL;
	u32 requested_order, current_order;

	/* change block size to an acceptable size */
	minimum_block_size = MALI_ALIGN(minimum_block_size, area->slot_size);

	requested_order = order_needed_for_size(minimum_block_size);

	MALI_DEBUG_PRINT(5, ("For size %d we need order %d (%d)\n", minimum_block_size, requested_order, 1 << requested_order));

	_mali_sys_mutex_lock(area->lock);
	/* ! critical section begin */

	for (current_order = requested_order; current_order <= area->max_order; ++current_order)
	{
		mali_embedded_list_link * list = area->freelist + (current_order - area->min_order);
		MALI_DEBUG_PRINT(7, ("Checking freelist 0x%X for order %d\n", list, current_order));
		if (_mali_embedded_list_is_empty(list)) continue; /* empty list */

		MALI_DEBUG_PRINT(7, ("Found an entry on the freelist for order %d\n", current_order));

		slot = MALI_EMBEDDED_LIST_GET_CONTAINER(virtual_memory_area_slot, link, list->next);
		_mali_embedded_list_remove(&slot->link);

		while (current_order > requested_order)
		{
			virtual_memory_area_slot * buddy;
			MALI_DEBUG_PRINT(7, ("Splitting slot 0x%X\n", slot));
			current_order--;
			list--;
			buddy = slot_get_buddy(area, slot, current_order - area->min_order);
			set_slot_order(buddy, current_order);
			set_slot_free(buddy, 1);
			_mali_embedded_list_insert_after(list, &buddy->link);
		}

		set_slot_order(slot, current_order);
		set_slot_free(slot, 0);
		/* update usage count */

		break;
	}

	/* ! critical section end */
	_mali_sys_mutex_unlock(area->lock);

	MALI_DEBUG_PRINT(7, ("Lock released"));

	return slot;
}

MALI_STATIC void backend_mmu_virtual_address_range_free(virtual_memory_area *area, virtual_memory_area_slot *slot)
{
	u32 current_order;

	/* we're manipulating the free list, so we need to lock it */
	_mali_sys_mutex_lock(area->lock);
	/* ! critical section begin */

	set_slot_free(slot, 1);
	current_order = get_slot_order(slot);

	/* merge the slots until there is no free buddy or we get to the top level */
	while (current_order <= area->max_order && get_slot_toplevel(slot) != current_order)
	{
		virtual_memory_area_slot * buddy;
		buddy = slot_get_buddy(area, slot, current_order - area->min_order);
		if (!slot_is_valid_buddy(area, buddy, current_order)) break;
		_mali_embedded_list_remove(&buddy->link); /* remove from free list */
		/* clear tracked data in both slots */
		set_slot_order(slot, 0);
		set_slot_free(slot, 0);
		set_slot_order(buddy, 0);
		set_slot_free(buddy, 0);
		/* make the parent control the new state */
		slot = slot_get_parent(area, slot, current_order - area->min_order);
		set_slot_order(slot, current_order + 1); /* merged has a higher order */
		set_slot_free(slot, 1); /* mark it as free */
		current_order++;
	}

	_mali_embedded_list_insert_after(&area->freelist[current_order - area->min_order], &slot->link);

	/* !critical section end */
	_mali_sys_mutex_unlock(area->lock);
}

mali_err_code mali_virtual_memory_area_create(void)
{
	mali_err_code err;

	/* Use 2GiB of virtual address space, from 1GiB up to 3GiB.
	 *
	 * Mali virtual memory:
	 * 	0x00000000
	 * 		256MiB Reserved
	 * 	0x10000000
	 * 		2GiB Normal Mali memory
	 * 	0x90000000
	 * 		256MiB Unused
	 * 	0xA0000000
	 * 		1GiB Mapped external memory (dma-buf)
	 * 	0xE0000000
	 *		Reserved
	 * 	0xFFFFFFFF
	 */
	
	/*                                                         256MiB        2GiB        256KiB */
	err = virtual_memory_area_create(&mali_normal_memory_area, 0x10000000, 0x80000000, 0x40000);
	if (MALI_ERR_NO_ERROR != err) return err;

	/*                                                           3GiB        1GiB        4MiB */
	err = virtual_memory_area_create(&mali_external_memory_area, 0xA0000000, 0x40000000, 0x400000);
	if (MALI_ERR_NO_ERROR != err)
	{
		virtual_memory_area_destroy(&mali_normal_memory_area);
		return err;
	}
	return MALI_ERR_NO_ERROR;
}

void mali_virtual_memory_area_destroy(void)
{
	virtual_memory_area_destroy(&mali_normal_memory_area);
	virtual_memory_area_destroy(&mali_external_memory_area);
}

MALI_STATIC virtual_memory_area *find_virtual_memory_area(arch_mem *descriptor)
{
	switch (descriptor->embedded_mali_mem.memory_subtype)
	{
		case MALI_MEM_TYPE_DMA_BUF_EXTERNAL_MEMORY:
			return &mali_external_memory_area;
			break;
		default:
			return &mali_normal_memory_area;
			break;
	}
}

mali_err_code mali_mmu_virtual_address_range_allocate(arch_mem *descriptor, u32 minimum_block_size)
{
	virtual_memory_area *area = NULL;
	virtual_memory_area_slot *slot = NULL;

	MALI_DEBUG_ASSERT_POINTER(descriptor);

	area = find_virtual_memory_area(descriptor);

	slot = backend_mmu_virtual_address_range_allocate(area, minimum_block_size);

	if (NULL != slot)
	{
		descriptor->cookie = MALI_REINTERPRET_CAST(u32)slot;
		descriptor->is_head_of_block = MALI_TRUE;
		descriptor->is_tail_of_block = MALI_TRUE;
		descriptor->embedded_mali_mem.size = slot_size_get(slot);
		descriptor->embedded_mali_mem.mali_addr = slot_mali_addr_get(area, slot);

		MALI_SUCCESS;
	}
	else 
	{
		MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
	}
}

void mali_mmu_virtual_address_range_free(arch_mem *descriptor)
{
	virtual_memory_area *area;
	virtual_memory_area_slot *slot = NULL;

	MALI_DEBUG_ASSERT_POINTER(descriptor);
	slot = MALI_REINTERPRET_CAST(virtual_memory_area_slot *)descriptor->cookie;
	MALI_DEBUG_ASSERT_POINTER(slot);

	area = find_virtual_memory_area(descriptor);

	backend_mmu_virtual_address_range_free(area, slot);
	descriptor->cookie = 0;
}
