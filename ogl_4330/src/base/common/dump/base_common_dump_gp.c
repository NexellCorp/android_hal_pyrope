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

#include <base/common/gp/base_common_gp_job.h>

/* Needed for mali_gp_try_start_result */
#include <base/arch/base_arch_gp.h>
#include <base/common/dump/base_common_dump_jobs_functions.h>

#define GET_DUMP_GP_INFO_FROM_INFO(ptr) ((mali_dump_job_info_gp*)(((u8*)(ptr)) - offsetof(mali_dump_job_info_gp, gp)))


/** Store the memory_handle that contains the pointer_array used by the PLB.
 * This is necessary to know to be able to patch the memory before dumping when DUMP_CRASHED is enabled.
 * Called from \a _internal_flush() in \a m200_gp_framebuilder.c through \a _mali_gp_job_set_plbu_pointer_array()
*/
void _mali_common_dump_gp_set_plbu_pointer_array(mali_gp_job_handle job_handle, mali_mem_handle pointer_array_handle)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);

	/* Store the pointer_array_handle given by the user to the dump_info struct in this GP-job */
	job->dump_info.pointer_array_user = pointer_array_handle;

	/* In function _mali_common_dump_gp_pre_start() we take a copy of the contents in
	this pointer_array_user before job is started */
}

/**
 * Dumping the register writes that are done during PLBU heap growth.
 * Called from \a _mali_base_common_gp_job_suspended_event() in \a base_comon_gp_job.c
 */
void _mali_common_dump_gp_heap_grow(mali_gp_job *job, u32 heap_start, u32 heap_end)
{
	mali_file * file;

	if ( MALI_FALSE == _mali_dump_system_is_enabled(job->ctx) || MALI_TRUE == job->dump_info.gp.disable_dump ) return;
	if ( MALI_FALSE == job->dump_info.gp.pre_run_dump_done)  return;

	file = _mali_base_common_dump_file_open(job->ctx);
	if (NULL==file)
	{
		MALI_DEBUG_TPRINT(1, ("Could not open dump file.\n"));
		return;
	}

	if ( heap_start == heap_end ) /* Not a valid heap, reset core */
	{
		if (_mali_dump_system_verbose_printing_is_enabled(job->ctx) )
		{
			MALI_DEBUG_PRINT(1, ("DumpGP: Heap could not grow. Writing reset.\n"));
		}
		_mali_sys_fprintf(file, "reset\n" );
		_mali_base_common_dump_file_close(file);
		return;
	}

	if (_mali_dump_system_verbose_printing_is_enabled(job->ctx) )
	{
		MALI_DEBUG_PRINT(1, ("Dumping Heap grow.\n"));
	}

	_mali_sys_fprintf(file, "#\n# PLBU Heap Grow. Continue with new Heap. GP Job nr: %d\n", job->dump_info.gp.job_nr);
	_mali_sys_fprintf(file, "writereg 28 00000024  # Clear OOM interrupt and potential hang interrupt \n");
	_mali_sys_fprintf(file, "writereg 2c 000003fe  # Set interrupt mask: wait for PLBU\n");
	_mali_sys_fprintf(file, "writereg 10 %08x  # Set Heap start\n", heap_start);
	_mali_sys_fprintf(file, "writereg 14 %08x  # Set Heap end\n", heap_end);
	_mali_sys_fprintf(file, "writereg 20 %08x  # Update PLBU allocation\n", 16);
	_mali_sys_fprintf(file, "wait posedge irq\n");

	_mali_base_common_dump_file_close(file);
}


/**
 * Setting dump_info pointer in TLS, so the settings can be acquired
 * when a new job is started in current callback. Dump memory if
 * DUMP_RESULT is set. If DUMP_CRASHED is set, it patch memory and
 * dump the job if it has crashed */
mali_sync_handle _mali_common_dump_gp_pre_callback(mali_gp_job * job, mali_job_completion_status completion_status)
{
	mali_sync_handle dump_sync;

	_mali_base_common_dump_pre_callback( job->ctx, &(job->dump_info.gp),
										 completion_status, &(job->registers[0])
									   );
	dump_sync = job->dump_info.gp.dump_sync;
	return dump_sync;
}

/**
 * Setting the TLS to not running in callback.
 * Release dump_info->dump_sync so that _mali_common_dump_gp_try_start() can return
 * if waiting on job complete is enabled.
*/
void _mali_common_dump_gp_post_callback(mali_base_ctx_handle ctx, mali_sync_handle dump_sync)
{
	_mali_base_common_dump_job_post_callback(ctx, dump_sync);
}


/** Filling the job->dump_info struct */
void _mali_common_dump_gp_pre_start(mali_gp_job *job)
{
	/* Filling the job->dump_info.gp struct */
	_mali_base_common_dump_job_info_fill(job->ctx, MALI_DUMP_GP, &(job->dump_info.gp) );

	/* Filling the rest of the job->dump_info struct (only gp part is set) */
	job->dump_info.pointer_array_patch_backup = NULL;

	/* Store the first elements in the pointer array so that it can be reconstructed if we must do a crash-dump */
	if ( (MALI_FALSE!=job->dump_info.gp.crash_dump_enable) && (NULL!=job->dump_info.pointer_array_user) )
	{
		_mali_mem_read( job->dump_info.pointer_array_first_elements,
						job->dump_info.pointer_array_user, 0,
	  					sizeof(job->dump_info.pointer_array_first_elements)
					  );
	}
}


/** This is used when we try to dump the gp_job after it crashed.
 * To be able to do that we must patch the pointer array so it becomes as it was before job start.
 * In this process the crashed pointer array is restored before in the unpatching which is done
 * after the dumping has been performed.
 * The algorithm used will not work if several GP jobs are used after eachother on the same frame
 * without resetting the pointer_array before starting the job.
 * This is because we for speed reasons only take a backup of the first part of the array before
 * the job starts, and when crash occure tries to recreate the whole buffer based on these values.
 */
void _mali_common_dump_gp_patch_pointer_array(mali_dump_job_info * dump_info)
{
	mali_dump_job_info_gp * dump_info_gp;
	u32 sizeB;
	u32 tile_diff_B;
	u32 i;
	u32 * mapped;
	u32 addr_write;

	dump_info_gp = GET_DUMP_GP_INFO_FROM_INFO(dump_info);

	/* Nothing to patch if this variable is not set by the user */
	if ( NULL==dump_info_gp->pointer_array_user )
	{
		return;
	}

	/* The algorithm below assume that all jobs have fixed distances between the pointers
	   in the pointer array when they start This assumption is true for the frame builder
	   today. But it might be changed in the future, so be aware of this. The array we patch
	   is created in _mali_frame_builder_init_pointer_array(..) in m200_gp_frame_builder.c */

	/* This is the distance between the first two pointers in the pointer array. */
	tile_diff_B = dump_info_gp->pointer_array_first_elements[1] - dump_info_gp->pointer_array_first_elements[0];

	/* Check that this distance is the same for the next 2 elements */
	if( (dump_info_gp->pointer_array_first_elements[2]!=dump_info_gp->pointer_array_first_elements[1]+tile_diff_B) ||\
		(dump_info_gp->pointer_array_first_elements[3]!=dump_info_gp->pointer_array_first_elements[2]+tile_diff_B)
	  )
	{
		MALI_DEBUG_ERROR(("Offset not constant in pointer_array. Can not dump.\n"));
		dump_info_gp->pointer_array_patch_backup = NULL;
		return;
	}

	/* Allocate memory that will store a copy of the pointer_array before we patch it,
	   so we can upatch it after the dump is finished */
	sizeB = _mali_mem_size_get(dump_info_gp->pointer_array_user);
	dump_info_gp->pointer_array_patch_backup = _mali_sys_malloc(sizeB);
	if ( NULL == dump_info_gp->pointer_array_patch_backup )
	{
		MALI_DEBUG_ERROR(("Could not do automatic patching of memory for dumping crashed gp-job.\n"));
		return;
	}

	/* Copy the memory we will patch to the backup memory */
	_mali_mem_read(dump_info_gp->pointer_array_patch_backup , dump_info_gp->pointer_array_user, 0, sizeB );

	/* Mapping memory for patching*/
	mapped = _mali_mem_ptr_map_area(dump_info_gp->pointer_array_user, 0, sizeB, 8, MALI_MEM_PTR_WRITABLE);
	if( NULL == mapped )
	{
		MALI_DEBUG_ERROR(("Could not map memory. Can not dump.\n"));
		_mali_sys_free(dump_info_gp->pointer_array_patch_backup);
		dump_info_gp->pointer_array_patch_backup = NULL;
		return;
	}

	/* Do the actual patching */
	addr_write = dump_info_gp->pointer_array_first_elements[0];
	MALI_DEBUG_PRINT(1,("Mali Frame number:%4d GP job:%4d Pathcing pointer array to original values\n", dump_info_gp->gp.frame_nr, dump_info_gp->gp.job_nr));
	for(i=0; i<(sizeB/4); i++)
	{
		mapped[i]  = addr_write;
		addr_write += tile_diff_B;
	}
	/* Patching finished.*/
	_mali_mem_ptr_unmap_area(dump_info_gp->pointer_array_user);
	/* All resources is released except from dump_info_gp->pointer_array_patch_backup,
	   which will be released by unpatch_pointer_array() */
}

/** Function restores the memory pathced by function
 * _mali_common_dump_gp_patch_pointer_array() */
void _mali_common_dump_gp_unpatch_pointer_array(mali_dump_job_info * dump_info)
{
	mali_dump_job_info_gp * dump_info_gp;
	dump_info_gp = GET_DUMP_GP_INFO_FROM_INFO(dump_info);

	if ( NULL != dump_info_gp->pointer_array_patch_backup )
	{
		MALI_DEBUG_PRINT(1,("Mali Frame number:%4d GP job:%4d Unpatching previous patch.\n",
							dump_info_gp->gp.frame_nr, dump_info_gp->gp.job_nr));

		_mali_mem_write(dump_info_gp->pointer_array_user, 0,
						dump_info_gp->pointer_array_patch_backup,
						_mali_mem_size_get(dump_info_gp->pointer_array_user) );

		_mali_sys_free(dump_info_gp->pointer_array_patch_backup);
		dump_info_gp->pointer_array_patch_backup = NULL;
	}
}

/**
 * Writes all registers in the job to the dump file, and load command to load the corresponding memory dump.
 * This makes it possible to analyze or simulate what the mali core actually did in the job.
 * Called from \a _mali_base_common_dump_pre_start_dump() and \a _mali_base_common_dump_pre_callback()
 * in file \a base_common_dump_jobs.c
 */
void _mali_common_dump_gp_regs(mali_base_ctx_handle ctx, mali_file *file_regs, u32 * reg_array)
{
	mali_bool is_vs_job;
	mali_bool is_plbu_job;
	u32 start_cmd;
	int i;

	MALI_IGNORE(ctx);

	if (NULL==file_regs) return;

	for(i=0;i<6;i++)
	{
		_mali_sys_fprintf(file_regs, "writereg %02x %08x\n", i*4, reg_array[i]);
	}

	/* Checking if the start and stop address differ, in that case they should run */
	is_vs_job   = (reg_array[0] != reg_array[1]);
	is_plbu_job = (reg_array[2] != reg_array[3]);
	/* Setting correct start cmd. Bit 0 starts VS, Bit 1 start PLBU */
	start_cmd  = is_vs_job   ? 0x01 : 0x00 ;
	start_cmd |= is_plbu_job ? 0x02 : 0x00 ;

	if( is_plbu_job)
	{
		_mali_sys_fprintf(file_regs, "writereg 2c 000003fe  # Set interrupt mask: wait for PLBU\n");
	}
	else
	{
		_mali_sys_fprintf(file_regs, "writereg 2c 000003fd  # Set interrupt mask: wait for VS\n");
	}

	_mali_sys_fprintf(file_regs, "writereg 20 %08x  # Start GP Core\n", start_cmd);
	_mali_sys_fprintf(file_regs, "wait posedge irq\n");

}

mali_bool _mali_common_dump_gp_enable_dump(mali_gp_job_handle job_handle, mali_bool enable)
{
	mali_gp_job * job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job);

	return _mali_base_common_dump_job_enable_dump(&(job->dump_info.gp), enable);
}
