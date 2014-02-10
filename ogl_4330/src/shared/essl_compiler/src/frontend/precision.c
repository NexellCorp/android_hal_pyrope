/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_symbol.h"
#include "frontend/precision.h"

#define ERROR_CONTEXT ctx->err_context
#define SOURCE_OFFSET n->hdr.source_offset

memerr _essl_precision_init_context(precision_context *ctx, mempool *pool, target_descriptor *desc, typestorage_context *typestor_context, error_context *err_context)
{
	/* init the global scope */
	ctx->pool = pool;
	ctx->typestor_context = typestor_context;
	ctx->err_context = err_context;
	ctx->desc = desc;
	ESSL_CHECK(ctx->node_to_precision_dict = _essl_mempool_alloc(ctx->pool, sizeof(*ctx->node_to_precision_dict)));
	ESSL_CHECK(_essl_ptrdict_init(ctx->node_to_precision_dict, pool));

	ctx->global_scope.prec_sampler_2d = PREC_LOW;
	ctx->global_scope.prec_sampler_3d = PREC_LOW;
	ctx->global_scope.prec_sampler_cube = PREC_LOW;
	ctx->global_scope.prec_sampler_2d_shadow = PREC_LOW;
	ctx->global_scope.prec_sampler_external = PREC_LOW;
	switch(desc->kind)
	{
	case TARGET_UNKNOWN:
		assert(0);
		break;
	case TARGET_FRAGMENT_SHADER:
/*		ctx->global_scope.prec_float = PREC_HIGH; FOR TESTING*/
		ctx->global_scope.prec_float = PREC_UNKNOWN;
		ctx->global_scope.prec_int = PREC_MEDIUM;
		break;
	case TARGET_VERTEX_SHADER:
		ctx->global_scope.prec_float = PREC_HIGH;
		ctx->global_scope.prec_int = PREC_HIGH;
		break;
	}


	ctx->current_scope = &ctx->global_scope;
	return MEM_OK;

}


memerr _essl_precision_enter_scope(precision_context *ctx)
{
	precision_scope *scope;
	ESSL_CHECK(scope = _essl_mempool_alloc(ctx->pool, sizeof(*scope)));

	*scope = *ctx->current_scope;
	scope->parent_scope = ctx->current_scope;

	ctx->current_scope = scope;
	return MEM_OK;
}


void _essl_precision_leave_scope(precision_context *ctx)
{
	assert(ctx->current_scope != NULL && ctx->current_scope->parent_scope != NULL);

	ctx->current_scope = ctx->current_scope->parent_scope;

}


static precision_qualifier get_default_precision_for_type(precision_context *ctx, const type_specifier* ts)
{
	precision_scope *scope = ctx->current_scope;

	/* else return the default for that basic type */
	switch(_essl_get_nonderived_basic_type(ts))
	{
		case TYPE_FLOAT:
			return scope->prec_float;
		case TYPE_INT:
			return scope->prec_int;
		case TYPE_SAMPLER_2D:
			return scope->prec_sampler_2d;
		case TYPE_SAMPLER_3D:
			return scope->prec_sampler_3d;
		case TYPE_SAMPLER_CUBE:
			return scope->prec_sampler_cube;
		case TYPE_SAMPLER_2D_SHADOW:
			return scope->prec_sampler_2d_shadow;
		case TYPE_SAMPLER_EXTERNAL:
			return scope->prec_sampler_external;
		case TYPE_BOOL:
			return PREC_MEDIUM;
		
		default:
			return PREC_UNKNOWN;
	}
}

static essl_bool type_has_precision_qualification(const type_specifier *t)
{
	type_basic basic_type = _essl_get_nonderived_basic_type(t);
	switch(basic_type)
	{
		case TYPE_FLOAT:
		case TYPE_INT:
		case TYPE_SAMPLER_2D:
		case TYPE_SAMPLER_3D:
		case TYPE_SAMPLER_CUBE:
		case TYPE_SAMPLER_2D_SHADOW:
		case TYPE_SAMPLER_EXTERNAL:
		case TYPE_BOOL:
			return ESSL_TRUE;

		default:
			return ESSL_FALSE;
	}
}

static precision_qualifier get_precision_qualifier_for_node(precision_context *ctx, node *n)
{
	void *v;
	v = _essl_ptrdict_lookup(ctx->node_to_precision_dict, n);
	if(v == NULL) return PREC_UNKNOWN;
	return (precision_qualifier)(long)v;
}

static memerr set_precision_qualifier_for_node(precision_context *ctx, node *n, precision_qualifier pq)
{
	if(type_has_precision_qualification(n->hdr.type))
	{
		void *v = (void *)(long)pq;
		scalar_size_specifier sz = ctx->desc->get_size_for_type_and_precision;
		ESSL_CHECK(_essl_ptrdict_insert(ctx->node_to_precision_dict, n, v));
		ESSL_CHECK(n->hdr.type = _essl_get_type_with_given_size(ctx->typestor_context, n->hdr.type, sz));

	}
	return MEM_OK;
}


/** propagate the precision upward to the children */
static memerr propagate_precision_upward(precision_context *ctx, node *n, precision_qualifier prec)
{
	unsigned i;
	unsigned num_children = GET_N_CHILDREN(n);
	if(GET_NODE_KIND(n->hdr.kind) != NODE_KIND_EXPRESSION && GET_NODE_KIND(n->hdr.kind) != NODE_KIND_DECLARATION) return MEM_OK;
	if(n->hdr.type == NULL) return MEM_OK;
	if(n->hdr.kind == EXPR_KIND_FUNCTION_CALL) return MEM_OK;
	if(n->hdr.kind == EXPR_KIND_STRUCT_CONSTRUCTOR) return MEM_OK;

	for(i = 0; i < num_children; ++i)
	{

		node *child = GET_CHILD(n, i);
		if(child != NULL)
		{
			essl_bool propagate_upward = !type_has_precision_qualification(child->hdr.type);

			if(PREC_UNKNOWN == get_precision_qualifier_for_node(ctx, child))
			{
				ESSL_CHECK(set_precision_qualifier_for_node(ctx, child, prec));
				propagate_upward = ESSL_TRUE;
			}
			if(propagate_upward)
			{
				ESSL_CHECK(propagate_precision_upward(ctx, child, prec));
			}

		}
	}
	return MEM_OK;
}

static memerr propagate_default_precision_upward(precision_context *ctx, node *n)
{
	unsigned i;
	unsigned num_children = GET_N_CHILDREN(n);
	if(GET_NODE_KIND(n->hdr.kind) != NODE_KIND_EXPRESSION && GET_NODE_KIND(n->hdr.kind) != NODE_KIND_DECLARATION) return MEM_OK;
	if(n->hdr.type == NULL) return MEM_OK;
	if(n->hdr.kind == EXPR_KIND_FUNCTION_CALL) return MEM_OK;
	if(n->hdr.kind == EXPR_KIND_STRUCT_CONSTRUCTOR) return MEM_OK;

	if(type_has_precision_qualification(n->hdr.type))
	{
		if(PREC_UNKNOWN == get_precision_qualifier_for_node(ctx, n))
		{
			precision_qualifier pq = get_default_precision_for_type(ctx, n->hdr.type);
			if(pq == PREC_UNKNOWN && n->hdr.kind != EXPR_KIND_CONSTANT)
			{
				REPORT_ERROR(ctx->err_context, ERR_SEM_FLOAT_INT_NO_DEFAULT_PRECISION, n->hdr.source_offset, "no default precision defined for expression\n");
			} else {
				ESSL_CHECK(set_precision_qualifier_for_node(ctx, n, pq));
				ESSL_CHECK(propagate_precision_upward(ctx, n, pq));
				
			}
			
		}
	} else {
		/* look for other children that may be lacking a precision qualifier */
		for(i = 0; i < num_children; ++i)
		{
			
			node *child = GET_CHILD(n, i);
			if(child != NULL)
			{
				ESSL_CHECK(propagate_default_precision_upward(ctx, child));
			}
		}
	}
	return MEM_OK;
}

static unsigned n_children_for_downwards_propagation(const node *n)
{
	if(n->hdr.kind == EXPR_KIND_BINARY && n->expr.operation == EXPR_OP_INDEX) return 1;
	if(n->hdr.kind == EXPR_KIND_ASSIGN) return 1;
	if(_essl_node_is_texture_operation(n)) return 1;
	return GET_N_CHILDREN(n);

}

static const type_specifier *get_type_with_set_precision(precision_context *ctx, node *n, const type_specifier *t,qualifier_set *qual, const char *expr_name)
{
	/* if there's not a precision qualifier then set the precision to the default */
	precision_qualifier pq = PREC_UNKNOWN;
	scalar_size_specifier sz;
	type_specifier *nt;
	
	if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *sd;
		ESSL_CHECK(nt = _essl_clone_type(ctx->pool, t));
		for(sd = nt->members; sd != NULL; sd = sd->next)
		{
			ESSL_CHECK_NONFATAL(sd->type = get_type_with_set_precision(ctx, n, sd->type, &sd->qualifier, "member"));
		}
		t = nt;
	} else if(t->child_type != NULL)
	{
		ESSL_CHECK(nt = _essl_clone_type(ctx->pool, t));
		ESSL_CHECK_NONFATAL(nt->child_type = get_type_with_set_precision(ctx, n, t->child_type, qual, expr_name));
		t = nt;
	} else {
		if(qual != NULL)
		{
			pq = qual->precision;
		}
		if(pq == PREC_UNKNOWN)
		{
			pq = get_default_precision_for_type(ctx, t);
		}
		/* if the precision is not resolved then report an error */
		if(PREC_UNKNOWN == pq && type_has_precision_qualification(t))
		{
			ERROR_CODE_PARAM(ERR_SEM_FLOAT_INT_NO_DEFAULT_PRECISION, "no default precision defined for %s\n", expr_name);
			return 0;
		}

		sz = ctx->desc->get_size_for_type_and_precision;
		ESSL_CHECK(t = _essl_get_type_with_given_size(ctx->typestor_context, t, sz));
		if(qual != NULL)
		{
			qual->precision = pq;
		}
	}


	return t;
}




memerr _essl_precision_visit_single_node(precision_context *ctx, node *n)
{
	precision_scope *scope = ctx->current_scope;
	essl_bool want_final_propagation = ESSL_FALSE;
	if(GET_NODE_KIND(n->hdr.kind) == NODE_KIND_DECLARATION)
	{
		want_final_propagation = ESSL_TRUE;
		switch(n->hdr.kind)
		{
		case DECL_KIND_PRECISION:
			switch(n->decl.prec_type)
			{
			case TYPE_FLOAT:
				scope->prec_float = n->decl.prec_decl;
				break;
			case TYPE_INT:
				scope->prec_int = n->decl.prec_decl;
				break;
			case TYPE_SAMPLER_2D:
				scope->prec_sampler_2d = n->decl.prec_decl;
				break;
			case TYPE_SAMPLER_CUBE:
				scope->prec_sampler_cube = n->decl.prec_decl;
				break;
			case TYPE_SAMPLER_3D:
				scope->prec_sampler_3d = n->decl.prec_decl;
				break;
			case TYPE_SAMPLER_2D_SHADOW:
				scope->prec_sampler_2d_shadow = n->decl.prec_decl;
				break;
			case TYPE_SAMPLER_EXTERNAL:
				scope->prec_sampler_external = n->decl.prec_decl;
				break;
			default:
				assert(0);
			}
			break;

		case DECL_KIND_VARIABLE:
			want_final_propagation = ESSL_FALSE;
			ESSL_CHECK_NONFATAL(n->decl.sym->type = get_type_with_set_precision(ctx, n, n->decl.sym->type, &n->decl.sym->qualifier, "variable"));
			n->hdr.type = n->decl.sym->type;
			ESSL_CHECK(set_precision_qualifier_for_node(ctx, n, n->decl.sym->qualifier.precision));
			ESSL_CHECK(propagate_precision_upward(ctx, n, n->decl.sym->qualifier.precision));
			break;

		case DECL_KIND_FUNCTION:
			ESSL_CHECK_NONFATAL(n->decl.sym->type = get_type_with_set_precision(ctx, n, n->decl.sym->type, &n->decl.sym->qualifier, "return value"));
			{
				single_declarator *parm;
				for(parm = n->decl.sym->parameters; parm != NULL; parm = parm->next)
				{
					ESSL_CHECK_NONFATAL(parm->type = get_type_with_set_precision(ctx, n, parm->type, &parm->qualifier, "parameter"));
					if(parm->sym)
					{
						parm->sym->type = parm->type;
						parm->sym->qualifier = parm->qualifier;
					}
				}
			}
			break;

		default:
			assert(0);
			break;
		}
	} else if(GET_NODE_KIND(n->hdr.kind) == NODE_KIND_EXPRESSION)
	{
		switch(n->hdr.kind)
		{
			/* function call and struct constructor parameters get precisions propagated from the types they reference */
		case EXPR_KIND_FUNCTION_CALL:
			{
				symbol *fun = n->expr.u.fun.sym;
				unsigned i;
				single_declarator *funparm;

				ESSL_CHECK(set_precision_qualifier_for_node(ctx, n, fun->qualifier.precision));


				for(i = 0, funparm = fun->parameters ; i < GET_N_CHILDREN(n) && funparm != NULL; ++i, funparm = funparm->next)
				{
					node* child = GET_CHILD(n, i);
					if(child != NULL && GET_NODE_KIND(child->hdr.kind) == NODE_KIND_EXPRESSION)
					{
						if(type_has_precision_qualification(child->hdr.type) && PREC_UNKNOWN == get_precision_qualifier_for_node(ctx, child))
						{
							precision_qualifier pq = funparm->qualifier.precision;
							ESSL_CHECK(set_precision_qualifier_for_node(ctx, child, pq));
							ESSL_CHECK(propagate_precision_upward(ctx, child, pq));
						}
					}
				}
				want_final_propagation = ESSL_TRUE;
			}
			break;

		case EXPR_KIND_STRUCT_CONSTRUCTOR:
			{
				unsigned i;
				single_declarator *parm;

				ESSL_CHECK_NONFATAL(n->hdr.type = get_type_with_set_precision(ctx, n, n->hdr.type, NULL, "struct"));
				
				parm = n->hdr.type->members;
				for(i = 0, parm = n->hdr.type->members; i < GET_N_CHILDREN(n) && parm != NULL; ++i, parm = parm->next)
				{
					node* child = GET_CHILD(n, i);
					if(child != NULL && GET_NODE_KIND(child->hdr.kind) == NODE_KIND_EXPRESSION)
					{
						if(type_has_precision_qualification(child->hdr.type) && PREC_UNKNOWN == get_precision_qualifier_for_node(ctx, child))
						{
							precision_qualifier pq = parm->qualifier.precision;
							ESSL_CHECK(set_precision_qualifier_for_node(ctx, child, pq));
							ESSL_CHECK(propagate_precision_upward(ctx, child, pq));
						}
					}
				}
				want_final_propagation = ESSL_TRUE;
			}
			break;

			/* lvalues get their precision qualifiers from the variables they reference */
		case EXPR_KIND_VARIABLE_REFERENCE:
			ESSL_CHECK(set_precision_qualifier_for_node(ctx, n, n->expr.u.sym->qualifier.precision));
			break;
		case EXPR_KIND_UNARY:
			if(n->expr.operation == EXPR_OP_MEMBER)
			{
				ESSL_CHECK(set_precision_qualifier_for_node(ctx, n, n->expr.u.member->qualifier.precision));

			}
			break;

		default:
			break;
		}

		if(n->hdr.kind != EXPR_KIND_FUNCTION_CALL)
		{
			/* loop through each of the children to determine the highest precision
			   then assign this to the constructor */
			precision_qualifier pq = get_precision_qualifier_for_node(ctx, n);
			essl_bool seen_lowp_int = ESSL_FALSE;
			
			if(pq == PREC_UNKNOWN)
			{
				unsigned int i;
				unsigned n_children_down = n_children_for_downwards_propagation(n);
				for(i = 0 ; i < n_children_down; ++i)
				{
					node* child = GET_CHILD(n, i);
					if(child != NULL)
					{
						type_basic bt = child->hdr.type->basic_type;
						precision_qualifier child_qual = get_precision_qualifier_for_node(ctx, child);
						if(child_qual > pq)
						{
							pq = child_qual;
						}

						if(child_qual == PREC_LOW && bt == TYPE_INT)
						{
							seen_lowp_int = ESSL_TRUE;
						}
					}
				}
			
				/* lowp ints cannot be represented as lowp floats so if we have a float constructor
				   containing a lowp int then have to ensure that the constructed value is a
				   mediump float */
				if(pq == PREC_LOW && seen_lowp_int && (n->hdr.type->basic_type == TYPE_FLOAT || n->hdr.type->basic_type == TYPE_MATRIX_OF))
				{
					pq = PREC_MEDIUM;
				}

				if(pq != PREC_UNKNOWN)
				{
					ESSL_CHECK(set_precision_qualifier_for_node(ctx, n, pq));
				}

			}
			

			pq = get_precision_qualifier_for_node(ctx, n);
			if(pq != PREC_UNKNOWN)
			{
				ESSL_CHECK(propagate_precision_upward(ctx, n, pq));
			}

		}
	
	} else {
		want_final_propagation = ESSL_TRUE;
	}


	/* final propagation upwards */
	if(want_final_propagation)
	{
		unsigned i;
		for(i = 0 ; i < GET_N_CHILDREN(n); ++i)
		{
			node* child = GET_CHILD(n, i);
			if(child != NULL)
			{
				ESSL_CHECK(propagate_default_precision_upward(ctx, child));
			}
		}
	}

	return MEM_OK;
}

static memerr calculate_precision(precision_context *ctx, node *n)
{
	unsigned i;

	essl_bool has_child_scope = (GET_NODE_KIND(n->hdr.kind) == NODE_KIND_STATEMENT ||
								 GET_NODE_KIND(n->hdr.kind) == NODE_KIND_TRANSLATION_UNIT) && n->stmt.child_scope != NULL;

	if(n->hdr.kind == DECL_KIND_FUNCTION)
	{
		ESSL_CHECK_NONFATAL(_essl_precision_visit_single_node(ctx, n));
	}

	if(has_child_scope)
	{
		ESSL_CHECK(_essl_precision_enter_scope(ctx));
	}

	for(i = 0; i < GET_N_CHILDREN(n); ++i)
	{
		node *child = GET_CHILD(n, i);
		if(child)
		{
			ESSL_CHECK_NONFATAL(calculate_precision(ctx, child));
			SET_CHILD(n, i, child);
		}
	}

	if(has_child_scope)
	{
		_essl_precision_leave_scope(ctx);
	}

	if(n->hdr.kind != DECL_KIND_FUNCTION)
	{
		ESSL_CHECK_NONFATAL(_essl_precision_visit_single_node(ctx, n));
	}

	return MEM_OK;
}

static node *new_type_conversion(precision_context *ctx, node *n)
{
	if(_essl_get_nonderived_basic_type(n->hdr.type) != n->hdr.type->basic_type)
	{
		node *cast;
		assert(n->hdr.type->basic_type == TYPE_MATRIX_OF);
		ESSL_CHECK(cast = _essl_new_builtin_constructor_expression(ctx->pool, 1));
		SET_CHILD(cast, 0, n);
		return cast;
		
	} else {
		return _essl_new_type_convert_expression(ctx->pool, EXPR_OP_IMPLICIT_TYPE_CONVERT, n);
	}
}

static memerr insert_bitwise_casts_for_children_with_specific_type(precision_context *ctx, node *n, unsigned start_child, unsigned one_past_last_child, scalar_size_specifier n_sz, type_basic n_basic_type)
{
	unsigned i;
	IGNORE_PARAM(n_basic_type);
	assert(one_past_last_child <= GET_N_CHILDREN(n));
	assert(start_child <= one_past_last_child);
	for(i = start_child; i < one_past_last_child; ++i)
	{
		node *child = GET_CHILD(n, i);
		if(child != NULL)
		{
			assert(n_basic_type == _essl_get_nonderived_basic_type(child->hdr.type));
			if(type_has_precision_qualification(child->hdr.type))
			{
				scalar_size_specifier child_sz = _essl_get_scalar_size_for_type(child->hdr.type);
				if(child_sz != n_sz)
				{
					node *cast;
					ESSL_CHECK(cast = new_type_conversion(ctx, child));
					_essl_ensure_compatible_node(cast, n);
					ESSL_CHECK(cast->hdr.type = _essl_get_type_with_given_size(ctx->typestor_context, child->hdr.type, n_sz));
					SET_CHILD(n, i, cast);
				}
			}
		}
	}
	return MEM_OK;
}

static memerr insert_bitwise_casts_for_children(precision_context *ctx, node *n, unsigned start_child, unsigned one_past_last_child)
{
	if(!type_has_precision_qualification(n->hdr.type)) return MEM_OK;
	return insert_bitwise_casts_for_children_with_specific_type(ctx, n, start_child, one_past_last_child, _essl_get_scalar_size_for_type(n->hdr.type), _essl_get_nonderived_basic_type(n->hdr.type));
}

static node *insert_bitwise_casts_single_node(precision_context *ctx, node *n)
{
	switch(n->hdr.kind)
	{
		/* nothing to do for these */
	case EXPR_KIND_VARIABLE_REFERENCE:
	case EXPR_KIND_CONSTANT:
	case EXPR_KIND_TYPE_CONVERT:
		return n;

	case EXPR_KIND_STRUCT_CONSTRUCTOR:
	{
		unsigned i;
		single_declarator *structmemb = n->hdr.type->members;

		for(i = 0, structmemb = n->hdr.type->members ; i < GET_N_CHILDREN(n) && structmemb != NULL; ++i, structmemb = structmemb->next)
		{
			if(type_has_precision_qualification(structmemb->type))
			{
				ESSL_CHECK(insert_bitwise_casts_for_children_with_specific_type(ctx, n, i, i+1, _essl_get_scalar_size_for_type(structmemb->type), _essl_get_nonderived_basic_type(structmemb->type)));
			}
		}
		return n;
	}

	case EXPR_KIND_FUNCTION_CALL:
	{
		symbol *fun = n->expr.u.fun.sym;
		unsigned i;
		single_declarator *funparm;



		for(i = 0, funparm = fun->parameters ; i < GET_N_CHILDREN(n) && funparm != NULL; ++i, funparm = funparm->next)
		{
			if(type_has_precision_qualification(funparm->type))
			{
				ESSL_CHECK(insert_bitwise_casts_for_children_with_specific_type(ctx, n, i, i+1, _essl_get_scalar_size_for_type(funparm->type), _essl_get_nonderived_basic_type(funparm->type)));
			}
		}
		return n;
	}

	case EXPR_KIND_BUILTIN_CONSTRUCTOR:
		/* will be handled later. could be handled now */
		return n;

	case DECL_KIND_VARIABLE:
		if(GET_CHILD(n, 0) != NULL)
		{
			ESSL_CHECK(insert_bitwise_casts_for_children(ctx, n, 0, 1));
		}
		return n;

	case STMT_KIND_RETURN:
		n->hdr.type = ctx->curr_function_return_type;
		if(GET_CHILD(n, 0) != NULL)
		{
			ESSL_CHECK(insert_bitwise_casts_for_children(ctx, n, 0, 1));
		}
		return n;

	case EXPR_KIND_TERNARY:
		ESSL_CHECK(insert_bitwise_casts_for_children(ctx, n, 1, 3));
		return n;
	case EXPR_KIND_VECTOR_COMBINE:
		ESSL_CHECK(insert_bitwise_casts_for_children(ctx, n, 0, GET_N_CHILDREN(n)));
		return n;


	case EXPR_KIND_UNARY:
	case EXPR_KIND_BINARY:
	case EXPR_KIND_ASSIGN:
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		if(_essl_node_is_texture_operation(n))
		{
			/* nothing to do */
			return n;
		}

		if(n->expr.operation == EXPR_OP_INDEX)
		{
			if(GET_CHILD(n, 0)->hdr.type->basic_type != TYPE_ARRAY_OF)
			{
				ESSL_CHECK(insert_bitwise_casts_for_children(ctx, n, 0, 1));
			}
			return n;
		}

		switch(n->expr.operation)
		{
		case EXPR_OP_MEMBER:
		case EXPR_OP_CHAIN:
			return n;
		case EXPR_OP_LT:
		case EXPR_OP_LE:
		case EXPR_OP_EQ:
		case EXPR_OP_NE:
		case EXPR_OP_GE:
		case EXPR_OP_GT:
		case EXPR_OP_FUN_LESSTHAN:
		case EXPR_OP_FUN_LESSTHANEQUAL:
		case EXPR_OP_FUN_GREATERTHAN:
		case EXPR_OP_FUN_GREATERTHANEQUAL:
		case EXPR_OP_FUN_EQUAL:
		case EXPR_OP_FUN_NOTEQUAL:
		{
			const type_specifier *comparison_type = GET_CHILD(n, 0)->hdr.type;
			scalar_size_specifier sz;
			precision_qualifier pq;
			if(!type_has_precision_qualification(comparison_type)) return n;
			if(comparison_type->basic_type == TYPE_MATRIX_OF) return n; /* will be resolved by eliminate_complex_ops */
			pq = get_precision_qualifier_for_node(ctx, n);
			DO_ASSERT(pq != PREC_UNKNOWN);
			sz = ctx->desc->get_size_for_type_and_precision;
			assert(sz != SIZE_UNKNOWN);
			ESSL_CHECK(insert_bitwise_casts_for_children_with_specific_type(ctx, n, 0, GET_N_CHILDREN(n), sz, _essl_get_nonderived_basic_type(comparison_type)));
			if(sz != _essl_get_scalar_size_for_type(n->hdr.type))
			{
				node *cast;
				const type_specifier *t;
				ESSL_CHECK(cast = new_type_conversion(ctx, n));
				_essl_ensure_compatible_node(cast, n);
				/* okay, now just fix up the type of n so we match the type of the comparison input */
				ESSL_CHECK(t = _essl_get_type_with_given_size(ctx->typestor_context, n->hdr.type, sz));
				n->hdr.type = t;
				return cast;

			}
			return n;
		}
		default:
			ESSL_CHECK(insert_bitwise_casts_for_children(ctx, n, 0, GET_N_CHILDREN(n)));
			return n;
		}
	default:
		assert(0);
	}
	return n;
}

static node *insert_bitwise_casts(precision_context *ctx, node *n)
{
	unsigned i;
	if(n->hdr.kind == DECL_KIND_FUNCTION)
	{
		assert(ctx->curr_function_return_type == NULL);
		assert(n->decl.sym->type != NULL);
		ctx->curr_function_return_type = n->decl.sym->type;
	}

	for(i = 0; i < GET_N_CHILDREN(n); ++i)
	{
		node *child = GET_CHILD(n, i);
		if(child)
		{
			ESSL_CHECK(child = insert_bitwise_casts(ctx, child));
			SET_CHILD(n, i, child);
		}
	}

	if(GET_NODE_KIND(n->hdr.kind) == NODE_KIND_EXPRESSION || n->hdr.kind == DECL_KIND_VARIABLE || n->hdr.kind == STMT_KIND_RETURN)
	{
		ESSL_CHECK(n = insert_bitwise_casts_single_node(ctx, n));
	}

	if(n->hdr.kind == DECL_KIND_FUNCTION)
	{
		assert(ctx->curr_function_return_type == n->decl.sym->type);
		ctx->curr_function_return_type = NULL;
	}


	return n;
}



memerr _essl_calculate_precision(precision_context *ctx, node *root)
{
	ESSL_CHECK_NONFATAL(calculate_precision(ctx, root));
	if(ctx->desc->has_multi_precision)
	{
		ESSL_CHECK_NONFATAL(insert_bitwise_casts(ctx, root));
	}
	return MEM_OK;
}
