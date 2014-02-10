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
#include "common/error_reporting.h"
#include "frontend/constant_fold.h"
#include "frontend/typecheck.h"
#include "frontend/precision.h"
#include "common/essl_node.h"
#include "common/symbol_table.h"
#include "common/type_name.h"


/** Code to retrieve the source offset for this module (not reported) */
#define SOURCE_OFFSET n->hdr.source_offset

/** Code to access the error context for this module */
#define ERROR_CONTEXT ctx->err_context

static memerr print_type_1(typecheck_context *ctx, symbol *sym, const type_specifier *t)
{
	unsigned vec_size = t->u.basic.vec_size;
	switch(t->basic_type)
	{
	case TYPE_BOOL:
		if(vec_size == 1)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "bool"));
		}else
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "bvec"));
		}
		break;
	case TYPE_FLOAT:
		if(vec_size == 1)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "float"));
		}else
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "vec"));
		}
		break;
	case TYPE_INT:
		if(vec_size == 1)
		{
			if (t->u.basic.int_signedness == INT_SIGNED)
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "int"));
			}
			else
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "unsigned"));
			}
		}else
		{
			if (t->u.basic.int_signedness == INT_SIGNED)
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "ivec"));
			}
			else
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "uvec"));
			}
		}

		break;
	case TYPE_VOID:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "void"));
		break;
	case TYPE_ARRAY_OF:
		ESSL_CHECK(print_type_1(ctx, sym, t->child_type));
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "[%d]", t->u.array_size));
		break;
	case TYPE_STRUCT:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "struct %s", sym->name));
		break;
	case TYPE_SAMPLER_2D:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "sampler2D"));
		break;
	case TYPE_SAMPLER_3D:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "sampler3D"));
		break;
	case TYPE_SAMPLER_CUBE:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "samplerCube"));
		break;
	
	case TYPE_SAMPLER_EXTERNAL:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "samplerExternalOES"));
		break;

	default:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "?"));
		break;
	}
	if(vec_size != 1 &&
		(t->basic_type == TYPE_INT ||
		t->basic_type == TYPE_FLOAT||
		t->basic_type == TYPE_BOOL)
		)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%d", vec_size));
	}
	return MEM_OK;
}

static char *get_type_string_name(typecheck_context *ctx, symbol *sym, const type_specifier *t)
{
	char *res_str;
	ESSL_CHECK(_essl_string_buffer_reset(ctx->strbuf));
	ESSL_CHECK(print_type_1(ctx, sym, t));
	ESSL_CHECK(res_str = _essl_string_buffer_to_string(ctx->strbuf, ctx->pool));
	return res_str;
}

static node *typecheck(typecheck_context *ctx, node *n);
static essl_bool type_is_or_has_array(const type_specifier* type);

/** check if the basic type is integer, float, boolean or matrix */
static essl_bool is_ifbm(const type_specifier *t)
{
	return t->basic_type == TYPE_INT || t->basic_type == TYPE_FLOAT || t->basic_type == TYPE_BOOL || (t->basic_type == TYPE_MATRIX_OF && t->child_type->basic_type == TYPE_FLOAT);
}

/** check if the basic type is integer, float or boolean */
static essl_bool is_ifb(const type_specifier *t)
{
	return t->basic_type == TYPE_INT || t->basic_type == TYPE_FLOAT || t->basic_type == TYPE_BOOL;
}

/** check if the basic type is integer, float or matrix */
static essl_bool is_ifm(const type_specifier *t)
{
	return t->basic_type == TYPE_INT || t->basic_type == TYPE_FLOAT || (t->basic_type == TYPE_MATRIX_OF && t->child_type->basic_type == TYPE_FLOAT);
}


/** Checks that the node is a valid lvalue.
    Reports an error and returns 0 if it's not.
*/
static int check_lvalue(typecheck_context *ctx, node *n)
{
/*
  The ESSL spec says:
The assignment operator stores the value of expression into lvalue. It will compile only if expression and
lvalue have the same type. All desired type-conversions must be specified explicitly via a constructor. Lvalues
must be writable. Variables that are built-in types, entire structures, structure fields, l-values with
the field selector ( . ) applied to select components or swizzles without repeated fields, and l-values
dereferenced with the array subscript operator ( [ ] ) are all l-values. Other binary or unary expressions,
non-dereferenced arrays, function names, swizzles with repeated fields, and constants cannot be l-values.
The ternary operator (?:) is also not allowed as an l-value.

Array variables are l-values and may be passed to parameters declared as out or inout. However, they
may not be used as the target of an assignment.

*/

	unsigned int i;
	int fields;
	swizzle_pattern swz;
	node *child0;
	assert(n != 0);

	switch(n->hdr.kind)
	{
	case EXPR_KIND_UNARY:
		child0 = GET_CHILD(n, 0);
		assert(child0 != 0);
		switch(n->expr.operation)
		{
		case EXPR_OP_MEMBER:
			return check_lvalue(ctx, child0);
		case EXPR_OP_SWIZZLE:
			swz = n->expr.u.swizzle;
			fields = 0;
			for(i = 0; i < N_COMPONENTS; ++i)
			{
				if((int)swz.indices[i] >= 0)
				{
					if((fields & (1 << (unsigned)swz.indices[i])))
					{
						ERROR_CODE_MSG(ERR_SEM_LVALUE_SWIZZLE_DUPLICATE_COMPONENTS, "L-value swizzle contains duplicate components\n");
						return 0;
					}
					fields |= (1 << (unsigned)swz.indices[i]);
				}
			}

			return check_lvalue(ctx, child0);
				
		default:
			break;
		}
		break;
	case EXPR_KIND_BINARY:
		child0 = GET_CHILD(n, 0);
		assert(child0 != 0);
		if(n->expr.operation == EXPR_OP_INDEX)
		{
			return check_lvalue(ctx, child0);
		}
		break;
/*
	case EXPR_KIND_ASSIGN:
		child0 = GET_CHILD(n, 0);
		assert(child0 != 0);
		return check_lvalue(ctx, child0);
*/
	case EXPR_KIND_VARIABLE_REFERENCE:
		n->hdr.type = n->expr.u.sym->type; /* pull in the possible changed type from the variable symbol */
		assert(n->hdr.type != 0);

		if(n->expr.u.sym->qualifier.variable == VAR_QUAL_CONSTANT || n->expr.u.sym->type->type_qual & TYPE_QUAL_NOMODIFY)
		{
			ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "L-value is constant\n");
			return 0;
		}

		if(n->expr.u.sym->address_space == ADDRESS_SPACE_UNIFORM)
		{
			ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "Cannot modify a uniform variable\n");
			return 0;
		}

		if(n->expr.u.sym->address_space == ADDRESS_SPACE_FRAGMENT_VARYING)
		{
			ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "Cannot modify a varying variable inside a fragment shader\n");
			return 0;
		}

		if(n->expr.u.sym->address_space == ADDRESS_SPACE_ATTRIBUTE)
		{
			ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "Cannot modify an attribute variable\n");
			return 0;
		}

		return 1;

	case EXPR_KIND_CONSTANT:
		/*
		* PS: constant folding will have changed EXPR_KIND_VARIABLE_REFERENCE
		* with VAR_QUAL_CONSTANT to EXPR_KIND_CONSTANT, so this will be also handled here.
		*/
		ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "L-value is constant\n");
		return 0;

	default:
		break;
	}


	ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "Operand is not an lvalue\n");
	return 0;
}

/** Type pointer */
typedef const type_specifier *const_typespec_ptr;

/** Calculates the size of an array type by translating the unresolved_array_size in into an array_size integer */
static int typecheck_array_size(typecheck_context *ctx, const_typespec_ptr *t, node *n)
{
	node *tmp;
	type_specifier *ts;
	assert(t && *t);
	if((*t)->basic_type == TYPE_STRUCT)
	{
		/* recurse through the members */
		single_declarator *member;
		for(member = (*t)->members; member != 0; member = member->next)
		{
			ESSL_CHECK_OOM(typecheck_array_size(ctx, &member->type, n));
		}
	}

	if((*t)->basic_type != TYPE_UNRESOLVED_ARRAY_OF) return 1;
	ESSL_CHECK_NONFATAL(tmp = typecheck(ctx, (*t)->u.unresolved_array_size));
	assert(tmp->hdr.type != 0);
	ESSL_CHECK_OOM(ts = _essl_clone_type(ctx->pool, (*t)));
	ts->u.unresolved_array_size = tmp;
	*t = ts;
	if(!_essl_node_is_constant(tmp) || tmp->hdr.type->basic_type != TYPE_INT || GET_VEC_SIZE(tmp->hdr.type) != 1)
	{
		ERROR_CODE_MSG(ERR_SEM_EXPRESSION_INTEGRAL_CONSTANT, "Array size must be a constant integral expression\n");
		return 0;
	}

	ts->basic_type = TYPE_ARRAY_OF;
	ts->u.array_size = ctx->desc->scalar_to_int(tmp->expr.u.value[0]);
	if(ts->u.array_size <= 0)
	{
		ERROR_CODE_MSG(ERR_SEM_ARRAY_SIZE_GREATER_ZERO, "Array size must be greater than zero\n");
		return 0;
		
	}
	return typecheck_array_size(ctx, &ts->child_type, n);


}

static enum {
	SIG_DIFFERENT,
	SIG_EQUAL_EXCEPT_QUALIFIERS,
	SIG_EQUAL
} function_signatures_equal(symbol *f1, symbol *f2)
{
	single_declarator *a = f1->parameters;
	single_declarator *b = f2->parameters;
	essl_bool equal_type = ESSL_TRUE;
	essl_bool equal_param_qual = ESSL_TRUE;
	while (a != NULL && b != NULL && equal_type)
	{
		if (!_essl_type_equal(a->type, b->type))
		{
			equal_type = ESSL_FALSE;
		}

		if (a->qualifier.direction != b->qualifier.direction)
		{
			equal_param_qual = ESSL_FALSE;
		}

		a = a->next;
		b = b->next;
	}

	if (a != NULL || b != NULL)
	{
		equal_type = ESSL_FALSE; /* different number of arguments */
	}

	if (!equal_type) return SIG_DIFFERENT;
	if (!equal_param_qual) return SIG_EQUAL_EXCEPT_QUALIFIERS;
	return SIG_EQUAL;
}

static memerr typecheck_function_declaration(typecheck_context *ctx, node *n)
{
	symbol *sym = n->decl.sym;
	symbol *prev_sym;
	for (prev_sym = sym->next ; prev_sym != NULL ; prev_sym = prev_sym->next)
	{
		switch (function_signatures_equal(sym, prev_sym))
		{
		case SIG_DIFFERENT:
			/* Different functions */
			break;
		case SIG_EQUAL_EXCEPT_QUALIFIERS:
			ERROR_CODE_STRING(ERR_SEM_FUNCTION_DIFF_REDECL, "Function '%s' redeclared with different parameter qualifier(s)\n", sym->name);
			return 0;
		case SIG_EQUAL:
			/* Same signature */
			
			/* return type must also match */
			if (!_essl_type_equal(sym->type, prev_sym->type))
			{
				ERROR_CODE_STRING(ERR_SEM_FUNCTION_DIFF_REDECL, "Function '%s' redeclared with different return type\n", sym->name);
				return 0;
			}

			if (sym->body)
			{
				/* This is a function definition */
			
				if (prev_sym->body || prev_sym->kind == SYM_KIND_BUILTIN_FUNCTION)
				{
					/* Only one definition allowed */
					ERROR_CODE_STRING(ERR_SEM_FUNCTION_REDEFINITION, "Function '%s' redefined\n", sym->name);
					return 0;
				}

				/* Map the declaration to the definition */
				ESSL_CHECK(_essl_ptrdict_insert(&ctx->fun_decl_to_def, prev_sym, sym));
			} else {
				/* This is a function declaration */
				
				if (!prev_sym->body)
				{
					/* Only one declaration allowed */
					ERROR_CODE_STRING(ERR_SEM_FUNCTION_DIFF_REDECL, "Function '%s' redeclared.\n", sym->name);
					return 0;
				}

				/* Map the declaration to the definition */
				ESSL_CHECK(_essl_ptrdict_insert(&ctx->fun_decl_to_def, sym, prev_sym));
			}
			
			break;
		}
	}

	return MEM_OK;
}

/* Determines and sets the type of a single node */
node *_essl_typecheck_single_node(typecheck_context *ctx, node *n)
{
	node *child0 = 0, *child1 = 0, *child2 = 0;
	assert(n != 0);

	if(GET_NODE_KIND(n->hdr.kind) == NODE_KIND_STATEMENT)
	{
		/*
		  typecheck statements
		*/
		switch(n->hdr.kind)
		{
		case STMT_KIND_UNKNOWN:
			assert(0);
			return 0;
		case STMT_KIND_RETURN:
			assert(n->hdr.type != 0);
			child0 = GET_CHILD(n, 0);
			if(child0 != 0 && n->hdr.type->basic_type == TYPE_VOID)
			{
				ERROR_CODE_MSG(ERR_SEM_VOID_FUNCTION_RETURNS_VALUE, "Function declared void but return statement has an argument\n");
				return 0;
				
			}
			if(child0 == 0)
			{
				assert(n->hdr.type != 0);
				if(n->hdr.type->basic_type != TYPE_VOID)
				{
				
					ERROR_CODE_MSG(ERR_SEM_FUNCTION_DOES_NOT_RETURN_VALUE, "Function declared with a return value but return statement has no argument.\n");
					return 0;
				
				}
			}
			if(child0 != 0)
			{
				assert(child0->hdr.type != 0);
				if(!_essl_type_equal(n->hdr.type, child0->hdr.type))
				{
					ERROR_CODE_TWOPARAMS(ERR_SEM_RETURN_TYPE_MISMATCH, "Type mismatch, cannot convert from '%s' to '%s'\n", _essl_get_type_name(ctx->pool, child0->hdr.type), _essl_get_type_name(ctx->pool, n->hdr.type));
					return 0;
					
				}
			}
			break;


		case STMT_KIND_CONTINUE:
		case STMT_KIND_BREAK:
			break;

		case STMT_KIND_DISCARD:
			if (ctx->desc->kind != TARGET_FRAGMENT_SHADER)
			{
				ERROR_CODE_MSG(ERR_SEM_DISCARD_IN_VERTEX_SHADER, "discard can only be used in fragment shaders.\n");
				return 0;
			}
			break;

		case STMT_KIND_IF:
			{
				const type_specifier *ts = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1);
				ESSL_CHECK_OOM(ts);
				n->hdr.type = ts;
				child0 = GET_CHILD(n, 0);
				assert(child0 != 0);
				assert(child0->hdr.type != 0);
				if(!_essl_type_equal(n->hdr.type, child0->hdr.type))
				{
					ERROR_CODE_MSG(ERR_SEM_IF_PARAMETER_BOOL, "if() condition must be of boolean type\n");
					return 0;
	
				}
				return n;
			}
		case STMT_KIND_WHILE:
		case STMT_KIND_DO:
			{
				const type_specifier *ts = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1);
				node *cond = NULL;
				ESSL_CHECK_OOM(ts);
				n->hdr.type = ts;
				if(n->hdr.kind == STMT_KIND_WHILE)
				{
					cond = GET_CHILD(n, 0);
				} else {
					cond = GET_CHILD(n, 1);
				}
				assert(cond != 0);
				assert(cond->hdr.type != 0);
				if(!_essl_type_equal(n->hdr.type, cond->hdr.type))
				{
					ERROR_CODE_MSG(ERR_SEM_IF_PARAMETER_BOOL, "while() condition must be of boolean type\n");
					return 0;
	
				}
				return n;
			}
		case STMT_KIND_FOR:
			{
				const type_specifier *ts = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1);
				ESSL_CHECK_OOM(ts);
				n->hdr.type = ts;
				child1 = GET_CHILD(n, 1);
				
				assert(child1 != 0);
				assert(child1->hdr.type != 0);
				if(!_essl_type_equal(n->hdr.type, child1->hdr.type))
				{
					ERROR_CODE_MSG(ERR_SEM_IF_PARAMETER_BOOL, "for() condition must be of boolean type\n");
					return 0;
	
				}
				return n;
			}
		case STMT_KIND_COMPOUND:
			return n;
		default:
			assert(0);
			return 0;
		}
	} else if(GET_NODE_KIND(n->hdr.kind) == NODE_KIND_DECLARATION)
	{
		type_basic nonderived_basic_type;
		essl_bool is_array = ESSL_FALSE;
		const type_specifier *ct;
		/*
		  typecheck declarations
		*/
		switch(n->hdr.kind)
		{
		case DECL_KIND_UNKNOWN:
			assert(0);
			return 0;
		case DECL_KIND_VARIABLE:


			n->hdr.type = n->decl.sym->type;
			n->decl.sym->body = GET_CHILD(n, 0);

			ESSL_CHECK_NONFATAL(typecheck_array_size(ctx, &n->hdr.type, n));
			n->decl.sym->type = n->hdr.type;

			assert(n->hdr.type->basic_type != TYPE_UNKNOWN);

			if (n->hdr.type->basic_type == TYPE_VOID)
			{
				ERROR_CODE_MSG(ERR_SEM_ILLEGAL_DECLARATION, "Cannot declare a variable of type void\n");
				return 0;
			}

			if ((n->decl.sym->type->basic_type == TYPE_SAMPLER_2D_SHADOW) &&
				(_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_EXT_SHADOW_SAMPLERS) == BEHAVIOR_WARN))
			{
				WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_EXT_shadow_samplers' used, variable '%s' declared with type 'sampler2DShadow'\n", n->decl.sym->name);
			}

			if ((n->decl.sym->type->basic_type == TYPE_SAMPLER_3D) &&
				(_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_TEXTURE_3D) == BEHAVIOR_WARN))
			{
				WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_OES_texture_3D' used, variable '%s' declared with type 'sampler3D'\n", n->decl.sym->name);
			}
			
			nonderived_basic_type = _essl_get_nonderived_basic_type(n->hdr.type);
			is_array = ESSL_FALSE;
			ct = n->hdr.type;
			while(!is_array && ct != NULL)
			{
				if(ct->basic_type == TYPE_ARRAY_OF)
				{
					is_array = ESSL_TRUE;
				}
				ct = ct->child_type;
			}

			switch(n->decl.sym->qualifier.variable)
			{
			case VAR_QUAL_NONE:
				break;

			case VAR_QUAL_CONSTANT:
				child0 = GET_CHILD(n, 0);
				if(child0 == 0)
				{
					ERROR_CODE_MSG(ERR_SEM_CONST_VARIABLE_NO_INITIALIZER, "const variable does not have an initializer\n");
					return 0;
				}
				if(!_essl_node_is_constant(child0))
				{
					ERROR_CODE_MSG(ERR_SEM_INITIALIZER_CONSTANT, "Initializer for const value must be a constant expression\n");
					return 0;
				}
				break;
			case VAR_QUAL_ATTRIBUTE:
				if(GET_CHILD(n, 0) != 0)
				{
					ERROR_CODE_MSG(ERR_SEM_ATTRIBUTE_INITIALIZER, "Attribute variable with initializer\n");
					return 0;
				}
				if(nonderived_basic_type != TYPE_FLOAT || is_array)
				{
					ERROR_CODE_MSG(ERR_SEM_ILLEGAL_DATA_TYPE_FOR_ATTRIBUTE, "Illegal type for attribute variable\n");
					return 0;
				}
				if (ctx->desc->kind != TARGET_VERTEX_SHADER)
				{
					ERROR_CODE_MSG(ERR_SEM_ATTRIBUTE_OUTSIDE_VERTEX_SHADER, "Attribute qualifier only allowed in vertex shaders\n");
					return 0;
				}
				break;
			case VAR_QUAL_VARYING:
				if(GET_CHILD(n, 0) != 0)
				{
					ERROR_CODE_MSG(ERR_SEM_VARYING_INITIALIZER, "Varying variable with initializer\n");
					return 0;
					
				}
				if(nonderived_basic_type != TYPE_FLOAT)
				{
					ERROR_CODE_MSG(ERR_SEM_ILLEGAL_DATA_TYPE_FOR_VARYING, "Illegal type for varying variable\n");
					return 0;
				}
				break;
			case VAR_QUAL_UNIFORM:
				if(GET_CHILD(n, 0) != 0)
				{
					ERROR_CODE_MSG(ERR_SEM_UNIFORM_INITIALIZER, "Uniform variable with initializer\n");
					return 0;
				}
				break;
			case VAR_QUAL_PERSISTENT:
				break;
			}

			if ((n->decl.sym->qualifier.variable != VAR_QUAL_UNIFORM) && _essl_type_is_or_has_sampler(n->hdr.type))
			{
				ERROR_CODE_MSG(ERR_SEM_ILLEGAL_SAMPLER_DECLARATION, "Sampler declared without uniform qualifier\n");
				return 0;
			}

			child0 = GET_CHILD(n, 0);
			if(child0 != 0)
			{
				assert(child0->hdr.type != 0);
				if(!_essl_type_equal(n->hdr.type, child0->hdr.type))
				{
					ERROR_CODE_TWOPARAMS(ERR_SEM_TYPE_MISMATCH, "Type mismatch, cannot convert from '%s' to '%s'\n", _essl_get_type_name(ctx->pool, child0->hdr.type), _essl_get_type_name(ctx->pool, n->hdr.type));
					return 0;
				}

				if(type_is_or_has_array(child0->hdr.type))
				{
					ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "Cannot assign to arrays or structs with arrays\n");
					return 0;
				}

			}

			break;
		case DECL_KIND_FUNCTION:
			if(type_is_or_has_array(n->decl.sym->type))
			{
					ERROR_CODE_MSG(ERR_SEM_RETURN_TYPE_ARRAY, "Function returns an array\n");
					return 0;
			}

			if(n->decl.sym->qualifier.variable == VAR_QUAL_ATTRIBUTE)
			{
					ERROR_CODE_MSG(ERR_SEM_ILLEGAL_QUALIFIER_FOR_RETURN_TYPE, "Attribute qualifier used on return type\n");
					return 0;
			}
			else if (n->decl.sym->qualifier.variable == VAR_QUAL_UNIFORM)
			{
					ERROR_CODE_MSG(ERR_SEM_ILLEGAL_QUALIFIER_FOR_RETURN_TYPE, "Uniform qualifier used on return type\n");
					return 0;
			}
			else if (n->decl.sym->qualifier.variable == VAR_QUAL_VARYING)
			{
					ERROR_CODE_MSG(ERR_SEM_ILLEGAL_QUALIFIER_FOR_RETURN_TYPE, "Varying qualifier used on return type\n");
					return 0;
			}
			else if (n->decl.sym->qualifier.variable == VAR_QUAL_CONSTANT)
			{
					ERROR_CODE_MSG(ERR_SEM_ILLEGAL_QUALIFIER_FOR_RETURN_TYPE, "Const qualifier used on return type\n");
					return 0;
			}

			{
				single_declarator *parm = n->decl.sym->parameters;
				while(parm != 0)
				{
					ESSL_CHECK_NONFATAL(typecheck_array_size(ctx, &parm->type, n));
					
					if(parm->sym)
					{
						parm->sym->type = parm->type;
					}

					if ((parm->qualifier.direction == DIR_OUT || parm->qualifier.direction == DIR_INOUT) &&  _essl_type_is_or_has_sampler(parm->type))
					{
						ERROR_CODE_MSG(ERR_SEM_ILLEGAL_SAMPLER_DECLARATION, "Samplers cannot have out or inout parameter qualifier\n");
						return 0;
					}

					if (parm->qualifier.variable == VAR_QUAL_ATTRIBUTE)
					{
						ERROR_CODE_MSG(ERR_SEM_ATTRIBUTE_INSIDE_FUNCTION, "Attribute qualifier used on function parameter\n");
						return 0;
					}
					else if (parm->qualifier.variable == VAR_QUAL_VARYING)
					{
						ERROR_CODE_MSG(ERR_SEM_ATTRIBUTE_INSIDE_FUNCTION, "Varying qualifier used on function parameter\n");
						return 0;
					}
					else if (parm->qualifier.variable == VAR_QUAL_UNIFORM)
					{
						ERROR_CODE_MSG(ERR_SEM_ATTRIBUTE_INSIDE_FUNCTION, "Uniform qualifier used on function parameter\n");
						return 0;
					}

					if ((parm->type->basic_type == TYPE_SAMPLER_2D_SHADOW) &&
						(_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_EXT_SHADOW_SAMPLERS) == BEHAVIOR_WARN))
					{
						if (parm->name.ptr != NULL && parm->name.len >= 0)
						{
							WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_EXT_shadow_samplers' used, parameter '%s' has type 'sampler2DShadow'\n", parm->name);
						}
						else
						{
							WARNING_CODE_MSG(ERR_WARNING, "Extension 'GL_EXT_shadow_samplers' used, unnamed parameter has type 'sampler2DShadow'\n");
						}
					}

					if ((parm->type->basic_type == TYPE_SAMPLER_3D) &&
						(_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_TEXTURE_3D) == BEHAVIOR_WARN))
					{
						if (parm->name.ptr != NULL && parm->name.len >= 0)
						{
							WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_OES_texture_3D' used, parameter '%s' has type 'sampler3D'\n", parm->name);
						}
						else
						{
							WARNING_CODE_MSG(ERR_WARNING, "Extension 'GL_OES_texture_3D' used, unnamed parameter has type 'sampler3D'\n");
						}
					}

					parm = parm->next;
				}		
				
			}

			ESSL_CHECK_NONFATAL(typecheck_function_declaration(ctx, n));
			
			break;

		case DECL_KIND_PRECISION:

			if ((n->decl.prec_type == TYPE_SAMPLER_2D_SHADOW) &&
			    (_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_EXT_SHADOW_SAMPLERS) == BEHAVIOR_WARN))
			{
				WARNING_CODE_MSG(ERR_WARNING, "Extension 'GL_EXT_shadow_samplers' used, default precision set\n");
			}
			else if ((n->decl.prec_type == TYPE_SAMPLER_3D) &&
			         (_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_TEXTURE_3D) == BEHAVIOR_WARN))
			{
				WARNING_CODE_MSG(ERR_WARNING, "Extension 'GL_OES_texture_3D' used, default precision set\n");
			}

			break;

		default:
			assert(0);
			break;
		}
	}
	else if(GET_NODE_KIND(n->hdr.kind) == NODE_KIND_EXPRESSION)
	{
		/*
		  typecheck expression
		*/
		const type_specifier *lht = 0;
		const type_specifier *rht = 0;
		expression_operator op;

		unsigned int i;
		if(GET_N_CHILDREN(n) > 0)
		{
			child0 = GET_CHILD(n, 0);
			if (child0) lht = child0->hdr.type;
			if (GET_N_CHILDREN(n) > 1)
			{
				child1 = GET_CHILD(n, 1);
				if(child1)
				{
					rht = child1->hdr.type;
				}
			}
		}

		op = n->expr.operation;
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			node *child = GET_CHILD(n, i);
			if(child)
			{
				if(GET_NODE_KIND(child->hdr.kind) == NODE_KIND_EXPRESSION)
				{
					assert(child->hdr.type != 0);
					if(child->hdr.type->basic_type == TYPE_UNRESOLVED_ARRAY_OF)
					{
						ESSL_CHECK_NONFATAL(typecheck_array_size(ctx, &child->hdr.type, child));
					}
				}
			}
		}

		switch(n->hdr.kind)
		{
		case EXPR_KIND_UNARY:
			assert(lht != 0);
			assert(GET_CHILD(n, 0) != 0);
			switch(op)
			{

			case EXPR_OP_MEMBER:
				break;
			case EXPR_OP_SWIZZLE:
				break;
			case EXPR_OP_MEMBER_OR_SWIZZLE:

				/*
				  A member reference and a swizzle looks identical
				  to the parser, because the parser does not know the
				  type of the operand.

				  Therefore, we will rewrite EXPR_OP_MEMBER_OR_SWIZZLE
				  into either EXPR_OP_MEMBER or EXPR_OP_SWIZZLE based
				  on the type of the operand.


				*/


				if(lht->basic_type == TYPE_ARRAY_OF)
				{
					ERROR_CODE_STRING(ERR_SEM_OPERATOR_UNSUPPORTED, "Request for member \"%s\" of an array\n", n->expr.u.member_string);
					return 0;

				}
				if(lht->basic_type == TYPE_STRUCT)
				{
					/* this a member reference */
					single_declarator *memb = lht->members;
					const char *nm;
					const char *nm2;
					while(memb)
					{
						if(!_essl_string_cmp(n->expr.u.member_string, memb->name))
						{
							/* match */
							const type_specifier *tmp = _essl_get_unqualified_type(ctx->pool, memb->type);
							ESSL_CHECK_OOM(tmp);
							n->hdr.type = tmp;
							n->expr.operation = EXPR_OP_MEMBER;
							n->expr.u.member = memb;
							return n;
						}
						memb = memb->next;
					}
					ESSL_CHECK_OOM(nm = _essl_string_to_cstring(ctx->pool, lht->name));
					ESSL_CHECK_OOM(nm2 = _essl_string_to_cstring(ctx->pool, n->expr.u.member_string));

					ERROR_CODE_TWOPARAMS(ERR_SEM_ILLEGAL_FIELD_SELECTOR, "Struct \"%s\" has no member named \"%s\"\n", nm, nm2);
					return 0;

				}

				if(is_ifb(lht) && GET_VEC_SIZE(lht) > 1)
				{
					/* this is a swizzle */
					string swz = n->expr.u.member_string;
					int posfam = 0;
					int colorfam = 0;
					int coordfam = 0;

					const char *ptr = swz.ptr;
					assert(ptr != 0);
					if(swz.len > 4)
					{
						ERROR_CODE_STRING(ERR_SEM_ILLEGAL_FIELD_SELECTOR, "Vector swizzle \"%s\" is bigger than 4\n", swz);
						return 0;
					}
					n->expr.u.swizzle = _essl_create_undef_swizzle();

					for(i = 0; i < (unsigned)swz.len; ++i)
					{
						switch(ptr[i])
						{
						case 'x':
						case 'y':
						case 'z':
						case 'w':
							posfam = 1;
							break;

						case 'r':
						case 'g':
						case 'b':
						case 'a':
							colorfam = 1;
							break;

						case 's':
						case 't':
						case 'p':
						case 'q':
							coordfam = 1;
							break;
						default:
							ERROR_CODE_PARAM(ERR_SEM_ILLEGAL_FIELD_SELECTOR, "Swizzle field selector '%c' unknown\n", ptr[i]);
							return 0;
						}

						switch(ptr[i])
						{
						case 'x':
						case 'r':
						case 's':
							n->expr.u.swizzle.indices[i] = (signed char)0;
							break;

						case 'y':
						case 'g':
						case 't':
							n->expr.u.swizzle.indices[i] = (signed char)1;
							break;

						case 'z':
						case 'b':
						case 'p':
							n->expr.u.swizzle.indices[i] = (signed char)2;
							break;
					
						case 'w':
						case 'a':
						case 'q':
							n->expr.u.swizzle.indices[i] = (signed char)3;
							break;
						}
						if((unsigned)n->expr.u.swizzle.indices[i] >= GET_VEC_SIZE(lht))
						{
							ERROR_CODE_MSG(ERR_SEM_ILLEGAL_FIELD_SELECTOR, "Swizzle field selector out of range\n");
							return 0;
						}
					}
					if(posfam + colorfam + coordfam != 1)
					{
						ERROR_CODE_STRING(ERR_SEM_FIELD_SELECTORS_SAME_SET, "Vector swizzle \"%s\" mixes components from different sets\n", swz);
						return 0;
					}

					ESSL_CHECK_OOM(n->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, lht, swz.len));
					n->expr.operation = EXPR_OP_SWIZZLE;
					
					return n;
				}
				ERROR_CODE_MSG(ERR_SEM_OPERATOR_UNSUPPORTED, "Member reference or swizzle attempted on non-structure and non-vector\n");
				return 0;
				
			case EXPR_OP_NOT:
				n->hdr.type = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1);
				ESSL_CHECK_OOM(n->hdr.type);
				if(!_essl_type_equal(n->hdr.type, lht))
				{
					ERROR_CODE_MSG(ERR_SEM_OPERATOR_UNSUPPORTED, "Operand to ! must have boolean type\n");
					return 0;
				}
				return n;

			case EXPR_OP_PRE_INC:
			case EXPR_OP_POST_INC:
			case EXPR_OP_PRE_DEC:
			case EXPR_OP_POST_DEC:
				assert(child0 != 0);
				if(!check_lvalue(ctx, child0))
				{
					return 0;
				}
				/*@fallthrough@*/

			case EXPR_OP_PLUS:
			case EXPR_OP_NEGATE:
				if(lht->basic_type != TYPE_FLOAT && lht->basic_type != TYPE_INT && lht->basic_type != TYPE_MATRIX_OF)
				{
					ERROR_CODE_MSG(ERR_SEM_OPERATOR_UNSUPPORTED, "Arithmetic unary operations only work with integer and floating-point values\n");
					return 0;
				}
				n->hdr.type = lht;
				return n;
			default:
				assert(0);
				break;
			}
			
			break;
		case EXPR_KIND_ASSIGN:
			assert(child0 != 0);
			assert(child1 != 0);
			assert(lht != 0);
			assert(rht != 0);

			if(!check_lvalue(ctx, child0))
			{
				return 0;
			}

			if(type_is_or_has_array(lht))
			{
				ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "Cannot assign to arrays or structs with arrays\n");
				return 0;
			}

			if (_essl_type_is_or_has_sampler(lht))
			{
				/* this can happen if we have a struct with a sampler as a function parameter */
				ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "Cannot modify sampler variable\n");
				return 0;
			}
			
			if(n->expr.operation == EXPR_OP_ASSIGN)
			{
				if(_essl_type_equal(lht, rht))
				{
					ESSL_CHECK_OOM(n->hdr.type = _essl_get_unqualified_type(ctx->pool, lht));
					return n;
				}
				ERROR_CODE_TWOPARAMS(ERR_SEM_TYPE_MISMATCH, "Type mismatch, cannot convert from '%s' to '%s'\n", _essl_get_type_name(ctx->pool, rht), _essl_get_type_name(ctx->pool, lht));
				return 0;
			}

			/* augmented assignment checking is handled by EXPR_KIND_BINARY */
			switch(op)
			{
			case EXPR_OP_ADD_ASSIGN:
				op = EXPR_OP_ADD;
				break;
			case EXPR_OP_SUB_ASSIGN:
				op = EXPR_OP_SUB;
				break;
			case EXPR_OP_MUL_ASSIGN:
				op = EXPR_OP_MUL;
				break;
			case EXPR_OP_DIV_ASSIGN:
				op = EXPR_OP_DIV;
				break;
			default:
				assert(0);
			}
			/*@fallthrough@*/

		case EXPR_KIND_BINARY:
			switch(op)
			{
			case EXPR_OP_ADD:
			case EXPR_OP_SUB:
			case EXPR_OP_MUL:
			case EXPR_OP_DIV:
				{
					if(!is_ifm(lht) || !is_ifm(rht))
					{
						ERROR_CODE_MSG(ERR_SEM_OPERATOR_UNSUPPORTED, "Arithmetic operations not allowed on this type\n");
						return 0;
					}
					else if(_essl_type_equal(lht, rht))
					{
						/* all good */
						ESSL_CHECK_OOM(n->hdr.type = _essl_get_unqualified_type(ctx->pool, lht));
					}

					else if(lht->basic_type == rht->basic_type)
					{
						/* scalar-vector operations */
						if(lht->basic_type != TYPE_MATRIX_OF && GET_VEC_SIZE(lht) == 1 && n->hdr.kind != EXPR_KIND_ASSIGN)
						{
							ESSL_CHECK_OOM(n->hdr.type = _essl_get_unqualified_type(ctx->pool, rht));

						}
						else if(rht->basic_type != TYPE_MATRIX_OF && GET_VEC_SIZE(rht) == 1)
						{
							ESSL_CHECK_OOM(n->hdr.type = _essl_get_unqualified_type(ctx->pool, lht));
						}
						else
						{
							ERROR_CODE_TWOPARAMS(ERR_SEM_TYPE_MISMATCH, "Type mismatch in arithmetic operation between '%s' and '%s'\n", _essl_get_type_name(ctx->pool, lht), _essl_get_type_name(ctx->pool, rht));
							return 0;
						}
					}


					/* matrix-scalar ops */
					else if(lht->basic_type == TYPE_FLOAT && GET_VEC_SIZE(lht) == 1 && rht->basic_type == TYPE_MATRIX_OF && n->hdr.kind != EXPR_KIND_ASSIGN)
					{
						ESSL_CHECK_OOM(n->hdr.type = _essl_get_unqualified_type(ctx->pool, rht));

					}
					else if(lht->basic_type == TYPE_MATRIX_OF && rht->basic_type == TYPE_FLOAT && GET_VEC_SIZE(rht) == 1)
					{
						ESSL_CHECK_OOM(n->hdr.type = _essl_get_unqualified_type(ctx->pool, lht));
					}

					/* matrix multiplication */
					else if(op == EXPR_OP_MUL)
					{
						if(lht->basic_type == TYPE_FLOAT && rht->basic_type == TYPE_MATRIX_OF && GET_VEC_SIZE(lht) == GET_MATRIX_N_ROWS(rht))
						{
							const type_specifier *t;
							ESSL_CHECK_OOM(t = _essl_get_unqualified_type(ctx->pool, lht));
							ESSL_CHECK_OOM(n->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, t, GET_MATRIX_N_COLUMNS(rht)));
						}
						else if(lht->basic_type == TYPE_MATRIX_OF && rht->basic_type == TYPE_FLOAT && GET_MATRIX_N_COLUMNS(lht) == GET_VEC_SIZE(rht) && n->hdr.kind != EXPR_KIND_ASSIGN)
						{
							const type_specifier *t;
							ESSL_CHECK_OOM(t = _essl_get_unqualified_type(ctx->pool, rht));
							ESSL_CHECK_OOM(n->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, t, GET_MATRIX_N_ROWS(lht)));
						}
						else
						{
							ERROR_CODE_TWOPARAMS(ERR_SEM_TYPE_MISMATCH, "Type mismatch in arithmetic operation between '%s' and '%s'\n", _essl_get_type_name(ctx->pool, lht), _essl_get_type_name(ctx->pool, rht));
							return 0;
						}
					}					
					else
					{
						ERROR_CODE_TWOPARAMS(ERR_SEM_TYPE_MISMATCH, "Type mismatch in arithmetic operation between '%s' and '%s'\n", _essl_get_type_name(ctx->pool, lht), _essl_get_type_name(ctx->pool, rht));
						return 0;
					}

					return n;
				}

			case EXPR_OP_EQ:
			case EXPR_OP_NE:
				if(!_essl_type_equal(lht, rht))
				{
					ERROR_CODE_TWOPARAMS(ERR_SEM_TYPE_MISMATCH, "Cannot compare '%s' with '%s'\n", _essl_get_type_name(ctx->pool, lht), _essl_get_type_name(ctx->pool, rht));
					return 0;
				}

				if(type_is_or_has_array(lht))
				{
					ERROR_CODE_MSG(ERR_SEM_OPERATOR_UNSUPPORTED, "Cannot compare arrays\n");
					return 0;
				}

				if (_essl_type_is_or_has_sampler(lht))
				{
					ERROR_CODE_MSG(ERR_SEM_OPERATOR_UNSUPPORTED, "Cannot compare samplers\n");
					return 0;
				}

				n->hdr.type = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1);
				ESSL_CHECK_OOM(n->hdr.type);
				return n;
			case EXPR_OP_LT:
			case EXPR_OP_LE:
			case EXPR_OP_GE:
			case EXPR_OP_GT:
				if(!_essl_type_equal(lht, rht))
				{
					ERROR_CODE_TWOPARAMS(ERR_SEM_TYPE_MISMATCH, "Cannot compare '%s' with '%s'\n", _essl_get_type_name(ctx->pool, lht), _essl_get_type_name(ctx->pool, rht));
					return 0;
				}

				if(lht->basic_type != TYPE_INT && lht->basic_type != TYPE_FLOAT)
				{
					ERROR_CODE_MSG(ERR_SEM_OPERATOR_UNSUPPORTED, "Cannot compare non-scalar types\n");
					return 0;
				}
				
				if(GET_VEC_SIZE(lht) > 1)
				{
					ERROR_CODE_MSG(ERR_SEM_OPERATOR_UNSUPPORTED, "Cannot compare vectors or matrices\n");
					return 0;
				}
				n->hdr.type = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1);
				ESSL_CHECK_OOM(n->hdr.type);
				return n;
			

			case EXPR_OP_LOGICAL_AND:
			case EXPR_OP_LOGICAL_OR:
			case EXPR_OP_LOGICAL_XOR:
				n->hdr.type = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1);
				ESSL_CHECK_OOM(n->hdr.type);
				if(!_essl_type_equal(n->hdr.type, lht) || !_essl_type_equal(n->hdr.type, rht))
				{
					ERROR_CODE_MSG(ERR_SEM_OPERATOR_UNSUPPORTED, "Operands to &&, || and ^^ must have boolean type\n");
					return 0;
				}
				return n;

			case EXPR_OP_INDEX:

				/*
				  There are three possible kinds of index expressions in ESSL -
				  the operand can either be an array, a vector or a matrix.
				*/
				if(rht->basic_type != TYPE_INT || GET_VEC_SIZE(rht) > 1)
				{
					ERROR_CODE_MSG(ERR_SEM_ARRAY_PARAMETER_INTEGER, "Only integer expression allowed as array subscripts\n");
					return 0;
				}
				
				if(lht->basic_type == TYPE_ARRAY_OF)
				{
					const type_specifier *ts = lht->child_type;
					n->hdr.type = ts;
					if(ctx->desc->no_elimination_of_statically_indexed_arrays)
					{
						symbol *sym = _essl_symbol_for_node(child0);
						if(sym != NULL)
						{
							sym->opt.is_indexed_statically = ESSL_FALSE;
						}
						sym = _essl_symbol_for_node(child1);
						if(sym != NULL)
						{
							sym->opt.is_indexed_statically = ESSL_FALSE;
						}
					}
					if(_essl_node_is_constant(child1))
					{
						/* compile time checking of array bounds */
						int idx = ctx->desc->scalar_to_int(child1->expr.u.value[0]);
						if(idx < 0)
						{
							ERROR_CODE_MSG(ERR_SEM_ARRAY_INDEX_NEGATIVE, "Negative array subscript\n");
							return 0;
						}

						if(idx >= lht->u.array_size)
						{
							ERROR_CODE_MSG(ERR_SEM_ARRAY_INDEX_TOO_BIG, "Array subscript too big\n");
							return 0;
						}

					}else
					{
						symbol *sym = _essl_symbol_for_node(child0);
						if(sym != NULL)
						{
							sym->opt.is_indexed_statically = ESSL_FALSE;
						}
						sym = _essl_symbol_for_node(child1);
						if(sym != NULL)
						{
							sym->opt.is_indexed_statically = ESSL_FALSE;
						}
					}
					return n;
				}
				
				if(lht->basic_type == TYPE_MATRIX_OF)
				{
					n->hdr.type = lht->child_type;
					if(ctx->desc->no_elimination_of_statically_indexed_arrays)
					{
						symbol *sym = _essl_symbol_for_node(child0);
						if(sym != NULL)
						{
							sym->opt.is_indexed_statically = ESSL_FALSE;
						}
						sym = _essl_symbol_for_node(child1);
						if(sym != NULL)
						{
							sym->opt.is_indexed_statically = ESSL_FALSE;
						}
					}
					if(_essl_node_is_constant(child1))
					{
						/* compile time checking of vector bounds */
						int idx = ctx->desc->scalar_to_int(child1->expr.u.value[0]);
						if(idx < 0)
						{
							ERROR_CODE_MSG(ERR_SEM_ARRAY_INDEX_NEGATIVE, "Negative matrix subscript\n");
							return 0;
						}

						if((unsigned)idx >= GET_MATRIX_N_COLUMNS(lht))
						{
							ERROR_CODE_MSG(ERR_SEM_ARRAY_INDEX_TOO_BIG, "Matrix subscript too big\n");
							return 0;
						}

					}else
					{
						symbol *sym = _essl_symbol_for_node(child0);
						if(sym != NULL)
						{
							sym->opt.is_indexed_statically = ESSL_FALSE;
						}
						sym = _essl_symbol_for_node(child1);
						if(sym != NULL)
						{
							sym->opt.is_indexed_statically = ESSL_FALSE;
						}
					}
					return n;

				}


				if(is_ifb(lht) && GET_VEC_SIZE(lht) > 1)
				{
					ESSL_CHECK_OOM(n->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, lht, 1));

					if(_essl_node_is_constant(child1))
					{
						/* compile time checking of vector bounds */
						int idx = ctx->desc->scalar_to_int(child1->expr.u.value[0]);
						if(idx < 0)
						{
							ERROR_CODE_MSG(ERR_SEM_ARRAY_INDEX_NEGATIVE, "Negative vector subscript\n");
							return 0;
						}

						if((unsigned)idx >= GET_VEC_SIZE(lht))
						{
							ERROR_CODE_MSG(ERR_SEM_ARRAY_INDEX_TOO_BIG, "Vector subscript too big\n");
							return 0;
						}

					}
					return n;

				}
				ERROR_CODE_MSG(ERR_SEM_TYPE_MISMATCH, "Only arrays, vectors and matrices can be indexed\n");
				return 0;
				
			case EXPR_OP_CHAIN:
				n->hdr.type = child1->hdr.type;
				return n;
			default:
				assert(0);
				break;
			}

			break;
		case EXPR_KIND_TERNARY:
			assert(child0 != 0);
			assert(child1 != 0);
			child2 = GET_CHILD(n, 2);
			assert(child2 != 0);
			assert(lht != 0);
			switch(op)
			{
			case EXPR_OP_CONDITIONAL:
				if(lht->basic_type != TYPE_BOOL)
				{
					ERROR_CODE_MSG(ERR_SEM_TERNARY_PARAMETER_BOOL, "?: parameter must be of boolean type\n");
					return 0;
				}
				assert(child1->hdr.type != 0);
				assert(child2->hdr.type != 0);
				if(!_essl_type_equal(child1->hdr.type, child2->hdr.type))
				{
					ERROR_CODE_MSG(ERR_SEM_TERNARY_TYPE_MISMATCH, "2nd and 3rd parameters of ?: must have the same type\n");
					return 0;
				}
				if(_essl_type_is_or_has_sampler(child1->hdr.type) || _essl_type_is_or_has_sampler(child2->hdr.type))
				{
					ERROR_CODE_MSG(ERR_SEM_ASSIGNMENT_NOT_LVALUE, "Sampler can't be an l-value\n");
					return MEM_ERROR;
				}
				n->hdr.type = _essl_get_unqualified_type(ctx->pool, child1->hdr.type);
				ESSL_CHECK_OOM(n->hdr.type);
				return n;
			default:
				assert(0);
				break;
			}
			break;
		case EXPR_KIND_VARIABLE_REFERENCE:
			/* type will only be changed if the variable reference refers to a function parameter
			   as the correct values will have been set up already if the variable was defined
			   within the function body */
			{

				if ((n->expr.u.sym->type->basic_type == TYPE_SAMPLER_2D_SHADOW) &&
					(_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_EXT_SHADOW_SAMPLERS) == BEHAVIOR_WARN))
				{
					WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_EXT_shadow_samplers' used, variable '%s' with type 'sampler2DShadow' referred\n", n->expr.u.sym->name);
				}

				if ((n->expr.u.sym->type->basic_type == TYPE_SAMPLER_3D) &&
					(_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_EXT_SHADOW_SAMPLERS) == BEHAVIOR_WARN))
				{
					WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_OES_texture_3D' used, variable '%s' with type 'sampler3D' referred\n", n->expr.u.sym->name);
				}

				if ((n->expr.u.sym->address_space == ADDRESS_SPACE_FRAGMENT_SPECIAL) &&
				    (_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_ARM_FRAMEBUFFER_READ) == BEHAVIOR_WARN) &&
				    ((_essl_string_cmp(n->expr.u.sym->name, _essl_cstring_to_string_nocopy("gl_FBColor")) == 0) ||
					 (_essl_string_cmp(n->expr.u.sym->name, _essl_cstring_to_string_nocopy("gl_FBDepth")) == 0) ||
					 (_essl_string_cmp(n->expr.u.sym->name, _essl_cstring_to_string_nocopy("gl_FBStencil")) == 0)))
				{
					WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_ARM_framebuffer_read' used, variable '%s' referred\n", n->expr.u.sym->name);
				}

				ESSL_CHECK_OOM(n->hdr.type = _essl_get_unqualified_type(ctx->pool, n->expr.u.sym->type));
			}
			break;

		case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		case EXPR_KIND_FUNCTION_CALL:
			/*
			  We need to resolve overloads based on the types of the
			  parameters. The first possible function is in
			  n->expr.u.fun.sym, the rest are found by following the
			  sibling pointers.
			 */
			assert(n->expr.u.fun.sym != 0);
			{
				symbol *fun = n->expr.u.fun.sym;
				while(fun)
				{
					single_declarator *funparm = fun->parameters;
					for(i = 0; i < GET_N_CHILDREN(n) && funparm != 0; ++i)
					{
						node *callparm = GET_CHILD(n, i);
						assert(callparm != 0 && callparm->hdr.type != 0);
						if(!_essl_type_equal(callparm->hdr.type, funparm->type))
						{
							goto nextfun;
						}
						funparm = funparm->next;
					}
					
					if((GET_N_CHILDREN(n) != i) || (funparm != 0))
					{
						/* wrong number of arguments */
						goto nextfun;
					}
					/* make sure that "out" and "inout" arguments are lvalues */
					
					
					funparm = fun->parameters;
					for(i = 0; i < GET_N_CHILDREN(n) && funparm != 0; ++i)
					{
						node *callparm = GET_CHILD(n, i);
						assert(callparm != 0);
						if(funparm->qualifier.direction == DIR_OUT || funparm->qualifier.direction == DIR_INOUT)
						{
							if(funparm->type->type_qual & TYPE_QUAL_NOMODIFY)
							{
								const char *outstr = "out";
								const char *inoutstr = "inout";
								ERROR_CODE_THREEPARAMS(ERR_LP_SYNTAX_ERROR, "In function '%s' parameter %d has both 'const' and '%s' qualifier \n",
													_essl_string_to_cstring(ctx->pool, n->expr.u.fun.sym->name),
													i + 1,
													funparm->qualifier.direction == DIR_OUT ? outstr: inoutstr);
								return MEM_ERROR;
							}

							ESSL_CHECK_NONFATAL(check_lvalue(ctx, callparm));
						}
						funparm = funparm->next;
					}

					/* okay, we've got a match */
					n->expr.u.fun.sym = fun;
					ESSL_CHECK_OOM(n->hdr.type = _essl_get_unqualified_type(ctx->pool, fun->type));
					if (fun->kind == SYM_KIND_BUILTIN_FUNCTION)
					{
						n->expr.hdr.kind = EXPR_KIND_BUILTIN_FUNCTION_CALL;
						n->expr.operation = fun->builtin_function;

						/* Give warning if this function was enabled by an extension and the warn behavior was specified */

						if ((n->expr.operation == EXPR_OP_FUN_SHADOW2D     ||
						     n->expr.operation == EXPR_OP_FUN_SHADOW2DPROJ ) &&
						    (_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_EXT_SHADOW_SAMPLERS) == BEHAVIOR_WARN))
						{
							WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_EXT_shadow_samplers' used, function call to '%s'\n", n->expr.u.fun.sym->name);
						}

						if ((n->expr.operation == EXPR_OP_FUN_TEXTURE3D     ||
						     n->expr.operation == EXPR_OP_FUN_TEXTURE3DPROJ ||
						     n->expr.operation == EXPR_OP_FUN_TEXTURE3DLOD  ||
						     n->expr.operation == EXPR_OP_FUN_TEXTURE3DPROJLOD) &&
						    (_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_TEXTURE_3D) == BEHAVIOR_WARN))
						{
							WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_OES_texture_3D' used, function call to '%s'\n", n->expr.u.fun.sym->name);
						}


						if ((n->expr.operation == EXPR_OP_FUN_TEXTUREEXTERNAL     ||
						     n->expr.operation == EXPR_OP_FUN_TEXTUREEXTERNALPROJ ||
						     n->expr.operation == EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W) &&
						    (_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_TEXTURE_EXTERNAL) == BEHAVIOR_WARN))
						{
							WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_OES_texture_external' used, function call to '%s'\n", n->expr.u.fun.sym->name);
						}

						if ((n->expr.operation == EXPR_OP_FUN_TEXTURE2DLOD_EXT     ||
						     n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT ||
						     n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT ||
							 n->expr.operation == EXPR_OP_FUN_TEXTURECUBELOD_EXT ||
							 n->expr.operation == EXPR_OP_FUN_TEXTURE2DGRAD_EXT ||
							 n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT ||
							 n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT ||
							 n->expr.operation == EXPR_OP_FUN_TEXTURECUBEGRAD_EXT) &&
						    (_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_EXT_SHADER_TEXTURE_LOD) == BEHAVIOR_WARN))
						{
							WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_EXT_shader_texture_lod' used, function call to '%s'\n", n->expr.u.fun.sym->name);
						}


						if ((n->expr.operation == EXPR_OP_FUN_DFDX ||
						     n->expr.operation == EXPR_OP_FUN_DFDY ||
						     n->expr.operation == EXPR_OP_FUN_FWIDTH) &&
						    (_essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_STANDARD_DERIVATIVES) == BEHAVIOR_WARN))
						{
							WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_OES_standard_derivatives' used, function call to '%s'\n", n->expr.u.fun.sym->name);
						}

					}
					else
					{
						n->expr.hdr.kind = EXPR_KIND_FUNCTION_CALL;
						ESSL_CHECK(_essl_ptrset_insert(&ctx->function_calls, n));
					}
					return n;
				nextfun:
					fun = fun->next;
				}

				/* no matching overloads, give error */
				
				ERROR_CODE_STRING(ERR_SEM_TYPE_MISMATCH, "No matching overload for function '%s' found\n", n->expr.u.fun.sym->name);
#ifndef UNIT_TEST

				fun = n->expr.u.fun.sym;
				{
					string_buffer *buf;
					char *err_str;
					single_declarator *funparm = fun->parameters;

					ESSL_CHECK(buf = _essl_new_string_buffer(ctx->err_context->pool));
					ESSL_CHECK(_essl_string_buffer_put_formatted(buf, "Expected prototype is"));

					while(fun != NULL)
					{
						if(fun != n->expr.u.fun.sym)
						{
							ESSL_CHECK(_essl_string_buffer_put_formatted(buf, " or"));
							ESSL_CHECK(_essl_string_buffer_put_string(buf, &n->expr.u.fun.sym->name));
						}
						ESSL_CHECK(_essl_string_buffer_put_formatted(buf, " '"));
						ESSL_CHECK(_essl_string_buffer_put_string(buf, &n->expr.u.fun.sym->name));
						ESSL_CHECK(_essl_string_buffer_put_formatted(buf, " ("));
						for(funparm = fun->parameters; funparm != 0; funparm = funparm->next)
						{
							char *type_str;
							type_str = get_type_string_name(ctx, funparm->sym, funparm->type);
							if(funparm != fun->parameters)
							{
								ESSL_CHECK(_essl_string_buffer_put_formatted(buf, ", %s", type_str));
							}else
							{
								ESSL_CHECK(_essl_string_buffer_put_formatted(buf, "%s", type_str));
							}
						}
						ESSL_CHECK(_essl_string_buffer_put_formatted(buf, ")'"));
						fun = fun->next;
					}
					ESSL_CHECK(_essl_string_buffer_put_formatted(buf, "\n"));
					ESSL_CHECK(err_str = _essl_string_buffer_to_string(buf, ctx->err_context->pool));
					WARNING_CODE_STRING(ERR_WARNING, "%s", _essl_cstring_to_string(ctx->err_context->pool, err_str));

				}
#endif

				return 0;
				
			}


		case EXPR_KIND_BUILTIN_CONSTRUCTOR:

			{
				node *args = child0;
				assert(args != 0 && args->hdr.type != 0);
				assert(n->hdr.type != 0);
				if(n->hdr.type->basic_type == TYPE_MATRIX_OF && GET_N_CHILDREN(n) == 1 && args->hdr.type->basic_type == TYPE_MATRIX_OF)
				{
					/* matrix from single matrix. this is cool */

				} else if(GET_N_CHILDREN(n) == 1 && ((is_ifb(n->hdr.type) && GET_VEC_SIZE(n->hdr.type) > 1) || n->hdr.type->basic_type == TYPE_MATRIX_OF)
						  && is_ifb(args->hdr.type) && GET_VEC_SIZE(args->hdr.type) == 1)
				{
					/* vector from single argument that's a scalar integer, float or bool. also cool */
				} else {
					unsigned int ressize = _essl_get_type_size(n->hdr.type);
					unsigned int argsize = 0;
					unsigned int k;
					for(k = 0; argsize < ressize && k < GET_N_CHILDREN(n); ++k)
					{
						node *child = GET_CHILD(n, k);
						assert(child != 0 && child->hdr.type!= 0);
						if(n->hdr.type->basic_type == TYPE_MATRIX_OF && child->hdr.type->basic_type == TYPE_MATRIX_OF)
						{
							/* matrix from matrix */
							ERROR_CODE_MSG(ERR_SEM_CONSTRUCTOR_MATRIX_FROM_MATRIX, "Cannot construct a matrix from more than one matrix\n");
							return 0;
						}
						if(!is_ifbm(child->hdr.type))
						{
							ERROR_CODE_PARAM(ERR_SEM_TYPE_MISMATCH, "Cannot use arguments of type '%s' for built-in constructors\n", _essl_get_type_name(ctx->pool, child->hdr.type));
							return 0;
						}
						


						argsize += _essl_get_type_size(child->hdr.type);

					}
					if(argsize < ressize)
					{
						ERROR_CODE_MSG(ERR_SEM_CONSTRUCTOR_TOO_FEW_ARGS, "Too few arguments for constructor\n");
						return 0;
					}
					if(k < GET_N_CHILDREN(n))
					{
						ERROR_CODE_MSG(ERR_SEM_CONSTRUCTOR_ARG_UNUSED, "Argument unused in constructor\n");
						return 0;
						
					}


				}

			}
			break;
		case EXPR_KIND_STRUCT_CONSTRUCTOR:
			assert(n != 0);
			assert(GET_CHILD(n, 0) != 0);
			assert(n->hdr.type != 0);
			{
				unsigned k;
				single_declarator *parm = n->hdr.type->members;
				assert(parm != 0);
				for(k = 0; k < GET_N_CHILDREN(n) && parm != 0; ++k)
				{
					node *callparm = GET_CHILD(n, k);
					assert(callparm != 0 && callparm->hdr.type != 0);
					if(!_essl_type_equal(callparm->hdr.type, parm->type))
					{
						/* argument type mismatch */
						ERROR_CODE_THREEPARAMS(ERR_SEM_CONSTRUCTOR_WRONG_ARGS, "Type mismatch, cannot convert argument #%u from '%s' to '%s'\n", k + 1, _essl_get_type_name(ctx->pool, callparm->hdr.type), _essl_get_type_name(ctx->pool, parm->type));
						return 0;
					}
					parm = parm->next;
				}
				
				if(k < GET_N_CHILDREN(n))
				{
					ERROR_CODE_MSG(ERR_SEM_CONSTRUCTOR_ARG_UNUSED, "Too many arguments for struct constructor\n");
					return 0;
				}	
				if(parm != 0)
				{
					ERROR_CODE_MSG(ERR_SEM_CONSTRUCTOR_TOO_FEW_ARGS, "Too few arguments for struct constructor\n");
					return 0;
				}
				
			}
			return n;

		default:
			break;

		}
	}

	return n;
}

/** Typechecks a tree by post-order traversal */
static node *typecheck(typecheck_context *ctx, node *n)
{
	unsigned i;

	if(n->hdr.kind == DECL_KIND_FUNCTION)
	{
		ESSL_CHECK_NONFATAL((n = _essl_typecheck_single_node(ctx, n)));
	}

	for(i = 0; i < GET_N_CHILDREN(n); ++i)
	{
		node *child = GET_CHILD(n, i);
		if(child)
		{
			ESSL_CHECK_NONFATAL(child = typecheck(ctx, child));
			SET_CHILD(n, i, child);
		}
	}

	if(n->hdr.kind != DECL_KIND_FUNCTION)
	{
		ESSL_CHECK_NONFATAL((n = _essl_typecheck_single_node(ctx, n)));
		assert(GET_NODE_KIND(n->hdr.kind) != NODE_KIND_EXPRESSION || n->hdr.type != 0); /* all expression nodes should have type after type checking */
		assert(n->hdr.type == 0 || n->hdr.type->basic_type != TYPE_UNRESOLVED_ARRAY_OF); /* all unresolved_array_size's should have been transformed to array_size now */
		ESSL_CHECK_NONFATAL(n = _essl_constant_fold_single_node(&ctx->cfold_ctx, n));
	}

	return n;
}

/* Typechecks a tree */
node *_essl_typecheck(typecheck_context *ctx, node *n, /*@out@*/ ptrdict **node_to_precision_dict)
{
	node *nn = typecheck(ctx, n);

	if (nn == NULL) return NULL;

	if (nn->hdr.kind == NODE_KIND_TRANSLATION_UNIT)
	{
		/* Now go and adjust the symbol pointers of all function calls to point to the definition */
		{
			ptrset_iter fun_it;
			node *n;
			_essl_ptrset_iter_init(&fun_it, &ctx->function_calls);
			while ((n = _essl_ptrset_next(&fun_it)) != NULL)
			{
				symbol *fun = n->expr.u.fun.sym;
				while (!fun->body)
				{
					fun = _essl_ptrdict_lookup(&ctx->fun_decl_to_def, fun);
					if (fun == NULL)
					{
						ERROR_CODE_STRING(ERR_LP_UNDEFINED_IDENTIFIER, "Function '%s' not defined\n", n->expr.u.fun.sym->name);
						return 0;
					}
				}
				n->expr.u.fun.sym = fun;
			}
		}
	}

	ESSL_CHECK_NONFATAL(_essl_calculate_precision(&ctx->prec_ctx, nn));
	if(node_to_precision_dict != NULL)
	{
		*node_to_precision_dict = ctx->prec_ctx.node_to_precision_dict;
	}

	return nn;
}

/* Stores references for the other modules that are called when type checking */
memerr _essl_typecheck_init(typecheck_context *ctx, mempool *pool, error_context *err_ctx, typestorage_context *ts_ctx, target_descriptor *desc, language_descriptor *lang_desc)
{
	ctx->pool = pool;
	ctx->err_context = err_ctx;
	ctx->typestor_context = ts_ctx;
	ctx->desc = desc;
	ctx->lang_desc = lang_desc;
	ESSL_CHECK(ctx->strbuf = _essl_new_string_buffer(err_ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&ctx->function_calls, pool));
	ESSL_CHECK(_essl_ptrdict_init(&ctx->fun_decl_to_def, pool));
	ESSL_CHECK(_essl_precision_init_context(&ctx->prec_ctx, ctx->pool, ctx->desc, ctx->typestor_context, ctx->err_context));
	ESSL_CHECK(_essl_constant_fold_init(&ctx->cfold_ctx, pool, ctx->desc, err_ctx));

	return MEM_OK;
}


/** Checks if given type is or contains an array
  * Checks if given type is an array.
  * If it is TYPE_STRUCT, then each member is checked (recursivly)
  * @param type The type specifier to check
  * @return ESSL_TRUE if it is or contains an array, ESSL_FALSE if not
  */
static essl_bool type_is_or_has_array(const type_specifier* type)
{
	if (type->basic_type == TYPE_ARRAY_OF || type->basic_type == TYPE_UNRESOLVED_ARRAY_OF)
	{
		return ESSL_TRUE;
	}

	if (type->basic_type == TYPE_STRUCT)
	{
		single_declarator* sub_decl = type->members;
		while (sub_decl != NULL)
		{
			if (type_is_or_has_array(sub_decl->type))
			{
				return ESSL_TRUE;
			}

			sub_decl = sub_decl->next;
		}
	}

	return ESSL_FALSE;
}



#ifdef UNIT_TEST

#include "frontend/parser.h"
#include "frontend/frontend.h"

int main(void)
{
	target_descriptor *desc;
	frontend_context *ctx;
	typecheck_context tctx;
	char *buf[] = {

		"2 + 7", "",
		"\n\n6 + 7 * 3 + 8.33 - 1 - (8 - 2)", "0:3: S0001: Type mismatch in arithmetic operation between 'int' and 'float'\n",
		"2 + 05", "",
		"2.0 + vec4(1.0, 2.0, 3.0, 4.9)", "", 
		"ivec3(1, 2, 3).zzx", "",
		"ivec3(1, 2, 3).zzxw", "0:1: S0026: Swizzle field selector out of range\n",
		"vec4(1, 2, 3, 4).rx", "0:1: S0025: Vector swizzle \"rx\" mixes components from different sets\n",
		"vec4(1, 2, 3, 4).zzxwww", "0:1: S0026: Vector swizzle \"zzxwww\" is bigger than 4\n", 


		
		"precision mediump float; uniform int adzomg;\n"
		"\n"
		"void azomg(int rofl, const bool dude);\n"
		"// comment\n"
		"/* another comment */\n"
		"void main(int lololol)\n"
		"{\n"
		"	int lolololol;\n"
		"	if(true)\n"
		"	{\n"
		"\n"
		"		const float a = 4.0, c = 3.4;\n"
		"		const int def = 34;\n"
		"		vec3 z;\n"
		"		int dd[3+def];\n"
		"		z[2];\n"
		"		while(bool b = a < 5.1 + 2.0)\n"
		"		{\n"
		"			z.xy += 3.0;\n"
		"		}\n"
		"\n"
		"		for(int i = 0; i < 4; ++i)\n"
		"		{\n"
		"			a;\n"
		"		}\n"
		"		//d;\n"
		"		return;\n"
		"\n"
		"	}\n"
		"}\n", "",

		"precision mediump float;\n"
		"int a(int b, float c) { return 4; }\n"
		"int a(vec2 e) { return 4; }\n"
		"void main(void) { a(4, 5.0); }", "",

		"precision mediump float;\n"
		"int a(int b, float c) { return 4; }\n"
		"int a(vec2 e) { return 4; }\n"
		"void main(void) { a(vec2(4, 5.0)); }", "",
		
		"precision mediump float;\n"
		"int a(int b, float c) { return 4; }\n"
		"int a(vec2 e) { return 4; }\n"
		"void main(void) { a(5.0); }", "0:4: S0001: No matching overload for function 'a' found\n",


		"precision mediump float;\n"
		"int a(int b, float c) { return 4.0; }\n", "0:2: S0042: Type mismatch, cannot convert from 'float' to 'int'\n",

		"precision mediump float;\n"
		"void a(int b, float c) { return 4; }\n", "0:2: S0039: Function declared void but return statement has an argument\n",

		"precision mediump float;\n"
		"int a(int b, float c) { return; }\n", "0:2: S0038: Function declared with a return value but return statement has no argument.\n",

		"struct blargh { int a; };", "",
		"struct blargh { int a, b; };", "", 
		"struct blargh { int a; int b; };", "", 
		"struct blargh { int a; int b; } abc;", "", 
		"struct { int a; int b; } abc;", "", 
		"struct { float cd; };", "",
		"void main() { struct { int a; int b; } abc; }", "", 
		"void main() { struct abc { int a; vec3 b[4]; } test; test.a = 5; test.b[2] = vec3(1.0, 2.0, 3.0); }" , "0:1: S0032: no default precision defined for member\n", 
		"precision mediump float; void main() { struct { int a; vec3 b[4]; } test; struct { int a; vec3 c[4]; } test2; test.a = 5; test.b[2] = vec3(1.0, 2.0, 3.0); }" , "", 
		
		"precision mediump float; struct blargh { int a; vec3 b[4]; }; void main() { blargh test; \n test.a = 5; \n test.b[2] = vec3(1.0, 2.0, 3.0); }", "", 

		"precision mediump float;\n"
		"struct simple_struct {\n"
			"float b;\n"
			"int a;\n"
		"};"
		"void main(void) { simple_struct(0.5, 1); }", "",

		"precision mediump float;\n"
		"struct simple_struct {\n"
			"float b;\n"
			"int a;\n"
		"};"
		"void main(void) { simple_struct(0, 1); }", "0:5: S0007: Type mismatch, cannot convert argument #1 from 'int' to 'float'\n",

		"precision lowp float;\n"
		"struct simple_struct {\n"
			"float b;\n"
			"int a;\n"
		"};"
		"void main(void) { simple_struct(0.5, 1).b; }", "",

		"precision mediump float;\n"
		"struct simple_struct {\n"
			"float b;\n"
			"int a;\n"
		"};"
		"void main(void) { simple_struct(0.5, 1).b = 0.5; }", "0:5: S0027: L-value is constant\n",

		"int a(int b, in int c); void main(void) { a(0, 1); }", "0:1: L0002: Function 'a' not defined\n",

		"int a(int b, in int c) { return 0; } void main(void) { a(0, 1); }", "",
		"int a(in int b, inout int c) { return 0; } void main(void) { int d; a(0, d); }", "",
		"int a(in int b, inout int c); void main(void) { a(0, 3); }", "0:1: S0027: L-value is constant\n",
		"int a(out int b, in int c); void main(void) { a(0, 3); }", "0:1: S0027: L-value is constant\n",

		"int zomg() { return 0; } int main() { return zomg() + 5; }", "",
		"int zomg() { return 0; } int main() { return zomg() + 5.0; }", "0:1: S0001: Type mismatch in arithmetic operation between 'int' and 'float'\n",


		"precision mediump float; void main() { vec3 a = vec3(1, 2.0, 2); }", "",
		"precision mediump float; void main() { vec3 a = vec3(1, 2.0, 2, 4); }", "0:1: S0008: Argument unused in constructor\n",
		"precision mediump float; void main() { vec3 a = vec3(1, 2.0); }", "0:1: S0009: Too few arguments for constructor\n",
		"precision mediump float; void main() { vec3 a = vec3(1); }", "",
		"precision mediump float; void main() { mat2 a = mat2(1); }", "",
		"precision mediump float; void main() { mat4 b; mat2 a = mat2(b); }", "",
		"precision mediump float; void main() { mat2 b; mat3 a = mat3(b); }", "",
		"precision mediump float; void main() { mat2 b; mat3 a = mat3(b, b); }", "0:1: S0010: Cannot construct a matrix from more than one matrix\n",

		"precision mediump float; void main() { struct { float a; } b; mat2 a = mat2(b); }", "0:1: S0001: Cannot use arguments of type 'unnamed struct' for built-in constructors\n",
		"precision mediump float; void main() { float b[2]; mat2 a = mat2(b); }", "0:1: S0001: Cannot use arguments of type 'float[2]' for built-in constructors\n", 


	};

	const int numbufs = sizeof(buf)/sizeof(buf[0])/2;
	int lengths[1];
	int i;
	char *error_log;

	mempool_tracker tracker;
	mempool p, *pool = &p;
	compiler_options* opts;

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&p, 0, &tracker);

	opts = _essl_new_compiler_options(pool);
	desc = _essl_new_target_descriptor(pool, TARGET_FRAGMENT_SHADER, opts);


	for (i = 0; i < numbufs; ++i)
	{
		node *expr;
		lengths[0] = strlen(buf[2*i]);
		
		ctx = _essl_new_frontend(pool, desc, buf[2*i], lengths, 1);
		assert(ctx);
		assert(_essl_typecheck_init(&tctx, pool, ctx->err_context, ctx->typestor_context, desc, ctx->lang_desc));

		if(i < 8)
		{
			expr = _essl_parse_expression(&ctx->parse_context);
		} else {
			expr = _essl_parse_translation_unit(&ctx->parse_context);
		}
		if(expr)
		{
			_essl_typecheck(&tctx, expr, NULL);
		}
		error_log = _essl_mempool_alloc(pool, _essl_error_get_log_size(ctx->err_context));
		assert(error_log);
		_essl_error_get_log(ctx->err_context, error_log, _essl_error_get_log_size(ctx->err_context));

		if(!expr || strcmp(error_log, buf[2*i+1]))
		{
			fprintf(stderr, "\n\n");
			fprintf(stderr, "Source:\n%s\n", buf[2*i]);
			fprintf(stderr, "Failure in test %i\n", i);
			fprintf(stderr, "Expected error log: '%s'\n", buf[2*i+1]);
			fprintf(stderr, "Actual error log: '%s'\n", error_log);
			assert(0);
		}
		assert(expr);
	}

	fprintf(stderr, "All tests OK!\n");
	_essl_mempool_destroy(&p);

	return 0;
}

#endif /* UNIT_TEST */



