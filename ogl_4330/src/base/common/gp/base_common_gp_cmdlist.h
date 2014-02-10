/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __BASE_COMMON_GP_CMD_LIST_H__
#define __BASE_COMMON_GP_CMD_LIST_H__

#include <base/mali_types.h>
#include <base/mali_debug.h>
#include <base/mali_memory.h>
#include <base/gp/mali_gp_handle.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Definition of the command list type.
 * A command list is built up of one or more mali memory blocks.
 * If more than one block is needed for the commands a special jump
 * instruction links the blocks together.
 *
 * @note The jump instruction for the PLBU and VS is currently the same. If that changes this code need to know for which unit it's building a command list.
 */
typedef struct mali_gp_cmd_list
{
	struct mali_gp_cmd_list_inline inlined;

	mali_mem_handle first;	/**< first mali memory block, this is the start of the command list */
	mali_mem_handle last; /**< last memory block. This is the block we are currently working on */

	u32   skip_count; /**< The first N instructions are usually used to set up the unit.
					   * If linking jobs this setup can be skipped. This field specifies how many instructions we should skip.
					   */

	u32   mapping_default_size; /**< Number of commands to minimum allocate in a new block */
	u32   mapping_size; /**< Number of instructions per memory block / mapping */
	u32   mapping_reserved; /**< We reserve some space for the linking of jobs, this specifies how may instructions we need */

	u64 * mapping_start; /**< Current cpu mapping of the mali memory block */

	mali_addr end_address; /**< The address of the last command to execute */
}mali_gp_cmd_list;


/**
 * Create a new GP command list.
 * Used to represent command lists for the VS and PLBU units on the GP.
 * The list will be initialized to 'size_increment' size,
 * and will be extended by this amount when the current buffer is full.
 * Commands are written to this buffer using the @see _mali_base_common_gp_cmdlist_write command.
 * @param size_increment Initial size and size of the increments when the buffer is extended.
 * @return A pointer to a new command list or NULL on error
 */
mali_gp_cmd_list * _mali_base_common_gp_cmdlist_create(u32 size_increment);

/**
 * Reset GP command list.
 * Used to revert a list back to the state it had on list creation.
 * More optimal than destroying the list and then creating a new one.
 * @param list The list to reset
 */
void _mali_base_common_gp_cmdlist_reset(mali_gp_cmd_list * list);

/**
 * Destroy a GP command list.
 * Frees the command list and all its resources.
 * @param list The command list to destroy.
 */
void _mali_base_common_gp_cmdlist_destroy(mali_gp_cmd_list * list);

/**
 * Complete the command list building sequence.
 * Should be called after the last command has been written.
 * @param list The command list to operate on
 */
void _mali_base_common_gp_cmdlist_done(mali_gp_cmd_list * list);

/**
 * Get command list start addresses, as seen by Mali.
 * @param list The command list to get the address of
 * @return The Mali seen address of the start of the command list
 */
MALI_STATIC_FORCE_INLINE mali_addr _mali_base_common_gp_cmdlist_get_start_address(mali_gp_cmd_list * list)
{
	MALI_DEBUG_ASSERT_POINTER(list);
	return _mali_mem_mali_addr_get(list->first, 0);
}

/**
 * Get command list end addresses, as seen by Mali.
 * @param list The command list to get the address of
 * @return The Mali seen address of the last instruction of the command list
 */
MALI_STATIC_FORCE_INLINE mali_addr _mali_base_common_gp_cmdlist_get_end_address(mali_gp_cmd_list * list)
{
	MALI_DEBUG_ASSERT_POINTER(list);
	MALI_DEBUG_ASSERT(0 != list->end_address, ("Calling get_end_address on a GP command list which is still building"));
	return list->end_address;
}


#ifdef __cplusplus
}
#endif

#endif /* __BASE_COMMON_GP_CMD_LIST_H__ */
