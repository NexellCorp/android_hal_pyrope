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
#include "middle/optimise_vector_ops.h"
#include "backend/extra_info.h"
#include "common/essl_node.h"


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
} optimise_vector_ops_context;


static node *create_float_constant(optimise_vector_ops_context *ctx, float value, unsigned vec_size, scalar_size_specifier scalar_size)
{
	node *n;
	unsigned i;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, vec_size));
	n->expr.u.value[0] = ctx->desc->float_to_scalar(value);
	for(i = 1; i < vec_size; ++i)
	{
		n->expr.u.value[i] = n->expr.u.value[0];
	}

	ESSL_CHECK(n->hdr.type = _essl_get_type_with_size(ctx->typestor_ctx, TYPE_FLOAT, vec_size, scalar_size));
	return n;
}

typedef struct node_swizzle_type_conv {
	node *n;
	swizzle_pattern swz;
	const type_specifier *type_conv;
} node_swizzle_type_conv;

typedef struct combiner_source_info {
	struct node_swizzle_type_conv last;
	struct node_swizzle_type_conv first_after_last_vector_combine;
} combiner_source_info;


/* Collect all inputs to a vector combine, bypassing all vector combines and swizzles along the way. */
static void collect_combiner_sources(node *n, swizzle_pattern swz, struct combiner_source_info *nodes, 
									unsigned *nodes_i, const type_specifier *type_conv, 
									node_swizzle_type_conv *first_after_last_vector_combine)
{
	unsigned i;
	essl_bool live = ESSL_FALSE;
	for (i = 0 ; i < N_COMPONENTS ; i++)
	{
		if (swz.indices[i] != -1)
		{
			live = ESSL_TRUE;
			break;
		}
	}
	if (!live) return;
	
	if (n->hdr.kind == EXPR_KIND_UNARY && n->expr.operation == EXPR_OP_SWIZZLE)
	{
		for (i = 0 ; i < N_COMPONENTS ; i++)
		{
			if (swz.indices[i] != -1)
			{
				swz.indices[i] = n->expr.u.swizzle.indices[swz.indices[i]];
			}
		}
		collect_combiner_sources(GET_CHILD(n, 0), swz, nodes, nodes_i, type_conv, first_after_last_vector_combine);
	}
	else if (n->hdr.kind == EXPR_KIND_TYPE_CONVERT && type_conv == NULL)
	{
		
		collect_combiner_sources(GET_CHILD(n, 0), swz, nodes, nodes_i, n->hdr.type, first_after_last_vector_combine);
	}
	else if (n->hdr.kind == EXPR_KIND_VECTOR_COMBINE)
	{
		node_swizzle_type_conv children[N_COMPONENTS];
		for (i = 0 ; i < GET_N_CHILDREN(n); i++)
		{
			children[i].n = GET_CHILD(n, i);
			children[i].swz = _essl_create_undef_swizzle();
			children[i].type_conv = type_conv;
		}

		for (i = 0 ; i < N_COMPONENTS ; i++)
		{
			if (swz.indices[i] != -1 && n->expr.u.combiner.mask[swz.indices[i]] != -1)
			{
				assert(swz.indices[i] < (int)GET_NODE_VEC_SIZE(n));
				children[n->expr.u.combiner.mask[swz.indices[i]]].swz.indices[i] = swz.indices[i];
			}
		}

		for (i = 0 ; i < GET_N_CHILDREN(n); i++)
		{
			collect_combiner_sources(children[i].n, children[i].swz, nodes, nodes_i, children[i].type_conv, &children[i]);
		}
	}
	else
	{
		/* collect the source */
		assert(*nodes_i < N_COMPONENTS);
		nodes[*nodes_i].last.n = n;
		nodes[*nodes_i].last.swz = swz;
		nodes[*nodes_i].last.type_conv = type_conv;
		assert(first_after_last_vector_combine != NULL);
		nodes[*nodes_i].first_after_last_vector_combine = *first_after_last_vector_combine;

		(*nodes_i)++;
	}
}

/* Check if two loads can be joined into one as sources to a vector combine. */
static essl_bool loads_equivalent(node *n1, node *n2)
{
	if(n1 == n2) return ESSL_TRUE;
	if(n1->hdr.kind != n2->hdr.kind) return ESSL_FALSE;
	switch(n1->hdr.kind)
	{
	case EXPR_KIND_LOAD:
		if(n1->hdr.is_control_dependent || n2->hdr.is_control_dependent) return ESSL_FALSE;
		if(n1->expr.u.load_store.address_space != n2->expr.u.load_store.address_space) return ESSL_FALSE;
		return loads_equivalent(GET_CHILD(n1, 0), GET_CHILD(n2, 0));
	case EXPR_KIND_VARIABLE_REFERENCE:
		return n1->expr.u.sym == n2->expr.u.sym;
	case EXPR_KIND_UNARY:
	case EXPR_KIND_BINARY:
		if(n1->expr.operation != n2->expr.operation) return ESSL_FALSE;
		switch(n1->expr.operation)
		{
		case EXPR_OP_MEMBER:
			if(n1->expr.u.member != n2->expr.u.member) return ESSL_FALSE;
			return loads_equivalent(GET_CHILD(n1, 0), GET_CHILD(n2, 0));
		case EXPR_OP_INDEX:
			if(!loads_equivalent(GET_CHILD(n1, 1), GET_CHILD(n2, 1))) return ESSL_FALSE;
			return loads_equivalent(GET_CHILD(n1, 0), GET_CHILD(n2, 0));
		default:
			return ESSL_FALSE;
		case EXPR_KIND_CONSTANT:
		{
			unsigned n_elems = _essl_get_type_size(n1->hdr.type);
			if(n_elems != _essl_get_type_size(n2->hdr.type)) return ESSL_FALSE;
			return 0 == memcmp(n1->expr.u.value, n2->expr.u.value, sizeof(n1->expr.u.value)*n_elems);
		}
		}
	default:
		return ESSL_FALSE;
	}
}

static essl_bool is_identity_swizzle(node * a, node *res)
{
	if(GET_NODE_VEC_SIZE(res) == GET_NODE_VEC_SIZE(a))
	{
		unsigned i;
		for(i = 0; i < GET_NODE_VEC_SIZE(res); ++i)
		{
			if(res->expr.u.swizzle.indices[i] != (signed char)i)
			{
				return ESSL_FALSE;
			}
		}
		return ESSL_TRUE;
		
	}
	return ESSL_FALSE;
}

static  node * optimise_vector_ops_single_node(optimise_vector_ops_context * ctx, node *n);

static essl_bool is_valid_subvector_access_idx(optimise_vector_ops_context *ctx, node * a, node * idx)
{
	int idx_val = ctx->desc->scalar_to_int(idx->expr.u.value[0]);
	return idx_val >= 0 && idx_val < (int)GET_NODE_VEC_SIZE(a);
}

static node * handle_swizzle_of_swizzle(optimise_vector_ops_context *ctx, node *res, node *a)
{
	node *newres;
	ESSL_CHECK(newres = _essl_clone_node(ctx->pool, res));
	newres->expr.u.swizzle = _essl_combine_swizzles(res->expr.u.swizzle, a->expr.u.swizzle);
	SET_CHILD(newres, 0, GET_CHILD(a, 0));
	return optimise_vector_ops_single_node(ctx, newres);
}

static node * handle_subvector_access(optimise_vector_ops_context *ctx, node *n)
{
	node *a;
	node *b;

	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(b = GET_CHILD(n, 1));

	if(b->hdr.kind == EXPR_KIND_CONSTANT
		&& is_valid_subvector_access_idx(ctx, a, b)
		)
	{
		/* turn index with a constant into a swizzle - this is more efficient */
		node *r;
		int idx = ctx->desc->scalar_to_int( b->expr.u.value[0]);
		assert(idx >= 0 && idx < (int)GET_NODE_VEC_SIZE(a));
		ESSL_CHECK(r = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, a));
		_essl_ensure_compatible_node(r, n);
		r->expr.u.swizzle.indices[0] = idx;
		return optimise_vector_ops_single_node(ctx, r);
	}else
	{
		return n;
	}
}

static node * handle_subvector_update(optimise_vector_ops_context *ctx, node *n)
{
	node *idx;
	node *scalar;
	node *vec;
	ESSL_CHECK(idx = GET_CHILD(n, 0));
	ESSL_CHECK(vec = GET_CHILD(n, 2));
	if(idx->hdr.kind == EXPR_KIND_CONSTANT)
	{
		/* turn index with a constant into a vector combine - this is more efficient */
		node *swz, *vec_combine;
		unsigned vec_size = GET_NODE_VEC_SIZE(vec);
		unsigned i;
		int idx_val = ctx->desc->scalar_to_int(idx->expr.u.value[0]);
		ESSL_CHECK(scalar = GET_CHILD(n, 1));
		if (idx_val < 0 || idx_val >= (int)vec_size) 
		{
			/* Outside vector range - ignore write */
			return optimise_vector_ops_single_node(ctx, vec);
		}

		/* Swizzle scalar to correct position */
		ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, scalar));
		_essl_ensure_compatible_node(swz, vec);
		swz->expr.u.swizzle.indices[idx_val] = 0;
		ESSL_CHECK(swz = optimise_vector_ops_single_node(ctx, swz));

		/* Combine */
		ESSL_CHECK(vec_combine = _essl_new_vector_combine_expression(ctx->pool, 2));
		SET_CHILD(vec_combine, 0, vec);
		SET_CHILD(vec_combine, 1, swz);
		_essl_ensure_compatible_node(vec_combine, vec);
		for (i = 0 ; i < vec_size ; i++)
		{
			vec_combine->expr.u.combiner.mask[i] = 0;
		}
		vec_combine->expr.u.combiner.mask[idx_val] = 1;

		return optimise_vector_ops_single_node(ctx, vec_combine);
	}else
	{
		return n;
	}
}

static node * handle_vector_combine(optimise_vector_ops_context *ctx, node *n)
{
	/* Check if two sources of the vector combine tree are the same value or are both constants.
	   If this is the case, join these sources into one. */
	struct combiner_source_info nodes[N_COMPONENTS];
	unsigned n_nodes = 0;
	unsigned size = GET_NODE_VEC_SIZE(n);
	swizzle_pattern swz = _essl_create_identity_swizzle(size);
	unsigned i,j;
	unsigned c;
	collect_combiner_sources(n, swz, nodes, &n_nodes, NULL, NULL);
	assert(n_nodes >= 1);
	for (i = 0 ; i < n_nodes-1 ; i++)
	{
		node *n1 = nodes[i].last.n;
		for (j = i+1 ; j < n_nodes ; j++)
		{
			node *n2 = nodes[j].last.n;
			if ((n1 == n2 ||
				 ((n1->hdr.kind == EXPR_KIND_CONSTANT || n1->hdr.kind == EXPR_KIND_DONT_CARE) 
				 && (n2->hdr.kind == EXPR_KIND_CONSTANT || n2->hdr.kind == EXPR_KIND_DONT_CARE)) ||
				 (n1->hdr.kind == EXPR_KIND_LOAD && n2->hdr.kind == EXPR_KIND_LOAD && loads_equivalent(n1, n2)) ||
				 (n1->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL && n2->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
				  n1->expr.operation == n2->expr.operation && GET_N_CHILDREN(n1) == 0 && GET_N_CHILDREN(n2) == 0))
				&& ((nodes[i].last.type_conv == NULL && nodes[j].last.type_conv == NULL) 
						|| (nodes[i].last.type_conv != NULL && nodes[j].last.type_conv != NULL
						&& _essl_type_scalar_part_equal(nodes[i].last.type_conv, nodes[j].last.type_conv))))
			{
				goto combine_duplicate;
			}
		}
	}
	return n;

combine_duplicate:
	{
		node *combined;
		if (nodes[i].last.n->hdr.kind == EXPR_KIND_CONSTANT || nodes[i].last.n->hdr.kind == EXPR_KIND_DONT_CARE)
		{
			/* Make new constant node */
			ESSL_CHECK(combined = create_float_constant(ctx, 0.0f, size, _essl_get_scalar_size_for_type(n->hdr.type)));
			_essl_ensure_compatible_node(combined, n);
			for (c = 0 ; c < size ; c++)
			{
				if (nodes[i].last.swz.indices[c] != -1 && nodes[i].last.n->hdr.kind == EXPR_KIND_CONSTANT)
				{
					combined->expr.u.value[c] = nodes[i].last.n->expr.u.value[nodes[i].last.swz.indices[c]];
				}
				else if (nodes[j].last.swz.indices[c] != -1 && nodes[j].last.n->hdr.kind == EXPR_KIND_CONSTANT)
				{
					combined->expr.u.value[c] = nodes[j].last.n->expr.u.value[nodes[j].last.swz.indices[c]];
				}
				else
				{
					/* Dummy value - keep zero */
				}
			}
		}
		else
		{
			ESSL_CHECK(combined = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, nodes[i].last.n));
			_essl_ensure_compatible_node(combined, n);
			ESSL_CHECK(combined->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, nodes[i].last.n->hdr.type, size));
			for (c = 0 ; c < N_COMPONENTS ; c++)
			{
				if (nodes[i].last.swz.indices[c] != -1)
				{
					assert(nodes[j].last.swz.indices[c] == -1);
					combined->expr.u.swizzle.indices[c] = nodes[i].last.swz.indices[c];
				}
				else if (nodes[j].last.swz.indices[c] != -1)
				{
					assert(nodes[i].last.swz.indices[c] == -1);
					combined->expr.u.swizzle.indices[c] = nodes[j].last.swz.indices[c];
				}
				else
				{
					combined->expr.u.swizzle.indices[c] = -1;
				}
			}
			_essl_swizzle_patch_dontcares(&combined->expr.u.swizzle, size);
		}
		ESSL_CHECK(combined = optimise_vector_ops_single_node(ctx, combined));

		if(nodes[i].last.type_conv != NULL)
		{
			node *type_conv;
			ESSL_CHECK(type_conv = _essl_new_type_convert_expression(ctx->pool, EXPR_OP_EXPLICIT_TYPE_CONVERT, combined));
			_essl_ensure_compatible_node(type_conv, n);
			ESSL_CHECK(type_conv->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, nodes[i].last.type_conv, size));
			ESSL_CHECK(type_conv = optimise_vector_ops_single_node(ctx, type_conv));

			combined = type_conv;
		}

		if(n_nodes == 2) 
		{
			return combined;
		}


		{
			int new_to_old_child_indices[N_COMPONENTS];
			node *combiner;
			unsigned ch;
			for(ch = 0; ch < n_nodes; ++ch)
			{
				if(ch < i)
				{
					new_to_old_child_indices[ch+1] = ch;
				} else if(ch == i)
				{
					continue;
				} else if(ch < j)
				{
					new_to_old_child_indices[ch] = ch;
				} else if(ch == j) 
				{
					continue;
				} else {
					new_to_old_child_indices[ch-1] = ch;
				}
			}
			ESSL_CHECK(combiner = _essl_new_vector_combine_expression(ctx->pool, n_nodes-1));
			_essl_ensure_compatible_node(combiner, n);

			SET_CHILD(combiner, 0, combined);

			for(ch = 1; ch < n_nodes-1; ++ch)
			{
				unsigned old_idx = new_to_old_child_indices[ch];
				node *child = nodes[old_idx].first_after_last_vector_combine.n;
				node *type_conv = child;
				node *swz;
				if(nodes[old_idx].first_after_last_vector_combine.type_conv != NULL)
				{
					ESSL_CHECK(type_conv = _essl_new_type_convert_expression(ctx->pool, EXPR_OP_EXPLICIT_TYPE_CONVERT, child));
					_essl_ensure_compatible_node(type_conv, child);
					ESSL_CHECK(type_conv->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, 
											nodes[old_idx].first_after_last_vector_combine.type_conv, 
											GET_NODE_VEC_SIZE(child)));
					ESSL_CHECK(type_conv = optimise_vector_ops_single_node(ctx, type_conv));
				}

				ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, type_conv));
				_essl_ensure_compatible_node(swz, n);
				swz->expr.u.swizzle = nodes[old_idx].first_after_last_vector_combine.swz;
				ESSL_CHECK(swz = optimise_vector_ops_single_node(ctx, swz));
				SET_CHILD(combiner, ch, swz);
			}

			for(c = 0; c < N_COMPONENTS; ++c)
			{
				combiner->expr.u.combiner.mask[c] = 0;
				for(ch = 1; ch < n_nodes-1; ++ch)
				{
					if(nodes[new_to_old_child_indices[ch]].first_after_last_vector_combine.swz.indices[c] != -1)
					{
						combiner->expr.u.combiner.mask[c] = ch;
						break;
					}
				}
			}
			return optimise_vector_ops_single_node(ctx, combiner);
		}

	}
	return n;
}

static node * handle_pow(optimise_vector_ops_context *ctx, node *n)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(n, 0));
	if(a->hdr.kind == EXPR_KIND_CONSTANT 
		&& _essl_is_node_all_value(ctx->desc, a, 1.0))
	{
		return optimise_vector_ops_single_node(ctx, a);
	}

	return n;
}

/**
 * @brief checks if @comp_id component of @n is equal to zero
 */
static memerr is_component_fp_zero(optimise_vector_ops_context *ctx, node *n, unsigned comp_id, essl_bool *res)
{
	*res = ESSL_FALSE;
	if(n->hdr.kind == EXPR_KIND_CONSTANT)
	{
		if(ctx->desc->scalar_to_float(n->expr.u.value[comp_id]) == 0.0)
		{
			*res = ESSL_TRUE;
		}
	}
	else if(n->hdr.kind == EXPR_KIND_VECTOR_COMBINE)
	{
		unsigned cid;
		node *child;
		cid = n->expr.u.combiner.mask[comp_id];
		ESSL_CHECK(child = GET_CHILD(n, cid));
		ESSL_CHECK(is_component_fp_zero(ctx, child, comp_id, res));
	}
	else if(n->hdr.kind == EXPR_KIND_UNARY 
		&& n->expr.operation == EXPR_OP_SWIZZLE)
	{
		node *child;
		unsigned cid;
		ESSL_CHECK(child = GET_CHILD(n, 0));
		cid = n->expr.u.swizzle.indices[comp_id];
		ESSL_CHECK(is_component_fp_zero(ctx, child, cid, res));
	}

	return MEM_OK;

}

/**
 * @brief counts the number of components equal to zero and returns 
 * 	a mask for the node where bits corresponding to non-zero components are set to 1
 */
static memerr count_node_zero_eq_components(optimise_vector_ops_context *ctx, node *n, unsigned n_comps, unsigned *mask)
{
	unsigned i;
	for(i = 0; i < n_comps; ++i)
	{
		essl_bool is_zero;
		ESSL_CHECK(is_component_fp_zero(ctx, n, i, &is_zero));
		if(is_zero)
		{
			(*mask) &= ~(1 << i);
		}
	}
	return MEM_OK;
}

/**
 * @brief creates new 'dot' operation based on 
 * 	the given 'dot' (@res)
 * 	its children (@a, @b)
 * 	and @mask that shows which components anre non-zero
 *
 */
static node * create_reduced_dot(optimise_vector_ops_context *ctx, node *res,
													node *a, node *b, unsigned mask)
{
	node *dot;
	if(mask == 0)
	{
		/* all components are equal to zero */
		ESSL_CHECK(dot = create_float_constant(ctx, 0.0, 1, _essl_get_scalar_size_for_type(res->hdr.type))); 
		return dot;
	}
	else
	{
		unsigned i, j;
		swizzle_pattern swz;
		unsigned n_comps;
		node *swz_a;
		node *swz_b;

		n_comps = GET_NODE_VEC_SIZE(a);
		assert(n_comps == GET_NODE_VEC_SIZE(b));
		swz = _essl_create_undef_swizzle();
		j = 0;
		for(i = 0; i < n_comps; i++)
		{
			if((mask & (1 << i)) != 0)
			{
				swz.indices[j] = i;
				j++;
			}
		}
		ESSL_CHECK(swz_a = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, a));
		_essl_ensure_compatible_node(swz_a, a);
		swz_a->expr.u.swizzle = swz;
		ESSL_CHECK(swz_a->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, a->hdr.type, j));
		ESSL_CHECK(swz_a = optimise_vector_ops_single_node(ctx, swz_a));

		ESSL_CHECK(swz_b = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, b));
		_essl_ensure_compatible_node(swz_b, b);
		swz_b->expr.u.swizzle = swz;
		ESSL_CHECK(swz_b->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, b->hdr.type, j));
		ESSL_CHECK(swz_b = optimise_vector_ops_single_node(ctx, swz_b));

		ESSL_CHECK(dot = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, swz_a, swz_b, NULL));
		_essl_ensure_compatible_node(dot, res);
		ESSL_CHECK(dot = optimise_vector_ops_single_node(ctx, dot));
		return dot;
	}
}


/**
 * @brief if a multiplier equals to zero reduce dot to a smaller vector size
 *
 * Transform:
 * %1 = swizzle <4 x f32> 0s012~ %0
 * %2 = constant <4 x f32> (0x0, 0x0, 0x0, 0x0)
 * %3 = vector_combine <4 x f32> 0c0001 %1, %2
 * %5 = dot <1 x f32> %4, %3
 *
 * to
 *
 * %1 = swizzle <3 x f32> 0s012 %0
 * %2 = swizzle <3 x f32> 0s012 %4
 * %5 = dot <1 x f32> %2, %1
 */
static node * handle_dot(optimise_vector_ops_context *ctx, node *n)
{
	node *a;
	node *b;
	unsigned n_comps;
	unsigned mask_et;
	unsigned mask;
	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(b = GET_CHILD(n, 1));

	n_comps = GET_NODE_VEC_SIZE(a);
	mask_et = mask = (1 << n_comps) -1;

	ESSL_CHECK(count_node_zero_eq_components(ctx, b, n_comps, &mask));
	if(mask_et != mask)
	{
		return create_reduced_dot(ctx, n, a, b, mask);
	}
	else
	{
		mask = mask_et;
		ESSL_CHECK(count_node_zero_eq_components(ctx, a, n_comps, &mask));
		if(mask_et != mask)
		{
			return create_reduced_dot(ctx, n, a, b, mask);
		}
		else
		{
			return n;
		}
	}

	return n;
}


static node * handle_add(optimise_vector_ops_context *ctx, node *n)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(b = GET_CHILD(n, 1));
	if(a->hdr.kind == EXPR_KIND_CONSTANT 
		&& _essl_is_node_all_value(ctx->desc, a, 0.0)
		&& (GET_NODE_VEC_SIZE(n) == GET_NODE_VEC_SIZE(b)
			|| GET_NODE_VEC_SIZE(b) == 1)
		)
	{
		node *new_res;
		unsigned vec_size = GET_NODE_VEC_SIZE(b);
		new_res = b;
		if(vec_size == 1)
		{
			unsigned i;
			ESSL_CHECK(new_res = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, b));
			_essl_ensure_compatible_node(new_res, n);
			for(i = 0; i < GET_NODE_VEC_SIZE(n); i++)
			{
				new_res->expr.u.swizzle.indices[i] = 0;
			}
		}
		return optimise_vector_ops_single_node(ctx, new_res);
	}
	else if(b->hdr.kind == EXPR_KIND_CONSTANT 
		&& _essl_is_node_all_value(ctx->desc, b, 0.0)
		&& (GET_NODE_VEC_SIZE(n) == GET_NODE_VEC_SIZE(a)
			|| GET_NODE_VEC_SIZE(a) == 1)
		)
	{
		node *new_res;
		unsigned vec_size = GET_NODE_VEC_SIZE(a);
		new_res = a;
		if(vec_size == 1)
		{
			unsigned i;
			ESSL_CHECK(new_res = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, a));
			_essl_ensure_compatible_node(new_res, n);
			for(i = 0; i < GET_NODE_VEC_SIZE(n); i++)
			{
				new_res->expr.u.swizzle.indices[i] = 0;
			}
		}
		return optimise_vector_ops_single_node(ctx, new_res);
	}
	else
	{
		return n;
	}
}

static node * handle_mul(optimise_vector_ops_context *ctx, node *n)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(b = GET_CHILD(n, 1));
	if(a->hdr.kind == EXPR_KIND_CONSTANT 
		&& _essl_is_node_all_value(ctx->desc, a, 0.0)
		&& (GET_NODE_VEC_SIZE(n) == GET_NODE_VEC_SIZE(a)
			|| GET_NODE_VEC_SIZE(a) == 1)
		)
	{
		node *new_res;
		unsigned vec_size = GET_NODE_VEC_SIZE(a);
		new_res = a;
		if(vec_size == 1)
		{
			unsigned i;
			ESSL_CHECK(new_res = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, a));
			_essl_ensure_compatible_node(new_res, n);
			for(i = 0; i < GET_NODE_VEC_SIZE(n); i++)
			{
				new_res->expr.u.swizzle.indices[i] = 0;
			}
		}
		return optimise_vector_ops_single_node(ctx, new_res);
	}
	else if(b->hdr.kind == EXPR_KIND_CONSTANT 
		&& _essl_is_node_all_value(ctx->desc, b, 0.0)
		&& (GET_NODE_VEC_SIZE(n) == GET_NODE_VEC_SIZE(b)
			|| GET_NODE_VEC_SIZE(b) == 1)
		)
	{
		node *new_res;
		unsigned vec_size = GET_NODE_VEC_SIZE(b);
		new_res = b;
		if(vec_size == 1)
		{
			unsigned i;
			ESSL_CHECK(new_res = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, b));
			_essl_ensure_compatible_node(new_res, n);
			for(i = 0; i < GET_NODE_VEC_SIZE(n); i++)
			{
				new_res->expr.u.swizzle.indices[i] = 0;
			}
		}
		return optimise_vector_ops_single_node(ctx, new_res);
	}
	else
	{
		return n;
	}
}


/**
 * handling compare created for gl_Position
 */
static  node * handle_eq(optimise_vector_ops_context *ctx, node *n)
{
	if(n->hdr.gl_pos_eq)
	{
		node *a;
		node *b;
		ESSL_CHECK(a = GET_CHILD(n, 0));
		ESSL_CHECK(b = GET_CHILD(n, 1));
		if(a->hdr.kind == EXPR_KIND_UNARY 
			&& a->expr.operation == EXPR_OP_SWIZZLE
			&& b->hdr.kind == EXPR_KIND_UNARY
			&& b->expr.operation == EXPR_OP_SWIZZLE)
		{
			node *ac;
			node *bc;
			ESSL_CHECK(ac = GET_CHILD(a, 0));
			ESSL_CHECK(bc = GET_CHILD(b, 0));
			if(ac == bc 
				&& GET_NODE_VEC_SIZE(a) == GET_NODE_VEC_SIZE(b)
				&& GET_NODE_VEC_SIZE(a) == 1)
			{
				node *new_res;
				unsigned int nsize = _essl_get_type_size(n->hdr.type);
				ESSL_CHECK(new_res = _essl_new_constant_expression(ctx->pool, nsize));
				_essl_ensure_compatible_node(new_res, n);
				if(a->expr.u.swizzle.indices[0] == b->expr.u.swizzle.indices[0])
				{
					new_res->expr.u.value[0] = ctx->desc->bool_to_scalar(ESSL_TRUE);
				}
				else
				{
					new_res->expr.u.value[0] = ctx->desc->bool_to_scalar(ESSL_FALSE);
				}
				return optimise_vector_ops_single_node(ctx, new_res);
			}
		}
	}

	return n;
}

static node * handle_conditional_select(optimise_vector_ops_context *ctx, node *n)
{
	node *a;
	node *b;
	node *c;

	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(b = GET_CHILD(n, 1));
	ESSL_CHECK(c = GET_CHILD(n, 2));
	if(a->hdr.kind == EXPR_KIND_CONSTANT)
	{
		node *new_res;
		int val;

		val = ctx->desc->scalar_to_bool(a->expr.u.value[0]);
		if (val != 0)
		{
			new_res = b;
		} else 
		{
			new_res = c;
		}
		return optimise_vector_ops_single_node(ctx, new_res);

	}else
	{
		return n;
	}
}

static  node * optimise_vector_ops_single_node(optimise_vector_ops_context * ctx, node *n) 
{
	node_kind kind;
	expression_operator op;
	if(n == NULL)
	{
		return NULL;
	}
	kind = n->hdr.kind;
	op = n->expr.operation;

	if(kind == EXPR_KIND_UNARY 
		&& op == EXPR_OP_SWIZZLE)
	{
		node *a = GET_CHILD(n, 0);
		assert(a != NULL);
		if(a->hdr.kind == EXPR_KIND_UNARY
			&& a->expr.operation == EXPR_OP_SWIZZLE)
		{
			return handle_swizzle_of_swizzle(ctx, n, a);
		}else if(is_identity_swizzle(a, n))
		{
			return a;
		}
	}
	else if(kind == EXPR_KIND_BINARY
			&& op == EXPR_OP_SUBVECTOR_ACCESS)
	{
		return handle_subvector_access(ctx, n);
	}
	else if(kind == EXPR_KIND_TERNARY
			&& op == EXPR_OP_SUBVECTOR_UPDATE)
	{
		return handle_subvector_update(ctx, n);
	}
	else if(kind == EXPR_KIND_VECTOR_COMBINE)
	{
		return handle_vector_combine(ctx, n);
	}
	else if(kind == EXPR_KIND_BUILTIN_FUNCTION_CALL
			&& op == EXPR_OP_FUN_POW)
	{
		return handle_pow(ctx, n);
	}
	else if(kind == EXPR_KIND_BUILTIN_FUNCTION_CALL
			&& op == EXPR_OP_FUN_DOT)
	{
		return handle_dot(ctx, n);
	}
	else if(kind == EXPR_KIND_BINARY
			&& op == EXPR_OP_ADD)
	{
		return handle_add(ctx, n);
	}
	else if(kind == EXPR_KIND_BINARY
			&& op == EXPR_OP_ADD)
	{
		return handle_mul(ctx, n);
	}
	else if(kind == EXPR_KIND_BINARY
			&& op == EXPR_OP_EQ)
	{
		return handle_eq(ctx, n);
	}
	else if(kind == EXPR_KIND_TERNARY 
			&& op == EXPR_OP_CONDITIONAL_SELECT)
	{
		return handle_conditional_select(ctx, n);
	}
	else if( kind == EXPR_KIND_TRANSFER)
	{
		return GET_CHILD(n, 0);
	}

	return n;

}

static node *process_node_w(optimise_vector_ops_context *ctx, node *n)
{
	node *nn;
	ESSL_CHECK(nn = optimise_vector_ops_single_node(ctx, n));
	return nn;
}

static node *process_node(optimise_vector_ops_context *ctx, node *n)
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
				ESSL_CHECK(tmp = process_node(ctx, src->source));
				src->source = tmp;
			}
		}

	} else {
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			node *child = GET_CHILD(n, i);
			if(child->hdr.kind != EXPR_KIND_PHI)
			{
				ESSL_CHECK(child = process_node(ctx, child));
				SET_CHILD(n, i, child);
			}
		}
	}	
	ESSL_CHECK(nn = process_node_w(ctx, n));
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, n, nn));
	return nn;
}



static memerr handle_block(optimise_vector_ops_context *ctx, basic_block *b)
{
	phi_list *phi;
	control_dependent_operation *c_op;
	ctx->current_block = b;
	if(b->source)
	{
		node *tmp;
		ESSL_CHECK(tmp = process_node(ctx, b->source));
		b->source = tmp;
	}
	for(c_op = b->control_dependent_ops; c_op != 0; c_op = c_op->next)
	{
		node *tmp;
		ESSL_CHECK(tmp = process_node(ctx, c_op->op));
		if(tmp->hdr.is_control_dependent)
		{
			c_op->op = tmp;
		}
	}

	for(phi = b->phi_nodes; phi != 0; phi = phi->next)
	{		
		node *tmp;
		ESSL_CHECK(tmp = process_node(ctx, phi->phi_node));
		phi->phi_node = tmp;
	}

	return MEM_OK;
}



memerr _essl_optimise_vector_ops(pass_run_context *pr_ctx, symbol *func)
{

	optimise_vector_ops_context ctx;
	unsigned int i;

	ctx.pool = pr_ctx->pool;
	ctx.cfg = func->control_flow_graph;
	ctx.desc = pr_ctx->desc;
	ctx.typestor_ctx = pr_ctx->ts_ctx;

	ESSL_CHECK(_essl_ptrdict_init(&ctx.visited, pr_ctx->tmp_pool));
	for (i = 0 ; i < ctx.cfg->n_blocks ; i++) 
	{
		ESSL_CHECK(handle_block(&ctx, ctx.cfg->postorder_sequence[i]));
	}
	return MEM_OK;
}
