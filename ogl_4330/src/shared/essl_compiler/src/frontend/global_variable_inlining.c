/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#define NEEDS_STDIO  /* for snprintf */

#include "common/essl_system.h"
#include "frontend/global_variable_inlining.h"
#include "frontend/callgraph.h"
#include "common/ptrdict.h"
#include "common/symbol_table.h"

typedef struct
{
	mempool *pool;
	ptrset *vars_to_inline;
	ptrset *visited;
	unsigned var_count;
	translation_unit *tu;
} global_variable_inline_context;



typedef enum {
	GLOBAL_VARIABLE_TO_LOCAL_VARIABLE,
	GLOBAL_VARIABLE_TO_PARAMETER
} global_variable_rewrite_method;

static memerr create_local_symbols(global_variable_inline_context *ctx, symbol *function, global_variable_rewrite_method rewrite_method, ptrdict *global_var_to_local_var)
{
	/* first create new symbols for the global variables */
	ptrset_iter it;
	symbol *g_var, *l_var;
	scope_kind scope = SCOPE_UNKNOWN;
	node *compound;

	compound = function->body;
	assert(GET_NODE_KIND(compound->hdr.kind) == NODE_KIND_STATEMENT);
	assert(compound->stmt.child_scope != NULL);

	switch(rewrite_method)
	{
	case GLOBAL_VARIABLE_TO_PARAMETER:
		scope = SCOPE_FORMAL;
		break;
	case GLOBAL_VARIABLE_TO_LOCAL_VARIABLE:
		scope = SCOPE_LOCAL;
		break;
	}
	_essl_ptrset_iter_init(&it, ctx->vars_to_inline);
	while((g_var = _essl_ptrset_next(&it)) != NULL)
	{
		char buf[100] = {0};
		string name;
		single_declarator *sd;
		node *decl;
		const char *old_name;
		qualifier_set new_qualifier;
		_essl_init_qualifier_set(&new_qualifier);
		new_qualifier.direction = DIR_INOUT;
		ESSL_CHECK(old_name = _essl_string_to_cstring(ctx->pool, g_var->name));
		snprintf(buf, 100, "?inlined_global_var_%d_%s", ctx->var_count++, old_name);
		buf[100-1] = '\0';
		name = _essl_cstring_to_string(ctx->pool, buf);
		ESSL_CHECK(name.ptr);
		ESSL_CHECK(l_var = _essl_new_variable_symbol(ctx->pool, name, g_var->type, g_var->qualifier, scope, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));
		l_var->opt.is_indexed_statically = g_var->opt.is_indexed_statically;
		ESSL_CHECK(_essl_symbol_scope_insert(compound->stmt.child_scope, l_var->name, l_var));
		ESSL_CHECK(_essl_ptrdict_insert(global_var_to_local_var, g_var, l_var));

		switch(rewrite_method)
		{
		case GLOBAL_VARIABLE_TO_PARAMETER:
			
			ESSL_CHECK(sd = _essl_new_single_declarator(ctx->pool, l_var->type, new_qualifier, &l_var->name, NULL, UNKNOWN_SOURCE_OFFSET));
			sd->sym = l_var;
			LIST_INSERT_BACK(&function->parameters, sd);
			break;
		case GLOBAL_VARIABLE_TO_LOCAL_VARIABLE:
			ESSL_CHECK(decl = _essl_new_variable_declaration(ctx->pool, l_var, NULL));
			decl->hdr.type = l_var->type;
			ESSL_CHECK(PREPEND_CHILD(compound, decl, ctx->pool));
			break;
		}
			
	}
	return MEM_OK;
}

static memerr find_and_rewrite_nodes(global_variable_inline_context *ctx, ptrdict *global_var_to_local_var, node *n)
{
	unsigned i;
	symbol *l_var, *g_var;
	node *var_ref;
	ptrset_iter it;
	if(n == NULL) return MEM_OK;

	switch(n->hdr.kind)
	{
	case EXPR_KIND_VARIABLE_REFERENCE:
		if( (l_var = _essl_ptrdict_lookup(global_var_to_local_var, n->expr.u.sym)) != NULL)
		{
			n->expr.u.sym = l_var;
		}
		break;
	case EXPR_KIND_FUNCTION_CALL:
		/* if(!n->expr.u.sym->inlined) break; */
		_essl_ptrset_iter_init(&it, ctx->vars_to_inline);
		while((g_var = _essl_ptrset_next(&it)) != NULL)
		{
			l_var = _essl_ptrdict_lookup(global_var_to_local_var, g_var);
			assert(l_var != NULL); /* should not fail */
			ESSL_CHECK(var_ref = _essl_new_variable_reference_expression(ctx->pool, l_var));
			var_ref->hdr.type = l_var->type;
			ESSL_CHECK(APPEND_CHILD(n, var_ref, ctx->pool));
			
		}

		break;

	default:
		break;
	}
	for(i = 0; i < GET_N_CHILDREN(n); ++i)
	{
		node *child = GET_CHILD(n, i);
		if(child != NULL)
		{
			ESSL_CHECK(find_and_rewrite_nodes(ctx, global_var_to_local_var, child));
		}

	}
	return MEM_OK;
}


static memerr rewrite_function(global_variable_inline_context *ctx, symbol *function, global_variable_rewrite_method rewrite_method)
{
	ptrdict d, *global_var_to_local_var = &d;
	ESSL_CHECK(_essl_ptrdict_init(global_var_to_local_var, ctx->pool));

	ESSL_CHECK(create_local_symbols(ctx, function, rewrite_method, global_var_to_local_var));

	ESSL_CHECK(find_and_rewrite_nodes(ctx, global_var_to_local_var, function->body));

	return MEM_OK;
}


static memerr visit_function(global_variable_inline_context *ctx, symbol *function)
{
	call_graph *cg;
	global_variable_rewrite_method rewrite_method = GLOBAL_VARIABLE_TO_PARAMETER;
	if(_essl_ptrset_has(ctx->visited, function)) return MEM_OK;
	ESSL_CHECK(_essl_ptrset_insert(ctx->visited, function));
	if(function == ctx->tu->entry_point)
	{
		rewrite_method = GLOBAL_VARIABLE_TO_LOCAL_VARIABLE;
	}
	ESSL_CHECK(rewrite_function(ctx, function, rewrite_method));
	for(cg = function->calls_to; cg != NULL; cg = cg->next)
	{
		/* if(cg->func->inline_function) */
		{
			ESSL_CHECK(visit_function(ctx, cg->func));
		}

	}
	return MEM_OK;
}


static memerr remove_global_variables(global_variable_inline_context *ctx, translation_unit *tu)
{
	symbol_list **sl;
	unsigned i;
	node *n = tu->root;
	for(sl = &tu->globals; *sl != NULL;  )
	{
		if(_essl_ptrset_has(ctx->vars_to_inline, (*sl)->sym))
		{
			*sl = (*sl)->next;
		} else {
			sl = &(*sl)->next;
		}
	}

	for(i = 0; i < GET_N_CHILDREN(n);  )
	{
		node *child = GET_CHILD(n, i);
		if(child != NULL && child->hdr.kind == DECL_KIND_VARIABLE &&_essl_ptrset_has(ctx->vars_to_inline, child->decl.sym))
		{
			memmove(n->hdr.children+i, n->hdr.children+i+1, (GET_N_CHILDREN(n)-i-1)*sizeof(nodeptr));
			ESSL_CHECK(SET_N_CHILDREN(n, GET_N_CHILDREN(n)-1, ctx->pool));

		} else {
			++i;
		}

	}
	return MEM_OK;

}

memerr _essl_inline_global_variables(mempool *pool, translation_unit *tu, ptrset *vars_to_inline)
{
	global_variable_inline_context context, *ctx = &context;
	ptrset visited;
	ESSL_CHECK(_essl_ptrset_init(&visited, pool));
	ctx->pool = pool;
	ctx->vars_to_inline = vars_to_inline;
	ctx->var_count = 0;
	ctx->tu = tu;
	ctx->visited = &visited;
	ESSL_CHECK(visit_function(ctx, tu->entry_point));
	ESSL_CHECK(remove_global_variables(ctx, tu));

	/* _essl_debug_print_node(ctx->pool, tu->root, 0); */
	return MEM_OK;
}

