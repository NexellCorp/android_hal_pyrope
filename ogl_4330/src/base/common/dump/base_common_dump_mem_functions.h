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
 * @file base_common_dump_mem_functions.h
 * Contains a function used by the dump system.
 * Used for dumping all memory allocated to a file.
 */

#ifndef _BASE_COMMON_DUMP_MEM_FUNCTIONS_H_
#define _BASE_COMMON_DUMP_MEM_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <base/mali_types.h>

/**
 * Dump all memory allocated by the given context to a file.
 * The dumpname is given by the environment variable
 * MALI_DUMP_FILE_NAME_PREFIX
 * or is set to "mali_dump" if the environment variable is unset.
 * Then the filename will contain a counter, and then the given
 * start comment.
 */
mali_err_code _mali_base_common_dump_mem_all(
	mali_base_ctx_handle ctx,
	mali_file *file_regs,
	char* file_name_buffer,
	u32 file_name_buffer_lenght,
	const char* pre_comment,
	u32 number,
	const char* post_comment,
	u32 frame_nr,
	mali_bool is_for_pp,
	u32 split_index
	) MALI_CHECK_RESULT;

#if defined(USING_MALI450)
	extern u32 _mali_base_common_dump_mem_m450_master_tile_list_physical_address;
#endif /* defined(USING_MALI450) */

#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DUMP_MEM_H_ */
