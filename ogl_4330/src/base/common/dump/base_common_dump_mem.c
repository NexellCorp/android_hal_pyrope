/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_types.h>
#include <base/mali_debug.h>
#include <base/mali_runtime.h>
#include <base/mali_memory.h>
#include "../mem/base_common_mem.h"
#include "base_common_dump_mem_functions.h"
#include "base_common_dump_jobs_functions.h"

#include <base/arch/base_arch_mem.h> /* for struct mali_mmu_dump */

#define MALI_DEFAULT_DUMP_FILE_NAME "mali_dump"

#if defined(USING_MALI450)
u32 _mali_base_common_dump_mem_m450_master_tile_list_physical_address = 0xcafe0000;
#endif /* defined(USING_MALI450) */


/* Code used by MMU dumping */
MALI_STATIC void dump_4k_page(u32 *page, u32 phys_addr, mali_file *file)
{
	int i;
	const int nr_bytes_pr_line = 4*sizeof(*page);

	for (i = 0; i < (4096/nr_bytes_pr_line); i++)
	{
		_mali_sys_fprintf(
			file,
			"%08X: %08X %08X %08X %08X\n",
			phys_addr + 16*i,
			page[i*4 + 0],
			page[i*4 + 1],
			page[i*4 + 2],
			page[i*4 + 3]);
	}
}

MALI_STATIC mali_err_code dump_mmu_mem(
	mali_base_ctx_handle  ctx,
	const char           *mmu_dump_file_name,
	mali_file            *file_regs,
	mali_bool             is_for_pp,
	u32                   split_index)
{
	u32            i;
	u32            phys_addr;
	mali_file     *mmu_dump_file = NULL;
	mali_err_code  err;
	mali_mmu_dump  dump;
	u32            mmu_dump_size;

	MALI_IGNORE(ctx);

	_mali_sys_memset(&dump, 0, sizeof(dump));
	mmu_dump_size = _mali_base_arch_mmu_dump_size_get();

	if (0 == mmu_dump_size)
	{
		return MALI_ERR_FUNCTION_FAILED;
	}

	if (MALI_FALSE == is_for_pp || 0 == split_index)
	{
		mmu_dump_file = _mali_sys_fopen(mmu_dump_file_name, "w");

		if (NULL == mmu_dump_file)
		{
			return MALI_ERR_FUNCTION_FAILED;
		}
	}

	dump.storage_buffer_size = mmu_dump_size;
	dump.storage_buffer = _mali_sys_malloc(dump.storage_buffer_size);

	err = MALI_ERR_OUT_OF_MEMORY;
	if (NULL != dump.storage_buffer)
	{
		err =_mali_base_arch_mmu_dump_get(&dump);
		if (MALI_ERR_NO_ERROR == err )
		{
			if  (MALI_FALSE == is_for_pp || 0 == split_index)
			{
				u32 *page_ptr = dump.pages;
				for (i = 0; i < dump.num_pages; i++)
				{
#if defined(USING_MALI450)
					_mali_base_common_dump_mem_m450_master_tile_list_physical_address = page_ptr[768+1] & 0xfffffff0;
#endif /* defined(USING_MALI450) */
					phys_addr = *page_ptr;
					page_ptr++;
					dump_4k_page(page_ptr, phys_addr, mmu_dump_file);
					page_ptr += 4096/sizeof(*page_ptr);
				}
			}

			if (NULL != file_regs)
			{
				u32 register_prefix;

				if (is_for_pp)
				{
					register_prefix = (0x12 << 24) | (split_index << 16);
				}
				else
				{
					register_prefix = 0x02 << 24;
				}

				_mali_sys_fprintf(file_regs, "# MMU page tables\n");

				if (MALI_FALSE == is_for_pp || 0 == split_index)
				{
					_mali_sys_fprintf(
						file_regs,
						"#MMU_ONLY_COMMAND load_mem %s # Page Tables\n",
						mmu_dump_file_name);
				}

				_mali_sys_fprintf(file_regs, "# MMU registers\n");

				for (i = 0; i < dump.num_registers; i++)
				{
					_mali_sys_fprintf(
						file_regs,
						"#MMU_ONLY_COMMAND writereg %08x %08x\n",
						register_prefix | dump.register_writes[i*2],
						dump.register_writes[1 + i*2]);
				}

				_mali_sys_fprintf(file_regs, "# MMU end\n#\n");
			}
		}

		_mali_sys_free(dump.storage_buffer);
	}

	if (MALI_FALSE == is_for_pp || split_index == 0)
	{
		_mali_sys_fclose(mmu_dump_file);
	}

	return err;
}

mali_err_code _mali_base_common_dump_mem_all(
	mali_base_ctx_handle  ctx,
	mali_file            *file_regs,
	char                 *file_name_buffer,
	u32                   file_name_buffer_length,
	const char           *pre_comment,
	u32                   number,
	const char           *post_comment,
	u32                   frame_nr,
	mali_bool             is_for_pp,
	u32                   split_index)
{
	mali_file     *file = NULL;
	const char    *file_name_prefix;
	const char    *delimiter = NULL;
	u32            mem_print_format;
	mali_err_code  err;
	mali_bool      print_one_liners;
	int            needed_length;
	const char    *dump_path_env;

	MALI_DEBUG_ASSERT_POINTER(file_name_buffer);
	if (NULL == pre_comment) pre_comment="";
	if (NULL == post_comment) post_comment="";

	/* If this is MALI_TRUE, the dump system should not dump registers or memory content.  It
	   should then only dump information about all memory handles on the system.  This is for
	   profiling of memory usage. */
	print_one_liners = _mali_dump_system_mem_print_one_liners(ctx);

	dump_path_env = _mali_sys_config_string_get("MALI_OUTPUT_FOLDER");

	file_name_prefix = MALI_DEFAULT_DUMP_FILE_NAME;

	/* FIRST, DO THE MMU DUMP - Only if we should dump actual memory (not the one line
	   profiling). */
	if (MALI_FALSE == print_one_liners)
	{
		file_name_buffer[0] = 0;
		if (NULL != dump_path_env)
		{
			needed_length = _mali_sys_snprintf(
				file_name_buffer,
				file_name_buffer_length,
				"%s/%s_%04d_%s_%03d_%s_%s.hex",
				dump_path_env,
				file_name_prefix,
				frame_nr,
				pre_comment,
				number,
				post_comment,
				"mmu");
		}
		else
		{
			needed_length = _mali_sys_snprintf(
				file_name_buffer,
				file_name_buffer_length,
				"%s_%04d_%s_%03d_%s_%s.hex",
				file_name_prefix,
				frame_nr,
				pre_comment,
				number,
				post_comment,
				"mmu");
		}

		if ((needed_length < 20) || ((u32) needed_length > file_name_buffer_length))
		{
			MALI_DEBUG_ERROR(("Dump system: could not create file dump name.\n"));
			_mali_sys_config_string_release(dump_path_env);
			return MALI_ERR_FUNCTION_FAILED;
		}

		err = dump_mmu_mem(ctx, file_name_buffer, file_regs, is_for_pp, split_index);

		if (MALI_ERR_NO_ERROR != err)
		{
			MALI_DEBUG_PRINT(3, ("Dump system: could not open dump file.\n"));
			/* We continue, assuming the reason was wrong devicedriver. */
		}
	}

	/* Then, dump Mali memory. */
	file_name_buffer[0] = 0;

	if (NULL != dump_path_env)
	{
		needed_length = _mali_sys_snprintf(
			file_name_buffer ,
			file_name_buffer_length,
			"%s/%s_%04d_%s_%03d_%s.hex",
			dump_path_env,
			file_name_prefix,
			frame_nr,
			pre_comment,
			number,
			post_comment);
	}
	else
	{
		needed_length = _mali_sys_snprintf(
			file_name_buffer ,
			file_name_buffer_length,
			"%s_%04d_%s_%03d_%s.hex",
			file_name_prefix,
			frame_nr,
			pre_comment,
			number,
			post_comment);
	}

	_mali_sys_config_string_release(dump_path_env);

	if ((needed_length < 20) || ((u32) needed_length > file_name_buffer_length))
	{
		MALI_DEBUG_ERROR(("Dump system: could not create file dump name.\n"));
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* The dump tests expects this to be printed also for release builds. */
	if (_mali_dump_system_verbose_printing_is_enabled(ctx))
	{
		_mali_sys_printf("Dumping to file: %s\n", file_name_buffer);
	}

	if (MALI_FALSE == is_for_pp || 0 == split_index)
	{
		file = _mali_sys_fopen(file_name_buffer, "w");
		if (NULL == file)
		{
			MALI_DEBUG_ERROR(("Dump system: could not open dump file.\n"));
			return MALI_ERR_FUNCTION_FAILED;
		}

		mem_print_format = 0;

		if (_mali_dump_system_verbose_printing_is_enabled(ctx))
		{
			mem_print_format = MALI_MEM_PRINT_VERBOSE;
		}

		if (MALI_FALSE != print_one_liners)
		{
			mem_print_format = mem_print_format | MALI_MEM_PRINT_ONE_LINERS;
			/* The delimitor to use beween each field in memory dumps. */
			delimiter = _mali_dump_system_mem_print_one_liners_delimiter(ctx);
		}

		_mali_base_common_mem_dump_all_mem(ctx, file, mem_print_format, delimiter);
		_mali_sys_fclose(file);

		if (MALI_FALSE == print_one_liners && NULL != file_regs)
		{
			_mali_sys_fprintf(file_regs, "load_mem %s\n", file_name_buffer);
		}
	}

	return MALI_ERR_NO_ERROR;
}
