/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_instrumented_context.h"
#include "mali_instrumented_frame.h"
#include "mali_instrumented_context_types.h"
#include "mali_instrumented_log_types.h"

#include "mali_counters.h"
#include "mali_counters_dump.h"
#include "mali_log.h"
#include "sw_profiling.h"
#include "mri_interface.h"

#include "base/mali_debug.h"

/* EGL controls when this global is created and destroyed */
static mali_instrumented_context *__mali_instrumented_context = NULL;

/*
 * Create the global instrumented context.
 * Returns the context on success, NULL on failure.
 * Not thread safe, so we rely on EGL to do this safely.
 */
MALI_EXPORT
mali_instrumented_context * MALI_CHECK_RESULT _instrumented_create_context(void)
{
	const char *config_string;
	mali_instrumented_context *context = _mali_sys_calloc(1, sizeof(mali_instrumented_context));
	if (NULL != context)
	{
		mali_err_code err;
		err = _mali_sys_thread_key_set_data(MALI_THREAD_KEY_INSTRUMENTED_DATA, (void*)NULL);
		MALI_IGNORE(err);
		context->lock = _mali_sys_lock_create();
		if (MALI_NO_HANDLE != context->lock)
		{
			/* Initialize linked list of tls structures */
			if (MALI_ERR_NO_ERROR == __mali_linked_list_init(&context->tls_structures))
			{
				/* Initialize linked list of working frames */
				if (MALI_ERR_NO_ERROR == __mali_linked_list_init(&context->working_frames))
				{
					/* Initialize mali_counters stuff */
					context->header_root = _mali_sys_calloc( 1, sizeof(mali_counter_group) );
					if (NULL != context->header_root)
					{
						context->counter_map = __mali_named_list_allocate();
						if (NULL != context->counter_map)
						{
							context->header_root->name = _mali_sys_strdup("Root");
							if (NULL != context->header_root->name)
							{
								context->header_root->description = _mali_sys_strdup("");
								if (NULL != context->header_root->description)
								{
									context->header_root->parent = NULL;
									context->header_root->leaf = MALI_FALSE;
									context->header_root->num_children = 0;
									context->header_root->children.groups = NULL;
									context->current_header = context->header_root;
									context->dump_file = NULL;
									context->options_mask = 0;
									context->inited = MALI_FALSE;
									context->sampling = _mali_sys_config_string_get_bool("MALI_RECORD_SW", MALI_FALSE) ||
														_mali_sys_config_string_get_bool("MALI_RECORD_HW", MALI_FALSE) ||
														_mali_sys_config_string_get_bool("MALI_RECORD_ALL", MALI_FALSE);
									context->counters = NULL;

									/* Initialize logging stuff */
									context->log_level = MALI_LOG_LEVEL_NONE;
									context->log_file = NULL;
									context->callbacks = NULL;
									context->numcallbacks = 0;

#if !defined(USING_MALI200)
									context->prev_l2_perf_counters_count = 0;
#endif

									config_string = _mali_sys_config_string_get("MALI_LOG_FILE");
									if (NULL != config_string)
									{
										if (MALI_ERR_NO_ERROR == MALI_INSTRUMENTED_OPEN_LOG_FILE(context, config_string))
										{
											MALI_INSTRUMENTED_REGISTER_LOG_CALLBACK(context, _instrumented_write_to_log_file);
										}
									}
									_mali_sys_config_string_release(config_string);

									config_string = _mali_sys_config_string_get("MALI_LOG_LEVEL");
									if (NULL != config_string)
									{
										if (0 == _mali_sys_strcmp(config_string, "ERROR"))
										{
											context->log_level = MALI_LOG_LEVEL_ERROR;
										}
										else if (0 == _mali_sys_strcmp(config_string, "WARNING"))
										{
											context->log_level = MALI_LOG_LEVEL_WARNING;
										}
										else if (0 == _mali_sys_strcmp(config_string, "EVERYTHING"))
										{
											context->log_level = MALI_LOG_LEVEL_EVERYTHING;
										}
										else if (0 == _mali_sys_strcmp(config_string, "DEBUG"))
										{
											context->log_level = MALI_LOG_LEVEL_DEBUG;
										}
										else
										{
											MALI_DEBUG_PRINT(0, ("Unknown MALI_LOG_LEVEL %s\n", config_string));
										}
									}
									_mali_sys_config_string_release(config_string);

									if (MALI_ERR_NO_ERROR == _mali_profiling_init(context))
									{
										config_string = _mali_sys_config_string_get("MALI_DUMP_FILE");
										if (NULL != config_string && _mali_sys_strlen(config_string) > 0)
										{
											context->options_mask |= INSTRUMENT_DUMP_COUNTERS;
										}
										_mali_sys_config_string_release(config_string);

										if (_mali_sys_config_string_get_bool("MALI_DUMP_FRAMEBUFFER", MALI_FALSE))
										{
											context->options_mask |= INSTRUMENT_DUMP_FRAMEBUFFER;
										}
										
										if (_mali_sys_config_string_get_bool("MALI_DUMP_FRAMEBUFFER_HEX", MALI_FALSE))
										{
											context->options_mask |= INSTRUMENT_DUMP_FRAMEBUFFER_HEX;
										}

										if (_mali_sys_config_string_get_bool("MALI_DUMP_PRINT", MALI_FALSE))
										{
											context->options_mask |= INSTRUMENT_PRINT_COUNTERS;
										}

										if (_mali_sys_config_string_get_bool("MALI_DUMP_NET", MALI_FALSE))
										{
											context->options_mask |= INSTRUMENT_WAIT_FOR_MRI;
										}

										__mali_instrumented_context = context;
										return context;
									}

									_instrumented_destroy_tls_structures(context);
									_mali_sys_free(context->header_root->description);
								}

								_mali_sys_free(context->header_root->name);
							}

							__mali_named_list_free(context->counter_map, NULL);
						}

						_mali_sys_free(context->header_root);
					}

					__mali_linked_list_deinit(&context->working_frames);
				}

				__mali_linked_list_deinit(&context->tls_structures);
			}

			_mali_sys_lock_destroy(context->lock);
		}

		_mali_sys_free(context);
	}

	__mali_instrumented_context = NULL;
	return NULL;
} /* _instrumented_create_context */

MALI_EXPORT
/*
 * Destroy the global instrumented context.
 * Not thread safe, so we rely on EGL to do this safely.
 */
void _instrumented_destroy_context(void)
{
	unsigned int i;
	MALI_DEBUG_ASSERT_POINTER(__mali_instrumented_context);

	if (NULL == __mali_instrumented_context) return;

	/* close any dump files */
	_instrumented_close_dump(__mali_instrumented_context);

	/* close any log files */
	MALI_INSTRUMENTED_CLOSE_LOG_FILE(__mali_instrumented_context);

	/* free the tls struct */
	_instrumented_destroy_tls_structures(__mali_instrumented_context);

	/* free registered callbacks */
	_mali_sys_free(__mali_instrumented_context->callbacks);

	/* traverse and deallocate group tree */
	_instrumented_counter_free_group(__mali_instrumented_context->header_root);
	_mali_sys_free(__mali_instrumented_context->header_root);

	/* deallocate all counter info (incl. info about said counters) */
	for (i = 0; i < __mali_instrumented_context->num_counters; i++)
	{
		_instrumented_free_counter_info(&__mali_instrumented_context->counters[i]);
	}

	/* deallocate counter array */
	_mali_sys_free(__mali_instrumented_context->counters);

	/* Deallocate id->index counter map, but not the data it's pointing at (we just did that) */
	__mali_named_list_free(__mali_instrumented_context->counter_map, NULL);

	_free_MRPServer(__mali_instrumented_context);

	/* Deinit linked list of tls structures */
	__mali_linked_list_deinit(&__mali_instrumented_context->tls_structures);

	/* Deinit linked list of working frames */
	__mali_linked_list_deinit(&__mali_instrumented_context->working_frames);

	/* Destroy lock */
	_mali_sys_lock_destroy(__mali_instrumented_context->lock);

	_mali_sys_free(__mali_instrumented_context);
	__mali_instrumented_context = NULL;
}

MALI_EXPORT
void _instrumented_finish_init(void)
{
	mali_bool dump_sw = MALI_FALSE;
	mali_bool dump_hw = MALI_FALSE;
	mali_bool dump_l2 = MALI_FALSE;
#if !defined(USING_MALI200)
	mali_bool dump_l2_all = MALI_FALSE;
	u32 l2_enabled_counters[MALI_L2_NUMBER_OF_HW_COUNTER_REGISTERS] = { MALI_L2_HW_COUNTER_DISABLED, };
	const char* dump_l2_str;
	u32 i;
#endif

	MALI_DEBUG_ASSERT_POINTER(__mali_instrumented_context);
	if (__mali_instrumented_context == NULL) return;
	if (__mali_instrumented_context->inited)
	{
		MALI_DEBUG_PRINT(0, ("Context has already been initialized" ));
		return;
	}
	__mali_instrumented_context->inited = MALI_TRUE;

	dump_sw = dump_hw = _mali_sys_config_string_get_bool("MALI_RECORD_ALL", MALI_FALSE);
	dump_sw = _mali_sys_config_string_get_bool("MALI_RECORD_SW", dump_sw );
	dump_hw = _mali_sys_config_string_get_bool("MALI_RECORD_HW", dump_hw );

#if !defined(USING_MALI200)
	dump_l2_str = _mali_sys_config_string_get("MALI_RECORD_L2" );

	if (dump_l2_str != NULL && dump_l2_str[0] != '\0')
	{
		/*
		 * The way we sample Mali L2 cache performance counters is a bit different than the other GP and PP counters.
		 * MALI_RECORD_L2 can be "ALL" or a list of comma seperated numbers. The numbers correspond to which L2 counters which should be enabled
		 * Currently, only 2 counters can be activated at any given time in HW.
		 * The values for the L2 counters can be found in the TRM, or you can check the malil2_counters.h file (offset values).
		 */
		dump_l2 = MALI_TRUE;

		if (_mali_sys_strcmp(dump_l2_str, "ALL") == 0)
		{
			dump_l2_all = MALI_TRUE; /* dump all L2 counters (as many as we have HW registers for) */
		}
		else
		{
			const char* startpos = dump_l2_str;
			for (i = 0; i < MALI_L2_NUMBER_OF_HW_COUNTER_REGISTERS; i++)
			{
				u32 cur_id = 0;
				int pos = 0;

				while ((startpos[pos] >= '0') && (startpos[pos] <= '9'))
				{
					cur_id = (cur_id * 10) + (startpos[pos] - '0');
					pos++;
				}

				if (pos > 0)
				{
					l2_enabled_counters[i] = cur_id;
				}

				while (((startpos[pos] < '0') || (startpos[pos] > '9')) && startpos[pos] != '\0')
				{
					pos++; /* skip non-digits */
				}

				if (startpos[pos] == '\0')
				{
					break; /* end of string, no point in continuing */
				}

				startpos += pos;
			}
		}
	}

	_mali_sys_config_string_release(dump_l2_str);
	dump_l2_str = NULL;

	if (dump_hw && dump_l2)
	{
		dump_l2 = MALI_FALSE; /* Dumping L2 along with all HW counters doesn't make any sense, because this will run the same GP and PP job multiple times. */
	}
#endif

	if (dump_sw || dump_hw || dump_l2)
	{
		u32 iterator;
		void *pointer = NULL;

		__mali_instrumented_context->sampling = MALI_TRUE;

		/* enable all counters because MALI_RECORD_ALL is set */
		pointer = __mali_named_list_iterate_begin(__mali_instrumented_context->counter_map, &iterator);
		while (pointer != NULL)
		{
			mali_bool ret = MALI_FALSE;
			u32 index = MALI_REINTERPRET_CAST(u32)(pointer) - 1;
			mali_bool enable = MALI_FALSE;
			mali_bool is_l2_counter = MALI_FALSE;

#if !defined(USING_MALI200)
			is_l2_counter = (iterator >= MALI_L2_COUNTER_OFFSET) && (iterator <= MALI_L2_COUNTER_OFFSET_END);

			if (dump_l2 && is_l2_counter)
			{
				if (dump_l2_all)
				{
					enable = MALI_TRUE;
				}
				else
				{
					/* Check the l2_enabled_counters array to see if this counter is enabled */
					for (i = 0; i < MALI_L2_NUMBER_OF_HW_COUNTER_REGISTERS; i++)
					{
						u32 offset = 0;
						/* Check the equivalent counter for both per frame, per GP job and per PP job/core */
						while ((iterator + offset) <= MALI_L2_COUNTER_OFFSET_END)
						{
							if ((MALI_L2_COUNTER_OFFSET + l2_enabled_counters[i] + offset) == iterator)
							{
								enable = MALI_TRUE;
								break;
							}
							offset += MALI_L2_GROUP_OFFSET;
						}

						if (enable)
						{
							break;
						}
					}
				}
			}
#endif

			if (dump_hw && (iterator & MALI_COUNTER_HARDWARE_BIT) && !is_l2_counter) enable = MALI_TRUE;
			if (dump_sw && !(iterator & MALI_COUNTER_HARDWARE_BIT)) enable = MALI_TRUE;

			if (enable)
			{
				ret = _instrumented_activate_counters(__mali_instrumented_context, &index, 1);
			}
			MALI_IGNORE(ret); /* Silently fail if activation failed */

			pointer = __mali_named_list_iterate_next(__mali_instrumented_context->counter_map, &iterator);
		}
	}
	_instrumented_open_dump(__mali_instrumented_context);
	_instrumented_write_headers(__mali_instrumented_context);

	if (__mali_instrumented_context->options_mask & INSTRUMENT_WAIT_FOR_MRI)
	{
		int mri_port = (int)_mali_sys_config_string_get_s64("MALI_DUMP_PORT", 3471, 1, 65536);
		_init_MRPServer(__mali_instrumented_context, mri_port);
	}
}  /* end of _instrumented_finish_init */



MALI_STATIC void _instrumented_destroy_tls( void *tls_struct )
{
	mali_err_code err;
	mali_instrumented_tls_struct *tls_structure =
	MALI_REINTERPRET_CAST(mali_instrumented_tls_struct*)(tls_struct);
	MALI_DEBUG_ASSERT_POINTER(tls_structure);

	clear_profiling_buffer( &(tls_structure->_mali_profiling_buffer) );

	_mali_sys_free(tls_structure);
    err = _mali_sys_thread_key_set_data(MALI_THREAD_KEY_INSTRUMENTED_DATA, (void*)NULL);
	MALI_IGNORE(err);
}

MALI_EXPORT
void _instrumented_destroy_tls_structures( mali_instrumented_context *ctx )
{
	__mali_linked_list_lock(&ctx->tls_structures);
	__mali_linked_list_empty(&ctx->tls_structures, _instrumented_destroy_tls);
	__mali_linked_list_unlock(&ctx->tls_structures);
}

MALI_STATIC_INLINE mali_instrumented_tls_struct* _instrumented_create_tls()
{
	mali_err_code err;
	mali_instrumented_tls_struct *tlspointer = _mali_sys_malloc(sizeof(mali_instrumented_tls_struct));
	if (NULL == tlspointer) return NULL;

	_mali_sys_memset(tlspointer, 0, sizeof(mali_instrumented_tls_struct));
    err = _mali_sys_thread_key_set_data(MALI_THREAD_KEY_INSTRUMENTED_DATA, tlspointer);
	if (err != MALI_ERR_NO_ERROR)
	{
		_mali_sys_free(tlspointer);
		return NULL;
	}
	return tlspointer;
}

MALI_STATIC_INLINE mali_err_code _register_tls_for_destruction(void *tlspointer, mali_instrumented_context *ctx)
{
	/* Insert a pointer to the new tls into the list of tls structures in
	   the instrumented context for later cleanup */
	__mali_linked_list_lock(&ctx->tls_structures);
	if (MALI_ERR_NO_ERROR != __mali_linked_list_insert_data(&ctx->tls_structures, tlspointer))
	{
		mali_err_code err;
		__mali_linked_list_unlock(&ctx->tls_structures);

        err = _mali_sys_thread_key_set_data(MALI_THREAD_KEY_INSTRUMENTED_DATA, (void*)NULL);
		MALI_IGNORE(err);
		return MALI_ERR_FUNCTION_FAILED;
	}
	__mali_linked_list_unlock(&ctx->tls_structures);
	MALI_SUCCESS;
}

MALI_EXPORT
mali_instrumented_tls_struct* _instrumented_get_tls_with_context( mali_instrumented_context *ctx )
{
	/* retrieve the thread local storage struct.. */
    mali_instrumented_tls_struct *tlspointer =
		_mali_sys_thread_key_get_data(MALI_THREAD_KEY_INSTRUMENTED_DATA);

	/* .. or allocate it if it's not there yet */
	if (NULL == tlspointer)
	{
		tlspointer = _instrumented_create_tls();
		if (NULL == tlspointer)
		{
			return NULL;
		}

		MALI_DEBUG_ASSERT_POINTER(ctx);

		if (MALI_ERR_NO_ERROR != _register_tls_for_destruction(tlspointer, ctx))
		{
			_instrumented_destroy_tls( tlspointer );
			return NULL;
		}
	}
	return tlspointer;
}

MALI_EXPORT
mali_instrumented_tls_struct* _instrumented_get_tls( void )
{
	/* retrieve the thread local storage struct.. */
    mali_instrumented_tls_struct *tlspointer =
		_mali_sys_thread_key_get_data(MALI_THREAD_KEY_INSTRUMENTED_DATA);

	/* .. or allocate it if it's not there yet */
	if (NULL == tlspointer)
	{
		mali_instrumented_context *ctx;

		tlspointer = _instrumented_create_tls();
		if (NULL == tlspointer)
		{
			return NULL;
		}

		ctx = _instrumented_get_context();
		MALI_DEBUG_ASSERT_POINTER(ctx);
		if (NULL == ctx)
		{
			_instrumented_destroy_tls( tlspointer );
			return NULL;
		}

		if (MALI_ERR_NO_ERROR != _register_tls_for_destruction(tlspointer, ctx))
		{
			_instrumented_destroy_tls( tlspointer );
			return NULL;
		}
	}
	return tlspointer;
}

/* coverity[-alloc] -- stop Coverity from detecting this as an allocation function. */
MALI_EXPORT
mali_instrumented_context *_instrumented_get_context(void)
{
	return __mali_instrumented_context;
}

MALI_EXPORT
void _instrumented_context_finish_frames(mali_instrumented_context *ctx)
{
	/* Finish the frames we can */
	mali_linked_list_entry *entry;

	_mali_sys_lock_lock(ctx->lock);

	__mali_linked_list_lock(&ctx->working_frames);

	entry = __mali_linked_list_get_first_entry(&ctx->working_frames);
	while (NULL != entry)
	{
		mali_instrumented_frame *frame = (mali_instrumented_frame *)entry->data;

		if (!_instrumented_frame_is_completed(frame))
		{
			break;
		}

		_instrumented_finish_frame(frame);
		entry = __mali_linked_list_remove_entry(&ctx->working_frames, entry); /* this will return the next valid element */
	}

	if (NULL == __mali_linked_list_get_first_entry(&ctx->working_frames))
	{
		/* all working frames are completed, check current frame */
		if (NULL != ctx->current_frame)
		{
			if (_instrumented_frame_is_completed(ctx->current_frame))
			{
				_instrumented_finish_frame(ctx->current_frame);
				ctx->current_frame = NULL;
			}
		}
	}

	__mali_linked_list_unlock(&ctx->working_frames);
	_mali_sys_lock_unlock(ctx->lock);
}
