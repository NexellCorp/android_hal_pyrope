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
#include "frontend/constant_fold.h"
#include "common/symbol_table.h"

#ifdef UNIT_TEST
#include "frontend/typecheck.h"
#include "frontend/parser.h"
#include "frontend/frontend.h"
#endif /* UNIT_TEST */


/** Code to retrieve the source offset for this module (not reported) */
#define SOURCE_OFFSET 0

/** Code to access the error context for this module */
#define ERROR_CONTEXT ctx->err_context

/** Struct member access */
/*@null@*/ static node *member(node *n)
{
	node *a;
	int index;
	a = GET_CHILD(n, 0);
	assert(a != 0);
	assert(a->hdr.kind == EXPR_KIND_STRUCT_CONSTRUCTOR);
	assert(n->hdr.type != 0);
	assert(a->hdr.type != 0);
	index = n->expr.u.member->index;

	assert(index >= 0 && index < (int)GET_N_CHILDREN(a));

	return GET_CHILD(a, index);
}

/** Vector swizzling */
/*@null@*/ static node *swizzle(constant_fold_context *ctx, node *n)
{
	node *m;
	unsigned int i;
	node *a;
	swizzle_pattern sp = n->expr.u.swizzle;
	unsigned size = GET_NODE_VEC_SIZE(n);
	a = GET_CHILD(n, 0);
	assert(a != 0);
	m = _essl_new_constant_expression(ctx->pool, size);
	ESSL_CHECK_OOM(m);
	_essl_ensure_compatible_node(m, n);
	m->hdr.type = n->hdr.type;
	for (i = 0; i < size; ++i)
	{
		if((int)sp.indices[i] >= 0)
		{
			m->expr.u.value[i] = a->expr.u.value[(unsigned)sp.indices[i]];
		}
	}
	return m;
}

/** Component-wise unary expressions, handled by calling the target-specific constant folder for each scalar */
/*@null@*/ static node *unary_op(constant_fold_context *ctx, node *n)
{
	node *a;
	node *m = 0;
	expression_operator op = n->expr.operation;
	unsigned int i;
	unsigned int asize;
	scalar_type dummy;
	a = GET_CHILD(n, 0);
	assert(a != 0);
	assert(a->hdr.type != 0);
	assert(n->hdr.type != 0);
	asize = _essl_get_type_size(a->hdr.type);
	dummy = ctx->desc->float_to_scalar(0.0);

	m = _essl_new_constant_expression(ctx->pool, asize);
	ESSL_CHECK_OOM(m);
	_essl_ensure_compatible_node(m, n);
	/* handle scalar-vector and component-wise operations at the same time */
	for (i = 0; i < asize; ++i)
	{
		m->expr.u.value[i] = ctx->desc->constant_fold(n->hdr.type, op, a->expr.u.value[i], dummy, dummy);
	}
	return m;
}

/** Component-wise binary expressions, handled by calling the target-specific constant folder for each pair of scalars */
/*@null@*/ static node *binary_op(constant_fold_context *ctx, node *n)
{
	node *a;
	node *b;
	node *m = 0;
	expression_operator op = n->expr.operation;
	unsigned int asize;
	unsigned int bsize;
	unsigned int i;
	scalar_type dummy;
	a = GET_CHILD(n, 0);
	assert(a != 0);
	b = GET_CHILD(n, 1);
	assert(b != 0);
	assert(a->hdr.type != 0);
	asize = _essl_get_type_size(a->hdr.type);
	assert(b->hdr.type != 0);
	bsize = _essl_get_type_size(b->hdr.type);
	assert(n->hdr.type != 0);

	dummy = ctx->desc->float_to_scalar(0.0);

	if(op == EXPR_OP_MUL 
	   && ((a->hdr.type->basic_type == TYPE_MATRIX_OF && bsize > 1)
		   || (b->hdr.type->basic_type == TYPE_MATRIX_OF && asize > 1)))
	   
	{
		m = _essl_new_constant_expression(ctx->pool, ESSL_MIN(asize, bsize));
		ESSL_CHECK_OOM(m);
		_essl_ensure_compatible_node(m, n);
		assert(m->hdr.type != 0);
		ctx->desc->constant_fold_sized(n->expr.operation, &m->expr.u.value[0], ESSL_MIN(asize, bsize), &a->expr.u.value[0], &b->expr.u.value[0], 0, a->hdr.type, b->hdr.type);
		return m;
	}


	m = _essl_new_constant_expression(ctx->pool, ESSL_MAX(asize, bsize));
	ESSL_CHECK_OOM(m);
	_essl_ensure_compatible_node(m, n);
	/* handle scalar-vector and component-wise operations at the same time */
	for (i = 0; i < ESSL_MAX(asize, bsize); ++i)
	{
		m->expr.u.value[i] = ctx->desc->constant_fold(n->hdr.type, op, a->expr.u.value[ESSL_MIN(i, asize-1)], b->expr.u.value[ESSL_MIN(i, bsize-1)], dummy);
	}
	return m;
}

/** Handles a built-in constructor.
 */
/*@null@*/ static node *constructor(constant_fold_context *ctx, node *n)
{
	unsigned int i, j;
	node *m;
	node *child = 0;
	unsigned int size;
	unsigned child_pos = 0;
	assert(n->hdr.type != 0);
	assert(n->hdr.kind == EXPR_KIND_BUILTIN_CONSTRUCTOR);
	size = _essl_get_type_size(n->hdr.type);

	m = _essl_new_constant_expression(ctx->pool, size);
	ESSL_CHECK_OOM(m);
	_essl_ensure_compatible_node(m, n);
	child = GET_CHILD(n, 0);
	assert(child != 0);
	assert(child->hdr.type != 0);
	if(GET_N_CHILDREN(n) == 1 && _essl_get_type_size(child->hdr.type) == 1 && size > 1)
	{
		if(n->hdr.type->basic_type == TYPE_MATRIX_OF)
		{
			/* scalar->matrix */
			unsigned n_rows = GET_MATRIX_N_ROWS(n->hdr.type);
			unsigned n_columns = GET_MATRIX_N_COLUMNS(n->hdr.type);
			for(i = 0; i < n_columns; ++i)
			{
				for(j = 0; j < n_rows; ++j)
				{
					if(i == j)
					{
						m->expr.u.value[i*n_rows + j] = ctx->desc->convert_scalar(n->hdr.type, child->expr.u.value[0]);
					} else {
						m->expr.u.value[i*n_rows + j] = ctx->desc->float_to_scalar(0.0);
					}
				}
			}
		} else {
			/* scalar->vector */
			for (i = 0; i < size; ++i)
			{
				m->expr.u.value[i] = ctx->desc->convert_scalar(n->hdr.type, child->expr.u.value[0]);
			}
		}
		return m;
	}

	if (GET_N_CHILDREN(n) == 1 && n->hdr.type->basic_type == TYPE_MATRIX_OF && child->hdr.type->basic_type == TYPE_MATRIX_OF)
	{
		/* matrix->matrix */
		unsigned src_n_rows = GET_MATRIX_N_ROWS(child->hdr.type);
		unsigned src_n_columns = GET_MATRIX_N_COLUMNS(child->hdr.type);
		unsigned dest_n_rows = GET_MATRIX_N_ROWS(n->hdr.type);
		unsigned dest_n_columns = GET_MATRIX_N_COLUMNS(n->hdr.type);

		unsigned min_n_rows = ESSL_MIN(dest_n_rows, src_n_rows);
		unsigned min_n_columns = ESSL_MIN(dest_n_columns, src_n_columns);
		for(i = 0; i < dest_n_columns; ++i)
		{
			for(j = 0; j < dest_n_rows; ++j)
			{
				if (i < min_n_columns && j < min_n_rows)
				{
					/* Matrix is always float - no type conversion needed */
					m->expr.u.value[i*dest_n_rows+j] = child->expr.u.value[i*src_n_rows+j];
				} else if (i == j) {
					m->expr.u.value[i*dest_n_rows+j] = ctx->desc->float_to_scalar(1.0);
				} else {
					m->expr.u.value[i*dest_n_rows+j] = ctx->desc->float_to_scalar(0.0);
				}
			}
		}
		return m;
	}

	for (i = 0; i < size;)
	{
		unsigned int child_size;
		child = GET_CHILD(n, child_pos);
		assert(child != 0);
		child_size = _essl_get_type_size(child->hdr.type);

		for (j = 0; i < size && j < child_size; ++i, ++j)
		{
			m->expr.u.value[i] = ctx->desc->convert_scalar(n->hdr.type, child->expr.u.value[j]);
		}
		++child_pos;
	}
	return m;
}

/** Handles a type conversion
 */
/*@null@*/ static node *type_convert(constant_fold_context *ctx, node *n)
{
	unsigned int i;
	node *m;
	node *child = 0;
	unsigned int size;
	assert(n->hdr.type != 0);
	assert(n->hdr.kind == EXPR_KIND_TYPE_CONVERT);
	size = _essl_get_type_size(n->hdr.type);

	m = _essl_new_constant_expression(ctx->pool, size);
	ESSL_CHECK_OOM(m);
	_essl_ensure_compatible_node(m, n);
	child = GET_CHILD(n, 0);
	assert(child != 0);
	assert(child->hdr.type != 0);

	for (i = 0; i < size; ++i)
	{
		m->expr.u.value[i] = ctx->desc->convert_scalar(n->hdr.type, child->expr.u.value[i]);
	}
	return m;
}

/** Built-in function calls */
/*@null@*/ static node *builtin_function_call(constant_fold_context *ctx, node *n)
{
	switch (n->expr.operation)
	{
	case EXPR_OP_FUN_RADIANS:
	case EXPR_OP_FUN_DEGREES:
	case EXPR_OP_FUN_SIN:
	case EXPR_OP_FUN_COS:
	case EXPR_OP_FUN_TAN:
	case EXPR_OP_FUN_ASIN:
	case EXPR_OP_FUN_ACOS:
	case EXPR_OP_FUN_EXP:
	case EXPR_OP_FUN_LOG:
	case EXPR_OP_FUN_EXP2:
	case EXPR_OP_FUN_LOG2:
	case EXPR_OP_FUN_SQRT:
	case EXPR_OP_FUN_INVERSESQRT:
	case EXPR_OP_FUN_ABS:
	case EXPR_OP_FUN_SIGN:
	case EXPR_OP_FUN_FLOOR:
	case EXPR_OP_FUN_CEIL:
	case EXPR_OP_FUN_FRACT:
	case EXPR_OP_FUN_NOT:
	case EXPR_OP_FUN_DFDX:
	case EXPR_OP_FUN_DFDY:
	case EXPR_OP_FUN_FWIDTH:
	case EXPR_OP_FUN_RCP:
		{
			/* Component-wise unary ops */
			node *m;
			unsigned int i;
			scalar_type dummy;
			unsigned int nsize;
			node *a = GET_CHILD(n, 0);
			dummy = ctx->desc->float_to_scalar(0.0);
			assert(a != 0);
			assert(n->hdr.type != 0);
			nsize = _essl_get_type_size(n->hdr.type);
			m = _essl_new_constant_expression(ctx->pool, nsize);
			ESSL_CHECK_OOM(m);
			_essl_ensure_compatible_node(m, n);
			assert(m->hdr.type != 0);
			for (i = 0; i < nsize; ++i)
			{
				m->expr.u.value[i] = ctx->desc->constant_fold(m->hdr.type, n->expr.operation, a->expr.u.value[i], dummy, dummy);
			}
			return m;
		}

	case EXPR_OP_FUN_POW:
	case EXPR_OP_FUN_MOD:
	case EXPR_OP_FUN_MIN:
	case EXPR_OP_FUN_MAX:
	case EXPR_OP_FUN_STEP:
	case EXPR_OP_FUN_LESSTHAN:
	case EXPR_OP_FUN_LESSTHANEQUAL:
	case EXPR_OP_FUN_GREATERTHAN:
	case EXPR_OP_FUN_GREATERTHANEQUAL:
	case EXPR_OP_FUN_EQUAL:
	case EXPR_OP_FUN_NOTEQUAL:
		{
			/* Component-wise binary ops */
			node *m;
			unsigned int i;
			node *a, *b;
			unsigned int asize, bsize;
			scalar_type dummy;
			dummy = ctx->desc->float_to_scalar(0.0);
			a = GET_CHILD(n, 0);
			assert(a != 0);
			b = GET_CHILD(n, 1);
			assert(b != 0);
			assert(a->hdr.type != 0);
			asize = _essl_get_type_size(a->hdr.type);
			assert(b->hdr.type != 0);
			bsize = _essl_get_type_size(b->hdr.type);
			assert(n->hdr.type != 0);
			m = _essl_new_constant_expression(ctx->pool, GET_NODE_VEC_SIZE(n));
			ESSL_CHECK_OOM(m);
			_essl_ensure_compatible_node(m, n);
			assert(m->hdr.type != 0);
			for (i = 0; i < ESSL_MAX(asize, bsize); ++i)
			{
				m->expr.u.value[i] = ctx->desc->constant_fold(m->hdr.type, n->expr.operation, a->expr.u.value[ESSL_MIN(i, asize - 1)], b->expr.u.value[ESSL_MIN(i, bsize -1)], dummy);
			}
			return m;
		}

	case EXPR_OP_FUN_CLAMP:
	case EXPR_OP_FUN_MIX:
	case EXPR_OP_FUN_SMOOTHSTEP:
		{
			/* Component-wise ternary ops */
			node *m;
			unsigned int i;
			node *a, *b, *c;
			unsigned int asize, bsize, csize;
			a = GET_CHILD(n, 0);
			assert(a != 0);
			b = GET_CHILD(n, 1);
			assert(b != 0);
			c = GET_CHILD(n, 2);
			assert(c != 0);
			assert(a->hdr.type != 0);
			asize = _essl_get_type_size(a->hdr.type);
			assert(b->hdr.type != 0);
			bsize = _essl_get_type_size(b->hdr.type);
			assert(c->hdr.type != 0);
			csize = _essl_get_type_size(c->hdr.type);
			assert(n->hdr.type != 0);
			m = _essl_new_constant_expression(ctx->pool, GET_NODE_VEC_SIZE(n));
			ESSL_CHECK_OOM(m);
			_essl_ensure_compatible_node(m, n);
			assert(m->hdr.type != 0);
			for (i = 0; i < ESSL_MAX(ESSL_MAX(asize, bsize), csize); ++i)
			{
				m->expr.u.value[i] = ctx->desc->constant_fold(m->hdr.type, n->expr.operation, a->expr.u.value[ESSL_MIN(i, asize - 1)], b->expr.u.value[ESSL_MIN(i, bsize -1)], c->expr.u.value[ESSL_MIN(i, csize -1)]);
			}
			return m;
		}

	case EXPR_OP_FUN_ATAN:
		{
			/* Can be either atan(x) or atan(x, y) */
			node *m;
			unsigned int i;
			node *a, *b = 0;
			unsigned int asize;
			scalar_type one = ctx->desc->float_to_scalar(1.0);
			a = GET_CHILD(n, 0);
			assert(a != 0);
			if(GET_N_CHILDREN(n) > 1)
			{
				b = GET_CHILD(n, 1);
			}
			assert(a->hdr.type != 0);
			asize = _essl_get_type_size(a->hdr.type);
			assert(n->hdr.type != 0);
			m = _essl_new_constant_expression(ctx->pool, GET_NODE_VEC_SIZE(n));
			ESSL_CHECK_OOM(m);
			_essl_ensure_compatible_node(m, n);
			assert(m->hdr.type != 0);
			for (i = 0; i < asize; ++i)
			{
				if (b != 0)
				{
					m->expr.u.value[i] = ctx->desc->constant_fold(m->hdr.type, n->expr.operation, a->expr.u.value[i], b->expr.u.value[i], one);
				}
				else
				{
					m->expr.u.value[i] = ctx->desc->constant_fold(m->hdr.type, n->expr.operation, a->expr.u.value[i], one, one);
				}
			}
			return m;
		}

	case EXPR_OP_FUN_LENGTH:
	case EXPR_OP_FUN_NORMALIZE:
	case EXPR_OP_FUN_ANY:
	case EXPR_OP_FUN_ALL:
		{
			/* Operation on a single vector */
			node *m;
			node *a = GET_CHILD(n, 0);
			unsigned int asize, nsize;
			assert(a != 0);
			assert(a->hdr.type != 0);
			asize = _essl_get_type_size(a->hdr.type);
			assert(n->hdr.type != 0);
			nsize = _essl_get_type_size(n->hdr.type);
			m = _essl_new_constant_expression(ctx->pool, nsize);
			ESSL_CHECK_OOM(m);
			_essl_ensure_compatible_node(m, n);
			assert(m->hdr.type != 0);
			ctx->desc->constant_fold_sized(n->expr.operation, &m->expr.u.value[0], asize, &a->expr.u.value[0], 0, 0, a->hdr.type, 0);
			return m;
		}

	case EXPR_OP_FUN_DISTANCE:
	case EXPR_OP_FUN_DOT:
	case EXPR_OP_FUN_REFLECT:
	case EXPR_OP_FUN_CROSS:
	case EXPR_OP_FUN_MATRIXCOMPMULT:
		{
			/* Operation on two args */
			node *m;
			node *a, *b;
			unsigned int asize;
			a = GET_CHILD(n, 0);
			assert(a != 0);
			b = GET_CHILD(n, 1);
			assert(b != 0);
			assert(a->hdr.type != 0);
			asize = _essl_get_type_size(a->hdr.type);
			assert(b->hdr.type != 0);
			assert(asize == _essl_get_type_size(b->hdr.type));
			assert(n->hdr.type != 0);
			m = _essl_new_constant_expression(ctx->pool, _essl_get_type_size(n->hdr.type));
			ESSL_CHECK_OOM(m);
			_essl_ensure_compatible_node(m, n);
			assert(m->hdr.type != 0);
			ctx->desc->constant_fold_sized(n->expr.operation, &m->expr.u.value[0], asize, &a->expr.u.value[0], &b->expr.u.value[0], 0, a->hdr.type, b->hdr.type);
			return m;
		}

	case EXPR_OP_FUN_FACEFORWARD:
	case EXPR_OP_FUN_REFRACT:
		{
			/* Operation on three args */
			node *m;
			node *a, *b, *c;
			unsigned int asize;
			a = GET_CHILD(n, 0);
			assert(a != 0);
			b = GET_CHILD(n, 1);
			assert(b != 0);
			c = GET_CHILD(n, 2);
			assert(c != 0);
			assert(a->hdr.type != 0);
			asize = _essl_get_type_size(a->hdr.type);
			assert(b->hdr.type != 0);
			assert(c->hdr.type != 0);
			assert(asize == _essl_get_type_size(b->hdr.type));
			assert(n->hdr.type != 0);
			m = _essl_new_constant_expression(ctx->pool, GET_NODE_VEC_SIZE(n));
			ESSL_CHECK_OOM(m);
			_essl_ensure_compatible_node(m, n);
			assert(m->hdr.type != 0);
			ctx->desc->constant_fold_sized(n->expr.operation, &m->expr.u.value[0], asize, &a->expr.u.value[0], &b->expr.u.value[0], &c->expr.u.value[0], a->hdr.type, b->hdr.type);
			return m;
		}

	default:
		/* Unknown builtin, don't constant fold it */
		break;
	}
	return n;
}

static essl_bool values_equal(constant_fold_context *ctx, node *a, node *b, node *cmp)
{
	if (a->hdr.kind == EXPR_KIND_STRUCT_CONSTRUCTOR)
	{
		unsigned i;
		/* Struct comparison */
		assert(b->hdr.kind == EXPR_KIND_STRUCT_CONSTRUCTOR);
		assert(a->hdr.type->basic_type == TYPE_STRUCT);
		assert(b->hdr.type->basic_type == TYPE_STRUCT);
		assert(GET_N_CHILDREN(a) == GET_N_CHILDREN(b));

		for (i = 0 ; i < GET_N_CHILDREN(a) ; i++)
		{
			if (!values_equal(ctx, GET_CHILD(a,i), GET_CHILD(b,i), cmp)) return ESSL_FALSE;
		}
		return ESSL_TRUE;
	} else {
		unsigned int asize;
		scalar_type res;
		asize = _essl_get_type_size(a->hdr.type);
		assert(asize == _essl_get_type_size(b->hdr.type));
		ctx->desc->constant_fold_sized(EXPR_OP_EQ, &res, asize, &a->expr.u.value[0], &b->expr.u.value[0], 0, a->hdr.type, b->hdr.type);
		return ctx->desc->scalar_to_bool(res);
	}
}

/** Constant folds a single node, calling helper functions if necessary */
node *_essl_constant_fold_single_node(constant_fold_context *ctx, node *n)
{
	node *first, *second, *third;
	assert(n != 0);
	if(GET_NODE_KIND(n->hdr.kind) != NODE_KIND_EXPRESSION) return n;
	switch(n->hdr.kind)
	{
	case EXPR_KIND_UNARY:
		first = GET_CHILD(n, 0);
		if(first == 0) return 0;
		assert(first->hdr.type != 0);
		switch (n->expr.operation)
		{
		case EXPR_OP_MEMBER:
			if (first->hdr.kind == EXPR_KIND_STRUCT_CONSTRUCTOR && _essl_node_is_constant(first))
			{
				return member(n);
			}
			break;
		case EXPR_OP_SWIZZLE:
			if (first->hdr.kind == EXPR_KIND_CONSTANT)
			{
				return swizzle(ctx, n);
			}
			break;
		case EXPR_OP_NOT:
		case EXPR_OP_NEGATE:
		case EXPR_OP_PLUS:
			if (first->hdr.kind == EXPR_KIND_CONSTANT)
			{
				return unary_op(ctx, n);
			}
			break;

		default:
			break;
		}
		break;
	case EXPR_KIND_BINARY:
		first = GET_CHILD(n, 0);
		if(first == 0) return 0;
		assert(first->hdr.type != 0);
		second = GET_CHILD(n, 1);
		if(second == 0) return 0;
		assert(second->hdr.type != 0);

		switch (n->expr.operation)
		{
		case EXPR_OP_ADD:
		case EXPR_OP_SUB:
		case EXPR_OP_MUL:
		case EXPR_OP_DIV:

		case EXPR_OP_LT:
		case EXPR_OP_LE:
		case EXPR_OP_GE:
		case EXPR_OP_GT:
		case EXPR_OP_LOGICAL_AND:
		case EXPR_OP_LOGICAL_OR:
		case EXPR_OP_LOGICAL_XOR:
			if (first->hdr.kind == EXPR_KIND_CONSTANT && second->hdr.kind == EXPR_KIND_CONSTANT)
			{
				return binary_op(ctx, n);
			}
			break;

		case EXPR_OP_EQ:
		case EXPR_OP_NE:
			if (_essl_node_is_constant(first) && _essl_node_is_constant(second))
			{
				node *m;
				unsigned int nsize;
				essl_bool result = values_equal(ctx, first, second, n);
				if (n->expr.operation == EXPR_OP_NE) result = !result;
				nsize = _essl_get_type_size(n->hdr.type);
				m = _essl_new_constant_expression(ctx->pool, nsize);
				ESSL_CHECK_OOM(m);
				_essl_ensure_compatible_node(m, n);
				assert(m->hdr.type != 0);
				m->expr.u.value[0] = ctx->desc->bool_to_scalar(result);
				return m;
			}
			break;

		case EXPR_OP_INDEX:
			if (first->hdr.kind == EXPR_KIND_CONSTANT && second->hdr.kind == EXPR_KIND_CONSTANT)
			{
				unsigned int size;
				node *res;
				int idx;
				assert(second->hdr.type != 0);
				idx= ctx->desc->scalar_to_int(second->expr.u.value[0]);
				assert(idx >= 0);

				assert(n->hdr.type != 0);
				size = _essl_get_type_size(n->hdr.type);

				res = _essl_new_constant_expression(ctx->pool, size);
				ESSL_CHECK_OOM(res);
				_essl_ensure_compatible_node(res, n);
				memcpy(&res->expr.u.value[0], &first->expr.u.value[idx*size], size*sizeof(scalar_type));
				return res;
			}
			break;

		case EXPR_OP_CHAIN:
			if (_essl_node_is_constant(first))
			{
				_essl_ensure_compatible_node(second, n);
				return second;
			}
			break;

		default:
			break;
		}
		break;
	case EXPR_KIND_TERNARY:
		first = GET_CHILD(n, 0);
		if(first == 0) return 0;
		assert(first->hdr.type != 0);

		second = GET_CHILD(n, 1);
		if(second == 0) return 0;
		assert(second->hdr.type != 0);

		third = GET_CHILD(n, 2);
		if(third == 0) return 0;
		assert(third->hdr.type != 0);

		switch (n->expr.operation)
		{
		case EXPR_OP_CONDITIONAL:
			if (first->hdr.kind == EXPR_KIND_CONSTANT &&
			    _essl_node_is_constant(second) &&
			    _essl_node_is_constant(third))
			{
				/*
				* All three operands must be constant (ref 5.10) for an expression to be constant.
				* We could have checked if the condition (first) is constant and returned second
				* or third, depending on the constant value of first, but this would violate what
				* ESSL defines as a constant expression.
				*/
				int val;
				assert(n->hdr.type != 0);
				val = ctx->desc->scalar_to_bool(first->expr.u.value[0]);
				if (val != 0)
				{
					_essl_ensure_compatible_node(second, n);
					return second;
				} else {
					_essl_ensure_compatible_node(third, n);
					return third;
				}
			}
			break;
		default:
			break;
		}
		break;
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		{
			unsigned i;
			for(i = 0; i < GET_N_CHILDREN(n); ++i)
			{
				node *child = GET_CHILD(n, i);
				if(child != 0 && child->hdr.kind != EXPR_KIND_CONSTANT)
				{
					return n;
				}
			}
			return builtin_function_call(ctx, n);
		}
	case EXPR_KIND_BUILTIN_CONSTRUCTOR:
		{
			unsigned i;
			for(i = 0; i < GET_N_CHILDREN(n); ++i)
			{
				node *child = GET_CHILD(n, i);
				if(child != 0 && child->hdr.kind != EXPR_KIND_CONSTANT)
				{
					return n;
				}
			}
			return constructor(ctx, n);
		}

	case EXPR_KIND_VARIABLE_REFERENCE:
		{
			symbol *sym = n->expr.u.sym;
			if (sym->kind == SYM_KIND_VARIABLE && sym->qualifier.variable == VAR_QUAL_CONSTANT)
			{
				node *res;
				assert(sym->body != 0);
				assert(sym->body->hdr.type != 0);
				assert(_essl_node_is_constant(sym->body));
				res = _essl_clone_node(ctx->pool, sym->body);
				ESSL_CHECK_OOM(res);
				_essl_ensure_compatible_node(res, n);
				return res;
			}
		}
		break;

	case EXPR_KIND_TYPE_CONVERT:
		first = GET_CHILD(n, 0);
		if(first == 0) return 0;
		assert(first->hdr.type != 0);
		if (first->hdr.kind == EXPR_KIND_CONSTANT)
		{
			return type_convert(ctx, n);
		}
		break;

	case EXPR_KIND_CONSTANT:
		/* Already constant */
		break;

	default:
		break;

	}

	return n;
}

/** Stores references for the other modules that are called when constant folding */
memerr _essl_constant_fold_init(constant_fold_context *ctx, mempool *pool, target_descriptor *desc, error_context *e_ctx)
{
	ctx->pool = pool;
	ctx->desc = desc;

	ctx->err_context = e_ctx;
	return MEM_OK;
}

#ifdef UNIT_TEST

void simple_test(mempool *pool, target_descriptor *desc)
{
	char *buf[] = {
		"2.0 + 7.0",
		"6.0 + 7.0 * float(3) + 8.33 - 1.0 - (8.0 - 2.0)", 
		"(vec2(6, int(8.2)) * vec2(1.0, 3.2)).x",
		"(vec2(6, int(8.2)) * vec2(1.0, 3.2)).g",
		"(vec3(1, 2, 3) * 4.0).z",
		"float(3)",	
		"-(+3)",	
		"!true",
		"-vec3(1, 2, 3).y",
		"-vec3(1, -2, 3).y",
		"vec3(42).y",
		"mat4(45)[1][1]", 
		"mat4(45)[0][0]",
		"mat4(45)[0][1]",
		"mat4(45)[2][3]",
		"mat4(45)[3][0]",
	};
	float solutions[] = {
		9.0f,
		28.33f,
		6.0f,
		25.6f,
		12.0f,
		3.0f,
		-3.0f,
		0.0f,
		-2.0f,
		2.0f,
		42.0f,
		45.0f,
		45.0f, 
		0.0f,
		0.0f,
		0.0f,
	};
	const int numbufs = sizeof(buf)/sizeof(buf[0]);
	int lengths[1];
	int i;
	char *error_log;

	for (i = 0; i < numbufs; ++i)
	{
		frontend_context *ctx;
		node *expr;
		node *folded;
		typecheck_context tctx;

		lengths[0] = strlen(buf[i]);
		
		ctx = _essl_new_frontend(pool, desc, buf[i], lengths, 1);
		assert(ctx);
		assert(_essl_typecheck_init(&tctx, pool, ctx->err_context, ctx->typestor_context, desc, ctx->lang_desc));

		expr = _essl_parse_expression(&ctx->parse_context);
		assert(expr);
		folded = _essl_typecheck(&tctx, expr, NULL);

		error_log = _essl_mempool_alloc(pool, _essl_error_get_log_size(ctx->err_context));
		assert(error_log);
		_essl_error_get_log(ctx->err_context, error_log, _essl_error_get_log_size(ctx->err_context));

		if (!(folded && folded->hdr.kind == EXPR_KIND_CONSTANT))
		{
			fprintf(stderr, "Test %d [%s]\n", i, buf[i]);
			error_log = _essl_mempool_alloc(pool, _essl_error_get_log_size(ctx->err_context));
			assert(error_log);
			_essl_error_get_log(ctx->err_context, error_log, _essl_error_get_log_size(ctx->err_context));
			fprintf(stderr, "Error log: [%ld]\n%s\n\n", (long)_essl_error_get_log_size(ctx->err_context), error_log);
			assert(0);
		}
		if (!(fabs(desc->scalar_to_float(folded->expr.u.value[0]) - solutions[i]) < 0.001))
		{
			fprintf(stderr, "Test case %d, '%s':\n", i, buf[i]);
			fprintf(stderr, "Expected %f, actual output was %f:\n", solutions[i], desc->scalar_to_float(folded->expr.u.value[0]));
			error_log = _essl_mempool_alloc(pool, _essl_error_get_log_size(ctx->err_context));
			assert(error_log);
			_essl_error_get_log(ctx->err_context, error_log, _essl_error_get_log_size(ctx->err_context));
			fprintf(stderr, "Error log: [%ld]\n%s\n\n", (long)_essl_error_get_log_size(ctx->err_context), error_log);
			assert(0);
		}
	}
}

int main(void)
{
	mempool_tracker tracker;
	mempool pool;
	compiler_options* opts;
	target_descriptor *desc;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	opts = _essl_new_compiler_options(&pool);
	desc = _essl_new_target_descriptor(&pool, TARGET_FRAGMENT_SHADER, opts);
	assert(desc);
	simple_test(&pool, desc);
	fprintf(stderr, "All tests OK!\n");
	_essl_mempool_destroy(&pool);
	return 0;
}

#endif /* UNIT_TEST */
