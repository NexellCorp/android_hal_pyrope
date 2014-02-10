/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "mali200/mali200_preschedule.h"
#include "backend/extra_info.h"
#include "common/symbol_table.h"
#include "mali200/mali200_grad.h"
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
	translation_unit *tu;
} mali200_preschedule_context;

static node * mali200_preschedule_single_node(mali200_preschedule_context * ctx, node *n);

static node *get_swizzled_node(node *n, swizzle_pattern *swz_out)
{
	int size = _essl_get_type_size(n->hdr.type);
	swizzle_pattern swz = _essl_create_identity_swizzle(size);
	while (n->hdr.kind == EXPR_KIND_UNARY && n->expr.operation == EXPR_OP_SWIZZLE)
	{
		swz = _essl_combine_swizzles(swz, n->expr.u.swizzle);
		n = GET_CHILD(n,0);
	}
	*swz_out = swz;
	return n;
}

/*@null@*/ static node *create_float_constant(mali200_preschedule_context *ctx, float value, unsigned vec_size)
{
	node *n;
	unsigned i;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, vec_size));
	n->expr.u.value[0] = ctx->desc->float_to_scalar(value);
	for(i = 1; i < vec_size; ++i)
	{
		n->expr.u.value[i] = n->expr.u.value[0];
	}

	ESSL_CHECK(n->hdr.type = _essl_get_type_with_default_size_for_target(ctx->typestor_ctx, TYPE_FLOAT, vec_size, ctx->desc));
	return n;
}


/*
  See if a node is a vector swizzle on the form .yyyy, and rewrite into a scalar swizzle .y in that case. Used by scalar-vector multiplication optimization
  Returns: new swizzle if rewrite was possible, old swizzle if not, NULL on out of memory.
 */
static node *try_rewrite_vector_swizzle_to_scalar_swizzle(mali200_preschedule_context *ctx, node *swz)
{
	unsigned i;
	node *res;
	int common_element = -1;
	if(swz->hdr.kind != EXPR_KIND_UNARY || swz->expr.operation != EXPR_OP_SWIZZLE) return swz;
	if(GET_NODE_VEC_SIZE(swz) == 1) return swz;
	for(i = 0; i < GET_NODE_VEC_SIZE(swz); ++i)
	{
		int ind = swz->expr.u.swizzle.indices[i];
		if(ind == -1) continue;
		if(common_element == -1) common_element = ind;
		if(ind != common_element) return swz;
	}
	ESSL_CHECK(res = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, GET_CHILD(swz, 0)));
	_essl_ensure_compatible_node(res, swz);
	ESSL_CHECK(res->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, swz->hdr.type, 1));

	res->expr.u.swizzle.indices[0] = common_element;
	return res;

}


static memerr process_address(mali200_preschedule_context *ctx, node *access, node_extra *access_info, node *address, nodeptr *res)
{
	assert(address->hdr.type != 0);
    if(address->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		symbol_list *lst;
		ESSL_CHECK(lst = _essl_mempool_alloc(ctx->pool, sizeof(*lst)));
		lst->sym = address->expr.u.sym;
		LIST_INSERT_BACK(&access_info->address_symbols, lst);
        *res = 0;
	} else if(address->hdr.kind == EXPR_KIND_UNARY && address->expr.operation == EXPR_OP_MEMBER)
	{
		node *child0;
		ESSL_CHECK(child0 = GET_CHILD(address, 0));
        access_info->address_offset += ctx->desc->get_type_member_offset(ctx->desc, address->expr.u.member, access->expr.u.load_store.address_space);
        return process_address(ctx, access, access_info, child0, res);
	} else if(address->hdr.kind == EXPR_KIND_BINARY && address->expr.operation == EXPR_OP_INDEX)
	{
		node *child0 = GET_CHILD(address, 0);
		node *child1 = GET_CHILD(address, 1);
		assert(child0 != 0);
		assert(child1 != 0 && child1->hdr.type != 0);
		access_info->is_indexed = 1;
        if(child1->hdr.kind == EXPR_KIND_CONSTANT)
		{

            access_info->address_offset += ctx->desc->scalar_to_int(child1->expr.u.value[0]) * ctx->desc->get_array_stride(ctx->desc, address->hdr.type, access->expr.u.load_store.address_space);
            return process_address(ctx, access, access_info, child0, res);
		} else
		{
			unsigned scale = ctx->desc->get_array_stride(ctx->desc, address->hdr.type, access->expr.u.load_store.address_space) / access_info->address_multiplier;
			node *mul = 0;
            if(scale != 1)
			{
				node *constant;
				ESSL_CHECK(constant = _essl_new_constant_expression(ctx->pool, 1));
				constant->expr.u.value[0] = ctx->desc->int_to_scalar((int)scale);
				_essl_ensure_compatible_node(constant, child1);
				ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, child1, EXPR_OP_MUL, constant));
				_essl_ensure_compatible_node(mul, child1);
			} else
			{
                mul = child1;
			}

            ESSL_CHECK(process_address(ctx, access, access_info, child0, res));
            if(*res != 0)
			{
				ESSL_CHECK(*res = _essl_new_binary_expression(ctx->pool, *res, EXPR_OP_ADD, mul));
				_essl_ensure_compatible_node(*res, child1);
			} else {
				*res = mul;
			}
		}
	}
	return MEM_OK;
}

static node *find_sampler_for_texture_node(node_extra *tex_node_extra, node *orig_sampler, node_extra **sampler_extra_p)
{
	node *sampler;
	node *sampler_pred;
	node_extra *sampler_extra = NULL;

	assert(orig_sampler->hdr.kind == EXPR_KIND_UNARY && orig_sampler->expr.operation == EXPR_OP_SWIZZLE);

	sampler = orig_sampler;

	sampler_pred = GET_CHILD(orig_sampler, 0);
	if (sampler_pred->hdr.kind == EXPR_KIND_LOAD &&
		sampler_pred->expr.u.load_store.address_space == ADDRESS_SPACE_UNIFORM)
	{
		node_extra *load_extra;
		ESSL_CHECK(load_extra = EXTRA_INFO(sampler_pred));
		if (load_extra->address_symbols != NULL && load_extra->address_symbols->next == NULL && _essl_is_optimized_sampler_symbol(load_extra->address_symbols->sym))
		{
			/* This is a direct load from a uniform sampler or sampler array.
			   Use the address of the sampler as the sampler index.
			*/
			tex_node_extra->address_multiplier = load_extra->address_multiplier;
			tex_node_extra->address_offset = load_extra->address_offset;
			tex_node_extra->address_symbols = load_extra->address_symbols;
			sampler_extra = load_extra;
			sampler = GET_CHILD(sampler_pred, 0);
		}
	}
	if(sampler_extra_p != NULL)
	{
		*sampler_extra_p = sampler_extra;
	}

	return sampler;
}

static node *find_coord_for_texture_node(node *orig_coord)
{
	node *coord;

	coord = orig_coord;
	while(1)
	{
		if(coord->hdr.kind == EXPR_KIND_UNARY && coord->expr.operation == EXPR_OP_SWIZZLE && _essl_is_identity_swizzle_sized(coord->expr.u.swizzle, GET_NODE_VEC_SIZE(coord)))
		{
			node *ch = GET_CHILD(coord, 0);
			assert(ch != 0);
			coord = ch;
			
		} else {
			break;
		}
	}

	return coord;

}

static node * read_texture_width_and_height(mali200_preschedule_context *ctx, node *vec_of_sampler_idx)
{
	scope * global_scope;
	symbol *sizes_sym;
	node *sampler_grd_idx;
	node *ref_to_sizes;
	node *address;
	node *load;

	global_scope = ctx->tu->root->stmt.child_scope;

	ESSL_CHECK(sizes_sym = _essl_symbol_table_lookup(global_scope, _essl_cstring_to_string_nocopy(TEXTURE_GRADEXT_SIZES_NAME)));
	ESSL_CHECK(sampler_grd_idx = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, vec_of_sampler_idx));
	_essl_ensure_compatible_node(sampler_grd_idx, vec_of_sampler_idx);
	ESSL_CHECK(sampler_grd_idx->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, sampler_grd_idx->hdr.type, 1));
	/* we agreed with the driver that the index in the Texture Sizes table is stored in the .z comp */
	sampler_grd_idx->expr.u.swizzle = _essl_create_scalar_swizzle(2);
	ESSL_CHECK(sampler_grd_idx = mali200_preschedule_single_node(ctx, sampler_grd_idx));

	ESSL_CHECK(ref_to_sizes = _essl_new_variable_reference_expression(ctx->pool, sizes_sym));
	ref_to_sizes->hdr.type = sizes_sym->type;
	ESSL_CHECK(ref_to_sizes = mali200_preschedule_single_node(ctx, ref_to_sizes));

	ESSL_CHECK(address = _essl_new_binary_expression(ctx->pool, ref_to_sizes, EXPR_OP_INDEX, sampler_grd_idx));
	_essl_ensure_compatible_node(address, ref_to_sizes);
	ESSL_CHECK(address->hdr.type = ref_to_sizes->hdr.type->child_type);
	ESSL_CHECK(address = mali200_preschedule_single_node(ctx, address));

	ESSL_CHECK(load = _essl_new_load_expression(ctx->pool, ADDRESS_SPACE_UNIFORM, address));
	_essl_ensure_compatible_node(load, address);
	ESSL_CHECK(load = mali200_preschedule_single_node(ctx, load));

	return load;

}

static node * calc_lod_from_derivatives_and_texture_params(mali200_preschedule_context *ctx, node *texture_wh, node *dx, node *dy)
{
	node *dx_sq;
	node *dy_sq;
	node *m0;
	node *m1;
	node *max_log2;
	node *max;
	node *constant;
	node *lod;
	node *texture_wh2;

	/**
	 *lod = 0.5 * log2(max ( TEX*GRAD ) )

		TEX is a 2x1 matrix:
			TEX = | wt*wt | ht*ht |
		
		GRAD is a 2x2 matrix containing the derivatives for the 2D texture we want to sample

		GRAD =	| dPdx.s*dPdx.s | dPdy.s*dPdy.s |
				| dPdx.t*dPdx.t | dPdy.t*dPdy.t |
	 */

	ESSL_CHECK(texture_wh2 = _essl_new_binary_expression(ctx->pool, texture_wh, EXPR_OP_MUL, texture_wh));
	_essl_ensure_compatible_node(texture_wh2, texture_wh);
	ESSL_CHECK(texture_wh2 = mali200_preschedule_single_node(ctx, texture_wh2));

	/* dPdx^2 = (dPdx.s*dPdx.s, dPdx.t*dPdx.t) */
	ESSL_CHECK(dx_sq = _essl_new_binary_expression(ctx->pool, dx, EXPR_OP_MUL, dx));
	_essl_ensure_compatible_node(dx_sq, dx);
	ESSL_CHECK(dx_sq = mali200_preschedule_single_node(ctx, dx_sq));

	/* dPdy^2 = (dPdy.s*dPdy.s, dPdy.t*dPdy.t) */
	ESSL_CHECK(dy_sq = _essl_new_binary_expression(ctx->pool, dy, EXPR_OP_MUL, dy));
	_essl_ensure_compatible_node(dy_sq, dy);
	ESSL_CHECK(dy_sq = mali200_preschedule_single_node(ctx, dy_sq));

	/* m0 = dPdx^2 * TEX */
	ESSL_CHECK(m0 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, texture_wh2, dx_sq, NULL));
	_essl_ensure_compatible_node(m0, dx_sq);
	ESSL_CHECK(m0->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, dx_sq->hdr.type, 1));
	ESSL_CHECK(m0 = mali200_preschedule_single_node(ctx, m0));

	/* m1 = dPdx^2 * TEX */
	ESSL_CHECK(m1 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, texture_wh2, dy_sq, NULL));
	_essl_ensure_compatible_node(m1, dy_sq);
	ESSL_CHECK(m1->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, dy_sq->hdr.type, 1));
	ESSL_CHECK(m1 = mali200_preschedule_single_node(ctx, m1));

	/* max = max(m0, m1) */
	ESSL_CHECK(max = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, m0, m1, NULL));
	_essl_ensure_compatible_node(max, m1);
	ESSL_CHECK(max = mali200_preschedule_single_node(ctx, max));

	/* log = log2(max(m0, m1)) */
	ESSL_CHECK(max_log2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_LOG2, max, NULL, NULL));
	_essl_ensure_compatible_node(max_log2, max);
	ESSL_CHECK(max_log2 = mali200_preschedule_single_node(ctx, max_log2));

	/* lod = 0.5*log2 */
	ESSL_CHECK(constant = create_float_constant(ctx, 0.5, 1));
	ESSL_CHECK(lod = _essl_new_binary_expression(ctx->pool, max_log2, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(lod, max_log2);
	ESSL_CHECK(lod = mali200_preschedule_single_node(ctx, lod));

	return lod;

}

static node * create_cubemap_transform(mali200_preschedule_context *ctx,
										node *coord)
{
	node *input;
	node *input_arg;
	const type_specifier * input_arg_type;

	ESSL_CHECK(input_arg_type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, coord->hdr.type, 3));
	ESSL_CHECK(input_arg = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, coord));
	input_arg->hdr.type = input_arg_type;
	input_arg->expr.u.swizzle = _essl_create_identity_swizzle(3);
	ESSL_CHECK(input_arg = mali200_preschedule_single_node(ctx, input_arg));

	ESSL_CHECK(input = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_MOV_CUBEMAP, input_arg, NULL, NULL));
	_essl_ensure_compatible_node(input, input_arg);
	ESSL_CHECK(input->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, input_arg->hdr.type, 4));
	ESSL_CHECK(input = mali200_preschedule_single_node(ctx, input));

	/* the .w component contains the face */
	return input;

}

static node *  make_dividend_for_texturegrad_cubemap(mali200_preschedule_context *ctx,
										node *value_vector,
										node *dividend_indices,
										node *dividend_signs)
{
	node *dividend;
	node *dividend_orig;
	node *swz;
	node *swz_0c;
	node *swz_1c;
	node *dividend_0_orig;
	node *dividend_1_orig;

	assert(GET_NODE_VEC_SIZE(dividend_indices) == 2);
	ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, dividend_indices));
	_essl_ensure_compatible_node(swz, dividend_indices);
	ESSL_CHECK(swz->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, dividend_indices->hdr.type, 1));
	swz->expr.u.swizzle = _essl_create_scalar_swizzle(0);
	ESSL_CHECK(swz = mali200_preschedule_single_node(ctx, swz));

	/* EXPR_OP_SUBVECTOR_ACCESS can handle only scalar 2nd operand */
	ESSL_CHECK(dividend_0_orig = _essl_new_binary_expression(ctx->pool, value_vector, EXPR_OP_SUBVECTOR_ACCESS, swz));
	_essl_ensure_compatible_node(dividend_0_orig, value_vector);
	ESSL_CHECK(dividend_0_orig->hdr.type =  _essl_get_type_with_given_vec_size(ctx->typestor_ctx, value_vector->hdr.type, 1));
	ESSL_CHECK(dividend_0_orig = mali200_preschedule_single_node(ctx, dividend_0_orig));

	ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, dividend_indices));
	_essl_ensure_compatible_node(swz, dividend_indices);
	ESSL_CHECK(swz->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, dividend_indices->hdr.type, 1));
	swz->expr.u.swizzle = _essl_create_scalar_swizzle(1);
	ESSL_CHECK(swz = mali200_preschedule_single_node(ctx, swz));

	ESSL_CHECK(dividend_1_orig = _essl_new_binary_expression(ctx->pool, value_vector, EXPR_OP_SUBVECTOR_ACCESS, swz));
	_essl_ensure_compatible_node(dividend_1_orig, value_vector);
	ESSL_CHECK(dividend_1_orig->hdr.type =  _essl_get_type_with_given_vec_size(ctx->typestor_ctx, value_vector->hdr.type, 1));
	ESSL_CHECK(dividend_1_orig = mali200_preschedule_single_node(ctx, dividend_1_orig));

	ESSL_CHECK(swz_0c = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, dividend_0_orig));
	_essl_ensure_compatible_node(swz_0c, dividend_0_orig);
	ESSL_CHECK(swz_0c->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, dividend_0_orig->hdr.type, 2));
	swz_0c->expr.u.swizzle.indices[0] = 0;
	swz_0c->expr.u.swizzle.indices[1] = 0;
	ESSL_CHECK(swz_0c = mali200_preschedule_single_node(ctx, swz_0c));

	ESSL_CHECK(swz_1c = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, dividend_1_orig));
	_essl_ensure_compatible_node(swz_1c, dividend_1_orig);
	ESSL_CHECK(swz_1c->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, dividend_1_orig->hdr.type, 2));
	swz_1c->expr.u.swizzle.indices[0] = 0;
	swz_1c->expr.u.swizzle.indices[1] = 0;
	ESSL_CHECK(swz_1c = mali200_preschedule_single_node(ctx, swz_1c));

	ESSL_CHECK(dividend_orig = _essl_new_vector_combine_expression(ctx->pool, 2));
	_essl_ensure_compatible_node(dividend_orig, swz_0c);
	SET_CHILD(dividend_orig, 0, swz_0c);
	SET_CHILD(dividend_orig, 1, swz_1c);
	dividend_orig->expr.u.combiner.mask[0] = 0;
	dividend_orig->expr.u.combiner.mask[1] = 1;
	ESSL_CHECK(dividend_orig = mali200_preschedule_single_node(ctx, dividend_orig));

	/* mul by sign */
	ESSL_CHECK(dividend = _essl_new_binary_expression(ctx->pool, dividend_orig, EXPR_OP_MUL, dividend_signs));
	_essl_ensure_compatible_node(dividend, dividend_orig);
	ESSL_CHECK(dividend = mali200_preschedule_single_node(ctx, dividend));

	return dividend;
}


static node *  calculate_projection(mali200_preschedule_context *ctx,
								node *value_vector,
								node *dividend_indices,
								node *dividend_signs,
								node *divisor_index)
{
	node *dividend;
	node *mul;
	node *half;
	node *divisor;
	node *divisor_scalar;
	node *abs;
	node *div;

	ESSL_CHECK(dividend = make_dividend_for_texturegrad_cubemap(ctx, value_vector, dividend_indices, dividend_signs));

	ESSL_CHECK(half = create_float_constant(ctx, 0.5, 2));
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, dividend,  EXPR_OP_MUL, half));
	_essl_ensure_compatible_node(mul, dividend);
	ESSL_CHECK(mul = mali200_preschedule_single_node(ctx, mul));

	ESSL_CHECK(divisor_scalar = _essl_new_binary_expression(ctx->pool, value_vector, EXPR_OP_SUBVECTOR_ACCESS, divisor_index));
	_essl_ensure_compatible_node(divisor_scalar, value_vector);
	ESSL_CHECK(divisor_scalar->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, value_vector->hdr.type, 1));
	ESSL_CHECK(divisor_scalar = mali200_preschedule_single_node(ctx, divisor_scalar));

	ESSL_CHECK(divisor = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, divisor_scalar));
	_essl_ensure_compatible_node(divisor, divisor_scalar);
	ESSL_CHECK(divisor->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, divisor_scalar->hdr.type, 2));
	divisor->expr.u.swizzle.indices[0] = 0;
	divisor->expr.u.swizzle.indices[1] = 0;
	ESSL_CHECK(divisor = mali200_preschedule_single_node(ctx, divisor));

	ESSL_CHECK(abs = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ABS, divisor, NULL, NULL));
	_essl_ensure_compatible_node(abs, divisor);
	ESSL_CHECK(abs = mali200_preschedule_single_node(ctx, abs));

	ESSL_CHECK(div = _essl_new_binary_expression(ctx->pool, mul, EXPR_OP_DIV, abs));
	_essl_ensure_compatible_node(div, mul);
	ESSL_CHECK(div = mali200_preschedule_single_node(ctx, div));

	return div;
}

static node * handle_div(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	node *divide;
	node *r;
	type_specifier *t;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	/* rewrite as a * (1.0/b) if a != 1.0 */
	ESSL_CHECK(divide = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_RCP, b, NULL, NULL));
	_essl_ensure_compatible_node(divide, b);
	ESSL_CHECK(t = _essl_clone_type(ctx->pool, res->hdr.type));
	t->basic_type = TYPE_FLOAT;
	t->u.basic.vec_size = GET_NODE_VEC_SIZE(b);
	divide->hdr.type = t;
	ESSL_CHECK(divide = mali200_preschedule_single_node(ctx, divide));
	
	if(_essl_is_node_all_value(ctx->desc, a, 1.0))
	{
		r = divide;
		if(GET_NODE_VEC_SIZE(divide) == 1 && GET_NODE_VEC_SIZE(res) > 1)
		{
			unsigned i;
			ESSL_CHECK(r = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, divide));
			_essl_ensure_compatible_node(r, res);
			for(i = 0; i < GET_NODE_VEC_SIZE(res); ++i)
			{
				r->expr.u.swizzle.indices[i] = 0;
			}
			ESSL_CHECK(r = mali200_preschedule_single_node(ctx, r));
		}
	} 
	else 
	{
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, divide));
		_essl_ensure_compatible_node(r, res);
		ESSL_CHECK(r = mali200_preschedule_single_node(ctx, r));
	}

	if(res->hdr.type->basic_type == TYPE_INT)
	{
		node *tmp;
		ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_TRUNC, r, 0, 0));
		_essl_ensure_compatible_node(tmp, res);
		return mali200_preschedule_single_node(ctx, tmp);
	} 
	else 
	{
		return r;
	}
}

static node * handle_add(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	unsigned int size;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));

	size = _essl_get_type_size(res->hdr.type);
	if(size <= 2)
	{
		/* check for pairwise horizontal add pattern */
		node *child_add, *child_non_add, *child_in1, *child_in2;
		swizzle_pattern child_out_swz, child_swz1, child_swz2;
		essl_bool match = ESSL_FALSE;
		/* Try left child */
		child_add = get_swizzled_node(a, &child_out_swz);
		if (child_add->hdr.kind == EXPR_KIND_BINARY && child_add->expr.operation == EXPR_OP_ADD)
		{
			child_non_add = b;

			child_in1 = get_swizzled_node(GET_CHILD(child_add,0), &child_swz1);
			child_in2 = get_swizzled_node(GET_CHILD(child_add,1), &child_swz2);

			/* Check that the add inputs are the same */
			match = child_in1 == child_in2;
		}

		if (!match)
		{
			/* Try right child */
			child_add = get_swizzled_node(b, &child_out_swz);
			if (child_add->hdr.kind == EXPR_KIND_BINARY && child_add->expr.operation == EXPR_OP_ADD)
			{
				child_non_add = a;

				child_in1 = get_swizzled_node(GET_CHILD(child_add,0), &child_swz1);
				child_in2 = get_swizzled_node(GET_CHILD(child_add,1), &child_swz2);

				/* Check that the add inputs are the same */
				match = child_in1 == child_in2;
			}
		}

		if (!match)
		{
			return res;
		}

		/* Click. Rewrite into pairwise horizontal add */
		{
			node *hadd_pair, *in_swz;
			int hadd_size = size*2;
			ESSL_CHECK(in_swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, child_in1));
			ESSL_CHECK(in_swz->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, hadd_size));
			in_swz->expr.u.swizzle.indices[0] = child_swz1.indices[child_out_swz.indices[0]];
			in_swz->expr.u.swizzle.indices[1] = child_swz2.indices[child_out_swz.indices[0]];
			if (in_swz->expr.u.swizzle.indices[0] == -1 || in_swz->expr.u.swizzle.indices[1] == -1) 
			{
				return res;
			}
			if (size == 2) 
			{
				in_swz->expr.u.swizzle.indices[2] = child_swz1.indices[child_out_swz.indices[1]];
				in_swz->expr.u.swizzle.indices[3] = child_swz2.indices[child_out_swz.indices[1]];
				if (in_swz->expr.u.swizzle.indices[2] == -1 || in_swz->expr.u.swizzle.indices[3] == -1) 
				{
					return res;
				}
				if(_essl_get_type_size(child_non_add->hdr.type) != size) 
				{
					return res;
				}
			} 
			else 
			{
				in_swz->expr.u.swizzle.indices[2] = -1;
				in_swz->expr.u.swizzle.indices[3] = -1;
			}

			ESSL_CHECK(hadd_pair = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_HADD_PAIR, in_swz, child_non_add, 0));
			_essl_ensure_compatible_node(hadd_pair, res);
			return mali200_preschedule_single_node(ctx, hadd_pair);
		}
	}
	else
	{
		return res;
	}
}

static node * handle_sub(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	node *tmp;
	node *r;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	/* rewrite as a + -b */
	ESSL_CHECK(tmp = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, b));
	_essl_ensure_compatible_node(tmp, b);
	ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));
	ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_ADD, tmp));
	_essl_ensure_compatible_node(r, res);
	return mali200_preschedule_single_node(ctx, r);
}

static node * handle_cmp(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	if(GET_NODE_VEC_SIZE(a) > 1)
	{
		node *all, *fun_cmp;
		expression_operator all_op, cmp_op;
		cmp_op = (res->expr.operation == EXPR_OP_EQ) ? EXPR_OP_FUN_EQUAL : EXPR_OP_FUN_NOTEQUAL;
		all_op = (res->expr.operation == EXPR_OP_EQ) ? EXPR_OP_FUN_ALL : EXPR_OP_FUN_ANY;
		ESSL_CHECK(fun_cmp = _essl_new_builtin_function_call_expression(ctx->pool, cmp_op, a, b, 0));
		_essl_ensure_compatible_node(fun_cmp, a);
		ESSL_CHECK(fun_cmp = mali200_preschedule_single_node(ctx, fun_cmp));
		ESSL_CHECK(all = _essl_new_builtin_function_call_expression(ctx->pool, all_op, fun_cmp, 0, 0));
		_essl_ensure_compatible_node(all, res);
		return mali200_preschedule_single_node(ctx, all);
	}
	else
	{
		return res;
	}
}

static node * handle_mul(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	if(a->hdr.kind == EXPR_KIND_CONSTANT 
		&& b->hdr.kind != EXPR_KIND_CONSTANT)
	{
		node *newres;
		ESSL_CHECK(newres = _essl_new_binary_expression(ctx->pool, b, EXPR_OP_MUL, a));
		_essl_ensure_compatible_node(newres, res);
		return mali200_preschedule_single_node(ctx, newres);
	}
	else
	{
		/* as we can perform scalar-vector multiplies faster than vector-vector multiplies, 
		 * check to see if one or more of the inputs are .yyyy-style swizzles, and rewrite into .y in that case */
		unsigned i;
		node *inputs[2];
		node *tmp;
		essl_bool can_rewrite = ESSL_FALSE;
		inputs[0] = a;
		inputs[1] = b;
		for(i = 0; i < 2; ++i)
		{

			ESSL_CHECK(tmp = try_rewrite_vector_swizzle_to_scalar_swizzle(ctx, inputs[i]));
			if(tmp != inputs[i])
			{
				ESSL_CHECK(inputs[i] = mali200_preschedule_single_node(ctx, tmp));
				can_rewrite = ESSL_TRUE;
			}
		}
		if(can_rewrite)
		{
			node *newres;
			ESSL_CHECK(newres = _essl_new_binary_expression(ctx->pool, inputs[0], EXPR_OP_MUL, inputs[1]));
			_essl_ensure_compatible_node(newres, res);
			return mali200_preschedule_single_node(ctx, newres);
		}
		else
		{
			return res;
		}
	}
}

static node * handle_csel(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	if(!_essl_is_node_comparison(a))
	{
		/* input to csel is not a comparison - make sure it is one by comparing unequal with false */
		node *zero;
		node *cmp;
		node *newres;
		ESSL_CHECK(zero = create_float_constant(ctx, 0.0, 1));
		ESSL_CHECK(cmp = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_NE, zero));
		_essl_ensure_compatible_node(cmp, a);
		ESSL_CHECK(cmp = mali200_preschedule_single_node(ctx, cmp));
		ESSL_CHECK(newres = _essl_clone_node(ctx->pool, res));
		SET_CHILD(newres, 0, cmp);
		return mali200_preschedule_single_node(ctx, newres);
	}
	else
	{
		return res;
	}
}

static node * handle_type_convert(mali200_preschedule_context * ctx, node *res)
{
	enum { CONV_NONE, CONV_BOOL, CONV_INT } conversion_needed = CONV_NONE;
	type_basic res_type = res->hdr.type->basic_type;
	type_basic arg_type;
	node *arg;
	node *conv;
	node *constant;
	ESSL_CHECK(arg = GET_CHILD(res, 0));
	arg_type = arg->hdr.type->basic_type;
	switch(arg_type)
	{
		case TYPE_BOOL:
			break;
		case TYPE_INT:
			if(res_type == TYPE_BOOL)
			{
				conversion_needed = CONV_BOOL;
			}
			break;
		case TYPE_FLOAT:
			if(res_type == TYPE_BOOL)
			{
				conversion_needed = CONV_BOOL;
			}
			if(res_type == TYPE_INT)
			{
				conversion_needed = CONV_INT;
			}
			break;
		default:
			break;
	}
	switch(conversion_needed)
	{
		case CONV_BOOL:
			ESSL_CHECK(constant = create_float_constant(ctx, 0.0, GET_NODE_VEC_SIZE(arg)));
			constant->hdr.source_offset = res->hdr.source_offset;
			ESSL_CHECK(conv = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_NOTEQUAL, arg, constant, 0));
			_essl_ensure_compatible_node(conv, res);
			return mali200_preschedule_single_node(ctx, conv);

		case CONV_INT:
			ESSL_CHECK(conv = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_TRUNC, arg, 0, 0));
			_essl_ensure_compatible_node(conv, res);
			return mali200_preschedule_single_node(ctx, conv);

		default:
			assert(conversion_needed == CONV_NONE);
			return arg;
	}
}

static node * handle_unary_lut_ops(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	if(GET_NODE_VEC_SIZE(res) > 1)
	{
		node *newres = NULL;
		unsigned i;
		unsigned n_comps;
		const type_specifier *t;

		/* vector operation, rewrite as combine(op($a.x), op($a.y), ...) */
		n_comps = GET_NODE_VEC_SIZE(a);

		ESSL_CHECK(t = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, a->hdr.type, 1));
		ESSL_CHECK(newres = _essl_new_vector_combine_expression(ctx->pool, n_comps));
		_essl_ensure_compatible_node(newres, res);
		assert(GET_NODE_VEC_SIZE(a) > 1);
		for(i = 0; i < n_comps; ++i)
		{
			node *swz;
			node *tmp;
			node *tmp2;
			/* first, swizzle in the right component */
			ESSL_CHECK(tmp = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, a));
			_essl_ensure_compatible_node(tmp, a);
			tmp->hdr.type = t;
			tmp->expr.u.swizzle = _essl_create_scalar_swizzle(i);

			ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));
			ESSL_CHECK(tmp2 = _essl_new_builtin_function_call_expression(ctx->pool, res->expr.operation, tmp, 0, 0));
			_essl_ensure_compatible_node(tmp2, tmp);
			ESSL_CHECK(tmp2 = mali200_preschedule_single_node(ctx, tmp2));

			ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, tmp2));
			_essl_ensure_compatible_node(swz, res);
			swz->expr.u.swizzle.indices[i] = 0;
			ESSL_CHECK(swz = mali200_preschedule_single_node(ctx, swz));
			SET_CHILD(newres, i, swz);
			newres->expr.u.combiner.mask[i] = i;
		}

		return mali200_preschedule_single_node(ctx, newres);
	}
	else
	{
		if(res->expr.operation == EXPR_OP_FUN_RCP
			&& a->hdr.kind == EXPR_KIND_CONSTANT)
		{
			/* Constant fold rcp of constant to achieve efficient division by constants */
			scalar_type dummy = ctx->desc->float_to_scalar(0.0);
			scalar_type one = ctx->desc->float_to_scalar(1.0);
			node *newres;
			assert(GET_NODE_VEC_SIZE(a) == 1);
			ESSL_CHECK(newres = _essl_new_constant_expression(ctx->pool, 1));
			_essl_ensure_compatible_node(newres, res);
			newres->expr.u.value[0] = ctx->desc->constant_fold(newres->hdr.type, EXPR_OP_DIV, one, a->expr.u.value[0], dummy);

			return mali200_preschedule_single_node(ctx, newres);
		}
		else
		{
			return res;
		}
	}

}
static node * handle_atan(mali200_preschedule_context * ctx, node *res)
{
	if(GET_NODE_VEC_SIZE(res) > 1)
	{
		node *a;
		node *b;
		node *newres = NULL;
		unsigned i;
		unsigned n_comps;
		const type_specifier *t;

		/* vector operation, rewrite as combine(op($a.x, $b.x), op($a.y, $b.y), ...) */
		ESSL_CHECK(a = GET_CHILD(res, 0));
		if(GET_N_CHILDREN(res) == 1)
		{
			return handle_unary_lut_ops(ctx, res);
		}
		else
		{
			ESSL_CHECK(b = GET_CHILD(res, 1));
			n_comps = GET_NODE_VEC_SIZE(a);
			ESSL_CHECK(t = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, a->hdr.type, 1));
			ESSL_CHECK(newres = _essl_new_vector_combine_expression(ctx->pool, n_comps));
			_essl_ensure_compatible_node(newres, res);

			for(i = 0; i < n_comps; ++i)
			{
				node *swz;
				node *tmp;
				node *tmp2;
				node *tmp3;
				/* first, swizzle in the right component */
				ESSL_CHECK(tmp = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, a));
				_essl_ensure_compatible_node(tmp, a);
				tmp->hdr.type = t;
				tmp->expr.u.swizzle = _essl_create_scalar_swizzle(i);
				ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));

				ESSL_CHECK(tmp2 = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, b));
				_essl_ensure_compatible_node(tmp2, a);
				tmp2->hdr.type = t;
				tmp2->expr.u.swizzle = _essl_create_scalar_swizzle(i);
				ESSL_CHECK(tmp2 = mali200_preschedule_single_node(ctx, tmp2));

				ESSL_CHECK(tmp3 = _essl_new_builtin_function_call_expression(ctx->pool, res->expr.operation, tmp, tmp2, 0));
				_essl_ensure_compatible_node(tmp3, tmp);
				ESSL_CHECK(tmp3 = mali200_preschedule_single_node(ctx, tmp3));

				ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, tmp3));
				_essl_ensure_compatible_node(swz, res);
				swz->expr.u.swizzle.indices[i] = 0;
				ESSL_CHECK(swz = mali200_preschedule_single_node(ctx, swz));

				SET_CHILD(newres, i, swz);
				newres->expr.u.combiner.mask[i] = i;
			}

			return mali200_preschedule_single_node(ctx, newres);
		}
	}
	else
	{
		if(GET_N_CHILDREN(res) == 1)
		{
			node *tmp2;
			node *tmp;
			node *a;
			ESSL_CHECK(a = GET_CHILD(res, 0));
			ESSL_CHECK(tmp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_ATAN_IT1, a, 0, 0));
			_essl_ensure_compatible_node(tmp2, res);
			ESSL_CHECK(tmp2->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 3));
			ESSL_CHECK(tmp2 = mali200_preschedule_single_node(ctx, tmp2));
			ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_ATAN_IT2, tmp2, 0, 0));
			_essl_ensure_compatible_node(tmp, res);
			ESSL_CHECK(tmp->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 1));
			return mali200_preschedule_single_node(ctx, tmp);
		}
		else
		{
			node *tmp2;
			node *cmb, *swz_x, *swz_y, *mul, *it2;
			node *y;
			node *x;
			assert(GET_N_CHILDREN(res) == 2);
			ESSL_CHECK(y = GET_CHILD(res, 0));
			ESSL_CHECK(x = GET_CHILD(res, 1));
			ESSL_CHECK(tmp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_ATAN_IT1, y, x, 0));
			_essl_ensure_compatible_node(tmp2, res);
			ESSL_CHECK(tmp2->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 3));
			ESSL_CHECK(tmp2 = mali200_preschedule_single_node(ctx, tmp2));

			ESSL_CHECK(swz_x = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, tmp2));
			_essl_ensure_compatible_node(swz_x, res);
			ESSL_CHECK(swz_x->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, tmp2->hdr.type, 1));
			swz_x->expr.u.swizzle = _essl_create_scalar_swizzle(0);
			ESSL_CHECK(swz_x = mali200_preschedule_single_node(ctx, swz_x));

			ESSL_CHECK(swz_y = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, tmp2));
			_essl_ensure_compatible_node(swz_y, swz_x);
			swz_y->expr.u.swizzle = _essl_create_scalar_swizzle(1);
			ESSL_CHECK(swz_y = mali200_preschedule_single_node(ctx, swz_y));

			ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, swz_x, EXPR_OP_MUL, swz_y));
			_essl_ensure_compatible_node(mul, swz_x);
			ESSL_CHECK(mul = mali200_preschedule_single_node(ctx, mul));

			ESSL_CHECK(cmb = _essl_new_vector_combine_expression(ctx->pool, 2));
			SET_CHILD(cmb, 0, tmp2);
			SET_CHILD(cmb, 1, mul);
			_essl_ensure_compatible_node(cmb, tmp2);
			cmb->expr.u.combiner.mask[0] = 1;
			cmb->expr.u.combiner.mask[1] = 0;
			cmb->expr.u.combiner.mask[2] = 0;
			ESSL_CHECK(cmb = mali200_preschedule_single_node(ctx, cmb));

			ESSL_CHECK(it2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_ATAN_IT2, cmb, 0, 0));
			_essl_ensure_compatible_node(it2, res);
			return mali200_preschedule_single_node(ctx, it2);
		}
	}
}

static node * handle_any_all(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *newres;
	node *partial_res;
	node *constant;
	const type_specifier *boolean = res->hdr.type;
	expression_operator op = res->expr.operation;

	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(constant = create_float_constant(ctx, op == EXPR_OP_FUN_ALL ? (float)GET_NODE_VEC_SIZE(a) : 0.0f, 1));
	if(GET_NODE_VEC_SIZE(a) > 2)
	{
		ESSL_CHECK(partial_res = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_HADD, a, NULL, NULL));
	} 
	else 
	{
		unsigned i;
		ESSL_CHECK(partial_res = _essl_new_binary_expression(ctx->pool, NULL, EXPR_OP_ADD, NULL));
		for(i = 0; i < 2; ++i)
		{
			node *tmp;
			ESSL_CHECK(tmp = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, a));
			_essl_ensure_compatible_node(tmp, a);
			tmp->hdr.type = boolean;
			tmp->expr.u.swizzle = _essl_create_scalar_swizzle(i);
			ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));
			SET_CHILD(partial_res, i, tmp);
		}
	}
	_essl_ensure_compatible_node(partial_res, res);
	partial_res->hdr.type = boolean;
	ESSL_CHECK(partial_res = mali200_preschedule_single_node(ctx, partial_res));
	
	ESSL_CHECK(newres = _essl_new_binary_expression(ctx->pool, partial_res, op == EXPR_OP_FUN_ALL ? EXPR_OP_EQ : EXPR_OP_NE, constant));
	_essl_ensure_compatible_node(newres, res);
	newres->hdr.type = boolean;
	return mali200_preschedule_single_node(ctx, newres);
}

static node * handle_sin_cos(mali200_preschedule_context * ctx, node *res)
{
	node *constant;
	node *tmp3;
	node *tmp;
	expression_operator op = (res->expr.operation == EXPR_OP_FUN_SIN) ? EXPR_OP_FUN_SIN_0_1 : EXPR_OP_FUN_COS_0_1;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(constant = create_float_constant(ctx, (float)(0.5/M_PI), 1));
	ESSL_CHECK(tmp3 = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(tmp3, a);
	ESSL_CHECK(tmp3 = mali200_preschedule_single_node(ctx, tmp3));
	ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, op, tmp3, NULL, NULL));
	_essl_ensure_compatible_node(tmp, res);
	return mali200_preschedule_single_node(ctx, tmp);
}

static node * handle_tan(mali200_preschedule_context * ctx, node *res)
{
	node *constant;
	node *tmp3;
	node *tmp2;
	node *tmp;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));

	ESSL_CHECK(constant = create_float_constant(ctx, (float)(0.5/M_PI), 1));
	ESSL_CHECK(tmp3 = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(tmp3, a);
	ESSL_CHECK(tmp3 = mali200_preschedule_single_node(ctx, tmp3));
	ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SIN_0_1, tmp3, 0, 0));
	_essl_ensure_compatible_node(tmp, res);
	ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));

	ESSL_CHECK(tmp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_COS_0_1, tmp3, 0, 0));
	_essl_ensure_compatible_node(tmp2, res);
	ESSL_CHECK(tmp2 = mali200_preschedule_single_node(ctx, tmp2));
	

	ESSL_CHECK(tmp3 = _essl_new_binary_expression(ctx->pool, tmp, EXPR_OP_DIV, tmp2));
	_essl_ensure_compatible_node(tmp3, res);
	return mali200_preschedule_single_node(ctx, tmp3);
}

static node * handle_asin(mali200_preschedule_context * ctx, node *res)
{
	node *x;
	node *mul, *one, *sub, *sqrt, *atan2;
	/* atan2(x, sqrt(1 - x**2)) */
	ESSL_CHECK(x = GET_CHILD(res, 0));
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, x, EXPR_OP_MUL, x));
	_essl_ensure_compatible_node(mul, res);
	ESSL_CHECK(mul = mali200_preschedule_single_node(ctx, mul));
	
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));
	
	ESSL_CHECK(sub = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_SUB, mul));
	_essl_ensure_compatible_node(sub, res);
	ESSL_CHECK(sub = mali200_preschedule_single_node(ctx, sub));
	
	ESSL_CHECK(sqrt = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SQRT, sub, 0, 0));
	_essl_ensure_compatible_node(sqrt, res);
	ESSL_CHECK(sqrt = mali200_preschedule_single_node(ctx, sqrt));
	
	ESSL_CHECK(atan2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ATAN, x, sqrt, 0));
	_essl_ensure_compatible_node(atan2, res);
	return mali200_preschedule_single_node(ctx, atan2);
}

static node * handle_acos(mali200_preschedule_context * ctx, node *res)
{
	node *x;
	node *mul, *one, *sub, *sqrt, *atan2;
	/* atan2(sqrt(1 - x**2), x) */
	ESSL_CHECK(x = GET_CHILD(res, 0));
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, x, EXPR_OP_MUL, x));
	_essl_ensure_compatible_node(mul, res);
	ESSL_CHECK(mul = mali200_preschedule_single_node(ctx, mul));
	
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));
	
	ESSL_CHECK(sub = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_SUB, mul));
	_essl_ensure_compatible_node(sub, res);
	ESSL_CHECK(sub = mali200_preschedule_single_node(ctx, sub));
	
	ESSL_CHECK(sqrt = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SQRT, sub, 0, 0));
	_essl_ensure_compatible_node(sqrt, res);
	ESSL_CHECK(sqrt = mali200_preschedule_single_node(ctx, sqrt));
	
	ESSL_CHECK(atan2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ATAN, sqrt, x, 0));
	_essl_ensure_compatible_node(atan2, res);
	return mali200_preschedule_single_node(ctx, atan2);
}

static node * handle_pow(mali200_preschedule_context * ctx, node *res)
{
	node *log2, *mul, *exp2;
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	ESSL_CHECK(log2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_LOG2, a, 0, 0));
	_essl_ensure_compatible_node(log2, res);
	ESSL_CHECK(log2 = mali200_preschedule_single_node(ctx, log2));

	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, log2, EXPR_OP_MUL, b));
	_essl_ensure_compatible_node(mul, res);
	ESSL_CHECK(mul = mali200_preschedule_single_node(ctx, mul));

	ESSL_CHECK(exp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_EXP2, mul, 0, 0));
	_essl_ensure_compatible_node(exp2, res);
	return mali200_preschedule_single_node(ctx, exp2);
}

static node * handle_sign(mali200_preschedule_context * ctx, node *res)
{
	node *constant;
	node *tmp;
	node *tmp2;
	node *tmp3;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(constant = create_float_constant(ctx, 0.0, 1));
	ESSL_CHECK(tmp = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_GT, constant));
	_essl_ensure_compatible_node(tmp, res);
	ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));
	ESSL_CHECK(tmp2 = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_LT, constant));
	_essl_ensure_compatible_node(tmp2, res);
	ESSL_CHECK(tmp2 = mali200_preschedule_single_node(ctx, tmp2));
	
	ESSL_CHECK(tmp3 = _essl_new_binary_expression(ctx->pool, tmp, EXPR_OP_SUB, tmp2));
	_essl_ensure_compatible_node(tmp3, res);
	return mali200_preschedule_single_node(ctx, tmp3);
}

static node * handle_mod(mali200_preschedule_context * ctx, node *res)
{
	node *div, *fract, *mul;
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	ESSL_CHECK(div = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_DIV, b));
	_essl_ensure_compatible_node(div, res);
	ESSL_CHECK(div = mali200_preschedule_single_node(ctx, div));
	ESSL_CHECK(fract = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_FRACT, div, 0, 0));
	_essl_ensure_compatible_node(fract, res);
	ESSL_CHECK(fract = mali200_preschedule_single_node(ctx, fract));
	
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, b, EXPR_OP_MUL, fract));
	_essl_ensure_compatible_node(mul, res);
	return mali200_preschedule_single_node(ctx, mul);
}

static node * handle_clamp(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *lolim;
	node *hilim;

	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(lolim = GET_CHILD(res, 1));
	ESSL_CHECK(hilim = GET_CHILD(res, 2));
	if(!_essl_is_node_all_value(ctx->desc, lolim, 0.0) 
		|| !_essl_is_node_all_value(ctx->desc, hilim, 1.0) 
		|| GET_NODE_VEC_SIZE(res) != GET_NODE_VEC_SIZE(a))
	{
		node *max, *min;
		ESSL_CHECK(max = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, a, lolim, 0));
		_essl_ensure_compatible_node(max, res);
		ESSL_CHECK(max = mali200_preschedule_single_node(ctx, max));

		ESSL_CHECK(min = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MIN, max, hilim, 0));
		_essl_ensure_compatible_node(min, res);
		return mali200_preschedule_single_node(ctx, min);
	}
	else
	{
		return res;
	}
}

static node * handle_max(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	if(_essl_is_node_all_value(ctx->desc, a, 0.0) 
		&& !_essl_is_node_all_value(ctx->desc, b, 0.0))
	{
		node *newres;
		ESSL_CHECK(newres = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, b, a, NULL));
		_essl_ensure_compatible_node(newres, res);
		return mali200_preschedule_single_node(ctx, newres);
	}
	else
	{
		return res;
	}

}

static node * handle_dot(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	node *tmp, *tmp2;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	ESSL_CHECK(tmp = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, b));
	_essl_ensure_compatible_node(tmp, a);
	ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));
	
	switch(GET_NODE_VEC_SIZE(a))
	{
		case 1:
			return tmp;
		case 2:
		{
			node *x, *y;
			ESSL_CHECK(x = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, tmp));
			x->expr.u.swizzle = _essl_create_scalar_swizzle(0);
			_essl_ensure_compatible_node(x, res);
			ESSL_CHECK(x = mali200_preschedule_single_node(ctx, x));

			ESSL_CHECK(y = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, tmp));
			y->expr.u.swizzle = _essl_create_scalar_swizzle(1);
			_essl_ensure_compatible_node(y, res);
			ESSL_CHECK(y = mali200_preschedule_single_node(ctx, y));
		
			ESSL_CHECK(tmp2 = _essl_new_binary_expression(ctx->pool, x, EXPR_OP_ADD, y));
			_essl_ensure_compatible_node(tmp2, res);
			return mali200_preschedule_single_node(ctx, tmp2);
		}
		default:
			ESSL_CHECK(tmp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_HADD, tmp, NULL, NULL));
			_essl_ensure_compatible_node(tmp2, res);
			return mali200_preschedule_single_node(ctx, tmp2);
	}
}

static node * handle_length(mali200_preschedule_context * ctx, node *res)
{
	node *tmp;
	node *tmp2;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	if(GET_NODE_VEC_SIZE(a) == 3)
	{
		/* do a normalize, then select only the w component. 
		 * NORM3 on mali200 yields a vec4 with length($a) in the w component, 
		 * although the last part is configurable */
		ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_NORMALIZE, a, 0, 0));
		_essl_ensure_compatible_node(tmp, res);
		ESSL_CHECK(tmp->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 4));
		ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));
		
		ESSL_CHECK(tmp2 = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, tmp));
		tmp2->expr.u.swizzle = _essl_create_scalar_swizzle(3);
		_essl_ensure_compatible_node(tmp2, res);
		return mali200_preschedule_single_node(ctx, tmp2);
	}
	else
	{
		ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, a, a, 0));
		_essl_ensure_compatible_node(tmp, res);
		ESSL_CHECK(tmp->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 1));
		ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));
		ESSL_CHECK(tmp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SQRT, tmp, 0, 0));
		_essl_ensure_compatible_node(tmp2, tmp);
		return mali200_preschedule_single_node(ctx, tmp2);
	}
}

static node * handle_normalize(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	if( GET_NODE_VEC_SIZE(a) != 3)
	{
		/* the other cases has to be done with dot + rsqrt + mul */
		node *tmp;
		node *tmp2;
		node *tmp3;
		ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, a, a, 0));
		_essl_ensure_compatible_node(tmp, res);

		ESSL_CHECK(tmp->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 1));
		ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));
		ESSL_CHECK(tmp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_INVERSESQRT, tmp, 0, 0));
		_essl_ensure_compatible_node(tmp2, tmp);
		ESSL_CHECK(tmp2 = mali200_preschedule_single_node(ctx, tmp2));
		ESSL_CHECK(tmp3 = _essl_new_binary_expression(ctx->pool, tmp2, EXPR_OP_MUL, a));
		_essl_ensure_compatible_node(tmp3, res);

		return mali200_preschedule_single_node(ctx, tmp3);
	}
	else
	{
		/* mali 200 has hardware support for normalize 3, use this */
		return res;
	}
}

static node * handle_derivatives(mali200_preschedule_context * ctx, node *res)
{
	node *scaled, *derivative;
	expression_operator operation = res->expr.operation == EXPR_OP_FUN_DFDX
	? EXPR_OP_FUN_M200_DERX
	: EXPR_OP_FUN_M200_DERY;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));

	if (ctx->desc->options->mali200_derivative_scale)
	{
		string dscale_name = {"gl_mali_DerivativeScale", 23};
		symbol *dscale_symbol;
		node *dscale_variable;
		node *scale_load, *scale;
		int comp = res->expr.operation == EXPR_OP_FUN_DFDX ? 0 : 1;
		
		dscale_symbol = _essl_symbol_table_lookup(ctx->tu->root->stmt.child_scope, dscale_name);
		ESSL_CHECK(dscale_variable = _essl_new_variable_reference_expression(ctx->pool, dscale_symbol));
		dscale_variable->hdr.type = dscale_symbol->type;
		ESSL_CHECK(dscale_variable = mali200_preschedule_single_node(ctx, dscale_variable));
		ESSL_CHECK(scale_load = _essl_new_load_expression(ctx->pool, ADDRESS_SPACE_UNIFORM, dscale_variable));
		_essl_ensure_compatible_node(scale_load, dscale_variable);
		ESSL_CHECK(scale_load = mali200_preschedule_single_node(ctx, scale_load));
		
		ESSL_CHECK(scale = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, scale_load));
		scale->expr.u.swizzle.indices[0] = comp;
		scale->expr.u.swizzle.indices[1] = comp;
		scale->expr.u.swizzle.indices[2] = comp;
		scale->expr.u.swizzle.indices[3] = comp;
		
		_essl_ensure_compatible_node(scale, a);
		ESSL_CHECK(scale = mali200_preschedule_single_node(ctx, scale));
		
		ESSL_CHECK(scaled = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, scale));
		_essl_ensure_compatible_node(scaled, a);
		ESSL_CHECK(scaled = mali200_preschedule_single_node(ctx, scaled));
	}
	else
	{
		scaled = a;
	}

	ESSL_CHECK(derivative = _essl_new_builtin_function_call_expression(ctx->pool, operation, scaled, NULL, NULL));
	_essl_ensure_compatible_node(derivative, res);
	SET_CHILD(res, 0, scaled);
	
	return mali200_preschedule_single_node(ctx, derivative);
}

static node * handle_fwidth(mali200_preschedule_context * ctx, node *res)
{
	node *a;
	node *dx, *dy;
	node *add;
	node *scaled;
	node *tmp;
	node *tmp2;

	ESSL_CHECK(a = GET_CHILD(res, 0));
	if (ctx->desc->options->mali200_derivative_scale)
	{
		string dscale_name = {"gl_mali_DerivativeScale", 23};
		symbol *dscale_symbol;
		node *dscale_variable;
		node *scale_load, *scale;
	
		dscale_symbol = _essl_symbol_table_lookup(ctx->tu->root->stmt.child_scope, dscale_name);
		ESSL_CHECK(dscale_variable = _essl_new_variable_reference_expression(ctx->pool, dscale_symbol));
		dscale_variable->hdr.type = dscale_symbol->type;
		ESSL_CHECK(dscale_variable = mali200_preschedule_single_node(ctx, dscale_variable));
		ESSL_CHECK(scale_load = _essl_new_load_expression(ctx->pool, ADDRESS_SPACE_UNIFORM, dscale_variable));
		_essl_ensure_compatible_node(scale_load, dscale_variable);
		ESSL_CHECK(scale_load = mali200_preschedule_single_node(ctx, scale_load));
	
		ESSL_CHECK(scale = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, scale_load));
		scale->expr.u.swizzle.indices[0] = 0;
		scale->expr.u.swizzle.indices[1] = 0;
		scale->expr.u.swizzle.indices[2] = 0;
		scale->expr.u.swizzle.indices[3] = 0;
	
		_essl_ensure_compatible_node(scale, a);
		ESSL_CHECK(scale = mali200_preschedule_single_node(ctx, scale));
	
		ESSL_CHECK(scaled = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, scale));
		_essl_ensure_compatible_node(scaled, a);
		ESSL_CHECK(scaled = mali200_preschedule_single_node(ctx, scaled));
	} 
	else 
	{
		scaled = a;
	}	

	ESSL_CHECK(dx = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_DERX, scaled, 0, 0));
	_essl_ensure_compatible_node(dx, res);
	
	ESSL_CHECK(dy = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_DERY, scaled, 0, 0));
	_essl_ensure_compatible_node(dy, res);
	
	dx->hdr.is_control_dependent = 1;
	dy->hdr.is_control_dependent = 1;
	ESSL_CHECK(_essl_clone_control_dependent_op(res, dx, ctx->cfg, ctx->pool));
	ESSL_CHECK(_essl_clone_control_dependent_op(res, dy, ctx->cfg, ctx->pool));
	
	ESSL_CHECK(dx = mali200_preschedule_single_node(ctx, dx));
	
	ESSL_CHECK(dy = mali200_preschedule_single_node(ctx, dy));
	
	/* since dx and dy are control dependent, 
	 * we need to explicitly insert them into the visit dictionary, 
	 * as they also can be reached from the control dependent ops list. */
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, dx, dx));
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, dy, dy));
	
	ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ABS, dx, 0, 0));
	_essl_ensure_compatible_node(tmp, res);
	ESSL_CHECK(tmp = mali200_preschedule_single_node(ctx, tmp));
	
	ESSL_CHECK(tmp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ABS, dy, 0, 0));
	_essl_ensure_compatible_node(tmp2, res);
	ESSL_CHECK(tmp2 = mali200_preschedule_single_node(ctx, tmp2));
	
	ESSL_CHECK(add = _essl_new_binary_expression(ctx->pool, tmp, EXPR_OP_ADD, tmp2));
	_essl_ensure_compatible_node(add, res);
	/* the traversal of control dependent ops will detect that add isn't control dependent and remove $res from consideration */
	return mali200_preschedule_single_node(ctx, add);
}

static node * handle_simple_texture(mali200_preschedule_context * ctx, node *res)
{
	expression_operator op = res->expr.operation;
	node *sampler;
	node *coord;
	node_extra *ne;
	ESSL_CHECK(sampler = GET_CHILD(res, 0));

	ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, res));
	ne->address_multiplier = 1;

	sampler = find_sampler_for_texture_node(ne, sampler, NULL);

	ESSL_CHECK(coord = GET_CHILD(res, 1));

	/* look for 2d projective texturing that expects to divide by w instead of z.
	   because we delete identity swizzles afterwards, we can't examine the type in the scheduler to determine what operand is needed
	 */
	assert(op != EXPR_OP_FUN_TEXTURE2DPROJ || _essl_get_type_size(coord->hdr.type) == 3);
	assert(op != EXPR_OP_FUN_TEXTURE2DPROJLOD || _essl_get_type_size(coord->hdr.type) == 3);
	assert(op != EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT || _essl_get_type_size(coord->hdr.type) == 3);
	assert(op != EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT || _essl_get_type_size(coord->hdr.type) == 4);

	ESSL_CHECK(coord = find_coord_for_texture_node(coord));

	switch(op)
	{
		case EXPR_OP_FUN_TEXTURE2DLOD_EXT:
			op = EXPR_OP_FUN_TEXTURE2DLOD;
			break;
		case EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT:
			op = EXPR_OP_FUN_TEXTURE2DPROJLOD;
			break;
		case EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT:
			op = EXPR_OP_FUN_TEXTURE2DPROJLOD_W;
			break;
		case EXPR_OP_FUN_TEXTURECUBELOD_EXT:
			op = EXPR_OP_FUN_TEXTURECUBELOD;
			break;
		default:
			break;
	}
	res->expr.operation = op;
	SET_CHILD(res, 0, sampler);
	SET_CHILD(res, 1, coord);
	/* these are control dependent, return the original node */
	return res;
}

static node * handle_2dgrad(mali200_preschedule_context * ctx, node *res)
{
	expression_operator op = res->expr.operation;
	node *sampler;
	node *coord;
	node_extra *ne;
	node *lod;
	node *texture_wh;
	node *dx;
	node *dy;
	ESSL_CHECK(sampler = GET_CHILD(res, 0));
	ESSL_CHECK(coord = GET_CHILD(res, 1));
	ESSL_CHECK(dx = GET_CHILD(res, 2));
	ESSL_CHECK(dy = GET_CHILD(res, 3));
	assert(GET_CHILD(res, 0) != NULL);

	assert(sampler->hdr.kind == EXPR_KIND_UNARY && sampler->expr.operation == EXPR_OP_SWIZZLE);

	ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, res));
	ne->address_multiplier = 1;

	sampler = GET_CHILD(sampler, 0);
	
	
	assert(op != EXPR_OP_FUN_TEXTURE2DGRAD_EXT || _essl_get_type_size(coord->hdr.type) == 2);

	/* look for 2d projective texturing that expects to divide by w instead of z.
	because we delete identity swizzles afterwards, we can't examine the type in the scheduler to determine what operand is needed
	*/
	assert(op != EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT || _essl_get_type_size(coord->hdr.type) == 3);
	assert(op != EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT || _essl_get_type_size(coord->hdr.type) == 4);


	ESSL_CHECK(coord = find_coord_for_texture_node(coord));

	/* make a load for texture width and height (vec2(w,h) from a magic uniform) */
	ESSL_CHECK(texture_wh = read_texture_width_and_height(ctx, sampler));

	/* calulate lod */
	ESSL_CHECK(lod = calc_lod_from_derivatives_and_texture_params(ctx, texture_wh, dx, dy));

	switch(op)
	{
		case EXPR_OP_FUN_TEXTURE2DGRAD_EXT:
			op = EXPR_OP_FUN_TEXTURE2DLOD;
			break;

		case EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT:
			op = EXPR_OP_FUN_TEXTURE2DPROJLOD;
			break;

		case EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT:
			op = EXPR_OP_FUN_TEXTURE2DPROJLOD_W;
			break;

		default:
			assert(0 && "unsupported opcode");
			break;
	}

	res->expr.operation = op;
	SET_CHILD(res, 0, sampler);
	SET_CHILD(res, 1, coord);
	SET_CHILD(res, 2, lod);
	SET_CHILD(res, 3, NULL);
	res->hdr.n_children = 3;
	/* these are control dependent, return the original node */
	return res;
}

static node * handle_cubegrad(mali200_preschedule_context * ctx, node *res)
{
	node *sampler;
	node *coord;
	node *dx;
	node *dy;
	node_extra *ne;
	node *lod;
	node *texture_wh;
	node *tx;
	node *ty;
	node *face;
	node *input;
	symbol *enc_sym;
	node *enc_const;
	node *encoding_vect_addr;
	node *encoding_vect;
	node *divisor_index;
	node *dividend_indices;
	node *dividend_signs;
	node *dividend_indices_abs;
	node *cnst;
	node *real_dividend_indices;
	node *tx_projected;
	node *ty_projected;
	node *t_projected;
	node *dx_projected;
	node *dy_projected;
	node *real_divisor_index;
	scope *global_scope;

	assert(GET_CHILD(res, 0) != NULL);
	ESSL_CHECK(sampler = GET_CHILD(res, 0));
	ESSL_CHECK(coord = GET_CHILD(res, 1));
	ESSL_CHECK(dx = GET_CHILD(res, 2));
	ESSL_CHECK(dy = GET_CHILD(res, 3));

	assert(sampler->hdr.kind == EXPR_KIND_UNARY && sampler->expr.operation == EXPR_OP_SWIZZLE);

	global_scope = ctx->tu->root->stmt.child_scope;

	ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, res));
	ne->address_multiplier = 1;

	sampler = GET_CHILD(sampler, 0);
	
	assert(res->expr.operation == EXPR_OP_FUN_TEXTURECUBEGRAD_EXT && _essl_get_type_size(coord->hdr.type) == 3);

	ESSL_CHECK(coord = find_coord_for_texture_node(coord));

	/**
	 * 1.Given the three input vectors, T, dX and dY
	 * (where T is taken as the pre-cubemap-transform texture coordinate, and dX and dY are derivative vectors;
	 * all of them are 3-component vectors), first compute the two vectors
	 * TX = T + dX, TY = T + dY
	 *
	 * 2.Perform Cube-Map transform on T.
	 *
	 * 3.From the Cube-Map descriptor, find out which face of the cube-map we will actually be sampling in.
	 *
	 * 4.Within the shader, project TX and TY onto the selected cube-map face and compute
	 * the difference between them and the actual projected cube-map coordinate. Some examples:
	 *
	 * a.The face is +X:
	 * vec2 TX_projected = 0.5 * vec2( -TX.z, -TX.y ) / abs( TX.x );
	 * vec2 TY_projected = 0.5 * vec2( -TY.z, -TY.y ) / abs( TY.x );
	 * vec2 T_projected = 0.5 * vec2( -T.z, -T.y ) / abs( T.x );
	 *
	 * vec2 dX_projected = TX_projected - T_projected;
	 * vec2 dY_projected = TY_projected - T_projected;
	 *
	 * b.The face is -Y:
	 * vec2 TX_projected = 0.5 * vec2( TX.x, -TX.z ) / abs( TX.y );
	 * vec2 TY_projected = 0.5 * vec2( TY.x, -TY.z ) / abs( TY.y );
	 * vec2 T_projected = 0.5 * vec2( T.x, -T.z ) / abs( T.y );
	 *
	 * vec2 dX_projected = TX_projected - T_projected;
	 * vec2 dY_projected = TY_projected - T_projected;
	 *
	 * The vectors dX_projected and dY_projected (both of which are 2D vectors at this point)
	 * can then be fed into the "2D calculate lod" sequence.
	 */

	/*1.*/
	ESSL_CHECK(tx = _essl_new_binary_expression(ctx->pool, coord, EXPR_OP_ADD, dx));
	_essl_ensure_compatible_node(tx, coord);
	ESSL_CHECK(tx = mali200_preschedule_single_node(ctx, tx));

	ESSL_CHECK(ty = _essl_new_binary_expression(ctx->pool, coord, EXPR_OP_ADD, dy));
	_essl_ensure_compatible_node(ty, coord);
	ESSL_CHECK(ty = mali200_preschedule_single_node(ctx, ty));

	/*2.*/
	ESSL_CHECK(input = create_cubemap_transform(ctx, coord));

	/*3. face is stored in .w component the cube-map result */
	ESSL_CHECK(face = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, input));
	_essl_ensure_compatible_node(face, input);
	ESSL_CHECK(face->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, input->hdr.type, 1));
	face->expr.u.swizzle = _essl_create_scalar_swizzle(3);
	ESSL_CHECK(face = mali200_preschedule_single_node(ctx, face));

	/*4.*/
	/*4.1. get the address of the encoding constant (created during entry point insertion) */
	ESSL_CHECK(enc_sym = _essl_symbol_table_lookup(global_scope, _essl_cstring_to_string_nocopy(TEXTURE_CUBEGRADEXT_ENC_CONST)));

	ESSL_CHECK(enc_const = _essl_new_variable_reference_expression(ctx->pool, enc_sym));
	enc_const->hdr.type = enc_sym->type;
	ESSL_CHECK(enc_const = mali200_preschedule_single_node(ctx, enc_const));

	/*4.2 take proper vector from the encoding constant*/
	ESSL_CHECK(encoding_vect_addr = _essl_new_binary_expression(ctx->pool, enc_const, EXPR_OP_INDEX, face));
	_essl_ensure_compatible_node(encoding_vect_addr, enc_const);
	ESSL_CHECK(encoding_vect_addr->hdr.type = enc_const->hdr.type->child_type);
	ESSL_CHECK(encoding_vect_addr = mali200_preschedule_single_node(ctx, encoding_vect_addr));

	ESSL_CHECK(encoding_vect = _essl_new_load_expression(ctx->pool, ADDRESS_SPACE_UNIFORM, encoding_vect_addr));
	_essl_ensure_compatible_node(encoding_vect, encoding_vect_addr);
	ESSL_CHECK(encoding_vect = mali200_preschedule_single_node(ctx, encoding_vect));

	/*4.3 divisor_index is stored in .z component*/
	ESSL_CHECK(divisor_index = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, encoding_vect));
	_essl_ensure_compatible_node(divisor_index, encoding_vect);
	ESSL_CHECK(divisor_index->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, encoding_vect->hdr.type, 1));
	divisor_index->expr.u.swizzle = _essl_create_scalar_swizzle(2);
	ESSL_CHECK(divisor_index = mali200_preschedule_single_node(ctx, divisor_index));

	/* divisor_index is incremented by 1, so we will subtract 1. */
	ESSL_CHECK(cnst = create_float_constant(ctx, -1.0, 1));
	ESSL_CHECK(real_divisor_index = _essl_new_binary_expression(ctx->pool, divisor_index, EXPR_OP_ADD, cnst));
	_essl_ensure_compatible_node(real_divisor_index, divisor_index);
	ESSL_CHECK(real_divisor_index = mali200_preschedule_single_node(ctx, real_divisor_index));

	/*4.4 take dividend indices signed. .xy components */
	ESSL_CHECK(dividend_indices = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, encoding_vect));
	_essl_ensure_compatible_node(dividend_indices, encoding_vect);
	ESSL_CHECK(dividend_indices->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, encoding_vect->hdr.type, 2));
	dividend_indices->expr.u.swizzle = _essl_create_identity_swizzle(2);
	ESSL_CHECK(dividend_indices = mali200_preschedule_single_node(ctx, dividend_indices));

	/*4.5 take dividend signs. */
	ESSL_CHECK(dividend_signs = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SIGN, dividend_indices, NULL, NULL));
	_essl_ensure_compatible_node(dividend_signs, dividend_indices);
	ESSL_CHECK(dividend_signs = mali200_preschedule_single_node(ctx, dividend_signs));

	/*4.6 take dividend indices abs. */
	ESSL_CHECK(dividend_indices_abs = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ABS, dividend_indices, NULL, NULL));
	_essl_ensure_compatible_node(dividend_indices_abs, dividend_indices);
	ESSL_CHECK(dividend_indices_abs = mali200_preschedule_single_node(ctx, dividend_indices_abs));

	/*4.7 dividend indices are incremented by 1, so we will subtract 1. */
	ESSL_CHECK(cnst = create_float_constant(ctx, -1.0, 2));
	ESSL_CHECK(real_dividend_indices = _essl_new_binary_expression(ctx->pool, dividend_indices_abs, EXPR_OP_ADD, cnst));
	_essl_ensure_compatible_node(real_dividend_indices, dividend_indices_abs);
	ESSL_CHECK(real_dividend_indices = mali200_preschedule_single_node(ctx, real_dividend_indices));


	/*4.8 TX_projected */
	ESSL_CHECK(tx_projected = calculate_projection(ctx, tx, real_dividend_indices, dividend_signs, real_divisor_index));

	/*4.9 TY_projected */
	ESSL_CHECK(ty_projected = calculate_projection(ctx, ty, real_dividend_indices, dividend_signs, real_divisor_index));

	/*4.10 T_projected */
	ESSL_CHECK(t_projected = calculate_projection(ctx, coord, real_dividend_indices, dividend_signs, real_divisor_index));

	/*4.11 dX_projected*/
	ESSL_CHECK(dx_projected = _essl_new_binary_expression(ctx->pool, t_projected, EXPR_OP_SUB, tx_projected));
	_essl_ensure_compatible_node(dx_projected, t_projected);
	ESSL_CHECK(dx_projected = mali200_preschedule_single_node(ctx, dx_projected));

	/*4.12 dY_projected*/
	ESSL_CHECK(dy_projected = _essl_new_binary_expression(ctx->pool, t_projected, EXPR_OP_SUB, ty_projected));
	_essl_ensure_compatible_node(dy_projected, t_projected);
	ESSL_CHECK(dy_projected = mali200_preschedule_single_node(ctx, dy_projected));

	/* make a load for texture width and height (vec2(w,h) from a magic uniform) */
	ESSL_CHECK(texture_wh = read_texture_width_and_height(ctx, sampler));

	/* calulate lod */
	ESSL_CHECK(lod = calc_lod_from_derivatives_and_texture_params(ctx, texture_wh, dx_projected, dy_projected));

	res->expr.operation = EXPR_OP_FUN_TEXTURECUBELOD;
	SET_CHILD(res, 0, sampler);
	SET_CHILD(res, 1, coord);
	SET_CHILD(res, 2, lod);
	SET_CHILD(res, 3, NULL);
	res->hdr.n_children = 3;
	/* these are control dependent, return the original node */
	return res;
}

static node * handle_texture(mali200_preschedule_context * ctx, node *res)
{
	expression_operator op;
	if(GET_CHILD(res, 0) == NULL)
	{
		return res;
	}
	op = res->expr.operation;
	if(op == EXPR_OP_FUN_TEXTURE1D
		|| op == EXPR_OP_FUN_TEXTURE1DPROJ
		|| op == EXPR_OP_FUN_TEXTURE1DLOD
		|| op == EXPR_OP_FUN_TEXTURE1DPROJLOD
		|| op == EXPR_OP_FUN_TEXTURE2D
		|| op == EXPR_OP_FUN_TEXTURE2DPROJ
		|| op == EXPR_OP_FUN_TEXTURE2DPROJ_W
		|| op == EXPR_OP_FUN_TEXTURE2DLOD
		|| op == EXPR_OP_FUN_TEXTURE2DPROJLOD
		|| op == EXPR_OP_FUN_TEXTURE2DPROJLOD_W
		|| op == EXPR_OP_FUN_TEXTURE3D
		|| op == EXPR_OP_FUN_TEXTURE3DPROJ
		|| op == EXPR_OP_FUN_TEXTURE3DLOD
		|| op == EXPR_OP_FUN_TEXTURE3DPROJLOD
		|| op == EXPR_OP_FUN_TEXTURECUBE
		|| op == EXPR_OP_FUN_TEXTURECUBELOD
		|| op == EXPR_OP_FUN_TEXTURE2DLOD_EXT
		|| op == EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT
		|| op == EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT
		|| op == EXPR_OP_FUN_TEXTURECUBELOD_EXT)
	{
		return handle_simple_texture(ctx, res);
	}
	else if(op == EXPR_OP_FUN_TEXTURE2DGRAD_EXT
			|| op == EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT
			|| op == EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT)
	{
		return handle_2dgrad(ctx, res);
	}
	else if(op == EXPR_OP_FUN_TEXTURECUBEGRAD_EXT)
	{
		return handle_cubegrad(ctx, res);
	}
	else
	{
		return res;
	}
}

static node * handle_memory_op(mali200_preschedule_context * ctx, node *res)
{
	node_extra *ne;
	node *nres = NULL;
	node *addr = NULL;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, res));
	ne->address_multiplier = (int)ctx->desc->get_address_multiplier(ctx->desc, res->hdr.type, res->expr.u.load_store.address_space);
	ne->is_indexed = 0;
	ESSL_CHECK(process_address(ctx, res, ne, a, &addr));
	SET_CHILD(res, 0, addr);

	if(res->hdr.kind == EXPR_KIND_LOAD && res->expr.u.load_store.address_space == ADDRESS_SPACE_FRAGMENT_SPECIAL)
	{
		string fragcoord_name = {"gl_FragCoord", 12};
		string pointcoord_name = {"gl_PointCoord", 13};
		string frontfacing_name = {"gl_FrontFacing", 14};
		string fbcolor_name = {"gl_FBColor", 10};
		string fbdepth_name = {"gl_FBDepth", 10};
		string fbstencil_name = {"gl_FBStencil", 12};
		string name;
		name = ne->address_symbols->sym->name;
		ne->address_symbols = ne->address_symbols->next;
		assert(addr == NULL);
		if(!_essl_string_cmp(name, fragcoord_name))
		{
			node *pos;
			node *scaled;
			node *swz_xyz;
			node *swz_w;
			node *inverse;
			node *combine_swz_xyz;
			node *combine_swz_w;

			ESSL_CHECK(pos = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_POS, NULL, NULL, NULL));
			_essl_ensure_compatible_node(pos, res);
			ESSL_CHECK(pos = mali200_preschedule_single_node(ctx, pos));

			ESSL_CHECK(swz_xyz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, pos));
			swz_xyz->expr.u.swizzle = _essl_create_identity_swizzle(3);
			_essl_ensure_compatible_node(swz_xyz, res);
			ESSL_CHECK(swz_xyz->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 3));
			ESSL_CHECK(swz_xyz = mali200_preschedule_single_node(ctx, swz_xyz));

			if (ctx->desc->options->mali200_fragcoord_scale)
			{
				string fcscale_name = {"gl_mali_FragCoordScale", 22};
				symbol *fcscale_symbol;
				node *fcscale_variable;
				node *fc_scale;

				fcscale_symbol = _essl_symbol_table_lookup(ctx->tu->root->stmt.child_scope, fcscale_name);
				ESSL_CHECK(fcscale_variable = _essl_new_variable_reference_expression(ctx->pool, fcscale_symbol));
				fcscale_variable->hdr.type = fcscale_symbol->type;
				ESSL_CHECK(fcscale_variable = mali200_preschedule_single_node(ctx, fcscale_variable));
				ESSL_CHECK(fc_scale = _essl_new_load_expression(ctx->pool, ADDRESS_SPACE_UNIFORM, fcscale_variable));
				_essl_ensure_compatible_node(fc_scale, fcscale_variable);
				ESSL_CHECK(fc_scale = mali200_preschedule_single_node(ctx, fc_scale));

				ESSL_CHECK(scaled = _essl_new_binary_expression(ctx->pool, swz_xyz, EXPR_OP_MUL, fc_scale));
				_essl_ensure_compatible_node(scaled, swz_xyz);
				ESSL_CHECK(scaled = mali200_preschedule_single_node(ctx, scaled));
			}
			else
			{
				scaled = swz_xyz;
			}

			ESSL_CHECK(swz_w = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, pos));
			swz_w->expr.u.swizzle = _essl_create_scalar_swizzle(3);
			_essl_ensure_compatible_node(swz_w, res);
			ESSL_CHECK(swz_w->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 1));
			ESSL_CHECK(swz_w = mali200_preschedule_single_node(ctx, swz_w));
				
			ESSL_CHECK(inverse = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_RCP, swz_w, NULL, NULL));
			_essl_ensure_compatible_node(inverse, swz_w);
			ESSL_CHECK(inverse = mali200_preschedule_single_node(ctx, inverse));
					
			ESSL_CHECK(combine_swz_xyz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, scaled));
			combine_swz_xyz->expr.u.swizzle.indices[0] = 0;
			combine_swz_xyz->expr.u.swizzle.indices[1] = 1;
			combine_swz_xyz->expr.u.swizzle.indices[2] = 2;
			combine_swz_xyz->expr.u.swizzle.indices[3] = -1;

			_essl_ensure_compatible_node(combine_swz_xyz, res);
			ESSL_CHECK(combine_swz_xyz = mali200_preschedule_single_node(ctx, combine_swz_xyz));

			ESSL_CHECK(combine_swz_w = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, inverse));
			combine_swz_w->expr.u.swizzle.indices[0] = -1;
			combine_swz_w->expr.u.swizzle.indices[1] = -1;
			combine_swz_w->expr.u.swizzle.indices[2] = -1;
			combine_swz_w->expr.u.swizzle.indices[3] = 0;

			_essl_ensure_compatible_node(combine_swz_w, res);
			ESSL_CHECK(combine_swz_w = mali200_preschedule_single_node(ctx, combine_swz_w));

			ESSL_CHECK(nres = _essl_new_vector_combine_expression(ctx->pool, 2));
			SET_CHILD(nres, 0, combine_swz_xyz);
			SET_CHILD(nres, 1, combine_swz_w);
			_essl_ensure_compatible_node(nres, res);
			nres->expr.u.combiner.mask[0] = 0;
			nres->expr.u.combiner.mask[1] = 0;
			nres->expr.u.combiner.mask[2] = 0;
			nres->expr.u.combiner.mask[3] = 1;
			ESSL_CHECK(nres = mali200_preschedule_single_node(ctx, nres));
			return nres;		
		} 
		else if(!_essl_string_cmp(name, pointcoord_name))
		{
			node *original_pc;

			ESSL_CHECK(original_pc = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_POINT, NULL, NULL, NULL));
			_essl_ensure_compatible_node(original_pc, res);
			ESSL_CHECK(original_pc = mali200_preschedule_single_node(ctx, original_pc));

			if (ctx->desc->options->mali200_pointcoord_scalebias)
			{
				/* Flip point coordinate to compensate for missing flipping support in Mali200 */
				string pcscalebias_name = {"gl_mali_PointCoordScaleBias", 27};
				symbol *pcscalebias_symbol;
				node *pcscalebias_variable;
				node *coordinate;
				node *pc_scalebias, *pc_scale, *pc_bias;
				node *scaled_pc;

				pcscalebias_symbol = _essl_symbol_table_lookup(ctx->tu->root->stmt.child_scope, pcscalebias_name);

				ESSL_CHECK(pcscalebias_variable = _essl_new_variable_reference_expression(ctx->pool, pcscalebias_symbol));
				pcscalebias_variable->hdr.type = pcscalebias_symbol->type;
				ESSL_CHECK(pcscalebias_variable = mali200_preschedule_single_node(ctx, pcscalebias_variable));
				ESSL_CHECK(pc_scalebias = _essl_new_load_expression(ctx->pool, ADDRESS_SPACE_UNIFORM, pcscalebias_variable));
				_essl_ensure_compatible_node(pc_scalebias, pcscalebias_variable);
				ESSL_CHECK(pc_scalebias = mali200_preschedule_single_node(ctx, pc_scalebias));

				ESSL_CHECK(pc_scale = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, pc_scalebias));
				pc_scale->expr.u.swizzle.indices[0] = 0;
				pc_scale->expr.u.swizzle.indices[1] = 1;
				_essl_ensure_compatible_node(pc_scale, original_pc);
				ESSL_CHECK(pc_scale = mali200_preschedule_single_node(ctx, pc_scale));
				ESSL_CHECK(pc_bias = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, pc_scalebias));
				pc_bias->expr.u.swizzle.indices[0] = 2;
				pc_bias->expr.u.swizzle.indices[1] = 3;
				_essl_ensure_compatible_node(pc_bias, original_pc);
				ESSL_CHECK(pc_bias = mali200_preschedule_single_node(ctx, pc_bias));
				ESSL_CHECK(scaled_pc = _essl_new_binary_expression(ctx->pool, original_pc, EXPR_OP_MUL, pc_scale));
				_essl_ensure_compatible_node(scaled_pc, original_pc);
				ESSL_CHECK(scaled_pc = mali200_preschedule_single_node(ctx, scaled_pc));
				ESSL_CHECK(coordinate = _essl_new_binary_expression(ctx->pool, scaled_pc, EXPR_OP_ADD, pc_bias));
				_essl_ensure_compatible_node(coordinate, original_pc);
				return mali200_preschedule_single_node(ctx, coordinate);
			}
			else
			{
				return original_pc;
			}

		} 
		else if(!_essl_string_cmp(name, frontfacing_name))
		{
			node *load;
			type_specifier *t;
			ESSL_CHECK(load = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_MISC_VAL, NULL, NULL, NULL));
			_essl_ensure_compatible_node(load, res);
			ESSL_CHECK(load = mali200_preschedule_single_node(ctx, load));
			ESSL_CHECK(t = _essl_clone_type(ctx->pool, res->hdr.type));
			t->basic_type = TYPE_FLOAT;
			t->u.basic.vec_size = 4;
			load->hdr.type = t;
			ESSL_CHECK(nres = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, load));
			nres->expr.u.swizzle = _essl_create_scalar_swizzle(0);
			_essl_ensure_compatible_node(nres, res);
			return nres;
		} 
		else if(!_essl_string_cmp(name, fbcolor_name))
		{
			node *load;
			ESSL_CHECK(load = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_LD_RGB, NULL, NULL, NULL));
			_essl_ensure_compatible_node(load, res);
			ESSL_CHECK(load = mali200_preschedule_single_node(ctx, load));
			return load;
		} 
		else if(!_essl_string_cmp(name, fbdepth_name))
		{
			node *load;
			ESSL_CHECK(load = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_LD_ZS, NULL, NULL, NULL));
			_essl_ensure_compatible_node(load, res);
			ESSL_CHECK(load = mali200_preschedule_single_node(ctx, load));
			ESSL_CHECK(load->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 4)); /* vec4 as LD_ZS clobbers an entire register */
			ESSL_CHECK(nres = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, load));
			nres->expr.u.swizzle = _essl_create_scalar_swizzle(0);
			_essl_ensure_compatible_node(nres, res);
			return nres;
		} 
		else if(!_essl_string_cmp(name, fbstencil_name))
		{
			node *load;
			ESSL_CHECK(load = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_LD_ZS, NULL, NULL, NULL));
			_essl_ensure_compatible_node(load, res);
			ESSL_CHECK(load = mali200_preschedule_single_node(ctx, load));
			ESSL_CHECK(load->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 4)); /* vec4 as LD_ZS clobbers an entire register */
			ESSL_CHECK(nres = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, load));
			nres->expr.u.swizzle = _essl_create_scalar_swizzle(1);
			_essl_ensure_compatible_node(nres, res);
			return nres;
		} 
		else 
		{
			assert(0);
		}

	}
	return res;
}

static node * mali200_preschedule_single_node(mali200_preschedule_context * ctx, node *n)
{
	node_kind kind;
	expression_operator op;
	ESSL_CHECK(n);
	kind = n->hdr.kind;
	op = n->expr.operation;

	if(kind == EXPR_KIND_BINARY)
	{
		if(op == EXPR_OP_DIV)
		{
			return handle_div(ctx, n);
		}
		else if(op == EXPR_OP_ADD)
		{
			return handle_add(ctx, n);
		}
		else if(op == EXPR_OP_SUB)
		{
			return handle_sub(ctx, n);
		}
		else if(op == EXPR_OP_EQ || op == EXPR_OP_NE)
		{
			return handle_cmp(ctx, n);
		}
		else if(op == EXPR_OP_MUL)
		{
			return handle_mul(ctx, n);
		}
		else
		{
			return n;
		}

	}
	else if(kind == EXPR_KIND_TERNARY)
	{
		if(op == EXPR_OP_CONDITIONAL_SELECT)
		{
			return handle_csel(ctx, n);
		}
		else
		{
			return n;
		}
	}
	else if(kind == EXPR_KIND_TYPE_CONVERT)
	{
		return handle_type_convert(ctx, n);
	}
	else if(kind == EXPR_KIND_BUILTIN_FUNCTION_CALL)
	{
		if(op == EXPR_OP_FUN_ATAN)
		{
			return handle_atan(ctx, n);
		}
		else if(op == EXPR_OP_FUN_SIN_0_1
				|| op == EXPR_OP_FUN_SIN_0_1
				|| op == EXPR_OP_FUN_COS_0_1
				|| op == EXPR_OP_FUN_EXP2
				|| op == EXPR_OP_FUN_LOG2
				|| op == EXPR_OP_FUN_SQRT
				|| op == EXPR_OP_FUN_INVERSESQRT
				|| op == EXPR_OP_FUN_RCP
				|| op == EXPR_OP_FUN_RCC)
		{
			return handle_unary_lut_ops(ctx, n);
		}
		else if(op == EXPR_OP_FUN_ALL
				|| op == EXPR_OP_FUN_ANY)
		{
			return handle_any_all(ctx, n);
		}
		else if(op == EXPR_OP_FUN_SIN
				|| op == EXPR_OP_FUN_COS)
		{
			return handle_sin_cos(ctx, n);
		}
		else if(op == EXPR_OP_FUN_TAN)
		{
			return handle_tan(ctx, n);
		}
		else if(op == EXPR_OP_FUN_ASIN)
		{
			return handle_asin(ctx, n);
		}
		else if(op == EXPR_OP_FUN_ACOS)
		{
			return handle_acos(ctx, n);
		}
		else if(op == EXPR_OP_FUN_POW)
		{
			return handle_pow(ctx, n);
		}
		else if(op == EXPR_OP_FUN_SIGN)
		{
			return handle_sign(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MOD)
		{
			return handle_mod(ctx, n);
		}
		else if(op == EXPR_OP_FUN_CLAMP)
		{
			return handle_clamp(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MAX)
		{
			return handle_max(ctx, n);
		}
		else if(op == EXPR_OP_FUN_DOT)
		{
			return handle_dot(ctx, n);
		}
		else if(op == EXPR_OP_FUN_LENGTH)
		{
			return handle_length(ctx, n);
		}
		else if(op == EXPR_OP_FUN_NORMALIZE)
		{
			return handle_normalize(ctx, n);
		}
		else if(op == EXPR_OP_FUN_DFDX
				|| op == EXPR_OP_FUN_DFDY)
		{
			return handle_derivatives(ctx, n);
		}
		else if(op == EXPR_OP_FUN_FWIDTH)
		{
			return handle_fwidth(ctx, n);
		}
		else if(_essl_node_is_texture_operation(n))
		{
			return handle_texture(ctx, n);
		}
		else
		{
			return n;
		}
	}
	else if(kind == EXPR_KIND_LOAD
			|| kind == EXPR_KIND_STORE)
	{
		return handle_memory_op(ctx, n);
	}
	else if(kind == EXPR_KIND_TRANSFER)
	{
		return GET_CHILD(n, 0);
	}
	else if(kind == EXPR_KIND_DONT_CARE)
	{
		node *tmp;
		ESSL_CHECK(tmp = create_float_constant(ctx, 0.0, GET_NODE_VEC_SIZE(n)));
		_essl_ensure_compatible_node(tmp, n);
		return mali200_preschedule_single_node(ctx, tmp);
	}
	else
	{
		return n;
	}
}

/*@null@*/ static node *process_node_w(mali200_preschedule_context *ctx, node *n)
{

	node *nn;
	
	ESSL_CHECK(nn = mali200_preschedule_single_node(ctx, n));
	return nn;


}

/*@null@*/ static node *process_node(mali200_preschedule_context *ctx, node *n)
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
			if(src->source->hdr.kind != EXPR_KIND_PHI)
			{
				node *tmp;
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

static memerr handle_block(mali200_preschedule_context *ctx, basic_block *b)
{
	phi_list *phi;
	control_dependent_operation *c_op;
	if(b->source)
	{
		node *tmp;
		ESSL_CHECK(tmp = process_node(ctx, b->source));
		b->source = tmp;
	}
	for(c_op = b->control_dependent_ops; c_op != 0; c_op = c_op->next)
	{
		node *tmp;
		node *n = c_op->op;
		assert(n->hdr.is_control_dependent);
		ESSL_CHECK(tmp = process_node(ctx, n));
		if(!tmp->hdr.is_control_dependent)
		{
			/* node is dead, kill it */
			control_dependent_operation *n_cd_op;
			control_dependent_operation **cd_it;
			operation_dependency **dep;
			unsigned i;
			ESSL_CHECK(n_cd_op = _essl_ptrdict_lookup(&ctx->cfg->control_dependence, n));
			_essl_ptrdict_remove(&ctx->cfg->control_dependence, n);


			for(i = 0; i < ctx->cfg->n_blocks; ++i)
			{
				basic_block *block = ctx->cfg->output_sequence[i];
				for(cd_it = &block->control_dependent_ops; (*cd_it) != 0; /* empty */)
				{		
					for(dep = &(*cd_it)->dependencies; *dep != 0; )
					{
						if((*dep)->dependent_on == n_cd_op)
						{
							*dep = (*dep)->next;
						} else {
							dep = &((*dep)->next);
						}
					}
					if((*cd_it)->op == n)
					{
						*cd_it = (*cd_it)->next; /* delete the node and go to the next */
					} else {
						cd_it = &((*cd_it)->next); /* skip to the next node */
					}
				}
			}


		} else {
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




memerr _essl_mali200_preschedule(mempool *pool, target_descriptor *desc, typestorage_context *ts_ctx, control_flow_graph *cfg, translation_unit *tu)
{

	mali200_preschedule_context ctx;
	unsigned int i;


	ctx.pool = pool;
	ctx.cfg = cfg;
	ctx.desc = desc;
	ctx.typestor_ctx = ts_ctx;
	ctx.tu = tu;
	ESSL_CHECK(_essl_ptrdict_init(&ctx.visited, pool));
	for (i = 0 ; i < cfg->n_blocks ; i++) {
		ESSL_CHECK(handle_block(&ctx, cfg->postorder_sequence[i]));
	}
	
	return MEM_OK;
}
