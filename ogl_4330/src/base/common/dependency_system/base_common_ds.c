/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_ds.c
 * Implementation of the dependency system.
 * See base_dependency_system.h for documentation of the public functions
 */

#include <base/mali_debug.h>
#include <base/common/base_common_sync_handle.h>
#include <base/common/mem/base_common_mem.h>
#include <base/common/dependency_system/base_common_ds.h>
#include <base/common/base_common_context.h>
#include <base/arch/base_arch_runtime.h>

#define ASSERT_TXT ("Error in mali dependency system")

/** Assert macros that checks that the system is sane in Debug builds */
#define MALI_DEBUG_ASSERT_RESOURCE(resource) MALI_DEBUG_ASSERT( \
		((NULL!=(resource))&&((resource)->magic_number==MALI_DS_RESOURCE_MAGIC_NR)), \
		("Illegal resource"))

#define MALI_DEBUG_ASSERT_CONSUMER(consumer) MALI_DEBUG_ASSERT( \
		((NULL!=(consumer))&&((consumer)->magic_number==MALI_DS_CONSUMER_MAGIC_NR)), \
		("Illegal consumer"))

#define MALI_DEBUG_ASSERT_CONNECTION(connection) MALI_DEBUG_ASSERT( \
			(   (NULL!=(connection))&&\
				((connection)->magic_number==MALI_DS_CONNECTION_MAGIC_NR)&&\
				( ((connection)->connection_resource)->magic_number==MALI_DS_RESOURCE_MAGIC_NR )&&\
				( ((connection)->connection_consumer)->magic_number==MALI_DS_CONSUMER_MAGIC_NR )\
			), \
		    ("Illegal connection"))

#define MALI_DEBUG_ASSERT_MANAGER(manager) MALI_DEBUG_ASSERT( \
		((NULL!=(manager))&&(manager->magic_number==MALI_DS_MANAGER_MAGIC_NR)), \
		("Illegal manager"))

MALI_STATIC void resource_schedule(mali_ds_resource * resource);
MALI_STATIC void connection_trigger(mali_ds_connection * connection);
MALI_STATIC void consumer_internal_activate(mali_ds_consumer * consumer);
MALI_STATIC void consumer_internal_release(mali_ds_consumer * consumer);
MALI_STATIC void resource_internal_release(mali_ds_resource * resource);
MALI_STATIC mali_ds_connection * manager_alloc_connection(mali_ds_manager * manager);
MALI_STATIC void manager_release_connection(mali_ds_manager * manager, mali_ds_connection * connection);
MALI_STATIC void internal_release_connections(mali_ds_consumer * consumer, mali_bool do_release_callback);
MALI_STATIC_INLINE void global_list_manipulation_mutex__grab(mali_ds_manager * manager,u32 tid);
MALI_STATIC_INLINE void global_list_manipulation_mutex__release(mali_ds_manager * manager,u32 tid);

#ifndef MALI_DEBUG_SKIP_CODE
MALI_STATIC void debug_ds_system_print_consumer(mali_ds_consumer * consumer);
MALI_STATIC void debug_ds_system_print_resource(mali_ds_resource * resource);
MALI_STATIC void internal_debug_ds_system_print_consumer(mali_ds_consumer * consumer, int indent_level);
MALI_STATIC void internal_debug_ds_system_print_resource(mali_ds_resource * resource, int indent_level);
#endif /* #ifndef MALI_DEBUG_SKIP_CODE*/

static struct mali_ds_manager * mali_global_ds_manager = NULL;

#define get_manager(X) mali_global_ds_manager

#if 0
MALI_STATIC_INLINE mali_ds_manager * get_manager(mali_base_ctx_handle ctx)
{
	return MALI_STATIC_CAST(mali_ds_manager * ) _mali_base_common_context_get_dependency_system_context(ctx);
}
#endif


MALI_EXPORT mali_ds_resource_handle  mali_common_ds_resource_allocate(mali_base_ctx_handle ctx,
                                                                      void * owner,
                                                                      mali_ds_cb_func_resource cb_on_release)
{
	mali_ds_resource * resource;
	mali_ds_resource_handle retval;

	MALI_IGNORE(ctx);

	retval=(mali_ds_resource_handle)MALI_NO_HANDLE;
	resource = _mali_sys_malloc(sizeof(*resource));
	if ( NULL==resource )
	{
		return retval;
	}

	resource->owner=owner;
	resource->ref_count_connections_out=0;
	resource->release_abort_type=MALI_DS_ABORT_NONE;
	MALI_EMBEDDED_LIST_HEAD_INIT_RUNTIME(&resource->connections_out);
	resource->state=MALI_DS_RESOURCE_STATE_UNUSED;
	resource->cb_on_release=cb_on_release;
	resource->manager=get_manager(ctx);
	resource->shutdown = MALI_FALSE;

	MALI_DEBUG_CODE( resource->magic_number = MALI_DS_RESOURCE_MAGIC_NR;)

	retval=MALI_STATIC_CAST(mali_ds_resource_handle)resource;
	return retval;
}

MALI_EXPORT void mali_common_ds_resource_set_callback_parameter(mali_ds_resource_handle resource_h, void * owner)
{
	mali_ds_resource * resource;
	resource = MALI_STATIC_CAST(mali_ds_resource *)resource_h;
	MALI_DEBUG_ASSERT_RESOURCE(resource);
	resource->owner = owner;
}


MALI_EXPORT mali_bool mali_common_ds_resource_has_dependencies(mali_ds_resource_handle resource_h)
{
	mali_ds_resource * resource;
	resource = MALI_STATIC_CAST(mali_ds_resource *)resource_h;
	MALI_DEBUG_ASSERT_RESOURCE(resource);
	return (resource->ref_count_connections_out!=0) ? MALI_TRUE : MALI_FALSE ;
}


MALI_EXPORT void mali_common_ds_consumer_set_callback_parameter(mali_ds_consumer_handle consumer_h, void * owner)
{
	mali_ds_consumer * consumer;
	consumer = MALI_STATIC_CAST(mali_ds_consumer *)consumer_h;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);
	consumer->owner = owner;
}

MALI_STATIC void connection_remove(mali_ds_connection * connection)
{
	mali_ds_manager  *manager;
	mali_ds_resource *resource;

	resource = connection->connection_resource;
	manager  = resource->manager;

	/* Remove connection from both lists */
	_mali_embedded_list_remove(&connection->element_on_consumer_list);
	_mali_embedded_list_remove(&connection->element_on_resource_list);

	/* We need to decrement the ref_count of the consumer not already triggered */
	if ( MALI_FALSE == connection->is_trigged )
	{
		/* The consumer */
		connection->connection_consumer->ref_count_untrigged_connections--;
	}

	/* We always need to decrement the ref_count of the resource */
	/* The calling function must schedule the resource afterwards!!! */
	resource->ref_count_connections_out--;
	/* Release the connection object */
	manager_release_connection(manager,connection);
}


MALI_EXPORT void mali_common_ds_resource_release_connections(mali_ds_resource_handle resource_h,
                                                             mali_ds_release keep_resource,
                                                             mali_ds_abort abort_flag)
{
	mali_ds_manager * manager;
	mali_ds_resource * resource;
	mali_ds_abort stored_abort_type;
	mali_ds_abort new_abort_type;
	u32 tid =_mali_sys_thread_get_current();

	resource = MALI_STATIC_CAST(mali_ds_resource *)resource_h;
	MALI_DEBUG_ASSERT_RESOURCE(resource);
	manager  = resource->manager;

	global_list_manipulation_mutex__grab(manager,tid);

	/* This will let us do asynchronous release.
	   It will now be actually released when all connections are gone.*/
	if ( MALI_DS_RELEASE==keep_resource)
	{
		resource->shutdown = MALI_TRUE;
	}

	stored_abort_type = resource->release_abort_type;
	new_abort_type = abort_flag;

	/* If there was nothing to abort, quickjump to end*/
	if  ( MALI_DS_ABORT_NONE == new_abort_type )
	{
		goto exit_function;
	}

	/* Setting the resource object, if thew new type has higher priority than the existing */
	if (new_abort_type > stored_abort_type)
	{
		resource->release_abort_type = new_abort_type;
	}

	/* If the original value was set, this is a callback releasing it once more. Quickjump to exit*/
	if  ( MALI_DS_ABORT_NONE != stored_abort_type )
	{
		goto exit_function;
	}

	/* Do the loop that releases all connections connected to this resource.
	   Since we may set MALI_DS_ABORT_WAITING, which will not abort connections to consumers that
	   has been activated (frames with running jobs) we may not abort all connections.
	   However; some connections we release will do callbacks that release other connections,
	   and that might also change the priority from MALI_DS_ABORT_WAITING to MALI_DS_ABORT_ALL.
	   In that case the callbackfunction will not do the extra release of this resource concurrently
	   but update the resource->release_abort_type, so that this while takes another turn and
	   actually release them all. */
	while  (resource->release_abort_type > stored_abort_type )
	{
		mali_ds_connection * connection;
		mali_bool end_of_list;

		stored_abort_type = resource->release_abort_type;

		end_of_list = MALI_FALSE;

		/* This is the only (known) way of traversing all the connections
		    and delete them. Since when one is deleted, we must run its callback function,
		    and the callback function may delete other dependencies. So we can not trust
		    the current connections next pointer, and must each time start traversing from
		    the beginning.. The only thing we trust is this resource object, since it
		    has been locked by setting the resource->release_abort_type variable, and
		    since it helds the global mutex.*/
		while (MALI_FALSE==end_of_list)
		{
			mali_ds_consumer * consumer;
			consumer = NULL;
			MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(connection,  &resource->connections_out, mali_ds_connection, element_on_resource_list)
			{
				/* Not abort activated consumers if the abort is set to just ABORT_WAITING. */
				if ( ( MALI_DS_ABORT_WAITING==stored_abort_type) &&
					 ( MALI_DS_CONSUMER_STATE_ACTIVATED==connection->connection_consumer->state )
				   )
				{
					continue;
				}
				consumer = connection->connection_consumer;
				break;
			}
			if (NULL==consumer)
			{
				end_of_list = MALI_TRUE;
				break;
			}

			/* Remove this connection from the Consumer and the Resource it is connected to */
			connection_remove(connection);

			/* The rest we abort. Start setting ERROR Status on the consumer */
			consumer->error_value=MALI_DS_ERROR;

			/* Do proper handling of the consumer that now has an error.*/
			switch(consumer->state)
			{
				case MALI_DS_CONSUMER_STATE_UNUSED:
					/* This is not possible since it had at least one connection */
					MALI_DEBUG_ASSERT(0, ASSERT_TXT);
					/* Fall through */
				case MALI_DS_CONSUMER_STATE_PREPARING:
					/* Fall through */
				case MALI_DS_CONSUMER_STATE_WAITING:
					/* This function will call a user callback function.
					We have protected the current resource by setting the
					resource->release_abort_type*/
					consumer_internal_activate(consumer);
					break;
				case MALI_DS_CONSUMER_STATE_ACTIVATED:
					/* This function will call a user callback function.
					We have protected the current resource by setting the
					resource->release_abort_type*/
					internal_release_connections(consumer, MALI_TRUE);
					break;
				default:
					MALI_DEBUG_ASSERT(0, ASSERT_TXT);
			} /* switch */
		} /* while not end of list of connections */
	} /* while this abort type is a higher one that has been runned before */

	exit_function:
	/* This will release the resource if it is empty and in shutdown mode.
	   In that case it may call a callback function.
	   Callback functions are safe at this point since we do not do anything
	   more in this function */
	resource_schedule(resource);

	global_list_manipulation_mutex__release(manager,tid);
}

MALI_EXPORT mali_ds_consumer_handle mali_common_ds_consumer_allocate(mali_base_ctx_handle ctx,
                                                                     void * owner,
                                                                     mali_ds_cb_func_consumer_activate cb_func_activate,
                                                                     mali_ds_cb_func_consumer_release cb_func_release)
{
	mali_ds_consumer * consumer;
	mali_ds_consumer_handle retval;
	mali_ds_manager * manager;

	MALI_IGNORE(ctx);

	retval=(mali_ds_consumer_handle)MALI_NO_HANDLE;
	manager =  get_manager(ctx);

	consumer = _mali_sys_malloc(sizeof(*consumer));
	if ( NULL!=consumer )
	{
		consumer->owner=owner;
		consumer->ref_count_untrigged_connections = 1; /* We treat non-flushed as a dependency */
		MALI_EMBEDDED_LIST_HEAD_INIT_RUNTIME(&consumer->connections_linked_list_header);
		consumer->cb_func_activate    = cb_func_activate;
		consumer->cb_func_release     = cb_func_release;
		consumer->cb_func_replace_resource = NULL;
		consumer->release_ref_count   = 0;
		consumer->state               = MALI_DS_CONSUMER_STATE_UNUSED;
		consumer->error_value         = MALI_DS_OK;
		consumer->release_mode        = MALI_DS_RELEASE_ALL;
		consumer->delete_self         = MALI_FALSE;
		consumer->manager             = manager;
		consumer->blocked_waiter_list = NULL;

		MALI_DEBUG_CODE( consumer->magic_number = MALI_DS_CONSUMER_MAGIC_NR;)

		retval=MALI_STATIC_CAST(mali_ds_consumer_handle)consumer;
	}

	return retval;
}

MALI_EXPORT void mali_common_ds_consumer_set_callback_activate(mali_ds_consumer_handle consumer_h, mali_ds_cb_func_consumer_activate cb_func_activate)
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();

	consumer=MALI_STATIC_CAST(mali_ds_consumer *)consumer_h;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);
	manager = consumer->manager;

	global_list_manipulation_mutex__grab(manager,tid);
	consumer->cb_func_activate = cb_func_activate;
	global_list_manipulation_mutex__release(manager,tid);
}

MALI_EXPORT void mali_common_ds_consumer_set_callback_release(mali_ds_consumer_handle consumer_h, mali_ds_cb_func_consumer_release cb_func_release)
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();

	consumer=MALI_STATIC_CAST(mali_ds_consumer *)consumer_h;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);
	manager = consumer->manager;

	global_list_manipulation_mutex__grab(manager,tid);
	consumer->cb_func_release = cb_func_release;
	global_list_manipulation_mutex__release(manager,tid);
}


MALI_EXPORT void mali_common_ds_consumer_set_callback_replace_resource(mali_ds_consumer_handle consumer_h, mali_ds_cb_func_consumer_replace_resource cb_func)
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();

	consumer=MALI_STATIC_CAST(mali_ds_consumer *)consumer_h;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);
	manager = consumer->manager;

	global_list_manipulation_mutex__grab(manager,tid);
	consumer->cb_func_replace_resource = cb_func;
	global_list_manipulation_mutex__release(manager,tid);
}

MALI_STATIC mali_bool is_a_tmp_consumer(const mali_ds_consumer * consumer)
{
	return (consumer->owner == consumer) ? MALI_TRUE : MALI_FALSE;
}

MALI_STATIC void internal_release_connections(mali_ds_consumer * consumer, mali_bool do_release_callback)
{
	mali_ds_error   error_value;
	mali_ds_connection * connection;
	mali_embedded_list_link *temp;
	mali_ds_manager * manager = consumer->manager;
	mali_bool delete_self = consumer->delete_self ;
	mali_ds_release_mode release_mode = consumer->release_mode;
	mali_embedded_list_link  to_delete_list;
	u32 tid =_mali_sys_thread_get_current();

	void *owner = consumer->owner;
	mali_ds_cb_func_consumer_release cb_func = consumer->cb_func_release;
	mali_ds_release cb_result = MALI_DS_RELEASE;

	error_value = consumer->error_value;

	MALI_EMBEDDED_LIST_HEAD_INIT_RUNTIME(&to_delete_list);

    /* Reset the consumer's internals */
	consumer->ref_count_untrigged_connections = 1;
	consumer->release_ref_count   = 0;
	consumer->error_value         = MALI_DS_OK ;
	consumer->release_mode        = MALI_DS_RELEASE_ALL;

	if (MALI_DS_RELEASE_ALL==release_mode)
	{
		consumer->state = MALI_DS_CONSUMER_STATE_UNUSED;
		/* Release all connections from the consumer - starting from the last (tail)
		   the last one is most likely a DS_WRITE from the display consumer - kick that one first */
		MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE_REVERSE(connection, temp, &consumer->connections_linked_list_header, mali_ds_connection, element_on_consumer_list)
		{
			mali_ds_resource * resource;

			/* Remove connection from resource list */
			_mali_embedded_list_remove(&connection->element_on_resource_list);

			resource = connection->connection_resource;
			/* We always need to decrement the ref_count of the resource */
			resource->ref_count_connections_out--;
			/* This might trigger callbacks, since it may activate other consumers */
			resource_schedule(resource);

			/* move it into the cleanup list */
			_mali_embedded_list_remove(&connection->element_on_consumer_list);
			_mali_embedded_list_insert_before(&to_delete_list, &connection->element_on_consumer_list);
		}
	}
	else
	{
		consumer->state = MALI_DS_CONSUMER_STATE_PREPARING;

		MALI_DEBUG_ASSERT(MALI_DS_RELEASE_WRITE_GOTO_UNFLUSHED==release_mode, ASSERT_TXT);
		/* Transfer all WRITE connections to the tmp_consumer, the READ connections remain */
		MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE(connection, temp, &consumer->connections_linked_list_header, mali_ds_connection, element_on_consumer_list)
		{
			mali_ds_resource * resource;

			if (MALI_DS_READ==connection->rights) continue;

			/* In this release mode we release the Write connections only */
			MALI_DEBUG_ASSERT(MALI_DS_WRITE==connection->rights, ASSERT_TXT);

			/* Remove connection from resource list */
			_mali_embedded_list_remove(&connection->element_on_resource_list);

			resource = connection->connection_resource;
			/* We always need to decrement the ref_count of the resource */
			resource->ref_count_connections_out--;
			/* This might trigger callbacks, since it may activate other consumers */
			resource_schedule(resource);

			/* move it into the cleanup list */
			_mali_embedded_list_remove(&connection->element_on_consumer_list);
			_mali_embedded_list_insert_before(&to_delete_list, &connection->element_on_consumer_list);
		}
	}

	global_list_manipulation_mutex__release(manager,tid);

	/* Callback function. We do not touch the consumer object after this */
	if ( (MALI_TRUE == do_release_callback) && (NULL != cb_func) )
	{
		cb_result = cb_func(manager->ctx, owner, error_value);
	}

	global_list_manipulation_mutex__grab(manager,tid);

	/* now free the memory */
	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE(connection, temp, &to_delete_list, mali_ds_connection, element_on_consumer_list)
	{
		/* Release the connection object */
		_mali_embedded_list_remove(&connection->element_on_consumer_list);
		manager_release_connection(manager,connection);
	}

	MALI_DEBUG_ASSERT(MALI_DS_RELEASE == cb_result, ("We do not support CMU behaviour anymore. Dependencies will be released before the callback"));
	if ( MALI_DS_RELEASE == cb_result )
	{
		if ( (MALI_TRUE == delete_self) && (MALI_DS_RELEASE_ALL == release_mode) )
		{
			consumer_internal_release(consumer);
		}
	}
}

MALI_STATIC void consumer_internal_release_check(mali_ds_consumer * consumer)
{
	switch(consumer->state)
	{

		case MALI_DS_CONSUMER_STATE_UNUSED:
			MALI_DEBUG_ASSERT(_mali_embedded_list_is_empty(&consumer->connections_linked_list_header), ASSERT_TXT);
			consumer->error_value = MALI_DS_OK;
			if ( MALI_TRUE == consumer->delete_self )
			{
				consumer_internal_release(consumer);
			}
			break;
		case MALI_DS_CONSUMER_STATE_PREPARING:
			if ( (0==consumer->release_ref_count) )
			{
				internal_release_connections(consumer, MALI_FALSE);
			}
			break;
		case MALI_DS_CONSUMER_STATE_WAITING:
			consumer->error_value = MALI_DS_ERROR;
			/* Callback - Do NOT touch the consumer in this function after this */
			consumer_internal_activate(consumer);
			break;
		case MALI_DS_CONSUMER_STATE_ACTIVATED:
			if ( 0==consumer->release_ref_count )
			{
				internal_release_connections(consumer, MALI_TRUE);
			}
			break;
		default:
			MALI_DEBUG_ASSERT(0, ("Illegal state\n"));
	}

}

MALI_EXPORT int mali_common_ds_consumer_active(mali_ds_consumer_handle consumer_h)
{
	mali_ds_consumer * consumer;

	consumer=MALI_STATIC_CAST(mali_ds_consumer *)consumer_h;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);

	return consumer->state != MALI_DS_CONSUMER_STATE_UNUSED;
}


MALI_EXPORT void mali_common_ds_consumer_release_ref_count_change(mali_ds_consumer_handle consumer_h, signed int ref_count_change)
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();

	consumer=MALI_STATIC_CAST(mali_ds_consumer *)consumer_h;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);
	manager = consumer->manager;

	global_list_manipulation_mutex__grab(manager,tid);

	/* 2 Special cases for the ref_count_change value */
	if ( MALI_DS_REF_COUNT_TRIGGER==ref_count_change)
	{
		ref_count_change = -consumer->release_ref_count;
	}

	consumer->release_ref_count += ref_count_change;
	MALI_DEBUG_ASSERT(consumer->release_ref_count >=0, ("Illegal ref_count setting\n"));

	consumer_internal_release_check(consumer);

	global_list_manipulation_mutex__release(manager,tid);
}

MALI_EXPORT void mali_common_ds_consumer_release_set_mode(mali_ds_consumer_handle consumer_h, mali_ds_release_mode mode)
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();
	consumer=MALI_STATIC_CAST(mali_ds_consumer *)consumer_h;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);

	manager = consumer->manager;
	global_list_manipulation_mutex__grab(manager,tid);
	if ( MALI_DS_CONSUMER_STATE_UNUSED==consumer->state ) consumer->state = MALI_DS_CONSUMER_STATE_PREPARING;
	consumer->release_mode = mode;

	global_list_manipulation_mutex__release(manager,tid);
}

MALI_EXPORT void mali_common_ds_consumer_set_error(mali_ds_consumer_handle consumer_h)
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();
	consumer=MALI_STATIC_CAST(mali_ds_consumer *)consumer_h;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);

	manager = consumer->manager;
	global_list_manipulation_mutex__grab(manager,tid);

	/* Setting the consumer work to have an ERROR - will last until it releases its connections */
	consumer->error_value = MALI_DS_ERROR;

	/* If the consumer was in its initial state, the state will be changed to PREPARING,
	since the internal variables are not equal to the reset state any more */
	if ( MALI_DS_CONSUMER_STATE_UNUSED==consumer->state )
	{
		consumer->state = MALI_DS_CONSUMER_STATE_PREPARING;
	}


	global_list_manipulation_mutex__release(manager,tid);
}

MALI_EXPORT void mali_common_ds_consumer_free(mali_ds_consumer_handle consumer_h)
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();
	consumer=MALI_STATIC_CAST(mali_ds_consumer *)consumer_h;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);

	manager = consumer->manager;
	global_list_manipulation_mutex__grab(manager,tid);

	consumer->delete_self = MALI_TRUE;

	consumer_internal_release_check(consumer);

	global_list_manipulation_mutex__release(manager,tid);

}

MALI_EXPORT void mali_common_ds_consumer_activation_ref_count_change(mali_ds_consumer_handle consumer_in, signed int ref_count_change )
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();

	consumer=MALI_STATIC_CAST(mali_ds_consumer * )consumer_in;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);

	manager = consumer->manager;
	global_list_manipulation_mutex__grab(manager,tid);

	consumer->ref_count_untrigged_connections += ref_count_change;
	MALI_DEBUG_ASSERT(consumer->ref_count_untrigged_connections  >= 0 , ASSERT_TXT);

	/* Consumer event will trigger when the ref_count is decreased for all
	   connections and released release_ref_count_set number of times */
	if(0==consumer->ref_count_untrigged_connections)
	{
		MALI_DEBUG_ASSERT( (MALI_DS_CONSUMER_STATE_WAITING==consumer->state) , ASSERT_TXT);
		consumer_internal_activate(consumer);
	}

	global_list_manipulation_mutex__release(manager,tid);
}

MALI_EXPORT void mali_common_ds_consumer_flush(mali_ds_consumer_handle consumer_in)
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();

	consumer=MALI_STATIC_CAST(mali_ds_consumer * )consumer_in;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);
	MALI_DEBUG_ASSERT( (MALI_DS_CONSUMER_STATE_PREPARING==consumer->state) || \
			 (MALI_DS_CONSUMER_STATE_UNUSED==consumer->state ), ASSERT_TXT);

	manager = consumer->manager;
	global_list_manipulation_mutex__grab(manager,tid);

	consumer->state = MALI_DS_CONSUMER_STATE_WAITING;

	consumer->ref_count_untrigged_connections--; /* Release the 1st ref_count used until flush */

	MALI_DEBUG_ASSERT( (consumer->ref_count_untrigged_connections >=0 ) , ASSERT_TXT);

	/* Consumer event will trigger when the ref_count is decreased for all
	   connections and released release_ref_count_set number of times */

	if(0==consumer->ref_count_untrigged_connections)
	{
		consumer_internal_activate(consumer);
	}

	global_list_manipulation_mutex__release(manager,tid);
}


/** This function flush this consumer, and do then wait until it is actually
 * activated before returning.
 * @NOTE Can not be called from inside a DS callback function.
 */
MALI_EXPORT mali_err_code mali_common_ds_consumer_flush_and_wait(mali_ds_consumer_handle consumer_in)
{
	mali_ds_consumer * consumer;
	mali_ds_manager * manager;
	mali_ds_wait_element * wait_element;
	mali_base_wait_handle wait = MALI_NO_HANDLE;
	mali_err_code err;
	u32 tid =_mali_sys_thread_get_current();

	consumer=MALI_STATIC_CAST(mali_ds_consumer * )consumer_in;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);
	MALI_DEBUG_ASSERT( (MALI_DS_CONSUMER_STATE_PREPARING==consumer->state) || \
			 (MALI_DS_CONSUMER_STATE_UNUSED==consumer->state ), ASSERT_TXT);

	manager = consumer->manager;
	global_list_manipulation_mutex__grab(manager,tid);

	/* This code just checks that we dont use this function from a DS callback.
	   The current implementation does not support that, since if it is in a callback
	   the current thread has locked the DS mutex, and other threads can not release
	   what we will wait for. */
	MALI_DEBUG_CODE(
		{
			if ( 1 != manager->global_list_lock_counter)
			{
				MALI_DEBUG_ERROR( ("Waiting for DS release in DS callback is not legal. Implementation error; Might lead to hang.") );
			}
		}
	); /* MALI_DEBUG_CODE */

	wait_element = _mali_sys_malloc(sizeof(*wait_element ));
	if ( NULL==wait_element)
	{
		err = MALI_ERR_OUT_OF_MEMORY;
		goto exit_function;
	}
	wait = _mali_base_arch_sys_wait_handle_create();

	if ( MALI_NO_HANDLE==wait)
	{
		_mali_sys_free(wait_element);
		err = MALI_ERR_OUT_OF_MEMORY;
		goto exit_function;
	}

	/* Nothing more can fail in this function */
	err = MALI_ERR_NO_ERROR;

	/* Add this element to the wait queue */
	wait_element->wait_handle = wait;
	wait_element->next = consumer->blocked_waiter_list;
	consumer->blocked_waiter_list = wait_element;

	/* Do the Flushing */
	consumer->state = MALI_DS_CONSUMER_STATE_WAITING;
	consumer->ref_count_untrigged_connections--; /* Release the 1st ref_count used until flush */

	/* Do activation, if everything it waits for is satisfied */
	if(0==consumer->ref_count_untrigged_connections)
	{
		consumer_internal_activate(consumer);
		err = MALI_ERR_NO_ERROR;
		goto exit_function;
	}

exit_function:
	global_list_manipulation_mutex__release(manager,tid);

	/* Do the waiting - if this was allocated*/
	if ( MALI_NO_HANDLE != wait )
	{
		_mali_base_arch_sys_wait_handle_wait(wait);
	}
	return err;
}

MALI_EXPORT mali_err_code  mali_common_ds_connect(mali_ds_consumer_handle consumer_handle,
                                                 mali_ds_resource_handle resource_handle,
                                                 mali_ds_connection_type rights)
{

	mali_ds_resource * resource;
	mali_ds_consumer * consumer;
	mali_ds_connection * new_connection;
	mali_bool same_manager;
	mali_ds_manager * manager;
	mali_err_code err;
	mali_bool write_is_trigged = MALI_FALSE;
	u32 tid =_mali_sys_thread_get_current();

	consumer = MALI_STATIC_CAST(mali_ds_consumer * ) consumer_handle;
	resource = MALI_STATIC_CAST(mali_ds_resource * ) resource_handle;

	MALI_DEBUG_ASSERT_RESOURCE(resource);
	MALI_DEBUG_ASSERT_CONSUMER(consumer);

	if (MALI_TRUE==resource->shutdown) MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	MALI_DEBUG_ASSERT( MALI_TRUE!=consumer->delete_self, ("Consumer should not be in delete state when being connected\n" ) );

	MALI_DEBUG_ASSERT( (MALI_DS_CONSUMER_STATE_UNUSED==consumer->state) || (MALI_DS_CONSUMER_STATE_PREPARING==consumer->state), ASSERT_TXT);

	same_manager = (resource->manager==consumer->manager);
	/* It is not a legal to have different managers, and indicates that user tries to make dependency between two different Base Contexts */
	MALI_DEBUG_ASSERT(same_manager, ASSERT_TXT);
	if ( ! same_manager )
	{
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}
	manager = resource->manager;
	/* If this is a write request, then all previous elements in the queue must be flushed */

	global_list_manipulation_mutex__grab(manager,tid);

	/* Detecting if we add duplicated Read dependencies to the same consumer, if so do early out */
	if ( MALI_DS_READ == rights )
	{
		mali_ds_connection * iterator_connection;

		/* Loops backwards through all the connections currently connected to the Resource we will connect to.*/
		MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_REVERSE(iterator_connection, &resource->connections_out,  mali_ds_connection, element_on_resource_list)
		{

			if ( MALI_DS_WRITE == iterator_connection->rights) break;
			if ( iterator_connection->connection_consumer == consumer)
			{
				/* Early out: The same connection is already existing */
				global_list_manipulation_mutex__release(manager,tid);
				return MALI_ERR_NO_ERROR;
			}
		}
	}

	/* The algorithm below performs the CopyOnWrite check designed by the GLES driver.
	   If COW should be performed, it calls a Callbackfunction on the Consumer.
	   The callback should then replace the Resource with a new one, and return this
	   as the Resource we will connect to.
	*/
	/* COW only needed when we add a write dependency */
	if ( MALI_DS_WRITE == rights )
	{
		mali_ds_connection * iterator_connection;

		/* Loops backwards through all the connections currently connected to the Resource we will connect to.*/
		MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_REVERSE(iterator_connection, &resource->connections_out,  mali_ds_connection, element_on_resource_list)
		{
			mali_ds_consumer     * iterator_consumer;
			mali_ds_consumer_state iterator_consumer_end_state;
			mali_ds_release_mode   iterator_consumer_release_mode;

			/* The iterator_consumer connected with iterator_connection,
			   is the iterator we check if forces the new consumer to do a Cow or not*/
			iterator_consumer              = iterator_connection->connection_consumer;
			iterator_consumer_end_state    = iterator_consumer->state;
			iterator_consumer_release_mode = iterator_consumer->release_mode;

			/* This connection is already invalid and should not be considered anymore */
			if ( is_a_tmp_consumer(iterator_consumer) )
			{
				continue;
			}

			/* If the previous connection will perform a write, we know that it will be released,
			   since write dependencies are always flushed after they are added and will be released on job finish. */
			if (MALI_DS_WRITE==iterator_connection->rights)
			{
				break; /* Abort the next-iterator_consumer-loop */
			}

			/* The connection must at this point be a Read dependency.
			   If it is flushed (in waiting or activated state) AND will release its read dependencies,
			   we continue to the next iteration (the connetion before this one).
			   If not we must perform the CopyOnWrite to replace the Resource we connect to. */
			switch(iterator_consumer_end_state)
			{
				case MALI_DS_CONSUMER_STATE_WAITING:
					/* Fallthrough - OK */
				case MALI_DS_CONSUMER_STATE_ACTIVATED:
					if( MALI_DS_RELEASE_ALL==iterator_consumer_release_mode)
					{
						continue; /* Jump up to next iterator_consumer in the loop */
					}
					/* Fallthrough */
				default:
					; /* break: We have to do COW */
			} /* switch */

			/* ******** COW prevention : If the READ connection has the same consumer as the WRITE one we are
			                             trying to add, just continue. And we only rely on the READ connection
			                             to become trigged. Scenario is a read-back render. We have to wait for
			                             READ but should neither block nor CoW for the directly following WRITE
			                             of the same consumer. */
			if(iterator_consumer == consumer)
			{
				write_is_trigged = MALI_TRUE;
				continue;
			}

			/* ******** COW ********************* */
			/* Replacing the resource we got as an input parameter to current fuction. */
			resource_handle = consumer->cb_func_replace_resource(resource->owner,consumer->owner);
			resource = MALI_STATIC_CAST(mali_ds_resource * ) resource_handle;
			break; /* Abort the next-iterator_consumer-loop */

		} /*  MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_REVERSE(iterator_connection*/
	} /* if ( MALI_DS_WRITE == rights ) */

	if (NULL==resource)
	{
		err = MALI_ERR_OUT_OF_MEMORY;
		goto exit_function ;
	}

	new_connection = manager_alloc_connection(manager);
	if ( MALI_NO_HANDLE==new_connection )
	{
		err = MALI_ERR_OUT_OF_MEMORY;
		goto exit_function;
	}

	/* Memset is setting the list pointers to Zero, so we do not need to do it manually before adding it to the lists */
	_mali_sys_memset(new_connection, 0, sizeof(*new_connection));

	/* Adding the new connection to the system */
	new_connection->connection_resource = resource;
	new_connection->connection_consumer = consumer;
	_mali_embedded_list_insert_tail(&resource->connections_out, &new_connection->element_on_resource_list);
	_mali_embedded_list_insert_tail(&consumer->connections_linked_list_header , &new_connection->element_on_consumer_list);
	new_connection->rights = rights;
	MALI_DEBUG_CODE( new_connection->magic_number = MALI_DS_CONNECTION_MAGIC_NR;)

	consumer->state = MALI_DS_CONSUMER_STATE_PREPARING;

	/* Decide if it could trigger the new connection right away, or if it is waiting for it to trigger */
	if( MALI_DS_WRITE==rights )
	{
		if(MALI_DS_RESOURCE_STATE_UNUSED == resource->state || write_is_trigged)
		{
			new_connection->is_trigged=MALI_TRUE;
		}
		else
		{
			new_connection->is_trigged=MALI_FALSE;
			consumer->ref_count_untrigged_connections++;
		}
		resource->state = MALI_DS_RESOURCE_STATE_BLOCKED;
	}
	else
	{
		MALI_DEBUG_ASSERT( MALI_DS_READ==rights, ASSERT_TXT );

		if(MALI_DS_RESOURCE_STATE_BLOCKED == resource->state )
		{
			new_connection->is_trigged=MALI_FALSE;
			consumer->ref_count_untrigged_connections++;
		}
		else
		{
			new_connection->is_trigged=MALI_TRUE;
			resource->state = MALI_DS_RESOURCE_STATE_READ;
		}
	}
	resource->ref_count_connections_out++;

	err = MALI_ERR_NO_ERROR;

exit_function:
	global_list_manipulation_mutex__release(manager,tid);

	MALI_ERROR(err);
}

MALI_EXPORT mali_err_code mali_common_ds_connect_and_activate_without_callback(mali_ds_consumer_handle consumer_handle,
                                                                       mali_ds_resource_handle resource_handle,
                                                                       mali_ds_connection_type rights)
{
	mali_ds_resource * resource;
	mali_ds_consumer * consumer;
	mali_ds_connection * connection;
	mali_bool same_manager;
	mali_ds_manager * manager;
	int i=0;
	u32 tid =_mali_sys_thread_get_current();


	consumer = MALI_STATIC_CAST(mali_ds_consumer * ) consumer_handle;
	resource = MALI_STATIC_CAST(mali_ds_resource * ) resource_handle;

	MALI_DEBUG_ASSERT_RESOURCE(resource);
	MALI_DEBUG_ASSERT_CONSUMER(consumer);

	if (MALI_TRUE==resource->shutdown) MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	MALI_DEBUG_ASSERT( MALI_TRUE!=consumer->delete_self, ("Consumer should not be in delete state while being connected\n" ) );

	same_manager = (resource->manager==consumer->manager);
	/* It is not a legal to have dependencies between different base contexts. The mangager is stored in the base context.*/
	MALI_DEBUG_ASSERT(same_manager, ASSERT_TXT);
	if ( ! same_manager )
	{
		MALI_ERROR(MALI_ERR_FUNCTION_FAILED);
	}

	manager = resource->manager;
	connection = manager_alloc_connection(manager);
	if ( MALI_NO_HANDLE==connection)
	{
		MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
	}

	/* Memset is setting the list pointers to Zero, so we do not need to do it manually before adding it to the lists */
	_mali_sys_memset(connection, 0, sizeof(*connection));

	global_list_manipulation_mutex__grab(manager,tid);

	/* We accept this function only for unused resources and consumers*/
	if ( MALI_DS_RESOURCE_STATE_UNUSED != resource->state )
	{
		global_list_manipulation_mutex__release(manager,tid);

		while ( MALI_DS_RESOURCE_STATE_UNUSED != resource->state )
		{
			i++;
			_mali_sys_usleep(1000);
			MALI_DEBUG_ASSERT( i <= 100, ASSERT_TXT );
			if ( i == 100 ) break;
		}
		global_list_manipulation_mutex__grab(manager,tid);
	}

	MALI_DEBUG_ASSERT(MALI_DS_CONSUMER_STATE_UNUSED==consumer->state, ASSERT_TXT);
	MALI_DEBUG_ASSERT(1==consumer->ref_count_untrigged_connections, ASSERT_TXT);

	connection->connection_resource = resource;
	connection->connection_consumer = consumer;

	/* Insert it at the beginning of the list */
	_mali_embedded_list_insert_after(&resource->connections_out, &connection->element_on_resource_list);
	_mali_embedded_list_insert_tail(&consumer->connections_linked_list_header , &connection->element_on_consumer_list);
	connection->rights = rights;
	connection->is_trigged=MALI_TRUE;
	MALI_DEBUG_CODE( connection->magic_number = MALI_DS_CONNECTION_MAGIC_NR;)

	if( MALI_DS_WRITE==rights )
	{
		resource->state = MALI_DS_RESOURCE_STATE_BLOCKED;
	}
	else
	{
		resource->state = MALI_DS_RESOURCE_STATE_READ;
	}

	resource->ref_count_connections_out++;

	/* Setting the consumer to be activated without calling the activation callback */
	consumer->state = MALI_DS_CONSUMER_STATE_ACTIVATED;
	consumer->ref_count_untrigged_connections = 0;

	global_list_manipulation_mutex__release(manager,tid);

	MALI_SUCCESS;
}

MALI_STATIC void mali_ds_fill_connections_free_list(mali_ds_manager * manager, mali_ds_manager_connection_block * new_block)
{
	int i;
	mali_ds_connection * connection = (mali_ds_connection * ) new_block->block_ptr;

	MALI_DEBUG_CODE( _mali_sys_memset(connection, 0 , MALI_DS_NUM_CONNECTIONS_PER_BLOCK * sizeof(mali_ds_connection));)

	for (i = MALI_DS_NUM_CONNECTIONS_PER_BLOCK - 1; i >= 0; --i)
	{
		/* tie them into the free list through the element_on_resource_list member */
		connection[i].element_on_resource_list.next = manager->free_connections_list.next;
 		manager->free_connections_list.next = &connection[i].element_on_resource_list;
	}
}

mali_err_code mali_ds_add_free_connections(mali_ds_manager * manager)
{
	mali_ds_manager_connection_block * new_connection_block;

	new_connection_block = (mali_ds_manager_connection_block * )_mali_sys_malloc(sizeof(mali_ds_manager_connection_block));
	if (NULL == new_connection_block) return MALI_ERR_OUT_OF_MEMORY;

	MALI_DEBUG_CODE( _mali_sys_memset(new_connection_block, 0 , sizeof (mali_ds_manager_connection_block));)

	new_connection_block->block_ptr = _mali_sys_malloc(MALI_DS_NUM_CONNECTIONS_PER_BLOCK * sizeof(mali_ds_connection));
	if (NULL == new_connection_block->block_ptr)
	{
		_mali_sys_free(new_connection_block);
		return MALI_ERR_OUT_OF_MEMORY;
	}

	_mali_embedded_list_insert_after(&manager->blocks_alloced, &new_connection_block->blocks);

	mali_ds_fill_connections_free_list(manager, new_connection_block);

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC void mali_ds_destroy_free_connections(mali_ds_manager * manager)
{
	mali_embedded_list_link * temp;
	mali_ds_manager_connection_block * iterator_connection;

	/* Loops connection blocks and frees them.*/
	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE(iterator_connection, temp, &manager->blocks_alloced,  mali_ds_manager_connection_block, blocks)
	{
		_mali_sys_free( iterator_connection->block_ptr );
		_mali_sys_free( iterator_connection );
	}
	manager->blocks_alloced.next = NULL;
}

/** Function called from the base contex system when the \a mali_base_ctx_handle ctx is being created */
MALI_EXPORT mali_err_code mali_common_dependency_system_open(mali_base_ctx_handle ctx)
{
	mali_ds_manager * manager;
	manager = _mali_sys_malloc(sizeof(*manager));
	MALI_CHECK_NON_NULL(manager, MALI_ERR_OUT_OF_MEMORY);
	manager->ctx = ctx;
	manager->global_list_manipulation_mutex = _mali_sys_mutex_create();
	if ( MALI_NO_HANDLE != manager->global_list_manipulation_mutex )
	{
		MALI_EMBEDDED_LIST_HEAD_INIT_RUNTIME(&manager->blocks_alloced);
		manager->free_connections_list.next = NULL;
		if ( MALI_ERR_NO_ERROR != mali_ds_add_free_connections(manager) )
		{
			_mali_sys_mutex_destroy(manager->global_list_manipulation_mutex);
			_mali_sys_free( manager );
			MALI_ERROR( MALI_ERR_OUT_OF_MEMORY );
		}
		manager->global_list_lock_thread_id = 0;
		manager->global_list_lock_counter = 0;
		MALI_DEBUG_CODE( manager->magic_number = MALI_DS_MANAGER_MAGIC_NR;)
		_mali_base_common_context_set_dependency_system_context(ctx, (struct mali_ds_manager *) manager);
		mali_global_ds_manager = manager;
		MALI_SUCCESS;
	}
	_mali_sys_free( manager );
	MALI_ERROR( MALI_ERR_OUT_OF_MEMORY );
}

/** Function called from the base contex system when the \a mali_base_ctx_handle ctx is shutting down */
MALI_EXPORT void mali_common_dependency_system_close(mali_base_ctx_handle ctx)
{
	mali_ds_manager * manager;
	u32 tid =_mali_sys_thread_get_current();

	manager = get_manager(ctx);
	global_list_manipulation_mutex__grab(manager,tid);
	global_list_manipulation_mutex__release(manager,tid);
	MALI_DEBUG_ASSERT( 0 == manager->global_list_lock_counter,("This thread still holds the lock!") );
	_mali_base_common_context_set_dependency_system_context(ctx, (struct mali_ds_manager *) NULL);

	mali_ds_destroy_free_connections(manager);

	_mali_sys_mutex_destroy(manager->global_list_manipulation_mutex);
	MALI_DEBUG_CODE( _mali_sys_memset(manager,0, sizeof(*manager));)
	_mali_sys_free(manager);
}


#ifndef MALI_DEBUG_SKIP_CODE

/** All the remaining functions in this file is only built in Debug builds */

void mali_common_ds_system_debug_print_consumer(mali_ds_consumer_handle consumer_in)
{
	mali_ds_consumer * consumer;
	u32 tid =_mali_sys_thread_get_current();

	consumer=MALI_STATIC_CAST(mali_ds_consumer * )consumer_in;

	global_list_manipulation_mutex__grab(consumer->manager,tid);
	debug_ds_system_print_consumer(consumer);
	global_list_manipulation_mutex__release(consumer->manager,tid);
}

void mali_common_ds_system_debug_print_resource(mali_ds_resource_handle resource_in)
{
	mali_ds_resource * resource;
	u32 tid =_mali_sys_thread_get_current();

	resource=MALI_STATIC_CAST(mali_ds_resource * )resource_in;

	global_list_manipulation_mutex__grab(resource->manager,tid);
	debug_ds_system_print_resource(resource);
	global_list_manipulation_mutex__release(resource->manager,tid);
}

#endif /* #ifndef MALI_DEBUG_SKIP_CODE */

MALI_STATIC void resource_schedule(mali_ds_resource * resource)
{
    mali_ds_connection * connection_first;
    mali_ds_connection * connection;
	mali_bool end_of_list;
    MALI_DEBUG_ASSERT_RESOURCE(resource);

    /* Debug code: verifying that if there is no connections in the linked list "connection out", the integer ref_count_connections_out is 0*/
    MALI_DEBUG_ASSERT( (MALI_TRUE==_mali_embedded_list_is_empty(&resource->connections_out))==((resource->ref_count_connections_out&0xffff)==0), ASSERT_TXT);

    /* If this resource has connections (cosumers that are depending on it) schedule them */
    if ( ! _mali_embedded_list_is_empty(&resource->connections_out) )
    {
	/* Adding some refcounting when we are in this function to make it safe to call this function recursively on the same resource */
	/* Recursive calling can happen through the connection_trigger() below, since that may call callback outside dependency system */
	resource->ref_count_connections_out+=0x10000;
	resource->state = MALI_DS_RESOURCE_STATE_READ;

	/* Always trigger the first connection in list if it is not triggered */
	connection_first = MALI_EMBEDDED_LIST_GET_CONTAINER(mali_ds_connection, element_on_resource_list, (&resource->connections_out)->next);
	MALI_DEBUG_ASSERT_CONNECTION(connection_first);
	if ( MALI_FALSE == connection_first->is_trigged )
	{
	    connection_trigger(connection_first);
	}

	/* Then trigger all following connections until the first connection with a write request */
	end_of_list = MALI_FALSE;
	while (MALI_FALSE==end_of_list)
	{
		end_of_list = MALI_TRUE;
		MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(connection, &resource->connections_out, mali_ds_connection, element_on_resource_list)
		{
			MALI_DEBUG_ASSERT_CONNECTION(connection);
			if ( MALI_DS_WRITE==connection->rights )
			{
				resource->state=MALI_DS_RESOURCE_STATE_BLOCKED;
				break;
			}
			if ( MALI_FALSE == connection->is_trigged )
			{
				end_of_list = MALI_FALSE;
				break;
			}
		}
		if ( MALI_FALSE ==end_of_list ) connection_trigger(connection);
	}

	resource->ref_count_connections_out-=0x10000;
    }

    /* Releasing resource if no one is using it, and it is in the outer resource_schedule() if it is called recursively */
    if ( resource->ref_count_connections_out == 0 )
    {
	MALI_DEBUG_ASSERT( _mali_embedded_list_is_empty(&resource->connections_out), ASSERT_TXT);
	if ( MALI_TRUE==resource->shutdown)
	{
	    void * owner = resource->owner;
	    mali_ds_cb_func_resource cb_func= resource->cb_on_release;
	    resource->owner=NULL;
	    resource->cb_on_release = NULL;
	    if ( NULL != cb_func )
	    {
		cb_func(resource->manager->ctx, owner);
	    }
	    resource_internal_release(resource);
	}
	else
	{
	    resource->state = MALI_DS_RESOURCE_STATE_UNUSED;
	}
    }
}

MALI_STATIC void connection_trigger(mali_ds_connection * connection)
{
	mali_ds_consumer * consumer;
	MALI_DEBUG_ASSERT(MALI_FALSE==connection->is_trigged, ASSERT_TXT);

	connection->is_trigged = MALI_TRUE;

	consumer= connection->connection_consumer;
	consumer->ref_count_untrigged_connections--;
	if( (0==consumer->ref_count_untrigged_connections) && (MALI_DS_CONSUMER_STATE_WAITING==consumer->state) )
	{
		 consumer_internal_activate(consumer);
	}
}

MALI_STATIC void consumer_internal_activate(mali_ds_consumer * consumer)
{
	mali_ds_manager * manager = consumer->manager;
	mali_ds_wait_element * blocked;

	MALI_DEBUG_ASSERT( (0==consumer->ref_count_untrigged_connections) || (MALI_DS_ERROR==consumer->error_value), ASSERT_TXT );
	MALI_DEBUG_ASSERT( MALI_DS_CONSUMER_STATE_WAITING == consumer->state, ASSERT_TXT);

	consumer->state = MALI_DS_CONSUMER_STATE_ACTIVATED;

	blocked = consumer->blocked_waiter_list;
	consumer->blocked_waiter_list = NULL;

	if ( NULL != consumer->cb_func_activate )
	{
		consumer->cb_func_activate(manager->ctx, consumer->owner, consumer->error_value);
	}
	else if (NULL == blocked)
	{
		/* STATE: There is no Activation callback function AND no one waiting for consumer to be active */
		/* This state is not legal */
		MALI_DEBUG_ASSERT(0==1, ASSERT_TXT);
		internal_release_connections(consumer, MALI_TRUE);
		return;
	}

	/* If there are elements that are waiting on this consumer to be active, here they are released */
	while ( NULL != blocked)
	{
		mali_ds_wait_element * blocked_next;
		blocked_next = blocked->next;

		/* Release current element */
		_mali_base_arch_sys_wait_handle_trigger(blocked->wait_handle);
		_mali_sys_free(blocked);

		/* Do the iteration to next element */
		blocked = blocked_next;
	}
}

MALI_STATIC void consumer_internal_release(mali_ds_consumer * consumer)
{
	consumer->owner=NULL;
	MALI_DEBUG_ASSERT( _mali_embedded_list_is_empty( &consumer->connections_linked_list_header ) == MALI_TRUE, ("Consumer still has connections while being deleted.\n" ) );
	MALI_DEBUG_CODE(_mali_sys_memset(consumer, 0, sizeof(mali_ds_consumer));)
	_mali_sys_free(consumer);
}

MALI_STATIC void resource_internal_release(mali_ds_resource * resource)
{
	MALI_DEBUG_CODE(_mali_sys_memset(resource, 0, sizeof(mali_ds_resource));)
	_mali_sys_free(resource);
}

MALI_STATIC mali_ds_connection * manager_alloc_connection(mali_ds_manager * manager)
{
	mali_ds_connection * connection;

	MALI_DEBUG_ASSERT_MANAGER(manager);

	if ( NULL != manager->free_connections_list.next )
	{
		connection = MALI_EMBEDDED_LIST_GET_CONTAINER(mali_ds_connection, element_on_resource_list, manager->free_connections_list.next);

		/* We don't need the list to be doubly linked. */
		manager->free_connections_list.next = connection->element_on_resource_list.next;
		MALI_DEBUG_CODE(connection->element_on_resource_list.next = connection->element_on_resource_list.prev = NULL;)
		return connection ;
	}
	if ( MALI_ERR_NO_ERROR != mali_ds_add_free_connections( manager))
	{
		return NULL;
	}

	MALI_DEBUG_ASSERT(manager->free_connections_list.next != NULL, ("free_ds_connections list has NULL ptr"));
	connection = MALI_EMBEDDED_LIST_GET_CONTAINER(mali_ds_connection, element_on_resource_list, manager->free_connections_list.next);

	/* We don't need the list to be doubly linked. */
	manager->free_connections_list.next = connection->element_on_resource_list.next;
	MALI_DEBUG_CODE(connection->element_on_resource_list.next = connection->element_on_resource_list.prev = NULL;)
	return connection ;
}

MALI_STATIC void manager_release_connection(mali_ds_manager * manager, mali_ds_connection * connection)
{
	MALI_DEBUG_ASSERT_MANAGER(manager);

	MALI_DEBUG_CODE( _mali_sys_memset(connection, 0 , sizeof(mali_ds_connection));)

	/* We don't need the list to be doubly linked. */
	connection->element_on_resource_list.next = manager->free_connections_list.next;
	manager->free_connections_list.next = &connection->element_on_resource_list;
}

/**
 * Note: this function appears to race global_list_manipulation_mutex__release
 * for access to global_list_lock_thread_id. Helgrind generates a warning
 * (unless it is suppressed), but this is a false positive.
 */
 MALI_STATIC void global_list_manipulation_mutex__grab(mali_ds_manager * manager, u32 tid)
{
	MALI_DEBUG_ASSERT_MANAGER(manager);

	if (  tid != manager->global_list_lock_thread_id )
	{
		_mali_sys_mutex_lock(manager->global_list_manipulation_mutex);
		manager->global_list_lock_thread_id = tid;
		MALI_DEBUG_ASSERT( 0 == manager->global_list_lock_counter, ("Locking scheme broken!") );
	}
	++manager->global_list_lock_counter;
}

MALI_STATIC void global_list_manipulation_mutex__release(mali_ds_manager * manager, u32 tid)
{
	MALI_DEBUG_ASSERT_MANAGER(manager);
	MALI_DEBUG_ASSERT( tid == manager->global_list_lock_thread_id,("Call release only from the same thread which locked!") );
	MALI_DEBUG_ASSERT( 0 < manager->global_list_lock_counter, ("Locking scheme broken!") );

	--manager->global_list_lock_counter;

	if ( 0 == manager->global_list_lock_counter )
	{
		manager->global_list_lock_thread_id = 0;
		_mali_sys_mutex_unlock( manager->global_list_manipulation_mutex );
	}
}

/** Rest of the file is DEBUG CODE, helping functions to print out the current state of the Dependency system. */
#ifndef MALI_DEBUG_SKIP_CODE

/* In debug buils this function can be called directly from GDB during a breakpoint */
MALI_STATIC void debug_ds_system_print_resource(mali_ds_resource * resource)
{
	internal_debug_ds_system_print_resource(resource, 0);
}

/* In debug buils this function can be called directly from GDB during a breakpoint */
MALI_STATIC void debug_ds_system_print_consumer(mali_ds_consumer * consumer)
{
	internal_debug_ds_system_print_consumer(consumer, 0);
}

static const char* internal_get_consumer_state_string(mali_ds_consumer *consumer)
{
	switch(consumer->state)
	{
		case MALI_DS_CONSUMER_STATE_UNUSED:
			return "Unused";
		case MALI_DS_CONSUMER_STATE_PREPARING:
			return "Preparing";
		case MALI_DS_CONSUMER_STATE_WAITING:
			return "Waiting";
		case MALI_DS_CONSUMER_STATE_ACTIVATED:
			return "Activated";
		default:
			return "Illegal state";
	}
}

/* magic string to use for indention - 32 spaces. After 16 levels we will see the ~ indicating
   we were out of characters. Add more or just live with the extreme case. */
static const char indention_string[] = "                                ~";
static const int  indention_scale    = 2;

MALI_STATIC void internal_debug_ds_system_print_resource(mali_ds_resource * resource, int indent_level)
{
	MALI_DEBUG_ASSERT_RESOURCE(resource);

	_mali_sys_printf("%.*sResource 0x%X Owner 0x%X\n",indention_scale*indent_level,indention_string,
	                                                  resource,
	                                                  resource->owner);
	{
		mali_ds_connection * connection_resource;
		mali_ds_consumer * consumer_next_level;
		MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(connection_resource, &resource->connections_out, mali_ds_connection, element_on_resource_list)
		{
			MALI_DEBUG_ASSERT_CONNECTION(connection_resource);
			consumer_next_level = connection_resource->connection_consumer;
			internal_debug_ds_system_print_consumer(consumer_next_level, indent_level+1);
		}
	}
}

MALI_STATIC void internal_debug_ds_system_print_consumer(mali_ds_consumer * consumer, int indent_level)
{
	mali_ds_connection * connection;
	MALI_DEBUG_ASSERT_CONSUMER(consumer);

	if ( is_a_tmp_consumer(consumer) )
	{
		_mali_sys_printf("%.*sTemp.Consumer 0x%X %s\n", indention_scale*indent_level,indention_string,
		                                                consumer,
		                                                internal_get_consumer_state_string(consumer));
	}
	else
	{
		_mali_sys_printf("%.*sConsumer 0x%X %s Owner 0x%X\n", indention_scale*indent_level,indention_string,
		                                                      consumer,
		                                                      internal_get_consumer_state_string(consumer),
		                                                      consumer->owner);
	}

	indent_level++;

	MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(connection,&consumer->connections_linked_list_header, mali_ds_connection, element_on_consumer_list)
	{
		mali_ds_resource * resource;
		MALI_DEBUG_ASSERT_CONNECTION(connection);
		resource = connection->connection_resource;
		MALI_DEBUG_ASSERT_RESOURCE(resource);

		/* Print all in one call. On Symbian we will otherwise see line breaks inserted between every print out, which
		   makes reading the log rather difficult. */
		_mali_sys_printf("%.*sResource 0x%X %s %s Owner 0x%X\n", indention_scale*indent_level,indention_string,
		                                                         resource,
		                                                         MALI_TRUE==connection->is_trigged?"Trigged":"Blocked",
															     MALI_DS_WRITE==connection->rights?"Write":"Read",
															     resource->owner);

		if (MALI_TRUE!=connection->is_trigged && (indent_level<6))
		{
			mali_ds_connection * connection_resource;
			mali_ds_consumer   * consumer_next_level;
			MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(connection_resource, &resource->connections_out, mali_ds_connection, element_on_resource_list)
			{
				MALI_DEBUG_ASSERT_CONNECTION(connection_resource);
				consumer_next_level = connection_resource->connection_consumer;
				if ( consumer_next_level!=consumer)
				{
					internal_debug_ds_system_print_consumer(consumer_next_level, indent_level+1);
				}
				else
				{
					_mali_sys_printf("%.*sConsumer Parent\n",indention_scale*(indent_level+1),indention_string);
				}
			}
		}
	}
}

#endif /* #ifndef MALI_DEBUG_SKIP_CODE*/

