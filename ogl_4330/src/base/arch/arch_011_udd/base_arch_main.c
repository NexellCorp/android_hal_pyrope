/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/* base backends can access OS and library functions directly */


#include <mali_system.h>
#include <stdlib.h>

#include "mali_osu.h"
#include "mali_uku.h"
#include "base_arch_main.h"

/* gp interface */
#include <base/common/gp/base_common_gp_job.h>
#include <base/arch/base_arch_gp.h>

#include "base_arch_timer.h"
#ifdef __SYMBIAN32__
#include <symbian_base.h>
#endif

#ifndef MALI_BASE_WORK_THREAD_NUMBER
#define MALI_BASE_WORK_THREAD_NUMBER 1
#endif

#ifndef MALI_WORKER_THREAD_AFFINITY
#define MALI_WORKER_THREAD_AFFINITY 0
#endif

#if MALI_WORKER_THREAD_AFFINITY
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <unistd.h>
#include <sys/syscall.h>
#undef _GNU_SOURCE
#undef __USE_GNU
#endif


#if MALI_TIMELINE_PROFILING_ENABLED
#include "base/mali_profiling.h"
#endif

/**
 * The user-kernel context shared among the common base_arch_ modules
 */
void *mali_uk_ctx;


/**
 * Arch initialization routine
 * Opens a file descriptor to the device driver
 * Initializes global variables
 * @return Standard mali_err_code
 */
MALI_STATIC mali_err_code arch_initialize(mali_base_ctx_handle ctx) MALI_CHECK_RESULT;

/**
 * Arch termination routine
 * Cleans up and closes device driver file descriptor
 */
MALI_STATIC void arch_terminate(mali_base_ctx_handle ctx);

/**
 * Start the notification monitor thread
 * A thread is run while the arch is initialized
 * which monitors the device driver and handles its notifications.
 * The thread uses the blocking WAIT_FOR_NOTIFICATION ioctl with a timeout
 * @return Standard Mali error code
 */
MALI_STATIC mali_err_code arch_worker_thread_start(void) MALI_CHECK_RESULT;

/**
 * Stop the notification monitor thread
 * Stops the thread created by @see arch_worker_thread_start.
 * Will notify the thread to exit and sleeps until the thread
 * has exited.
 */
MALI_STATIC void arch_worker_thread_stop(void);

/**
 * The notification monitor
 * This function is run in a separate thread created by @see arch_worker_thread_start
 * @param unused Not used, required by pthread
 * @return NULL, required by pthread
 */
MALI_STATIC u32 arch_worker_thread(void * unused);


/**
 * Verify device file
 * This function is run after the open call is done on the device file.
 * It checks that we have a valid handle, that the handle represents a character device file
 * and that one of our ioctl is supported and gives meaninful value.
 * @return MALI_ERR_NO_ERROR If all testing was OK, MALI_ERR_FUNCTION_FAILED if not
 */
MALI_STATIC mali_err_code arch_verify_device_file(void) MALI_CHECK_RESULT;

/**
 * The object representing the worker thread
 */
MALI_STATIC _mali_osu_thread_t *worker_thread[MALI_BASE_WORK_THREAD_NUMBER];

/** Load the settings DB with data from the kernel driver.
 */
_mali_osk_errcode_t arch_init_settings(void);

/**
 * Information about the system the driver has been loaded on
 */
 _vr_mem_info  mem_info_second =
	{
		1073741824,
		_VR_GP_L2_ALLOC | _VR_CPU_WRITEABLE | _VR_CPU_READABLE | _VR_PP_READABLE | _VR_PP_WRITEABLE |_VR_GP_READABLE | _VR_GP_WRITEABLE | _VR_MMU_READABLE | _VR_MMU_WRITEABLE,
		30,
		(int)VR_CACHE_GP_READ_ALLOCATE,
		NULL
	};
 _vr_mem_info  mem_info =
	{	1073741824,
		_VR_CPU_WRITEABLE | _VR_CPU_READABLE | _VR_PP_READABLE | _VR_PP_WRITEABLE |_VR_GP_READABLE | _VR_GP_WRITEABLE | _VR_MMU_READABLE | _VR_MMU_WRITEABLE,
		30,
		(int)VR_CACHE_STANDARD,
		&mem_info_second
	};

MALI_STATIC u32 settings_db[MALI_SETTING_MAX];

MALI_STATIC mali_err_code arch_initialize(mali_base_ctx_handle ctx)
{
	MALI_IGNORE(ctx);

	MALI_CHECK(_MALI_OSK_ERR_OK == _mali_uku_open(&mali_uk_ctx), MALI_ERR_FUNCTION_FAILED);

	if (MALI_ERR_NO_ERROR != arch_verify_device_file())
	{
		MALI_CHECK(_MALI_OSK_ERR_OK == _mali_uku_close(&mali_uk_ctx), MALI_ERR_FUNCTION_FAILED);
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}

	/* Initialise settings DB */
	MALI_CHECK_NO_ERROR(arch_init_settings());

#if MALI_TIMELINE_PROFILING_ENABLED
	if (MALI_TRUE == _mali_base_profiling_get_enable_state())
	{
		/* The Mali device driver told us to enable profiling from beginning, so lets do that */
		_mali_instrumented_plugin_feature_enable(CINSTR_CLIENTAPI_ALL, CINSTR_FEATURE_TIMELINE_PROFILING);
	}
#endif

	MALI_CHECK_NO_ERROR(arch_worker_thread_start());

	arch_init_timer();

	MALI_SUCCESS;
}

MALI_STATIC void arch_terminate(mali_base_ctx_handle ctx)
{
	MALI_IGNORE(ctx);

	arch_worker_thread_stop();

	arch_cleanup_timer();

	if (_MALI_OSK_ERR_OK != _mali_uku_close(&mali_uk_ctx))
	{
		MALI_DEBUG_ERROR(("_mali_uku_close failed"));
	}
}

MALI_STATIC mali_err_code arch_verify_device_file(void)
{
	_vr_uk_get_api_version_s api_version_query = {NULL, 0, 0};

	/* Check the API version */
	api_version_query.ctx = mali_uk_ctx;
	api_version_query.version = _VR_UK_API_VERSION;
	if (_MALI_OSK_ERR_OK != _mali_uku_get_api_version( &api_version_query ))
	{
		_mali_sys_printf("ERROR in Mali driver:\n * Mali device driver failed the API version check\n * The solution is probably to rebuild the libraries and the Mali device driver.\n");
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}
	else
	{
		/* the ioctl was accepted, check if we're compatible */
		if (api_version_query.compatible)
		{
			MALI_SUCCESS;
		}
		else
		{
			/* version mismatch */
			/* check if this is an old/too new version or just garbage data */
			if (_IS_VERSION_ID(api_version_query.version))
			{
					_mali_sys_printf("ERROR in Mali driver:\n * Device driver API mismatch\n * Device driver API version: %d\n * User space API version: %d\n", _GET_VERSION(api_version_query.version), _GET_VERSION(_VR_UK_API_VERSION));
					MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
			else
			{
				_mali_sys_printf("ERROR in Mali driver:\n * Mali device driver does not seem to be valid\n");
				MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
			}
		}
	}
}

mali_err_code _mali_base_arch_open(void)
{
	return arch_initialize(NULL) ;
}

void _mali_base_arch_close(void)
{
	arch_terminate( NULL );
}

_vr_mem_info * _mali_base_arch_get_mem_info(void)
{
	return &mem_info;
}

MALI_STATIC mali_err_code arch_worker_thread_start(void)
{
	unsigned int i = MALI_BASE_WORK_THREAD_NUMBER;
	while(i--)
	{
		MALI_CHECK(_MALI_OSU_ERR_OK == _mali_osu_create_thread(&worker_thread[i], (_mali_osu_threadproc_t *)arch_worker_thread, (void*)i), MALI_ERR_FUNCTION_FAILED);
		MALI_DEBUG_PRINT(3, ("Created thread: %p Successfully\n", worker_thread[i]));
	}

	MALI_SUCCESS;
}

MALI_STATIC void arch_worker_thread_stop(void)
{
	_vr_uk_post_notification_s post_args = {NULL, 0};
	unsigned int i = MALI_BASE_WORK_THREAD_NUMBER;
	mali_err_code err;

	/* request the thread to exit */
	post_args.ctx = mali_uk_ctx;
	post_args.type = _VR_NOTIFICATION_APPLICATION_QUIT;

	err = MALI_ERR_NO_ERROR;
	while(i-- && (MALI_ERR_NO_ERROR ==err))
	{
		err = (mali_err_code)_mali_uku_post_notification(&post_args);
	}
	if ( MALI_ERR_NO_ERROR == err )
	{
		i = MALI_BASE_WORK_THREAD_NUMBER;
		while(i-- && MALI_ERR_NO_ERROR==err)
	{
		/* wait until thread has exited */
			MALI_DEBUG_PRINT(3, ("Waiting for thread: %p \n", worker_thread[i]));
			if (_MALI_OSU_ERR_OK != _mali_osu_wait_for_thread(worker_thread[i], NULL))
		{
				MALI_DEBUG_PRINT(0, ("_mali_osu_wait_for_thread failed to wait for thread exit\n"));
		}
	}
	}
	else
	{
		MALI_DEBUG_PRINT(0, ("_mali_uku_post_notification failed to post quit message\n"));
	}
}

#if MALI_WORKER_THREAD_AFFINITY
#define MALI_MAX_CPU_CORES 4
MALI_STATIC void set_affinity( int callback_thread_id )
{
	int ret;
	pid_t tid = syscall( __NR_gettid );

	/* First callback thread is always locked to core0 which we know is always started.
	 * Second callback thread can run on any core!=0 so we iterate until we find one. */
	if ( callback_thread_id == 0 )
	{
		int cpu_mask = 1 << (int)callback_thread_id;
		ret = syscall( __NR_sched_setaffinity, tid, sizeof(cpu_mask), &cpu_mask);
	}
	else
	{
		int cpu_affinity;
		for ( cpu_affinity = 1; cpu_affinity < MALI_MAX_CPU_CORES; cpu_affinity++ )
		{
			int cpu_mask = 1 << (int)cpu_affinity;
			ret = syscall( __NR_sched_setaffinity, tid, sizeof(cpu_mask), &cpu_mask);
			if ( ret == 0 ) break;
		}
	}

	MALI_DEBUG_CODE(
		if ( ret ) MALI_DEBUG_PRINT( 5, ( "Warning: Could not set thread%d(%d)'s affinity. Continuing..\n", callback_thread_id, tid ) );
	);
}
#undef MALI_MAX_CPU_CORES
#endif /*MALI_WORKER_THREAD_AFFINITY*/

MALI_STATIC u32 arch_worker_thread(void * callback_thread_id )
{
	/* notification declared here to take initialization out of the loop.
	   We need it only once to make Valgrind not report false positives due to
	   lacking understanding of the IOCTL call return values */
	_vr_uk_wait_for_notification_s notification = { 0, 0, { { 0, 0, 0 } } };
	_mali_osk_errcode_t err;

	MALI_IGNORE(callback_thread_id);

	MALI_DEBUG_PRINT(3, ("Arch worker thread started\n"));

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_START|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_START_STOP_SW_CALLBACK_THREAD, 0, 0, 0, 0, 0);
#endif

	/* loop while not requested to exit */
	while (1)
	{
		MALI_DEBUG_PRINT(7, ("Calling _mali_uku_wait_for_notification()\n"));
		/* receive a notification from the kernel */
		notification.ctx = mali_uk_ctx;

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SUSPEND|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_NONE, 0, 0, 0, 0, 0);
#endif

		err = _mali_uku_wait_for_notification( &notification );

#if MALI_WORKER_THREAD_AFFINITY
		/* Set affinity during runtime as cores can be dynamically started/stopped */
		set_affinity((int)callback_thread_id);
#endif /*MALI_WORKER_THREAD_AFFINITY*/

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_RESUME|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_NONE, 0, 0, 0, 0, 0);
#endif

		if (_MALI_OSK_ERR_OK != err)
		{
			/* ioctl retured an error, major fault */
			MALI_DEBUG_PRINT(1, ("Error %d received from _mali_uku_wait_for_notification(), should never happen, exiting worker thread\n", err));

			return 0;
		}

		/* check the lower 4 bits to determine the destination */
		switch ( (notification.type & _VR_NOTIFICATION_SUBSYSTEM_MASK) >> _VR_NOTIFICATION_SUBSYSTEM_SHIFT)
		{
			case _VR_UK_PP_SUBSYSTEM:
				_mali_base_arch_pp_event_handler((u32)notification.type, MALI_REINTERPRET_CAST(void*)&notification.data);
				break;
			case _VR_UK_GP_SUBSYSTEM:
				_mali_base_arch_gp_event_handler((u32)notification.type, MALI_REINTERPRET_CAST(void*)&notification.data);
				break;
			case _VR_UK_CORE_SUBSYSTEM:
				/* core notification */
				if (_VR_NOTIFICATION_CORE_SHUTDOWN_IN_PROGRESS == notification.type)
				{
					MALI_DEBUG_PRINT(4, ("Arch worker thread asked to stop by the kernel module"));

#if MALI_TIMELINE_PROFILING_ENABLED
					_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP | VR_PROFILING_EVENT_CHANNEL_SOFTWARE
					                               | VR_PROFILING_EVENT_REASON_START_STOP_SW_CALLBACK_THREAD, 0, 0, 0, 0, 0);
#endif
					return 0;
				}
				else if (_VR_NOTIFICATION_APPLICATION_QUIT == notification.type)
				{
					MALI_DEBUG_PRINT(4, ("Arch worker thread asked to stopped by self notification"));

#if MALI_TIMELINE_PROFILING_ENABLED
					_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP | VR_PROFILING_EVENT_CHANNEL_SOFTWARE
					                               | VR_PROFILING_EVENT_REASON_START_STOP_SW_CALLBACK_THREAD, 0, 0, 0, 0, 0);
#endif
					return 0;
				}
				else if (_VR_NOTIFICATION_SETTINGS_CHANGED == notification.type)
				{
					_vr_uk_user_setting_t setting;
					MALI_DEBUG_PRINT(4, ("Arch worker thread notified that driver settings have changed.\n"));

					setting = notification.data.setting_changed.setting;

					MALI_DEBUG_ASSERT((_VR_UK_USER_SETTING_MAX > setting), ("Setting out of range.\n"));

					settings_db[setting] = notification.data.setting_changed.value;
				}
				else
				{
					MALI_DEBUG_PRINT(1, ("Unknown core notification recevied: %d\n", notification.type));
					/* we just ignore it */
				}
				break;
			default:
				MALI_DEBUG_PRINT(1, ("Unknown notification type received: 0x%02X. Ignoring\n", notification.type));
				break;
		}
	}
}

#if !defined(USING_MALI200)
mali_bool arch_l2_counters_needs_reset()
{
	/* We need to force a reset of the L2 counters the first time we start a job (GP or PP) which has L2 counters enabled */
	static mali_bool l2_needs_reset = MALI_TRUE;
	mali_bool ret = l2_needs_reset;
	l2_needs_reset = MALI_FALSE;
	return ret;
}
#endif

u32 arch_get_setting(_mali_setting_t setting)
{
	_vr_uk_get_user_setting_s call_arguments = {mali_uk_ctx, setting, 0};
	if(_MALI_OSK_ERR_OK == _mali_uku_get_user_setting(&call_arguments))
	{
		return call_arguments.value;
	}
	else
	{
		return 0;
	}
}

u32 _mali_base_arch_get_setting(_mali_setting_t setting)
{
	MALI_DEBUG_ASSERT((MALI_SETTING_MAX > setting), ("setting is out of range.\n"));
	return settings_db[setting];
}

_mali_osk_errcode_t arch_init_settings(void)
{
	_vr_uk_get_user_settings_s args;
	_mali_osu_errcode_t ret;

	args.ctx = mali_uk_ctx;

	MALI_DEBUG_ASSERT((_VR_UK_USER_SETTING_MAX == (_vr_uk_user_setting_t)MALI_SETTING_MAX),
	                  ("mali_setting_t and _vr_uk_user_setting_t needs to be updated.\n"));

	ret = _mali_uku_get_user_settings(&args);
	if(_MALI_OSU_ERR_OK != ret)
	{
		return ret;
	}

	_mali_osu_memcpy(settings_db, args.settings, sizeof(args.settings));

	return _MALI_OSU_ERR_OK;
}
