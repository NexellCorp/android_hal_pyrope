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
#include "middle/expand_builtin_functions.h"
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
	expand_builtin_functions_options flags;
} expand_builtin_functions_context;


static node *create_float_constant(expand_builtin_functions_context *ctx, float value, unsigned vec_size, scalar_size_specifier scalar_size)
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

static node * expand_builtin_functions_single_node(expand_builtin_functions_context * ctx, node *n);


static node *handle_distance(expand_builtin_functions_context *ctx, node *n)
{
	node *sub;
	node *r;
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(b = GET_CHILD(n, 1));
	ESSL_CHECK(sub = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_SUB, b));
	_essl_ensure_compatible_node(sub, a);
	ESSL_CHECK(sub = expand_builtin_functions_single_node(ctx, sub));
	
	ESSL_CHECK(r = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_LENGTH, sub, NULL, NULL));
	_essl_ensure_compatible_node(r, n);
	return expand_builtin_functions_single_node(ctx, r);

}

static node *handle_mix(expand_builtin_functions_context *ctx, node *n)
{
	node *a;
	node *b;
	node *u;
	node *invu;
	node *mul0;
	node *mul1;
	node *one;
	node *r;

	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(b = GET_CHILD(n, 1));
	ESSL_CHECK(u = GET_CHILD(n, 2));
	
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, GET_NODE_VEC_SIZE(u), _essl_get_scalar_size_for_type(u->hdr.type)));
	one->hdr.type = u->hdr.type;
	ESSL_CHECK(invu = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_SUB, u));
	_essl_ensure_compatible_node(invu, n);
	invu->hdr.type = u->hdr.type;
	ESSL_CHECK(invu = expand_builtin_functions_single_node(ctx, invu));
	
	ESSL_CHECK(mul0 = _essl_new_binary_expression(ctx->pool, b, EXPR_OP_MUL, u));
	_essl_ensure_compatible_node(mul0, n);
	ESSL_CHECK(mul0 = expand_builtin_functions_single_node(ctx, mul0));

	ESSL_CHECK(mul1 = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, invu));
	_essl_ensure_compatible_node(mul1, n);
	ESSL_CHECK(mul1 = expand_builtin_functions_single_node(ctx, mul1));

	ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, mul0, EXPR_OP_ADD, mul1));
	_essl_ensure_compatible_node(r, n);
	return expand_builtin_functions_single_node(ctx, r);


}

static node * handle_cross(expand_builtin_functions_context * ctx, node *n)
{
	node *a;
	node *b;
	node *swz[4];
	node *mul[2];
	unsigned i;
	node *r;

	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(b = GET_CHILD(n, 1));

	/* cross(a, b) -> a.yzx*b.zxy - a.zxy*b.yzx */
	
	ESSL_CHECK(swz[0] = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, a));
	ESSL_CHECK(swz[1] = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, b));
	ESSL_CHECK(swz[2] = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, a));
	ESSL_CHECK(swz[3] = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, b));
	swz[0]->expr.u.swizzle.indices[0] = 1;
	swz[0]->expr.u.swizzle.indices[1] = 2;
	swz[0]->expr.u.swizzle.indices[2] = 0;
	
	swz[1]->expr.u.swizzle.indices[0] = 2;
	swz[1]->expr.u.swizzle.indices[1] = 0;
	swz[1]->expr.u.swizzle.indices[2] = 1;
	
	swz[2]->expr.u.swizzle = swz[1]->expr.u.swizzle;
	swz[3]->expr.u.swizzle = swz[0]->expr.u.swizzle;
	
	for(i = 0; i < 4; ++i)
	{
		_essl_ensure_compatible_node(swz[i], n);
		ESSL_CHECK(swz[i] = expand_builtin_functions_single_node(ctx, swz[i]));
	}
	
	for(i = 0; i < 2; ++i)
	{
		ESSL_CHECK(mul[i] = _essl_new_binary_expression(ctx->pool, swz[i*2+0], EXPR_OP_MUL, swz[i*2+1]));
		_essl_ensure_compatible_node(mul[i], n);
		ESSL_CHECK(mul[i] = expand_builtin_functions_single_node(ctx, mul[i]));
		SET_CHILD(n, i, mul[i]);
	}
	ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, mul[0], EXPR_OP_SUB, mul[1]));
	_essl_ensure_compatible_node(r, n);
	return expand_builtin_functions_single_node(ctx, r);
}

static node * handle_faceforward(expand_builtin_functions_context * ctx, node *n)
{
	/*
	  faceforward(N, I, Nref) -> csel(dot(Nref, I) < 0.0, N, -N)
	  
	*/
	node *dot, *zero, *cmp, *negN, *csel;
	scalar_size_specifier sz;
	node *N;
	node *I;
	node *Nref;
	ESSL_CHECK(N = GET_CHILD(n, 0));
	ESSL_CHECK(I = GET_CHILD(n, 1));
	ESSL_CHECK(Nref = GET_CHILD(n, 2));
	sz = _essl_get_scalar_size_for_type(n->hdr.type);
	ESSL_CHECK(dot = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, Nref, I, 0));
	_essl_ensure_compatible_node(dot, n);
	ESSL_CHECK(dot->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, n->hdr.type, 1));
	ESSL_CHECK(dot = expand_builtin_functions_single_node(ctx, dot));
	
	ESSL_CHECK(zero = create_float_constant(ctx, 0.0, 1, sz));
	
	ESSL_CHECK(cmp = _essl_new_binary_expression(ctx->pool, dot, EXPR_OP_LT, zero));
	_essl_ensure_compatible_node(cmp, n);
	ESSL_CHECK(cmp->hdr.type = _essl_get_type_with_size(ctx->typestor_ctx, TYPE_BOOL, 1, sz));
	ESSL_CHECK(cmp = expand_builtin_functions_single_node(ctx, cmp));
	
	ESSL_CHECK(negN = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, N));
	_essl_ensure_compatible_node(negN, n);
	ESSL_CHECK(negN = expand_builtin_functions_single_node(ctx, negN));
	
	ESSL_CHECK(csel = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, cmp, N, negN));
	_essl_ensure_compatible_node(csel, n);
	return expand_builtin_functions_single_node(ctx, csel);

}

static node * handle_smoothstep(expand_builtin_functions_context * ctx, node *n)
{
	node *sub0, *sub1;
	node *div;
	node *clamp;
	node *zero, *one, *two, *three;
	node *sub2;
	node *mul0, *mul1, *mul2;
	scalar_size_specifier res_scalar_size;
	node *edge0;
	node *edge1;
	node *x;
	ESSL_CHECK(edge0 = GET_CHILD(n, 0));
	ESSL_CHECK(edge1 = GET_CHILD(n, 1));
	ESSL_CHECK(x = GET_CHILD(n, 2));

	res_scalar_size = _essl_get_scalar_size_for_type(n->hdr.type);

	ESSL_CHECK(sub0 = _essl_new_binary_expression(ctx->pool,  x, EXPR_OP_SUB, edge0));
	_essl_ensure_compatible_node(sub0, n);
	ESSL_CHECK(sub0 = expand_builtin_functions_single_node(ctx, sub0));

	ESSL_CHECK(sub1 = _essl_new_binary_expression(ctx->pool, edge1, EXPR_OP_SUB, edge0));
	_essl_ensure_compatible_node(sub1, n);
	sub1->hdr.type = edge0->hdr.type;
	ESSL_CHECK(sub1 = expand_builtin_functions_single_node(ctx, sub1));

	ESSL_CHECK(div = _essl_new_binary_expression(ctx->pool, sub0, EXPR_OP_DIV, sub1));
	_essl_ensure_compatible_node(div, n);
	ESSL_CHECK(div = expand_builtin_functions_single_node(ctx, div));

	ESSL_CHECK(zero = create_float_constant(ctx, 0.0, 1, res_scalar_size));
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1, res_scalar_size));
	ESSL_CHECK(two = create_float_constant(ctx, 2.0, 1, res_scalar_size));
	ESSL_CHECK(three = create_float_constant(ctx, 3.0, 1, res_scalar_size));
				
	ESSL_CHECK(clamp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_CLAMP, div, zero, one));
	_essl_ensure_compatible_node(clamp, n);
	ESSL_CHECK(clamp = expand_builtin_functions_single_node(ctx, clamp));

	ESSL_CHECK(mul0 = _essl_new_binary_expression(ctx->pool, clamp, EXPR_OP_MUL, two));
	_essl_ensure_compatible_node(mul0, n);
	ESSL_CHECK(mul0 = expand_builtin_functions_single_node(ctx, mul0));

	ESSL_CHECK(sub2 = _essl_new_binary_expression(ctx->pool, three, EXPR_OP_SUB, mul0));
	_essl_ensure_compatible_node(sub2, n);
	ESSL_CHECK(sub2 = expand_builtin_functions_single_node(ctx, sub2));

	ESSL_CHECK(mul1 = _essl_new_binary_expression(ctx->pool, clamp, EXPR_OP_MUL, clamp));
	_essl_ensure_compatible_node(mul1, n);
	ESSL_CHECK(mul1 = expand_builtin_functions_single_node(ctx, mul1));

	ESSL_CHECK(mul2 = _essl_new_binary_expression(ctx->pool, mul1, EXPR_OP_MUL, sub2));
	_essl_ensure_compatible_node(mul2, n);
	return expand_builtin_functions_single_node(ctx, mul2);
}

static node *handle_reflect(expand_builtin_functions_context * ctx, node *n)
{
	/*
	  reflect(I, N) -> I - (dot(N, I) * 2) * N
	  
	*/
	
	node *dot, *mul0, *two, *mul1, *sub;
	node *I;
	node *N;
	ESSL_CHECK(I = GET_CHILD(n, 0));
	ESSL_CHECK(N = GET_CHILD(n, 1));
	
	ESSL_CHECK(dot = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, N, I, 0));
	_essl_ensure_compatible_node(dot, n);
	ESSL_CHECK(dot->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, n->hdr.type, 1));
	ESSL_CHECK(dot = expand_builtin_functions_single_node(ctx, dot));
	
	ESSL_CHECK(two = create_float_constant(ctx, 2.0, 1, _essl_get_scalar_size_for_type(n->hdr.type)));
	
	if(ctx->flags & EXPAND_BUILTIN_FUNCTIONS_HAS_FAST_MUL_BY_POW2)
	{
		ESSL_CHECK(mul0 = _essl_new_binary_expression(ctx->pool, dot, EXPR_OP_MUL, N));
		_essl_ensure_compatible_node(mul0, n);
		ESSL_CHECK(mul0 = expand_builtin_functions_single_node(ctx, mul0));
		
		ESSL_CHECK(mul1 = _essl_new_binary_expression(ctx->pool, mul0, EXPR_OP_MUL, two));
		_essl_ensure_compatible_node(mul1, n);
		ESSL_CHECK(mul1 = expand_builtin_functions_single_node(ctx, mul1));
	}
	else 
	{
		ESSL_CHECK(mul0 = _essl_new_binary_expression(ctx->pool, dot, EXPR_OP_MUL, two));
		_essl_ensure_compatible_node(mul0, dot);
		ESSL_CHECK(mul0 = expand_builtin_functions_single_node(ctx, mul0));
		
		ESSL_CHECK(mul1 = _essl_new_binary_expression(ctx->pool, mul0, EXPR_OP_MUL, N));
		_essl_ensure_compatible_node(mul1, n);
		ESSL_CHECK(mul1 = expand_builtin_functions_single_node(ctx, mul1));
	}

	ESSL_CHECK(sub = _essl_new_binary_expression(ctx->pool, I, EXPR_OP_SUB, mul1));
	_essl_ensure_compatible_node(sub, n);
	return expand_builtin_functions_single_node(ctx, sub);		
}

static node * handle_refract(expand_builtin_functions_context * ctx, node *n)
{
	node *dot;
	node *squared_dot, *squared_eta;
	node *one, *zero, *veczero;
	node *inv_squared_dot;
	node *mega_squared;
	node *k;
	node *condition;
	node *sqrt_k;
	node *eta_dot;
	node *sqrt_k_eta_dot;
	node *eta_I;
	node *scaled_N;
	node *k_pos_res;
	node *csel;
	scalar_size_specifier res_scalar_size;

	node *I;
	node *N;
	node *eta;
	ESSL_CHECK(I = GET_CHILD(n, 0));
	ESSL_CHECK(N = GET_CHILD(n, 1));
	ESSL_CHECK(eta = GET_CHILD(n, 2));
	
	res_scalar_size = _essl_get_scalar_size_for_type(n->hdr.type);

	ESSL_CHECK(dot = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, N, I, 0));
	_essl_ensure_compatible_node(dot, n);
	dot->hdr.type = eta->hdr.type;
	ESSL_CHECK(dot = expand_builtin_functions_single_node(ctx, dot));

	ESSL_CHECK(squared_dot = _essl_new_binary_expression(ctx->pool, dot, EXPR_OP_MUL, dot));
	_essl_ensure_compatible_node(squared_dot, dot);
	ESSL_CHECK(squared_dot = expand_builtin_functions_single_node(ctx, squared_dot));

	ESSL_CHECK(squared_eta = _essl_new_binary_expression(ctx->pool, eta, EXPR_OP_MUL, eta));
	_essl_ensure_compatible_node(squared_eta, dot);
	ESSL_CHECK(squared_eta = expand_builtin_functions_single_node(ctx, squared_eta));

	ESSL_CHECK(zero = create_float_constant(ctx, 0.0, 1, res_scalar_size));
	ESSL_CHECK(veczero = create_float_constant(ctx, 0.0, _essl_get_type_size(n->hdr.type), res_scalar_size));
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1, res_scalar_size));
			
	ESSL_CHECK(inv_squared_dot = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_SUB, squared_dot));
	_essl_ensure_compatible_node(inv_squared_dot, dot);
	ESSL_CHECK(inv_squared_dot = expand_builtin_functions_single_node(ctx, inv_squared_dot));
				
	ESSL_CHECK(mega_squared = _essl_new_binary_expression(ctx->pool, inv_squared_dot, EXPR_OP_MUL, squared_eta));
	_essl_ensure_compatible_node(mega_squared, dot);
	ESSL_CHECK(mega_squared = expand_builtin_functions_single_node(ctx, mega_squared));

	ESSL_CHECK(k = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_SUB, mega_squared));
	_essl_ensure_compatible_node(k, dot);
	ESSL_CHECK(k = expand_builtin_functions_single_node(ctx, k));
					
	ESSL_CHECK(condition = _essl_new_binary_expression(ctx->pool, k, EXPR_OP_LT, zero));
	_essl_ensure_compatible_node(condition, dot);
	ESSL_CHECK(condition->hdr.type = _essl_get_type_with_size(ctx->typestor_ctx, TYPE_BOOL, 1, res_scalar_size));
	ESSL_CHECK(condition = expand_builtin_functions_single_node(ctx, condition));
								
	ESSL_CHECK(sqrt_k = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_SQRT, k, 0, 0));
	_essl_ensure_compatible_node(sqrt_k, dot);
	ESSL_CHECK(sqrt_k = expand_builtin_functions_single_node(ctx, sqrt_k));
				
	ESSL_CHECK(eta_dot = _essl_new_binary_expression(ctx->pool, dot, EXPR_OP_MUL, eta));
	_essl_ensure_compatible_node(eta_dot, dot);
	ESSL_CHECK(eta_dot = expand_builtin_functions_single_node(ctx, eta_dot));
				
	ESSL_CHECK(sqrt_k_eta_dot = _essl_new_binary_expression(ctx->pool, sqrt_k, EXPR_OP_ADD, eta_dot));
	_essl_ensure_compatible_node(sqrt_k_eta_dot, dot);
	ESSL_CHECK(sqrt_k_eta_dot = expand_builtin_functions_single_node(ctx, sqrt_k_eta_dot));
				
	ESSL_CHECK(eta_I = _essl_new_binary_expression(ctx->pool, I, EXPR_OP_MUL, eta));
	_essl_ensure_compatible_node(eta_I, n);
	ESSL_CHECK(eta_I = expand_builtin_functions_single_node(ctx, eta_I));
				
	ESSL_CHECK(scaled_N = _essl_new_binary_expression(ctx->pool, N, EXPR_OP_MUL, sqrt_k_eta_dot));
	_essl_ensure_compatible_node(scaled_N, n);
	ESSL_CHECK(scaled_N = expand_builtin_functions_single_node(ctx, scaled_N));
				
	ESSL_CHECK(k_pos_res = _essl_new_binary_expression(ctx->pool, eta_I, EXPR_OP_SUB, scaled_N));
	_essl_ensure_compatible_node(k_pos_res, n);
	ESSL_CHECK(k_pos_res = expand_builtin_functions_single_node(ctx, k_pos_res));
								
	ESSL_CHECK(csel = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, condition, veczero, k_pos_res));
	_essl_ensure_compatible_node(csel, n);
	return expand_builtin_functions_single_node(ctx, csel);
}

static node * handle_radians(expand_builtin_functions_context * ctx, node *n)
{
	node *constant;
	node *r;
	node *a;
	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(constant = create_float_constant(ctx, (float)(M_PI/180.0), 1, _essl_get_scalar_size_for_type(n->hdr.type)));

	ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(r, n);
	return expand_builtin_functions_single_node(ctx, r);
}

static node * handle_degrees(expand_builtin_functions_context * ctx, node *n)
{
	node *constant;
	node *r;
	node *a;
	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(constant = create_float_constant(ctx, (float)(180.0/M_PI), 1, _essl_get_scalar_size_for_type(n->hdr.type)));

	ESSL_CHECK(r = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(r, n);
	return expand_builtin_functions_single_node(ctx, r);
}

static node * handle_exp(expand_builtin_functions_context * ctx, node *n)
{
	node *constant;
	node *mul;
	node *r;
	node *a;
	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(constant = create_float_constant(ctx, (float)LOG2_E, 1, _essl_get_scalar_size_for_type(n->hdr.type)));

	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, a, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(mul, n);
	ESSL_CHECK(mul = expand_builtin_functions_single_node(ctx, mul));

	ESSL_CHECK(r = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_EXP2, mul, NULL, NULL));
	_essl_ensure_compatible_node(r, n);
	return expand_builtin_functions_single_node(ctx, r);
}

static node * handle_log(expand_builtin_functions_context * ctx, node *n)
{
	node *constant;
	node *mul;
	node *log2;
	node *a;

	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(log2 = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_LOG2, a, NULL, NULL));
	_essl_ensure_compatible_node(log2, n);
	ESSL_CHECK(log2 = expand_builtin_functions_single_node(ctx, log2));

	ESSL_CHECK(constant = create_float_constant(ctx, (float)(1.0/LOG2_E), 1, _essl_get_scalar_size_for_type(n->hdr.type)));
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, log2, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(mul, n);
	return expand_builtin_functions_single_node(ctx, mul);
}

static node * handle_step(expand_builtin_functions_context * ctx, node *n)
{
	node *r;
	type_specifier *t;
	node *cons;
	node *a;
	node *b;

	ESSL_CHECK(a = GET_CHILD(n, 0));
	ESSL_CHECK(b = GET_CHILD(n, 1));

	ESSL_CHECK(r = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_LESSTHANEQUAL, a, b, NULL));
	_essl_ensure_compatible_node(r, n);
	ESSL_CHECK(t = _essl_clone_type(ctx->pool, r->hdr.type));
	t->basic_type = TYPE_BOOL;
	r->hdr.type = t;
	ESSL_CHECK(r = expand_builtin_functions_single_node(ctx, r));

	ESSL_CHECK(cons = _essl_new_type_convert_expression(ctx->pool, EXPR_OP_IMPLICIT_TYPE_CONVERT, r));
	_essl_ensure_compatible_node(cons, n);
	return expand_builtin_functions_single_node(ctx, cons);
}

static node * expand_builtin_functions_single_node(expand_builtin_functions_context * ctx, node *n)

{
	node_kind kind;
	expression_operator op;
	
	ESSL_CHECK(n);
	kind = n->hdr.kind;
	op = n->expr.operation;

	if(kind == EXPR_KIND_BUILTIN_FUNCTION_CALL)
	{
		if(op == EXPR_OP_FUN_DISTANCE)
		{
			return handle_distance(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MIX)
		{
			return handle_mix(ctx, n);
		}
		else if(op == EXPR_OP_FUN_CROSS)
		{
			return handle_cross(ctx, n);
		}
		else if(op == EXPR_OP_FUN_FACEFORWARD)
		{
			return handle_faceforward(ctx, n);
		}
		else if(op == EXPR_OP_FUN_SMOOTHSTEP)
		{
			return handle_smoothstep(ctx, n);
		}
		else if(op == EXPR_OP_FUN_REFLECT)
		{
			return handle_reflect(ctx, n);
		}
		else if(op == EXPR_OP_FUN_REFRACT)
		{
			return handle_refract(ctx, n);
		}
		else if(op == EXPR_OP_FUN_RADIANS)
		{
			return handle_radians(ctx, n);
		}
		else if(op == EXPR_OP_FUN_DEGREES)
		{
			return handle_degrees(ctx, n);
		}
		else if(op == EXPR_OP_FUN_EXP)
		{
			return handle_exp(ctx, n);
		}
		else if(op == EXPR_OP_FUN_LOG)
		{
			return handle_log(ctx, n);
		}
		else if(op == EXPR_OP_FUN_STEP)
		{
			return handle_step(ctx, n);
		}
	}
	else if(kind == EXPR_KIND_TRANSFER)
	{
		return GET_CHILD(n, 0);
	}

	return n;
}

static node *process_node_w(expand_builtin_functions_context *ctx, node *n)
{

	node *nn;
	
	ESSL_CHECK(nn = expand_builtin_functions_single_node(ctx, n));
	return nn;


}

/*@null@*/ static node *process_node(expand_builtin_functions_context *ctx, node *n)
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



static memerr handle_block(expand_builtin_functions_context *ctx, basic_block *b)
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



memerr _essl_expand_builtin_functions(pass_run_context *pr_ctx, symbol *func)
{

	expand_builtin_functions_context ctx;
	unsigned int i;


	ctx.pool = pr_ctx->pool;
	ctx.cfg = func->control_flow_graph;
	ctx.desc = pr_ctx->desc;
	ctx.typestor_ctx = pr_ctx->ts_ctx;
	ctx.flags = pr_ctx->desc->expand_builtins_options;

	ESSL_CHECK(_essl_ptrdict_init(&ctx.visited, pr_ctx->tmp_pool));
	for (i = 0 ; i < ctx.cfg->n_blocks ; i++) 
	{
		ESSL_CHECK(handle_block(&ctx, ctx.cfg->postorder_sequence[i]));
	}
	return MEM_OK;
}
