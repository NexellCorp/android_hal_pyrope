/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_types.h>
#include <base/mali_debug.h>
#include <base/common/gp/base_common_gp_cmdlist.h>
#include <mali_gp_plbu_cmd.h>

#include <stddef.h>

/* config for the command list builder */
#define MALI_COMMAND_LIST_COMMAND_SIZE 8
#define MALI_COMMAND_LIST_JUMP_INSTRUCTIONS 1
#define MALI_COMMAND_LIST_ALIGNMENT_BYTES 64


/* map the current(last) mem block and update start, pos and left fields */
MALI_STATIC void cmdlist_current_map(mali_gp_cmd_list * list);
/* unmap the current(last) mem block and update start and current pos fields. Left is NOT updated for possible remapping of the last mapped block */
MALI_STATIC void cmdlist_current_unmap(mali_gp_cmd_list * list);
/* Reset the end address so that all commands in the command list are executed */
MALI_STATIC void _mali_base_common_gp_cmdlist_reset_end_address( mali_gp_cmd_list * list );

mali_gp_cmd_list * _mali_base_common_gp_cmdlist_create(u32 commands_per_increment)
{
	mali_gp_cmd_list * result;

	MALI_DEBUG_ASSERT(commands_per_increment > MALI_COMMAND_LIST_JUMP_INSTRUCTIONS, ("Command list increment too small: %d", commands_per_increment));

	result = MALI_REINTERPRET_CAST(mali_gp_cmd_list *)_mali_sys_calloc(1, sizeof(mali_gp_cmd_list));
	if (NULL == result) return NULL;

	result->mapping_default_size = commands_per_increment;
	result->mapping_size = commands_per_increment;
	result->mapping_reserved = MALI_COMMAND_LIST_JUMP_INSTRUCTIONS;
	result->first = _mali_mem_alloc(NULL, result->mapping_size * MALI_COMMAND_LIST_COMMAND_SIZE, MALI_COMMAND_LIST_ALIGNMENT_BYTES, MALI_GP_READ);
	result->last = result->first;
	result->end_address = 0;

	/* result->inlined.mapping is set up by cmdlist_current_map */
	result->inlined.mapping_left = result->mapping_size - result->mapping_reserved;

	if (NULL == result->first)
	{
		_mali_sys_free(result);
		return NULL;
	}

	cmdlist_current_map(result);

	_mali_base_common_gp_cmdlist_reset_end_address( result );

	return result;
}

void _mali_base_common_gp_cmdlist_reset(mali_gp_cmd_list * list)
{
	mali_mem_handle list_to_free;

	if (NULL != list->mapping_start)
	{
		cmdlist_current_unmap(list);
	}

	list_to_free = _mali_mem_list_remove_item(list->first);
	if (list_to_free)
	{
		_mali_mem_list_free(list_to_free);
	}

	list->last = list->first;
	list->end_address = 0;
	list->inlined.mapping_left = list->mapping_default_size - list->mapping_reserved;
	cmdlist_current_map(list);
}

void _mali_base_common_gp_cmdlist_destroy(mali_gp_cmd_list * list)
{
	MALI_DEBUG_ASSERT_POINTER(list);
	if (NULL == list) return;

	MALI_DEBUG_ASSERT(NULL == list->mapping_start, ("Destroy called on an active command list"));
	if (NULL != list->mapping_start)
	{
		cmdlist_current_unmap(list);
	}

	_mali_mem_list_free(list->first);
	list->first = NULL;
	_mali_sys_free(list);

	return;
}

MALI_EXPORT u64 * _mali_base_common_gp_cmdlist_extend(mali_gp_cmd_list_handle handle, u32 min_count)
{
	mali_mem_handle newblock;
	mali_gp_cmd_list * list;
	list =  (mali_gp_cmd_list*)(((u8*)handle) - offsetof(mali_gp_cmd_list, inlined));

	/* allocate a new buffer */
	newblock = _mali_mem_alloc(NULL, MAX(min_count, list->mapping_default_size) * MALI_COMMAND_LIST_COMMAND_SIZE, MALI_COMMAND_LIST_ALIGNMENT_BYTES, MALI_GP_READ);
	if (NULL == newblock)
	{
		return NULL;
	}

	/* add a jump to the new alloc */
	*list->inlined.mapping = GP_PLBU_COMMAND_LIST_JUMP(_mali_mem_mali_addr_get(newblock, 0));

	/* swap the mapping */
	cmdlist_current_unmap(list);
	_mali_mem_list_insert_after(list->last, newblock);
	list->last = newblock;
	cmdlist_current_map(list);

	return list->inlined.mapping;
}

void _mali_base_common_gp_cmdlist_done(mali_gp_cmd_list * list)
{
	/* when done unmap the current block, which will transfer the conents to mali memory if was buffered in CPU memory */
	MALI_DEBUG_ASSERT_POINTER(list);
	if (NULL != list->mapping_start) cmdlist_current_unmap(list);
	_mali_base_common_gp_cmdlist_reset_end_address(list);
}

MALI_STATIC void cmdlist_current_map(mali_gp_cmd_list * list)
{
	MALI_DEBUG_ASSERT_POINTER(list);
	MALI_DEBUG_ASSERT(NULL == list->mapping_start, ("Command list already has a mapping"));

	/* map the last block added. We never read the memory and therefore can skip read access and pre update */
	list->mapping_start = _mali_mem_ptr_map_area(	list->last,
													0, list->mapping_size * MALI_COMMAND_LIST_COMMAND_SIZE,
													MALI_COMMAND_LIST_ALIGNMENT_BYTES, MALI_MEM_PTR_WRITABLE | MALI_MEM_PTR_NO_PRE_UPDATE);


	MALI_DEBUG_ASSERT(NULL != list->mapping_start, ("This code was written under the assumption mali memory is always mapped, so this call should never return NULL"));

	list->inlined.mapping = list->mapping_start;
	list->inlined.mapping_left = list->mapping_size - list->mapping_reserved;
}

MALI_STATIC void cmdlist_current_unmap(mali_gp_cmd_list * list)
{
	MALI_DEBUG_ASSERT_POINTER(list);
	MALI_DEBUG_ASSERT_POINTER(list->mapping_start);

	_mali_mem_ptr_unmap_area(list->last);
	list->mapping_start = NULL;
	list->inlined.mapping = NULL;
	/* we do NOT update the mapping_left field since it's used during chaining */
}

MALI_STATIC void _mali_base_common_gp_cmdlist_reset_end_address( mali_gp_cmd_list * list )
{
	u32 end_offset;
	MALI_DEBUG_ASSERT_POINTER(list);

	end_offset = (list->mapping_size - list->inlined.mapping_left - list->mapping_reserved) * MALI_COMMAND_LIST_COMMAND_SIZE;
	list->end_address = _mali_mem_mali_addr_get(list->last, end_offset);
}
