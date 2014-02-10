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
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_preschedule.h"
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
	compiler_options *opts;
	error_context *error_context;
} maligp2_preschedule_context;


static node *create_maligp2_store_constructor_expression(mempool *pool, unsigned n_args)
{
	return _essl_new_node(pool, EXPR_KIND_MALIGP2_STORE_CONSTRUCTOR, n_args);
}

/*@null@*/ static node *create_float_constant(maligp2_preschedule_context *ctx, float value, unsigned vec_size)
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


/*@null@*/ static node *create_int_constant(maligp2_preschedule_context *ctx, int value, unsigned vec_size)
{
	node *n;
	unsigned i;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, vec_size));
	n->expr.u.value[0] = ctx->desc->int_to_scalar(value);
	for(i = 1; i < vec_size; ++i)
	{
		n->expr.u.value[i] = n->expr.u.value[0];
	}

	ESSL_CHECK(n->hdr.type = _essl_get_type_with_default_size_for_target(ctx->typestor_ctx, TYPE_INT, vec_size, ctx->desc));
	return n;
}



/*@null@*/ static const type_specifier *get_scalar_type(maligp2_preschedule_context *ctx, const type_specifier *t)
{
	return _essl_get_type_with_given_vec_size(ctx->typestor_ctx, t, 1);
}



/*@null@*/ static node *create_scalar_swizzle(maligp2_preschedule_context *ctx, node *n, unsigned swz_idx)
{
	node *swz = 0;
	ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, n));
	swz->expr.u.swizzle = _essl_create_scalar_swizzle(swz_idx);
	_essl_ensure_compatible_node(swz, n);
	ESSL_CHECK(swz->hdr.type = get_scalar_type(ctx, n->hdr.type));
	return swz;

}

static memerr process_address(maligp2_preschedule_context *ctx, node *access, node_extra *access_info, node *address, nodeptr *res)
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
        if(child1->hdr.kind == EXPR_KIND_CONSTANT)
		{
            access_info->address_offset += ctx->desc->scalar_to_int(child1->expr.u.value[0]) * ctx->desc->get_array_stride(ctx->desc, address->hdr.type, access->expr.u.load_store.address_space);
            return process_address(ctx, access, access_info, child0, res);
		} else
		{
			unsigned scale = ctx->desc->get_array_stride(ctx->desc, address->hdr.type, access->expr.u.load_store.address_space) / access_info->address_multiplier;
			node *mul = 0;
			assert(scale > 0);
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



/*@null@*/ static node *clone_load_expression(maligp2_preschedule_context *ctx, node *orig)
{
	node *clone;
	ESSL_CHECK(clone = _essl_clone_node(ctx->pool, orig));

	/* fake the visit of the cloned node */
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, clone, clone));

	if(orig->hdr.is_control_dependent)
	{
		/* now we need to clone the control dependencies as well */
		ESSL_CHECK(_essl_clone_control_dependent_op(orig, clone, ctx->cfg, ctx->pool));

	}

	return clone;
}


static memerr kill_load_expression(maligp2_preschedule_context *ctx, node *n)
{

	if(n->hdr.is_control_dependent)
	{
		/* identify and kill dependencies on this node */
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
	}

	return MEM_OK;
}

static node * maligp2_preschedule_single_node(maligp2_preschedule_context * ctx, node *_parm_node);
static node * handle_vector_builtin_function(maligp2_preschedule_context * ctx, node *res);
static node * handle_vector_binary_expression(maligp2_preschedule_context * ctx, node *res);

static /*@null@*/ node *create_atan_approximation(maligp2_preschedule_context *ctx, node *u, /*@null@*/ node *v, node *compatible_node)
{	/* atan(u) or atan2(u, v) */


	/*
	   function res=cgatan2(u, v)

	   coefs = [-0.01348047 0.05747731 -0.1212391 0.1956359 -0.3329946 0.9999956];

	   abs_u = abs(u);
	   abs_v = abs(v);
	   max_u_v = max(abs_v, abs_u);
	   inv_max_u_v = 1.0./max_u_v;
	   min_u_v = min(abs_v, abs_u);
	   x = min_u_v.*inv_max_u_v;
	   xsqr = x.*x;
	   horner_tmp = xsqr .* coefs(1) + coefs(2);
	   horner_tmp = horner_tmp.*xsqr + coefs(3);
	   horner_tmp = horner_tmp.*xsqr + coefs(4);
	   horner_tmp = horner_tmp.*xsqr + coefs(5);
	   horner_tmp = horner_tmp.*xsqr + coefs(6);
	   poly_res = horner_tmp.*x;

	   if abs_v < abs_u
	   tmp_res = pi/2 - poly_res;
	   else
	   tmp_res = poly_res;
	   end

	   if v < 0.0
	   tmp2_res = pi - tmp_res;
	   else
	   tmp2_res = tmp_res;
	   end

	res = sign(u)*tmp2_res;


	*/
	essl_bool is_atan2 = (v != NULL);
	node *constant;
	node *abs_u;
	node *abs_v;
	node *upper;
	node *lower;
	node *poly_res;
	node *tmp_res;
	node *res;
	ESSL_CHECK(abs_u = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ABS, u, NULL, NULL));
	_essl_ensure_compatible_node(abs_u, compatible_node);
	ESSL_CHECK(abs_u = maligp2_preschedule_single_node(ctx, abs_u));
	if(is_atan2)
	{
		ESSL_CHECK(abs_v = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ABS, v, NULL, NULL));
		_essl_ensure_compatible_node(abs_v, compatible_node);
		ESSL_CHECK(abs_v = maligp2_preschedule_single_node(ctx, abs_v));

		ESSL_CHECK(upper = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, abs_u, abs_v, NULL));
		_essl_ensure_compatible_node(upper, compatible_node);
		ESSL_CHECK(upper = maligp2_preschedule_single_node(ctx, upper));

		ESSL_CHECK(lower = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MIN, abs_u, abs_v, NULL));
		_essl_ensure_compatible_node(lower, compatible_node);
		ESSL_CHECK(lower = maligp2_preschedule_single_node(ctx, lower));

	} 
	else 
	{
		ESSL_CHECK(abs_v = create_float_constant(ctx, 1.0, 1));


		ESSL_CHECK(constant = create_float_constant(ctx, 1.0, 1));
		ESSL_CHECK(upper = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, abs_u, constant, NULL));
		_essl_ensure_compatible_node(upper, compatible_node);
		ESSL_CHECK(upper = maligp2_preschedule_single_node(ctx, upper));

		ESSL_CHECK(constant = create_float_constant(ctx, 1.0, 1));
		ESSL_CHECK(lower = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MIN, abs_u, constant, NULL));
		_essl_ensure_compatible_node(lower, compatible_node);
		ESSL_CHECK(lower = maligp2_preschedule_single_node(ctx, lower));
		
	}

	{
		node *inv_upper;
		node *x;
		node *xsqr;
		node *acc = NULL;
		node *x_acc;

		int i;
		float coefs[6] = {-0.01348047f, 0.05747731f, -0.1212391f, 0.1956359f, -0.3329946f, 0.9999956f};
		ESSL_CHECK(inv_upper = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_RCC, upper, NULL, NULL));
		_essl_ensure_compatible_node(inv_upper, compatible_node);
		ESSL_CHECK(inv_upper = maligp2_preschedule_single_node(ctx, inv_upper));

		ESSL_CHECK(x = _essl_new_binary_expression(ctx->pool, lower, EXPR_OP_MUL, inv_upper));
		_essl_ensure_compatible_node(x, compatible_node);
		ESSL_CHECK(x = maligp2_preschedule_single_node(ctx, x));

		ESSL_CHECK(xsqr = _essl_new_binary_expression(ctx->pool, x, EXPR_OP_MUL, x));
		_essl_ensure_compatible_node(xsqr, compatible_node);
		ESSL_CHECK(xsqr = maligp2_preschedule_single_node(ctx, xsqr));

		x_acc = x;

		for(i = 0; i < 6; ++i)
		{
			node *tmp2;
			ESSL_CHECK(constant = create_float_constant(ctx, coefs[6-i-1], 1));
			ESSL_CHECK(tmp2 = _essl_new_binary_expression(ctx->pool, constant, EXPR_OP_MUL, x_acc));
			_essl_ensure_compatible_node(tmp2, compatible_node);
			ESSL_CHECK(tmp2 = maligp2_preschedule_single_node(ctx, tmp2));

			if(i == 0)
			{
				acc = tmp2;
			} else {
				ESSL_CHECK(acc = _essl_new_binary_expression(ctx->pool, tmp2, EXPR_OP_ADD, acc));
				_essl_ensure_compatible_node(acc, compatible_node);
				ESSL_CHECK(acc = maligp2_preschedule_single_node(ctx, acc));
			}
			if(i == 5)
			{
				x_acc = NULL;
			} else {
				ESSL_CHECK(x_acc = _essl_new_binary_expression(ctx->pool, x_acc, EXPR_OP_MUL, xsqr));
				_essl_ensure_compatible_node(x_acc, compatible_node);
				ESSL_CHECK(x_acc = maligp2_preschedule_single_node(ctx, x_acc));
			}

		}
		poly_res = acc;
	
	}

	{
		/* deal with abs_v < abs_u */
		node *condition;
		node *negative_res;
		ESSL_CHECK(condition = _essl_new_binary_expression(ctx->pool, abs_v, EXPR_OP_LT, abs_u));
		_essl_ensure_compatible_node(condition, compatible_node);
		ESSL_CHECK(condition = maligp2_preschedule_single_node(ctx, condition));

		ESSL_CHECK(constant = create_float_constant(ctx, (float)(M_PI/2.0), 1));

		ESSL_CHECK(negative_res = _essl_new_binary_expression(ctx->pool, constant, EXPR_OP_SUB, poly_res));
		_essl_ensure_compatible_node(negative_res, compatible_node);
		ESSL_CHECK(negative_res = maligp2_preschedule_single_node(ctx, negative_res));

		ESSL_CHECK(tmp_res = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, condition, negative_res, poly_res));
		_essl_ensure_compatible_node(tmp_res, compatible_node);
		ESSL_CHECK(tmp_res = maligp2_preschedule_single_node(ctx, tmp_res));

	}
	if(is_atan2)
	{
		/* handle v negative */
		node *condition;
		node *negative_res;
		ESSL_CHECK(constant = create_float_constant(ctx, 0.0, 1));

		ESSL_CHECK(condition = _essl_new_binary_expression(ctx->pool, v, EXPR_OP_LT, constant));
		_essl_ensure_compatible_node(condition, compatible_node);
		ESSL_CHECK(condition = maligp2_preschedule_single_node(ctx, condition));

		ESSL_CHECK(constant = create_float_constant(ctx, (float)M_PI, 1));

		ESSL_CHECK(negative_res = _essl_new_binary_expression(ctx->pool, constant, EXPR_OP_SUB, tmp_res));
		_essl_ensure_compatible_node(negative_res, compatible_node);
		ESSL_CHECK(negative_res = maligp2_preschedule_single_node(ctx, negative_res));

		ESSL_CHECK(tmp_res = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, condition, negative_res, tmp_res));
		_essl_ensure_compatible_node(tmp_res, compatible_node);
		ESSL_CHECK(tmp_res = maligp2_preschedule_single_node(ctx, tmp_res));
	}


	{
		/* handle u negative */
		node *condition;
		node *negative_res;
		ESSL_CHECK(constant = create_float_constant(ctx, 0.0, 1));

		ESSL_CHECK(condition = _essl_new_binary_expression(ctx->pool, u, EXPR_OP_LT, constant));
		_essl_ensure_compatible_node(condition, compatible_node);
		ESSL_CHECK(condition = maligp2_preschedule_single_node(ctx, condition));

		ESSL_CHECK(negative_res = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, tmp_res));
		_essl_ensure_compatible_node(negative_res, compatible_node);
		ESSL_CHECK(negative_res = maligp2_preschedule_single_node(ctx, negative_res));

		ESSL_CHECK(res = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, condition, negative_res, tmp_res));
		_essl_ensure_compatible_node(res, compatible_node);
		ESSL_CHECK(res = maligp2_preschedule_single_node(ctx, res));
	}

	return res;

}


static /*@null@*/ node *integer_pow(maligp2_preschedule_context *ctx, node *x, int y, node *compatible_node)
{
	node *res;
	assert(y >= 0);
	if(y == 0)
	{
		ESSL_CHECK(res = create_float_constant(ctx, 1.0, _essl_get_type_size(x->hdr.type)));
		_essl_ensure_compatible_node(res, compatible_node);
	} 
	else if(y == 1)
	{
		res = x;
	} 
	else 
	{
		node *xsqr;
		ESSL_CHECK(xsqr = _essl_new_binary_expression(ctx->pool, x, EXPR_OP_MUL, x));
		_essl_ensure_compatible_node(xsqr, compatible_node);
		ESSL_CHECK(xsqr = maligp2_preschedule_single_node(ctx, xsqr));
		ESSL_CHECK(res = integer_pow(ctx, xsqr, y>>1, compatible_node));
		if(y & 1)
		{
			ESSL_CHECK(res = _essl_new_binary_expression(ctx->pool, x, EXPR_OP_MUL, res));
			_essl_ensure_compatible_node(res, compatible_node);
			ESSL_CHECK(res = maligp2_preschedule_single_node(ctx, res));
		
		}
	}
	return res;
}


static /*@null@*/ node *extend_with_swizzle(maligp2_preschedule_context *ctx, node *n, node *compatible_node)
{
	unsigned swz_size = _essl_get_type_size(compatible_node->hdr.type);
	unsigned i;
	node *res;
	assert(_essl_get_type_size(n->hdr.type) == 1);

	ESSL_CHECK(res = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, n));
	_essl_ensure_compatible_node(res, compatible_node);

	for(i = 0; i < swz_size; ++i)
	{
		res->expr.u.swizzle.indices[i] = 0;
	}
	return maligp2_preschedule_single_node(ctx, res);
}

static node * handle_rcp(maligp2_preschedule_context * ctx, node *res)
{
	node *b;

	ESSL_CHECK(b = GET_CHILD(res, 0));
	if(b->hdr.kind == EXPR_KIND_CONSTANT)
	{
		node *newres;
		scalar_type dummy = ctx->desc->float_to_scalar(0.0);
		scalar_type one = ctx->desc->float_to_scalar(1.0);

		unsigned bsize;
		unsigned i;

		bsize = _essl_get_type_size(b->hdr.type);
		ESSL_CHECK(newres = _essl_new_constant_expression(ctx->pool, bsize));
		_essl_ensure_compatible_node(newres, res);
		for (i = 0; i < bsize; ++i)
		{
			newres->expr.u.value[i] = ctx->desc->constant_fold(res->hdr.type, EXPR_OP_DIV, one, b->expr.u.value[i], dummy);
		}
		return maligp2_preschedule_single_node(ctx, newres);
	}
	else if(GET_VEC_SIZE(res->hdr.type) > 1)
	{
		return handle_vector_builtin_function(ctx, res);
	}
	else
	{
		return res;
	}
}

static node * handle_div(maligp2_preschedule_context * ctx, node *res)
{
	  /* rewrite as a * (1.0/b) if a != 1.0 */
	node *divide;
	node *r;
	type_specifier *t;
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	ESSL_CHECK(divide = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_RCP, b, NULL, NULL));
	_essl_ensure_compatible_node(divide, b);
	ESSL_CHECK(t = _essl_clone_type(ctx->pool, res->hdr.type));
	t->basic_type = TYPE_FLOAT;
	t->u.basic.vec_size = GET_NODE_VEC_SIZE(b);
	divide->hdr.type = t;
	ESSL_CHECK(divide = maligp2_preschedule_single_node(ctx, divide));
	
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
			ESSL_CHECK(r = maligp2_preschedule_single_node(ctx, r));
		}
	} 
	else 
	{
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, divide));
		_essl_ensure_compatible_node(r, res);
		ESSL_CHECK(r = maligp2_preschedule_single_node(ctx, r));
	}
	if(res->hdr.type->basic_type == TYPE_INT)
	{
		node *tmp;
		ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_TRUNC, r, 0, 0));
		_essl_ensure_compatible_node(tmp, res);
		return maligp2_preschedule_single_node(ctx, tmp);
	}
	else 
	{
		return r;
	}
}

static node * handle_sub(maligp2_preschedule_context * ctx, node *res)
{
	/* rewrite as a + -b */
	node *tmp;
	node *r;
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	ESSL_CHECK(tmp = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, b));
	_essl_ensure_compatible_node(tmp, b);
	ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
	ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_ADD, tmp));
	_essl_ensure_compatible_node(r, res);
	return maligp2_preschedule_single_node(ctx, r);
}

static node * handle_not(maligp2_preschedule_context * ctx, node *res)
{
	/* this becomes 1.0 - a */
	node *constant, *sub;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(constant = create_float_constant(ctx, 1.0, 1));
	ESSL_CHECK(sub = _essl_new_binary_expression(ctx->pool, constant, EXPR_OP_SUB, a));
	_essl_ensure_compatible_node(sub, res);
	return maligp2_preschedule_single_node(ctx, sub);
}

static essl_bool is_swizzle_of_undef(const node *res)
{
	unsigned i;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(res->expr.u.swizzle.indices[i] != -1)
		{
			return ESSL_FALSE;
		}
	}
	return ESSL_TRUE;
}

static node * handle_swizzle(maligp2_preschedule_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	if(GET_VEC_SIZE(res->hdr.type) > 1)
	{
		/* split into separate swizzles + constructor */
		node *newres, *tmp;
		unsigned i, n = GET_NODE_VEC_SIZE(res);
		ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool,n));
		_essl_ensure_compatible_node(newres, res);
		for(i = 0; i < n; ++i)
		{
			ESSL_CHECK(tmp = create_scalar_swizzle(ctx, a, res->expr.u.swizzle.indices[i]));
			ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
			SET_CHILD(newres, i, tmp);
		}
		return maligp2_preschedule_single_node(ctx, newres);
	}
	else if(is_swizzle_of_undef(res))
	{
		node *tmp;
		ESSL_CHECK(tmp = _essl_new_dont_care_expression(ctx->pool, res->hdr.type));
		_essl_ensure_compatible_node(tmp, res);
		return maligp2_preschedule_single_node(ctx, tmp);
	}
	else if(a->hdr.kind == EXPR_KIND_BUILTIN_CONSTRUCTOR)
	{
		node *child = NULL;
		assert(_essl_get_type_size(res->hdr.type) == 1);
		assert(res->expr.u.swizzle.indices[0] < (int)(GET_N_CHILDREN(a)));
		ESSL_CHECK(child = GET_CHILD(a, res->expr.u.swizzle.indices[0]));
		/*assert(_essl_get_type_size(child->hdr.type) == 1);*/
		return child;
	}
	else if(res->expr.u.swizzle.indices[0] == 0 
			&& GET_NODE_VEC_SIZE(res) == 1 
			&& GET_NODE_VEC_SIZE(a) == 1)
	{
		return a;
	}
	else if(a->hdr.kind == EXPR_KIND_CONSTANT)
	{
		node *tmp;
		assert(GET_NODE_VEC_SIZE(res) == 1);
		ESSL_CHECK(tmp = _essl_new_constant_expression(ctx->pool, 1));
		_essl_ensure_compatible_node(tmp, res);
		assert(res->expr.u.swizzle.indices[0] >= 0 && res->expr.u.swizzle.indices[0] < (int)_essl_get_type_size(a->hdr.type));
		tmp->expr.u.value[0] = a->expr.u.value[res->expr.u.swizzle.indices[0]];
		return maligp2_preschedule_single_node(ctx, tmp);
	}
	else if(a->hdr.kind == EXPR_KIND_DONT_CARE)
	{
		node *tmp;
		ESSL_CHECK(tmp = _essl_new_dont_care_expression(ctx->pool, res->hdr.type));
		_essl_ensure_compatible_node(tmp, res);
		return maligp2_preschedule_single_node(ctx, tmp);
	}
	else if(_essl_node_is_texture_operation(a))
	{
		REPORT_ERROR(ctx->error_context, ERR_LP_UNDEFINED_IDENTIFIER, a->hdr.source_offset, "Function '%s' not supported on target\n", a->expr.u.fun.sym->name);
		return NULL;
	}
	else
	{
		assert(0 && "uh-oh, wayward swizzle");
		return NULL;
	}
}

static node * handle_unary_vector_expression(maligp2_preschedule_context * ctx, node *res)
{
	node *a;
	/* split into separate operations + constructor */
	node *newres, *tmp, *swz;
	unsigned i, n = GET_NODE_VEC_SIZE(res);
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool, n));
	_essl_ensure_compatible_node(newres, res);
	for(i = 0; i < n; ++i)
	{
		ESSL_CHECK(swz = create_scalar_swizzle(ctx, a, i));
		ESSL_CHECK(swz = maligp2_preschedule_single_node(ctx, swz));
		
		ESSL_CHECK(tmp = _essl_new_unary_expression(ctx->pool, res->expr.operation, swz));
		_essl_ensure_compatible_node(tmp, swz);
		ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
		SET_CHILD(newres, i, tmp);
	}
	return maligp2_preschedule_single_node(ctx, newres);
}

static node * handle_trunc(maligp2_preschedule_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	if(a->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL
		&& a->expr.operation == EXPR_OP_FUN_FLOOR)
	{
		return a;
	}
	else
	{
		node *abs, *floor, *sign, *mul;
		ESSL_CHECK(abs = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ABS, a, 0, 0));
		_essl_ensure_compatible_node(abs, res);
		abs->hdr.type = a->hdr.type;
		ESSL_CHECK(abs = maligp2_preschedule_single_node(ctx, abs));

		ESSL_CHECK(floor = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_FLOOR, abs, 0, 0));
		_essl_ensure_compatible_node(floor, res);
		ESSL_CHECK(floor = maligp2_preschedule_single_node(ctx, floor));

		ESSL_CHECK(sign = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SIGN, a, 0, 0));
		_essl_ensure_compatible_node(sign, res);
		ESSL_CHECK(sign = maligp2_preschedule_single_node(ctx, sign));

		ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, sign, EXPR_OP_MUL, floor));
		_essl_ensure_compatible_node(mul, res);
		return maligp2_preschedule_single_node(ctx, mul);
	}
}

static node * handle_vector_combine(maligp2_preschedule_context * ctx, node *res)
{
	/* split into separate operations + constructor */
	node *newres, *swz;
	unsigned i, n = GET_NODE_VEC_SIZE(res);
	ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool, n));
	_essl_ensure_compatible_node(newres, res);
	for(i = 0; i < n; ++i)
	{
		node *sel;
		assert(res->expr.u.combiner.mask[i] != -1);
		ESSL_CHECK(sel = GET_CHILD(res, res->expr.u.combiner.mask[i]));
		ESSL_CHECK(swz = create_scalar_swizzle(ctx, sel, i));
		ESSL_CHECK(swz = maligp2_preschedule_single_node(ctx, swz));
		
		SET_CHILD(newres, i, swz);
	}
	return maligp2_preschedule_single_node(ctx, newres);
}

static node * handle_subvector_access(maligp2_preschedule_context * ctx, node *res)
{
	node *a;
	node *idx;
	unsigned n;
	const type_specifier *boolean_type;
	node *xy_part = NULL, *zw_part = NULL, *newres = NULL, *cond = NULL;
	node *cnst;

	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(idx = GET_CHILD(res, 1));
	n = GET_NODE_VEC_SIZE(a);
	assert(n > 1);
	ESSL_CHECK(boolean_type = _essl_get_type_with_default_size_for_target(ctx->typestor_ctx, TYPE_BOOL, 1, ctx->desc));
	{
		node *cnst, *lt, *x, *y;
		ESSL_CHECK(cnst = create_int_constant(ctx, 0, 1));
		_essl_ensure_compatible_node(cnst, res);
		cnst->hdr.type = idx->hdr.type;
		ESSL_CHECK(cnst = maligp2_preschedule_single_node(ctx, cnst));
		
		ESSL_CHECK(lt = _essl_new_binary_expression(ctx->pool, cnst, EXPR_OP_LT, idx));
		_essl_ensure_compatible_node(lt, res);
		lt->hdr.type = boolean_type;
		ESSL_CHECK(lt = maligp2_preschedule_single_node(ctx, lt));

		ESSL_CHECK(x = create_scalar_swizzle(ctx, a, 0));
		ESSL_CHECK(x = maligp2_preschedule_single_node(ctx, x));

		ESSL_CHECK(y = create_scalar_swizzle(ctx, a, 1));
		ESSL_CHECK(y = maligp2_preschedule_single_node(ctx, y));

		ESSL_CHECK(xy_part = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, lt, y, x));
		_essl_ensure_compatible_node(xy_part, res);
		ESSL_CHECK(xy_part = maligp2_preschedule_single_node(ctx, xy_part));
	}

	if(n == 2) 
	{
		return xy_part;
	}
	else if(n == 3)
	{
		ESSL_CHECK(zw_part = create_scalar_swizzle(ctx, a, 2));
		ESSL_CHECK(zw_part = maligp2_preschedule_single_node(ctx, zw_part));
	} 
	else 
	{
		node *cnst, *lt, *z, *w;
		ESSL_CHECK(cnst = create_int_constant(ctx, 2, 1));
		_essl_ensure_compatible_node(cnst, res);
		cnst->hdr.type = idx->hdr.type;
		ESSL_CHECK(cnst = maligp2_preschedule_single_node(ctx, cnst));
		
		ESSL_CHECK(lt = _essl_new_binary_expression(ctx->pool, cnst, EXPR_OP_LT, idx));
		_essl_ensure_compatible_node(lt, res);
		lt->hdr.type = boolean_type;
		ESSL_CHECK(lt = maligp2_preschedule_single_node(ctx, lt));

		ESSL_CHECK(z = create_scalar_swizzle(ctx, a, 2));
		ESSL_CHECK(z = maligp2_preschedule_single_node(ctx, z));

		ESSL_CHECK(w = create_scalar_swizzle(ctx, a, 3));
		ESSL_CHECK(w = maligp2_preschedule_single_node(ctx, w));

		ESSL_CHECK(zw_part = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, lt, w, z));
		_essl_ensure_compatible_node(zw_part, res);
		ESSL_CHECK(zw_part = maligp2_preschedule_single_node(ctx, zw_part));
	}

	ESSL_CHECK(cnst = create_int_constant(ctx, 1, 1));
	_essl_ensure_compatible_node(cnst, res);
	cnst->hdr.type = idx->hdr.type;
	ESSL_CHECK(cnst = maligp2_preschedule_single_node(ctx, cnst));

	ESSL_CHECK(cond = _essl_new_binary_expression(ctx->pool, cnst, EXPR_OP_LT, idx));
	_essl_ensure_compatible_node(cond, res);
	cond->hdr.type = boolean_type;
	ESSL_CHECK(cond = maligp2_preschedule_single_node(ctx, cond));

	ESSL_CHECK(newres = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, cond, zw_part, xy_part));
	_essl_ensure_compatible_node(newres, res);
	return maligp2_preschedule_single_node(ctx, newres);
}

static node * handle_subvector_update(maligp2_preschedule_context * ctx, node *res)
{
	/* strategy: generate code like this:
	   x = csel(idx, scalar, vec.x);
	   idx = idx - 1;
	   y = csel(idx, scalar, vec.y);
	   idx = idx - 1;
	   z = csel(idx, scalar, vec.z);
	   idx = idx - 1;
	   w = csel(idx, scalar, vec.w);
       return vec4(x, y, z, w);
	*/
	unsigned n;
	unsigned i;
	node *cons;
	node *minus_one;
	node *idx;
	node *scalar;
	node *vec;
	node *index;
	ESSL_CHECK(idx = GET_CHILD(res, 0));
	ESSL_CHECK(scalar = GET_CHILD(res, 1));
	ESSL_CHECK(vec = GET_CHILD(res, 2));
	ESSL_CHECK(minus_one = create_float_constant(ctx, -1.0, 1));
	n = GET_NODE_VEC_SIZE(vec);

	ESSL_CHECK(cons = _essl_new_builtin_constructor_expression(ctx->pool, n));
	_essl_ensure_compatible_node(cons, res);
	index = idx;
	for(i = 0; i < n; ++i)
	{
		node *orig, *csel;
		ESSL_CHECK(orig = create_scalar_swizzle(ctx, vec, i));
		ESSL_CHECK(orig = maligp2_preschedule_single_node(ctx, orig));
		
		ESSL_CHECK(csel = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, index, orig, scalar));
		_essl_ensure_compatible_node(csel, res);
		csel->hdr.type = scalar->hdr.type;
		ESSL_CHECK(csel = maligp2_preschedule_single_node(ctx, csel));
		SET_CHILD(cons, i, csel);

		if(i != (n - 1))
		{
			node *tmp;
			ESSL_CHECK(tmp = _essl_new_binary_expression(ctx->pool, index, EXPR_OP_ADD, minus_one));
			_essl_ensure_compatible_node(tmp, index);
			ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
			index = tmp;
		}
	}
	return maligp2_preschedule_single_node(ctx, cons);
}

static node * handle_comparison(maligp2_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	expression_operator op;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	op = res->expr.operation;
	if(op == EXPR_OP_LE)
	{
		node *r;
		assert(GET_NODE_VEC_SIZE(res) == 1);
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, b, EXPR_OP_GE, a));
		_essl_ensure_compatible_node(r, res);
		return maligp2_preschedule_single_node(ctx, r);
	}
	else if(op == EXPR_OP_GT)
	{
		node *r;
		assert(GET_NODE_VEC_SIZE(res) == 1);
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, b, EXPR_OP_LT, a));
		_essl_ensure_compatible_node(r, res);
		return maligp2_preschedule_single_node(ctx, r);
	}
	else if (op == EXPR_OP_EQ || op == EXPR_OP_NE)
	{
		if(GET_NODE_VEC_SIZE(a) > 1)
		{
			node *all, *fun_cmp;
			expression_operator fun_op = (op == EXPR_OP_EQ) ? EXPR_OP_FUN_EQUAL : EXPR_OP_FUN_NOTEQUAL;
			expression_operator all_op = (op == EXPR_OP_EQ) ? EXPR_OP_FUN_ALL : EXPR_OP_FUN_ANY;
			ESSL_CHECK(fun_cmp = _essl_new_builtin_function_call_expression(ctx->pool, fun_op, a, b, NULL));
			_essl_ensure_compatible_node(fun_cmp, a);
			ESSL_CHECK(fun_cmp = maligp2_preschedule_single_node(ctx, fun_cmp));
			ESSL_CHECK(all = _essl_new_builtin_function_call_expression(ctx->pool, all_op, fun_cmp, NULL, NULL));
			_essl_ensure_compatible_node(all, res);
			return maligp2_preschedule_single_node(ctx, all);
		}
		else
		{
			if(op == EXPR_OP_EQ)
			{
				node *tmp1, *tmp2, *tmp3;
				assert(GET_NODE_VEC_SIZE(res) == 1);
				ESSL_CHECK(tmp1 = _essl_new_binary_expression(ctx->pool, b, EXPR_OP_GE, a));
				_essl_ensure_compatible_node(tmp1, res);
				ESSL_CHECK(tmp1 = maligp2_preschedule_single_node(ctx, tmp1));
				ESSL_CHECK(tmp2 = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_GE, b));
				_essl_ensure_compatible_node(tmp2, res);
				ESSL_CHECK(tmp2 = maligp2_preschedule_single_node(ctx, tmp2));

				ESSL_CHECK(tmp3 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MIN, tmp1, tmp2, 0));
				_essl_ensure_compatible_node(tmp3, res);
				return maligp2_preschedule_single_node(ctx, tmp3);
			}
			else
			{
				node *tmp1, *tmp2, *tmp3;
				assert(op == EXPR_OP_NE);
				assert(GET_NODE_VEC_SIZE(res) == 1);
				ESSL_CHECK(tmp1 = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_LT, b));
				_essl_ensure_compatible_node(tmp1, res);
				ESSL_CHECK(tmp1 = maligp2_preschedule_single_node(ctx, tmp1));
				ESSL_CHECK(tmp2 = _essl_new_binary_expression(ctx->pool, b, EXPR_OP_LT, a));
				_essl_ensure_compatible_node(tmp2, res);
				ESSL_CHECK(tmp2 = maligp2_preschedule_single_node(ctx, tmp2));

				ESSL_CHECK(tmp3 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, tmp1, tmp2, 0));
				_essl_ensure_compatible_node(tmp3, res);
				return maligp2_preschedule_single_node(ctx, tmp3);
			}
		}
	}
	else if(op == EXPR_OP_FUN_LESSTHAN)
	{
		node *r;
		assert(GET_NODE_VEC_SIZE(res) == 1);
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_LT, b));
		_essl_ensure_compatible_node(r, res);
		return maligp2_preschedule_single_node(ctx, r);
	}
	else if(op == EXPR_OP_FUN_LESSTHANEQUAL)
	{
		node *r;
		assert(GET_NODE_VEC_SIZE(res) == 1);
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_LE, b));
		_essl_ensure_compatible_node(r, res);
		return maligp2_preschedule_single_node(ctx, r);
	}
	else if(op == EXPR_OP_FUN_EQUAL)
	{
		node *r;
		assert(GET_NODE_VEC_SIZE(res) == 1);
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_EQ, b));
		_essl_ensure_compatible_node(r, res);
		return maligp2_preschedule_single_node(ctx, r);
	}
	else if(op == EXPR_OP_FUN_NOTEQUAL)
	{
		node *r;
		assert(GET_NODE_VEC_SIZE(res) == 1);
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_NE, b));
		_essl_ensure_compatible_node(r, res);
		return maligp2_preschedule_single_node(ctx, r);
	}
	else if(op == EXPR_OP_FUN_GREATERTHANEQUAL)
	{
		node *r;
		assert(GET_NODE_VEC_SIZE(res) == 1);
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_GE, b));
		_essl_ensure_compatible_node(r, res);
		return maligp2_preschedule_single_node(ctx, r);
	}
	else if(op == EXPR_OP_FUN_GREATERTHAN)
	{
		node *r;
		assert(GET_NODE_VEC_SIZE(res) == 1);
		ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_GT, b));
		_essl_ensure_compatible_node(r, res);
		return maligp2_preschedule_single_node(ctx, r);
	}
	else if(GET_VEC_SIZE(res->hdr.type) > 1)
	{
		if(res->hdr.kind == EXPR_KIND_BINARY)
		{
			return handle_vector_binary_expression(ctx, res);
		}
		else
		{
			return handle_vector_builtin_function(ctx, res);
		}
	}
	else
	{
		return res;
	}
}

static node * handle_logical_xor(maligp2_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	node *r;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	assert(GET_NODE_VEC_SIZE(res) == 1);
	ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_NE, b));
	_essl_ensure_compatible_node(r, res);
	return maligp2_preschedule_single_node(ctx, r);
}

static node * handle_vector_binary_expression(maligp2_preschedule_context * ctx, node *res)
{
	/* split into separate operations + constructor */
	node *newres, *tmp, *left, *right;
	unsigned i, n = GET_NODE_VEC_SIZE(res);
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool, n));
	_essl_ensure_compatible_node(newres, res);
	for(i = 0; i < n; ++i)
	{
		left = a;
		if(GET_NODE_VEC_SIZE(a) > 1)
		{
			ESSL_CHECK(left = create_scalar_swizzle(ctx, a, i));
			ESSL_CHECK(left = maligp2_preschedule_single_node(ctx, left));
		}
		right = b;
		if(GET_NODE_VEC_SIZE(b) > 1)
		{
			ESSL_CHECK(right = create_scalar_swizzle(ctx, b, i));
			ESSL_CHECK(right = maligp2_preschedule_single_node(ctx, right));
		}
		ESSL_CHECK(tmp = _essl_new_binary_expression(ctx->pool, left, res->expr.operation, right));
		_essl_ensure_compatible_node(tmp, left);
		ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
		SET_CHILD(newres, i, tmp);
	}
	return maligp2_preschedule_single_node(ctx, newres);
}

static node * handle_mul(maligp2_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	node *c = NULL;
	node *nc;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));

	if(a->hdr.kind == EXPR_KIND_CONSTANT)
	{
		c = a;
		nc = b;
	}
	else if(b->hdr.kind == EXPR_KIND_CONSTANT)
	{
		c = b;
		nc = a;
	}
	else
	{
		return res;
	}

	assert(c != NULL);
	if(_essl_is_node_all_value(ctx->desc, c, 1.0))
	{
		node *nres = nc;
		if(_essl_get_type_size(nc->hdr.type) < _essl_get_type_size(res->hdr.type))
		{
			ESSL_CHECK(nres = extend_with_swizzle(ctx, nc, res));
		}
		return nres;
	}
	else if(_essl_is_node_all_value(ctx->desc, c, -1.0))
	{
		node *nres;
		ESSL_CHECK(nres = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, nc));
		_essl_ensure_compatible_node(nres, res);
		ESSL_CHECK(nres = maligp2_preschedule_single_node(ctx, nres));
		if(_essl_get_type_size(nc->hdr.type) < _essl_get_type_size(res->hdr.type))
		{
			ESSL_CHECK(nres = extend_with_swizzle(ctx, nc, res));
		}
		return nres;
	}
	else
	{
		return res;
	}
}

static node * handle_csel(maligp2_preschedule_context * ctx, node *res)
{
	if(GET_VEC_SIZE(res->hdr.type) > 1)
	{
		node *newres, *tmp, *single_cond, *left, *right;
		node *cond;
		node *a;
		node *b;
		unsigned i, n = GET_NODE_VEC_SIZE(res);
		ESSL_CHECK(cond = GET_CHILD(res, 0));
		ESSL_CHECK(a = GET_CHILD(res, 1));
		ESSL_CHECK(b = GET_CHILD(res, 2));
		ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool, n));
		_essl_ensure_compatible_node(newres, res);
		for(i = 0; i < n; ++i)
		{
			single_cond = cond;
			if(GET_NODE_VEC_SIZE(cond) > 1)
			{
				ESSL_CHECK(single_cond = create_scalar_swizzle(ctx, cond, i));
				ESSL_CHECK(single_cond = maligp2_preschedule_single_node(ctx, single_cond));
			}
			left = a;
			if(GET_NODE_VEC_SIZE(a) > 1)
			{
				ESSL_CHECK(left = create_scalar_swizzle(ctx, a, i));
				ESSL_CHECK(left = maligp2_preschedule_single_node(ctx, left));
			}
			right = b;
			if(GET_NODE_VEC_SIZE(b) > 1)
			{
				ESSL_CHECK(right = create_scalar_swizzle(ctx, b, i));
				ESSL_CHECK(right = maligp2_preschedule_single_node(ctx, right));
			}
			ESSL_CHECK(tmp = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, single_cond, left, right));
			_essl_ensure_compatible_node(tmp, left);
			ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
			SET_CHILD(newres, i, tmp);
		}
		return maligp2_preschedule_single_node(ctx, newres);
	}
	else
	{
		return res;
	}
}

static node * handle_load(maligp2_preschedule_context * ctx, node *res)
{
	node *address;
	node_extra *ne;
	node *newaddr = NULL;
	node *newres;
	unsigned i, n = _essl_get_type_size(res->hdr.type);
	ESSL_CHECK(address = GET_CHILD(res, 0));
	ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool, n));
	_essl_ensure_compatible_node(newres, res);
	ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, res));
	ne->address_multiplier = MALIGP2_NATIVE_VECTOR_SIZE;
	assert(res->hdr.type->basic_type != TYPE_MATRIX_OF);
	ESSL_CHECK(process_address(ctx, res, ne, address, &newaddr));
	SET_CHILD(res, 0, newaddr);

	if(_essl_get_type_size(res->hdr.type) > 1)
	{
		ESSL_CHECK(res->hdr.type = get_scalar_type(ctx, res->hdr.type));
		for(i = 0; i < n; ++i)
		{
			node *load = NULL;
			{
				node_extra *load_extra;
				ESSL_CHECK(load = clone_load_expression(ctx, res));
				ESSL_CHECK(load_extra = CREATE_EXTRA_INFO(ctx->pool, load));
				*load_extra = *ne;
				load_extra->address_offset += i;
			}
		
			SET_CHILD(newres, i, load);

		}
        ESSL_CHECK(kill_load_expression(ctx, res));
		return maligp2_preschedule_single_node(ctx, newres);
	} 
	else 
	{
		return res;
	}
}

static essl_bool is_output_store(node *address)
{
	node *var_ref = address;
	while(var_ref && var_ref->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE)
	{
		var_ref = GET_CHILD(var_ref, 0);
	}
	assert(var_ref != 0);
	return var_ref->expr.u.sym->address_space == ADDRESS_SPACE_VERTEX_VARYING;
}

static node * handle_store(maligp2_preschedule_context * ctx, node *res)
{
	node *address;
	node *value;
	address = GET_CHILD(res, 0);
	value = GET_CHILD(res, 1);
	if(value != NULL 
		&& GET_NODE_VEC_SIZE(value) > 1 
		&& value->hdr.kind != EXPR_KIND_MALIGP2_STORE_CONSTRUCTOR)
	{
		/* split into swizzles + constructor */
		node *newres, *swz;
		unsigned i, n = GET_NODE_VEC_SIZE(value);
		ESSL_CHECK(newres = create_maligp2_store_constructor_expression(ctx->pool, n));
		_essl_ensure_compatible_node(newres, value);
		for(i = 0; i < n; ++i)
		{
			ESSL_CHECK(swz = create_scalar_swizzle(ctx, value, i));
			ESSL_CHECK(swz = maligp2_preschedule_single_node(ctx, swz));
			SET_CHILD(newres, i, swz);
		}
		ESSL_CHECK(newres = maligp2_preschedule_single_node(ctx, newres));
		SET_CHILD(res, 1, newres);
		return maligp2_preschedule_single_node(ctx, res);
	}
	else if(address != NULL)
	{
		if(is_output_store(address))
		{
			node_extra *ne;
			node *newaddr = NULL;
			ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, res));
			ne->address_multiplier = MALIGP2_NATIVE_VECTOR_SIZE;
			assert(res->hdr.type->basic_type != TYPE_MATRIX_OF);
			ESSL_CHECK(process_address(ctx, res, ne, address, &newaddr));
			assert(newaddr == NULL);
			SET_CHILD(res, 0, NULL);
			return res;
		}
		else
		{
			node *var_ref = address;
			node_extra *ne;
			node_extra *ne_store;
			node *newaddr = NULL;
			node *add;
			while(var_ref && var_ref->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE)
			{
				var_ref = GET_CHILD(var_ref, 0);
			}
			assert(var_ref != NULL);
			ESSL_CHECK(var_ref = _essl_clone_node(ctx->pool, var_ref));

			ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, var_ref));
			ne->address_multiplier = MALIGP2_NATIVE_VECTOR_SIZE;

			assert(res->hdr.type->basic_type != TYPE_MATRIX_OF);

			ESSL_CHECK(process_address(ctx, res, ne, address, &newaddr));
			if(newaddr)
			{
				ESSL_CHECK(add = _essl_new_binary_expression(ctx->pool, var_ref, EXPR_OP_ADD, newaddr));
				_essl_ensure_compatible_node(add, newaddr);
				address = add;
			}
			else 
			{
				address = var_ref;
			}
			SET_CHILD(res, 0, address);
			ESSL_CHECK(ne_store = CREATE_EXTRA_INFO(ctx->pool, res));
			ne_store->address_symbols = ne->address_symbols;
			ne_store->address_multiplier = ne->address_multiplier;
			ne_store->address_offset = ne->address_offset;
			return res;
		}
	}
	else
	{
		return res;
	}
}

static node * handle_builtin_constructor(maligp2_preschedule_context * ctx, node *res)
{
	if(_essl_get_type_size(res->hdr.type) > GET_N_CHILDREN(res))
	{
		node *newres;
		unsigned in_idx = 0, in_n = GET_N_CHILDREN(res);
		unsigned out_idx = 0, out_n = _essl_get_type_size(res->hdr.type);
		assert(res->hdr.type->basic_type != TYPE_MATRIX_OF);
		ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool, out_n));
		_essl_ensure_compatible_node(newres, res);
		for(in_idx = 0; in_idx < in_n; ++in_idx)
		{
			node *child;
			ESSL_CHECK(child = GET_CHILD(res, in_idx));

			if(_essl_get_type_size(child->hdr.type) == 1)
			{
				SET_CHILD(newres, out_idx++, child);
			} 
			else 
			{
				unsigned j;
				for(j = 0; j < _essl_get_type_size(child->hdr.type); ++j)
				{
					node *tmp;
					ESSL_CHECK(tmp = create_scalar_swizzle(ctx, child, j));
					ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
					SET_CHILD(newres, out_idx++, tmp);
				}
			}
		}
		assert(out_idx == out_n);
		return maligp2_preschedule_single_node(ctx, newres);
	}
	else
	{
		return res;
	}
}

static node * handle_type_convert(maligp2_preschedule_context * ctx, node *res)
{
	node *arg;	
	enum { CONV_NONE, CONV_BOOL, CONV_INT } conversion_needed = CONV_NONE;
	type_basic res_type;
	type_basic arg_type;
	node *conv;
	node *constant;
	ESSL_CHECK(arg = GET_CHILD(res, 0));
	res_type = res->hdr.type->basic_type;
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
			return maligp2_preschedule_single_node(ctx, conv);

		case CONV_INT:
			ESSL_CHECK(conv = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_TRUNC, arg, 0, 0));
			_essl_ensure_compatible_node(conv, res);
			return maligp2_preschedule_single_node(ctx, conv);

		default:
			assert(conversion_needed == CONV_NONE);
			return arg;
	}
}

static node * handle_length(maligp2_preschedule_context * ctx, node *res)
{
	node *r;
	node *dot;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(dot = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, a, a, 0));
	_essl_ensure_compatible_node(dot, res);
	ESSL_CHECK(dot = maligp2_preschedule_single_node(ctx, dot));

	ESSL_CHECK(r = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SQRT, dot, NULL, NULL));
	_essl_ensure_compatible_node(r, res);
	return maligp2_preschedule_single_node(ctx, r);
}

static node * handle_normalize(maligp2_preschedule_context * ctx, node *res)
{
	node *dot, *rsq, *mul;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(dot = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, a, a, 0));
	_essl_ensure_compatible_node(dot, res);
	ESSL_CHECK(dot->hdr.type = get_scalar_type(ctx, res->hdr.type));
	ESSL_CHECK(dot = maligp2_preschedule_single_node(ctx, dot));
	
	ESSL_CHECK(rsq = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_INVERSESQRT, dot, 0, 0));
	_essl_ensure_compatible_node(rsq, dot);
	ESSL_CHECK(rsq = maligp2_preschedule_single_node(ctx, rsq));
	
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, rsq));
	_essl_ensure_compatible_node(mul, res);
	return maligp2_preschedule_single_node(ctx, mul);
}

static node * handle_dot(maligp2_preschedule_context * ctx, node *res)
{
	node *products[4];
	node *xy_part = NULL, *zw_part = NULL, *newres = NULL;
	unsigned i, n;
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	n = GET_NODE_VEC_SIZE(a);
	for(i = 0; i < n; ++i)
	{
		node *aswz, *bswz, *mul;
		ESSL_CHECK(aswz = create_scalar_swizzle(ctx, a, i));
		ESSL_CHECK(aswz = maligp2_preschedule_single_node(ctx, aswz));

		ESSL_CHECK(bswz = create_scalar_swizzle(ctx, b, i));
		ESSL_CHECK(bswz = maligp2_preschedule_single_node(ctx, bswz));
		
		ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, aswz, EXPR_OP_MUL, bswz));
		_essl_ensure_compatible_node(mul, res);
		ESSL_CHECK(mul = maligp2_preschedule_single_node(ctx, mul));
		products[i] = mul;
	}
	if(n == 1) 
	{
		return products[0];
	}
	else
	{
		ESSL_CHECK(xy_part = _essl_new_binary_expression(ctx->pool, products[0], EXPR_OP_ADD, products[1]));
		_essl_ensure_compatible_node(xy_part, res);
		ESSL_CHECK(xy_part = maligp2_preschedule_single_node(ctx, xy_part));
	}

	if(n == 2)
	{
		return xy_part;
	}
	else if(n == 3)
	{
		zw_part = products[2];
	} 
	else 
	{
		ESSL_CHECK(zw_part = _essl_new_binary_expression(ctx->pool, products[2], EXPR_OP_ADD, products[3]));
		_essl_ensure_compatible_node(zw_part, res);
		ESSL_CHECK(zw_part = maligp2_preschedule_single_node(ctx, zw_part));	
	}
	ESSL_CHECK(newres = _essl_new_binary_expression(ctx->pool, xy_part, EXPR_OP_ADD, zw_part));
	_essl_ensure_compatible_node(newres, res);
	return maligp2_preschedule_single_node(ctx, newres);
}

static node * handle_vector_builtin_function(maligp2_preschedule_context * ctx, node *res)
{
	if(GET_N_CHILDREN(res) == 1)
	{
		node *a;
		node *newres, *tmp, *swz;
		unsigned i, n = GET_NODE_VEC_SIZE(res);
		ESSL_CHECK(a = GET_CHILD(res, 0));
		/* split into separate operations + constructor */
		ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool, n));
		_essl_ensure_compatible_node(newres, res);
		for(i = 0; i < n; ++i)
		{
			ESSL_CHECK(swz = create_scalar_swizzle(ctx, a, i));
			ESSL_CHECK(swz = maligp2_preschedule_single_node(ctx, swz));

			ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, res->expr.operation, swz, 0, 0));
			_essl_ensure_compatible_node(tmp, swz);
			ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
			SET_CHILD(newres, i, tmp);
		}
		return maligp2_preschedule_single_node(ctx, newres);
	}
	else if(GET_N_CHILDREN(res) == 2)
	{
		node *newres, *tmp, *left, *right;
		unsigned i, n = GET_NODE_VEC_SIZE(res);
		node *a;
		node *b;
		/* split into separate operations + constructor */
		ESSL_CHECK(a = GET_CHILD(res, 0));
		ESSL_CHECK(b = GET_CHILD(res, 1));
		ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool, n));
		_essl_ensure_compatible_node(newres, res);
		for(i = 0; i < n; ++i)
		{
			left = a;
			if(GET_NODE_VEC_SIZE(a) > 1)
			{
				ESSL_CHECK(left = create_scalar_swizzle(ctx, a, i));
				ESSL_CHECK(left = maligp2_preschedule_single_node(ctx, left));
			}
			right = b;
			if(GET_NODE_VEC_SIZE(b) > 1)
			{
				ESSL_CHECK(right = create_scalar_swizzle(ctx, b, i));
				ESSL_CHECK(right = maligp2_preschedule_single_node(ctx, right));
			}
			assert(GET_NODE_VEC_SIZE(left) == 1);
			assert(GET_NODE_VEC_SIZE(right) == 1);

			ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, res->expr.operation, left, right, 0));
			_essl_ensure_compatible_node(tmp, left);
			assert(GET_NODE_VEC_SIZE(tmp) == 1);
			ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
			SET_CHILD(newres, i, tmp);
		}
		return maligp2_preschedule_single_node(ctx, newres);
	}
	else
	{
		node *newres, *tmp, *a_extract, *b_extract, *c_extract;
		unsigned i, n = GET_NODE_VEC_SIZE(res);
		node *a;
		node *b;
		node *c;
		assert(GET_N_CHILDREN(res) == 3);
		ESSL_CHECK(a = GET_CHILD(res, 0));
		ESSL_CHECK(b = GET_CHILD(res, 1));
		ESSL_CHECK(c = GET_CHILD(res, 2));
		/* split into separate operations + constructor */
		ESSL_CHECK(newres = _essl_new_builtin_constructor_expression(ctx->pool, n));
		_essl_ensure_compatible_node(newres, res);
		for(i = 0; i < n; ++i)
		{
			a_extract = a;
			if(GET_NODE_VEC_SIZE(a) > 1)
			{
				ESSL_CHECK(a_extract = create_scalar_swizzle(ctx, a, i));
				ESSL_CHECK(a_extract = maligp2_preschedule_single_node(ctx, a_extract));
			}
			b_extract = b;
			if(GET_NODE_VEC_SIZE(b) > 1)
			{
				ESSL_CHECK(b_extract = create_scalar_swizzle(ctx, b, i));
				ESSL_CHECK(b_extract = maligp2_preschedule_single_node(ctx, b_extract));
			}
			c_extract = c;
			if(GET_NODE_VEC_SIZE(c) > 1)
			{
				ESSL_CHECK(c_extract = create_scalar_swizzle(ctx, c, i));
				ESSL_CHECK(c_extract = maligp2_preschedule_single_node(ctx, c_extract));
			}

			assert(GET_NODE_VEC_SIZE(a_extract) == 1);
			assert(GET_NODE_VEC_SIZE(b_extract) == 1);
			assert(GET_NODE_VEC_SIZE(c_extract) == 1);
			ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, res->expr.operation, a_extract, b_extract, c_extract));
			_essl_ensure_compatible_node(tmp, a_extract);
			assert(GET_NODE_VEC_SIZE(tmp) == 1);
			ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
			SET_CHILD(newres, i, tmp);
		}
		return maligp2_preschedule_single_node(ctx, newres);
	}
}

static node * handle_abs(maligp2_preschedule_context * ctx, node *res)
{
	node *negate, *max;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	assert(GET_NODE_VEC_SIZE(res) == 1);
	ESSL_CHECK(negate = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, a));
	_essl_ensure_compatible_node(negate, res);
	ESSL_CHECK(negate = maligp2_preschedule_single_node(ctx, negate));

	ESSL_CHECK(max = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, a, negate, 0));
	_essl_ensure_compatible_node(max, res);
	return maligp2_preschedule_single_node(ctx, max);
}

static node * handle_clamp(maligp2_preschedule_context * ctx, node *res)
{
	node *lolim;
	node *hilim;
	
	ESSL_CHECK(lolim = GET_CHILD(res, 1));
	ESSL_CHECK(hilim = GET_CHILD(res, 2));
	if(lolim->hdr.kind != EXPR_KIND_CONSTANT 
		|| hilim->hdr.kind != EXPR_KIND_CONSTANT)
	{
		node *max, *min;
		node *a;
		ESSL_CHECK(a = GET_CHILD(res, 0));
		ESSL_CHECK(min = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MIN, a, hilim, 0));
		_essl_ensure_compatible_node(min, res);
		ESSL_CHECK(min = maligp2_preschedule_single_node(ctx, min));

		ESSL_CHECK(max = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, lolim, min, 0));
		_essl_ensure_compatible_node(max, res);
		return maligp2_preschedule_single_node(ctx, max);
	}
	else if(GET_VEC_SIZE(res->hdr.type) > 1)
	{
		return handle_vector_builtin_function(ctx, res);
	}
	else
	{
		return res;
	}
}

static node * handle_all_any(maligp2_preschedule_context * ctx, node *res)
{
	node *a;
	unsigned n;
	expression_operator op = res->expr.operation == EXPR_OP_FUN_ALL ? EXPR_OP_FUN_MIN : EXPR_OP_FUN_MAX;
	node *xy_part = NULL, *zw_part = NULL, *newres = NULL;
	node *x, *y;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	n = GET_NODE_VEC_SIZE(a);
	ESSL_CHECK(x = create_scalar_swizzle(ctx, a, 0));
	ESSL_CHECK(x = maligp2_preschedule_single_node(ctx, x));

	ESSL_CHECK(y = create_scalar_swizzle(ctx, a, 1));
	ESSL_CHECK(y = maligp2_preschedule_single_node(ctx, y));

	ESSL_CHECK(xy_part = _essl_new_builtin_function_call_expression(ctx->pool, op, x, y, 0));
	_essl_ensure_compatible_node(xy_part, res);
	ESSL_CHECK(xy_part = maligp2_preschedule_single_node(ctx, xy_part));

	if(n == 2)
	{
		return xy_part;
	}
	else if(n == 3)
	{
		ESSL_CHECK(zw_part = create_scalar_swizzle(ctx, a, 2));
		ESSL_CHECK(zw_part = maligp2_preschedule_single_node(ctx, zw_part));
	}
	else 
	{
		node *z, *w;
		ESSL_CHECK(z = create_scalar_swizzle(ctx, a, 2));
		ESSL_CHECK(z = maligp2_preschedule_single_node(ctx, z));

		ESSL_CHECK(w = create_scalar_swizzle(ctx, a, 3));
		ESSL_CHECK(w = maligp2_preschedule_single_node(ctx, w));

		ESSL_CHECK(zw_part = _essl_new_builtin_function_call_expression(ctx->pool, op, z, w, 0));
		_essl_ensure_compatible_node(zw_part, res);
		ESSL_CHECK(zw_part = maligp2_preschedule_single_node(ctx, zw_part));
	}

	ESSL_CHECK(newres = _essl_new_builtin_function_call_expression(ctx->pool, op, xy_part, zw_part, 0));
	_essl_ensure_compatible_node(newres, res);
	return maligp2_preschedule_single_node(ctx, newres);
}

static essl_bool is_suitable_pow_exp(maligp2_preschedule_context * ctx, const node *b)
{
	float v;
	int iv;
	if (GET_NODE_VEC_SIZE(b) > 1) 
	{
		return ESSL_FALSE;
	}
	v = ctx->desc->scalar_to_float(b->expr.u.value[0]);
	iv = (int)v;
	return (v == (float)iv) && iv >= 0 && iv < 30;  /* 30 is the first with eight multiplies */

}

static node * handle_pow(maligp2_preschedule_context * ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	if(b->hdr.kind == EXPR_KIND_CONSTANT
		&& is_suitable_pow_exp(ctx, b))
	{
		float v;
		int iv;
		v = ctx->desc->scalar_to_float(b->expr.u.value[0]);
		iv = (int)v;
		return integer_pow(ctx, a, iv, res);
	}
	else
	{
		node *input0 = a;
		node *input1 = b;
		node *log2, *mul, *exp2;
		ESSL_CHECK(log2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_LOG2, input0, 0, 0));
		_essl_ensure_compatible_node(log2, res);
		ESSL_CHECK(log2 = maligp2_preschedule_single_node(ctx, log2));

		ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, log2, EXPR_OP_MUL, input1));
		_essl_ensure_compatible_node(mul, res);
		ESSL_CHECK(mul = maligp2_preschedule_single_node(ctx, mul));

		ESSL_CHECK(exp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_EXP2, mul, 0, 0));
		_essl_ensure_compatible_node(exp2, res);
		return maligp2_preschedule_single_node(ctx, exp2);
	}
}

static node * handle_exp2(maligp2_preschedule_context * ctx, node *res)
{
	node *fixed;
	node *exp2_result;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(fixed = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MALIGP2_FLOAT_TO_FIXED, a, 0, 0));
	_essl_ensure_compatible_node(fixed, a);

	ESSL_CHECK(exp2_result = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MALIGP2_FIXED_EXP2, fixed, NULL, NULL));
	_essl_ensure_compatible_node(exp2_result, res);
	ESSL_CHECK(exp2_result = maligp2_preschedule_single_node(ctx, exp2_result));

	if(ctx->opts->maligp2_exp2_workaround)
	{
		/* Work around hardware bugzilla 3607. If the input was -1.0, return 0.5 instead of relying on the buggy mul-add */
		node *csel;
		node *one;
		node *half;
		node *is_not_minus_one;
		ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));
		ESSL_CHECK(half = create_float_constant(ctx, 0.5, 1));
		ESSL_CHECK(is_not_minus_one = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_ADD, one));
		_essl_ensure_compatible_node(is_not_minus_one, res);
		ESSL_CHECK(is_not_minus_one = maligp2_preschedule_single_node(ctx, is_not_minus_one));
		ESSL_CHECK(csel = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, is_not_minus_one, exp2_result, half));
		_essl_ensure_compatible_node(csel, res);
		ESSL_CHECK(csel = maligp2_preschedule_single_node(ctx, csel));
		return csel;
	} 
	else 
	{
		return exp2_result;
	}
}

static node * handle_log2(maligp2_preschedule_context * ctx, node *res)
{
	node *fixed;
	node *result;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(fixed = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MALIGP2_LOG2_FIXED, a, 0, 0));
	_essl_ensure_compatible_node(fixed, res);
	ESSL_CHECK(result = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MALIGP2_FIXED_TO_FLOAT, fixed, 0, 0));
	_essl_ensure_compatible_node(result, fixed);
	return maligp2_preschedule_single_node(ctx, result);
}

static node * handle_sqrt(maligp2_preschedule_context * ctx, node *res)
{
	/* sqrt -> 1/inverseSqrt(a) */
	node *invsqrt;
	node *one;
	node *div;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(invsqrt = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_INVERSESQRT, a, 0, 0));
	_essl_ensure_compatible_node(invsqrt, res);
	ESSL_CHECK(invsqrt = maligp2_preschedule_single_node(ctx, invsqrt));
	
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));
	ESSL_CHECK(div = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_DIV, invsqrt));
	_essl_ensure_compatible_node(div, res);
	return maligp2_preschedule_single_node(ctx, div);
}

static node * handle_ceil(maligp2_preschedule_context * ctx, node *res)
{
	/* ceil(a) -> -floor(-a) */
	node *neg1;
	node *neg2;
	node *floor;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(neg1 = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, a));
	_essl_ensure_compatible_node(neg1, res);
	ESSL_CHECK(neg1 = maligp2_preschedule_single_node(ctx, neg1));

	ESSL_CHECK(floor = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_FLOOR, neg1, 0, 0));
	_essl_ensure_compatible_node(floor, res);
	ESSL_CHECK(floor = maligp2_preschedule_single_node(ctx, floor));

	ESSL_CHECK(neg2 = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, floor));
	_essl_ensure_compatible_node(neg2, res);
	return maligp2_preschedule_single_node(ctx, neg2);
}

static node * handle_fract(maligp2_preschedule_context * ctx, node *res)
{
	node *floor;
	node *sub;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));

	ESSL_CHECK(floor = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_FLOOR, a, 0, 0));
	_essl_ensure_compatible_node(floor, res);
	ESSL_CHECK(floor = maligp2_preschedule_single_node(ctx, floor));

	ESSL_CHECK(sub = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_SUB, floor));
	_essl_ensure_compatible_node(sub, res);
	return maligp2_preschedule_single_node(ctx, sub);
}

static node * handle_cos(maligp2_preschedule_context * ctx, node *res)
{
	node *pi_half;
	node *add;
	node *r;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(pi_half = create_float_constant(ctx, (float)M_PI/2.0f, 1));
	ESSL_CHECK(add = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_ADD, pi_half));
	_essl_ensure_compatible_node(add, res);
	ESSL_CHECK(add = maligp2_preschedule_single_node(ctx, add));

	ESSL_CHECK(r = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SIN, add, NULL, NULL));
	_essl_ensure_compatible_node(r, res);
	return maligp2_preschedule_single_node(ctx, r);
}

static node * handle_mod(maligp2_preschedule_context * ctx, node *res)
{
	node *div, *fract, *mul;
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	ESSL_CHECK(div = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_DIV, b));
	_essl_ensure_compatible_node(div, res);
	ESSL_CHECK(div = maligp2_preschedule_single_node(ctx, div));

	ESSL_CHECK(fract = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_FRACT, div, 0, 0));
	_essl_ensure_compatible_node(fract, res);
	ESSL_CHECK(fract = maligp2_preschedule_single_node(ctx, fract));
	
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, b, EXPR_OP_MUL, fract));
	_essl_ensure_compatible_node(mul, res);
	return maligp2_preschedule_single_node(ctx, mul);
}

static node * handle_sin(maligp2_preschedule_context * ctx, node *res)
{
	/*

	   scale = 0.999726;
	   acc = 0.0;

	   coef1 = pi^1/factorial(1);

	   coef3 = -pi^3/factorial(3) * scale;

	   coef5 = pi^5/factorial(5) * scale;

	   coef7 = -pi^7/factorial(7) * scale;


	   divpi = v/pi;
	   div2pi = v/2.0/pi;
	   div2pi_mod = div2pi - floor(div2pi);
	   div2pi_new = div2pi_mod - 0.25;
	   half = div2pi_new - floor(div2pi_new) - 0.5;
	   x = (divpi - floor(divpi + 0.5)); % x in range -0.5..0.5

	   xsq = x^2;

	   x_acc = x;
	   acc = coef1*x_acc;
	   x_acc = x_acc*xsq;
	   acc = acc + coef3*x_acc;
	   x_acc = x_acc*xsq;
	   acc = acc + coef5*x_acc;
	   x_acc = x_acc*xsq;
	   acc = acc + coef7*x_acc;


	res = acc * sign(half);
	*/
	float coefs[4] = {3.1415926535897931f, -5.1662968267482361f, 2.5494652949304188f, -0.599100330839758f};
	int i;
	node *constant;

	node *x;
	node *xsq;
	node *tmp;
	node *tmp2;
	node *divpi;
	node *offset_divpi;
	node *div2pi;
	node *div2pi_mod;
	node *div2pi_floor;
	node *div2pi_new;
	node *half;
	node *x_acc;
	node *cond;
	node *neg_res;
	node *acc = NULL;
	node *v;
	ESSL_CHECK(v = GET_CHILD(res, 0));

	/* divpi = v/pi;
	   offset_divpi = divpi + 0.5; */
	ESSL_CHECK(constant = create_float_constant(ctx, 1.0f/(float)M_PI, 1));
	ESSL_CHECK(divpi = _essl_new_binary_expression(ctx->pool, v, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(divpi, res);
	ESSL_CHECK(divpi = maligp2_preschedule_single_node(ctx, divpi));
	ESSL_CHECK(constant = create_float_constant(ctx, 0.5, 1));
	ESSL_CHECK(offset_divpi = _essl_new_binary_expression(ctx->pool, divpi, EXPR_OP_ADD, constant));
	_essl_ensure_compatible_node(offset_divpi, res);
	ESSL_CHECK(offset_divpi = maligp2_preschedule_single_node(ctx, offset_divpi));
	
	/* div2pi = v/2.0/pi; */
	ESSL_CHECK(constant = create_float_constant(ctx, 1.0f/(float)M_PI/2.0f, 1));
	ESSL_CHECK(div2pi = _essl_new_binary_expression(ctx->pool, v, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(div2pi, res);
	ESSL_CHECK(div2pi = maligp2_preschedule_single_node(ctx, div2pi));

	/* floor(div2pi); */
	ESSL_CHECK(div2pi_floor = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_FLOOR, div2pi, NULL, NULL));
	_essl_ensure_compatible_node(div2pi_floor, res);
	ESSL_CHECK(div2pi_floor = maligp2_preschedule_single_node(ctx, div2pi_floor));

	/* div2pi_mod = div2pi - floor(div2pi); */
	ESSL_CHECK(div2pi_mod = _essl_new_binary_expression(ctx->pool, div2pi, EXPR_OP_SUB, div2pi_floor));
	_essl_ensure_compatible_node(div2pi_mod, res);
	ESSL_CHECK(div2pi_mod = maligp2_preschedule_single_node(ctx, div2pi_mod));

	/* div2pi_new = div2pi_mod - 0.25; */
	ESSL_CHECK(constant = create_float_constant(ctx, -0.25, 1));
	ESSL_CHECK(div2pi_new = _essl_new_binary_expression(ctx->pool, div2pi_mod, EXPR_OP_ADD, constant));
	_essl_ensure_compatible_node(div2pi_new, res);
	ESSL_CHECK(div2pi_new = maligp2_preschedule_single_node(ctx, div2pi_new));
	
	/* half = (div2pi_new  - floor(div2pi_new)) -0.5; */
	ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_FLOOR, div2pi_new, NULL, NULL));
	_essl_ensure_compatible_node(tmp, res);
	ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));

	ESSL_CHECK(tmp2 = _essl_new_binary_expression(ctx->pool, div2pi_new, EXPR_OP_SUB, tmp));
	_essl_ensure_compatible_node(tmp2, res);
	ESSL_CHECK(tmp2 = maligp2_preschedule_single_node(ctx, tmp2));

	ESSL_CHECK(constant = create_float_constant(ctx, -0.5, 1));

	ESSL_CHECK(half = _essl_new_binary_expression(ctx->pool, tmp2, EXPR_OP_ADD, constant));
	_essl_ensure_compatible_node(half, res);
	ESSL_CHECK(half = maligp2_preschedule_single_node(ctx, half));

	/* x = divpi - floor(offset_divpi) */
	ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_FLOOR, offset_divpi, NULL, NULL));
	_essl_ensure_compatible_node(tmp, res);
	ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));
	

	ESSL_CHECK(x = _essl_new_binary_expression(ctx->pool, divpi, EXPR_OP_SUB, tmp));
	_essl_ensure_compatible_node(x, res);
	ESSL_CHECK(x = maligp2_preschedule_single_node(ctx, x));

	ESSL_CHECK(xsq = _essl_new_binary_expression(ctx->pool, x, EXPR_OP_MUL, x));
	_essl_ensure_compatible_node(xsq, res);
	ESSL_CHECK(xsq = maligp2_preschedule_single_node(ctx, xsq));

	x_acc = x;

	for(i = 0; i < 4; ++i)
	{
		ESSL_CHECK(constant = create_float_constant(ctx, coefs[i], 1));
		ESSL_CHECK(tmp2 = _essl_new_binary_expression(ctx->pool, constant, EXPR_OP_MUL, x_acc));
		_essl_ensure_compatible_node(tmp2, res);
		ESSL_CHECK(tmp2 = maligp2_preschedule_single_node(ctx, tmp2));

		if(i == 0)
		{
			acc = tmp2;
		} 
		else 
		{
			ESSL_CHECK(acc = _essl_new_binary_expression(ctx->pool, tmp2, EXPR_OP_ADD, acc));
			_essl_ensure_compatible_node(acc, res);
			ESSL_CHECK(acc = maligp2_preschedule_single_node(ctx, acc));
		}
		if(i == 3)
		{
			x_acc = NULL;
		} 
		else 
		{
			ESSL_CHECK(x_acc = _essl_new_binary_expression(ctx->pool, x_acc, EXPR_OP_MUL, xsq));
			_essl_ensure_compatible_node(x_acc, res);
			ESSL_CHECK(x_acc = maligp2_preschedule_single_node(ctx, x_acc));
		}

	}
	tmp2 = acc;
	
	/* can't use sign() as sign(0) = 0 */
	ESSL_CHECK(constant = create_float_constant(ctx, 0.0, 1));

	ESSL_CHECK(cond = _essl_new_binary_expression(ctx->pool, half, EXPR_OP_LT, constant));
	_essl_ensure_compatible_node(cond, res);
	ESSL_CHECK(cond = maligp2_preschedule_single_node(ctx, cond));

	ESSL_CHECK(neg_res = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, tmp2));
	_essl_ensure_compatible_node(neg_res, res);
	ESSL_CHECK(neg_res = maligp2_preschedule_single_node(ctx, neg_res));

	ESSL_CHECK(tmp2 = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, cond, neg_res, tmp2));
	_essl_ensure_compatible_node(tmp2, res);
	ESSL_CHECK(tmp2 = maligp2_preschedule_single_node(ctx, tmp2));

	return tmp2;
}

static node * handle_atan(maligp2_preschedule_context * ctx, node *res)
{
	if(GET_N_CHILDREN(res) == 1)
	{
		node *u;
		ESSL_CHECK(u = GET_CHILD(res, 0));
		return create_atan_approximation(ctx, u, NULL, res);
	}
	else
	{
		node *u;
		node *v;
		assert(GET_N_CHILDREN(res) == 2);
		ESSL_CHECK(u = GET_CHILD(res, 0));
		ESSL_CHECK(v = GET_CHILD(res, 1));
		return create_atan_approximation(ctx, u, v, res);
	}
}

/* quick and dirty tan/asin/acos approximations - they just defer to sin/cos/atan */

static node * handle_tan(maligp2_preschedule_context * ctx, node *res)
{
	node *tmp;
	node *tmp2;
	node *tmp3;
	node *x;
	ESSL_CHECK(x = GET_CHILD(res, 0));
	ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SIN, x, 0, 0));
	_essl_ensure_compatible_node(tmp, res);
	ESSL_CHECK(tmp = maligp2_preschedule_single_node(ctx, tmp));

	ESSL_CHECK(tmp2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_COS, x, 0, 0));
	_essl_ensure_compatible_node(tmp2, res);
	ESSL_CHECK(tmp2 = maligp2_preschedule_single_node(ctx, tmp2));


	ESSL_CHECK(tmp3 = _essl_new_binary_expression(ctx->pool, tmp, EXPR_OP_DIV, tmp2));
	_essl_ensure_compatible_node(tmp3, res);
	return maligp2_preschedule_single_node(ctx, tmp3);
}

static node * handle_asin(maligp2_preschedule_context * ctx, node *res)
{
	/* atan2(x, sqrt(1 - x**2)) */
	node *mul, *one, *sub, *sqrt, *atan2;
	node *x;
	ESSL_CHECK(x = GET_CHILD(res, 0));
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, x, EXPR_OP_MUL, x));
	_essl_ensure_compatible_node(mul, res);
	ESSL_CHECK(mul = maligp2_preschedule_single_node(ctx, mul));
	
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));
	
	ESSL_CHECK(sub = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_SUB, mul));
	_essl_ensure_compatible_node(sub, res);
	ESSL_CHECK(sub = maligp2_preschedule_single_node(ctx, sub));
	
	ESSL_CHECK(sqrt = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SQRT, sub, 0, 0));
	_essl_ensure_compatible_node(sqrt, res);
	ESSL_CHECK(sqrt = maligp2_preschedule_single_node(ctx, sqrt));
	
	ESSL_CHECK(atan2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ATAN, x, sqrt, 0));
	_essl_ensure_compatible_node(atan2, res);
	return maligp2_preschedule_single_node(ctx, atan2);
}

static node * handle_acos(maligp2_preschedule_context * ctx, node *res)
{
	/* atan2(sqrt(1 - x**2), x) */
	node *mul, *one, *sub, *sqrt, *atan2;
	node *x;
	ESSL_CHECK(x = GET_CHILD(res, 0));
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, x, EXPR_OP_MUL, x));
	_essl_ensure_compatible_node(mul, res);
	ESSL_CHECK(mul = maligp2_preschedule_single_node(ctx, mul));
	
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));
	
	ESSL_CHECK(sub = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_SUB, mul));
	_essl_ensure_compatible_node(sub, res);
	ESSL_CHECK(sub = maligp2_preschedule_single_node(ctx, sub));
	
	ESSL_CHECK(sqrt = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SQRT, sub, 0, 0));
	_essl_ensure_compatible_node(sqrt, res);
	ESSL_CHECK(sqrt = maligp2_preschedule_single_node(ctx, sqrt));
	
	ESSL_CHECK(atan2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ATAN, sqrt, x, 0));
	_essl_ensure_compatible_node(atan2, res);
	return maligp2_preschedule_single_node(ctx, atan2);
}

static node * maligp2_preschedule_single_node(maligp2_preschedule_context * ctx, node *n) 
{
	node_kind kind;
	expression_operator op;
	ESSL_CHECK(n);
	kind = n->hdr.kind;
	op = n->expr.operation;

	if(kind == EXPR_KIND_UNARY)
	{
		if(op == EXPR_OP_NOT)
		{
			return handle_not(ctx, n);
		}
		else if(op == EXPR_OP_SWIZZLE)
		{
			return handle_swizzle(ctx, n);
		}
		else if(op == EXPR_OP_MEMBER)
		{
			/* get member away before doing general unary vector dissolve */
			return n;
		}
		else if(GET_VEC_SIZE(n->hdr.type) > 1)
		{
			return handle_unary_vector_expression(ctx, n);
		}
		else
		{
			return n;
		}
	}
	else if(kind == EXPR_KIND_BINARY)
	{
		if(op == EXPR_OP_DIV)
		{
			return handle_div(ctx, n);
		}
		else if(op == EXPR_OP_SUB)
		{
			return handle_sub(ctx, n);
		}
		else if(op == EXPR_OP_SUBVECTOR_ACCESS)
		{
			return handle_subvector_access(ctx, n);
		}
		else if(_essl_is_node_comparison(n))
		{
			return handle_comparison(ctx, n);
		}
		else if(op == EXPR_OP_LOGICAL_XOR)
		{
			return handle_logical_xor(ctx, n);
		}
		else if(op == EXPR_OP_INDEX)
		{
			return n;
		}
		else if(GET_VEC_SIZE(n->hdr.type) > 1)
		{
			return handle_vector_binary_expression(ctx, n);
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
		if(op == EXPR_OP_SUBVECTOR_UPDATE)
		{
			return handle_subvector_update(ctx, n);
		}
		else if(op == EXPR_OP_CONDITIONAL_SELECT)
		{
			return handle_csel(ctx, n);
		}
		else
		{
			return n;
		}
	}
	else if(kind == EXPR_KIND_BUILTIN_FUNCTION_CALL)
	{
		if(op == EXPR_OP_FUN_RCP)
		{
			return handle_rcp(ctx, n);
		}
		else if(op == EXPR_OP_FUN_TRUNC)
		{
			return handle_trunc(ctx, n);
		}
		else if(op == EXPR_OP_FUN_LENGTH)
		{
			return handle_length(ctx, n);
		}
		else if(op == EXPR_OP_FUN_NORMALIZE)
		{
			return handle_normalize(ctx, n);
		}
		else if(op == EXPR_OP_FUN_DOT)
		{
			return handle_dot(ctx, n);
		}
		else if(op == EXPR_OP_FUN_CLAMP)
		{
			return handle_clamp(ctx, n);
		}
		else if(GET_VEC_SIZE(n->hdr.type) > 1)
		{
			return handle_vector_builtin_function(ctx, n);
		}
		else if(_essl_is_node_comparison(n))
		{
			return handle_comparison(ctx, n);
		}
		else if(op == EXPR_OP_FUN_NOT)
		{
			return handle_not(ctx, n);
		}
		else if(op == EXPR_OP_FUN_ABS)
		{
			return handle_abs(ctx, n);
		}
		else if(op == EXPR_OP_FUN_ALL 
				|| op == EXPR_OP_FUN_ANY)
		{
			return handle_all_any(ctx, n);
		}
		else if(op == EXPR_OP_FUN_POW)
		{
			return handle_pow(ctx, n);
		}
		else if(op == EXPR_OP_FUN_EXP2)
		{
			return handle_exp2(ctx, n);
		}
		else if(op == EXPR_OP_FUN_LOG2)
		{
			return handle_log2(ctx, n);
		}
		else if(op == EXPR_OP_FUN_SQRT)
		{
			return handle_sqrt(ctx, n);
		}
		else if(op == EXPR_OP_FUN_CEIL)
		{
			return handle_ceil(ctx, n);
		}
		else if(op == EXPR_OP_FUN_FRACT)
		{
			return handle_fract(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MOD)
		{
			return handle_mod(ctx, n);
		}
		else if(op == EXPR_OP_FUN_COS)
		{
			return handle_cos(ctx, n);
		}
		else if(op == EXPR_OP_FUN_SIN)
		{
			return handle_sin(ctx, n);
		}
		else if(op == EXPR_OP_FUN_ATAN)
		{
			return handle_atan(ctx, n);
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
		else
		{
			return n;
		}
	}
	else if(kind == EXPR_KIND_VECTOR_COMBINE)
	{
		return handle_vector_combine(ctx, n);
	}
	else if(kind == EXPR_KIND_LOAD)
	{
		return handle_load(ctx, n);
	}
	else if(kind == EXPR_KIND_STORE)
	{
		return handle_store(ctx, n);
	}
	else if(kind == EXPR_KIND_BUILTIN_CONSTRUCTOR)
	{
		return handle_builtin_constructor(ctx, n);
	}
	else if(kind == EXPR_KIND_TYPE_CONVERT)
	{
		return handle_type_convert(ctx, n);
	}
	else if(kind == EXPR_KIND_DONT_CARE)
	{
		node *tmp;
		ESSL_CHECK(tmp = create_float_constant(ctx, 0.0, GET_NODE_VEC_SIZE(n)));
		_essl_ensure_compatible_node(tmp, n);
		return maligp2_preschedule_single_node(ctx, tmp);
	}
	else if(kind == EXPR_KIND_TRANSFER)
	{
		return GET_CHILD(n, 0);
	}
	else
	{
		return n;
	}
}

/*@null@*/ static node *process_node_w(maligp2_preschedule_context *ctx, node *n)
{

	node *nn;

	if(_essl_node_is_texture_operation(n))
	{
		return n;
	}
	
	ESSL_CHECK(nn = maligp2_preschedule_single_node(ctx, n));
	assert(nn->hdr.kind == EXPR_KIND_BUILTIN_CONSTRUCTOR
		   || nn->hdr.kind == EXPR_KIND_MALIGP2_STORE_CONSTRUCTOR
		   || nn->hdr.kind == EXPR_KIND_STORE
		   || nn->hdr.kind == EXPR_KIND_CONSTANT
		   || nn->hdr.kind == EXPR_KIND_DONT_CARE
		   || nn->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE
		   || (nn->hdr.kind == EXPR_KIND_UNARY && nn->expr.operation == EXPR_OP_MEMBER )
		   || (nn->hdr.kind == EXPR_KIND_BINARY && nn->expr.operation == EXPR_OP_INDEX)
		   || _essl_get_type_size(nn->hdr.type) == 1 
		   || nn->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL);
	return nn;


}

/*@null@*/ static node *process_node(maligp2_preschedule_context *ctx, node *n)
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


static memerr pre_handle_block(maligp2_preschedule_context *ctx, basic_block *b)
{
	phi_list **phi;
	if(b->termination == TERM_KIND_JUMP && b->source != 0)
	{
		if(b->successors[BLOCK_TRUE_TARGET]->output_visit_number ==  b->output_visit_number + 1)
		{
			essl_bool comparison = ESSL_FALSE;
			expression_operator flipped_op = EXPR_OP_UNKNOWN;
			node *source = b->source;
			node *new_source;
			while (source->hdr.kind == EXPR_KIND_TRANSFER)
			{
				source = GET_CHILD(source, 0);
			}

			/* Swap the targets */
			{
				basic_block *tmp = b->successors[BLOCK_TRUE_TARGET];
				b->successors[BLOCK_TRUE_TARGET] = b->successors[BLOCK_DEFAULT_TARGET];
				b->successors[BLOCK_DEFAULT_TARGET] = tmp;
			}

			/* Is the source a comparison? */
			if (source->hdr.kind == EXPR_KIND_BINARY)
			{
				switch (source->expr.operation)
				{
				case EXPR_OP_LT:
					flipped_op = EXPR_OP_GE;
					comparison = ESSL_TRUE;
					break;
				case EXPR_OP_LE:
					flipped_op = EXPR_OP_GT;
					comparison = ESSL_TRUE;
					break;
				case EXPR_OP_EQ:
					flipped_op = EXPR_OP_NE;
					comparison = ESSL_TRUE;
					break;
				case EXPR_OP_NE:
					flipped_op = EXPR_OP_EQ;
					comparison = ESSL_TRUE;
					break;
				case EXPR_OP_GE:
					flipped_op = EXPR_OP_LT;
					comparison = ESSL_TRUE;
					break;
				case EXPR_OP_GT:
					flipped_op = EXPR_OP_LE;
					comparison = ESSL_TRUE;
					break;
				default:
					break;
				}
			}

			/* Invert the comparison or insert a not */
			if (comparison)
			{
				ESSL_CHECK(new_source = _essl_new_binary_expression(ctx->pool, GET_CHILD(source, 0), flipped_op, GET_CHILD(source, 1)));
			}
			else
			{
				ESSL_CHECK(new_source = _essl_new_unary_expression(ctx->pool, EXPR_OP_NOT, source));
			}
			_essl_ensure_compatible_node(new_source, source);
			b->source = new_source;

		}
		assert(b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number ==  b->output_visit_number + 1);
	}


	for(phi = &b->phi_nodes; (*phi) != 0; /* empty */)
	{		
		unsigned size = _essl_get_type_size((*phi)->phi_node->hdr.type);
		if(size > 1)
		{
			/* split up */
			node *new_phis[16] = {0};
			unsigned i;
			assert(size <= MALIGP2_NATIVE_VECTOR_SIZE);
			for(i = 0; i < size; ++i)
			{
				phi_source *source;
				phi_list *lst;
				ESSL_CHECK(new_phis[i] = _essl_new_phi_expression(ctx->pool, (*phi)->phi_node->expr.u.phi.block));
				_essl_ensure_compatible_node(new_phis[i], (*phi)->phi_node);
				ESSL_CHECK(new_phis[i]->hdr.type = get_scalar_type(ctx, (*phi)->phi_node->hdr.type));
				for(source = (*phi)->phi_node->expr.u.phi.sources; source != 0; source = source->next)
				{
					phi_source* newsrc;
					ESSL_CHECK(newsrc = LIST_NEW(ctx->pool, phi_source));

					newsrc->join_block = source->join_block;
					ESSL_CHECK(newsrc->source = create_scalar_swizzle(ctx, source->source, i));
					LIST_INSERT_FRONT(&new_phis[i]->expr.u.phi.sources, newsrc);
				}		
				ESSL_CHECK(lst = LIST_NEW(ctx->pool, phi_list));
				lst->phi_node = new_phis[i];
				lst->sym = 0;
				assert(lst->phi_node->hdr.type != NULL);
				LIST_INSERT_BACK(&b->phi_nodes, lst);
			}
			ESSL_CHECK(SET_N_CHILDREN((*phi)->phi_node, size, ctx->pool));
			
			for(i = 0; i < size; ++i)
			{
				SET_CHILD((*phi)->phi_node, i, new_phis[i]);
			}
			(*phi)->phi_node->hdr.kind = EXPR_KIND_BUILTIN_CONSTRUCTOR;

			*phi = (*phi)->next; /* delete the node and go to the next */
		} else {
			phi = &((*phi)->next); /* skip to the next node */
		}
	}

	return MEM_OK;
}


static memerr handle_block(maligp2_preschedule_context *ctx, basic_block *b)
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



memerr _essl_maligp2_preschedule(mempool *pool, target_descriptor *desc, typestorage_context *ts_ctx, control_flow_graph *cfg,
								 compiler_options *opts, error_context *error_context)
{

	maligp2_preschedule_context ctx;
	unsigned int i;

	mempool visit_pool;

	ESSL_CHECK(_essl_mempool_init(&visit_pool, 0, _essl_mempool_get_tracker(pool)));
	ctx.pool = pool;
	ctx.cfg = cfg;
	ctx.desc = desc;
	ctx.typestor_ctx = ts_ctx;
	ctx.opts = opts;
	ctx.error_context = error_context;

	if(!_essl_ptrdict_init(&ctx.visited, &visit_pool)) goto error;
	for (i = 0 ; i < cfg->n_blocks ; i++) {
		if(!pre_handle_block(&ctx, cfg->postorder_sequence[i])) goto error;
	}
	for (i = 0 ; i < cfg->n_blocks ; i++) {
		if(!handle_block(&ctx, cfg->postorder_sequence[i])) goto error;
	}
	_essl_mempool_destroy(&visit_pool);
	return MEM_OK;
error:
	_essl_mempool_destroy(&visit_pool);
	return MEM_ERROR;
}
