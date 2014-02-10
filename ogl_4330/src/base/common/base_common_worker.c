/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_worker.h>

#include <mali_osu.h>

#if MALI_INSTRUMENTED
#include <mali_instrumented_context.h>
#endif

#if MALI_TIMELINE_PROFILING_ENABLED
#include "base/mali_profiling.h"
#endif

typedef struct mali_base_worker_task
{
	mali_base_worker_task_proc    task_proc;
	void                         *task_param;
	struct mali_base_worker_task *next_task;
#if MALI_INSTRUMENTED
	mali_instrumented_frame      *instrumented_frame;
#endif
} mali_base_worker_task;

typedef struct mali_base_worker
{
	mali_mutex_handle      mutex;
	_mali_osu_thread_t    *worker_thread;
	mali_bool              processing_loop_done;
	mali_base_worker_task *first_task;           /**< Start of singly linked list. */
	mali_base_worker_task *last_task;            /**< For quickly appending tasks to the end of the list. */
	mali_lock_handle       message_lock;
	mali_bool              idle_policy;
} mali_base_worker;

MALI_STATIC_INLINE mali_base_worker *_mali_base_worker_get(mali_base_worker_handle worker_handle)
{
	return MALI_REINTERPRET_CAST(mali_base_worker *)(worker_handle);
}

MALI_STATIC_INLINE void _mali_base_worker_thread_wait_for_message(mali_base_worker_handle worker_handle)
{
	mali_base_worker *worker = _mali_base_worker_get(worker_handle);

	MALI_DEBUG_ASSERT_POINTER(worker);

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SUSPEND | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_NONE, 0, 0, 0, 0, 0);
#endif

	_mali_sys_lock_lock(worker->message_lock);

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_RESUME | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_NONE, 0, 0, 0, 0, 0);
#endif
}

MALI_STATIC void _mali_base_worker_thread_execute_tasks(mali_base_worker_task *msg)
{
	while (NULL != msg)
	{
		mali_base_worker_task *prev_msg;

#if MALI_INSTRUMENTED
		if (NULL != msg->instrumented_frame)
		{
			_instrumented_set_active_frame(msg->instrumented_frame);
		}
#endif

		msg->task_proc(msg->task_param);

#if MALI_INSTRUMENTED
		if (NULL != msg->instrumented_frame)
		{
			_instrumented_release_frame(msg->instrumented_frame);
			_instrumented_set_active_frame(NULL);
		}
#endif

		prev_msg = msg;
		msg = msg->next_task;

		_mali_sys_free(prev_msg);
	}
}

MALI_STATIC void *_mali_base_worker_thread(void *start_param)
{
	mali_base_worker *worker = MALI_REINTERPRET_CAST(mali_base_worker *)(start_param);
	mali_bool done = MALI_FALSE;

	MALI_DEBUG_ASSERT_POINTER(worker);

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_START | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_START_STOP_SW_WORKER_THREAD, 0, 0, 0, 0, 0);
#endif

	/* Set the scheduling policy for this thread to idle. */
	if (worker->idle_policy)
	{
		_mali_osu_thread_set_idle_scheduling_policy();
	}

	while (MALI_FALSE == done)
	{
		mali_base_worker_task *first_task = NULL;

		_mali_base_worker_thread_wait_for_message(worker);

		/* Protect task list. */
		_mali_sys_mutex_lock(worker->mutex);

		done = worker->processing_loop_done;

		/* Take all messages. */
		first_task = worker->first_task;

		worker->first_task = NULL;
		worker->last_task = NULL;

		/* Task list has been copied and emptied.  Unlock mutex to enable adding of more
		   tasks. */
		_mali_sys_mutex_unlock(worker->mutex);

		_mali_base_worker_thread_execute_tasks(first_task);
	}

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_STOP | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_START_STOP_SW_WORKER_THREAD, 0, 0, 0, 0, 0);
#endif

	/* Worker thread done. */
	return NULL;
}

MALI_STATIC mali_bool _mali_base_worker_init(mali_base_worker *worker)
{
	MALI_DEBUG_ASSERT_POINTER(worker);

	worker->mutex = _mali_sys_mutex_create();
	if (MALI_NO_HANDLE == worker->mutex) return MALI_FALSE;

	worker->message_lock = _mali_sys_lock_create();
	if (MALI_NO_HANDLE == worker->message_lock)
	{
		_mali_sys_mutex_destroy(worker->mutex);
		worker->mutex = MALI_NO_HANDLE;
		return MALI_FALSE;
	}

	/* Must be locked before the task processing thread starts. */
	_mali_sys_lock_lock(worker->message_lock);

	if (_MALI_OSU_ERR_OK != _mali_osu_create_thread(&worker->worker_thread, _mali_base_worker_thread, MALI_REINTERPRET_CAST(void *)(worker)))
	{
		_mali_sys_lock_unlock(worker->message_lock);
		_mali_sys_lock_destroy(worker->message_lock);
		worker->message_lock = MALI_NO_HANDLE;

		_mali_sys_mutex_destroy(worker->mutex);
		worker->mutex = MALI_NO_HANDLE;

		return MALI_FALSE;
	}

	return MALI_TRUE;
}

MALI_STATIC_INLINE void _mali_base_worker_signal_new_message(mali_base_worker *worker)
{
	mali_err_code err;

	MALI_DEBUG_ASSERT_POINTER(worker);

	/* This try lock prevents a double unlock from happening if a new task is added before the
	   worker thread manages to lock for the previous task add.  (If the lock is not locked, the
	   try lock *WILL* lock it.) */
	err = _mali_sys_lock_try_lock(worker->message_lock);
	MALI_IGNORE(err);

	/* Wake up the worker thread. */
	_mali_sys_lock_unlock(worker->message_lock);
}

MALI_STATIC void _mali_base_worker_quit(mali_base_worker *worker)
{
	u32 exit_code;
	_mali_osk_errcode_t err;

	MALI_DEBUG_ASSERT_POINTER(worker);

	_mali_sys_mutex_lock(worker->mutex);
	worker->processing_loop_done = MALI_TRUE;
	_mali_sys_mutex_unlock(worker->mutex);

	_mali_base_worker_signal_new_message(worker);

	err = _mali_osu_wait_for_thread(worker->worker_thread, &exit_code);
	MALI_IGNORE(err);

	_mali_base_worker_signal_new_message(worker);
}

mali_base_worker_handle _mali_base_worker_create(mali_bool idle_policy)
{
	mali_base_worker *worker_ptr = _mali_sys_calloc(1, sizeof(mali_base_worker));
	if (NULL == worker_ptr) return MALI_BASE_WORKER_NO_HANDLE;

	worker_ptr->idle_policy = idle_policy;
	if (MALI_FALSE == _mali_base_worker_init(worker_ptr))
	{
		_mali_sys_free(worker_ptr);
		return MALI_BASE_WORKER_NO_HANDLE;
	}

	return MALI_REINTERPRET_CAST(mali_base_worker_handle)worker_ptr;
}

void _mali_base_worker_destroy(mali_base_worker_handle worker_handle)
{
	mali_base_worker *worker = _mali_base_worker_get(worker_handle);

	MALI_DEBUG_ASSERT_POINTER(worker);

	_mali_base_worker_quit(worker);

	_mali_sys_mutex_destroy(worker->mutex);
	worker->mutex = MALI_NO_HANDLE;

	_mali_sys_lock_destroy(worker->message_lock);
	worker->message_lock = MALI_NO_HANDLE;

	_mali_sys_free(worker);
}

mali_err_code _mali_base_worker_task_add(mali_base_worker_handle worker_handle, mali_base_worker_task_proc task_proc, void *task_param)
{
	mali_base_worker      *worker   = _mali_base_worker_get(worker_handle);
	mali_base_worker_task *task_msg = NULL;

	MALI_DEBUG_ASSERT_POINTER(worker);

	task_msg = _mali_sys_calloc(1, sizeof(mali_base_worker_task));
	if (NULL == task_msg)
	{
		return MALI_ERR_OUT_OF_MEMORY;
	}
	task_msg->task_proc  = task_proc;
	task_msg->task_param = task_param;

	_mali_sys_mutex_lock(worker->mutex);

	if (MALI_TRUE == worker->processing_loop_done)
	{
		/* The worker thread has been signaled to quit. */
		_mali_sys_mutex_unlock(worker->mutex);
		_mali_sys_free(task_msg);
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* Add task to end of task queue. */
	if (NULL == worker->first_task) worker->first_task = task_msg;
	if (NULL != worker->last_task) worker->last_task->next_task = task_msg;
	worker->last_task = task_msg;

#if MALI_INSTRUMENTED
	task_msg->instrumented_frame = _instrumented_get_active_frame();
	if (NULL != task_msg->instrumented_frame)
	{
		_instrumented_addref_frame(task_msg->instrumented_frame);
	}
#endif

	/* Wake up the worker thread. */
	_mali_base_worker_signal_new_message(worker);

	/* Unlock the mutex after calling _mali_base_worker_signal_new_message to prevent a double
	   unlock. */
	_mali_sys_mutex_unlock(worker->mutex);

	return MALI_ERR_NO_ERROR;
}
