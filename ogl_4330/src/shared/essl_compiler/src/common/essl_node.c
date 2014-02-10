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
#include "common/essl_node.h"
#include "common/essl_common.h"
#include "common/symbol_table.h"
#include "common/essl_target.h"


node *_essl_new_node(mempool *pool, node_kind kind, unsigned n_children)
{
	node *n;
	unsigned child_array_size = n_children == 0 ? 1 : n_children;
	size_t n_bytes = sizeof(node)  + child_array_size * sizeof(nodeptr);
	ESSL_CHECK(n = _essl_mempool_alloc(pool, n_bytes));
	n->hdr.kind = kind;
	n->hdr.n_children = (unsigned short)n_children;
	n->hdr.child_array_size = (unsigned short)child_array_size;
	n->hdr.children = (nodeptr*)((char*)n + sizeof(node));
	return n;
}


node *_essl_clone_node(mempool *pool, node *orig)
{
	node *n = 0;
	size_t nodesize;
	unsigned int array_size;
	nodeptr *children = 0;
	if(orig->hdr.kind == EXPR_KIND_CONSTANT)
	{
		unsigned size;
		if(orig->hdr.type == 0) return 0;
		size  = _essl_get_type_size(orig->hdr.type);
		n = _essl_new_constant_expression(pool, size);
		nodesize = sizeof(node)
		           + (size - 1) * sizeof(scalar_type);
		
	} else {
		n = _essl_new_node(pool, orig->hdr.kind, orig->hdr.n_children);
		nodesize = sizeof(node);
	}
	ESSL_CHECK(n);
	children = n->hdr.children;
	array_size = n->hdr.child_array_size;
	memcpy(n, orig, nodesize);
	n->hdr.children = children;
	n->hdr.child_array_size = array_size;
	if(n->hdr.kind & NODE_KIND_EXPRESSION)
	{
		n->expr.info = NULL;
		n->expr.earliest_block = NULL;
		n->expr.best_block = NULL;
		n->expr.latest_block = NULL;
	}
	memcpy(n->hdr.children, orig->hdr.children, orig->hdr.n_children*sizeof(nodeptr));
	return n;
}



memerr _essl_node_set_n_children(node *n, unsigned n_children, mempool *pool)
{

	if(n_children > n->hdr.child_array_size)
	{
		nodeptr *new_buf = 0;
		if( n_children > 0 )
		{
			ESSL_CHECK(new_buf = _essl_mempool_alloc(pool, n_children*sizeof(nodeptr)));
			if(n->hdr.n_children > 0)
			{
				memcpy(new_buf, n->hdr.children, n->hdr.n_children*sizeof(nodeptr));
			}
		}
		n->hdr.children = new_buf;
		n->hdr.child_array_size = (unsigned short)n_children;
		/* old buffer disappears into thin air */

	}
	n->hdr.n_children = (unsigned short)n_children;
	return MEM_OK;
}






unsigned _essl_node_get_n_children(const node *n)
{
	return n->hdr.n_children;
}


node *_essl_node_get_child(const node *n, unsigned idx)
{
	assert(idx < n->hdr.n_children);
	assert(n != n->hdr.children[idx]); /* a node should not have itself for a child */
	return n->hdr.children[idx];
}


nodeptr *_essl_node_get_child_address(const node *n, unsigned idx)
{
	assert(idx < n->hdr.n_children);
	assert(n != n->hdr.children[idx]); /* a node should not have itself for a child */
	return &n->hdr.children[idx];
}


void _essl_node_set_child(node *n, unsigned idx, node *child)
{
	assert(n != child); /* a node should not have itself for a child */
	assert(idx < n->hdr.n_children);
	n->hdr.children[idx] = child;
}


memerr _essl_node_append_child(node *n, node *child, mempool *pool)
{
	assert(n != child); /* a node should not have itself for a child */
	if(n->hdr.n_children >= n->hdr.child_array_size)
	{
		unsigned new_size = (unsigned)n->hdr.child_array_size * 2;
		nodeptr *new_buf;
		if(new_size < 4) new_size = 4;
		new_buf = _essl_mempool_alloc(pool, new_size * sizeof(nodeptr));
		ESSL_CHECK(new_buf);
		if(n->hdr.n_children > 0)
		{
			memcpy(new_buf, n->hdr.children, n->hdr.n_children*sizeof(nodeptr));
		}
		n->hdr.child_array_size = (unsigned short)new_size;
		n->hdr.children = new_buf;
	}

	n->hdr.children[n->hdr.n_children++] = child;
	return MEM_OK;
}

memerr _essl_node_prepend_child(node *n, node *child, mempool *pool)
{
	assert(n != child); /* a node should not have itself for a child */
	if(n->hdr.n_children >= n->hdr.child_array_size)
	{
		unsigned new_size = (unsigned)n->hdr.child_array_size * 2;
		nodeptr *new_buf;
		if(new_size < 4) new_size = 4;
		new_buf = _essl_mempool_alloc(pool, new_size * sizeof(nodeptr));
		ESSL_CHECK(new_buf);
		if(n->hdr.n_children > 0)
		{
			memcpy(new_buf, n->hdr.children, n->hdr.n_children*sizeof(nodeptr));
		}
		n->hdr.child_array_size = (unsigned short)new_size;
		n->hdr.children = new_buf;
	}
	assert(n->hdr.n_children < n->hdr.child_array_size);
	memmove(n->hdr.children+1, n->hdr.children, n->hdr.n_children*sizeof(nodeptr));
	n->hdr.children[0] = child;
	++n->hdr.n_children;
	return MEM_OK;
}


node *_essl_new_translation_unit(mempool *pool, scope *global_scope)
{
	node *n = _essl_new_node(pool, NODE_KIND_TRANSLATION_UNIT, 0);
	if (!n)
	{
		return 0;
	}
	n->stmt.child_scope = global_scope;
	return n;
}


node *_essl_new_unary_expression(mempool *pool, expression_operator op, node *operand)
{
	node *n = _essl_new_node(pool, EXPR_KIND_UNARY, 1);
	if (!n)
	{
		return 0;
	}
	n->expr.operation = op;
	if(op == EXPR_OP_SWIZZLE)
	{
		n->expr.u.swizzle = _essl_create_undef_swizzle();
	}
	SET_CHILD(n, 0, operand);
	return n;
}


node *_essl_new_binary_expression(mempool *pool, node *left, 
				  expression_operator op, node *right)
{
	node *n = _essl_new_node(pool, EXPR_KIND_BINARY, 2);
	if (!n)
	{
		return 0;
	}

	n->expr.operation = op;
	SET_CHILD(n, 0, left);
	SET_CHILD(n, 1, right);
	return n;
}


node *_essl_new_assign_expression(mempool *pool, node *left, 
				  expression_operator op, node *right)
{
	node *n = _essl_new_node(pool, EXPR_KIND_ASSIGN, 2);
	if (!n)
	{
		return 0;
	}
	n->expr.operation = op;
	SET_CHILD(n, 0, left);
	SET_CHILD(n, 1, right);
	return n;
}


node *_essl_new_ternary_expression(mempool *pool, expression_operator op, 
				   node *a, node *b, node *c)

{
	node *n = _essl_new_node(pool, EXPR_KIND_TERNARY, 3);
	if (!n)
	{
		return 0;
	}
	n->expr.operation = op;
	SET_CHILD(n, 0, a);
	SET_CHILD(n, 1, b);
	SET_CHILD(n, 2, c);
	return n;
}

/*@null@*/ node *_essl_new_vector_combine_expression(struct _tag_mempool *pool, unsigned n_children)
{
	node *n;
	ESSL_CHECK(n = _essl_new_node(pool, EXPR_KIND_VECTOR_COMBINE, n_children));
	n->expr.u.combiner = _essl_create_undef_combiner();
	return n;
}


node *_essl_new_constant_expression(mempool *pool, unsigned int vec_size)
{
	node *n;
	size_t nodesize_without_child_array = sizeof(node) + (size_t)(vec_size - 1) * sizeof(scalar_type);
	size_t nodesize = nodesize_without_child_array + 1 * sizeof(nodeptr);
	n = _essl_mempool_alloc(pool, nodesize);
	if (!n)
	{
		return 0;
	}
	n->hdr.children = (nodeptr*)((char*)n + nodesize_without_child_array);
	n->hdr.n_children = 0;
	n->hdr.child_array_size = 1;
	n->hdr.kind = EXPR_KIND_CONSTANT;
	return n;

}


node *_essl_new_variable_reference_expression(mempool *pool, symbol *sym)
{
	node *n = _essl_new_node(pool, EXPR_KIND_VARIABLE_REFERENCE, 0);
	if (!n)
	{
		return 0;
	}
	assert(sym != 0);
	n->expr.u.sym = sym;
	return n;
}

node *_essl_new_builtin_constructor_expression(mempool *pool, unsigned n_args)
{
	return _essl_new_node(pool, EXPR_KIND_BUILTIN_CONSTRUCTOR, n_args);
}

node *_essl_new_struct_constructor_expression(mempool *pool, unsigned n_args)
{
	return _essl_new_node(pool, EXPR_KIND_STRUCT_CONSTRUCTOR, n_args);
}


node *_essl_new_function_call_expression(mempool *pool, symbol *sym, unsigned n_args)
{
	node *n = _essl_new_node(pool, EXPR_KIND_FUNCTION_CALL, n_args);
	if (!n)
	{
		return 0;
	}
	n->expr.u.fun.sym = sym;
	n->expr.u.fun.arguments = 0;
	return n;
}


/*@null@*/ node *_essl_new_builtin_function_call_expression(struct _tag_mempool *pool, 
		    expression_operator fun, node *arg0, /*@null@*/ node *arg1, 
		    /*@null@*/ node *arg2)
{
	node *n;
	unsigned n_args = 0;
	if(arg0 != 0) n_args = 1;
	if(arg1 != 0) n_args = 2;
	if(arg2 != 0) n_args = 3;
	ESSL_CHECK(n = _essl_new_node(pool, EXPR_KIND_BUILTIN_FUNCTION_CALL, n_args));

	n->expr.operation = fun;
	if(arg0 != 0) SET_CHILD(n, 0, arg0);
	if(arg1 != 0) SET_CHILD(n, 1, arg1);
	if(arg2 != 0) SET_CHILD(n, 2, arg2);
	return n;
}


node *_essl_new_phi_expression(mempool *pool, struct _tag_basic_block *block)
{
	node *n = _essl_new_node(pool, EXPR_KIND_PHI, 0);
	if (!n)
	{
		return 0;
	}
	n->expr.u.phi.block = block;
	return n;
}


node *_essl_new_dont_care_expression(struct _tag_mempool *pool, const type_specifier *type)
{
	node *n = _essl_new_node(pool, EXPR_KIND_DONT_CARE, 0);
	if (!n)
	{
		return 0;
	}
	n->hdr.type = type;
	return n;
	

}


node *_essl_new_transfer_expression(struct _tag_mempool *pool, node *expr)
{
	node *n = _essl_new_node(pool, EXPR_KIND_TRANSFER, 1);
	if (!n)
	{
		return 0;
	}
	_essl_ensure_compatible_node(n, expr);
	SET_CHILD(n, 0, expr);
	return n;
}


node *_essl_new_load_expression(struct _tag_mempool *pool, address_space_kind address_space, 
				node *address)
{
	node *n = _essl_new_node(pool, EXPR_KIND_LOAD, 1);
	if (!n)
	{
		return 0;
	}
	if(address != NULL)
	{
		_essl_ensure_compatible_node(n, address);
	}
	SET_CHILD(n, 0, address);
	n->expr.u.load_store.address_space = address_space;
	return n;
}


node *_essl_new_store_expression(struct _tag_mempool *pool, address_space_kind address_space, 
				 node *address, node *expr) {
	node *n = _essl_new_node(pool, EXPR_KIND_STORE, 2);
	if (!n)                            /* ESSL_CHECK */
		return 0;
	n->expr.u.load_store.address_space = address_space;
	SET_CHILD(n, 0, address);
	SET_CHILD(n, 1, expr);
	_essl_ensure_compatible_node(n, expr);
	return n;
}

/*@null@*/ node *_essl_new_depend_expression(struct _tag_mempool *pool, unsigned n_children)
{
	return _essl_new_node(pool, EXPR_KIND_DEPEND, n_children);
}

/*@null@*/ node *_essl_new_type_convert_expression(struct _tag_mempool *pool, expression_operator op, node *arg)
{
	node *n = _essl_new_node(pool, EXPR_KIND_TYPE_CONVERT, 1);
	if(n == NULL) return NULL;
	assert(op == EXPR_OP_IMPLICIT_TYPE_CONVERT || op == EXPR_OP_EXPLICIT_TYPE_CONVERT);
	n->expr.operation = op;
	SET_CHILD(n, 0, arg);
	return n;
}

node *_essl_new_flow_control_statement(mempool *pool, node_kind kind, node *returnvalue)
{
	node *n = _essl_new_node(pool, kind, kind == STMT_KIND_RETURN ? 1u : 0u);
	if (!n)
	{
		return 0;
	}
	if(kind == STMT_KIND_RETURN)
	{
		SET_CHILD(n, 0, returnvalue);
	}
	return n;
}


node *_essl_new_if_statement(mempool *pool, node *condition, node *thenpart, node *elsepart)
{
	node *n = _essl_new_node(pool, STMT_KIND_IF, 3);
	if (!n)
	{
		return 0;
	}
	SET_CHILD(n, 0, condition);
	SET_CHILD(n, 1, thenpart);
	SET_CHILD(n, 2, elsepart);
	return n;
}


node *_essl_new_while_statement(mempool *pool, node *condition, node *body)
{
	node *n = _essl_new_node(pool, STMT_KIND_WHILE, 2);
	if (!n)
	{
		return 0;
	}
	SET_CHILD(n, 0, condition);
	SET_CHILD(n, 1, body);
	return n;
}


node *_essl_new_do_statement(mempool *pool, node *body, node *condition)
{
	node *n = _essl_new_node(pool, STMT_KIND_DO, 2);
	if (!n)
	{
		return 0;
	}
	SET_CHILD(n, 0, body);
	SET_CHILD(n, 1, condition);
	return n;
}


node *_essl_new_for_statement(mempool *pool, node *initial, node *condition, 
			      node *increment, node *body)
{
	node *n = _essl_new_node(pool, STMT_KIND_FOR, 4);
	if (!n)
	{
		return 0;
	}
	SET_CHILD(n, 0, initial);
	SET_CHILD(n, 1, condition);
	SET_CHILD(n, 2, increment);
	SET_CHILD(n, 3, body);
	return n;
}


node *_essl_new_compound_statement(mempool *pool)
{
	node *n = _essl_new_node(pool, STMT_KIND_COMPOUND, 0);
	if (!n)
	{
		return 0;
	}
	return n;
}


node *_essl_new_function_declaration(mempool *pool, symbol *sym)
{
	node *n = _essl_new_node(pool, DECL_KIND_FUNCTION, 1);
	if (!n)
	{
		return 0;
	}
	assert(sym != 0);
	n->decl.sym = sym;
	SET_CHILD(n, 0, 0);
	return n;
}


node *_essl_new_variable_declaration(mempool *pool, symbol *sym, node *initializer)
{
	node *n = _essl_new_node(pool, DECL_KIND_VARIABLE, 1);
	if (!n)
	{
		return 0;
	}
	assert(sym != 0);
	n->decl.sym = sym;
	SET_CHILD(n, 0, initializer);
	return n;
}


node *_essl_new_precision_declaration(mempool *pool, type_basic basic_type, precision_qualifier precision)
{
	node *n = _essl_new_node(pool, DECL_KIND_PRECISION, 0);
	if (!n)
	{
		return 0;
	}
	n->decl.prec_type = basic_type;
	n->decl.prec_decl = precision;
	return n;
}

void _essl_set_node_position(node *n, int source_offset)
{
	n->hdr.source_offset = source_offset;
}

void _essl_ensure_compatible_node(node *newnode, node *oldnode)
{
	assert(oldnode->hdr.type != 0);
	newnode->hdr.type = oldnode->hdr.type;
	newnode->hdr.source_offset = oldnode->hdr.source_offset;
	newnode->hdr.is_invariant = oldnode->hdr.is_invariant;
}


symbol *_essl_symbol_for_node(node *n)
{
	while(n!=0 && 
		n->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE  && 
		GET_N_CHILDREN(n) > 0)
	{
		n = GET_CHILD(n, 0);
		if(n != 0)
		{
			/* if the child isn't a variable reference, swizzle, member or index, abort */
			if( n->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE &&
				(n->hdr.kind != EXPR_KIND_UNARY || (n->expr.operation != EXPR_OP_SWIZZLE && n->expr.operation != EXPR_OP_MEMBER)) &&
				(n->hdr.kind != EXPR_KIND_BINARY || n->expr.operation != EXPR_OP_INDEX) &&
				(n->hdr.kind != EXPR_KIND_ASSIGN) )
			{
				return NULL;
			}
		}
	}


	if(n == NULL) return NULL;
	if(n->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		assert(n->expr.u.sym != NULL);
		return n->expr.u.sym;
	}
	return NULL;

}

essl_bool _essl_is_optimized_sampler_symbol(const symbol *s)
{
	const type_specifier *t = s->type;
	return _essl_is_sampler_type(t);
}


/** Returns nonzero if n is a constant expression */
essl_bool _essl_node_is_constant(node *n)
{
	assert(n != 0);
	if(n->hdr.kind == EXPR_KIND_STRUCT_CONSTRUCTOR)
	{
		size_t i = 0;
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			node *c;
			ESSL_CHECK(c = GET_CHILD(n, i));
			if(!_essl_node_is_constant(c)) return ESSL_FALSE;
		}

		return ESSL_TRUE;
	} else {
		return (essl_bool)(n->hdr.kind == EXPR_KIND_CONSTANT);
	}

}

void _essl_rewrite_node_to_transfer(node *n, node *child)
{
	assert(n->hdr.child_array_size >= 1);
	assert(child);
	if(n != child) /* it's okay to want to transfer a node to itself. This is a no-op */
	{
		n->hdr.kind = EXPR_KIND_TRANSFER;
		n->expr.operation = EXPR_OP_UNKNOWN;
		n->hdr.n_children = 1;
		n->hdr.children[0] = child;
	}
}


swizzle_pattern _essl_create_undef_swizzle(void)
{
	unsigned i;
	swizzle_pattern swz;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		swz.indices[i] = -1;
	}
	return swz;
}

swizzle_pattern _essl_create_scalar_swizzle(unsigned swz_idx)
{
	swizzle_pattern swz = _essl_create_undef_swizzle();
	swz.indices[0] = swz_idx;
	return swz;
}

swizzle_pattern _essl_create_identity_swizzle(unsigned swz_size)
{
	unsigned i;
	swizzle_pattern swz;
	for(i = 0; i < swz_size; ++i)
	{
		swz.indices[i] = i;
	}

	for(i = swz_size; i < N_COMPONENTS; ++i)
	{
		swz.indices[i] = -1;
	}
	return swz;
}

swizzle_pattern _essl_combine_swizzles(swizzle_pattern later, swizzle_pattern earlier)
{
	swizzle_pattern res = later;
	size_t i;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(res.indices[i] != -1)
		{
			int idx;
			assert(res.indices[i] >= 0 && res.indices[i] < N_COMPONENTS);
			idx = earlier.indices[res.indices[i]];
			res.indices[i] = idx;
		}
	}
	return res;
}

essl_bool _essl_is_identity_swizzle(swizzle_pattern swz)
{
	unsigned i;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(swz.indices[i] != (int)i && swz.indices[i] != -1) return ESSL_FALSE;
	}
	return ESSL_TRUE;
}

essl_bool _essl_is_identity_swizzle_sized(swizzle_pattern swz, unsigned size)
{
	unsigned i;
	for(i = 0; i < size; ++i)
	{
		if(swz.indices[i] != (int)i) return ESSL_FALSE;
	}
	return ESSL_TRUE;
}


combine_pattern _essl_create_undef_combiner(void)
{
	unsigned i;
	combine_pattern cmb;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		cmb.mask[i] = -1;
	}
	return cmb;
}

combine_pattern _essl_create_on_combiner(unsigned cmb_size)
{
	unsigned i;
	combine_pattern cmb;
	for(i = 0; i < cmb_size; ++i)
	{
		cmb.mask[i] = 1;
	}
	for(i = cmb_size; i < N_COMPONENTS; ++i)
	{
		cmb.mask[i] = -1;
	}
	return cmb;
}

swizzle_pattern _essl_create_identity_swizzle_from_swizzle(swizzle_pattern swz)
{
	unsigned i;
	swizzle_pattern res;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(swz.indices[i] != -1)
		{
			res.indices[i] = i;
		} else {
			res.indices[i] = -1;
		}
	}
	return res;
}

swizzle_pattern _essl_create_identity_swizzle_from_mask(unsigned live_mask)
{
	swizzle_pattern swz;
	unsigned i;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(((live_mask >> i)&1) != 0)
		{
			swz.indices[i] = i;
		} else {
			swz.indices[i] = -1;
		}
	}
	return swz;
}

unsigned _essl_mask_from_swizzle_output(swizzle_pattern swz)
{
	unsigned mask = 0;
	unsigned i;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(swz.indices[i] != -1) mask |= (1<<i);
	}
	return mask;
}

unsigned _essl_mask_from_swizzle_input(swizzle_pattern *swz) {
	unsigned mask = 0;
	unsigned i;
	for (i = 0 ; i < N_COMPONENTS ; i++) {
		if (swz->indices[i] >= 0) {
			mask |= 1 << swz->indices[i];
		}
	}
	return mask;
}


essl_bool _essl_node_is_texture_operation(const node *n)
{
	return (n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
					   (
						   n->expr.operation == EXPR_OP_FUN_TEXTURE1D ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE1DPROJ ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE1DLOD ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE1DPROJLOD ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2D ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJ ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJ_W ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DLOD ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJLOD ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJLOD_W ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE3D ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE3DPROJ ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE3DLOD ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE3DPROJLOD ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURECUBE ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURECUBELOD ||
						   n->expr.operation == EXPR_OP_FUN_TEXTUREEXTERNAL ||
						   n->expr.operation == EXPR_OP_FUN_TEXTUREEXTERNALPROJ ||
						   n->expr.operation == EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W ||
						   n->expr.operation == EXPR_OP_FUN_SHADOW2D ||
						   n->expr.operation == EXPR_OP_FUN_SHADOW2DPROJ ||
						   n->expr.operation == EXPR_OP_FUN_SHADOW2DLOD ||
						   n->expr.operation == EXPR_OP_FUN_SHADOW2DPROJLOD ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DLOD_EXT ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURECUBELOD_EXT ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DGRAD_EXT ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT ||
						   n->expr.operation == EXPR_OP_FUN_TEXTURECUBEGRAD_EXT));
}



node *_essl_create_vector_combine_for_nodes(mempool *pool, typestorage_context *typestor_context, node *left, node *right, node *n)
{
	/* cool with only one non-zero argument */
	if(left == NULL) return right;
	if(right == NULL) return left;
	

	{
		unsigned left_size = GET_NODE_VEC_SIZE(left);
		unsigned right_size = GET_NODE_VEC_SIZE(right);
		unsigned res_size = left_size + right_size;
		const type_specifier *res_type;
		node *res;
		node *l_swz;
		node *r_swz;
		unsigned i;

		ESSL_CHECK(res_type = _essl_get_type_with_given_vec_size(typestor_context, left->hdr.type, res_size));

		ESSL_CHECK(l_swz = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, left));
		_essl_ensure_compatible_node(l_swz, n);
		l_swz->hdr.type = res_type;

		ESSL_CHECK(r_swz = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, right));
		_essl_ensure_compatible_node(r_swz, n);
		r_swz->hdr.type = res_type;
		
		ESSL_CHECK(res = _essl_new_vector_combine_expression(pool, 2));
		_essl_ensure_compatible_node(res, n);
		SET_CHILD(res, 0, l_swz);
		SET_CHILD(res, 1, r_swz);
		res->hdr.type = res_type;
		
		for(i = 0; i < left_size; ++i)
		{
			res->expr.u.combiner.mask[i] = 0;
			l_swz->expr.u.swizzle.indices[i] = i;
			r_swz->expr.u.swizzle.indices[i] = -1;
		}
		for(; i < res_size; ++i)
		{
			res->expr.u.combiner.mask[i] = 1;
			l_swz->expr.u.swizzle.indices[i] = -1;
			r_swz->expr.u.swizzle.indices[i] = i - left_size;
		}
		return res;
	}

}


swizzle_pattern _essl_create_swizzle_from_combiner(combine_pattern a, int active_component)
{
	swizzle_pattern res = _essl_create_undef_swizzle();
	size_t i;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(a.mask[i] == active_component)
		{
			res.indices[i] = i;
		}
	}
	return res;
}

swizzle_pattern _essl_invert_swizzle(swizzle_pattern swz)
{
	swizzle_pattern res = _essl_create_undef_swizzle();
	unsigned i;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		int j = swz.indices[i];
		if(j >= 0)
		{
			assert(res.indices[j] == -1);
			res.indices[j] = i;
		}
	}
	return res;
}

void _essl_swizzle_patch_dontcares(swizzle_pattern *swz, unsigned swz_size) 
{
	/* Patch up swizzle with dummy components */
	int valid_comp = -1;
	unsigned i;
	for (i = 0 ; i < swz_size ; i++) {
		if (swz->indices[i] != -1) {
			valid_comp = swz->indices[i];
			break;
		}
	}
	assert(valid_comp != -1);
	for (i = 0 ; i < swz_size ; i++) {
		if (swz->indices[i] == -1) {
			swz->indices[i] = valid_comp;
		}
	}
}

essl_bool _essl_is_node_all_value(target_descriptor *desc, node *n, float value)
{
	unsigned n_comps, i;
	if(n->hdr.kind != EXPR_KIND_CONSTANT) return ESSL_FALSE;
	assert(n->hdr.type != 0);
	n_comps = _essl_get_type_size(n->hdr.type);
	for(i = 0; i < n_comps; ++i)
	{
		if(desc->scalar_to_float( n->expr.u.value[i]) != value)
		{
			return ESSL_FALSE;
		}

	}
	return ESSL_TRUE;

}

essl_bool _essl_is_node_comparison(const node *n)
{
	essl_bool res = ESSL_FALSE;
	if(n->hdr.kind == EXPR_KIND_BINARY ||
		n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL)
	{
		expression_operator op = n->expr.operation;
		if(op == EXPR_OP_LT 
				|| op == EXPR_OP_LE
				|| op == EXPR_OP_EQ
				|| op == EXPR_OP_NE
				|| op == EXPR_OP_GE
				|| op == EXPR_OP_GT
				|| op == EXPR_OP_FUN_LESSTHAN
				|| op == EXPR_OP_FUN_LESSTHANEQUAL
				|| op == EXPR_OP_FUN_EQUAL
				|| op == EXPR_OP_FUN_NOTEQUAL
				|| op == EXPR_OP_FUN_GREATERTHANEQUAL
				|| op == EXPR_OP_FUN_GREATERTHAN)
		{
			res = ESSL_TRUE;
		}
	}

	return res;
}
