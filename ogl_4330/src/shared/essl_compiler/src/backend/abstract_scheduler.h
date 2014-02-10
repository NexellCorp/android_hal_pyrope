/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BACKEND_SCHEDULER_H
#define BACKEND_SCHEDULER_H

#include "common/essl_node.h"
#include "common/essl_mem.h"
#include "common/ptrdict.h"
#include "common/ptrset.h"
#include "common/basic_block.h"

/*
  The scheduling of a function body proceeds like this:

while(more_blocks())
  block = begin_block()
  << initialize scheduling of block >>
  << schedule block termination >>
  while (<< any operation 'top' to schedule along with termination >>)
    top = schedule_extra_operation(top)
    << schedule 'top' >>
  while (!block_complete())
    op = next_operation()
    << schedule 'op' >>
    schedule_operation(op)
    while (<< any descendents of 'op' to schedule along with it >>)
      dop = << descendent to schedule >>
      dop = schedule_extra_operation(dop)
      << schedule 'dop' >>
  while (<< potentially free space for more ops >> && more_operations())
    op = next_operation()
    if (<< we want to schedule 'op' in this block >>)
      << schedule 'op' >>
      schedule_operation(op)
      while (<< any descendents of 'op' to schedule along with it >>)
        dop = << descendent to schedule >>
        dop = schedule_extra_operation(dop)
        << schedule 'dop' >>
    else
      postpone_operation(op)
  << finalize scheduling of block >>
  finish_block()

*/

typedef struct _tag_scheduler_context scheduler_context;

typedef int (*operation_priority_fun)(node *n, int current_register_pressure);

typedef int (*data_dependency_fun)(/*@null@*/ node *n, node *dependency);
typedef int (*control_dependency_fun)(/*@null@*/ node *n, node *dependency);
typedef int (*phi_source_dependency_fun)(void *user_ptr, node *phi, phi_source *dependency);


struct _tag_scheduler_context {
	mempool *pool; /**< The memory pool to use for allocations */
	control_flow_graph *cfg; /**< The control flow graph that we are operating on */
	operation_priority_fun operation_priority; /**< Callback to determine operation priorities */

	basic_block *current_block; /**< The current block being scheduled */
	node *active_operation; /**< Operation just returned by next_operation */
	int next_block_index; /**< The index in the output sequence of the next block to be scheduled */
	ptrset available; /**< The operations available for scheduling */
	ptrset partially_scheduled_data_uses; /**< The operations that have some scheduled data uses, but are not available for scheduling yet */
	int remaining_control_dependent_ops; /**< Number of control dependent ops yet to be scheduled in this block before it is complete */

	ptrdict use_dominator; /**< Map from node to block indicating the dominator block for the uses of this node */
	ptrdict use_count; /**< Map from node to int indicating the number of uses of this node */

	/*@null@*/ data_dependency_fun data_dependency_callback;
	/*@null@*/ control_dependency_fun control_dependency_callback;
	/*@null@*/ phi_source_dependency_fun phi_source_dependency_callback;
	void *user_ptr;

};


/* --- Scheduler state change functions --- */

/** Ready the scheduler for scheduling instructions for a particular function */
memerr _essl_scheduler_init(scheduler_context *ctx, mempool *pool, control_flow_graph *cfg,
							operation_priority_fun operation_priority, void *user_ptr);

/** returns whether there are any more blocks to schedule */
int _essl_scheduler_more_blocks(scheduler_context *ctx);

/** Start scheduling the next block. Returns the new current block,
	or null on out-of-memory */
basic_block *_essl_scheduler_begin_block(scheduler_context *ctx, int subcycle);

/**
   Finish scheduling the current block.
   All control-dependent operations and phi nodes for this block must have been scheduled.
*/
memerr _essl_scheduler_finish_block(scheduler_context *ctx);

/** Returns the highest prioritized available operation to be scheduled */
node *_essl_scheduler_next_operation(scheduler_context *ctx);

/**
   Signals that this operation has been scheduled.

   The operation must have been just returned from next_operation.
*/
memerr _essl_scheduler_schedule_operation(scheduler_context *ctx, node *operation, int subcycle);

/**
   Signals that an operation will be scheduled along with the just scheduled one.

   If the operation has more than one use, the operation is split so that it will
   be scheduled for the other uses as well.

   parent_operation  Parent operation (used to determine if this is a
   chained schedule_extra). Can be NULL, and this isn't a chained
   schedule_extra in that case

   operation  Operation to schedule. Should be a double pointer to the
   only use of the operation, as schedule_extra might rewrite that
   use.

*/
memerr _essl_scheduler_schedule_extra_operation(scheduler_context *ctx, node **operation_ptr, int subcycle);

/**
   Signals that one unscheduled use of operation from parent_operation will be removed
*/
memerr _essl_scheduler_forget_unscheduled_use(scheduler_context *ctx, node *operation);

/**
   Signals that one scheduled use of operation from parent_operation will be used once more.
*/
memerr _essl_scheduler_add_scheduled_use(node *operation);

/**
   Signals that this operation (which must have been just returned by next_op)
   will not be scheduled in the current block.
*/
memerr _essl_scheduler_postpone_operation(scheduler_context *ctx, node *operation);


/* --- Scheduler state query functions --- */

/** Returns whether all operations that must be scheduled in the current block have been scheduled. */
essl_bool _essl_scheduler_block_complete(scheduler_context *ctx);

/** Returns whether there are any more operations that can be scheduled in the current block. */
essl_bool _essl_scheduler_more_operations(scheduler_context *ctx);


/** Returns the subcycle of the earliest use of a given operation */
int _essl_scheduler_get_earliest_use(node *op);

/** Returns the subcycle of the latest use of a given operation */
int _essl_scheduler_get_latest_use(node *op);

/** See if the use node is the only node pointing to source. use must be schedulable for this to work. */
essl_bool _essl_scheduler_is_only_use_of_source(node *use, node *source);


/** See if an operation has been partially scheduled. Used to see whether scheduling a use of an operation would cause increased register pressure */
essl_bool _essl_scheduler_is_operation_partially_scheduled_estimate(scheduler_context *ctx, node *op);


/* --- Delay callback functions --- */

/** Set callback for additional delays for a data dependency */
void _essl_scheduler_set_data_dependency_delay_callback(scheduler_context *ctx, data_dependency_fun callback);

/** Set callback for additional delays for a control dependency */
void _essl_scheduler_set_control_dependency_delay_callback(scheduler_context *ctx, control_dependency_fun callback);

/** Set callback for additional delays for a phi node->phi source dependency */
void _essl_scheduler_set_phi_source_dependency_delay_callback(scheduler_context *ctx, phi_source_dependency_fun callback);

#endif
