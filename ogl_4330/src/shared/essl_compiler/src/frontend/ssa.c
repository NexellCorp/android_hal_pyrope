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
#include "common/essl_list.h"
#include "common/symbol_table.h"
#include "common/ptrdict.h"
#include "common/general_dict.h"
#include "common/error_reporting.h"
#include "common/essl_mem.h"
#include "common/basic_block.h"

#include "frontend/ssa.h"

/*
	This module implements algorithms that are based on
	"Efficiently computing static single assignment form and the control dependence graph"
	Ron Cytron, Jeanne Ferrante, Barry K. Rosen, Mark N. Wegman, F. Kenneth Zadeck
	
	http://portal.acm.org/citation.cfm?id=115320


*/




/** Code to retrieve the source offset for this module (not reported) */
#define SOURCE_OFFSET 0

/** Code to access the error context for this module */
#define ERROR_CONTEXT ctx->err_context

typedef struct _tag_value_list {
	struct _tag_value_list *next;
	/*@null@*/ node *value;

} value_list;
ASSUME_ALIASING(value_list, generic_list);

typedef struct {
	/*@null@*/ value_list *lst;
} node_stack;


/*@null@*/ static node *node_stack_top(node_stack *s)
{
	if(s->lst == 0) return 0;
	return s->lst->value;

}

/*@null@*/ static node *node_stack_pop(node_stack *s)
{
	node *v;
	ESSL_CHECK(s->lst);
	v = s->lst->value;
	s->lst = s->lst->next;
	return v;
}


/*@null@*/ static node *node_stack_push(node_stack *s, mempool *pool, node *value)
{
	value_list *l = _essl_mempool_alloc(pool, sizeof(*l));
	ESSL_CHECK(l);
	l->next = 0;
	l->value = value;
	LIST_INSERT_FRONT(&s->lst, l);
	return value;
}


typedef struct {
	error_context *err_context;
	mempool *pool;
	ptrset visited;
	ptrdict value_stacks;
	ptrdict used_phi_nodes;
	general_dict node_to_sym;
	ptrset *dominator_tree;
} rename_context;

/*@null@*/ static node *node_stack_get_or_create_top(rename_context *ctx, node_stack *s, const type_specifier *t)
{
	node *res;
	assert(t->basic_type != TYPE_STRUCT);
	res = node_stack_top(s);
	if(res != NULL) return res;
	/* nope, we need to synthesize a don't care */
	ESSL_CHECK(res = _essl_new_dont_care_expression(ctx->pool, t));
	ESSL_CHECK(node_stack_push(s, ctx->pool, res));
	return res;
}



static symbol *lookup_symbol(rename_context *ctx, node *n)
{
	symbol *s = NULL;
	s = _essl_general_dict_lookup(&ctx->node_to_sym, n);
	if(s == NULL && n->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		s = n->expr.u.sym;
	}
	return s;
}

static node *clone_address(mempool *pool, node *address)
{
	node *clone;
	unsigned i;
	assert((address->hdr.kind == EXPR_KIND_UNARY && address->expr.operation == EXPR_OP_MEMBER) || 
			(address->hdr.kind == EXPR_KIND_BINARY && address->expr.operation == EXPR_OP_INDEX) ||
			address->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE ||
			address->hdr.kind == EXPR_KIND_CONSTANT);
	ESSL_CHECK(clone = _essl_clone_node(pool, address));
	for(i = 0; i < GET_N_CHILDREN(clone); ++i)
	{
		node *child = GET_CHILD(clone, i);
		if(child != NULL)
		{
			ESSL_CHECK(child = clone_address(pool, child));
			SET_CHILD(clone, i, child);
		}
	}
	return clone;
}

static symbol *create_dummy_symbol(mempool *pool, general_dict *node_to_sym, node *address)
{
	symbol *sym;
	node *cloned_address;
	string name = {"<struct_elim_var>", 17};
	ESSL_CHECK(sym = _essl_new_variable_symbol_with_default_qualifiers(pool, name, address->hdr.type, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));
	ESSL_CHECK(cloned_address = clone_address(pool, address));
	ESSL_CHECK(_essl_general_dict_insert(node_to_sym, cloned_address, sym));
	return sym;
}


/*@null@*/ static node_stack *node_stack_get(rename_context *ctx, symbol *var)
{
	return (node_stack *)_essl_ptrdict_lookup(&ctx->value_stacks, var);
}

/*@null@*/ static node_stack *node_stack_node_get(rename_context *ctx, node *n)
{
	symbol *sym;
	ESSL_CHECK(sym = lookup_symbol(ctx, n));
	return node_stack_get(ctx, sym);
}

/*@null@*/ static node_stack *node_stack_get_or_create(rename_context *ctx, symbol *var)
{
	node_stack *s;
	s = (node_stack *)_essl_ptrdict_lookup(&ctx->value_stacks, var);
	if(s == 0)
	{
		s = _essl_mempool_alloc(ctx->pool, sizeof(*s));
		ESSL_CHECK(s);
		s->lst = 0;
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->value_stacks, var, s));
	}
	return s;
}

/*@null@*/ static node_stack *node_stack_node_get_or_create(rename_context *ctx, node *n)
{
	symbol *sym;
	sym = lookup_symbol(ctx, n);
	if(sym == NULL)
	{
		ESSL_CHECK(sym = create_dummy_symbol(ctx->pool, &ctx->node_to_sym, n));
	}
	return node_stack_get_or_create(ctx, sym);
}

static memerr ssa_rename(rename_context *ctx, basic_block *b);
static memerr visit_children(rename_context *ctx, basic_block *curr)
{
	/* visit the children of dom in the dominator tree */

	basic_block *c;
	ptrset_iter it;
	_essl_ptrset_iter_init(&it, &ctx->dominator_tree[curr->postorder_visit_number]);
	while( (c = _essl_ptrset_next(&it)) != NULL )
	{
		if(_essl_ptrset_has(&ctx->visited, c) == 0)
		{
			ESSL_CHECK(ssa_rename(ctx, c));
		}	
	}
	return MEM_OK;
}


static essl_bool var_equal_fun(target_descriptor *desc, const void *va, const void *vb)
{
	const node *a = va;
	const node *b = vb;
	assert(a != NULL && b != NULL);
	assert((a->hdr.kind == EXPR_KIND_UNARY && a->expr.operation == EXPR_OP_MEMBER) || 
			(a->hdr.kind == EXPR_KIND_BINARY && a->expr.operation == EXPR_OP_INDEX) ||
			a->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE);
	assert((b->hdr.kind == EXPR_KIND_UNARY && b->expr.operation == EXPR_OP_MEMBER) || 
			(b->hdr.kind == EXPR_KIND_BINARY && b->expr.operation == EXPR_OP_INDEX) ||
			b->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE);
	if(a->hdr.kind != b->hdr.kind) return ESSL_FALSE;
	if(a->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		return a->expr.u.sym == b->expr.u.sym;
	}
	if(a->hdr.kind == EXPR_KIND_UNARY && a->expr.operation == EXPR_OP_MEMBER && b->expr.operation == EXPR_OP_MEMBER)
	{
		if(a->expr.u.member != b->expr.u.member) return ESSL_FALSE;
		return var_equal_fun(desc, GET_CHILD(a, 0), GET_CHILD(b, 0));

	}

	if(a->hdr.kind == EXPR_KIND_BINARY && a->expr.operation == EXPR_OP_INDEX && b->expr.operation == EXPR_OP_INDEX)
	{
		node *ac = GET_CHILD(a, 1);
		node *bc = GET_CHILD(b, 1);
		unsigned int i;
		if(!_essl_node_is_constant(ac) || !_essl_node_is_constant(bc))
		{
			return ESSL_FALSE;
		}

		if(GET_NODE_VEC_SIZE(ac) != GET_NODE_VEC_SIZE(bc))
		{
			return ESSL_FALSE;
		}
		for(i = 0 ; i < GET_NODE_VEC_SIZE(ac); i++)
		{
			if(desc->scalar_to_int( ac->expr.u.value[i]) != desc->scalar_to_int( bc->expr.u.value[i]))
			{
				return ESSL_FALSE;
			}
		}
		return var_equal_fun(desc, GET_CHILD(a, 0), GET_CHILD(b, 0));

	}

	assert(0);
	return ESSL_FALSE;
}

static general_dict_hash_type var_hash_fun(target_descriptor *desc, const void *vn)
{
	general_dict_hash_type h = 1337;
	const node *n = vn;
	while((n->hdr.kind == EXPR_KIND_UNARY && n->expr.operation == EXPR_OP_MEMBER) ||
		  (n->hdr.kind == EXPR_KIND_BINARY && n->expr.operation == EXPR_OP_INDEX) )
	{
		if(n->hdr.kind == EXPR_KIND_UNARY && n->expr.operation == EXPR_OP_MEMBER)
		{
			while(n->hdr.kind == EXPR_KIND_UNARY && n->expr.operation == EXPR_OP_MEMBER)
			{
				h = h*31337 + (general_dict_hash_type)n->expr.u.member;
				n = GET_CHILD(n, 0);
			}
		}else if(n->hdr.kind == EXPR_KIND_BINARY && n->expr.operation == EXPR_OP_INDEX)
		{
			while(n->hdr.kind == EXPR_KIND_BINARY && n->expr.operation == EXPR_OP_INDEX)
			{
				node *ch1 = GET_CHILD(n, 1);
				assert(ch1->hdr.kind == EXPR_KIND_CONSTANT && GET_NODE_VEC_SIZE(ch1) == 1);
				h = h*31337 + (general_dict_hash_type)desc->scalar_to_int( ch1->expr.u.value[0]);
				n = GET_CHILD(n, 0);
			}
		}else
		{
			assert(n->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE);
		}
	}
	assert(n->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE);
	h = h*31337 + (general_dict_hash_type)n->expr.u.sym;
	return h;
}

static memerr ssa_rename(rename_context *ctx, basic_block *b)
{
	local_operation *a;
	node *tmp;
	unsigned i;
	node_stack *s = 0;
	
	phi_list *phi;
	ESSL_CHECK(_essl_ptrset_insert(&ctx->visited, b));

	for(phi = b->phi_nodes; phi != 0; phi = phi->next)
	{
		if(phi->sym == 0) continue;
		s = node_stack_get_or_create(ctx, phi->sym);
		ESSL_CHECK_OOM(s);
		tmp = node_stack_push(s, ctx->pool, phi->phi_node);
		ESSL_CHECK_OOM(tmp);
	}


	for(a = b->local_ops; a != 0; a = a->next)
	{
		node *n = a->op;
		node *child1;
		switch(n->hdr.kind)
		{
		case EXPR_KIND_ASSIGN:
			child1 = GET_CHILD(n, 1);
			assert(child1 != 0);
			s = node_stack_node_get_or_create(ctx, GET_CHILD(n, 0));
			ESSL_CHECK_OOM(s);
			tmp = node_stack_push(s, ctx->pool, child1);
			ESSL_CHECK_OOM(tmp);
			break;
		case EXPR_KIND_VARIABLE_REFERENCE:
			s = node_stack_get_or_create(ctx, n->expr.u.sym);
			ESSL_CHECK_OOM(s);
			ESSL_CHECK_OOM(tmp = node_stack_get_or_create_top(ctx, s, n->hdr.type));
			/* OPT: Enable these warnings but only in sensible situations.
			if(tmp->hdr.kind == EXPR_KIND_DONT_CARE && n->expr.u.sym->address_space == ADDRESS_SPACE_THREAD_LOCAL)
			{
				const char *nm;
				ESSL_CHECK(nm = _essl_string_to_cstring(ctx->pool, n->expr.u.sym->name));
				(void)REPORT_WARNING_NODE(ctx->err_context, ERR_WARNING, n, "Use of unitialized variable \"%s\"\n", nm);
			}
			*/
			if(tmp->hdr.kind == EXPR_KIND_PHI)
			{
				long curr_count = (long)_essl_ptrdict_lookup(&ctx->used_phi_nodes, tmp);
				
				ESSL_CHECK(_essl_ptrdict_insert(&ctx->used_phi_nodes, tmp, (void*)(curr_count + 1)));

			}

			_essl_rewrite_node_to_transfer(n, tmp);

			break;
		case EXPR_KIND_UNARY:
			assert(n->expr.operation == EXPR_OP_MEMBER);
			assert(GET_CHILD(n, 0) != 0);
			s = node_stack_node_get_or_create(ctx, n);
			ESSL_CHECK_OOM(s);
			ESSL_CHECK_OOM(tmp = node_stack_get_or_create_top(ctx, s, n->hdr.type));
			/* OPT: Enable these warnings but only in sensible situations.
			if(tmp->hdr.kind == EXPR_KIND_DONT_CARE && n->expr.u.sym->address_space == ADDRESS_SPACE_THREAD_LOCAL)
			{
				const char *nm;
				ESSL_CHECK(nm = _essl_string_to_cstring(ctx->pool, n->expr.u.sym->name));
				(void)REPORT_WARNING_NODE(ctx->err_context, ERR_WARNING, n, "Use of unitialized variable \"%s\"\n", nm);
			}
			*/
			if(tmp->hdr.kind == EXPR_KIND_PHI)
			{
				long curr_count = (long)_essl_ptrdict_lookup(&ctx->used_phi_nodes, tmp);
				
				ESSL_CHECK(_essl_ptrdict_insert(&ctx->used_phi_nodes, tmp, (void*)(curr_count + 1)));

			}

			/* Rewrite the deref as a transfer */
			_essl_rewrite_node_to_transfer(n, tmp);

			break;

		case EXPR_KIND_BINARY:
			assert(n->expr.operation == EXPR_OP_INDEX);
			assert(GET_CHILD(n, 0) != 0);
			s = node_stack_node_get_or_create(ctx, n);
			ESSL_CHECK_OOM(s);
			ESSL_CHECK_OOM(tmp = node_stack_get_or_create_top(ctx, s, n->hdr.type));
			/* OPT: Enable these warnings but only in sensible situations.
			if(tmp->hdr.kind == EXPR_KIND_DONT_CARE && n->expr.u.sym->scope == SCOPE_LOCAL)
			{
				const char *nm;
				ESSL_CHECK(nm = _essl_string_to_cstring(ctx->pool, n->expr.u.sym->name));
				(void)REPORT_WARNING_NODE(ctx->err_context, ERR_WARNING, n, "Use of unitialized variable \"%s\"\n", nm);
			}
			*/
			if(tmp->hdr.kind == EXPR_KIND_PHI)
			{
				long curr_count = (long)_essl_ptrdict_lookup(&ctx->used_phi_nodes, tmp);

				ESSL_CHECK(_essl_ptrdict_insert(&ctx->used_phi_nodes, tmp, (void*)(curr_count + 1)));

			}

			/* Rewrite the deref as a transfer */
			_essl_rewrite_node_to_transfer(n, tmp);

			break;

		default:
			assert(0); /*Should only get pure assignments and variable references here */
			break;
		}

	}

	
	for(i = 0; i < b->n_successors; ++i)
	{
		basic_block *c = b->successors[i];
		for(phi = c->phi_nodes; phi != 0; phi = phi->next)
		{
			phi_source *src = 0;
			node *s_node = 0;
			if(phi->sym == 0) continue;
			ESSL_CHECK_OOM(s = node_stack_get_or_create(ctx, phi->sym));
			ESSL_CHECK_OOM(s_node = node_stack_get_or_create_top(ctx, s, phi->sym->type));

			if(s_node->hdr.kind == EXPR_KIND_PHI)
			{
				long curr_count = (long)_essl_ptrdict_lookup(&ctx->used_phi_nodes, s_node);
				
				ESSL_CHECK(_essl_ptrdict_insert(&ctx->used_phi_nodes, s_node, (void*)(curr_count + 1)));

			}
			src = LIST_NEW(ctx->pool, phi_source);
			ESSL_CHECK_OOM(src);
			src->join_block = b;
			src->source = s_node;
			LIST_INSERT_FRONT(&phi->phi_node->expr.u.phi.sources, src);
		}

	}


	ESSL_CHECK(visit_children(ctx, b));

	for(a = b->local_ops; a != 0; a = a->next)
	{
		node *n = a->op;
		if(n->hdr.kind == EXPR_KIND_ASSIGN)
		{
			s = node_stack_node_get(ctx, GET_CHILD(n, 0));
			assert(s != 0);
			tmp = node_stack_pop(s);
			assert(tmp != 0);

		}
	}
	
	for(phi = b->phi_nodes; phi != 0; phi = phi->next)
	{
		if(phi->sym)
		{
			s = node_stack_get(ctx, phi->sym);
			assert(s != 0);
			tmp = node_stack_pop(s);
			assert(tmp != 0);
		}
	}


	return MEM_OK;

}
typedef struct {
	mempool *pool;
	ptrset visited;
	general_dict *node_to_sym;
} phi_insert_context;

/*@null@*/ static phi_list *find_phi_node(basic_block *b, symbol *sym)
{
	phi_list *phi;
	for(phi = b->phi_nodes; phi != 0; phi = phi->next)
	{
		if(phi->sym == sym) return phi;
	}
	return 0;

}

static memerr insert_phi_node(phi_insert_context *ctx, basic_block *b, symbol *sym, int recursive)
{
	if(!find_phi_node(b, sym))
	{
		/* insert stuff */
		phi_list *lst = LIST_NEW(ctx->pool, phi_list);
		node *phi_node = _essl_new_phi_expression(ctx->pool, b);
		ESSL_CHECK(lst);
		ESSL_CHECK(phi_node);
		lst->sym = sym;
		lst->phi_node = phi_node;
		phi_node->hdr.type = sym->type;
		LIST_INSERT_FRONT(&b->phi_nodes, lst);

	}
	if(recursive && _essl_ptrset_has(&ctx->visited, b))
	{
		/* okay, we already did this node, so we need to redo the parts changed by the phi statement we just inserted */
		ptrset_iter it;
		basic_block *c = 0;

		for(_essl_ptrset_iter_init(&it, &b->dominance_frontier); (c = _essl_ptrset_next(&it)) != 0;)
		{
			
			ESSL_CHECK(insert_phi_node(ctx, c, sym, (int)(b != c)));
		}
	}
	
	return MEM_OK;
}

static memerr insert_phi_for_node(phi_insert_context *ctx, basic_block *b, node *address)
{
	symbol *sym;
	if(address->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		ESSL_CHECK(sym = _essl_symbol_for_node(address));
	} else {
		
		assert((address->hdr.kind == EXPR_KIND_UNARY && address->expr.operation == EXPR_OP_MEMBER) ||
				(address->hdr.kind == EXPR_KIND_BINARY && address->expr.operation == EXPR_OP_INDEX));
		sym = _essl_general_dict_lookup(ctx->node_to_sym, address);
		if(sym == NULL)
		{
			ESSL_CHECK(sym = create_dummy_symbol(ctx->pool, ctx->node_to_sym, address));
		}
	}
	assert(sym != NULL);
	return insert_phi_node(ctx, b, sym, 1);

}

static memerr create_phi_nodes(mempool *pool, control_flow_graph *fb, general_dict *node_to_sym)
{
	basic_block *b;
	phi_insert_context context, *ctx = &context;
	ctx->pool = pool;
	ctx->node_to_sym = node_to_sym;
	ESSL_CHECK(_essl_ptrset_init(&ctx->visited, pool));
	for(b = fb->entry_block; b != 0; b = b->next)
	{
		ptrset_iter it;
		basic_block *c = 0;
		ESSL_CHECK(_essl_ptrset_insert(&ctx->visited, b));
		for(_essl_ptrset_iter_init(&it, &b->dominance_frontier); (c = _essl_ptrset_next(&it)) != 0;)
		{
			local_operation *op = 0;
			phi_list *phi = 0;
			for(phi = b->phi_nodes; phi != 0; phi = phi->next)
			{
				if(phi->sym)
				{
					ESSL_CHECK(insert_phi_node(ctx, c, phi->sym, 1));
				}
			}

			for(op = b->local_ops; op != 0; op = op->next)
			{
				node *n = op->op;
				if(n->hdr.kind == EXPR_KIND_ASSIGN)
				{
					ESSL_CHECK(insert_phi_for_node(ctx, c, GET_CHILD(n, 0)));
				}
			}
		}
	}


	return MEM_OK;
}


static memerr prune_phi_nodes(control_flow_graph *cfg, ptrdict *used_phi_nodes)
{
	basic_block *b;
	essl_bool did_change = ESSL_TRUE;
	while(did_change)
	{
		did_change = ESSL_FALSE;
		for(b = cfg->entry_block; b != 0; b = b->next)
		{
			phi_list **phi;
			for(phi = &b->phi_nodes; *phi != 0; )
			{
				/* phi nodes made by make_basic_blocks haven't had all their uses flagged, so we won't eliminate them. test if this is so by examining the phi symbol */
				long use_count = (long)_essl_ptrdict_lookup(used_phi_nodes, (*phi)->phi_node);
				if((*phi)->sym == 0 || use_count > 0)
				{
					phi = &(*phi)->next;
				} else {
					phi_source *ps;
					/* decrement the use count of the source phi nodes, if any */
					for(ps = (*phi)->phi_node->expr.u.phi.sources; ps != 0; ps = ps->next)
					{
						if(ps->source->hdr.kind == EXPR_KIND_PHI)
						{
							long curr_count = (long)_essl_ptrdict_lookup(used_phi_nodes, ps->source);
				
							ESSL_CHECK(_essl_ptrdict_insert(used_phi_nodes, ps->source, (void*)(curr_count - 1)));


						}

					}
					*phi = (*phi)->next; /* delete the node */
					did_change = ESSL_TRUE;
					
				}
			}
		}
	}


	return MEM_OK;

}


/*
  calculate the dominance tree by placing every block into the set of its immediate dominator.
  returns a ptrset array of sets per block in postorder sequence.
*/
static ptrset *calculate_dominator_tree(mempool *pool, control_flow_graph *g)
{
	ptrset *tree;
	unsigned n_blocks = g->n_blocks;
	unsigned i;
	ESSL_CHECK(tree = _essl_mempool_alloc(pool, sizeof(ptrset)*n_blocks));

	for(i = 0; i < n_blocks; ++i)
	{
		ESSL_CHECK(_essl_ptrset_init(&tree[i], pool));
	}

	for(i = 0; i < n_blocks; ++i)
	{
		basic_block *b = g->postorder_sequence[i];
		if(b->immediate_dominator != NULL)
		{
			ESSL_CHECK(_essl_ptrset_insert(&tree[b->immediate_dominator->postorder_visit_number], b));
		}
	}	

	return tree;
}


memerr _essl_ssa_transform(mempool *pool, target_descriptor *desc, error_context *err, symbol *function)
{
	rename_context ctx;
	control_flow_graph *g = function->control_flow_graph;
	ESSL_CHECK(g);
	ctx.err_context = err;
	ctx.pool = pool;
	ESSL_CHECK(_essl_ptrdict_init(&ctx.value_stacks, pool));
	ESSL_CHECK(_essl_general_dict_init(&ctx.node_to_sym, pool, desc, var_equal_fun, var_hash_fun));
	ESSL_CHECK(_essl_ptrset_init(&ctx.visited, pool));
	ESSL_CHECK(_essl_ptrdict_init(&ctx.used_phi_nodes, pool));
	ESSL_CHECK(ctx.dominator_tree = calculate_dominator_tree(pool, g));

	ESSL_CHECK(create_phi_nodes(pool, g, &ctx.node_to_sym));
	ESSL_CHECK(ssa_rename(&ctx, g->entry_block));
	ESSL_CHECK(prune_phi_nodes(g, &ctx.used_phi_nodes));
	return MEM_OK;

}
