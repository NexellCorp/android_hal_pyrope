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
#include "common/essl_mem.h"
#include "common/ptrdict.h"
#include "common/basic_block.h"
#include "middle/control_deps.h"
#include "common/symbol_table.h"

#define UNKNOWN_OFFSET (~0ULL)

static memerr add_dependency(mempool *pool, control_dependent_operation *a, control_dependent_operation *b)
{
	operation_dependency *dep;
	ESSL_CHECK(dep = LIST_NEW(pool, operation_dependency));

	dep->dependent_on = a;
	LIST_INSERT_BACK(&b->dependencies, dep);

	return MEM_OK;
}



static essl_bool is_global_op(node *n)
{
	return n->hdr.kind == EXPR_KIND_FUNCTION_CALL;
}

static essl_bool is_dependent_op(node *n)
{
	return n->hdr.kind == EXPR_KIND_LOAD || n->hdr.kind == EXPR_KIND_STORE || is_global_op(n);
}

/* different from _essl_symbol_for_node in that it strictly accepts only LIR address expressions */
static node *symbol_node_for_node(node *n)
{
	while(n!=0 && n->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE && GET_N_CHILDREN(n) > 0)
	{
		n = GET_CHILD(n, 0);
		if(n != 0)
		{
			/* if the child isn't a variable reference, member or index, this is a pointer expression, so stop */
			if( n->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE &&
				(n->hdr.kind != EXPR_KIND_UNARY || (n->expr.operation != EXPR_OP_MEMBER)) &&
				(n->hdr.kind != EXPR_KIND_BINARY || n->expr.operation != EXPR_OP_INDEX) 
				)
			{
				return NULL;
			}
		}
	}


	if(n == NULL) return NULL;
	if(n->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		assert(n->expr.u.sym != NULL);
		return n;

	}
	return NULL;

}

static symbol *symbol_for_node(node *n)
{
	node *s;

	s = symbol_node_for_node(n);
	if( s!= NULL )
	{
		return s->expr.u.sym;
	}
	return NULL;

}

static unsigned long long calc_offset_from_struct_start(single_declarator *mem)
{
	unsigned long long res = 0;
	const type_specifier *strukt;
	single_declarator *field;

	strukt = mem->parent_type;
	assert(strukt->basic_type == TYPE_STRUCT);
	field = strukt->members;
	while(field->index != mem->index)
	{
		res +=_essl_get_type_size(field->type);
		field = field->next;
	}

	return res;
}

/**
   Check if the memory operation given by node b depends on the operation given by node a
 */

static unsigned long long calc_memory_offset(target_descriptor *desc, node *a, node *a_sym)
{
	unsigned long long res = 0;
	node *a_addr;
	a_addr = GET_CHILD(a, 0);
	while(a_addr != a_sym)
	{
		assert(a_addr != NULL);
		if(a_addr->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
		{
			return UNKNOWN_OFFSET;
		} else if(a_addr->hdr.kind == EXPR_KIND_UNARY)
		{
			assert(a_addr->expr.operation == EXPR_OP_MEMBER);
			res += calc_offset_from_struct_start(a_addr->expr.u.member);
		} else {
			node *a_index;
			essl_bool a_index_constant;
			assert(a_addr->hdr.kind == EXPR_KIND_BINARY);
			assert(a_addr->expr.operation == EXPR_OP_INDEX);
			a_index = GET_CHILD(a_addr, 1);

			a_index_constant = a_index->hdr.kind == EXPR_KIND_CONSTANT;

			if(a_index_constant)
			{
				essl_address_offset stride;
				unsigned long long idx;
				stride = desc->get_array_stride(desc, a_addr->hdr.type, a->expr.u.load_store.address_space);
				idx = desc->scalar_to_int(a_index->expr.u.value[0]);
				res += ((unsigned long long)stride) * idx;
			}else
			{
				return UNKNOWN_OFFSET;
			}
		}
		a_addr = GET_CHILD(a_addr, 0);
	}

	return res;

}

/**
   Check if the memory operation given by node b depends on the operation given by node a
   b_covers_a is true if everything that depends on 
 */
		 
static essl_bool do_memory_operations_depend_on_each_other(target_descriptor *desc, node *a, node *b, essl_bool *b_covers_a)
		 
{
	symbol *a_sym = NULL;
	symbol *b_sym = NULL;
	node *a_node_sym = NULL;
	node *b_node_sym = NULL;
	essl_bool a_is_load = a->hdr.kind == EXPR_KIND_LOAD;
	essl_bool b_is_load = b->hdr.kind == EXPR_KIND_LOAD;
	unsigned long long a_offset, b_offset;

	assert(a->hdr.kind == EXPR_KIND_LOAD || a->hdr.kind == EXPR_KIND_STORE);
	assert(b->hdr.kind == EXPR_KIND_LOAD || b->hdr.kind == EXPR_KIND_STORE);
	*b_covers_a = ESSL_FALSE;
	if(a->expr.u.load_store.address_space != b->expr.u.load_store.address_space)
	{
		/* different address spaces can't depend on each other */
		return ESSL_FALSE;
	}

	if(a_is_load && b_is_load)
	{
		/* two load operations can't depend on each other */
		return ESSL_FALSE;
	}

	a_sym = symbol_for_node(GET_CHILD(a, 0));
	b_sym = symbol_for_node(GET_CHILD(b, 0));

	if(b_sym == NULL)
	{
		/* okay, b is a pointer access. therefore we assume aliasing, and b covers a if b is not a load*/
		*b_covers_a = !b_is_load;
		return ESSL_TRUE;
	} else if(a_sym == NULL)
	{
		assert(b_sym != NULL);
		/* a is a pointer access, but b is not. we must still assume aliasing, but can't assume coverage */
		*b_covers_a = ESSL_FALSE;
		return ESSL_TRUE;
	}

	if(a_sym != b_sym)
	{
		/* reads and writes to different symbols. can't interfere with each other, except for arguments */
		scope_kind a_scope = (scope_kind)a_sym->scope;
		scope_kind b_scope = (scope_kind)b_sym->scope;

		if(a_scope == b_scope  && (a_scope == SCOPE_ARGUMENT || a_scope == SCOPE_FORMAL))
		{
			/* arguments and parameters are tricky, since unrelated
			   arguments might occupy the same space on the stack.

			   Since we're considering these in a single basic block,
			   the case we're going to see is loads from out
			   parameters for a first function call mixed with stores
			   for in parameters to a second function call. load+load
			   can therefore not conflict, and store+store cannot
			   conflict, but load+store or store+load can conflict */

			return a->expr.operation != b->expr.operation;

		}

		return ESSL_FALSE;
	}


	/* Okay, memory operation on the same symbol. Let's see if the addressing expressions can conflict */

		/* Okay, memory operation on the same symbol. Let's see if the addressing expressions can conflict */
	a_node_sym = symbol_node_for_node(GET_CHILD(a, 0));
	b_node_sym = symbol_node_for_node(GET_CHILD(b, 0));

	*b_covers_a = !b_is_load;
	{
		node *a_addr = a;
		node *b_addr = b;
		a_addr = GET_CHILD(a_addr, 0);
		b_addr = GET_CHILD(b_addr, 0);
		while(a_addr != a_node_sym && b_addr != b_node_sym)
		{
			assert(a_addr != NULL && b_addr != NULL);

			if(a_addr == b_addr &&
				a_addr->hdr.kind != EXPR_KIND_BINARY &&
				a_addr->expr.operation != EXPR_OP_INDEX)
			{
				/* indexing expressions will be handled in later lines */
				return ESSL_TRUE;
			}

			if(a_addr->hdr.kind != b_addr->hdr.kind)
			{
				a_addr = GET_CHILD(a_addr, 0);
				b_addr = GET_CHILD(b_addr, 0);
				continue;
			}
			if(a_addr->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
			{
				assert(a_addr->expr.u.sym == b_addr->expr.u.sym);
				return ESSL_TRUE;
			} else if(a_addr->hdr.kind == EXPR_KIND_UNARY)
			{
				assert(a_addr->expr.operation == EXPR_OP_MEMBER && b_addr->expr.operation == EXPR_OP_MEMBER);
				if(a_addr->expr.u.member != b_addr->expr.u.member)
				{
					return ESSL_FALSE; /* assuming identity equivalence of structs */
				}
			} else {
				node *a_index;
				node *b_index;
				essl_bool a_index_constant;
				essl_bool b_index_constant;
				assert(a_addr->hdr.kind == EXPR_KIND_BINARY);
				assert(a_addr->expr.operation == EXPR_OP_INDEX && b_addr->expr.operation == EXPR_OP_INDEX);
				a_index = GET_CHILD(a_addr, 1);
				b_index = GET_CHILD(b_addr, 1);

				a_index_constant = a_index->hdr.kind == EXPR_KIND_CONSTANT;
				b_index_constant = b_index->hdr.kind == EXPR_KIND_CONSTANT;

				if(!a_index_constant && b_index_constant)
				{
					/* b conflicts with a, but not everything a conflicts with will conflict with b */
					*b_covers_a = ESSL_FALSE;
				}

			}
			a_addr = GET_CHILD(a_addr, 0);
			b_addr = GET_CHILD(b_addr, 0);
		}

		a_offset = calc_memory_offset(desc, a, a_node_sym);
		b_offset = calc_memory_offset(desc, b, b_node_sym);

		if(a_offset != b_offset && (a_offset != UNKNOWN_OFFSET && b_offset != UNKNOWN_OFFSET))
		{
			return ESSL_FALSE;
		}

	}

	return ESSL_TRUE;
}


/**
   Check if the operation given by node b depends on the operation given by node a
 */
static essl_bool do_nodes_depend_on_each_other(target_descriptor *desc, node *a, node *b, essl_bool *b_covers_a)
{
	assert(is_dependent_op(a) && is_dependent_op(b));

	if(is_global_op(b))
	{
		*b_covers_a = ESSL_TRUE;
		return ESSL_TRUE;
	} else if(is_global_op(a))
	{
		*b_covers_a = ESSL_FALSE;
		return ESSL_TRUE;
	}


	return do_memory_operations_depend_on_each_other(desc, a, b, b_covers_a);

}

static essl_bool addresses_identical(node *n1, node *n2)
{
	if(n1 == n2) return ESSL_TRUE;
	if(n1->hdr.kind != n2->hdr.kind) return ESSL_FALSE;
	switch(n1->hdr.kind)
	{
	case EXPR_KIND_VARIABLE_REFERENCE:
		return n1->expr.u.sym == n2->expr.u.sym;
	case EXPR_KIND_UNARY:
	case EXPR_KIND_BINARY:
		if(n1->expr.operation != n2->expr.operation) return ESSL_FALSE;
		switch(n1->expr.operation)
		{
		case EXPR_OP_MEMBER:
			if(n1->expr.u.member != n2->expr.u.member) return ESSL_FALSE;
			return addresses_identical(GET_CHILD(n1, 0), GET_CHILD(n2, 0));
		case EXPR_OP_INDEX:
			if(!addresses_identical(GET_CHILD(n1, 1), GET_CHILD(n2, 1))) return ESSL_FALSE;
			return addresses_identical(GET_CHILD(n1, 0), GET_CHILD(n2, 0));
		default:
			return ESSL_FALSE;
		}
	case EXPR_KIND_CONSTANT:
	{
		unsigned n_elems = _essl_get_type_size(n1->hdr.type);
		if(n_elems != _essl_get_type_size(n2->hdr.type)) return ESSL_FALSE;
		return 0 == memcmp(n1->expr.u.value, n2->expr.u.value, sizeof(n1->expr.u.value)*n_elems);
	}
	default:
		return ESSL_FALSE;
	}
}

memerr _essl_forward_stores_to_loads_and_elide_stores(pass_run_context *pr_ctx, symbol *func)
{

	/* 
	   The algorithm:

	   Keep track of currently active operations. An operation goes active when encountered, and inactive when we've encountered a new operation that depends on the old operation, and that has the property that everything that every operation that will depend on the old operation will also depend on the new operation.
	   
	   Whenever we encounter a new operation, we check if it depends on any of the active operations, and then we insert into the active operation set, and optionally remove old operations from the active operation set.

	   Whenever we encounter a load, we see if we have an active store that completely covers it.
	*/
	mempool *tmp_pool = pr_ctx->tmp_pool;
	control_flow_graph *cfg = func->control_flow_graph;
	ptrset active_ops;
	ptrset symbols_with_loads;
	unsigned i;
	symbol *any_mem_sym;
	string name = {"<any_memory_part_var>", 21}; /* this is a dummy symbol necessary to represent any part of the memory  */
	ESSL_CHECK(any_mem_sym = _essl_new_variable_symbol_with_default_qualifiers(pr_ctx->pool, name, NULL, SCOPE_GLOBAL,
														ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));

	ESSL_CHECK(_essl_ptrset_init(&active_ops, tmp_pool));
	ESSL_CHECK(_essl_ptrset_init(&symbols_with_loads, tmp_pool));
	

	for(i = 0; i < cfg->n_blocks; ++i)
	{
		control_dependent_operation **op_ptr;
		basic_block *block = cfg->output_sequence[i];
		ESSL_CHECK(_essl_ptrset_clear(&active_ops));

		for(op_ptr = &block->control_dependent_ops; *op_ptr != 0; )
		{
			essl_bool eliminate_op = ESSL_FALSE;
			control_dependent_operation *op = *op_ptr;
			node *found_source = NULL;
			assert(op->op != 0);
			if(is_dependent_op(op->op))
			{
				if(op->op->hdr.kind == EXPR_KIND_LOAD) /* TODO only do this for loads that aren't volatile */
				{
					/* see if we have a preceding store that defines exactly what we're loading. 
					 * if so, delete the control dependent op and replace the load with a depend
					 * node pointing to the value stored */
					control_dependent_operation *a;
					ptrset_iter it;
					_essl_ptrset_iter_init(&it, &active_ops);
					while( (a = _essl_ptrset_next(&it)) != NULL)
					{
						if(a->op->hdr.kind == EXPR_KIND_STORE && 
							a->op->expr.u.load_store.address_space == op->op->expr.u.load_store.address_space)
						{
							if(addresses_identical(GET_CHILD(a->op, 0), GET_CHILD(op->op, 0)))
							{
								/* the ptrset guarantees that the traversal order is the insertion order. 
								 * therefore, to find the node latest in time, we need to 
								 * select the latest node in traversel order that matches */
								
								found_source = a->op;
							}
						}
						
					}

					if(found_source != NULL)
					{
						_essl_ptrdict_remove(&cfg->control_dependence, op->op);
						op->op->hdr.is_control_dependent = 0;
						_essl_rewrite_node_to_transfer(op->op, GET_CHILD(found_source, 1));
						eliminate_op = ESSL_TRUE;
					} else {
						symbol *sym = symbol_for_node(GET_CHILD(op->op, 0));
						if(sym == NULL)
						{
							/* if we didn't find a symbol for the load this means that the load can read any part of the memory
							 * and we don't know which part is read */
							ESSL_CHECK(_essl_ptrset_insert(&symbols_with_loads, any_mem_sym));
						}else if(sym != NULL && sym->address_space == ADDRESS_SPACE_THREAD_LOCAL && sym->scope == SCOPE_LOCAL)
						{
							ESSL_CHECK(_essl_ptrset_insert(&symbols_with_loads, sym));
						}
					}

				}

				if(!eliminate_op)
				{
					control_dependent_operation *a;
					ptrset_iter it;
					_essl_ptrset_iter_init(&it, &active_ops);
					while( (a = _essl_ptrset_next(&it)) != NULL)
					{
						essl_bool b_covers_a = ESSL_FALSE;
						if(do_nodes_depend_on_each_other(pr_ctx->desc, a->op, op->op, &b_covers_a))
						{
							if(b_covers_a)
							{
								ESSL_CHECK(_essl_ptrset_remove(&active_ops, a));
							}
						}
						
					}

					ESSL_CHECK(_essl_ptrset_insert(&active_ops, op));
				}

			}

			if(eliminate_op)
			{
				*op_ptr = (*op_ptr)->next;
			} else {
				op_ptr = &(*op_ptr)->next;
			}
			
		}
	}

	/* go and elide stores to local variables which we have no loads left for */
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		control_dependent_operation **op_ptr;
		basic_block *block = cfg->output_sequence[i];
		ESSL_CHECK(_essl_ptrset_clear(&active_ops));

		for(op_ptr = &block->control_dependent_ops; *op_ptr != 0; )
		{
			essl_bool eliminate_op = ESSL_FALSE;
			control_dependent_operation *op = *op_ptr;
			assert(op->op != 0);
			if(op->op->hdr.kind == EXPR_KIND_STORE) /* TODO only do this for stores that aren't volatile */
			{
				/* see if this is a store of a known symbol, which is a local variable that we have no loads for */

				/* TODO don't do this if someone takes the address of the symbol and passes it to other functions */
				symbol *sym = symbol_for_node(GET_CHILD(op->op, 0));
				if(sym != NULL && sym->address_space == ADDRESS_SPACE_THREAD_LOCAL && sym->scope == SCOPE_LOCAL)
				{
					if(!_essl_ptrset_has(&symbols_with_loads, sym) &&
							!_essl_ptrset_has(&symbols_with_loads, any_mem_sym))
					{
						/* no loads, we'll drop this store */
						eliminate_op = ESSL_TRUE;
					}
				}
			}

			if(eliminate_op)
			{
				*op_ptr = (*op_ptr)->next;
			} else {
				op_ptr = &(*op_ptr)->next;
			}
			
		}
	}

	return MEM_OK;



}


memerr _essl_control_dependencies_calc(pass_run_context *pr_ctx, symbol *func)
{

	/* we can expect three kinds of global dependencies here:

	 * variable access
	 * variable modification
	 * function calls
	 
	 Function calls should depend on each other
	 All _control_dependent_ variable accesses and modifications should depend on function calls.
	 Modifications of the same variable should depend on each other
	 Accesses of the same variable should depend on the previous and next modification of the variable

	 we will perform this in three passes
	*/
	mempool *tmp_pool = pr_ctx->tmp_pool;
	mempool *perm_pool = pr_ctx->pool;
	control_dependency_options options = pr_ctx->desc->control_dep_options;
	control_flow_graph *cfg = func->control_flow_graph;
	ptrdict last_ops;
	ptrdict_iter it;
	symbol *sym;
	control_dependent_operation *end;
	control_dependent_operation *op;
	control_dependent_operation *prev = 0;
	control_dependent_operation *prev_func_call = 0;
	basic_block *b;
	unsigned i;
	ESSL_CHECK(_essl_ptrdict_init(&last_ops, tmp_pool));
	
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		b = cfg->output_sequence[i];

		prev_func_call = 0;
		ESSL_CHECK(_essl_ptrdict_clear(&last_ops));

		/* first pass: create dependencies for variable modifications and function calls */
		for(op = b->control_dependent_ops; op != 0; op = op->next)
		{
			assert(op->op != 0);
			switch(op->op->hdr.kind)
			{
			case EXPR_KIND_FUNCTION_CALL:
				if(prev_func_call)
				{
					ESSL_CHECK(add_dependency(perm_pool, prev_func_call, op));
				}
				prev_func_call = op;
				for(_essl_ptrdict_iter_init(&it, &last_ops); (sym = _essl_ptrdict_next(&it, (void**)(void*)&prev)) != 0; )
				{
					if(sym->address_space == ADDRESS_SPACE_GLOBAL || sym->scope == SCOPE_ARGUMENT)
					{
						assert(prev != 0);
						ESSL_CHECK(add_dependency(perm_pool, prev, op));
						ESSL_CHECK(_essl_ptrdict_remove(&last_ops, sym));
					}
				}
				break;
			case EXPR_KIND_DEPEND:
			case EXPR_KIND_BUILTIN_FUNCTION_CALL:
				break;
				
			case EXPR_KIND_STORE:
				sym = _essl_symbol_for_node(op->op);
				ESSL_CHECK(sym);
				prev = _essl_ptrdict_lookup(&last_ops, sym);
				if(prev)
				{
					ESSL_CHECK(add_dependency(perm_pool, prev, op));
				} else if(prev_func_call)
				{
					ESSL_CHECK(add_dependency(perm_pool, prev_func_call, op));
				}
				ESSL_CHECK(_essl_ptrdict_insert(&last_ops, sym, op));
				break;
				
			default:
				break;
				
			}
		}
	}

	prev_func_call = 0;
	ESSL_CHECK(_essl_ptrdict_clear(&last_ops));


	for(i = 0; i < cfg->n_blocks; ++i)
	{
		b = cfg->output_sequence[i];
		if((options & CONTROL_DEPS_ACROSS_BASIC_BLOCKS) == 0)
		{
			prev_func_call = 0;
			ESSL_CHECK(_essl_ptrdict_clear(&last_ops));
		}
		/* second pass: create forward dependencies for variable accesses. */
		for(op = b->control_dependent_ops; op != 0; op = op->next)
		{
			assert(op->op != 0);
			switch(op->op->hdr.kind)
			{
			case EXPR_KIND_FUNCTION_CALL:
				prev_func_call = op;
				for(_essl_ptrdict_iter_init(&it, &last_ops); (sym = _essl_ptrdict_next(&it, 0)) != 0; )
				{
					if(sym->address_space == ADDRESS_SPACE_GLOBAL || sym->scope == SCOPE_ARGUMENT)
					{
						ESSL_CHECK(_essl_ptrdict_remove(&last_ops, sym)); /* normally not safe to mess with a dictionary while iterating over it, but since we're replacing existing keys, it's okay */
					}
				}
				break;

			case EXPR_KIND_STORE:
				sym = _essl_symbol_for_node(op->op);
				ESSL_CHECK(sym);
				ESSL_CHECK(_essl_ptrdict_insert(&last_ops, sym, op));
				break;
			case EXPR_KIND_DEPEND:
			case EXPR_KIND_BUILTIN_FUNCTION_CALL:
				break;

			default:
				/* variable reference */
				sym = _essl_symbol_for_node(op->op);
				ESSL_CHECK(sym);
				prev = _essl_ptrdict_lookup(&last_ops, sym);
				if(prev) 
				{
			
					ESSL_CHECK(add_dependency(perm_pool, prev, op));

				} else if(prev_func_call)
				{
					ESSL_CHECK(add_dependency(perm_pool, prev_func_call, op));
				}
				break;

			}
		}
	}


	prev_func_call = 0;
	ESSL_CHECK(_essl_ptrdict_clear(&last_ops));
	

	for(i = cfg->n_blocks; i --> 0;)
	{
		b = cfg->output_sequence[i];
		prev_func_call = 0;
		ESSL_CHECK(_essl_ptrdict_clear(&last_ops));
		/* third pass: create backward dependencies for variable accesses. traverse in reverse order */
		end = LIST_REVERSE(b->control_dependent_ops, control_dependent_operation);
		for(op = end; op != 0; op = op->next)
		{
			assert(op->op != 0);
			switch(op->op->hdr.kind)
			{
			case EXPR_KIND_FUNCTION_CALL:
				prev_func_call = op;
				for(_essl_ptrdict_iter_init(&it, &last_ops); (sym = _essl_ptrdict_next(&it, 0)) != 0; )
				{
					if(sym->address_space == ADDRESS_SPACE_GLOBAL || sym->scope == SCOPE_ARGUMENT)
					{
						ESSL_CHECK(_essl_ptrdict_remove(&last_ops, sym)); /* normally not safe to mess with a dictionary while iterating over it, but since we're replacing existing keys, it's okay */
					}
				}
				break;

			case EXPR_KIND_STORE: 
				sym = _essl_symbol_for_node(op->op);
				ESSL_CHECK(sym);
				ESSL_CHECK(_essl_ptrdict_insert(&last_ops, sym, op));
				break;

			case EXPR_KIND_DEPEND:
			case EXPR_KIND_BUILTIN_FUNCTION_CALL:
				break;
			
			default:
				/* variable reference */
				sym = _essl_symbol_for_node(op->op);
				ESSL_CHECK(sym);
				prev = _essl_ptrdict_lookup(&last_ops, sym);
				if(prev) 
				{
			
					ESSL_CHECK(add_dependency(perm_pool, op, prev));
				} else if(prev_func_call)
				{
					ESSL_CHECK(add_dependency(perm_pool, op, prev_func_call));
				}
				break;

			}
		}




		/* make sure the list is the right way again */
		if(end != 0)
		{
			(void)LIST_REVERSE(end, control_dependent_operation);
		}
	}
	return MEM_OK;



}
