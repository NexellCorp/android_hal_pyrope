/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "frontend/optimise_loop_entry.h"

typedef struct _tag_loop_entry_context {
	mempool *pool;
	target_descriptor *desc;
} loop_entry_context;

static node *clone_exp(loop_entry_context *ctx, node *exp)
{
	unsigned i;
	ESSL_CHECK(exp = _essl_clone_node(ctx->pool, exp));
	for (i = 0 ; i < GET_N_CHILDREN(exp) ; i++)
	{
		node *child = GET_CHILD(exp, i);
		if (child != NULL)
		{
			node *cloned_child = clone_exp(ctx, child);
			ESSL_CHECK(cloned_child);
			SET_CHILD(exp, i, cloned_child);
		}
	}

	if (exp->hdr.kind == EXPR_KIND_FUNCTION_CALL)
	{
		/* Correct callee call count */
		symbol *callee = exp->expr.u.fun.sym;
		callee->call_count += 1;
	}

	return exp;
}

static memerr optimise_loop_entry_stmt(loop_entry_context *ctx, node *stmt)
{
	if (stmt == NULL) return MEM_OK;

	switch (stmt->hdr.kind)
	{
	case STMT_KIND_COMPOUND:
		{
			unsigned k;
			for(k = 0; k < GET_N_CHILDREN(stmt); ++k)
			{
				ESSL_CHECK(optimise_loop_entry_stmt(ctx, GET_CHILD(stmt, k)));
			}
		}
		break;

	case STMT_KIND_IF:
		ESSL_CHECK(optimise_loop_entry_stmt(ctx, GET_CHILD(stmt, 1)));
		ESSL_CHECK(optimise_loop_entry_stmt(ctx, GET_CHILD(stmt, 2)));
		break;

	case STMT_KIND_DO:
		ESSL_CHECK(optimise_loop_entry_stmt(ctx, GET_CHILD(stmt, 0)));
		break;

	case STMT_KIND_WHILE:
		ESSL_CHECK(optimise_loop_entry_stmt(ctx, GET_CHILD(stmt, 1)));
		{
			/* Rewrite into pre-conditioned while loop */
			node *cond = GET_CHILD(stmt, 0);
			node *body = GET_CHILD(stmt, 1);
			node *pre_cond = clone_exp(ctx, cond);
			ESSL_CHECK(pre_cond);

			stmt->hdr.kind = STMT_KIND_COND_WHILE;
			ESSL_CHECK(SET_N_CHILDREN(stmt, 3, ctx->pool));
			SET_CHILD(stmt, 0, pre_cond);
			SET_CHILD(stmt, 1, body);
			SET_CHILD(stmt, 2, cond);
		}
		break;
			
	case STMT_KIND_FOR:
		ESSL_CHECK(optimise_loop_entry_stmt(ctx, GET_CHILD(stmt, 3)));
		{
			/* Rewrite into pre-conditioned for loop */
			node *init = GET_CHILD(stmt, 0);
			node *cond = GET_CHILD(stmt, 1);
			node *update = GET_CHILD(stmt, 2);
			node *body = GET_CHILD(stmt, 3);
			node *pre_cond = clone_exp(ctx, cond);
			ESSL_CHECK(pre_cond);

			stmt->hdr.kind = STMT_KIND_COND_FOR;
			ESSL_CHECK(SET_N_CHILDREN(stmt, 5, ctx->pool));
			SET_CHILD(stmt, 0, init);
			SET_CHILD(stmt, 1, pre_cond);
			SET_CHILD(stmt, 2, body);
			SET_CHILD(stmt, 3, update);
			SET_CHILD(stmt, 4, cond);
		}
		break;

	default:
		break;
	}

	return MEM_OK;
}

memerr _essl_optimise_loop_entry(mempool *pool, symbol *function, target_descriptor *desc)
{
	loop_entry_context le_ctx;
	le_ctx.pool = pool;
	le_ctx.desc = desc;

	ESSL_CHECK(optimise_loop_entry_stmt(&le_ctx, function->body));

	return MEM_OK;
}
