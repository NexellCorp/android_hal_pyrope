/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"

#include "backend/abstract_scheduler.h"
#include "backend/extra_info.h"


static memerr make_op_available(scheduler_context *ctx, node *op) {
	ESSL_CHECK(_essl_ptrset_insert(&ctx->available, op));
	return MEM_OK;
}

typedef enum {
	DATA_DEPENDENCY_USE,
	CONTROL_DEPENDENCY_USE,
	CONTROL_BLOCK_DEPENDENCY
} dependency_use_kind;

/**
   if subcycle is -1, the use isn't counted in earliest/latest use 
*/
static void update_earliest_latest_use(node_extra *info, int subcycle, int extra_delay)
{
	if(subcycle != -1)
	{
		info->earliest_use = ESSL_MAX(info->earliest_use, subcycle+extra_delay);
		info->latest_use = ESSL_MIN(info->latest_use, subcycle);
	}
}


static memerr update_dominator_consider_for_available(scheduler_context *ctx, node *op, basic_block *block, dependency_use_kind use_kind)
{
	/* Update use dominator */
	node_extra *info;

	basic_block *common_dominator = NULL;

	info = EXTRA_INFO(op);
	if(use_kind != CONTROL_DEPENDENCY_USE)
	{
		if (_essl_ptrdict_has_key(&ctx->use_dominator, op)) {
			basic_block *dominator_block = _essl_ptrdict_lookup(&ctx->use_dominator, op);
			common_dominator = _essl_common_dominator(block, dominator_block);
		} else {
			common_dominator = block;
		}
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->use_dominator, op, common_dominator));
	} else {
		common_dominator = _essl_ptrdict_lookup(&ctx->use_dominator, op);
	}

	if (op->hdr.kind == EXPR_KIND_PHI) {
		/* Phi nodes are not put on the available list or counted as register pressure */
	} else {
		
		if (info->unscheduled_use_count == 0) {
			assert(common_dominator != 0);

			/* it's not partially scheduled anymore */
			_essl_ptrset_remove(&ctx->partially_scheduled_data_uses, op);


			/* Make operation available in this or a later block */
			if (common_dominator == ctx->current_block) {
				ESSL_CHECK(make_op_available(ctx, op));
			} else {
				ESSL_CHECK(_essl_ptrset_insert(&common_dominator->ready_operations, op));
			}
		} else {
			if(use_kind == DATA_DEPENDENCY_USE && op->expr.best_block == block)
			{
				/* This operation is partially scheduled, and might just end up in the current block.
				   Chuck it onto the partially scheduled list so we count it for register pressure */
				ESSL_CHECK(_essl_ptrset_insert(&ctx->partially_scheduled_data_uses, op));
			}

		}
	}

	return MEM_OK;
}



static memerr mark_use_of_op(scheduler_context *ctx, node *op, basic_block *block, int subcycle, int extra_delay, dependency_use_kind use_kind) {
	node_extra *info;

	info = EXTRA_INFO(op);
	assert(info != NULL);
	update_earliest_latest_use(info, subcycle, extra_delay);

	assert(info->scheduled_use_count >= 0);
	assert(info->unscheduled_use_count > 0);
	++info->scheduled_use_count;
	--info->unscheduled_use_count;
	return update_dominator_consider_for_available(ctx, op, block, use_kind);
}

static memerr mark_phi_node_uses(scheduler_context *ctx, basic_block *phi_block) {
	phi_list *phi;
	for (phi = phi_block->phi_nodes ; phi != 0 ; phi = phi->next) {
		phi_source *ps;
		for (ps = phi->phi_node->expr.u.phi.sources ; ps != 0 ; ps = ps->next) {
			if(ps->join_block == ctx->current_block)
			{
				/* -999 to make sure that latest_use is outside of the current block */
				int extra_delay = 0;
				if(ctx->phi_source_dependency_callback)
				{
					extra_delay = ctx->phi_source_dependency_callback(ctx->user_ptr, phi->phi_node, ps);
				}
				ESSL_CHECK(mark_use_of_op(ctx, ps->source, ps->join_block, -999, +999+extra_delay, DATA_DEPENDENCY_USE));
			}
		}
	}
	return MEM_OK;
}

memerr _essl_scheduler_init(scheduler_context *ctx, mempool *pool, control_flow_graph *cfg,
							operation_priority_fun operation_priority, void *user_ptr) {
	unsigned i;

	ctx->pool = pool;
	ctx->cfg = cfg;
	ctx->operation_priority = operation_priority;
	ctx->data_dependency_callback = 0;
	ctx->control_dependency_callback = 0;
	ctx->phi_source_dependency_callback = 0;
	ctx->user_ptr = user_ptr;

	ctx->current_block = 0;
	ctx->active_operation = 0;
	ctx->next_block_index = (int)cfg->n_blocks;
	ESSL_CHECK(_essl_ptrset_init(&ctx->available, pool));
	ESSL_CHECK(_essl_ptrset_init(&ctx->partially_scheduled_data_uses, pool));
	ESSL_CHECK(_essl_ptrdict_init(&ctx->use_dominator, pool));
	ESSL_CHECK(_essl_ptrdict_init(&ctx->use_count, pool));

	/* Init ready_operations maps */
	for (i = 0 ; i < cfg->n_blocks ; i++) {
		basic_block *b = cfg->output_sequence[i];
		ESSL_CHECK(_essl_ptrset_init(&b->ready_operations, pool));
	}

	return MEM_OK;
}

int _essl_scheduler_more_blocks(scheduler_context *ctx) {
	return ctx->next_block_index > 0;
}

basic_block *_essl_scheduler_begin_block(scheduler_context *ctx, int subcycle) {
	int extra_delay;
	unsigned i;
	assert(ctx->current_block == 0);
	assert(_essl_scheduler_more_blocks(ctx));
	
	_essl_ptrset_clear(&ctx->partially_scheduled_data_uses);

	ctx->current_block = ctx->cfg->output_sequence[--ctx->next_block_index];

	/* Mark a use of the source for the block termination and successor block phi nodes */
	for(i = 0; i < ctx->current_block->n_successors; ++i)
	{
		ESSL_CHECK(mark_phi_node_uses(ctx, ctx->current_block->successors[i]));		
	}

	if (ctx->current_block->source != NULL) {
		extra_delay = 0;
		if(ctx->data_dependency_callback) extra_delay = ctx->data_dependency_callback(0, ctx->current_block->source);
		ESSL_CHECK(mark_use_of_op(ctx, ctx->current_block->source, ctx->current_block, subcycle, extra_delay, DATA_DEPENDENCY_USE));
	}

	if(ctx->current_block->termination == TERM_KIND_EXIT && ctx->current_block->source != NULL)
	{
			node_extra *info;
			info = EXTRA_INFO(ctx->current_block->source);
			info->latest_use = -999;
			info->earliest_use = -999;
	}

	/* Record control dependence and mark one use of all control-dependent operations */
	{
		control_dependent_operation *cd_op;
		int n_cdo = 0;
		for (cd_op = ctx->current_block->control_dependent_ops ; cd_op != 0 ; cd_op = cd_op->next) {
/*			extra_delay = 0;
			if(ctx->control_dependency_callback) extra_delay = ctx->control_dependency_callback(ctx->user_ptr, 0, cd_op->op, subcycle); */
			ESSL_CHECK(mark_use_of_op(ctx, cd_op->op, ctx->current_block, -1, 0, CONTROL_BLOCK_DEPENDENCY));
			n_cdo++;
		}
		ctx->remaining_control_dependent_ops = n_cdo;
	}

	/* Make all ready operations available */
	{
		ptrset_iter it;
		node *op;
		_essl_ptrset_iter_init(&it, &ctx->current_block->ready_operations);
		while ((op = _essl_ptrset_next(&it)) != 0) {
			ESSL_CHECK(make_op_available(ctx, op));
		}
		ESSL_CHECK(_essl_ptrset_clear(&ctx->current_block->ready_operations));
	}

	return ctx->current_block;
}

#ifndef NDEBUG
static int no_available_control_dependent_ops(scheduler_context *ctx) {
	control_dependent_operation *cd_op;
	for (cd_op = ctx->current_block->control_dependent_ops ; cd_op != 0 ; cd_op = cd_op->next) {
		if (_essl_ptrset_has(&ctx->available, cd_op->op)) {
			return 0;
		}
	}
	return 1;
}
#endif

memerr _essl_scheduler_finish_block(scheduler_context *ctx) {
	assert(ctx->current_block != 0);
	assert(ctx->active_operation == 0);
	assert(no_available_control_dependent_ops(ctx));

	/* Postpone all available operations */
	while (_essl_scheduler_more_operations(ctx)) {
		node *op = _essl_scheduler_next_operation(ctx);
		ESSL_CHECK(_essl_scheduler_postpone_operation(ctx, op));
	}
	ctx->current_block = 0;
	return MEM_OK;
}

essl_bool _essl_scheduler_is_operation_partially_scheduled_estimate(scheduler_context *ctx, node *op)
{
	return _essl_ptrset_has(&ctx->partially_scheduled_data_uses, op);

}

static int pressure_for_op_def(node *op)
{
	unsigned n_live = 0;
	if(op->hdr.kind == EXPR_KIND_STORE)
	{
		return 0;
	}
	if(op->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE || (op->hdr.kind == EXPR_KIND_BINARY && op->expr.operation == EXPR_OP_INDEX) || (op->hdr.kind == EXPR_KIND_UNARY && op->expr.operation == EXPR_OP_MEMBER))
	{
		return 1;
	}

	if(op->hdr.live_mask == 0)
	{
		n_live = GET_NODE_VEC_SIZE(op);
	} else {
		/* count number of bits in live mask */
		unsigned live = op->hdr.live_mask;
		while(live != 0)
		{
			++n_live;
			live = live & (live - 1); /* clear MSB */
		}
	}
	return n_live;
}

static int get_register_pressure_estimate(scheduler_context *ctx)
{
	int register_pressure = 0;
	ptrset_iter it;
	node *op;

	_essl_ptrset_iter_init(&it, &ctx->partially_scheduled_data_uses);
	while( (op = _essl_ptrset_next(&it)) != NULL)
	{
		node_extra *ex;
		ex = EXTRA_INFO(op);
		assert(ex != NULL);
		if(ex->scheduled_use_count == 0)
		{
			_essl_ptrset_remove(&ctx->partially_scheduled_data_uses, op);
		} else {
			register_pressure += pressure_for_op_def(op);
		}
	}

	_essl_ptrset_iter_init(&it, &ctx->available);
	while( (op = _essl_ptrset_next(&it)) != NULL)
	{
		node_extra *ex;
		ex = EXTRA_INFO(op);
		assert(ex != NULL);
		assert(ex->unscheduled_use_count == 0);
		if(ex->scheduled_use_count == 0)
		{
			_essl_ptrset_remove(&ctx->available, op);
		} else {
			register_pressure += pressure_for_op_def(op);
		}
	}

	/*printf("register pressure: %d\n", register_pressure); */
	return register_pressure;
}


node *_essl_scheduler_next_operation(scheduler_context *ctx) {
	node *best_op = NULL;
	node *op;
	int best_priority = -2000000000;
	ptrset_iter it;
	int register_pressure;
	assert(ctx->current_block != 0);
	assert(ctx->active_operation == 0);
	assert(_essl_scheduler_more_operations(ctx));


	register_pressure = get_register_pressure_estimate(ctx);

	_essl_ptrset_iter_init(&it, &ctx->available);
	while( (op = _essl_ptrset_next(&it)) != NULL)
	{
		node_extra *ex;
		ex = EXTRA_INFO(op);
		assert(ex != NULL);
		assert(ex->unscheduled_use_count == 0);
		if(ex->scheduled_use_count == 0)
		{
			_essl_ptrset_remove(&ctx->available, op);
		} else {
			int pri = ctx->operation_priority(op, register_pressure);
			if(pri > best_priority)
			{
				best_op = op;
				best_priority = pri;
			}

		}
	}
	assert(best_op != NULL);
	_essl_ptrset_remove(&ctx->available, best_op);

	ctx->active_operation = best_op;

	assert(ctx->active_operation->expr.earliest_block == NULL || ctx->active_operation->expr.latest_block == NULL || _essl_common_dominator(ctx->active_operation->expr.earliest_block, ctx->active_operation->expr.latest_block) == ctx->active_operation->expr.earliest_block);
	return ctx->active_operation;
}

essl_bool _essl_scheduler_more_operations(scheduler_context *ctx) {
	node *op;
	node_extra *ex;
	ptrset_iter it;
	assert(ctx->current_block != 0);
	assert(ctx->active_operation == 0);

	_essl_ptrset_iter_init(&it, &ctx->available);
	while( (op = _essl_ptrset_next(&it)) != NULL)
	{
		ex = EXTRA_INFO(op);
		assert(ex != NULL);
		assert(ex->unscheduled_use_count == 0);
		if(ex->scheduled_use_count == 0)
		{
			_essl_ptrset_remove(&ctx->available, op);
		} else {
			/* found op that needs scheduling */
			return ESSL_TRUE;
		}

	}
	return ESSL_FALSE;
}




int _essl_scheduler_get_earliest_use(node *op)
{
	node_extra *ei = EXTRA_INFO(op);
	return ei->earliest_use;
}


int _essl_scheduler_get_latest_use(node *op)
{
	node_extra *ei = EXTRA_INFO(op);
	return ei->latest_use;
}

/** See if the use node is the only node pointing to source. use must be schedulable for this to work. */
essl_bool _essl_scheduler_is_only_use_of_source(node *use, node *source)
{
	node_extra *source_ex;
	IGNORE_PARAM(use);
	assert(EXTRA_INFO(use)->unscheduled_use_count == 0);
	source_ex = EXTRA_INFO(source);
	assert(source_ex->unscheduled_use_count + source_ex->scheduled_use_count > 0);
	return source_ex->unscheduled_use_count + source_ex->scheduled_use_count == 1;
}


memerr _essl_scheduler_schedule_operation(scheduler_context *ctx, node *operation, int subcycle)
{

	assert(ctx->current_block != 0);
	assert(operation == ctx->active_operation);
	assert(EXTRA_INFO(operation) != NULL);

	/* printf("%3d/%d/%d Scheduled: %s\n", ctx->current_block->output_visit_number, subcycle, subcycle/4, _essl_label_for_node(ctx->pool, 0, operation)); */
	

	/* Mark operation as a scheduled use for all operations it depends on. */
	{
		unsigned i;
		for (i = 0 ; i < GET_N_CHILDREN(operation) ; i++) {
			node *child = GET_CHILD(operation, i);
			if(child)
			{
				int extra_delay = 0;
				if(ctx->data_dependency_callback) extra_delay = ctx->data_dependency_callback( operation, child);
				ESSL_CHECK(mark_use_of_op(ctx, child, ctx->current_block, subcycle, extra_delay, DATA_DEPENDENCY_USE));
			}
		}
		if (operation->hdr.is_control_dependent) {
			control_dependent_operation *cd_op;
			operation_dependency *dep;
			cd_op = _essl_ptrdict_lookup(&ctx->cfg->control_dependence, operation);
			for (dep = cd_op->dependencies ; dep != 0 ; dep = dep->next) {
				int extra_delay = 0;
				if(ctx->control_dependency_callback) extra_delay = ctx->control_dependency_callback(operation, dep->dependent_on->op); 
				ESSL_CHECK(mark_use_of_op(ctx, dep->dependent_on->op, ctx->current_block, subcycle, extra_delay, CONTROL_DEPENDENCY_USE));
			}
		}
	}

	/** Node has been scheduled. Its use dominator is no longer needed. */
	_essl_ptrdict_remove(&ctx->use_dominator, operation);

	/** If the node is control-dependent, decrease counter */
	if (operation->hdr.is_control_dependent) {
		ctx->remaining_control_dependent_ops--;
	}

	ctx->active_operation = 0;

	return MEM_OK;
}

memerr _essl_scheduler_schedule_extra_operation(scheduler_context *ctx, node **operation_ptr, int subcycle)
{
	node *operation = *operation_ptr;
	assert(ctx->current_block != 0);
	assert(!operation->hdr.is_control_dependent);
	/*
	printf("%3d Extra:     %s\n", ctx->current_block->output_visit_number, _essl_label_for_node(ctx->pool, 0, operation));
	*/

	{
		node_extra *info = EXTRA_INFO(operation);

		if (info->unscheduled_use_count + info->scheduled_use_count > 1) {
			/* Operation result is used more than once or has already been scheduled. Clone it and return the clone. */
			node *clone; size_t i;
			node_extra *clone_info;
			ESSL_CHECK(clone = _essl_clone_node(ctx->pool, operation));
			*operation_ptr = clone;
			ESSL_CHECK(clone_info = CREATE_EXTRA_INFO(ctx->pool, clone));
			clone_info->unscheduled_use_count = 1;
			clone_info->scheduled_use_count = 0;
			clone_info->address_symbols = info->address_symbols;
			clone_info->address_offset = info->address_offset;
			clone_info->address_multiplier = info->address_multiplier;
			clone_info->is_indexed = info->is_indexed;
			for(i = 0; i < GET_N_CHILDREN(clone); ++i)
			{
				node *child = GET_CHILD(clone, i);
				if(child)
				{
					node_extra *cinfo = EXTRA_INFO(child);
					++cinfo->unscheduled_use_count;
				}
			}

			assert(ctx->active_operation == 0);
			--info->scheduled_use_count;

			ctx->active_operation = clone;
			ESSL_CHECK(_essl_scheduler_schedule_operation(ctx, clone, subcycle));
			assert(clone_info->scheduled_use_count == 0);
		} else {

			assert(ctx->active_operation == 0);
			--info->scheduled_use_count;
			assert(info->scheduled_use_count >= 0);
			ctx->active_operation = operation;
			ESSL_CHECK(_essl_scheduler_schedule_operation(ctx, operation, subcycle));
		}

		return MEM_OK;
	}
}

memerr _essl_scheduler_forget_unscheduled_use(scheduler_context *ctx, node *operation)
{
	node_extra *info = EXTRA_INFO(operation);
	--info->unscheduled_use_count;
	ESSL_CHECK(update_dominator_consider_for_available(ctx, operation, ctx->current_block, DATA_DEPENDENCY_USE));
	return MEM_OK;
}

memerr _essl_scheduler_add_scheduled_use(node *operation)
{
	node_extra *info = EXTRA_INFO(operation);
	ESSL_CHECK(info);
	++info->scheduled_use_count;
	return MEM_OK;
}


memerr _essl_scheduler_postpone_operation(scheduler_context *ctx, node *operation) {
	basic_block *dominator_block;
	basic_block *ready_block;
	int bi;

	assert(ctx->current_block != 0);
	assert(ctx->current_block->immediate_dominator != 0);
	assert(ctx->active_operation == operation);
	assert(_essl_ptrdict_has_key(&ctx->use_dominator, operation));

	/* Find next-to-be-scheduled block which dominates all blocks where
	   uses of the operation has been scheduled. */
	dominator_block = _essl_ptrdict_lookup(&ctx->use_dominator, operation);
	dominator_block = _essl_common_dominator(dominator_block, ctx->current_block->immediate_dominator);
	/*
	printf("%3d Postponed: %s (to %d imm %d)\n", ctx->current_block->output_visit_number, _essl_label_for_node(ctx->pool, 0, operation),
		   dominator_block->output_visit_number, ctx->current_block->immediate_dominator->output_visit_number);
	*/
	ready_block = 0;
	for (bi = ctx->next_block_index - 1; bi >= 0 ; bi--) {
		basic_block *b = ctx->cfg->output_sequence[bi];
		if (_essl_common_dominator(dominator_block, b) == b) {
			ready_block = b;
			break;
		}
	}
	assert(ready_block != 0);

	/* Insert the operation into the ready list for this block */
	ESSL_CHECK(_essl_ptrset_insert(&ready_block->ready_operations, operation));
	ctx->active_operation = 0;
	return MEM_OK;
}

essl_bool _essl_scheduler_block_complete(scheduler_context *ctx) {
	assert(ctx->current_block != 0);
	assert(ctx->active_operation == 0);

	return ctx->remaining_control_dependent_ops == 0;
}

void _essl_scheduler_set_data_dependency_delay_callback(scheduler_context *ctx, data_dependency_fun callback)
{
	ctx->data_dependency_callback = callback;
}

void _essl_scheduler_set_control_dependency_delay_callback(scheduler_context *ctx, control_dependency_fun callback)
{
	ctx->control_dependency_callback = callback;
}

void _essl_scheduler_set_phi_source_dependency_delay_callback(scheduler_context *ctx, phi_source_dependency_fun callback)
{
	ctx->phi_source_dependency_callback = callback;
}
