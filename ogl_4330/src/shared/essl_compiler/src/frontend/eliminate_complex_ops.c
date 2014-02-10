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
#include "frontend/eliminate_complex_ops.h"
#include "frontend/make_basic_blocks.h"
#include "frontend/callgraph.h"
#include "common/unique_names.h"


/** Context class used during struct elimination.
 */
typedef struct {
	mempool *pool;						/*!< Pointer to the memory pool to use during the process */
	translation_unit *tu;
	typestorage_context *typestor_ctx;
	ptrdict visited;
	ptrdict assigns;
} eliminate_complex_ops_context;


/** Context class used during struct elimination.
 */
typedef struct {
	mempool *pool;						/*!< Pointer to the memory pool to use during the process */
	unique_name_context temp_names;		/*!< Unique name generator for temporary return values from functions */
} eliminate_complex_returns_context;



/*@null@*/ static node *create_index_int_constant(eliminate_complex_ops_context *ctx, int value)
{
	node *n;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, 1));
	n->expr.u.value[0] = ctx->tu->desc->int_to_scalar(value);

	ESSL_CHECK(n->hdr.type = _essl_get_type_with_default_size_for_target(ctx->typestor_ctx, TYPE_INT, 1, ctx->tu->desc));
	return n;
}


/*@null@*/ static node *process_single_node(eliminate_complex_ops_context *ctx, node *n);

/** \brief Rewrite all calls to a function that used to return a struct. Create a temporary variable for the new 'out' parameter,
 *  fixup the argument list to pass it, then wrap it all up in a 'chain' with a reference to the temp as the last value.
 *
 *	\param[in]		ctx				Context struct
 *	\param[in]		call_from		Pointer to the head of the callers list
 *	\param[in]		type			Pointer to a type_specifier identifying the previous return type, now the type of the
 *									'out' parameter
 *	\return							Memory error or ok
 */
static memerr internalize_struct_argument(eliminate_complex_returns_context* ctx, call_graph* call_from, type_specifier* type, qualifier_set return_qual)
{
	call_node* caller;
	node* top_compound = call_from->func->body;
	const char* tempname;
	
	/* Traverse across all callers and fixup the argument list */
	caller = call_from->callers;
	while(caller != 0)
	{
		node* callnode = caller->caller;
		node* chain;
		node* call_clone;
		node* variable_ref;
		node* variable_def;
		symbol* temp;
		string name;
		int nchildren;

		ESSL_CHECK(tempname = _essl_unique_name_get_or_create(&ctx->temp_names, caller));
		name = _essl_cstring_to_string(ctx->pool, tempname);
		ESSL_CHECK(name.ptr);
		ESSL_CHECK(temp = _essl_new_variable_symbol(ctx->pool, name, type, return_qual, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));
		temp->opt.is_indexed_statically = ESSL_FALSE;
		
		ESSL_CHECK(variable_ref = _essl_new_variable_reference_expression(ctx->pool, temp));
		variable_ref->hdr.type = temp->type;
		ESSL_CHECK(APPEND_CHILD(callnode, variable_ref, ctx->pool));
		
		/* Now wrap the function into a 'chain' with the temp as the last parameter.
		 * This replaces the return value in all possible uses.
		 */
		ESSL_CHECK(call_clone = _essl_clone_node(ctx->pool, callnode));
		ESSL_CHECK(chain = _essl_new_binary_expression(ctx->pool, call_clone, EXPR_OP_CHAIN, variable_ref));
		chain->hdr.type = variable_ref->hdr.type;
		*callnode = *chain;
		
		/* Add the temporary variable definition to the head of the function */
		ESSL_CHECK(variable_def = _essl_new_variable_declaration(ctx->pool, temp, 0));
		variable_def->hdr.type = type;
		nchildren = GET_N_CHILDREN(top_compound);
		ESSL_CHECK(SET_N_CHILDREN(top_compound, nchildren+1, ctx->pool));
		while(nchildren-- > 0)
			SET_CHILD(top_compound, nchildren+1, GET_CHILD(top_compound, nchildren));
		SET_CHILD(top_compound, 0, variable_def);
		
		caller = caller->next;
	}

	return MEM_OK;
}

/** \brief Process a function that used to return a struct so that the return value is now copied into the new
 *  'out' parameter, and the return has no argument.
 *
 *	\param[in]		ctx				Context struct
 *	\param[in]		root			Pointer to the root node of the function body
 *	\param[in]		variable_ref	Pointer to the node representing the 'out' variable for the return value
 *	\return							Memory error or ok
 */
static memerr replace_returns(eliminate_complex_returns_context* ctx, node* root, node* variable_ref)
{
	int nchildren, child;
	
	if(root->hdr.kind == STMT_KIND_RETURN && GET_N_CHILDREN(root) == 1)
	{
		node* assign;
		node* compound;
		node* ret_clone;
		
		/* Create a new compound to contain the assignment and rewritten return */
		ESSL_CHECK(compound = _essl_new_compound_statement(ctx->pool));
		/* Create a new assign from the original 'return' argument, to the specified 'out' parameter. */
		ESSL_CHECK(assign = _essl_new_assign_expression(ctx->pool, variable_ref, EXPR_OP_ASSIGN, GET_CHILD(root, 0)));
		assign->hdr.type = variable_ref->hdr.type;
		ESSL_CHECK(APPEND_CHILD(compound, assign, ctx->pool));
		/* Clear the child of the return, no return value anymore */
		SET_CHILD(root, 0, 0);
		ESSL_CHECK(ret_clone = _essl_clone_node(ctx->pool, root));
		ESSL_CHECK(APPEND_CHILD(compound, ret_clone, ctx->pool));
		/* Rewrite the original return with the new compound */
		*root = *compound;
	}
	else
	{
		/* Recursively process the whole function */
		nchildren = GET_N_CHILDREN(root);
		for(child = 0; child < nchildren; ++child)
		{
			node* next = GET_CHILD(root, child);
			if(next != 0)
			{
				ESSL_CHECK(replace_returns(ctx, next, variable_ref));
			}
		}
	}
	return MEM_OK;
}


/** \brief Identify all functions in a translation unit that return a struct and rewrite them to
 *  return void and take an additional 'out' parameter for the result.
 * 
 *	\param[in]		ctx				Context struct
 *	\param[in]		tu				Pointer to the translation unit to process
 *	\return							Memory error or ok
 */
static memerr replace_returned_structs(eliminate_complex_returns_context *ctx, translation_unit* tu )
{
	symbol_list* function;
	type_specifier* return_type;
	type_specifier* new_type;
	single_declarator* out_val;
	string out_val_name;
	call_graph* call_from;
	
	/* Look for functions that return a struct */
	for(function = tu->functions; function != 0; function = function->next)
	{
		/* Check if the function returns a struct */
		if(function->sym->type->basic_type == TYPE_STRUCT || function->sym->type->basic_type == TYPE_MATRIX_OF)
		{
			node* variable_ref;
			symbol* retval;
			qualifier_set qual;
			
			
			/* Rewrite it to return void... */
			ESSL_CHECK(new_type = _essl_clone_type(ctx->pool, function->sym->type));

			ESSL_CHECK(return_type = _essl_new_type(ctx->pool));
			return_type->basic_type = TYPE_VOID;
			function->sym->type = return_type;

			qual = function->sym->qualifier;
			qual.direction = DIR_OUT;

			_essl_init_qualifier_set(&function->sym->qualifier);

			/* ...and add an extra out param for the struct */
			out_val_name = _essl_cstring_to_string(ctx->pool, "%retval");
			ESSL_CHECK(out_val_name.ptr);
			ESSL_CHECK(retval = _essl_new_variable_symbol(ctx->pool, out_val_name, new_type, qual, SCOPE_FORMAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));
			retval->opt.is_indexed_statically = ESSL_FALSE;
			ESSL_CHECK(out_val = _essl_new_single_declarator(ctx->pool, new_type, qual, &out_val_name, NULL, UNKNOWN_SOURCE_OFFSET));
			out_val->sym = retval;
			
			LIST_INSERT_BACK(&function->sym->parameters, out_val);
			
			/* Scan through the function body, and replace all STMT_KIND_RETURN nodes with
			 * assignments to the extra 'out' parameter.
			 */
			ESSL_CHECK(variable_ref = _essl_new_variable_reference_expression(ctx->pool, retval));
			variable_ref->hdr.type = retval->type;
			
			ESSL_CHECK(replace_returns(ctx, function->sym->body, variable_ref));
			
			/* Now need to fixup all the callers */
			for(call_from = function->sym->calls_from; call_from != 0; call_from = call_from->next)
			{
				ESSL_CHECK(internalize_struct_argument(ctx, call_from, new_type, qual));
			}
		}
	}
	
	return MEM_OK;
}

/** \brief Replace a struct comparison (recursively) with individual member comparisons, joined by the 
 *  appropriate logic ops depending on the comparison type.
 *
 *  \param[in]		ctx			Context structure
 *	\param[in]		left		Pointer to the node representing the left side of the comparison
 *	\param[in]		right		Pointer to the node representing the right side of the comparison
 *	\param[in]		comparator	Pointer to the node representing the comparison, initially the original 
 *								comparison node.
 *	\param[in]		member		Pointer to the declarator representing the member list of the struct being exploded
 *	\return						New node on success, 0 on failure
 */
/*@null@*/ static node *explode_struct_comparison(eliminate_complex_ops_context *ctx, node* left, node* right, node* comparator, single_declarator* member)
{
	node* current = 0;
	
	for(; member != 0; member = member->next)
	{
		/* Create member accessors for the left and right side members */
		node *left_accessor, *right_accessor, *newcomparator;
		ESSL_CHECK(left_accessor = _essl_new_unary_expression(ctx->pool, EXPR_OP_MEMBER, left));
		ESSL_CHECK(right_accessor = _essl_new_unary_expression(ctx->pool, EXPR_OP_MEMBER, right));
		left_accessor->expr.u.member = member;
		right_accessor->expr.u.member = member;
		left_accessor->hdr.type = member->type;
		right_accessor->hdr.type = member->type;
		if(member->type->basic_type == TYPE_STRUCT)
		{
			ESSL_CHECK(newcomparator = explode_struct_comparison(ctx, left_accessor, right_accessor, comparator, member->type->members));
		}
		else
		{
			/* clone the comparator, as each member comparison needs its own node */
			scalar_size_specifier sz;
			const type_specifier *comparison_type = left_accessor->hdr.type;
			ESSL_CHECK(newcomparator = _essl_clone_node(ctx->pool, comparator));
			SET_CHILD(newcomparator, 0, left_accessor);
			SET_CHILD(newcomparator, 1, right_accessor);

			sz = _essl_get_scalar_size_for_type(comparison_type);
			assert(sz != SIZE_UNKNOWN);
			if(sz != _essl_get_scalar_size_for_type(newcomparator ->hdr.type))
			{
				node *cast;
				const type_specifier *t;
				ESSL_CHECK(cast = _essl_new_type_convert_expression(ctx->pool, EXPR_OP_IMPLICIT_TYPE_CONVERT, newcomparator));
				_essl_ensure_compatible_node(cast, newcomparator);
				/* okay, now just fix up the type of n so we match the type of the comparison input */
				ESSL_CHECK(t = _essl_get_type_with_given_size(ctx->typestor_ctx, newcomparator->hdr.type, sz));
				newcomparator->hdr.type = t;

				newcomparator = cast;
			}
				
		}
		/* If this is not the first member, create a logic operation of the appropriate type
		 * and link the previous comparator, and this new one under it. Making the 'current' 
		 * operation the new logicop.
		 * If the original comparator is NE, the 'joining' logic op is OR, otherwise it is AND.
		 */
		ESSL_CHECK(newcomparator = process_single_node(ctx, newcomparator));
		if(0 != current)
		{
			node* logicop;
			expression_operator logic_type = EXPR_OP_LOGICAL_AND;
			if(comparator->expr.operation == EXPR_OP_NE)
				logic_type = EXPR_OP_LOGICAL_OR;
			ESSL_CHECK(logicop = _essl_new_binary_expression(ctx->pool, current, logic_type, newcomparator));
			_essl_ensure_compatible_node(logicop, comparator);	
			ESSL_CHECK(logicop = process_single_node(ctx, logicop));
			current = logicop;
		}
		else
		{
		/* otherwise, the 'current' is the new comparator, which should take care of the case of
		 * a struct with a single member
		 */
			current = newcomparator;
		}
	}
	
	return current;
}


/** \brief Replace a matrix comparison (recursively) with individual column comparisons, joined by the 
 *  appropriate logic ops depending on the comparison type.
 *
 *  \param[in]		ctx			Context structure
 *	\param[in]		left		Pointer to the node representing the left side of the comparison
 *	\param[in]		right		Pointer to the node representing the right side of the comparison
 *	\param[in]		comparator	Pointer to the node representing the comparison, initially the original 
 *								comparison node.
 *	\return						New node on success, 0 on failure
 */
/*@null@*/ static node *explode_matrix_comparison(eliminate_complex_ops_context *ctx, node* n)
{
	node* current = 0;
	unsigned i;
	unsigned len;
	node *A;
	node *B;
	ESSL_CHECK(A = GET_CHILD(n, 0));
	ESSL_CHECK(B = GET_CHILD(n, 1));
	len = GET_MATRIX_N_COLUMNS(A->hdr.type);
	assert(A->hdr.type->basic_type == TYPE_MATRIX_OF && GET_MATRIX_N_COLUMNS(A->hdr.type) == len);
	assert(B->hdr.type->basic_type == TYPE_MATRIX_OF && GET_MATRIX_N_COLUMNS(B->hdr.type) == len);
	for(i = 0; i < len; ++i)
	{
		node *num, *A_index, *B_index;
		node *newcomparator;
		
		ESSL_CHECK(num = create_index_int_constant(ctx, i));
		ESSL_CHECK(num = process_single_node(ctx, num));

		ESSL_CHECK(A_index = _essl_new_binary_expression(ctx->pool, A, EXPR_OP_INDEX, num));
		_essl_ensure_compatible_node(A_index, n);
		A_index->hdr.type = A->hdr.type->child_type;
		ESSL_CHECK(A_index = process_single_node(ctx, A_index));

		ESSL_CHECK(B_index = _essl_new_binary_expression(ctx->pool, B, EXPR_OP_INDEX, num));
		_essl_ensure_compatible_node(B_index, n);
		B_index->hdr.type = B->hdr.type->child_type;
		ESSL_CHECK(B_index = process_single_node(ctx, B_index));

		/* clone the comparator, as each member comparison needs its own comparator */
		{
			scalar_size_specifier sz;
			const type_specifier *comparison_type = A_index->hdr.type;
			ESSL_CHECK(newcomparator = _essl_clone_node(ctx->pool, n));
			SET_CHILD(newcomparator, 0, A_index);
			SET_CHILD(newcomparator, 1, B_index);

			sz = _essl_get_scalar_size_for_type(comparison_type);
			assert(sz != SIZE_UNKNOWN);
			if(sz != _essl_get_scalar_size_for_type(newcomparator->hdr.type))
			{
				node *cast;
				const type_specifier *t;
				ESSL_CHECK(cast = _essl_new_type_convert_expression(ctx->pool, EXPR_OP_IMPLICIT_TYPE_CONVERT, newcomparator));
				_essl_ensure_compatible_node(cast, newcomparator);
				/* okay, now just fix up the type of n so we match the type of the comparison input */
				ESSL_CHECK(t = _essl_get_type_with_given_size(ctx->typestor_ctx, newcomparator->hdr.type, sz));
				newcomparator->hdr.type = t;
				ESSL_CHECK(newcomparator = process_single_node(ctx, newcomparator));
				SET_CHILD(cast, 0, newcomparator);
				ESSL_CHECK(cast = process_single_node(ctx, cast));

				newcomparator = cast;
			} else {
				ESSL_CHECK(newcomparator = process_single_node(ctx, newcomparator));

			}
		}

		/* If this is not the first member, create a logic operation of the appropriate type
		 * and link the previous comparator, and this new one under it. Making the 'current' 
		 * operation the new logicop.
		 * If the original comparator is NE, the 'joining' logic op is OR, otherwise it is AND.
		 */
		if(0 != current)
		{
			node* logicop;
			expression_operator logic_type = EXPR_OP_LOGICAL_AND;
			if(n->expr.operation == EXPR_OP_NE)
			{
				logic_type = EXPR_OP_LOGICAL_OR;
			}
			ESSL_CHECK(logicop = _essl_new_binary_expression(ctx->pool, current, logic_type, newcomparator));
			_essl_ensure_compatible_node(logicop, n);
			ESSL_CHECK(logicop = process_single_node(ctx, logicop));
			current = logicop;
		}
		else
		{
		/* otherwise, the 'current' is the new comparator, which should take care of the case of
		 * a struct with a single member
		 */
			current = newcomparator;
		}
	}
	
	return current;
}


/* store n to a temporary variable and reload it in order to keep the register pressure down */
/*@null@*/ static node *store_reload_variable(eliminate_complex_ops_context *ctx, node *n)
{
		string name = {"%store_reload_temp", 18};
		symbol *sym;
		node *var_ref_store, *assign;
		node *chain;
		node *var_ref_load;
		ESSL_CHECK(sym = _essl_new_variable_symbol_with_default_qualifiers(ctx->pool, name, n->hdr.type, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));
		sym->opt.is_indexed_statically = ESSL_FALSE;

		ESSL_CHECK(var_ref_store = _essl_new_variable_reference_expression(ctx->pool, sym));
		_essl_ensure_compatible_node(var_ref_store, n);
		ESSL_CHECK(var_ref_store = process_single_node(ctx, var_ref_store));

		ESSL_CHECK(assign = _essl_new_assign_expression(ctx->pool, var_ref_store, EXPR_OP_ASSIGN, n));
		_essl_ensure_compatible_node(assign, var_ref_store);
		ESSL_CHECK(assign = process_single_node(ctx, assign));


		ESSL_CHECK(var_ref_load = _essl_new_variable_reference_expression(ctx->pool, sym));
		_essl_ensure_compatible_node(var_ref_load, n);
		ESSL_CHECK(var_ref_load = process_single_node(ctx, var_ref_load));
		
		ESSL_CHECK(chain = _essl_new_binary_expression(ctx->pool, assign, EXPR_OP_CHAIN, var_ref_load));
		_essl_ensure_compatible_node(chain, var_ref_load);
		ESSL_CHECK(chain = process_single_node(ctx, chain));
		return chain;
}

static essl_bool is_expensive_matrix_result(node *n)
{
	unsigned i;
	/* see if this is the result of an (already rewritten) expensive matrix operation */
	if(n->hdr.type->basic_type != TYPE_MATRIX_OF || GET_MATRIX_N_COLUMNS(n->hdr.type) < 3) return ESSL_FALSE;
	if(n->hdr.kind != EXPR_KIND_BUILTIN_CONSTRUCTOR) return ESSL_FALSE;

	for(i = 0; i < GET_N_CHILDREN(n); ++i)
	{
		node *child = GET_CHILD(n, i);
		if(child != NULL)
		{
			if(child->hdr.kind != EXPR_KIND_BINARY) return ESSL_FALSE;
			if(child->expr.operation != EXPR_OP_ADD &&
			   child->expr.operation != EXPR_OP_SUB &&
			   child->expr.operation != EXPR_OP_MUL &&
			   child->expr.operation != EXPR_OP_DIV)
			{
				return ESSL_FALSE;
			}

		}
	}
	return ESSL_TRUE;

}

/*@null@*/ static node *rewrite_matrix_matrix_multiply(eliminate_complex_ops_context *ctx, node *n)
{
	node *res;
	node *A;
	node *B;
	node *A_indices[N_COMPONENTS] = {0};
	unsigned i, j;
	unsigned A_n_columns;
	unsigned B_n_rows;
	unsigned B_n_columns;
	ESSL_CHECK(A = GET_CHILD(n, 0));
	if(is_expensive_matrix_result(A))
	{
		ESSL_CHECK(A = store_reload_variable(ctx, A));
	}

	ESSL_CHECK(B = GET_CHILD(n, 1));
	if(is_expensive_matrix_result(B))
	{
		ESSL_CHECK(B = store_reload_variable(ctx, B));
	}

	A_n_columns = GET_MATRIX_N_COLUMNS(A->hdr.type);
	B_n_columns = GET_MATRIX_N_COLUMNS(B->hdr.type);
	B_n_rows = GET_MATRIX_N_ROWS(B->hdr.type);

	assert(A_n_columns == B_n_rows);
/*
  algorithm:
  for(j = 0; j < B_n_columns; ++j)
  {
     vec4 acc = vecA_n_rows(0.0);
     for(i = 0; i < B_n_rows; ++i)
     {
	     acc += A[i] * b[i][j];

     }
     res[j] = acc;
  }


*/


	for(i = 0; i < A_n_columns; ++i)
	{
		node *num;

		
		ESSL_CHECK(num = create_index_int_constant(ctx, i));
		ESSL_CHECK(num = process_single_node(ctx, num));

		ESSL_CHECK(A_indices[i] = _essl_new_binary_expression(ctx->pool, A, EXPR_OP_INDEX, num));
		_essl_ensure_compatible_node(A_indices[i], n);
		ESSL_CHECK(A_indices[i]->hdr.type = _essl_get_single_matrix_column_type(A->hdr.type));
		ESSL_CHECK(A_indices[i] = process_single_node(ctx, A_indices[i]));
	}

	ESSL_CHECK(res = _essl_new_builtin_constructor_expression(ctx->pool, B_n_columns));
	_essl_ensure_compatible_node(res, n);
	for(j = 0; j < B_n_columns; ++j)
	{
		node *B_index, *num;
		node *column = 0;
		
		ESSL_CHECK(num = create_index_int_constant(ctx, j));
		ESSL_CHECK(num = process_single_node(ctx, num));
		ESSL_CHECK(B_index = _essl_new_binary_expression(ctx->pool, B, EXPR_OP_INDEX, num));
		_essl_ensure_compatible_node(B_index, n);
		ESSL_CHECK(B_index->hdr.type = _essl_get_single_matrix_column_type(B->hdr.type));

		for(i = 0; i < B_n_rows; ++i)
		{
			node *mul;
			node *swz;
			ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, B_index));
			swz->expr.u.swizzle = _essl_create_scalar_swizzle(i);
			_essl_ensure_compatible_node(swz, n);
			ESSL_CHECK(swz->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, B_index->hdr.type, 1));
			ESSL_CHECK(swz = process_single_node(ctx, swz));


			ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, A_indices[i], EXPR_OP_MUL, swz));
			_essl_ensure_compatible_node(mul, A_indices[i]);
			ESSL_CHECK(mul = process_single_node(ctx, mul));
			
			if(column == 0)
			{
				column = mul;
			} else {
				ESSL_CHECK(column = _essl_new_binary_expression(ctx->pool, column, EXPR_OP_ADD, mul));
				_essl_ensure_compatible_node(column, mul);
				ESSL_CHECK(column = process_single_node(ctx, column));
				
			}

		}

		SET_CHILD(res, j, column);

	}

	ESSL_CHECK(res = process_single_node(ctx, res));

	return res;
}


/*@null@*/ static node *rewrite_vector_matrix_multiply(eliminate_complex_ops_context *ctx, node *n)
{
	node *res;
	node *mat;
	node *vec;
	unsigned i;
	unsigned mat_n_columns;
	ESSL_CHECK(vec = GET_CHILD(n, 0));
	ESSL_CHECK(mat = GET_CHILD(n, 1));
	if(is_expensive_matrix_result(mat))
	{
		ESSL_CHECK(mat = store_reload_variable(ctx, mat));
	}
	mat_n_columns = GET_MATRIX_N_COLUMNS(mat->hdr.type);

	assert(GET_NODE_VEC_SIZE(vec) == GET_MATRIX_N_ROWS(mat->hdr.type));
	ESSL_CHECK(res = _essl_new_builtin_constructor_expression(ctx->pool, mat_n_columns));
	_essl_ensure_compatible_node(res, n);
	for(i = 0; i < mat_n_columns; ++i)
	{
		/* cons[i] = dot(mat[i], vec) */
		node *index, *num, *dot;


		ESSL_CHECK(num = create_index_int_constant(ctx, i));
		ESSL_CHECK(num = process_single_node(ctx, num));

		ESSL_CHECK(index = _essl_new_binary_expression(ctx->pool, mat, EXPR_OP_INDEX, num));
		_essl_ensure_compatible_node(index, n);
		ESSL_CHECK(index = process_single_node(ctx, index));


		ESSL_CHECK(dot = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_DOT, index, vec, 0));
		_essl_ensure_compatible_node(dot, n);
		ESSL_CHECK(dot->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, res->hdr.type, 1));
		ESSL_CHECK(dot = process_single_node(ctx, dot));
		SET_CHILD(res, i, dot);

	}
	
	return process_single_node(ctx, res);
}

/*@null@*/ static node *rewrite_matrix_vector_multiply(eliminate_complex_ops_context *ctx, node *n)
{
	node *res;
	node *mat;
	node *vec;
	node *muls[4] = {0};
	unsigned i;
	unsigned mat_n_columns;
	ESSL_CHECK(mat = GET_CHILD(n, 0));

	if(is_expensive_matrix_result(mat))
	{
		ESSL_CHECK(mat = store_reload_variable(ctx, mat));
	}

	ESSL_CHECK(vec = GET_CHILD(n, 1));

	mat_n_columns = GET_MATRIX_N_COLUMNS(mat->hdr.type);


	assert(mat_n_columns == GET_NODE_VEC_SIZE(vec));
	for(i = 0; i < mat_n_columns; ++i)
	{
		/* cons[i] = mat[i] * vec[i] */
		node *index, *swz, *num;

		ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, vec));
		swz->expr.u.swizzle = _essl_create_scalar_swizzle(i);
		_essl_ensure_compatible_node(swz, n);
		ESSL_CHECK(swz->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, vec->hdr.type, 1));
		ESSL_CHECK(swz = process_single_node(ctx, swz));

		ESSL_CHECK(num = create_index_int_constant(ctx, i));
		ESSL_CHECK(num = process_single_node(ctx, num));

		ESSL_CHECK(index = _essl_new_binary_expression(ctx->pool, mat, EXPR_OP_INDEX, num));
		_essl_ensure_compatible_node(index, n);
		ESSL_CHECK(index->hdr.type = _essl_get_single_matrix_column_type(mat->hdr.type));
		ESSL_CHECK(index = process_single_node(ctx, index));


		ESSL_CHECK(muls[i] = _essl_new_binary_expression(ctx->pool, index, EXPR_OP_MUL, swz));
		_essl_ensure_compatible_node(muls[i], n);
		ESSL_CHECK(muls[i] = process_single_node(ctx, muls[i]));


	}
	res = muls[0];
	for(i = 1; i < mat_n_columns; ++i)
	{
		ESSL_CHECK(res = _essl_new_binary_expression(ctx->pool, res, EXPR_OP_ADD, muls[i]));
		_essl_ensure_compatible_node(res, n);
		ESSL_CHECK(res = process_single_node(ctx, res));
	}
	
	return res;
}




/*@null@*/ static node *rewrite_component_wise_matrix_op(eliminate_complex_ops_context *ctx, node *n, expression_operator op)
{
	node *res;
	node *A;
	node *B;
	unsigned i;
	unsigned n_columns = 0;
	const type_specifier *vec_type = NULL;
	ESSL_CHECK(A = GET_CHILD(n, 0));
	ESSL_CHECK(B = GET_CHILD(n, 1));

	/* note: don't assume that B is a scalar if A is a matrix, as this code needs to handle matrix - matrix operations */
	if(A->hdr.type->basic_type == TYPE_MATRIX_OF) 
	{ 
		n_columns = GET_MATRIX_N_COLUMNS(A->hdr.type);
		ESSL_CHECK(vec_type = _essl_get_single_matrix_column_type(A->hdr.type));
	} else if(B->hdr.type->basic_type == TYPE_MATRIX_OF) 
	{
		n_columns = GET_MATRIX_N_COLUMNS(B->hdr.type);
		ESSL_CHECK(vec_type = _essl_get_single_matrix_column_type(B->hdr.type));
	} else {
		assert(0);
		return NULL;
	}



	ESSL_CHECK(res = _essl_new_builtin_constructor_expression(ctx->pool, n_columns));
	_essl_ensure_compatible_node(res, n);
	for(i = 0; i < n_columns; ++i)
	{
		node *A_index, *B_index, *num;
		node *operation;


		ESSL_CHECK(num = create_index_int_constant(ctx, i));
		ESSL_CHECK(num = process_single_node(ctx, num));
		if(A->hdr.type->basic_type == TYPE_MATRIX_OF)
		{
			ESSL_CHECK(A_index = _essl_new_binary_expression(ctx->pool, A, EXPR_OP_INDEX, num));
			_essl_ensure_compatible_node(A_index, n);
			ESSL_CHECK(A_index->hdr.type = _essl_get_single_matrix_column_type(A->hdr.type));
			ESSL_CHECK(A_index = process_single_node(ctx, A_index));
		} else {
			A_index = A;
			assert(GET_NODE_VEC_SIZE(A) == 1);
		}
		
		if(B->hdr.type->basic_type == TYPE_MATRIX_OF)
		{
			ESSL_CHECK(B_index = _essl_new_binary_expression(ctx->pool, B, EXPR_OP_INDEX, num));
			_essl_ensure_compatible_node(B_index, n);
			ESSL_CHECK(B_index->hdr.type = _essl_get_single_matrix_column_type(B->hdr.type));
			ESSL_CHECK(B_index = process_single_node(ctx, B_index));
		} else {
			B_index = B;
			assert(GET_NODE_VEC_SIZE(B) == 1);
		}
		

		ESSL_CHECK(operation = _essl_new_binary_expression(ctx->pool, A_index, op, B_index));
		_essl_ensure_compatible_node(operation, n);
		ESSL_CHECK(operation->hdr.type = vec_type);
		ESSL_CHECK(operation = process_single_node(ctx, operation));


		SET_CHILD(res, i, operation);

	}
	
	return res;
}




/*@null@*/ static node *rewrite_unary_matrix_op(eliminate_complex_ops_context *ctx, node *n)
{
	node *res;
	node *A;
	unsigned i;
	unsigned len = 0;
	ESSL_CHECK(A = GET_CHILD(n, 0));
	len = GET_MATRIX_N_COLUMNS(A->hdr.type);



	ESSL_CHECK(res = _essl_new_builtin_constructor_expression(ctx->pool, len));
	_essl_ensure_compatible_node(res, n);
	for(i = 0; i < len; ++i)
	{
		node *A_index, *num;
		node *operation;


		ESSL_CHECK(num = create_index_int_constant(ctx, i));
		ESSL_CHECK(num = process_single_node(ctx, num));
		ESSL_CHECK(A_index = _essl_new_binary_expression(ctx->pool, A, EXPR_OP_INDEX, num));
		_essl_ensure_compatible_node(A_index, n);
		ESSL_CHECK(A_index->hdr.type = _essl_get_single_matrix_column_type(A->hdr.type));
		ESSL_CHECK(A_index = process_single_node(ctx, A_index));

		
		ESSL_CHECK(operation = _essl_new_unary_expression(ctx->pool, n->expr.operation, A_index));
		_essl_ensure_compatible_node(operation, A_index);
		ESSL_CHECK(operation = process_single_node(ctx, operation));

		SET_CHILD(res, i, operation);

	}
	
	return process_single_node(ctx, res);
}


/** Returns the operation performed (add, sub, mul or div) by an
 *  operation assignment or increment/decrement operation.
 */
static expression_operator embedded_op(expression_operator op) {
	switch (op) {
	case EXPR_OP_ADD:
	case EXPR_OP_PRE_INC:
	case EXPR_OP_POST_INC:
	case EXPR_OP_ADD_ASSIGN:
		return EXPR_OP_ADD;
	case EXPR_OP_SUB:
	case EXPR_OP_PRE_DEC:
	case EXPR_OP_POST_DEC:
	case EXPR_OP_SUB_ASSIGN:
		return EXPR_OP_SUB;
	case EXPR_OP_MUL:
	case EXPR_OP_MUL_ASSIGN:
		return EXPR_OP_MUL;
	case EXPR_OP_DIV:
	case EXPR_OP_DIV_ASSIGN:
		return EXPR_OP_DIV;
	default:
		assert(0); /* no embedded operation */
		return EXPR_OP_UNKNOWN;
	}
}



/*@null@*/ static node *process_single_node(eliminate_complex_ops_context *ctx, node *n)
{

	/* if the node is a struct comparison, replace it appropriately */
	switch(n->hdr.kind)
	{
	case EXPR_KIND_UNARY:
		switch(n->expr.operation)
		{
		case EXPR_OP_PRE_INC:
		case EXPR_OP_PRE_DEC:
		case EXPR_OP_POST_INC:
		case EXPR_OP_POST_DEC:
		case EXPR_OP_NEGATE:
		case EXPR_OP_PLUS:
			if(n->hdr.type->basic_type == TYPE_MATRIX_OF)
			{
				ESSL_CHECK(n = rewrite_unary_matrix_op(ctx, n));
			}
			break;
		default:
			break;
			
		}
		break;
	case EXPR_KIND_ASSIGN:
		switch(n->expr.operation)
		{
		case EXPR_OP_ADD_ASSIGN:
		case EXPR_OP_SUB_ASSIGN:
		case EXPR_OP_MUL_ASSIGN:
		case EXPR_OP_DIV_ASSIGN:
		{
			node *left_lvalue;
			node *left_rvalue;
			node *left;
			node *right;
			node *op;
			node *assign;
			symbol *sym;
			ESSL_CHECK(left = GET_CHILD(n, 0));
			ESSL_CHECK(right = GET_CHILD(n, 1));
			ESSL_CHECK(sym = _essl_symbol_for_node(n));
			ESSL_CHECK(_essl_middle_split_lvalue(left, &left_lvalue, &left_rvalue, _essl_is_var_ref_control_dependent(sym), ctx->pool));

			ESSL_CHECK(op = _essl_new_binary_expression(ctx->pool, left_rvalue, embedded_op(n->expr.operation), right));
			_essl_ensure_compatible_node(op, left_rvalue);
			ESSL_CHECK(op = process_single_node(ctx, op));
			ESSL_CHECK(assign = _essl_new_assign_expression(ctx->pool, left_lvalue, EXPR_OP_ASSIGN, op));
			_essl_ensure_compatible_node(assign, op);
			return process_single_node(ctx, assign);
		}
		default:
			break;
		}
		break;
	case EXPR_KIND_BINARY:
	{
		node *left, *right;
		ESSL_CHECK(left = GET_CHILD(n, 0));
		ESSL_CHECK(right = GET_CHILD(n, 1));
		switch(n->expr.operation)
		{
		case EXPR_OP_NE:
		case EXPR_OP_EQ:
			/* Check both sides of the comparison are of type STRUCT */
			if(left->hdr.type->basic_type == TYPE_STRUCT)
			{
				single_declarator* members = left->hdr.type->members;
				
				/* Explode the comparison, and overwrite the original comparison with the
				 * returned top level logic op.
				 */
				ESSL_CHECK(n = explode_struct_comparison(ctx, left, right, n, members));

			} else if(left->hdr.type->basic_type == TYPE_MATRIX_OF)
			{
				ESSL_CHECK(n = explode_matrix_comparison(ctx, n));

			}


			break;

		case EXPR_OP_MUL:
			if(left->hdr.type->basic_type == TYPE_MATRIX_OF && right->hdr.type->basic_type == TYPE_MATRIX_OF)
			{
				ESSL_CHECK(n = rewrite_matrix_matrix_multiply(ctx, n));
			} else if(left->hdr.type->basic_type == TYPE_MATRIX_OF && right->hdr.type->basic_type == TYPE_FLOAT && GET_NODE_VEC_SIZE(right) > 1)
			{
				ESSL_CHECK(n = rewrite_matrix_vector_multiply(ctx, n));
			} else if(left->hdr.type->basic_type == TYPE_FLOAT && GET_NODE_VEC_SIZE(left) > 1 && right->hdr.type->basic_type == TYPE_MATRIX_OF)
			{
				ESSL_CHECK(n = rewrite_vector_matrix_multiply(ctx, n));

			} else if(left->hdr.type->basic_type == TYPE_MATRIX_OF && right->hdr.type->basic_type == TYPE_FLOAT && GET_NODE_VEC_SIZE(right) == 1)
			{
				ESSL_CHECK(n = rewrite_component_wise_matrix_op(ctx, n, n->expr.operation));
			} else if(left->hdr.type->basic_type == TYPE_FLOAT && GET_NODE_VEC_SIZE(left) == 1 && right->hdr.type->basic_type == TYPE_MATRIX_OF)
			{
				ESSL_CHECK(n = rewrite_component_wise_matrix_op(ctx, n, n->expr.operation));
			}
			break;

		case EXPR_OP_ADD:
		case EXPR_OP_SUB:
		case EXPR_OP_DIV:
			if(left->hdr.type->basic_type == TYPE_MATRIX_OF || right->hdr.type->basic_type == TYPE_MATRIX_OF)
			{
				ESSL_CHECK(n = rewrite_component_wise_matrix_op(ctx, n, n->expr.operation));
			}
			break;

		default:
			break;
		}
		break;

	case EXPR_KIND_BUILTIN_FUNCTION_CALL:

		if(n->expr.operation == EXPR_OP_FUN_MATRIXCOMPMULT)
		{
				ESSL_CHECK(n = rewrite_component_wise_matrix_op(ctx, n, EXPR_OP_MUL));
		}

		break;

	}
	default:
		break;
	}
			
	return n;
}

/*@null@*/ static node *process_node(eliminate_complex_ops_context *ctx, node *n)
{
	unsigned i;
	node *nn;
	assert(n != 0);
	if((nn = _essl_ptrdict_lookup(&ctx->visited, n)) != 0)
	{
		return nn;
	}

	for(i = 0; i < GET_N_CHILDREN(n); ++i)
	{
		node *child = GET_CHILD(n, i);
		if(child != 0)
		{
			ESSL_CHECK(child = process_node(ctx, child));
			SET_CHILD(n, i, child);
		}
	}

	ESSL_CHECK(nn = process_single_node(ctx, n));
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, n, nn));
	if(nn != n)
	{
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, nn, nn));
	}
	return nn;
}


/** \brief Scan over an entire translation unit searching for complex ops to rewrite.
 *
 *	\param[in]		ctx		Context struct
 *	\param[in]		tu		Pointer to the translation unit to check
 *	\return					Memory error or ok
 */
static memerr rewrite_complex_ops_in_translation_unit(eliminate_complex_ops_context *ctx, translation_unit* tu )
{
	symbol_list* function;

	/* Iterate over all functions in the translation unit */
	for(function = tu->functions; function != 0; function = function->next)
	{
		if(function->sym->body != 0)
		{
			ESSL_CHECK(function->sym->body = process_node(ctx, function->sym->body));
		}

	}
	
	return MEM_OK;
}

/** \brief	Perform complex ops eliminations on the entire translation unit passed.
 *
 *	\param		pool		Pointer to the memory pool to use during the process.
 *	\param		tu			Pointer to the translation unit to process.
 *	\return					Memory error or ok.
 */
memerr _essl_eliminate_complex_ops(mempool *pool, typestorage_context *typestor_ctx, translation_unit* tu )
{
	eliminate_complex_ops_context ctx;
	mempool temp_pool;
	ctx.pool = pool;
	ctx.typestor_ctx = typestor_ctx;
	ctx.tu = tu;
	ESSL_CHECK(_essl_mempool_init(&temp_pool, 0, pool->tracker));

	if(_essl_ptrdict_init(&ctx.visited, &temp_pool) != MEM_OK)
	{
		_essl_mempool_destroy(&temp_pool);
		return MEM_ERROR;
	}
	if(_essl_ptrdict_init(&ctx.assigns, &temp_pool) != MEM_OK)
	{
		_essl_mempool_destroy(&temp_pool);
		return MEM_ERROR;
	}

	if(_essl_ptrdict_clear(&ctx.visited) != MEM_OK)
	{
		_essl_mempool_destroy(&temp_pool);
		return MEM_ERROR;
	}
	
	if(rewrite_complex_ops_in_translation_unit(&ctx, tu) != MEM_OK)
	{
		_essl_mempool_destroy(&temp_pool);
		return MEM_ERROR;
	}

	_essl_mempool_destroy(&temp_pool);
/*
	{
	char* output;
	output = _essl_visit_and_dot_output(pool, tu->root, DOT_FORMAT_DEFAULT);
	printf("%s\n\n", output);
	}
*/
	return MEM_OK;
}

/** \brief	Perform first phase struct elimination on the entire translation unit passed.
 *
 *	The first phase is to eliminate struct returns. Struct
 *	returns are rewritten as 'void' returns, and an additional 'out' parameter is added.
 *
 *	\param		pool		Pointer to the memory pool to use during the process.
 *	\param		tu			Pointer to the translation unit to process.
 *	\return					Memory error or ok.
 */
memerr _essl_eliminate_complex_returns(mempool *pool, translation_unit* tu )
{
	eliminate_complex_returns_context ctx;
	ctx.pool = pool;

	ESSL_CHECK(_essl_unique_name_init(&ctx.temp_names, pool, "%temp"));

	/* Fixup any functions that return structs */
	ESSL_CHECK(replace_returned_structs(&ctx, tu));
	
/*
	{
	char* output;
	output = _essl_visit_and_dot_output(pool, tu->root, DOT_FORMAT_DEFAULT);
	printf("%s\n\n", output);
	}
*/
	return MEM_OK;
}
