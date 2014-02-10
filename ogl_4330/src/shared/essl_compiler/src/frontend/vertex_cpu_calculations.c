/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#define NEEDS_STDIO  /* for snprintf */

#include "frontend/vertex_cpu_calculations.h"

#include "common/essl_system.h"
#include "common/ptrdict.h"
#include "common/ptrset.h"
#include "common/essl_target.h"
#include "common/essl_node.h"
#include "common/essl_type.h"
#include "common/symbol_table.h"
#include "maligp2/maligp2_instruction.h"

/**
 * Optimization context
 */
typedef struct vscpu_context
{
	mempool *pool;
	mempool temp_pool;
	translation_unit *tu;
	target_descriptor *desc;
	symbol *func;
	ptrdict cpu_attr;
	ptrset visited;
	ptrdict assigns;
	ptrdict copied;
	unsigned num_attrs;
}vscpu_context;

static essl_bool is_supported_arihtmetic(node *op)
{
	essl_bool res = ESSL_FALSE;

	if(op->hdr.type->basic_type == TYPE_INT)
	{
		return res;
	}
	if(op->hdr.kind == EXPR_KIND_BINARY &&
		op->expr.operation == EXPR_OP_MUL)
	{
		if(GET_CHILD(op, 0)->hdr.type->basic_type == TYPE_MATRIX_OF &&
			GET_CHILD(op, 1)->hdr.type->basic_type == TYPE_FLOAT &&
			GET_NODE_VEC_SIZE(GET_CHILD(op, 1)) == 4)
		{
			res = ESSL_TRUE;
		}
	}

	if(op->hdr.kind == EXPR_KIND_BUILTIN_CONSTRUCTOR)
	{
		res = ESSL_TRUE;
	}

	return res;

}

/**
 * weight function 
 */
static unsigned get_node_cpu_weight(node *op)
{
	switch (op->hdr.kind)
	{
	  case EXPR_KIND_PHI:
	  case EXPR_KIND_DONT_CARE:
	  case EXPR_KIND_TRANSFER:
	  case EXPR_KIND_DEPEND:
	  case EXPR_KIND_BUILTIN_CONSTRUCTOR:
	  case EXPR_KIND_CONSTANT:
	  case EXPR_KIND_VARIABLE_REFERENCE:
		return 0;
	  case EXPR_KIND_UNARY:
		switch(op->expr.operation)
		{
		  case EXPR_OP_SWIZZLE:
			return 0;
		  default:
			return 1;
		}
	  case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		switch(op->expr.operation)
		{
		  case EXPR_OP_FUN_CLAMP:
		  case EXPR_OP_FUN_ABS:
		  case EXPR_OP_FUN_TRUNC:
			return 0;
		  default:
			return 1;
		}

	  default:
		return 1;
	}
}

static essl_bool is_node_inputs_recognized_pattern(vscpu_context *ctx, node *op, unsigned *weight, essl_bool *is_attr)
{
	unsigned i;
	essl_bool res = ESSL_TRUE;

	*weight += get_node_cpu_weight(op);
	if(op->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		symbol *sym = _essl_symbol_for_node(op);
		ESSL_CHECK(sym);
		if(sym->address_space == ADDRESS_SPACE_UNIFORM)
		{
			return ESSL_TRUE;
		}else if(sym->address_space == ADDRESS_SPACE_ATTRIBUTE)
		{
			*is_attr = ESSL_TRUE;
			return ESSL_TRUE;
		}else
		{
			node *assign;
			assign = _essl_ptrdict_lookup(&ctx->assigns, sym);
			if(assign != NULL && assign->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
			{
				symbol *sym = _essl_symbol_for_node(assign);
				ESSL_CHECK(sym);
				if(sym->address_space == ADDRESS_SPACE_ATTRIBUTE)
				{
					*is_attr = ESSL_TRUE;
					return ESSL_TRUE;
				}else
				{
					return ESSL_FALSE;
				}
			}else
			{
				return ESSL_FALSE;
			}
		}
	}

	if(op->hdr.kind == EXPR_KIND_CONSTANT)
	{
		return ESSL_TRUE;
	}
	
	if(is_supported_arihtmetic(op) == ESSL_FALSE)
	{
		return ESSL_FALSE;
	}

		/* check all input nodes */
	for(i = 0; i < GET_N_CHILDREN(op); i++)
	{
		node *child = GET_CHILD(op, i);
		res &= is_node_inputs_recognized_pattern(ctx, child, weight, is_attr);
	}
	
	return res;
}

/**
 * analyzing whether a node is an initial candidate 
 *
 * @param ctx optimization context
 * @param n node to analyze
 * 
 * @returns MEM_OK if all allocations were correct
 */
static memerr collect_nodes(vscpu_context *ctx, node *n)
{
	essl_bool is_pattern;
	node *child;
	unsigned weight = 0;
	essl_bool is_attr = ESSL_FALSE;

	if((_essl_ptrset_has(&ctx->visited, n)) == ESSL_TRUE)
	{
		return MEM_OK;
	}
	ESSL_CHECK(_essl_ptrset_insert(&ctx->visited, n));


	if(n->hdr.kind != EXPR_KIND_ASSIGN && n->hdr.kind != DECL_KIND_VARIABLE)
	{
		unsigned i;
		for(i = 0; i < GET_N_CHILDREN(n); i++)
		{
			child = GET_CHILD(n, i);
			if(child != NULL)
			{
				ESSL_CHECK(collect_nodes(ctx, child));
			}
		}
		return MEM_OK;
	}
	if(n->hdr.kind == DECL_KIND_VARIABLE)
	{
		child = GET_CHILD(n, 0);
		if(child == NULL)
		{
			return MEM_OK;
		}
	}else
	{
		ESSL_CHECK(child = GET_CHILD(n, 1));
	}

	is_pattern = is_node_inputs_recognized_pattern(ctx, child, &weight, &is_attr);

	if(is_pattern && weight != 0 && is_attr == ESSL_TRUE)
	{
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->cpu_attr, child, n));
	}else
	{
		ESSL_CHECK(collect_nodes(ctx, child));
	}

	return MEM_OK;

}

static memerr collect_assign_nodes(vscpu_context *ctx, node *n)
{
	node *left;
	node *right;
	symbol *sym;

	if(GET_NODE_KIND(n->hdr.kind) == NODE_KIND_STATEMENT && n != ctx->func->body)
	{
		return MEM_OK;
	}
	if(n->hdr.kind != EXPR_KIND_ASSIGN)
	{
		unsigned i;
		node *child;
		for(i = 0; i < GET_N_CHILDREN(n); i++)
		{
			child = GET_CHILD(n, i);
			if(child != NULL)
			{
				ESSL_CHECK(collect_assign_nodes(ctx, child));
			}
		}
		return MEM_OK;
	}

	ESSL_CHECK(left = GET_CHILD(n, 0));
	if(left->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE)
	{
		return MEM_OK;
	}

	ESSL_CHECK(right = GET_CHILD(n, 1));

	if(right->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE)
	{
		return MEM_OK;
	}

	ESSL_CHECK(sym = _essl_symbol_for_node(right));
	if(sym->address_space != ADDRESS_SPACE_ATTRIBUTE)
	{
		return MEM_OK;
	}

	ESSL_CHECK(sym = _essl_symbol_for_node(left));

	if(_essl_ptrdict_lookup(&ctx->assigns, sym) != NULL)
	{
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->assigns, sym, NULL));
	}else
	{
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->assigns, sym, right));
	}

	return MEM_OK;

}


/**
 * Finding  all initial candidates 
 */
static memerr find_supported_vertex_patterns_for_func(vscpu_context *ctx)
{
	if(ctx->func->body != NULL)
	{
		ESSL_CHECK(collect_nodes(ctx, ctx->func->body));
	}
	return MEM_OK;
}

static memerr collect_assigns(vscpu_context *ctx)
{
	if(ctx->func->body != NULL)
	{
		ESSL_CHECK(collect_assign_nodes(ctx, ctx->func->body));
	}
	return MEM_OK;
}


static void keep_used_symbols(vscpu_context *ctx, node *n)
{
	node *child;
	unsigned i;

	if(n->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		node *assign;
		symbol *sym = n->expr.u.sym;
		if(sym->address_space == ADDRESS_SPACE_UNIFORM || 
			sym->address_space == ADDRESS_SPACE_ATTRIBUTE)
		{
			sym->keep_symbol = ESSL_TRUE;
		}else
		{
			assign = _essl_ptrdict_lookup(&ctx->assigns, sym);
			if(assign != NULL && assign->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
			{
				assert(assign->expr.u.sym->address_space == ADDRESS_SPACE_ATTRIBUTE);
				n->expr.u.sym = assign->expr.u.sym;
				n->expr.u.sym->keep_symbol = ESSL_TRUE;
			}
		
		}
	}


	for(i = 0; i < GET_N_CHILDREN(n); i++)
	{
		child = GET_CHILD(n, i);
		if(child != NULL)
		{
			keep_used_symbols(ctx, child);
		}
	}
	return;
}

/**
 * Copy node
 */
static node * copy_hoisted_node(vscpu_context *ctx, node *n)
{
	unsigned i;
	node *res;
	res = _essl_ptrdict_lookup(&ctx->copied, n);
	if(res != NULL)
	{
		return res;
	}

	ESSL_CHECK(res = _essl_clone_node(ctx->pool, n));
	for(i = 0; i < GET_N_CHILDREN(n); i++)
	{
		node *pred, *copy_pred;
		pred = GET_CHILD(n, i);
		ESSL_CHECK(copy_pred = copy_hoisted_node(ctx, pred));
		SET_CHILD(res, i, copy_pred);
	}
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->copied, n, res));
	return res;
}

static memerr move_calc_to_new_attr(vscpu_context *ctx, node *n, node *succ)
{
	const type_specifier *inp_type;
	string sym_name;
	qualifier_set qual;
	char uni_name[50];
	node *child;
	symbol *sym;
	symbol_list *sym_list;
	node *copy;
	int address;

	assert(ctx->desc->kind == TARGET_VERTEX_SHADER);
	address = -1;
	ESSL_CHECK(child = GET_CHILD(succ, 0));
	if(child->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		symbol *sym;
		const char * name_ptr;
		ESSL_CHECK(sym = _essl_symbol_for_node(child));
		ESSL_CHECK(name_ptr = _essl_string_to_cstring(ctx->pool, sym->name));
		if(strstr(name_ptr, "gl_Position") != NULL)
		{
			snprintf(uni_name, 50, "?__gl_mali_vscpu_attr_gl_Position_%d", ctx->num_attrs);
			address = 0xFF;
		}
		else
		{
			/* applying patterns only for gl_position */
			return MEM_OK;
		}
	}
	else
	{
		/* applying patterns only for gl_position */
		return MEM_OK;
	}

	sym_name = _essl_cstring_to_string(ctx->pool, uni_name);
	ESSL_CHECK(sym_name.ptr);

	_essl_init_qualifier_set(&qual);

	qual.variable = VAR_QUAL_ATTRIBUTE;
	qual.precision = PREC_HIGH;

	inp_type = n->hdr.type;

	/* variable to substitute hoisted out calculations */
	ESSL_CHECK(sym = _essl_new_variable_symbol(ctx->pool, sym_name, inp_type, qual, SCOPE_GLOBAL, ADDRESS_SPACE_ATTRIBUTE, n->hdr.source_offset));
	sym->is_persistent_variable = ESSL_FALSE;
	sym->opt.is_indexed_statically = ESSL_FALSE;
	ESSL_CHECK(sym_list = LIST_NEW(ctx->pool, symbol_list));
	sym_list->sym = sym;
	LIST_INSERT_BACK(&ctx->tu->cpu_processed, sym_list);
	
	ESSL_CHECK(copy = copy_hoisted_node(ctx, n));

	sym->body = copy;
	sym->keep_symbol = ESSL_TRUE;
	sym->address = address;

	keep_used_symbols(ctx, copy);
	if(sym->type->basic_type == TYPE_MATRIX_OF)
	{
		ctx->num_attrs += GET_MATRIX_N_COLUMNS(sym->type);
	}else
	{
		ctx->num_attrs++;
	}

	return MEM_OK;
}

static memerr move_calculations_to_cpu(vscpu_context *ctx)
{
	ptrdict_iter it;
	node *n_attr;
	node *assgn;

	_essl_ptrdict_iter_init(&it, &ctx->cpu_attr);
	while((n_attr = _essl_ptrdict_next(&it, (void_ptr *)(void*)&assgn)) != NULL)
	{
		ESSL_CHECK(move_calc_to_new_attr(ctx, n_attr, assgn));
	}

	return MEM_OK;	
}

/**
 */
static memerr find_supported_vertex_patterns(vscpu_context *ctx, symbol *function)
{
	ctx->func = function;

	ESSL_CHECK(collect_assigns(ctx));	
	ESSL_CHECK(find_supported_vertex_patterns_for_func(ctx));

	return MEM_OK;
}

/**
 * Driver function for the optimization
 * To be called from middle passes run.
 */
memerr _essl_move_vertex_calculations_to_cpu(mempool *pool,  translation_unit* tu)
{
	vscpu_context ctx;
	symbol_list *sl;
	unsigned num_attr = 0;


	for(sl = tu->vertex_attributes; sl != NULL; sl = sl->next)
	{
		/* consider all attributes as vec4 for simplicity */
		if(sl->sym->type->basic_type == TYPE_MATRIX_OF)
		{
			num_attr += GET_MATRIX_N_COLUMNS(sl->sym->type);
		}else
		{
			num_attr++;
		}
	}

	ctx.pool = pool;
	ctx.desc = tu->desc;
	ctx.tu = tu;
	ctx.num_attrs = num_attr;

	ESSL_CHECK(_essl_mempool_init(&ctx.temp_pool, 0, _essl_mempool_get_tracker(pool)));


	if(_essl_ptrdict_init(&ctx.cpu_attr, &ctx.temp_pool) == MEM_ERROR)
	{
		_essl_mempool_destroy(&ctx.temp_pool);
		return MEM_ERROR;
	}

	if(_essl_ptrdict_init(&ctx.copied, &ctx.temp_pool) == MEM_ERROR)
	{
		_essl_mempool_destroy(&ctx.temp_pool);
		return MEM_ERROR;
	}

	if(_essl_ptrdict_init(&ctx.assigns, &ctx.temp_pool) == MEM_ERROR)
	{
		_essl_mempool_destroy(&ctx.temp_pool);
		return MEM_ERROR;
	}

	if(_essl_ptrset_init(&ctx.visited, &ctx.temp_pool) == MEM_ERROR)
	{
		_essl_mempool_destroy(&ctx.temp_pool);
		return MEM_ERROR;
	}


	for(sl = tu->functions; sl != 0; sl = sl->next)
	{
		if(find_supported_vertex_patterns(&ctx, sl->sym) == MEM_ERROR)
		{
			_essl_mempool_destroy(&ctx.temp_pool);
			return MEM_ERROR;
		}
	}

	if(move_calculations_to_cpu(&ctx) == MEM_ERROR)
	{
		_essl_mempool_destroy(&ctx.temp_pool);
		return MEM_ERROR;
	}

	_essl_mempool_destroy(&ctx.temp_pool);
	return MEM_OK;

}

