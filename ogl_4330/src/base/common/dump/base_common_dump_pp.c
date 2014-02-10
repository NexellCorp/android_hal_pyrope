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
#include <base/pp/mali_pp_job.h>

#include <base/common/pp/base_common_pp_job.h>

#if defined(USING_MALI450)
/* exposes _mali_base_common_dump_mem_m450_master_tile_list_physical_address */
#include "base_common_dump_mem_functions.h"
#endif /* defined(USING_MALI450) */

/* Needed for mali_pp_try_start_result */
#include <base/arch/base_arch_pp.h>
#include <base/common/dump/base_common_dump_jobs_functions.h>

#define MGMT_REG_CTRL_MGMT_CMD_START_RENDER (1<<6)
#define MGMT_REG_CTRL_MGMT_IRQ_MASK_ALL     0x1ff

typedef enum mali_mgmt_reg_addr
{
	MGMT_REG_CTRL_MGMT			= 0x100C,
 	MGMT_REG_IRQ_MASK			= 0x1028
} mali_mgmt_reg_addr;

/**
 * Telling is type PP core and core nr
 * Following spec from intern trondheim wiki site: HW_common_testbench#APB_addressing
*/
MALI_INLINE u32 get_pp_core_dump_addr_prefix(u32 core_nr)
{
	u32 addr_prefix;

	#define MALI_TESTBENCH_CORE_TYPE_BIT_NR   24
	#define MALI_TESTBENCH_CORE_TYPE_PP       0x10
	#define MALI_TESTBENCH_CORE_NR_BIT_NR     16

	/* Setting this is CORE_TYPE_PP */
	addr_prefix  = MALI_TESTBENCH_CORE_TYPE_PP<<MALI_TESTBENCH_CORE_TYPE_BIT_NR;
	/* Setting this is the given core number */
	addr_prefix |= core_nr<<MALI_TESTBENCH_CORE_NR_BIT_NR;

	return addr_prefix ;
}

/**
 * Setting dump_info pointer in TLS, so the settings can be acquired when a new job
 * is started in current callback. Dump memory if DUMP_RESULT is set.
 * If DUMP_CRASHED is set, it patch memory and dump the job if it has crashed
 */
mali_sync_handle _mali_common_dump_pp_pre_callback(mali_pp_job * job, mali_job_completion_status completion_status)
{
	mali_sync_handle dump_sync;
	dump_sync = job->dump_info.dump_sync;
	_mali_base_common_dump_pre_callback( job->ctx, &(job->dump_info), completion_status, &(job));
	return dump_sync ;
}

/**
* Setting the TLS to not running in callback.
* Release dump_info->dump_sync so that _mali_common_dump_gp_try_start() can return
* if waiting on job complete is enabled.
*/
void _mali_common_dump_pp_post_callback(mali_base_ctx_handle ctx, mali_sync_handle dump_sync)
{
	_mali_base_common_dump_job_post_callback(ctx, dump_sync);
}


/** Filling the job->dump_info struct */
void _mali_common_dump_pp_pre_start(mali_pp_job *job)
{
	/* Filling the job->dump_info struct */
	_mali_base_common_dump_job_info_fill(job->ctx, MALI_DUMP_PP, &(job->dump_info) );
}

#if defined(USING_MALI450)
static void mali_dump_450_frame_info(mali_file *file_regs, m450_pp_job_frame_info *info)
{
	u32 master_tile_list_virtual_address  = 0xfff00000;
	_mali_sys_fprintf(file_regs, "#\n");
	_mali_sys_fprintf(file_regs, "# Register writes for the Mali450 Dynamic Load Balancing Unit\n");
	_mali_sys_fprintf(file_regs, "writereg 80014000 %08x # Physical address of DLB unit\n",
					_mali_base_common_dump_mem_m450_master_tile_list_physical_address | (1<<0));
	_mali_sys_fprintf(file_regs, "writereg 80014004 %08x # Virtual  address of DLB unit\n",
					master_tile_list_virtual_address);
	_mali_sys_fprintf(file_regs, "writereg 80014008 %08x # Tile list virtual base address\n",
					info->slave_tile_list_mali_address);
	_mali_sys_fprintf(file_regs, "writereg 8001400c %08x # Framebuffer dimensions\n",
					(info->master_x_tiles - 1) | ((info->master_y_tiles-1)<<16));
	_mali_sys_fprintf(file_regs, "writereg 80014010 %08x # Tile list configuration\n",
					info->binning_pow2_x | (info->binning_pow2_y<<16) | (info->size<<28));
	_mali_sys_fprintf(file_regs, "writereg 80014014 %08x # Start tile positions\n",
					0 | (0<<8) | ((info->master_x_tiles-1)<<16) | ((info->master_y_tiles-1)<<24) );
	_mali_sys_fprintf(file_regs, "#\n");
}
#endif /* defined(USING_MALI450) */

static void mali_dump_pp_regs(mali_file *file_regs, mali_pp_registers *registers, u32 core_nr)
{
	int i, reg_nr;
	u32 addr_prefix = 0;
 	int reg_array_length;

	addr_prefix = get_pp_core_dump_addr_prefix(core_nr);

	/* Frame registers */
#if defined(USING_MALI400) || defined(USING_MALI450)
	reg_array_length = 23;
#elif defined(USING_MALI200)
	reg_array_length = 20;
#endif

	reg_nr = 0;

	if (0 == core_nr)
	{
		_mali_sys_fprintf(file_regs, "writereg %08x %08x # %s\n", M200_FRAME_REG_REND_LIST_ADDR | addr_prefix, registers->frame_regs[0], mali_dump_get_register_name(reg_nr++));
	}
	else
	{
		_mali_sys_fprintf(file_regs, "writereg %08x %08x # %s\n", M200_FRAME_REG_REND_LIST_ADDR | addr_prefix, registers->frame_regs_addr_frame[core_nr-1], mali_dump_get_register_name(reg_nr++));
	}
	for(i = 1; i < 12; i++)
	{
		u32 value, reg_address, core_reg_addr;
		value = registers->frame_regs[i];
		reg_address = M200_FRAME_REG_REND_LIST_ADDR + (i * sizeof(mali_reg_value));
		core_reg_addr = reg_address | addr_prefix ;
		_mali_sys_fprintf(file_regs, "writereg %08x %08x # %s\n", core_reg_addr , value, mali_dump_get_register_name(reg_nr++));
	}
	if (0 == core_nr)
	{
		_mali_sys_fprintf(file_regs, "writereg %08x %08x # %s\n", (M200_FRAME_REG_FS_STACK_ADDR * 4) | addr_prefix, registers->frame_regs[12], mali_dump_get_register_name(reg_nr++));
	}
	else
	{
		_mali_sys_fprintf(file_regs, "writereg %08x %08x # %s\n", (M200_FRAME_REG_FS_STACK_ADDR * 4) | addr_prefix, registers->frame_regs_addr_stack[core_nr-1], mali_dump_get_register_name(reg_nr++));
	}
	for(i = 13; i < reg_array_length; i++)
	{
		u32 value, reg_address, core_reg_addr;
		value = registers->frame_regs[i];
		reg_address = M200_FRAME_REG_REND_LIST_ADDR + (i * sizeof(mali_reg_value));
		core_reg_addr = reg_address | addr_prefix ;
		_mali_sys_fprintf(file_regs, "writereg %08x %08x # %s\n", core_reg_addr , value, mali_dump_get_register_name(reg_nr++));
	}


	/* WBx registers */
	reg_array_length = 12;

	for(i = 0; i < reg_array_length; i++)
	{
		u32 value, reg_address, core_reg_addr;
		value = registers->wb0_regs[i];
		reg_address = 0x100 + (i * sizeof(mali_reg_value));
		core_reg_addr = reg_address | addr_prefix ;
		_mali_sys_fprintf(file_regs, "writereg %08x %08x # %s\n", core_reg_addr, value, mali_dump_get_register_name(reg_nr++));
	}
	for(i = 0; i < reg_array_length; i++)
	{
		u32 value, reg_address, core_reg_addr;
		value = registers->wb1_regs[i];
		reg_address = 0x200 + (i * sizeof(mali_reg_value));
		core_reg_addr = reg_address | addr_prefix ;
		_mali_sys_fprintf(file_regs, "writereg %08x %08x # %s\n", core_reg_addr, value, mali_dump_get_register_name(reg_nr++));
	}
	for(i = 0; i < reg_array_length; i++)
	{
		u32 value, reg_address, core_reg_addr;
		value = registers->wb2_regs[i];
		reg_address = 0x300 + (i * sizeof(mali_reg_value));
		core_reg_addr = reg_address | addr_prefix ;
		_mali_sys_fprintf(file_regs, "writereg %08x %08x # %s\n", core_reg_addr, value, mali_dump_get_register_name(reg_nr++));
	}

	_mali_sys_fprintf(file_regs, "writereg %08x %08x   # Set MGMT_IRQ_MASK_ALL\n",
					  MGMT_REG_IRQ_MASK |addr_prefix , MGMT_REG_CTRL_MGMT_IRQ_MASK_ALL);
	_mali_sys_fprintf(file_regs, "writereg %08x %08x   # Set MGMT_CMD_START_RENDER\n",
					  MGMT_REG_CTRL_MGMT|addr_prefix  ,MGMT_REG_CTRL_MGMT_CMD_START_RENDER);
	_mali_sys_fprintf(file_regs, "wait posedge irq\n");

	return;
}

/**
* Writes all registers in the job to the dump file, and load command to load the corresponding memory dump.
* This makes it possible to analyze or simulate what the mali core actually did in the job.
*/
void _mali_common_dump_pp_regs(mali_base_ctx_handle ctx, mali_file *file_regs, mali_pp_job * job, u32 subjob_nr)
{
	mali_pp_registers *registers;

	MALI_IGNORE(ctx);

	if (NULL==file_regs) return;

#if defined(USING_MALI450)
	if (0 == subjob_nr) mali_dump_450_frame_info(file_regs, &job->info);
#endif /* defined(USING_MALI450) */

	registers = &job->registers;

	mali_dump_pp_regs(file_regs, registers, subjob_nr);
}

mali_bool _mali_common_dump_pp_enable_dump(mali_pp_job_handle job_handle, mali_bool enable)
{
	mali_pp_job *job;
	job = MALI_REINTERPRET_CAST(mali_pp_job*)job_handle;

	MALI_DEBUG_ASSERT_POINTER(job);

	return _mali_base_common_dump_job_enable_dump(&(job->dump_info), enable);
}
