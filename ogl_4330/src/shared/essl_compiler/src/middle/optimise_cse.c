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
#include "common/general_dict.h"
#include "middle/optimise_cse.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif /* M_PI */


#define LOG2_E 1.4426950408889634

typedef struct 
{
	mempool *pool;
	control_flow_graph *cfg;
	target_descriptor *desc;
	ptrdict visited;
	typestorage_context *typestor_ctx;
	basic_block *current_block;
	general_dict nodes;
} optimise_cse_context;

static essl_bool node_is_cse_candidate(node *n)
{
	if(n->hdr.is_control_dependent) return ESSL_FALSE;
	switch(n->hdr.kind)
	{
	case EXPR_KIND_LOAD:
		switch(n->expr.u.load_store.address_space)
		{
		case ADDRESS_SPACE_FRAGMENT_VARYING:
			return ESSL_TRUE;
		default:
			return ESSL_FALSE;
		}

	case EXPR_KIND_CONSTANT:
		return ESSL_TRUE;

	case EXPR_KIND_UNARY:
		if(n->expr.operation == EXPR_OP_MEMBER) return ESSL_TRUE;
		return ESSL_TRUE;
	case EXPR_KIND_BINARY:
		if(n->expr.operation == EXPR_OP_INDEX) return ESSL_TRUE;
		return ESSL_FALSE;
	case EXPR_KIND_VARIABLE_REFERENCE:
		return ESSL_TRUE;
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		return ESSL_TRUE;
	case EXPR_KIND_VECTOR_COMBINE:
		return ESSL_TRUE;
	case EXPR_KIND_TERNARY:
		return ESSL_TRUE;

	default:
		return ESSL_FALSE;
	}
}

static essl_bool tree_equals(target_descriptor *desc, const void *va, const void *vb)
{
	const node *a = va;
	const node *b = vb;
	unsigned i;
	unsigned n;
	if(a == b) return ESSL_TRUE; /* early ok */
	if(a == NULL || b == NULL) return ESSL_FALSE;
	if(a->hdr.kind != b->hdr.kind) return ESSL_FALSE;
	if(a->hdr.is_control_dependent || b->hdr.is_control_dependent) return ESSL_FALSE;
	if(GET_N_CHILDREN(a) != GET_N_CHILDREN(b)) return ESSL_FALSE;

	if(a->expr.earliest_block != b->expr.earliest_block) return ESSL_FALSE;
	if(a->expr.latest_block != b->expr.latest_block) return ESSL_FALSE;
	/* don't need to test best_block, if earliest and latest block is the same the best block is the same as well */

	if(!_essl_type_with_scalar_size_equal(a->hdr.type, b->hdr.type)) return ESSL_FALSE;

	switch(a->hdr.kind)
	{
	case EXPR_KIND_UNARY:
	case EXPR_KIND_BINARY:
	case EXPR_KIND_TERNARY:
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		if(a->expr.operation != b->expr.operation) return ESSL_FALSE;
		if(a->expr.operation == EXPR_OP_SWIZZLE)
		{
			for(i = 0; i < N_COMPONENTS; ++i)
			{
				if(a->expr.u.swizzle.indices[i] != b->expr.u.swizzle.indices[i]) return ESSL_FALSE;
			}
		} else if(a->expr.operation == EXPR_OP_MEMBER)
		{
			if(a->expr.u.member != b->expr.u.member) return ESSL_FALSE;
		}
		break;

	case EXPR_KIND_VARIABLE_REFERENCE:
		if(a->expr.u.sym != b->expr.u.sym) return ESSL_FALSE;
		break;
	case EXPR_KIND_CONSTANT:
		n = _essl_get_type_size(a->hdr.type);
		if(memcmp(&a->expr.u.value[0], &b->expr.u.value[0], n * sizeof(scalar_type))) return ESSL_FALSE;
		break;
	case EXPR_KIND_FUNCTION_CALL:
		return ESSL_FALSE;
	case EXPR_KIND_BUILTIN_CONSTRUCTOR:
		break;
	case EXPR_KIND_STRUCT_CONSTRUCTOR:
		break;
	case EXPR_KIND_PHI:
		return ESSL_FALSE; /* not identity -> not equal */
	case EXPR_KIND_DONT_CARE:
		break;
	case EXPR_KIND_LOAD:
	case EXPR_KIND_STORE:
		if(a->expr.u.load_store.address_space != b->expr.u.load_store.address_space) return ESSL_FALSE;
		break;
	case EXPR_KIND_TYPE_CONVERT:
		break;

	case EXPR_KIND_VECTOR_COMBINE:
		for(i = 0; i < N_COMPONENTS; ++i)
		{
			if(a->expr.u.combiner.mask[i] != b->expr.u.combiner.mask[i]) return ESSL_FALSE;
		}
		break;
		
	default:
		return ESSL_FALSE;
	}

	n = GET_N_CHILDREN(a);
	for(i = 0; i < n; ++i)
	{
		node *ach = GET_CHILD(a, i);
		node *bch = GET_CHILD(b, i);
		if(!tree_equals(desc, ach, bch)) return ESSL_FALSE;
	}
	
	return ESSL_TRUE;
}

static general_dict_hash_type tree_hash(target_descriptor *desc, const void *va)
{
	const node *a = va;
	IGNORE_PARAM(desc);
	return a->hdr.kind;
}


/*@null@*/ static memerr process_node_w_pass1(optimise_cse_context *ctx, node *n)
{
	if(node_is_cse_candidate(n))
	{
		ESSL_CHECK(_essl_general_dict_insert(&ctx->nodes, n, n));
	}


	return MEM_OK;
}

/*@null@*/ static memerr process_node_pass1(optimise_cse_context *ctx, node *n)
{
	unsigned i;
	assert(n != 0);
	if((_essl_ptrdict_lookup(&ctx->visited, n)) != 0)
	{
		return MEM_OK;
	}

	if(n->hdr.kind == EXPR_KIND_PHI)
	{
		phi_source *src;
		for(src = n->expr.u.phi.sources; src != 0; src = src->next)
		{
			if(src->source->hdr.kind != EXPR_KIND_PHI)
			{
				ESSL_CHECK(process_node_pass1(ctx, src->source));
			}
		}

	} else {
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			node *child = GET_CHILD(n, i);
			if(child->hdr.kind != EXPR_KIND_PHI)
			{
				ESSL_CHECK(process_node_pass1(ctx, child));
			}
		}
	}	
	ESSL_CHECK(process_node_w_pass1(ctx, n));
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, n, n));
	return MEM_OK;
}



static memerr handle_block_pass1(optimise_cse_context *ctx, basic_block *b)
{
	phi_list *phi;
	control_dependent_operation *c_op;
	ctx->current_block = b;
	if(b->source)
	{
		ESSL_CHECK(process_node_pass1(ctx, b->source));
	}
	for(c_op = b->control_dependent_ops; c_op != 0; c_op = c_op->next)
	{
		ESSL_CHECK(process_node_pass1(ctx, c_op->op));
	}

	for(phi = b->phi_nodes; phi != 0; phi = phi->next)
	{		
		ESSL_CHECK(process_node_pass1(ctx, phi->phi_node));
	}

	return MEM_OK;
}



/*@null@*/ static node *process_node_w_pass2(optimise_cse_context *ctx, node *n)
{

	node *nn = n;
	if(node_is_cse_candidate(n))
	{
		ESSL_CHECK(nn = _essl_general_dict_lookup(&ctx->nodes, n));
	}

	return nn;
}

/*@null@*/ static node *process_node_pass2(optimise_cse_context *ctx, node *n)
{
	unsigned i;
	node *nn;
	assert(n != 0);
	if((nn = _essl_ptrdict_lookup(&ctx->visited, n)) != 0)
	{
		return nn;
	}

	if(n->hdr.kind == EXPR_KIND_PHI)
	{
		phi_source *src;
		for(src = n->expr.u.phi.sources; src != 0; src = src->next)
		{
			node *tmp;
			if(src->source->hdr.kind != EXPR_KIND_PHI)
			{
				ESSL_CHECK(tmp = process_node_pass2(ctx, src->source));
				src->source = tmp;
			}
		}

	} else {
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			node *child = GET_CHILD(n, i);
			if(child->hdr.kind != EXPR_KIND_PHI)
			{
				ESSL_CHECK(child = process_node_pass2(ctx, child));
				SET_CHILD(n, i, child);
			}
		}
	}	
	ESSL_CHECK(nn = process_node_w_pass2(ctx, n));
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, n, nn));
	return nn;
}



static memerr handle_block_pass2(optimise_cse_context *ctx, basic_block *b)
{
	phi_list *phi;
	control_dependent_operation *c_op;
	ctx->current_block = b;
	if(b->source)
	{
		node *tmp;
		ESSL_CHECK(tmp = process_node_pass2(ctx, b->source));
		b->source = tmp;
	}
	for(c_op = b->control_dependent_ops; c_op != 0; c_op = c_op->next)
	{
		node *tmp;
		ESSL_CHECK(tmp = process_node_pass2(ctx, c_op->op));
		if(tmp->hdr.is_control_dependent)
		{
			c_op->op = tmp;
		}
	}

	for(phi = b->phi_nodes; phi != 0; phi = phi->next)
	{		
		node *tmp;
		ESSL_CHECK(tmp = process_node_pass2(ctx, phi->phi_node));
		phi->phi_node = tmp;
	}

	return MEM_OK;
}




memerr _essl_optimise_cse(mempool *pool, target_descriptor *desc, typestorage_context *ts_ctx, control_flow_graph *cfg)
{

	optimise_cse_context ctx;
	unsigned int i;

	mempool visit_pool;

	ESSL_CHECK(_essl_mempool_init(&visit_pool, 0, _essl_mempool_get_tracker(pool)));
	ctx.pool = pool;
	ctx.cfg = cfg;
	ctx.desc = desc;
	ctx.typestor_ctx = ts_ctx;
	if(!_essl_general_dict_init(&ctx.nodes, &visit_pool, desc, tree_equals, tree_hash)) goto error;
	if(!_essl_ptrdict_init(&ctx.visited, &visit_pool)) goto error;
	for (i = 0 ; i < cfg->n_blocks ; i++) {
		if(!handle_block_pass1(&ctx, cfg->postorder_sequence[i])) goto error;
	}
	if(!_essl_ptrdict_clear(&ctx.visited)) goto error;
	for (i = 0 ; i < cfg->n_blocks ; i++) {
		if(!handle_block_pass2(&ctx, cfg->postorder_sequence[i])) goto error;
	}
	_essl_mempool_destroy(&visit_pool);
	return MEM_OK;
error:
	_essl_mempool_destroy(&visit_pool);
	return MEM_ERROR;
}
