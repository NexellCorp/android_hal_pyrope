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
 * @file base_common_context.c
 *
 * Implementation of the base context object creation and destruction.
 * Today the context separation is just an empty placeholder pending
 * design of the actuall separation.
 *
 * See base_common_context.h for documentation of the public functions
 */

#include <base/mali_debug.h>
#include <base/common/base_common_sync_handle.h>
#include <base/common/mem/base_common_mem.h>
#include <base/common/pp/base_common_pp_job.h>
#include <base/common/gp/base_common_gp_job.h>
#include "base_common_context.h"
#include <base/common/dependency_system/base_common_ds_context.h>
#ifdef MALI_DUMP_ENABLE
#include <base/common/dump/base_common_dump_jobs_functions_frontend.h>
#endif /* #ifdef MALI_DUMP_ENABLE */

/**
 * Used to check that a valid/undamanaged context is passed.
 */
#define BASE_CONTEXT_MAGIC 0xAFBA5544

/**
 * The reference count is set to this value on start to detect
 * too many close-calls without going into the negative range too soon
 */
#define BASE_CONTEXT_REF_COUNT_OFFSET 0x1000

/**
 * The context data type.
 */
typedef struct mali_base_context
{
	MALI_DEBUG_CODE(
		u32 magic; /**< Magic checked in add/release ref and destroy */
	)
	u32 number_of_owners;
	mali_sync_handle sync; /**< Sync handle used to track the references to this context */
	mali_base_wait_handle wait_handle; /**< Wait handle used during context destroy to block until all refs are gone */
	struct mali_ds_manager * dependency_system_context; /**< Context for global dependency system. Has a mutex protecting ds lists. */
#ifdef MALI_DUMP_ENABLE
	struct mali_dump_context * dump_context;
#endif
	mali_base_worker_handle cleanup_thread;
} mali_base_context;

MALI_STATIC mali_base_context * mali_global_ctx = NULL;

MALI_STATIC mali_atomic_int frame_id_counter;
MALI_STATIC mali_atomic_int frame_builder_id_counter;

#ifdef MALI_DUMP_ENABLE
struct mali_dump_context * _mali_base_common_context_get_dump_context(mali_base_ctx_handle ctx_handle)
{
	mali_base_context * ctx;
	struct mali_dump_context * retval;
	MALI_DEBUG_ASSERT_HANDLE(ctx_handle);
	ctx = MALI_REINTERPRET_CAST(mali_base_context*)ctx_handle;
	retval = ctx->dump_context ;
	return retval;
}

void _mali_base_common_context_set_dump_context(mali_base_ctx_handle ctx_handle, struct mali_dump_context *dump_context)
{
	mali_base_context * ctx;
	MALI_DEBUG_ASSERT_HANDLE(ctx_handle);
	ctx = MALI_REINTERPRET_CAST(mali_base_context*)ctx_handle;
	ctx->dump_context = dump_context;
}
#endif /* #ifdef MALI_DUMP_ENABLE  */


struct mali_ds_manager * _mali_base_common_context_get_dependency_system_context(mali_base_ctx_handle ctx_handle)
{
	mali_base_context * ctx;
	struct mali_ds_manager * retval;
	MALI_DEBUG_ASSERT_HANDLE(ctx_handle);
	ctx = MALI_REINTERPRET_CAST(mali_base_context*)ctx_handle;
	retval = ctx->dependency_system_context ;
	return retval;
}

void _mali_base_common_context_set_dependency_system_context(mali_base_ctx_handle ctx_handle, struct mali_ds_manager *ds_context)
{
	mali_base_context * ctx;
	MALI_DEBUG_ASSERT_HANDLE(ctx_handle);
	ctx = MALI_REINTERPRET_CAST(mali_base_context*)ctx_handle;
	ctx->dependency_system_context = ds_context;
}

MALI_STATIC mali_err_code _mali_base_common_context_cleanup_thread_start(mali_base_ctx_handle ctx_handle)
{
	mali_base_context * ctx;
	MALI_DEBUG_ASSERT_HANDLE(ctx_handle);
	ctx = MALI_REINTERPRET_CAST(mali_base_context*)ctx_handle;

/* Only create the cleanup thread if we will use it; currently only deferred frame reset uses it */
#if MALI_USE_DEFERRED_FRAME_RESET
	/* Create new worker thread with idle policy for deferred reset. */
	ctx->cleanup_thread = _mali_base_worker_create(MALI_TRUE);
	return (ctx->cleanup_thread == MALI_BASE_WORKER_NO_HANDLE) ? MALI_ERR_FUNCTION_FAILED : MALI_ERR_NO_ERROR;
#else
	ctx->cleanup_thread = MALI_BASE_WORKER_NO_HANDLE;
	return MALI_ERR_NO_ERROR;
#endif /* MALI_USE_DEFERRED_FRAME_RESET */
}

void _mali_base_common_context_cleanup_thread_end(mali_base_ctx_handle ctx_handle)
{
	mali_base_context * ctx;
	MALI_DEBUG_ASSERT_HANDLE(ctx_handle);
	ctx = MALI_REINTERPRET_CAST(mali_base_context*)ctx_handle;

	if (MALI_BASE_WORKER_NO_HANDLE != ctx->cleanup_thread)
	{
		_mali_base_worker_destroy(ctx->cleanup_thread);
		ctx->cleanup_thread = MALI_BASE_WORKER_NO_HANDLE;
	}
}

mali_err_code _mali_base_common_context_cleanup_thread_enqueue(mali_base_ctx_handle ctx_handle, mali_base_worker_task_proc task_proc, void *task_param)
{
	mali_base_context * ctx;
	MALI_DEBUG_ASSERT_HANDLE(ctx_handle);
	ctx = MALI_REINTERPRET_CAST(mali_base_context*)ctx_handle;

#if MALI_USE_DEFERRED_FRAME_RESET
	return _mali_base_worker_task_add(ctx->cleanup_thread, MALI_REINTERPRET_CAST(mali_base_worker_task_proc)task_proc, task_param);
#else
	return MALI_ERR_FUNCTION_FAILED;
#endif /* MALI_USE_DEFERRED_FRAME_RESET */
}

/**
 * Subsystem open helper routine
 * Opens all the compiled in subsystems.
 * If one subsystem open fails all subsystems are closed before returning.
 * @param ctx The base context to open all the subsystems with
 * @return MALI_ERR_NO_ERROR if all subsystems are opened and ready, MALI_ERR_FUNCTION_FAILED if not.
 */
MALI_STATIC mali_err_code _mali_base_common_context_open_subsystems(mali_base_ctx_handle ctx) MALI_CHECK_RESULT;

MALI_STATIC mali_err_code _mali_base_common_context_open_subsystems(mali_base_ctx_handle ctx)
{
	_mali_sys_atomic_initialize(&frame_id_counter, 0);
	_mali_sys_atomic_initialize(&frame_builder_id_counter, 1);

	if (MALI_ERR_NO_ERROR == _mali_base_common_mem_open(ctx))
	{
		if (MALI_ERR_NO_ERROR == _mali_base_common_pp_open(ctx))
		{
			if (MALI_ERR_NO_ERROR == _mali_base_common_gp_open(ctx))
			{
				if (MALI_ERR_NO_ERROR == mali_common_dependency_system_open(ctx))
				{

#ifdef MALI_DUMP_ENABLE
					if (MALI_ERR_NO_ERROR == _mali_dump_system_open(ctx))
					{
#endif /* #ifdef MALI_DUMP_ENABLE */

						if (MALI_ERR_NO_ERROR == _mali_base_common_context_cleanup_thread_start(ctx))
						{
							/* all subsystem opens OK */
							MALI_SUCCESS;
						}

#ifdef MALI_DUMP_ENABLE
						_mali_dump_system_close(ctx);
					}
#endif /* #ifdef MALI_DUMP_ENABLE */
					mali_common_dependency_system_close(ctx);
				}
				_mali_base_common_gp_close(ctx);

			}
			_mali_base_common_pp_close(ctx);
		}
		_mali_base_common_mem_close(ctx);
	}

	MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
}

MALI_TEST_EXPORT
mali_base_ctx_handle _mali_base_common_context_create(void)
{
	mali_mutex_handle base_static_mutex;

	base_static_mutex = _mali_sys_mutex_static( MALI_STATIC_MUTEX_BASE );
	_mali_sys_mutex_lock(base_static_mutex);

	if ( mali_global_ctx )
	{
		mali_global_ctx->number_of_owners++;
		_mali_sys_mutex_unlock(base_static_mutex);
		return MALI_REINTERPRET_CAST(mali_base_ctx_handle)mali_global_ctx;
	}

	_mali_sys_load_config_strings();

	/* create the context, (calloced to 0-init)*/
	mali_global_ctx = MALI_REINTERPRET_CAST(mali_base_context*)_mali_sys_calloc(1, sizeof(mali_base_context));
	if (NULL != mali_global_ctx)
	{
		mali_global_ctx->sync = _mali_base_common_sync_handle_core_new(MALI_REINTERPRET_CAST(mali_base_ctx_handle)mali_global_ctx);
		if (MALI_NO_HANDLE != mali_global_ctx->sync)
		{
			mali_global_ctx->wait_handle = _mali_base_common_sync_handle_get_wait_handle(mali_global_ctx->sync);
			if (MALI_NO_HANDLE != mali_global_ctx->wait_handle)
			{
				/* set initial values */
				MALI_DEBUG_CODE( mali_global_ctx->magic = BASE_CONTEXT_MAGIC );

				if (MALI_ERR_NO_ERROR == _mali_base_common_context_open_subsystems(MALI_REINTERPRET_CAST(mali_base_ctx_handle)mali_global_ctx))
				{
					mali_global_ctx->number_of_owners = 1;
					_mali_sys_mutex_unlock(base_static_mutex);
					return MALI_REINTERPRET_CAST(mali_base_ctx_handle)mali_global_ctx;
				}

				_mali_base_common_sync_handle_flush(mali_global_ctx->sync); /* frees the sync handle */
				_mali_wait_on_handle(mali_global_ctx->wait_handle); /* wait on the handle, which will free it */
			}
			else _mali_base_common_sync_handle_flush(mali_global_ctx->sync); /* frees the sync handle */
		}
		_mali_sys_free(mali_global_ctx);
		mali_global_ctx = NULL;
	}
	_mali_sys_mutex_unlock(base_static_mutex);
	return MALI_NO_HANDLE;
}

MALI_TEST_EXPORT
void _mali_base_common_context_destroy(mali_base_ctx_handle ctx_handle)
{
	mali_mutex_handle base_static_mutex;

	MALI_DEBUG_ASSERT( mali_global_ctx==(mali_base_context*)ctx_handle , \
					   ("Invalid in call to _mali_base_common_context_destroy.") );
	MALI_DEBUG_ASSERT(BASE_CONTEXT_MAGIC == mali_global_ctx->magic, ("Base context damaged."));

	if (NULL == mali_global_ctx) return;
	
	base_static_mutex = _mali_sys_mutex_static( MALI_STATIC_MUTEX_BASE );
	_mali_sys_mutex_lock(base_static_mutex);

	MALI_DEBUG_ASSERT(0!=mali_global_ctx->number_of_owners , ("Illegal number_of_owners."));

	mali_global_ctx->number_of_owners--;

	if ( 0==mali_global_ctx->number_of_owners)
	{
		if (MALI_BASE_WORKER_NO_HANDLE != mali_global_ctx->cleanup_thread)
		{
			/* EGL has not terminated the cleanup thread, do it ourselves. */
			_mali_base_common_context_cleanup_thread_end(ctx_handle);
		}

		_mali_base_common_sync_handle_flush(mali_global_ctx->sync);
		_mali_wait_on_handle(mali_global_ctx->wait_handle);

		#ifdef MALI_DUMP_ENABLE
			_mali_dump_system_close((mali_base_ctx_handle)mali_global_ctx);
		#endif /* #ifdef MALI_DUMP_ENABLE */

		mali_common_dependency_system_close((mali_base_ctx_handle)mali_global_ctx);
		_mali_base_common_gp_close((mali_base_ctx_handle)mali_global_ctx);
		_mali_base_common_pp_close((mali_base_ctx_handle)mali_global_ctx);
		_mali_base_common_mem_close((mali_base_ctx_handle)mali_global_ctx);

		/* invalidate the magic */
		MALI_DEBUG_CODE( mali_global_ctx->magic = 0; )

		/* Release the context object */
		_mali_sys_free(mali_global_ctx);
		mali_global_ctx = NULL;
	}
	_mali_sys_mutex_unlock(base_static_mutex);
}

void _mali_base_common_context_reference_add(mali_base_ctx_handle ctx_handle)
{
	mali_base_context * ctx;
	MALI_DEBUG_ASSERT_HANDLE(ctx_handle);
	ctx = MALI_REINTERPRET_CAST(mali_base_context*)ctx_handle;
	MALI_DEBUG_ASSERT(BASE_CONTEXT_MAGIC == ctx->magic, ("Base context damaged or invalid in call to _mali_base_common_context_reference_add"));
	MALI_DEBUG_ASSERT_HANDLE(ctx->sync);
	_mali_base_common_sync_handle_register_reference(ctx->sync);
}

void _mali_base_common_context_reference_remove(mali_base_ctx_handle ctx_handle)
{
	mali_base_context * ctx;
	MALI_DEBUG_ASSERT_HANDLE(ctx_handle);
	ctx = MALI_REINTERPRET_CAST(mali_base_context*)ctx_handle;
	MALI_DEBUG_ASSERT(BASE_CONTEXT_MAGIC == ctx->magic, ("Base context damaged or invalid in call to _mali_base_common_context_reference_remove"));
	MALI_DEBUG_ASSERT_HANDLE(ctx->sync);
	_mali_base_common_sync_handle_release_reference(ctx->sync);
}

mali_base_frame_id _mali_base_common_frame_id_get_new(mali_base_ctx_handle ctx)
{
	MALI_IGNORE(ctx);
	return MALI_REINTERPRET_CAST(mali_base_frame_id)_mali_sys_atomic_inc_and_return(&frame_id_counter);
}

mali_base_frame_builder_id _mali_base_common_frame_builder_id_get_new(mali_base_ctx_handle ctx)
{
	MALI_IGNORE(ctx);
	return MALI_REINTERPRET_CAST(mali_base_frame_builder_id) _mali_sys_atomic_inc_and_return(&frame_builder_id_counter);
}
