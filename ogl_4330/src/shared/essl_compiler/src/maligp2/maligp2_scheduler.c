/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "common/essl_system.h"
#include "backend/abstract_scheduler.h"
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_scheduler.h"
#include "maligp2/maligp2_slot.h"
#include "backend/extra_info.h"
#include "common/essl_random.h"
#include "common/essl_node.h"


static int maligp2_op_weight(node *op, int adv_fun_cost)
{
#ifdef RANDOMIZED_COMPILATION
	return _essl_get_random_int(0, 100);
#else
 	switch (op->hdr.kind) {
	case EXPR_KIND_PHI:
	case EXPR_KIND_DONT_CARE:
	case EXPR_KIND_TRANSFER:
	case EXPR_KIND_DEPEND:
	case EXPR_KIND_MALIGP2_STORE_CONSTRUCTOR:
		return 0;
	case EXPR_KIND_LOAD:
		/* prefer loads to constants */
		if(op->expr.u.load_store.address_space == ADDRESS_SPACE_ATTRIBUTE)
		{
			return 1;
		}
		if (GET_N_CHILDREN(op) == 1 && GET_CHILD(op, 0) != 0)
		{
			/* Indexed load */
			return 2;
		} 
		else 
		{
			/* Unindexed load */
			return 2;
		}
	case EXPR_KIND_BINARY:
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
	case EXPR_KIND_UNARY:
		switch (op->expr.operation) {
		case EXPR_OP_FUN_RCP:
		case EXPR_OP_FUN_RCC:
		case EXPR_OP_FUN_MALIGP2_FIXED_EXP2:
		case EXPR_OP_FUN_MALIGP2_LOG2_FIXED:
		case EXPR_OP_FUN_INVERSESQRT:
		case EXPR_OP_FUN_EXP:
		case EXPR_OP_FUN_EXP2:
		case EXPR_OP_FUN_LOG:
		case EXPR_OP_FUN_LOG2:
		case EXPR_OP_DIV:
			return adv_fun_cost;
		case EXPR_OP_FUN_POW:
		case EXPR_OP_FUN_SQRT:
			return adv_fun_cost * 2;
		case EXPR_OP_FUN_SIN:
		case EXPR_OP_FUN_COS:	
			return adv_fun_cost * 2;
		case EXPR_OP_FUN_ASIN:
		case EXPR_OP_FUN_ACOS:
		case EXPR_OP_FUN_ATAN:
		case EXPR_OP_FUN_TAN:
			return adv_fun_cost * 3;
		case EXPR_OP_NEGATE:
			return 0;
		case EXPR_OP_SWIZZLE:
			return 0;
		default:
			return 1;
		}
	case EXPR_KIND_VECTOR_COMBINE:
		return 0;
	default:
		return 1;
	}
#endif
}

int _essl_maligp2_op_weight_scheduler(node *op)
{
	return maligp2_op_weight(op, 10);
}

int _essl_maligp2_op_weight_realistic(node *op)
{
	return maligp2_op_weight(op, 5);
}


static maligp2_instruction_word *add_word(maligp2_scheduler_context *ctx)
{
	maligp2_instruction_word *word;
	if (ctx->earliest_cycle >= MAX_COMPILER_INSTRUCTION_WORDS)
	{
		REPORT_ERROR(ctx->error_context, ERR_RESOURCES_EXHAUSTED, 0, "Maximum number of compiler supported instructions (%d) exceeded.\n", MAX_COMPILER_INSTRUCTION_WORDS);
		return NULL;
	}
	ESSL_CHECK(word = _essl_new_maligp2_instruction_word(ctx->pool, ctx->earliest_cycle++));
	word->original_cycle = word->cycle;
	word->successor = ctx->earliest_instruction_word;
	if(ctx->earliest_instruction_word)
	{
		ctx->earliest_instruction_word->predecessor = word;
	} 
	else 
	{
		ctx->latest_instruction_word = word;
	}
	ctx->earliest_instruction_word = word;
	return word;

}

static int count_unscheduled_child(node *n)
{
	node_extra *ex;
	if(n == NULL) 
	{
		return 0;
	}
	ex = EXTRA_INFO(n);
	if(ex->unscheduled_use_count == ex->original_use_count)
	{
		return 1;
	} 
	else 
	{
		return 0;
	}
}


static int get_register_pressure_effect_for_node(node *n)
{
	unsigned i;
	node *child;
	int effect = 0;
	if(n == NULL) 
	{
		return 0;
	}
	if(n->hdr.kind == EXPR_KIND_STORE)
	{
		effect += count_unscheduled_child(GET_CHILD(n, 0));

		child = GET_CHILD(n, 1);
		assert(child != NULL);
		if(child->hdr.kind == EXPR_KIND_MALIGP2_STORE_CONSTRUCTOR)
		{
			for(i = 0; i < GET_N_CHILDREN(child); ++i)
			{
				effect += count_unscheduled_child(GET_CHILD(child, 0));
			}
		} 
		else 
		{
			effect += count_unscheduled_child(child);
		}
		return effect;
	} else if(n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL && n->expr.operation == EXPR_OP_FUN_CLAMP)
	{
		/* the constant nodes of a clamp don't count */
		effect = -1;
		effect += count_unscheduled_child(GET_CHILD(n, 0));
		return effect;
	}

	/* regular case */
	effect = -1; /* one def */
	for(i = 0; i < GET_N_CHILDREN(n); ++i)
	{
		essl_bool already_seen = ESSL_FALSE;
		unsigned j;
		child = GET_CHILD(n, i);
		for(j = 0; j < i; ++j)
		{
			if(GET_CHILD(n, j) == child)
			{
				already_seen = ESSL_TRUE;
				break;
			}
		}
		if(!already_seen)
		{
			effect += count_unscheduled_child(child);
		}
	}
	return effect;
}


static int get_slack_for_node(/*@null@*/ node *n)
{
	static const int A_LOT_OF_CYCLES = 30000;
	node_extra *ne;
	if(n == NULL) 
	{
		return A_LOT_OF_CYCLES;
	}
	ESSL_CHECK(ne = EXTRA_INFO(n));
	switch(ne->operation_depth)
	{
	case 0:
	case 1:
	case 2:
		return A_LOT_OF_CYCLES;

	default:
#define COMPLEX_STORE_SLACK 25
		if(n->hdr.kind == EXPR_KIND_STORE)
		{
			/* let's see if this is a cheap load */
			node *child = GET_CHILD(n, 1);
			assert(child != NULL);
			if(child->hdr.kind == EXPR_KIND_LOAD)
			{
				return A_LOT_OF_CYCLES;
			} 
			else if(child->hdr.kind == EXPR_KIND_MALIGP2_STORE_CONSTRUCTOR)
			{
				unsigned i;
				for(i = 0; i < GET_N_CHILDREN(child); ++i)
				{
					node *ld = GET_CHILD(child, i);
					if(ld == NULL || ld->hdr.kind != EXPR_KIND_LOAD)
					{
						return COMPLEX_STORE_SLACK;
					}
				}
				return A_LOT_OF_CYCLES;
			}

			return COMPLEX_STORE_SLACK;
		} 
		else 
		{
			return A_LOT_OF_CYCLES;
		}
	}
}



static memerr schedule(maligp2_scheduler_context *ctx, /*@null@*/ node *root, ...) 
{
	/* Space in available words? */
	int slots[MALIGP2_MAX_INSTRUCTIONS_PER_BATCH];
	node *transfer_nodes[MALIGP2_MAX_INSTRUCTIONS_PER_BATCH] = {0};
	maligp2_instruction_word *word = 0;
	int address_reg = MALIGP2_ADDR_UNKNOWN;
	int earliest_use = -1; 
	int latest_use = -1;
	int earliest_use_cycle;
	int latest_use_cycle;
	int slack = get_slack_for_node(root);
	if(root != NULL) 
	{
		earliest_use = _essl_scheduler_get_earliest_use(root);
		latest_use = _essl_scheduler_get_latest_use(root);
	}
	
	earliest_use_cycle = MALIGP2_SUBCYCLE_TO_CYCLE(earliest_use);
	latest_use_cycle = MALIGP2_SUBCYCLE_TO_CYCLE(latest_use);
	for(;;)
	{
		if(ctx->earliest_instruction_word != NULL)
		{
			int earliest_instruction_cycle = ctx->earliest_instruction_word->cycle;
			for (word = ctx->latest_instruction_word ; word != 0 ; word = word->predecessor) 
			{
				essl_bool allocated = ESSL_FALSE;
				int cycles_from_earliest = earliest_instruction_cycle - word->cycle;
				if (word->cycle >= earliest_use_cycle && cycles_from_earliest <= slack) 
				{
					int same_cycle = (word->cycle == latest_use_cycle);
					
					va_list arglist;
					va_start(arglist, root);
					
					allocated = _essl_maligp2_allocate_slots(ctx->cfg, ctx->desc, word, earliest_use, same_cycle, slots, 
													transfer_nodes, MALIGP2_RESERVATION_DELETE, 
													MALIGP2_PERFORM_CSE, &address_reg, arglist);
					va_end(arglist);
					
					if (allocated != 0) 
					{
						/* Slots are available */
						break;
					}
				}
			}
			if(word != NULL) 
			{
				break; /* did we break out the inner loop? */
			}
		}
		/* Allocate new word if allowed to */
		if (ctx->can_add_cycles) 
		{

			ESSL_CHECK(add_word(ctx));
			
		} 
		else 
		{
			/* Don't schedule it yet */
			ESSL_CHECK(_essl_scheduler_postpone_operation(ctx->sctx, root));
			return MEM_OK;
		}
	}

	/* Write the instructions */
	{
		va_list arglist;
		va_start(arglist, root);
		ESSL_CHECK_VA(arglist, _essl_maligp2_write_instructions(ctx, word, slots, transfer_nodes, address_reg, arglist));
		va_end(arglist);
		return MEM_OK;
	}
}

static memerr handle_add(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, MALIGP2_ADD, &res, a, b, MALIGP2_FINISH);
}

static memerr handle_mul(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, MALIGP2_MUL, &res, a, b, MALIGP2_FINISH);
}

static memerr handle_mul_expwrap(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, MALIGP2_MUL_EXPWRAP, &res, a, b, MALIGP2_FINISH);
}

static memerr handle_rcp(maligp2_scheduler_context *ctx, node *res)
{
	node *b;
	ESSL_CHECK(b = GET_CHILD(res, 0));
	ESSL_CHECK(_essl_scheduler_add_scheduled_use(b));
	ESSL_CHECK(_essl_scheduler_add_scheduled_use(b));
	ESSL_CHECK(_essl_scheduler_add_scheduled_use(b));
	return schedule(ctx, res, 
				   MALIGP2_MUL_ADD, &res, b, 
				   MALIGP2_NEXT_CYCLE,
				   MALIGP2_RCP, NULL, b,
				   MALIGP2_MUL_EXPWRAP, NULL, b, b,
				   MALIGP2_FINISH);
}

static memerr handle_rcc(maligp2_scheduler_context *ctx, node *res)
{
	node *b;
	ESSL_CHECK(b = GET_CHILD(res, 0));
	ESSL_CHECK(_essl_scheduler_add_scheduled_use(b));
	ESSL_CHECK(_essl_scheduler_add_scheduled_use(b));
	ESSL_CHECK(_essl_scheduler_add_scheduled_use(b));
	return schedule(ctx, res, 
				   MALIGP2_MUL_ADD, &res, b, 
				   MALIGP2_NEXT_CYCLE,
				   MALIGP2_RCC, NULL, b,
				   MALIGP2_MUL_EXPWRAP, NULL, b, b,
				   MALIGP2_FINISH);
}

static memerr handle_identity(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, MALIGP2_MOV, &res, a, MALIGP2_FINISH);
}

static memerr handle_lt(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, MALIGP2_SLT, &res, a, b, MALIGP2_FINISH);
}

static memerr handle_ge(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, MALIGP2_SGE, &res, a, b, MALIGP2_FINISH);
}

static memerr handle_constant(maligp2_scheduler_context *ctx, node *res)
{
	return schedule(ctx, res, 
				   MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD, &res, 
				   MALIGP2_CONSTANT, &res, MALIGP2_FINISH);	

}

static memerr handle_load(maligp2_scheduler_context *ctx, node *res)
{
	node *index;

	index = GET_CHILD(res, 0);
	if(res->expr.u.load_store.address_space == ADDRESS_SPACE_ATTRIBUTE)
	{
		assert(index == NULL);
		return schedule(ctx, res, 
				   MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD, &res,
				   MALIGP2_LOAD_INPUT_REG, &res, MALIGP2_FINISH);
	}
	else if(index != NULL)
	{
		return schedule(ctx, res, 
				   MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD, &res,
				   MALIGP2_LOAD_CONSTANT_INDEXED, &res, MALIGP2_NEXT_CYCLE, 
				   MALIGP2_NEXT_CYCLE, 
				   MALIGP2_NEXT_CYCLE, 
				   MALIGP2_NEXT_CYCLE,
				   MALIGP2_SET_LOAD_ADDRESS, NULL, index, 4, MALIGP2_FINISH);
	}
	else
	{
		return schedule(ctx, res, 
				   MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD, &res,
				   MALIGP2_LOAD_CONSTANT, &res, MALIGP2_FINISH);
	}
}

static memerr handle_store(maligp2_scheduler_context *ctx, node *res)
{
	node *cons;
	node *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;

	ESSL_CHECK(cons = GET_CHILD(res, 1));
	if(cons->hdr.kind == EXPR_KIND_MALIGP2_STORE_CONSTRUCTOR)
	{
		unsigned nchild = GET_N_CHILDREN(cons);
		switch(nchild)
		{
			case 4:
				ESSL_CHECK(v4 = GET_CHILD(cons, 3));
				/* fall-through */
			case 3:
				ESSL_CHECK(v3 = GET_CHILD(cons, 2));
				/* fall-through */
			case 2:
				ESSL_CHECK(v2 = GET_CHILD(cons, 1));
				ESSL_CHECK(v1 = GET_CHILD(cons, 0));
				break;
			default:
				assert(0 && "not implemented operation");
				return MEM_ERROR;
		}
	}
	else
	{
		v1 = cons;
		cons = NULL;
	}

	if(res->expr.u.load_store.address_space == ADDRESS_SPACE_VERTEX_VARYING)
	{
		return schedule(ctx, res, 
						MALIGP2_STORE_OUTPUT_REG, &res, v1, v2, v3, v4, 
						MALIGP2_SCHEDULE_EXTRA, &cons,
						MALIGP2_RESERVE_MOV, &v1,
						MALIGP2_RESERVE_MOV, &v2,
						MALIGP2_RESERVE_MOV, &v3,
						MALIGP2_RESERVE_MOV, &v4,
						MALIGP2_FINISH);
	}
	else
	{
		node_extra *ne;
		node *address;
		ESSL_CHECK(address = GET_CHILD(res, 0));
		ESSL_CHECK(ne = EXTRA_INFO(res));
		if(ne->address_symbols != NULL && ne->address_symbols->sym->is_persistent_variable)
		{
			/** If this is a variable that's marked as a persistent
			 * global, we can't write to it in the last couple of cycles,
			 * as it might be read in the first cycle of the next instance
			 * of this shader, and SIF does not insert a mandatory 5-cycle
			 * delay between shader instances. Fix this by modifying
			 * earliest use to disallow it. */


			ne->earliest_use = ESSL_MAX(ne->earliest_use, MALIGP2_CYCLE_TO_SUBCYCLE(3, 1));
		}
		return schedule(ctx, res, 
						MALIGP2_STORE_CONSTANT, &res, v1, v2, v3, v4, 
						MALIGP2_SCHEDULE_EXTRA, &cons,
						MALIGP2_STORE_SELECT_ADDRESS_REG, NULL, 
						MALIGP2_SET_STORE_ADDRESS, NULL, address, 1,
						MALIGP2_RESERVE_MOV, &v1,
						MALIGP2_RESERVE_MOV, &v2,
						MALIGP2_RESERVE_MOV, &v3,
						MALIGP2_RESERVE_MOV, &v4,
						MALIGP2_FINISH);

	}

}

static memerr handle_csel(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	node *b;
	node *cond;
	ESSL_CHECK(cond = GET_CHILD(res, 0));
	ESSL_CHECK(a = GET_CHILD(res, 1));
	ESSL_CHECK(b = GET_CHILD(res, 2));

	return schedule(ctx, res, 
				   MALIGP2_CSEL, &res, cond, a, b,
				   MALIGP2_FINISH);
}

static memerr handle_fixed_exp2(maligp2_scheduler_context *ctx, node *res)
{
	node *fix;
	ESSL_CHECK(fix = GET_CHILD(res, 0));
	if(fix->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
		fix->expr.operation == EXPR_OP_FUN_MALIGP2_FLOAT_TO_FIXED)
	{
		node *a;
		ESSL_CHECK(a = GET_CHILD(fix, 0));
		ESSL_CHECK(_essl_scheduler_add_scheduled_use(a));
		ESSL_CHECK(_essl_scheduler_add_scheduled_use(a));
		ESSL_CHECK(_essl_scheduler_add_scheduled_use(a));
		return schedule(ctx, res, 
				   MALIGP2_MUL_ADD, &res, fix,
				   MALIGP2_NEXT_CYCLE,
				   MALIGP2_EX2, NULL, fix,
				   MALIGP2_MUL_EXPWRAP, NULL, fix, fix,
				   MALIGP2_RESERVE_MOV_MISC, NULL, fix,
				   MALIGP2_NEXT_CYCLE,
				   MALIGP2_FLOAT_TO_FIXED, &fix, a,
				   MALIGP2_FINISH);
	}
	else
	{
		assert(0 && "not implemented operation");
		return MEM_ERROR;
	}
}

static memerr handle_fixed2float(maligp2_scheduler_context *ctx, node *res)
{
	node *fix;
	ESSL_CHECK(fix = GET_CHILD(res, 0));
	if(fix->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
		fix->expr.operation == EXPR_OP_FUN_MALIGP2_LOG2_FIXED)
	{
		node *a;
		ESSL_CHECK(a = GET_CHILD(fix, 0));
		ESSL_CHECK(_essl_scheduler_add_scheduled_use(a));
		ESSL_CHECK(_essl_scheduler_add_scheduled_use(a));
		ESSL_CHECK(_essl_scheduler_add_scheduled_use(a));
		return schedule(ctx, res, 
				   MALIGP2_FIXED_TO_FLOAT, &res, fix,
				   MALIGP2_NEXT_CYCLE,
				   MALIGP2_RESERVE_MOV_MISC, NULL, fix,
				   MALIGP2_NEXT_CYCLE,
				   MALIGP2_MUL_ADD, &fix, a, 
				   MALIGP2_NEXT_CYCLE,
				   MALIGP2_LG2, NULL, a,
				   MALIGP2_MUL_EXPWRAP, NULL, a, a,
				   MALIGP2_FINISH);
	}
	else
	{
		assert(0 && "not implemented operation");
		return MEM_ERROR;
	}
}

static memerr handle_rqsrt(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(_essl_scheduler_add_scheduled_use(a));
	ESSL_CHECK(_essl_scheduler_add_scheduled_use(a));
	ESSL_CHECK(_essl_scheduler_add_scheduled_use(a));
	return schedule(ctx, res, 
				   MALIGP2_MUL_ADD, &res, a, 
				   MALIGP2_NEXT_CYCLE,
				   MALIGP2_RSQ, NULL, a,
				   MALIGP2_MUL_EXPWRAP, NULL, a, a,
				   MALIGP2_FINISH);

}

static memerr handle_floor(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, MALIGP2_FLOOR, &res, a, MALIGP2_FINISH);
}

static memerr handle_sign(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, MALIGP2_SIGN, &res, a, MALIGP2_FINISH);
}

static memerr handle_min(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, MALIGP2_MIN, &res, a, b, MALIGP2_FINISH);
}

static memerr handle_max(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, MALIGP2_MAX, &res, a, b, MALIGP2_FINISH);
}

static memerr handle_clamp(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	node *lowlim;
	node *highlim;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(lowlim = GET_CHILD(res, 1));
	ESSL_CHECK(highlim = GET_CHILD(res, 2));
	if(lowlim->hdr.kind == EXPR_KIND_CONSTANT
		&& highlim->hdr.kind == EXPR_KIND_CONSTANT)
	{
		return schedule(ctx, res, 
						MALIGP2_CLAMP, &res, a, 
						MALIGP2_LOCKED_CONSTANT, &lowlim, 0, 
						MALIGP2_LOCKED_CONSTANT, &highlim, 1, 
						MALIGP2_FINISH);
	}
	else
	{
		assert(0 && "not implemented operation");
		return MEM_ERROR;
	}
}

static memerr handle_negate(maligp2_scheduler_context *ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, MALIGP2_NEGATE, &res, a, MALIGP2_FINISH);
}

static memerr handle_depend(maligp2_scheduler_context *ctx, node *res)
{
	node_extra *resinfo;
	ESSL_CHECK(resinfo = EXTRA_INFO(res));
	ESSL_CHECK(_essl_scheduler_schedule_operation(ctx->sctx, res, resinfo->earliest_use));
	return MEM_OK;
}

static memerr maligp2_schedule_single_operation(maligp2_scheduler_context *ctx, node *n)
{
	node_kind kind;
	expression_operator op;
	ESSL_CHECK(n);
	kind = n->hdr.kind;
	op = n->expr.operation;
	if(kind == EXPR_KIND_UNARY)
	{
		if(op == EXPR_OP_IDENTITY)
		{
			return handle_identity(ctx, n);
		}
		else if(op == EXPR_OP_NEGATE)
		{
			return handle_negate(ctx, n);
		}
		else
		{
			assert(0 && "not implemented operation");
			return MEM_ERROR;
		}
	}
	else if(kind == EXPR_KIND_BINARY)
	{
		if(op == EXPR_OP_ADD)
		{
			return handle_add(ctx, n);
		}
		else if(op == EXPR_OP_MUL)
		{
			return handle_mul(ctx, n);
		}
		else if(op == EXPR_OP_LT)
		{
			return handle_lt(ctx, n);
		}
		else if(op == EXPR_OP_GE)
		{
			return handle_ge(ctx, n);
		}
		else
		{
			assert(0 && "not implemented operation");
			return MEM_ERROR;
		}
	}
	else if(kind == EXPR_KIND_BUILTIN_FUNCTION_CALL)
	{
		if(op == EXPR_OP_FUN_MALIGP2_MUL_EXPWRAP)
		{
			return handle_mul_expwrap(ctx, n);
		}
		else if(op == EXPR_OP_FUN_RCP)
		{
			return handle_rcp(ctx, n);
		}
		else if(op == EXPR_OP_FUN_RCC)
		{
			return handle_rcc(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MALIGP2_FIXED_EXP2)
		{
			return handle_fixed_exp2(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MALIGP2_FIXED_TO_FLOAT)
		{
			return handle_fixed2float(ctx, n);
		}
		else if(op == EXPR_OP_FUN_INVERSESQRT)
		{
			return handle_rqsrt(ctx, n);
		}
		else if(op == EXPR_OP_FUN_FLOOR)
		{
			return handle_floor(ctx, n);
		}
		else if(op == EXPR_OP_FUN_SIGN)
		{
			return handle_sign(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MIN)
		{
			return handle_min(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MAX)
		{
			return handle_max(ctx, n);
		}
		else if(op == EXPR_OP_FUN_CLAMP)
		{
			return handle_clamp(ctx, n);
		}
		else if(_essl_node_is_texture_operation(n))
		{
			REPORT_ERROR(ctx->error_context, ERR_LP_UNDEFINED_IDENTIFIER, n->hdr.source_offset, 
									"Function '%s' not supported on target\n", n->expr.u.fun.sym->name);
			return MEM_ERROR;
		}
		else
		{
			assert(0 && "not implemented operation");
			return MEM_ERROR;
		}
	}
	else if(kind == EXPR_KIND_VARIABLE_REFERENCE || kind == EXPR_KIND_CONSTANT)
	{
		return handle_constant(ctx, n);
	}
	else if(kind == EXPR_KIND_LOAD)
	{
		return handle_load(ctx, n);
	}
	else if(kind == EXPR_KIND_STORE)
	{
		return handle_store(ctx, n);
	}
	else if(kind == EXPR_KIND_TERNARY)
	{
		if(op == EXPR_OP_CONDITIONAL_SELECT)
		{
			return handle_csel(ctx, n);
		}
		else
		{
			assert(0 && "not implemented operation");
			return MEM_ERROR;
		}
	}
	else if(kind == EXPR_KIND_DEPEND)
	{
		return handle_depend(ctx, n);
	}
	else
	{
		assert(0 && "not implemented operation");
		return MEM_ERROR;
	}

	return MEM_OK;
}

static int operation_priority(node *n, int current_register_pressure)
{
#ifdef RANDOMIZED_COMPILATION
       return _essl_get_random_int(0, 1000);
#else
	node_extra *ex;
	int register_pressure_scale = 0;
	int register_pressure_effect = get_register_pressure_effect_for_node(n);


	if(current_register_pressure < 4) 
	{
		register_pressure_scale = 2;
	}
	else if(current_register_pressure < 8)
	{
		register_pressure_scale = 0;
	}
	else if(current_register_pressure < 10) 
	{
		register_pressure_scale = -2;
	}
	else if(current_register_pressure < 12)
	{
		register_pressure_scale = -5;
	}
	else if(current_register_pressure < 20)
	{
		register_pressure_scale = -10;
	}
	else
	{
		register_pressure_scale = -15;
	}
	ex = EXTRA_INFO(n);

	return 2*ex->operation_depth - 0*ex->earliest_use + register_pressure_scale*register_pressure_effect;
#endif
}

static int data_dependency_delay(/*@null@*/ node *n, node *dependency)
{
	int delay = 0;
	if(n != NULL)
	{
		switch(n->hdr.kind)
		{
		case EXPR_KIND_BINARY:
		case EXPR_KIND_BUILTIN_FUNCTION_CALL:
			switch(n->expr.operation)
			{
			case EXPR_OP_FUN_RCP:
			case EXPR_OP_FUN_RCC:
			case EXPR_OP_FUN_INVERSESQRT:
			case EXPR_OP_FUN_MALIGP2_FIXED_EXP2:
			case EXPR_OP_FUN_MALIGP2_LOG2_FIXED:
				delay += MALIGP2_CYCLE_TO_SUBCYCLE(1, 0); /* we need the input to the lut op one cycle earlier than the mul_add that defs the result */
				break;
			default:
				break;
			}
			break;
		case EXPR_KIND_LOAD:
			if(dependency != 0)
			{
				delay += MALIGP2_CYCLE_TO_SUBCYCLE(4, -1); /* we need that address kinda early */
			}
			break;
		case EXPR_KIND_STORE:
			if(dependency != 0 && dependency == GET_CHILD(n, 0))
			{
				delay += MALIGP2_CYCLE_TO_SUBCYCLE(0, 1); /* we need that address a bit early */
			}
		default:
			break;
		}
	}
	if(dependency != 0)
	{
		switch(dependency->hdr.kind)
		{
		case EXPR_KIND_BINARY:
		case EXPR_KIND_BUILTIN_FUNCTION_CALL:
			switch(dependency->expr.operation)
			{
			case EXPR_OP_FUN_RCP:
			case EXPR_OP_FUN_RCC:
			case EXPR_OP_FUN_INVERSESQRT:
			case EXPR_OP_FUN_MALIGP2_FIXED_EXP2:
			case EXPR_OP_FUN_MALIGP2_LOG2_FIXED:
				delay += MALIGP2_CYCLE_TO_SUBCYCLE(2, -1);
			default:
				break;
			}
		default:
			break;
		}
	}
	return delay;
}

static int control_dependency_delay(/*@null@*/ node *n, node *dependency)
{
	if(n != 0 && dependency != 0)
	{
		if(n->hdr.kind == EXPR_KIND_LOAD && dependency->hdr.kind == EXPR_KIND_STORE)
		{
			return MALIGP2_CYCLE_TO_SUBCYCLE(3, 1);
		}
	}
	return 0;
}

static memerr schedule_block_exit(maligp2_scheduler_context *ctx, basic_block *b)
{

	
	/* exit block with no out variables and no conditional predecessors -> ret is done in the return blocks instead */
	if(b->termination == TERM_KIND_EXIT && b->control_dependent_ops == 0)
	{
		predecessor_list *pred;
		essl_bool cond_pred = ESSL_FALSE;
		for (pred = b->predecessors ; pred != 0 ; pred = pred->next) 
		{
			if (pred->block->termination == TERM_KIND_JUMP && pred->block->source != 0) 
			{
				cond_pred = ESSL_TRUE;
				break;
			}
		}
		if (!cond_pred) 
		{
			return MEM_OK;
		}
	}

	switch(b->termination)
	{
	case TERM_KIND_UNKNOWN:
		/*@ fall-through @*/
	case TERM_KIND_DISCARD:
		assert(0);
		break;
	case TERM_KIND_JUMP:
		if (b->source != 0) 
		{
			/* Conditional jump */
			ESSL_CHECK(schedule(ctx, NULL, MALIGP2_JMP_COND, NULL, b->successors[BLOCK_TRUE_TARGET], MALIGP2_MOV_MISC, NULL, b->source, MALIGP2_FINISH));
			break;
		} 
		else 
		{
			if (b->successors[BLOCK_DEFAULT_TARGET]->termination != TERM_KIND_EXIT || b->successors[BLOCK_DEFAULT_TARGET]->control_dependent_ops)
			{
				/* This is either a normal jump or a jump to an exit block containing control-dependent operations. */
				if(b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number == b->output_visit_number + 1)
				{
					/* jump to the next block -> no-op */
					ESSL_CHECK(add_word(ctx)); /* add an instruction anyway, as each block ought to have at least one instruction in order to make the register allocator happy */
					break;
				}
				ESSL_CHECK(schedule(ctx, NULL, MALIGP2_JMP, NULL, b->successors[BLOCK_DEFAULT_TARGET], MALIGP2_FINISH));
				break;
			}
			/* This is a jump to an exit block with no control-dependent operations. We can just return directly. */
			b->termination = TERM_KIND_EXIT;
			{
				/* Remove corresponding phi source of the exit block and make it the source of this block */
				basic_block *exit_block = b->successors[BLOCK_DEFAULT_TARGET];
				essl_bool found = ESSL_FALSE;
				phi_list **phi_p;
				for (phi_p = &exit_block->phi_nodes ; !found && *phi_p != 0 ; phi_p = &(*phi_p)->next) {
					phi_source **phi_s_p;
					for (phi_s_p = &(*phi_p)->phi_node->expr.u.phi.sources ; *phi_s_p != 0 ; phi_s_p = &(*phi_s_p)->next)
					{
						if ((*phi_s_p)->join_block == b) {
							b->source = (*phi_s_p)->source;
							*phi_s_p = (*phi_s_p)->next;
							if ((*phi_p)->phi_node->expr.u.phi.sources == 0) {
								*phi_p = (*phi_p)->next;
								exit_block->source = 0;
							}
							found = ESSL_TRUE;
							break;
						}
					}
				}
			}
		}
		/*@ fall-through @*/
	case TERM_KIND_EXIT:
		if(ctx->desc->has_entry_point && ctx->function != ctx->tu->entry_point && !ctx->function->opt.pilot.is_proactive_func)
		{
			ESSL_CHECK(schedule(ctx, NULL, MALIGP2_RET, NULL, MALIGP2_FINISH));
		} else 
		{
			/* don't do a RET. the exec boundaries will make sure to terminate the program */
			ESSL_CHECK(add_word(ctx));
		}
		break;
	}
	return MEM_OK;
}



memerr _essl_maligp2_schedule_function(mempool *pool, translation_unit *tu, symbol *function, 
							maligp2_relocation_context *relocation_context, error_context *error_context)
{
	scheduler_context sched_ctx, *sctx = &sched_ctx;
	maligp2_scheduler_context maligp2_ctx, *ctx = &maligp2_ctx;
	ctx->sctx = sctx;
	ctx->pool = pool;
	ctx->tu = tu;
	ctx->desc = tu->desc;
	ctx->can_add_cycles = ESSL_TRUE;
	ctx->earliest_instruction_word = 0;
	ctx->latest_instruction_word = 0;
	ctx->earliest_cycle = 1;
	ctx->function = function;
	ctx->cfg = function->control_flow_graph;
	ctx->relocation_context = relocation_context;
	ctx->error_context = error_context;
	ESSL_CHECK(_essl_scheduler_init(sctx, ctx->pool, ctx->cfg, operation_priority, ctx));
	_essl_scheduler_set_data_dependency_delay_callback(sctx, data_dependency_delay);
	_essl_scheduler_set_control_dependency_delay_callback(sctx, control_dependency_delay);

	while(_essl_scheduler_more_blocks(sctx))
	{
		basic_block *b;
		ESSL_CHECK(b = _essl_scheduler_begin_block(sctx, MALIGP2_CYCLE_TO_SUBCYCLE(ctx->earliest_cycle, 1)));
		b->bottom_cycle = ctx->earliest_cycle;

		ctx->earliest_instruction_word = 0;
		ctx->latest_instruction_word = 0;
		ctx->can_add_cycles = ESSL_TRUE;
		ESSL_CHECK(schedule_block_exit(ctx, b));
		while(!_essl_scheduler_block_complete(sctx))
		{
			node *op = _essl_scheduler_next_operation(sctx);
			ESSL_CHECK_NONFATAL(maligp2_schedule_single_operation(ctx, op));
		}
		
		
		while(_essl_scheduler_more_operations(sctx))
		{
			node *op = _essl_scheduler_next_operation(sctx);

			ctx->can_add_cycles = ESSL_FALSE;
			if(op->expr.earliest_block == b || op->expr.best_block == b)
			{
				ctx->can_add_cycles = ESSL_TRUE;
			}
			if(op->expr.earliest_block == NULL)
			{
				ctx->can_add_cycles = ESSL_TRUE;
			}

			ESSL_CHECK(maligp2_schedule_single_operation(ctx, op));
			
		}

		assert(_essl_scheduler_block_complete(sctx));
		b->earliest_instruction_word = ctx->earliest_instruction_word;
		b->latest_instruction_word = ctx->latest_instruction_word;
		ESSL_CHECK(_essl_scheduler_finish_block(sctx));

		b->top_cycle = ctx->earliest_cycle-1;
	}

	return MEM_OK;
}
