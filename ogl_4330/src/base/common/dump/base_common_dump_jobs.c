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
#include <base/common/base_common_context.h>

#include "../mem/base_common_mem.h"
#include "../base_common_sync_handle.h"
#include "base_common_dump_jobs_functions.h"
#include "base_common_dump_jobs_functions_frontend.h"
#include "base_common_dump_mem_functions.h"
#include "base_common_dump_gp_functions_backend.h"
#include "base_common_dump_pp_functions_backend.h"

#define MALI_MEM_PRINT_ONE_LINERS_DELIMITER_DEFAULT   "\t"

#ifdef __SYMBIAN32__
#define MALI_DEFAULT_DUMP_CONFIG_FILE_NAME "D:\\config.txt"
#else
#define MALI_DEFAULT_DUMP_CONFIG_FILE_NAME "config.txt"
#endif

/**  Datastructure used to generate the list of which jobs or frames that should be dumped */
typedef struct mali_dump_range_element
{
	u32 first;
	u32 last;
	struct mali_dump_range_element * next;
	struct mali_dump_range_element * prev;
} mali_dump_range_element;


/**
 * The context that stores all global data for the dumping system.
 * It is one mali_dump_context in each mali_base_ctx_handle,
 * which would normally be only one of in each process.
 * It is created from \a _mali_base_common_context_open_subsystems()
 * and it is destroyed by: \a _mali_base_common_context_destroy()
 */
typedef struct mali_dump_context
{
	mali_atomic_int frame_counter;		/**< Stores a job counter that increments for each new frame swap */

	mali_bool dump_system_enabled ;   /**< Toggles whether any of the dump system is enabled */
	mali_bool dump_result;            /**< Toggles whether jobs that were dumped before the started should be dumped after also*/
	mali_bool dump_crashed;           /**< Toggles whether crashed jobs should be dumped */

	/**
	 * The locker variable tells if the blue-lock for dumping is enabled.
	 * When this is enabled the job start function in the base driver is no longer asynchronous, which means
	 * that the job_start function will wait for the mali job to complete before returning.
	 */
	mali_bool locker_enabled;

	mali_bool verbose_printing;       /**< Toggles printing of dump status to console */
	mali_bool mem_print_one_liners;   /**< Toggles if memory dumps should write Excel friendly output, and not any memory content */
	char *    mem_print_one_liners_delimiter; /**< Stores the delimiter we would use if we enable mem_print_one_liners */


	mali_atomic_int pp_job_counter; /**< Stores a job counter that increments for each pp job start */
	mali_atomic_int gp_job_counter; /**< Stores a job counter that increments for each gp job start */

	/**
	 * \defgroup mali_dump_lists_range_pointers Global mali_dump_range_element pointer variables
	 * These are the pointers to the lists that keeps track on which job numbers and frame numbers that should be dumped.
	 * They are devided between "frame" and "job". The frame part keeps track on which frame numbers should be dumped.
	 * For frame numbers that should be dumped the system will dump all jobs in these frames.
	 * Frame number is stored globally in the thread, and is incremented from the framebuilder on swap.
	 * Job number is increased for each time the base driver receives a new job to render. It is a separate counter
	 * for Mali_PP and Mali_GP.
	 * When the system start, in the \a mali_dump_system_open function call, these lists are generated.
	 * When the shuts down, in the \a mali_dump_system_close function call, these lists are released.
	 * The list is double linked lists with elements that contain ranges of jobs_numbers or frame_numbers
	 * that should be dumped. The lists are sorted, and the lowest range/number is in the first element.
	 * Each list is pointed to by two variables: One pointer will always point to the first element in the list.
	 * The second pointer will point to the last used element in the list.
	 * The purpose of having a separate variable for the last used element is to increase speed of the function
	 * that checks if the next frame_nr or job_nr should be dumped.
	 * @{ */
	mali_dump_range_element * dump_list_range_frame_pp_first;
	mali_dump_range_element * dump_list_range_frame_pp;
	mali_dump_range_element * dump_list_range_frame_gp_first;
	mali_dump_range_element * dump_list_range_frame_gp;
	mali_dump_range_element * dump_list_range_job_pp_first;
	mali_dump_range_element * dump_list_range_job_pp;
	mali_dump_range_element * dump_list_range_job_gp_first;
	mali_dump_range_element * dump_list_range_job_gp;
	/** @} */

} mali_dump_context;


MALI_INLINE mali_dump_context * dump_ctx(mali_base_ctx_handle ctx);
MALI_INLINE mali_dump_job_info * job_info_get(mali_base_ctx_handle ctx);
MALI_STATIC mali_bool dump_gp_job_is_enabled(mali_base_ctx_handle ctx, u32 frame_nr, u32 job_nr_gp);
MALI_STATIC mali_bool dump_pp_job_is_enabled(mali_base_ctx_handle ctx, u32 frame_nr, u32 job_nr_pp);
MALI_STATIC mali_bool dump_crashed_is_enabled(mali_base_ctx_handle ctx);
MALI_DEBUG_CODE(MALI_STATIC void debug_list_print(mali_dump_context *dump_context);)
MALI_STATIC u32 pp_job_counter_inc_and_return(mali_base_ctx_handle ctx);
MALI_STATIC u32 gp_job_counter_inc_and_return(mali_base_ctx_handle ctx);
MALI_STATIC void verbose_printing_set(mali_base_ctx_handle ctx, mali_bool verbose);
MALI_STATIC u32 frame_counter_get(mali_base_ctx_handle ctx);
MALI_STATIC mali_bool check_number_is_in_dump_range(mali_base_ctx_handle ctx, u32 number,
											mali_dump_range_element ** dump_list_element);
MALI_STATIC mali_err_code generate_dump_list(mali_dump_range_element ** dump_list, const char * txt);
MALI_STATIC mali_dump_range_element * get_new_list_entry(u32 first, u32 last);
MALI_STATIC mali_err_code insert_element(mali_dump_range_element ** dump_list, u32 first, u32 last);
MALI_STATIC  void merge_element_if_possible(mali_dump_range_element * element);
MALI_STATIC void free_dump_range_list(mali_dump_range_element ** dump_list);


/** Prepares all the dumping data structures so that the other dumping functions can be used.
 * The function is not ref-counted. It is only legal to call it once, before calling function
 * \a mali_dump_system_close. The function must be called prior to any other function calls
 * to this dump API.
 * Function is called from \a _mali_base_common_context_open_subsystems()
 */
mali_err_code _mali_dump_system_open(mali_base_ctx_handle ctx)
{
	const char* readline;
	mali_bool dump_all;
	mali_dump_context *dump_context;

	MALI_DEBUG_ASSERT( NULL==(dump_ctx(ctx)) , ("Trying to open an already opened dump system."));

	dump_context = (mali_dump_context *) _mali_sys_calloc(1, sizeof(mali_dump_context));
	if ( NULL==dump_context) MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);

	/* This line is intentionally set here, since we in the error_cleanup function excpect this to be set to do the cleanup */
	_mali_base_common_context_set_dump_context(ctx, dump_context);

	/* Generating dump lists - the lists of which frame numbers that should be dumped */
	readline = _mali_sys_config_string_get("MALI_DUMP_FRAME");
	MALI_CHECK_GOTO(MALI_ERR_NO_ERROR==generate_dump_list(&dump_context->dump_list_range_frame_pp_first, readline), error_cleanup);
	MALI_CHECK_GOTO(MALI_ERR_NO_ERROR==generate_dump_list(&dump_context->dump_list_range_frame_gp_first, readline), error_cleanup);
	_mali_sys_config_string_release(readline);

	readline = _mali_sys_config_string_get("MALI_DUMP_FRAME_PP");
	MALI_CHECK_GOTO(MALI_ERR_NO_ERROR==generate_dump_list(&dump_context->dump_list_range_frame_pp_first, readline), error_cleanup);
	_mali_sys_config_string_release(readline);

	readline = _mali_sys_config_string_get("MALI_DUMP_FRAME_GP");
	MALI_CHECK_GOTO(MALI_ERR_NO_ERROR==generate_dump_list(&dump_context->dump_list_range_frame_gp_first, readline), error_cleanup);
	_mali_sys_config_string_release(readline);

	/* Generating dump lists job- the lists of which job numbers that should be dumped */
	readline = _mali_sys_config_string_get("MALI_DUMP_JOB_PP");
	MALI_CHECK_GOTO(MALI_ERR_NO_ERROR==generate_dump_list(&dump_context->dump_list_range_job_pp_first, readline), error_cleanup);
	_mali_sys_config_string_release(readline);

	readline = _mali_sys_config_string_get("MALI_DUMP_JOB_GP");
	MALI_CHECK_GOTO(MALI_ERR_NO_ERROR==generate_dump_list(&dump_context->dump_list_range_job_gp_first, readline), error_cleanup);
	_mali_sys_config_string_release(readline);

	readline = _mali_sys_config_string_get("MALI_MEM_PRINT_ONE_LINERS_DELIMITER");
	if ( (NULL!=readline)&& (0!=_mali_sys_strlen(readline)) )
	{
		dump_context->mem_print_one_liners_delimiter = _mali_sys_strdup(readline);
	}
	else
	{
		dump_context->mem_print_one_liners_delimiter = _mali_sys_strdup(MALI_MEM_PRINT_ONE_LINERS_DELIMITER_DEFAULT);
	}
	MALI_CHECK_GOTO(NULL!=dump_context->mem_print_one_liners_delimiter, error_cleanup);
	_mali_sys_config_string_release(readline);

	readline = NULL;

	/* Setting the current pointers to the first in each list */
	dump_context->dump_list_range_frame_pp     = dump_context->dump_list_range_frame_pp_first;
	dump_context->dump_list_range_frame_gp     = dump_context->dump_list_range_frame_gp_first;
	dump_context->dump_list_range_job_pp = dump_context->dump_list_range_job_pp_first;
	dump_context->dump_list_range_job_gp = dump_context->dump_list_range_job_gp_first;

	dump_context->mem_print_one_liners = _mali_sys_config_string_get_bool("MALI_MEM_PRINT_ONE_LINERS", MALI_FALSE);

	dump_context->dump_crashed   = _mali_sys_config_string_get_bool("MALI_DUMP_CRASHED", MALI_FALSE);
	dump_all       = _mali_sys_config_string_get_bool("MALI_DUMP_ALL", MALI_FALSE);
	dump_context->dump_result = _mali_sys_config_string_get_bool("MALI_DUMP_RESULT", MALI_FALSE);

	dump_context->locker_enabled = _mali_sys_config_string_get_bool("MALI_DUMP_SERIALIZE", dump_context->dump_result);

	if (MALI_FALSE != dump_all )
	{
		MALI_CHECK_GOTO(MALI_ERR_NO_ERROR==insert_element(&dump_context->dump_list_range_job_gp, 1, 999999), error_cleanup);
		if ( MALI_FALSE == dump_context->mem_print_one_liners )
		{
			MALI_CHECK_GOTO(MALI_ERR_NO_ERROR==insert_element(&dump_context->dump_list_range_job_pp, 1, 999999), error_cleanup);
		}
	}

	if( \
		  (NULL!=dump_context->dump_list_range_frame_pp) || \
		  (NULL!=dump_context->dump_list_range_frame_gp) || \
		  (NULL!=dump_context->dump_list_range_job_pp)   || \
		  (NULL!=dump_context->dump_list_range_job_gp)      \
	  )
	{
		verbose_printing_set(ctx, MALI_TRUE);
	} else
	{
		verbose_printing_set(ctx, MALI_FALSE);
		if ( MALI_FALSE != dump_context->mem_print_one_liners)
		{
			MALI_CHECK_GOTO(MALI_ERR_NO_ERROR==insert_element(&dump_context->dump_list_range_job_gp, 1, 999999), error_cleanup);
			MALI_DEBUG_PRINT(1,("Not specified which jobs to dump. Turn on all GP-jobs.\n"));
		}
	}

	if ( MALI_FALSE != dump_context->verbose_printing )
	{
		if((NULL!=dump_context->dump_list_range_frame_pp))
		{
			MALI_DEBUG_PRINT(1,("Dumping PP Frames\n"));
		}
		if((NULL!=dump_context->dump_list_range_frame_gp))
		{
			MALI_DEBUG_PRINT(1,("Dumping GP Frames\n"));
		}
		if((NULL!=dump_context->dump_list_range_job_pp))
		{
			MALI_DEBUG_PRINT(1,("Dumping PP Jobs\n"));
		}
		if((NULL!=dump_context->dump_list_range_job_gp))
		{
			MALI_DEBUG_PRINT(1,("Dumping GP Jobs\n"));
		}
		if((MALI_TRUE==dump_all))
		{
			MALI_DEBUG_PRINT(1,("Dumping all Jobs\n"));
		}
		if((MALI_TRUE==dump_context->dump_crashed))
		{
			MALI_DEBUG_PRINT(1,("Dumping Jobs that crash\n"));
		}
		if((MALI_TRUE==dump_context->dump_result))
		{
			MALI_DEBUG_PRINT(1,("Dumping Result of Jobs\n"));
		}
		if((MALI_TRUE==dump_context->locker_enabled))
		{
			MALI_DEBUG_PRINT(1,("Dumping Forcing Job Serialization\n"));
		}

		MALI_DEBUG_CODE(debug_list_print(dump_context));
	}

	if ( \
			(NULL!=dump_context->dump_list_range_frame_pp) || \
			(NULL!=dump_context->dump_list_range_frame_gp) || \
			(NULL!=dump_context->dump_list_range_job_pp)   || \
			(NULL!=dump_context->dump_list_range_job_gp)   || \
			(MALI_TRUE==dump_context->verbose_printing)    || \
			(MALI_TRUE==dump_context->dump_crashed)           \
	   )
	{
		dump_context->dump_system_enabled = MALI_TRUE;
	}

	_mali_sys_atomic_initialize(&dump_context->pp_job_counter, 0);
	_mali_sys_atomic_initialize(&dump_context->gp_job_counter, 0);
	_mali_sys_atomic_initialize(&dump_context->frame_counter, 1);

	MALI_SUCCESS;

error_cleanup:
	if (NULL!= readline) _mali_sys_config_string_release(readline);
	_mali_dump_system_close(ctx);
	MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
}


/** Release all the dumping data structures.
 * The function is not ref-counted. It is only legal to call it once, after the function
 * \a mali_dump_system_open has been called. After current close function has been called
 * not other dumping function is allowed to be called before a \a mali_dump_system_open
 * has been called again.
 * Function is called from \a _mali_base_common_context_destroy()
 */
void _mali_dump_system_close(mali_base_ctx_handle ctx)
{
	mali_dump_context * dump_context;

	/* Stage1) Store the dump_context_pointer, and set the one in the version in the base version to null. */
	dump_context = dump_ctx(ctx);
	_mali_base_common_context_set_dump_context(ctx, (mali_dump_context *)NULL);
	MALI_DEBUG_ASSERT( NULL!=dump_context , ("Not legal to run function when dump system is not open."));

	/* Stage2) Release all the mali_dump_range_elements */
	free_dump_range_list( &dump_context->dump_list_range_frame_pp_first);
	free_dump_range_list( &dump_context->dump_list_range_frame_gp_first );
	free_dump_range_list( &dump_context->dump_list_range_job_pp_first);
	free_dump_range_list( &dump_context->dump_list_range_job_gp_first);

	/* Stage3) Free memory */
	if ( NULL!=dump_context->mem_print_one_liners_delimiter) _mali_sys_free(dump_context->mem_print_one_liners_delimiter );

	/* Stage4) Free the context */
	_mali_sys_free(dump_context);
}

/**
 * Returning MALI_TRUE if the dump system is enabled.
 * It is enabled if the variables may let any job be dumped.
 */
mali_bool _mali_dump_system_is_enabled(mali_base_ctx_handle ctx)
{
	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));
	return dump_ctx(ctx)->dump_system_enabled;
}


/**
 * If this is MALI_TRUE, the dump system should not dump registers or memory content.
 * It should then only dump information about all memory handles on the system.
 * This is for profiling of memory usage.
 */
mali_bool _mali_dump_system_mem_print_one_liners(mali_base_ctx_handle ctx)
{
	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));
	return dump_ctx(ctx)->mem_print_one_liners;
}

/**
 * Returns the delimitor to use beween each field in memory dumps if
 * \a _mali_dump_system_mem_print_one_liners() is enabled.
 */
char * _mali_dump_system_mem_print_one_liners_delimiter(mali_base_ctx_handle ctx)
{
	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));
	return dump_ctx(ctx)->mem_print_one_liners_delimiter;
}


/** Returns whether the dumping system should print status information to console*/
mali_bool _mali_dump_system_verbose_printing_is_enabled(mali_base_ctx_handle ctx)
{
	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));
	return dump_ctx(ctx)->verbose_printing;
}

/** Lets different parts of the system toggle verbose printing.
 * If the user has set the MALI_DUMP_VERBOSE environmentvariable, the input parameter
 * to this function is overridden by this environmentvariable.
*/

/** Increase the frame counter stored in TLS in current thread.
 * This function should only be called by the frame_builder on frame builder swap.
*/
MALI_EXPORT void _mali_base_common_dump_frame_counter_increment(mali_base_ctx_handle ctx)
{
	mali_dump_job_info * in_callback_info;
	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx), ("Not legal to run function when dump system is not open."));
	_mali_sys_atomic_inc(&dump_ctx(ctx)->frame_counter);

	/* Incrementing the frame_number in the job if we are in a callback function */
	in_callback_info = job_info_get(ctx);
	if ( NULL!= in_callback_info)
	{
		in_callback_info->frame_nr++;
	}
}

/** Setting the TLS to not running in callback. Release dump_info->dump_sync */
void _mali_base_common_dump_job_post_callback(mali_base_ctx_handle ctx, mali_sync_handle dump_sync)
{
	if( MALI_FALSE ==_mali_dump_system_is_enabled(ctx))
	{
		return;
	}
	_mali_base_common_dump_tls_job_info_set(ctx, NULL);

	if ( MALI_NO_HANDLE != dump_sync )
	{
		_mali_base_common_sync_handle_release_reference(dump_sync);
	}
}



void _mali_base_common_dump_tls_job_info_set(mali_base_ctx_handle ctx, mali_dump_job_info * dump_info)
{
	mali_err_code err;
	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));
	err = _mali_sys_thread_key_set_data(MALI_THREAD_KEY_DUMP_CALLBACK, (void*)dump_info);
	MALI_IGNORE(err);
}




/**
 * Open the config.txt file used for dumping of registers and commands for loading memory dumps.
 */
mali_file * _mali_base_common_dump_file_open(mali_base_ctx_handle ctx)
{
	char filename[512];
	u32 needed_length;
	const char* dump_path_env = _mali_sys_config_string_get("MALI_OUTPUT_FOLDER");
	mali_file * opened_file;

	MALI_IGNORE(ctx);

	if (NULL != dump_path_env)
	{
		needed_length = _mali_sys_snprintf(filename,
		                                   sizeof(filename),
		                                   "%s/%s",
		                                   dump_path_env,
		                                   MALI_DEFAULT_DUMP_CONFIG_FILE_NAME);
	}
	else
	{
		needed_length = _mali_sys_snprintf(filename,
		                                   sizeof(filename),
		                                   "%s",
		                                   MALI_DEFAULT_DUMP_CONFIG_FILE_NAME);
	}

	_mali_sys_config_string_release(dump_path_env);

	if (needed_length > sizeof(filename))
	{
		MALI_DEBUG_ERROR(("Mali dumping: Could not create filename\n"));
		return NULL;
	}

	opened_file = _mali_sys_fopen(filename, "a");
	if ( NULL == opened_file )
	{
		MALI_DEBUG_ERROR(("Mali dumping: Could not open dump file: %s\n", filename));
	}

	return opened_file;
}

/**
 * Open the opened config.txt file.
 */
void _mali_base_common_dump_file_close(mali_file * file)
{
	_mali_sys_fclose(file);
}

/**
 * Setting dump_info pointer in TLS, so the settings can be acquired when a new job
 * is started in current callback. Dump memory if DUMP_RESULT is set.
 * If DUMP_CRASHED is set, it patch memory and dump the job if it has crashed
 */
void _mali_base_common_dump_pre_callback(mali_base_ctx_handle ctx,
        mali_dump_job_info * dump_info, mali_job_completion_status completion_status,
        void * dump_reg_info)
{
	mali_bool dump_as_crashed;
	u32 subjobs;
	u32 subjob_nr;

	if( MALI_FALSE ==_mali_dump_system_is_enabled(ctx) || MALI_TRUE == dump_info->disable_dump )
	{
		return;
	}

	/* Setting dump_info pointer in TLS, so we can use the settings when we start a new job in callback */
	_mali_base_common_dump_tls_job_info_set(ctx, dump_info);

	/* Setting default values */
	dump_as_crashed   = MALI_FALSE;

	/* We should dump and patch memory if job Crashed and Dump on crash is set */
	if ( MALI_JOB_STATUS_END_SUCCESS != completion_status)
	{
		if ( dump_info->crash_dump_enable )
		{
			dump_as_crashed   = MALI_TRUE;
			verbose_printing_set(ctx, MALI_TRUE);
			if (MALI_FALSE==dump_info->is_pp_job)
			{
			/* This is used when we try to dump the gp_job after it crashed.
			 * To be able to do that we must patch the pointer array so it becomes as it was before job start.
			 * In this process the crashed pointer array is restored before in the unpatching which is done
			 * after the dumping has been performed.
			 * The algorithm used will not work if several GP jobs are used after eachother on the same frame
			 * without resetting the pointer_array before starting the job.
			 * This is because we for speed reasons only take a backup of the first part of the array before
			 * the job starts, and when crash occure tries to recreate the whole buffer based on these values.
			 */
				_mali_common_dump_gp_patch_pointer_array(dump_info);
			}
		}
	}

	/* Perform the actual dumping */
	if ( (MALI_TRUE==dump_as_crashed) || (MALI_TRUE==dump_info->post_run_dump_enable)  )
	{
		mali_err_code err;
		const char * end_comment;
		const char * core_text;
		char * file_name;
		u32 file_name_size;
		mali_file *file_regs=NULL;

		/* Skip writing to register file if we only do memory profiling.*/
		if( MALI_FALSE==_mali_dump_system_mem_print_one_liners(ctx))
		{
			file_regs = _mali_base_common_dump_file_open(ctx);
			if ( NULL== file_regs)
			{
				MALI_DEBUG_ERROR(("Mali Dumping: Could not open the register write file.\n"));
			}
		}

		if (MALI_FALSE == dump_info->is_pp_job)
		{
			core_text = "gp";
			subjobs = 1;
		}
		else
		{
			core_text = "pp";
			subjobs = MAX(1, ((mali_pp_job*)dump_reg_info)->num_cores);
		}

		if ( MALI_JOB_STATUS_END_SUCCESS != completion_status)
		{
			end_comment = "crashed";
		}
		else
		{
			end_comment = "finished";
		}

		file_name = dump_info->last_mem_dump_file_name;
		file_name_size = sizeof(dump_info->last_mem_dump_file_name);

		for (subjob_nr = 0; subjob_nr < subjobs; subjob_nr++)
		{
			if ( (NULL!=file_regs) && (MALI_TRUE==dump_as_crashed) )
			{
				_mali_sys_fprintf(file_regs, "# Dumping Results %s Job nr: %d, subjob %d from Frame nr: %d\n",
				                              core_text, dump_info->job_nr, subjob_nr, dump_info->frame_nr);
				_mali_sys_fprintf(file_regs, "reset\n");
			}

			if ( MALI_TRUE==dump_as_crashed )
			{
				err = _mali_base_common_dump_mem_all(ctx, file_regs, file_name, file_name_size , core_text,
					  dump_info->job_nr, end_comment , dump_info->frame_nr, dump_info->is_pp_job, subjob_nr);
			}
			else
			{
				err = _mali_base_common_dump_mem_all(ctx, NULL /*Not write load_mem*/, file_name, file_name_size , core_text,
					  dump_info->job_nr, end_comment , dump_info->frame_nr, dump_info->is_pp_job, subjob_nr);
			}

			/* If is crashed gp job */
			if ( (MALI_TRUE==dump_as_crashed) && (MALI_FALSE==dump_info->is_pp_job))
			{
				 /* Function restores the memory pathced by function
				 * _mali_common_dump_gp_patch_pointer_array() */
				_mali_common_dump_gp_unpatch_pointer_array(dump_info);
			}

			if ( NULL!=file_regs )
			{
				if ( MALI_ERR_NO_ERROR==err )
				{
					if (MALI_TRUE==dump_as_crashed)
					{
						/* Writes all registers in the job to the dump file,
						and load command to load the corresponding memory dump.*/
						if (MALI_FALSE==dump_info->is_pp_job)
						{
							_mali_common_dump_gp_regs(ctx, file_regs, (u32*)dump_reg_info);
						}
						else
						{
							_mali_common_dump_pp_regs(ctx, file_regs, (mali_pp_job *)dump_reg_info, subjob_nr);
						}
					}

					/* Adding a comment in the config.h about all jobs this result dump belongs to */
					_mali_sys_fprintf(file_regs, "# The result from %s job nr: %d belonging to Frame nr: %d is in file: %s\n",
									  core_text, dump_info->job_nr, dump_info->frame_nr, file_name );
				}
			}
		}
		_mali_base_common_dump_file_close(file_regs);
	}
}

/** Doing memory dump before starting if enabled */
void _mali_base_common_dump_pre_start_dump(mali_base_ctx_handle ctx, mali_dump_job_info * dump_info, void * dump_reg_info)
{
	if ( MALI_TRUE !=  _mali_dump_system_is_enabled(ctx) || MALI_TRUE == dump_info->disable_dump)
	{
		return;
	}

	if ( (MALI_TRUE == dump_info->pre_run_dump_enable) && (MALI_FALSE==dump_info->pre_run_dump_done) )
	{
		const char * core_text;
		char *file_name_mem;
		int file_name_mem_size;
		mali_err_code err;
		mali_file *file_regs=NULL;
		u32 subjobs;
		u32 subjob_nr;

		/* Skip writing to register file if we only do memory profiling.*/
		if( MALI_FALSE==_mali_dump_system_mem_print_one_liners(ctx))
		{
			file_regs = _mali_base_common_dump_file_open(ctx);
			if ( NULL== file_regs)
			{
				MALI_DEBUG_ERROR(("Mali Dumping: Could not open the register write file.\n"));
			}
		}

		file_name_mem      =      &(dump_info->last_mem_dump_file_name[0]);
		file_name_mem_size = sizeof(dump_info->last_mem_dump_file_name);

		if (MALI_FALSE == dump_info->is_pp_job)
		{
			core_text = "gp";
			subjobs = 1;
		}
		else
		{
			core_text = "pp";
			subjobs = MAX(1, ((mali_pp_job*)dump_reg_info)->num_cores);
		}

		for (subjob_nr = 0; subjob_nr < subjobs; subjob_nr++)
		{
			if ( NULL!=file_regs )
			{
				_mali_sys_fprintf(file_regs, "#\n# Dumping Start %s Job nr: %d, subjob %d from Frame nr: %d\n",
				                              core_text, dump_info->job_nr, subjob_nr, dump_info->frame_nr);
				_mali_sys_fprintf(file_regs, "reset\n");
			}

			err = _mali_base_common_dump_mem_all(ctx, file_regs, file_name_mem, file_name_mem_size, core_text,
					dump_info->job_nr, "start" , dump_info->frame_nr, dump_info->is_pp_job, subjob_nr);
			if ( NULL!=file_regs )
			{
				if ( MALI_ERR_NO_ERROR == err )
				{
					if ( MALI_FALSE==dump_info->is_pp_job)
					{
						/* Writes all registers in the job to the dump file,
						and load command to load the corresponding memory dump.*/
						_mali_common_dump_gp_regs(ctx, file_regs, (u32*) dump_reg_info);
					} else
					{
						/* Writes all registers in the job to the dump file,
						and load command to load the corresponding memory dump.*/
						_mali_common_dump_pp_regs(ctx, file_regs, (mali_pp_job *)dump_reg_info, subjob_nr);
					}
					dump_info->pre_run_dump_done = MALI_TRUE;
				}
			}
		}
		_mali_base_common_dump_file_close(file_regs);
	}
}


/**
 * Filling the job->dump_info struct
 */
void _mali_base_common_dump_job_info_fill(mali_base_ctx_handle ctx, mali_dump_core_type core, mali_dump_job_info * info)
{
	mali_dump_job_info * in_callback_info;

	if ( MALI_FALSE ==_mali_dump_system_is_enabled(ctx) || MALI_TRUE == info->disable_dump )
	{
		return;
	}

	info->last_mem_dump_file_name[0] = 0;
	info->pre_run_dump_done = MALI_FALSE;
	info->post_run_dump_done = MALI_FALSE;

	if ( MALI_DUMP_GP==core)
	{
		info->is_pp_job = MALI_FALSE;
		info->job_nr   = gp_job_counter_inc_and_return(ctx);
	}
	else
	{
		MALI_DEBUG_ASSERT(MALI_DUMP_PP==core, ("Illegal core type\n"));
		info->is_pp_job = MALI_TRUE;
		info->job_nr   = pp_job_counter_inc_and_return(ctx);
	}

	in_callback_info = job_info_get(ctx);
	info->inline_waiter  = NULL;

	if ( NULL != in_callback_info)
	{
		/* We ARE in another job's callback function. Transfering data from this one */
		info->frame_nr = in_callback_info->frame_nr;
		info->dump_sync = in_callback_info->dump_sync;
		if ( NULL != info->dump_sync ) _mali_base_common_sync_handle_register_reference(info->dump_sync);
	}
	else
	{
		info->frame_nr = frame_counter_get(ctx);
		if ( MALI_FALSE != dump_ctx(ctx)->locker_enabled)
		{
			mali_base_wait_handle  inline_waiter;

			info->dump_sync = _mali_base_common_sync_handle_new(ctx);
			if ( MALI_NO_HANDLE!=info->dump_sync  )
			{
				inline_waiter  = _mali_base_common_sync_handle_get_wait_handle(info->dump_sync);
				if ( MALI_NO_HANDLE != inline_waiter  )
				{
					info->inline_waiter = inline_waiter;
					_mali_base_common_sync_handle_register_reference(info->dump_sync);
					_mali_base_common_sync_handle_flush(info->dump_sync);
				}
				else
				{
					_mali_base_common_sync_handle_flush(info->dump_sync);
					info->dump_sync = MALI_NO_HANDLE;
				}
			}

			if(MALI_NO_HANDLE==info->dump_sync ) MALI_DEBUG_ERROR(("Could not allocate dump_sync.\n"));
		}
	}

	if ( MALI_DUMP_GP==core)
	{
		info->pre_run_dump_enable   = dump_gp_job_is_enabled(ctx, info->frame_nr , info->job_nr);
	}
	else
	{
		MALI_DEBUG_ASSERT(MALI_DUMP_PP==core, ("Illegal core type\n"));
		info->pre_run_dump_enable   = dump_pp_job_is_enabled(ctx, info->frame_nr , info->job_nr);
	}

	info->post_run_dump_enable = MALI_FALSE;
	if ( MALI_TRUE == info->pre_run_dump_enable )
	{
		if ( MALI_FALSE!=dump_ctx(ctx)->dump_result )
		{
			info->post_run_dump_enable = MALI_TRUE;
		}
		info->crash_dump_enable  = MALI_FALSE;
	}
	else
	{
		info->crash_dump_enable  = dump_crashed_is_enabled(ctx);
	}

	if (_mali_dump_system_verbose_printing_is_enabled(ctx) )
	{
		const char * core_text;
		core_text = (MALI_FALSE!=info->is_pp_job) ? "pp" : "gp";
		_mali_sys_printf("Mali START Frame number:%4d %s job:%4d\n", info->frame_nr, core_text, info->job_nr);
	}
}



MALI_INLINE mali_dump_context * dump_ctx(mali_base_ctx_handle ctx)
{
	mali_dump_context * retval;
	retval = _mali_base_common_context_get_dump_context(ctx);
	return retval;
}

MALI_INLINE mali_dump_job_info * job_info_get(mali_base_ctx_handle ctx)
{
	mali_dump_job_info * retval;
	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));
	retval =(mali_dump_job_info *) _mali_sys_thread_key_get_data(MALI_THREAD_KEY_DUMP_CALLBACK);
	return retval;
}

/** Return \a MALI_TRUE if the dump system should dump gp job \a job_nr_gp in frame \a frame_nr */
MALI_STATIC mali_bool dump_gp_job_is_enabled(mali_base_ctx_handle ctx, u32 frame_nr, u32 job_nr_gp)
{
	mali_bool retval;

	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));
	retval = check_number_is_in_dump_range(ctx, frame_nr , &dump_ctx(ctx)->dump_list_range_frame_gp);
	MALI_DEBUG_PRINT(3, ("Dumpsystem: Check GP is enabled, FrameNumber: %d. Return: %d\n", frame_nr , retval) );
	if ( MALI_FALSE == retval )
	{
		retval = check_number_is_in_dump_range(ctx, job_nr_gp , &dump_ctx(ctx)->dump_list_range_job_gp);
		MALI_DEBUG_PRINT(3, ("Dumpsystem: Check GP is enabled, JobGPNumber: %d. Return: %d\n", job_nr_gp , retval) );
	}
	return retval;
}

/** Return \a MALI_TRUE if the dump system should dump pp job \a job_nr_pp in frame \a frame_nr */
MALI_STATIC mali_bool dump_pp_job_is_enabled(mali_base_ctx_handle ctx, u32 frame_nr, u32 job_nr_pp)
{
	mali_bool retval;

	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));
	retval = check_number_is_in_dump_range(ctx, frame_nr, &dump_ctx(ctx)->dump_list_range_frame_pp);
	MALI_DEBUG_PRINT(3, ("Dumpsystem: Check PP is enabled, FrameNumber: %d. Return: %d\n", frame_nr , retval) );
	if ( MALI_FALSE == retval )
	{
		retval = check_number_is_in_dump_range(ctx, job_nr_pp , &dump_ctx(ctx)->dump_list_range_job_pp);
		MALI_DEBUG_PRINT(3, ("Dumpsystem: Check PP is enabled, JobPPNumber: %d. Return: %d\n", job_nr_pp , retval) );
	}
	return retval;
}

/** Returns MALI_TRUE if the system should dump crashed jobs.
 * If it should not, it returns MALI_FALSE */
MALI_STATIC mali_bool dump_crashed_is_enabled(mali_base_ctx_handle ctx)
{
	MALI_IGNORE(ctx);
	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));
	return dump_ctx(ctx)->dump_crashed;
}



MALI_DEBUG_CODE(
				MALI_STATIC void debug_list_print(mali_dump_context *dump_context)
{
	mali_dump_range_element * iterator;

	MALI_DEBUG_PRINT(1,("Print all range lists: \n"));

	iterator =dump_context->dump_list_range_frame_pp;
	if(NULL!=iterator)
	{
		MALI_DEBUG_PRINT(1,("Dumping List of PP Frames:\n"));
		while(NULL!=iterator)
		{
			if ( iterator->first==iterator->last) MALI_DEBUG_PRINT(1,("%d, ", iterator->first));
			else MALI_DEBUG_PRINT(1,("%d-%d, ", iterator->first, iterator->last));
			iterator = iterator->next;
		}
		MALI_DEBUG_PRINT(1,("\n"));

	}
	iterator = dump_context->dump_list_range_frame_gp;
	if(NULL!=iterator)
	{
		MALI_DEBUG_PRINT(1,("Dumping List of GP Frames:\n"));
		while(NULL!=iterator)
		{
			if ( iterator->first==iterator->last) MALI_DEBUG_PRINT(1,("%d, ", iterator->first));
			else MALI_DEBUG_PRINT(1,("%d-%d, ", iterator->first, iterator->last));
			iterator = iterator->next;
		}
		MALI_DEBUG_PRINT(1,("\n"));
	}

	iterator = dump_context->dump_list_range_job_pp;
	if(NULL!=iterator)
	{
		MALI_DEBUG_PRINT(1,("Dumping List of PP jobs:\n"));
		while(NULL!=iterator)
		{
			if ( iterator->first==iterator->last) MALI_DEBUG_PRINT(1,("%d, ", iterator->first));
			else MALI_DEBUG_PRINT(1,("%d-%d, ", iterator->first, iterator->last));
			iterator = iterator->next;
		}
		MALI_DEBUG_PRINT(1,("\n"));
	}

	iterator = dump_context->dump_list_range_job_gp;
	if(NULL!=iterator)
	{
		MALI_DEBUG_PRINT(1,("Dumping List of GP jobs: \n"));
		while(NULL!=iterator)
		{
			if ( iterator->first==iterator->last) MALI_DEBUG_PRINT(1,("%d, ", iterator->first));
			else MALI_DEBUG_PRINT(1,("%d-%d, ", iterator->first, iterator->last));
			iterator = iterator->next;
		}
		MALI_DEBUG_PRINT(1,("\n"));
	}

} /* Function: void debug_list_print(mali_dump_context *dump_context) */
			   ) /*  MALI_DEBUG_CODE( */


/**
 * Getting the pp job counter value, and increase the counter.
 * \param ctx The base context
 */
					   MALI_STATIC u32 pp_job_counter_inc_and_return(mali_base_ctx_handle ctx)
{
	u32 retval;
	retval = _mali_sys_atomic_inc_and_return(&dump_ctx(ctx)->pp_job_counter);
	return retval;
}

/**
 * Getting the gp job counter value, and increase the counter.
 * \param ctx The base context
 */
MALI_STATIC u32 gp_job_counter_inc_and_return(mali_base_ctx_handle ctx)
{
	u32 retval;
	retval = _mali_sys_atomic_inc_and_return(&dump_ctx(ctx)->gp_job_counter);
	return retval;
}

MALI_STATIC void verbose_printing_set(mali_base_ctx_handle ctx, mali_bool verbose)
{
	mali_bool new_value;
	MALI_DEBUG_ASSERT( NULL!=dump_ctx(ctx) , ("Not legal to run function when dump system is not open."));

	new_value = _mali_sys_config_string_get_bool("MALI_DUMP_VERBOSE", verbose);
	if ( (MALI_TRUE==new_value) && (MALI_FALSE==dump_ctx(ctx)->verbose_printing) )
	{
		MALI_DEBUG_PRINT(1,("Mali Dump system verbose printing is enabled.\n"));
	}
	dump_ctx(ctx)->verbose_printing = new_value;
}

/** Return the current frame counter, which is stored on a TLS in current thread.*/
MALI_STATIC u32 frame_counter_get(mali_base_ctx_handle ctx)
{
	mali_dump_job_info * in_callback_info;
	mali_dump_context * dump_context;
	u32 retval;
	dump_context = dump_ctx(ctx);
	MALI_DEBUG_ASSERT( NULL!=dump_context , ("Not legal to run function when dump system is not open."));

	in_callback_info = job_info_get(ctx);

	if ( NULL!= in_callback_info)
	{
		retval = in_callback_info->frame_nr;
	}
	else
	{
		retval = _mali_sys_atomic_get(&dump_context->frame_counter);
	}
	return retval;
}


/** Checks if the current number is in the sorted linked list of number ranges.
 * If it is it returns true, and sets the input pointer to the element with the number in it.
 * If not it returns false.
 * Since the function stores back the current pointer, the traversion of the list is
 * very fast for applications with only one frame counter.
 */
MALI_STATIC mali_bool check_number_is_in_dump_range(mali_base_ctx_handle ctx, u32 number, mali_dump_range_element ** dump_list_element)
{
	mali_bool retval;
	mali_dump_range_element * list_element;
	MALI_DEBUG_CODE(int i=0; int j=0;);

	MALI_IGNORE(ctx);

	MALI_DEBUG_ASSERT( NULL!=dump_list_element, ("Internal error"));

	list_element = *dump_list_element;

	if ( NULL == list_element ) return MALI_FALSE;

	while ( (list_element->first > number)  && (NULL!=list_element->prev) )
	{
		MALI_DEBUG_CODE(i++);
		list_element = list_element->prev;
	}
	while ( (list_element->last  < number)  && (NULL!=list_element->next) )
	{
		MALI_DEBUG_CODE(j++);
		list_element = list_element->next;
	}

	/* Store back current pointer */
	*dump_list_element = list_element;

	retval = ((list_element->first <= number) && (list_element->last >= number));

	MALI_DEBUG_PRINT(3,("Mali Dump: check_dump_is_enabled count-fw:%d count-bw:%d. Ret: %d\n",i,j, retval));

	return retval;
}

/** Function will generate a list of mali_dump_range_element elements.
 * The list contains single elements and ranges of numbers in a sorted
 * increasing double linked list. These lists are used to find out if
 * the current job_number, or frame_number exist in any of the elements
 * in the list. If it does the job should be dumped.*/
MALI_STATIC mali_err_code generate_dump_list(mali_dump_range_element ** dump_list, const char * txt)
{
	mali_err_code status;
	char * text_parse;

	MALI_DEBUG_PRINT(4, ("generate_dump_list text: %s \n", txt));

	status = MALI_ERR_NO_ERROR;
	text_parse = (char*) txt;

	if ( (NULL==txt) || (0==txt[0])) return status;

	while ( MALI_ERR_NO_ERROR == status )
	{
		u32 first,last;
		/* Traversing to first number, exiting on end*/
		while ( (*text_parse < '0' ) || (*text_parse > '9' ) )
		{
			if ( 0 == *text_parse ) return status;
			text_parse++;
		}
		/* Parsing the first part */
		first = last = (u32) _mali_sys_strtoull(text_parse, &text_parse, 10);
		while(' '==*text_parse)text_parse++;
		/* Parsing the last part */
		if ('-'==*text_parse)
		{
			text_parse++;
			while(' '==*text_parse)text_parse++;
			if ( (*text_parse >= '0' ) && (*text_parse <= '9' ) )
			{
				last = (u32) _mali_sys_strtoull(text_parse, &text_parse, 10);
			}
		}

		/* Adding it to the list */
		status = insert_element(dump_list, MIN(first,last), MAX(first,last));
	}
	return status;
}

MALI_STATIC mali_dump_range_element * get_new_list_entry(u32 first, u32 last)
{
	mali_dump_range_element * new_entry;

	new_entry = (mali_dump_range_element*) _mali_sys_malloc(sizeof(mali_dump_range_element));
	/* We do not do extensive error handling of this debug/profiling code: just abort on OOM */
	if ( NULL == new_entry )
	{
		MALI_DEBUG_ERROR(("Out of memory when building dump lists.\n"));
		return NULL;
	}
	new_entry->first = MIN(first, last);
	new_entry->last  = MAX(first, last);
	new_entry->next  = NULL;
	new_entry->prev  = NULL;
	return new_entry;
}


/** Function that inserts a range or a single element to a \a mali_dump_range_element.
 * The new element is inserted at the correct place in the list, so that it remains sorted.*/
MALI_STATIC mali_err_code insert_element(mali_dump_range_element ** dump_list, u32 first, u32 last)
{
	mali_dump_range_element * new_entry;
	mali_dump_range_element * previous_entry;
	mali_dump_range_element * next_entry;
	mali_dump_range_element * first_entry;

	MALI_DEBUG_ASSERT( NULL!=dump_list, ("Internal error"));

	MALI_DEBUG_PRINT(4, ("insert_element: %d-%d \n", first,last));

	/* Special handling if the list was empty */
	if ( NULL == *dump_list)
	{
		*dump_list = get_new_list_entry(first,last);
		if(NULL==*dump_list) MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
		MALI_DEBUG_PRINT(4, ("insert_element first: 0x%X \n", *dump_list));
		MALI_SUCCESS;
	}

	/* Special handling if the element should be placed first */
	first_entry = *dump_list;
	if ( first_entry->first >= first)
	{
		/* Then the new start point must be before the first_entry */
		if (last < (first_entry->first-1))
		{
			/* Then the new end point must be before the first_entry - we insert a new element at beginning*/
			new_entry = get_new_list_entry(first,last);
			if ( NULL==new_entry) MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
			new_entry->next   = first_entry;
			first_entry->prev = new_entry;
			*dump_list = new_entry;
			MALI_DEBUG_PRINT(4, ("insert_element before first: 0x%X \n", *dump_list));
		}
		else
		{
			/* Then the new startpoint is before the first element, and the endpoint is within or after this element. */
			/* Updating startpoint of the first element */
			first_entry->first = first;
			/* Updating endpoint of the first element */
			first_entry->last = MAX( first_entry->last,last );
			/* Since the new endpoint may be inside next entry we must merge if possible */
			merge_element_if_possible(first_entry);
			MALI_DEBUG_PRINT(4, ("insert_element merged with first: 0x%X \n", *dump_list));
		}

		MALI_SUCCESS;
	}

	/* We then start to iterate through the list and insert the new element at the correct spot.
	Working with previous_entry and next_entry, checking if the new element should be put inside
	here. If the new element starts after next_entry we iterate so that
	previous_entry=next_entry and next_entry= previous_entry->next */

	previous_entry = first_entry;
	while (MALI_TRUE)
	{
		next_entry = previous_entry->next;

		if ( first <= (previous_entry->last+1))
		{
			/* The new start point element is already within the *previous_entry. */
			MALI_DEBUG_ASSERT(first > previous_entry->first, ("Internal error - should not be able to iterate to here"));

			if ( last > previous_entry->last)
			{
				/* The new element can be inserted by extending the *previous_entry end_point */
				previous_entry->last = last;
				merge_element_if_possible(previous_entry);
				MALI_DEBUG_PRINT(4, ("insert_element merged with previous: 0x%X \n", previous_entry));
			}
			else
			{
				/* The new element is already within the *previous_entry. No changes needed */
				MALI_DEBUG_PRINT(4, ("insert_element within previous: 0x%X \n", previous_entry));
			}
			MALI_SUCCESS;
		}
		/* When the code enters here, the new element must be after previous_entry */
		if ( NULL == next_entry )
		{
			/* The previous element was the last in list. Inserting new at end. */
			new_entry = get_new_list_entry(first,last);
			if (NULL==new_entry) MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
			previous_entry->next = new_entry;
			new_entry->prev = previous_entry;
			new_entry->next = NULL;
			MALI_DEBUG_PRINT(4, ("insert_element at end: 0x%X \n", new_entry));
			MALI_SUCCESS;
		}

		/* The next entry exist. Checking if the entire part of the new entry should be before next_entry.*/
		if ( (next_entry->first-1) > last)
		{
			/* Now we know that the new_entry must be after after previous_entry but before next_entry */
			/* Inserting this new element */
			new_entry = get_new_list_entry(first,last);
			if (NULL==new_entry) MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
			previous_entry->next = new_entry;
			next_entry->prev = new_entry;
			new_entry->prev = previous_entry;
			new_entry->next = next_entry;
			MALI_DEBUG_PRINT(4, ("insert_element between prev and next: 0x%X \n", new_entry));
			MALI_SUCCESS;
		}
		/* Now we know that parts of the new_entry must either be inside of next_entry, or all after. */
		if ( first < (next_entry->last+1) )
		{
			/* Now we now that at least part of the new_entry must be inside the next_entry- Updating*/
			next_entry->first = MIN( next_entry->first ,first);
			next_entry->last = MAX( next_entry->last,last);
			merge_element_if_possible(next_entry);
			MALI_DEBUG_PRINT(4, ("insert_element expands the next_entry: 0x%X \n", next_entry));
			MALI_SUCCESS;
		}
		/* Now we know that the new_entry must be completely after the next_entry. Do next iteration.*/
		previous_entry = next_entry;
	}
}

/* Recursively merge of current element with next element if they overlap */
MALI_STATIC  void merge_element_if_possible(mali_dump_range_element * element)
{
	while(MALI_TRUE)
	{
		mali_dump_range_element * next;
		next = element->next;

		if ( NULL==next) return;
		if ( next->first > (element->last+1) ) return ;

		/* This point in code we now that the (last number +1) in current element is
		equal or bigger than the first number in the next. They can be merged. */
		MALI_DEBUG_PRINT(4,("Mali Dump: Merging element %d-%d with %d-%d\n", element->first, element->last, next->first, next->last));
		element->last = MAX(element->last, next->last);
		element->next = next->next;
		if ( NULL!=next->next) next->next->prev = element;
		_mali_sys_free(next);
	}
}


/** Traverse through all the elements in the list, freeing them one at a time also updating the initial input pointer. */
MALI_STATIC void free_dump_range_list(mali_dump_range_element ** dump_list)
{
	MALI_DEBUG_ASSERT( ((*dump_list==NULL)  ||  (NULL==(*dump_list)->prev)), ("Current element is not NULL or first in list."));

	while ( *dump_list )
	{
		mali_dump_range_element * list_item_not_longer_in_use;
		list_item_not_longer_in_use = *dump_list;
		*dump_list = (*dump_list)->next;
		/* Does not need to update the previous pointer since they will all be released anyway */
		MALI_DEBUG_PRINT(4,("Mali Dump: Free element: %d-%d.\n", list_item_not_longer_in_use->first, list_item_not_longer_in_use->last ));
		_mali_sys_free(list_item_not_longer_in_use);
	}
	return;
}

mali_bool _mali_base_common_dump_job_enable_dump(mali_dump_job_info * info, mali_bool enable)
{
	mali_bool ret = (info->disable_dump == MALI_TRUE) ? MALI_FALSE : MALI_TRUE;
	info->disable_dump = (enable == MALI_TRUE) ? MALI_FALSE : MALI_TRUE;
	return ret;
}
