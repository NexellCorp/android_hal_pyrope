/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <ctype.h>
#include "common/lir_printer.h"


#define TERM_KIND_NAME(name) case name: return #name
#define ADDR_SPACE_NAME(name) case name: return #name
#define SCOPE_KIND_NAME(name) case name: return #name
#define EXPR_NAME(expr) case expr: return #expr

static int get_bb_nbr(control_flow_graph *graph, basic_block *bb)
{
	unsigned i;
	for (i = 0; i < graph->n_blocks; i++)
	{
		if (graph->output_sequence[i] == bb)
		{
			return i;
		}
	}
	return -1;
}

static const char * _essl_get_node_name(expression_operator op)
{
	switch (op)
	{
		EXPR_NAME(EXPR_OP_UNKNOWN);
		EXPR_NAME(EXPR_OP_IDENTITY);
		EXPR_NAME(EXPR_OP_SPILL);               
		EXPR_NAME(EXPR_OP_SPLIT);
		EXPR_NAME(EXPR_OP_SCHEDULER_SPLIT);
		EXPR_NAME(EXPR_OP_MEMBER_OR_SWIZZLE);
		EXPR_NAME(EXPR_OP_MEMBER);
		EXPR_NAME(EXPR_OP_SWIZZLE);           
		EXPR_NAME(EXPR_OP_NOT); 
		EXPR_NAME(EXPR_OP_PRE_INC); 
		EXPR_NAME(EXPR_OP_POST_INC);  
		EXPR_NAME(EXPR_OP_PRE_DEC);
		EXPR_NAME(EXPR_OP_POST_DEC);
		EXPR_NAME(EXPR_OP_NEGATE); 
		EXPR_NAME(EXPR_OP_PLUS);
		EXPR_NAME(EXPR_OP_ADD);
		EXPR_NAME(EXPR_OP_SUB); 
		EXPR_NAME(EXPR_OP_MUL);
		EXPR_NAME(EXPR_OP_DIV);
		EXPR_NAME(EXPR_OP_LT);
		EXPR_NAME(EXPR_OP_LE);
		EXPR_NAME(EXPR_OP_EQ);
		EXPR_NAME(EXPR_OP_NE);
		EXPR_NAME(EXPR_OP_GE);
		EXPR_NAME(EXPR_OP_GT);  
		EXPR_NAME(EXPR_OP_INDEX);
		EXPR_NAME(EXPR_OP_CHAIN); 
		EXPR_NAME(EXPR_OP_LOGICAL_AND); 
		EXPR_NAME(EXPR_OP_LOGICAL_OR);
		EXPR_NAME(EXPR_OP_LOGICAL_XOR);
		EXPR_NAME(EXPR_OP_SUBVECTOR_ACCESS); 
		EXPR_NAME(EXPR_OP_ASSIGN);                
		EXPR_NAME(EXPR_OP_ADD_ASSIGN);            
		EXPR_NAME(EXPR_OP_SUB_ASSIGN);            
		EXPR_NAME(EXPR_OP_MUL_ASSIGN);            
		EXPR_NAME(EXPR_OP_DIV_ASSIGN);            
		EXPR_NAME(EXPR_OP_CONDITIONAL);           
		EXPR_NAME(EXPR_OP_CONDITIONAL_SELECT);    
		EXPR_NAME(EXPR_OP_SUBVECTOR_UPDATE);      
		EXPR_NAME(EXPR_OP_EXPLICIT_TYPE_CONVERT);
		EXPR_NAME(EXPR_OP_IMPLICIT_TYPE_CONVERT);
		EXPR_NAME(EXPR_OP_FUN_RADIANS);
		EXPR_NAME(EXPR_OP_FUN_DEGREES);
		EXPR_NAME(EXPR_OP_FUN_SIN);
		EXPR_NAME(EXPR_OP_FUN_COS);
		EXPR_NAME(EXPR_OP_FUN_TAN);
		EXPR_NAME(EXPR_OP_FUN_ASIN);
		EXPR_NAME(EXPR_OP_FUN_ACOS);
		EXPR_NAME(EXPR_OP_FUN_ATAN);
		EXPR_NAME(EXPR_OP_FUN_POW);
		EXPR_NAME(EXPR_OP_FUN_EXP);
		EXPR_NAME(EXPR_OP_FUN_LOG);
		EXPR_NAME(EXPR_OP_FUN_EXP2);
		EXPR_NAME(EXPR_OP_FUN_LOG2);
		EXPR_NAME(EXPR_OP_FUN_SQRT);
		EXPR_NAME(EXPR_OP_FUN_INVERSESQRT);
		EXPR_NAME(EXPR_OP_FUN_ABS);
		EXPR_NAME(EXPR_OP_FUN_SIGN);
		EXPR_NAME(EXPR_OP_FUN_FLOOR);
		EXPR_NAME(EXPR_OP_FUN_CEIL);
		EXPR_NAME(EXPR_OP_FUN_FRACT);
		EXPR_NAME(EXPR_OP_FUN_MOD);
		EXPR_NAME(EXPR_OP_FUN_MIN);
		EXPR_NAME(EXPR_OP_FUN_MAX);
		EXPR_NAME(EXPR_OP_FUN_CLAMP);
		EXPR_NAME(EXPR_OP_FUN_MIX);
		EXPR_NAME(EXPR_OP_FUN_STEP);
		EXPR_NAME(EXPR_OP_FUN_SMOOTHSTEP);
		EXPR_NAME(EXPR_OP_FUN_LENGTH);
		EXPR_NAME(EXPR_OP_FUN_DISTANCE);
		EXPR_NAME(EXPR_OP_FUN_DOT);
		EXPR_NAME(EXPR_OP_FUN_CROSS);
		EXPR_NAME(EXPR_OP_FUN_NORMALIZE);
		EXPR_NAME(EXPR_OP_FUN_FACEFORWARD);
		EXPR_NAME(EXPR_OP_FUN_REFLECT);
		EXPR_NAME(EXPR_OP_FUN_REFRACT);
		EXPR_NAME(EXPR_OP_FUN_MATRIXCOMPMULT);
		EXPR_NAME(EXPR_OP_FUN_LESSTHAN);
		EXPR_NAME(EXPR_OP_FUN_LESSTHANEQUAL);
		EXPR_NAME(EXPR_OP_FUN_GREATERTHAN);
		EXPR_NAME(EXPR_OP_FUN_GREATERTHANEQUAL);
		EXPR_NAME(EXPR_OP_FUN_EQUAL);
		EXPR_NAME(EXPR_OP_FUN_NOTEQUAL);
		EXPR_NAME(EXPR_OP_FUN_ANY);
		EXPR_NAME(EXPR_OP_FUN_ALL);
		EXPR_NAME(EXPR_OP_FUN_NOT);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE1D);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE1DPROJ);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE1DLOD);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE1DPROJLOD);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2D);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DPROJ);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DPROJ_W);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DLOD);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DPROJLOD);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DPROJLOD_W);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE3D);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE3DPROJ);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE3DLOD);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE3DPROJLOD);
		EXPR_NAME(EXPR_OP_FUN_TEXTURECUBE);
		EXPR_NAME(EXPR_OP_FUN_TEXTURECUBELOD);
		EXPR_NAME(EXPR_OP_FUN_SHADOW2D);
		EXPR_NAME(EXPR_OP_FUN_SHADOW2DPROJ);
		EXPR_NAME(EXPR_OP_FUN_SHADOW2DLOD);
		EXPR_NAME(EXPR_OP_FUN_SHADOW2DPROJLOD);
		EXPR_NAME(EXPR_OP_FUN_TEXTUREEXTERNAL);
		EXPR_NAME(EXPR_OP_FUN_TEXTUREEXTERNALPROJ);
		EXPR_NAME(EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DLOD_EXT);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT);
		EXPR_NAME(EXPR_OP_FUN_TEXTURECUBELOD_EXT);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DGRAD_EXT);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT);
		EXPR_NAME(EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT);
		EXPR_NAME(EXPR_OP_FUN_TEXTURECUBEGRAD_EXT);

		EXPR_NAME(EXPR_OP_FUN_DFDX);
		EXPR_NAME(EXPR_OP_FUN_DFDY);
		EXPR_NAME(EXPR_OP_FUN_FWIDTH);
		EXPR_NAME(EXPR_OP_FUN_TRUNC); 
		EXPR_NAME(EXPR_OP_FUN_RCP); 
		EXPR_NAME(EXPR_OP_FUN_RCC); 
		EXPR_NAME(EXPR_OP_FUN_SIN_0_1); 
		EXPR_NAME(EXPR_OP_FUN_COS_0_1); 
		EXPR_NAME(EXPR_OP_FUN_SIN_PI); 
		EXPR_NAME(EXPR_OP_FUN_COS_PI); 
		EXPR_NAME(EXPR_OP_FUN_CLAMP_0_1); 
		EXPR_NAME(EXPR_OP_FUN_CLAMP_M1_1); 
		EXPR_NAME(EXPR_OP_FUN_MAX_0); 
		EXPR_NAME(EXPR_OP_FUN_FMULN); 
		EXPR_NAME(EXPR_OP_FUN_M200_ATAN_IT1);
		EXPR_NAME(EXPR_OP_FUN_M200_ATAN_IT2);
		EXPR_NAME(EXPR_OP_FUN_M200_HADD);
		EXPR_NAME(EXPR_OP_FUN_M200_HADD_PAIR);
		EXPR_NAME(EXPR_OP_FUN_M200_POS);
		EXPR_NAME(EXPR_OP_FUN_M200_POINT);
		EXPR_NAME(EXPR_OP_FUN_M200_MISC_VAL);
		EXPR_NAME(EXPR_OP_FUN_M200_LD_RGB);
		EXPR_NAME(EXPR_OP_FUN_M200_LD_ZS);
		EXPR_NAME(EXPR_OP_FUN_M200_DERX); 
		EXPR_NAME(EXPR_OP_FUN_M200_DERY); 
		EXPR_NAME(EXPR_OP_FUN_M200_MOV_CUBEMAP);
		EXPR_NAME(EXPR_OP_FUN_MALIGP2_FIXED_TO_FLOAT); 
		EXPR_NAME(EXPR_OP_FUN_MALIGP2_FLOAT_TO_FIXED); 
		EXPR_NAME(EXPR_OP_FUN_MALIGP2_FIXED_EXP2); 
		EXPR_NAME(EXPR_OP_FUN_MALIGP2_LOG2_FIXED); 
		EXPR_NAME(EXPR_OP_FUN_MALIGP2_MUL_EXPWRAP);
	default:
		assert(0 && "unknown expression operator");
		return 0;
	}

}

static const char *  _essl_get_term_kind_name(term_kind kind)
{
	switch (kind)
	{
		TERM_KIND_NAME(TERM_KIND_UNKNOWN);
		TERM_KIND_NAME(TERM_KIND_JUMP);
		TERM_KIND_NAME(TERM_KIND_DISCARD);
		TERM_KIND_NAME(TERM_KIND_EXIT);
	default:
		assert(0 && "unknown termination kind");
		return 0;
	}
}

static const char *get_addr_space_name(address_space_kind addr_space)
{
	switch (addr_space)
	{
		ADDR_SPACE_NAME(ADDRESS_SPACE_UNKNOWN);
		ADDR_SPACE_NAME(ADDRESS_SPACE_THREAD_LOCAL);
		ADDR_SPACE_NAME(ADDRESS_SPACE_GLOBAL);
		ADDR_SPACE_NAME(ADDRESS_SPACE_UNIFORM);
		ADDR_SPACE_NAME(ADDRESS_SPACE_ATTRIBUTE);
		ADDR_SPACE_NAME(ADDRESS_SPACE_VERTEX_VARYING);
		ADDR_SPACE_NAME(ADDRESS_SPACE_FRAGMENT_VARYING);
		ADDR_SPACE_NAME(ADDRESS_SPACE_FRAGMENT_SPECIAL);
		ADDR_SPACE_NAME(ADDRESS_SPACE_FRAGMENT_OUT);
		ADDR_SPACE_NAME(ADDRESS_SPACE_REGISTER);
	default:
		return 0;
	}
}

static const char *get_scope_kind_name(scope_kind sk)
{
	switch (sk)
	{
		SCOPE_KIND_NAME(SCOPE_UNKNOWN);
		SCOPE_KIND_NAME(SCOPE_LOCAL);
		SCOPE_KIND_NAME(SCOPE_FORMAL);
		SCOPE_KIND_NAME(SCOPE_ARGUMENT);
		SCOPE_KIND_NAME(SCOPE_GLOBAL);
	default:
		return 0;
	}
}



static memerr  print_expr_name(lir_printer_context *ctx, expression_operator op)
{
	const char *s;
	unsigned int i;
	char str[32];

	s = _essl_get_node_name(op);

	for (i = 0; i < (sizeof(str) - 1) && s[i] != 0; i++)
	{
		str[i] = tolower(s[i]);
	}
	str[i] = 0;

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s ", &str[8]));

	return MEM_OK;
}


static memerr  print_type_1(lir_printer_context *ctx, const type_specifier *t)
{
	if (t->basic_type == TYPE_BOOL ||
		t->basic_type == TYPE_FLOAT ||
		t->basic_type == TYPE_INT)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "<%d x ", t->u.basic.vec_size));
	}
	switch(t->basic_type)
	{
	case TYPE_BOOL:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "b"));
		break;
	case TYPE_FLOAT:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "f"));
		break;
	case TYPE_INT:
		if (t->u.basic.int_signedness == INT_SIGNED)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "i"));
		}
		else
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "u"));
		}
		break;
	case TYPE_VOID:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "void"));
		break;
	case TYPE_ARRAY_OF:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "[%d x ", t->u.array_size));
		ESSL_CHECK(print_type_1(ctx, t->child_type));
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "]"));
		break;
	case TYPE_STRUCT:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "{"));
		{
			single_declarator *member = t->members;
			while (member)
			{
				ESSL_CHECK(print_type_1(ctx, member->type));
				member = member->next;
				if (member)
				{
					ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ","));
					ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, " "));
				}
			}
		}
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "}"));
		break;

	default:
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "?"));
		break;
	}
	if (t->basic_type != TYPE_VOID)
	{
		if (t->u.basic.scalar_size == SIZE_BITS8)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "8"));
		}
		else if (t->u.basic.scalar_size == SIZE_BITS16)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "16"));
		}
		else if (t->u.basic.scalar_size == SIZE_BITS32)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "32"));
		}
		else if (t->u.basic.scalar_size == SIZE_BITS64)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "64"));
		}
		else if (t->u.basic.scalar_size == SIZE_UNKNOWN &&
				 (t->basic_type == TYPE_ARRAY_OF || t->basic_type == TYPE_STRUCT))
		{
			/* Print nothing */
		}
		else
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "??"));
		}
	}
	if (t->basic_type == TYPE_BOOL ||
		t->basic_type == TYPE_FLOAT ||
		t->basic_type == TYPE_INT)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ">"));
	}

	return MEM_OK;
}

static memerr  print_type(lir_printer_context *ctx, const type_specifier *t)
{
	ESSL_CHECK(print_type_1(ctx, t));
	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, " "));

	return MEM_OK;
}

static memerr  print_constant(lir_printer_context *ctx, node *n)
{
	unsigned i, m;
	type_basic bt;

	assert(n->hdr.kind == EXPR_KIND_CONSTANT);

	m = _essl_get_type_size(n->hdr.type);
	bt = n->hdr.type->basic_type;

	if (m > 1)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "("));
	}
	for (i = 0; i < m; i++)
	{
		if (bt == TYPE_BOOL)
		{
			essl_bool val;
			val = ctx->desc->scalar_to_bool(n->expr.u.value[i]);
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s", val ? "true" : "false"));
		}
		else if (bt == TYPE_INT)
		{
			unsigned long long val;
			val = ctx->desc->scalar_to_int(n->expr.u.value[i]);
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "0x%llx", val));
		}
		else if (bt == TYPE_FLOAT)
		{
			double val;
			val = ctx->desc->scalar_to_float(n->expr.u.value[i]);
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%e", val));
		}else
		{
			/* image, sampler, whatever. just print the hex value */
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "(%08x)", *((unsigned *)&n->expr.u.value[i])));
		}

		if (i != (m - 1))
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ", "));
		}
	}
	if (m > 1)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ")"));
	}
	return MEM_OK;
}


static memerr  print_node(lir_printer_context *ctx, node *child)
{
	const char *s;
	ESSL_CHECK(s = _essl_unique_name_get_or_create(&ctx->node_names, child));
	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s ", s));
	return MEM_OK;
}

static memerr  print_children(lir_printer_context *ctx, node *n)
{
	unsigned i;

	if (n->hdr.kind == EXPR_KIND_UNARY &&
		n->expr.operation == EXPR_OP_SWIZZLE)
	{
		unsigned j;
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "0s"));
		for (j = 0; j < n->hdr.type->u.basic.vec_size; j++)
		{
			if (n->expr.u.swizzle.indices[j] >= 0)
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%x", n->expr.u.swizzle.indices[j]));
			}
			else
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "~"));
			}
		}
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, " "));
	}
	else if (n->hdr.kind == EXPR_KIND_VECTOR_COMBINE)
	{
		unsigned j;
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "0c"));
		for (j = 0; j < n->hdr.type->u.basic.vec_size; j++)
		{
			if (n->expr.u.combiner.mask[j] != -1)
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%x", n->expr.u.combiner.mask[j]));
			}
			else
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "~"));
			}
		}
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, " "));
	}

	for (i = 0; i < GET_N_CHILDREN(n); i++)
	{
		node *child = GET_CHILD(n, i);
		if(child != NULL)
		{
			ESSL_CHECK(print_node(ctx, child));
		}else
		{

		}
		if (i != (GET_N_CHILDREN(n) - 1))
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ", "));
		}
	}
	return MEM_OK;
}


static operation_dependency *find_control_dependences(basic_block *bb, node *n)
{
	control_dependent_operation *op;
	for (op = bb->control_dependent_ops; op != 0; op = op->next)
	{
		if (op->op == n)
		{
			return op->dependencies;
		}
	}
	return 0;
}


static memerr  print_control_dep(lir_printer_context *ctx, basic_block *bb, node *n)
{
	operation_dependency *dependencies;

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "    control_dep "));
	ESSL_CHECK(print_node(ctx, n));
	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ": "));

	dependencies = find_control_dependences(bb, n);
	if (dependencies != 0)
	{
		const char *name;
		for (; dependencies != 0; dependencies = dependencies->next)
		{
			control_dependent_operation *op = dependencies->dependent_on;
			ESSL_CHECK(name = _essl_unique_name_get_or_create(&ctx->node_names, op->op));
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s", name));
			if (dependencies->next != 0)
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ", "));
			}
		}
	}
	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "\n"));

	return MEM_OK;
}


static essl_bool is_phi_in_bb(basic_block *bb, node *n)
{
	phi_list *phi;
	for (phi = bb->phi_nodes; phi != 0; phi = phi->next)
	{
		if (n == phi->phi_node)
		{
			return ESSL_TRUE;
		}
	}
	return ESSL_FALSE;
}


static memerr  process_node(lir_printer_context *ctx, basic_block *bb, node *n)
{
	unsigned i;

	if (n->hdr.kind == EXPR_KIND_PHI && !is_phi_in_bb(bb, n))
	{
		/* This phi-node lives in another basic block.  It will be printed
		 * there. */
		return MEM_OK;
	}

	if (_essl_ptrset_has(&ctx->visited, n))
	{
		return MEM_OK;
	}
	ESSL_CHECK(_essl_ptrset_insert(&ctx->visited, n));

	if (n->hdr.kind == EXPR_KIND_PHI)
	{
		phi_source *src;
		for (src = n->expr.u.phi.sources; src != 0; src = src->next)
		{
			ESSL_CHECK(process_node(ctx, bb, src->source));
		}
	}
	else
	{
		for (i = 0; i < GET_N_CHILDREN(n); i++)
		{
			node *child = GET_CHILD(n, i);
			if (child != NULL)
			{
				ESSL_CHECK(process_node(ctx, bb, child));
			}
		}
	}

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "  "));
	ESSL_CHECK(print_node(ctx, n));
	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, " = "));

	if (n->hdr.kind == EXPR_KIND_UNARY ||
		n->hdr.kind == EXPR_KIND_BINARY ||
		n->hdr.kind == EXPR_KIND_ASSIGN ||
		n->hdr.kind == EXPR_KIND_TERNARY ||
		n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL )
	{
		ESSL_CHECK(print_expr_name(ctx, n->expr.operation));

		ESSL_CHECK(print_type(ctx, n->hdr.type));

		ESSL_CHECK(print_children(ctx, n));
	}
	else if (n->hdr.kind == EXPR_KIND_CONSTANT)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "constant "));
		ESSL_CHECK(print_type(ctx, n->hdr.type));
		ESSL_CHECK(print_constant(ctx, n));
	}
	else if (n->hdr.kind == EXPR_KIND_PHI)
	{
		phi_source *src;
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "phi "));
		ESSL_CHECK(print_type(ctx, n->hdr.type));
		for (src = n->expr.u.phi.sources; src != 0; src = src->next)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "["));
			ESSL_CHECK(print_node(ctx, src->source));
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ", %%bb%d]", ctx->function ? get_bb_nbr(ctx->function->control_flow_graph, src->join_block) : -1));

			if (src->next != 0)
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ", "));
			}
		}
	}
	else if (n->hdr.kind == EXPR_KIND_FUNCTION_CALL)
	{
		const char *name;
		parameter *param;
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "call "));
		ESSL_CHECK(print_type(ctx, n->hdr.type));
		ESSL_CHECK(name = _essl_string_to_cstring(ctx->pool, n->expr.u.fun.sym->name));
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s", name));
		param = n->expr.u.fun.arguments;
		while (param != NULL)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, " "));
			if (param->store != NULL)
			{
				ESSL_CHECK(print_node(ctx, param->store->n));
			}
			else
			{
				/* This should not happen. But it does. */
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "???"));
			}
			param = param->next;
			if (param != NULL)
			{
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ","));
			}
		}
	}
	else if (n->hdr.kind == EXPR_KIND_DEPEND)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "DEPEND!!!! "));
	}
	else if (n->hdr.kind == EXPR_KIND_TYPE_CONVERT)
	{
		ESSL_CHECK(print_expr_name(ctx, n->expr.operation));
		ESSL_CHECK(print_type(ctx, GET_CHILD(n, 0)->hdr.type));
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "to "));
		ESSL_CHECK(print_type(ctx, n->hdr.type));
		ESSL_CHECK(print_children(ctx, n));
	}
	else if (n->hdr.kind == EXPR_KIND_VECTOR_COMBINE)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "vector_combine "));
		ESSL_CHECK(print_type(ctx, n->hdr.type));
		ESSL_CHECK(print_children(ctx, n));
	}
	else if (n->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		const char *name;
		symbol *sym = n->expr.u.sym;
		ESSL_CHECK(name = _essl_string_to_cstring(ctx->pool, sym->name));
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "address @%s", name));
	}
	else if (n->hdr.kind == EXPR_KIND_BUILTIN_CONSTRUCTOR)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "constructor "));
		ESSL_CHECK(print_type(ctx, n->hdr.type));
		ESSL_CHECK(print_children(ctx, n));
	}
	else if (n->hdr.kind == EXPR_KIND_DONT_CARE)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "undef "));
		ESSL_CHECK(print_type(ctx, n->hdr.type));
	}
	else if (n->hdr.kind == EXPR_KIND_TRANSFER)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "transfer "));
		ESSL_CHECK(print_children(ctx, n));
	}
	else if (n->hdr.kind == EXPR_KIND_LOAD)
	{
		const char *addr_space_str = get_addr_space_name(n->expr.u.load_store.address_space);
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "load (%s) ", addr_space_str));
		ESSL_CHECK(print_children(ctx, n));
	}
	else if (n->hdr.kind == EXPR_KIND_STORE)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "store "));
		ESSL_CHECK(print_children(ctx, n));
	}else if (n->hdr.kind == DECL_KIND_VARIABLE)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "var (%s) ", _essl_string_to_cstring(ctx->pool, n->decl.sym->name)));
		ESSL_CHECK(print_type(ctx, n->hdr.type));
	}
	else if (GET_NODE_KIND(n->hdr.kind) == NODE_KIND_STATEMENT)
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "stmt "));
	}
	else
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "OTHER KIND %d!!!! ", n->hdr.kind));
	}

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "\n"));

	if (n->hdr.is_control_dependent)
	{
		ESSL_CHECK(print_control_dep(ctx, bb, n));
	}

	return MEM_OK;
}

memerr  _essl_lir_basic_block_to_txt(lir_printer_context *ctx, basic_block *bb)
{
	preallocated_var *p;
	control_dependent_operation *cd;
	phi_list *phi;
	const char *term_kind_name;

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "\n"));

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "bb%d:  ; (po%d)\n",
										bb->output_visit_number,
										bb->postorder_visit_number));

	for (p = bb->preallocated_defs; p != NULL; p = p->next)
	{
		ESSL_CHECK(process_node(ctx, bb, p->var));
	}

	for (cd = bb->control_dependent_ops; cd != NULL; cd = cd->next)
	{
		ESSL_CHECK(process_node(ctx, bb, cd->op));
	}

	/* Phi-nodes are written as they are used during the normal
	 * node processing, but we need to pass over the list of phi-nodes
	 * here so that the phi-nodes unused in this basic block still
	 * get printed here. */
	for (phi = bb->phi_nodes; phi != 0; phi = phi->next)
	{
		ESSL_CHECK(process_node(ctx, bb, phi->phi_node));
	}

	for (p = bb->preallocated_uses; p != NULL; p = p->next)
	{
		ESSL_CHECK(process_node(ctx, bb, p->var));
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "  prealloc_use "));
		ESSL_CHECK(print_node(ctx, p->var));
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "\n"));
	}

	assert(bb->branch_instruction == 0);
	ESSL_CHECK(term_kind_name = _essl_get_term_kind_name(bb->termination));
	if (bb->source != 0)
	{
		ESSL_CHECK(process_node(ctx, bb, bb->source));

		if (bb->termination == TERM_KIND_JUMP)
		{
			basic_block *succ;
			int bb_nbr;

			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "  br "));
			ESSL_CHECK(print_node(ctx, bb->source));

			succ = bb->successors[BLOCK_TRUE_TARGET];
			bb_nbr = succ->output_visit_number;
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ", label %%bb%d [T]", bb_nbr));
			succ = bb->successors[BLOCK_DEFAULT_TARGET];
			bb_nbr = succ->output_visit_number;
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ", label %%bb%d [F]", bb_nbr));
		}
		else
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "  %s ", term_kind_name));
			ESSL_CHECK(print_node(ctx, bb->source));

			if (bb->termination == TERM_KIND_DISCARD)
			{
				basic_block *succ = bb->successors[BLOCK_DEFAULT_TARGET];
				int bb_nbr = succ->output_visit_number;
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, ", bb%d", bb_nbr));
			}
		}
	}
	else
	{
		if (bb->termination == TERM_KIND_JUMP ||
			bb->termination == TERM_KIND_DISCARD)
		{
			basic_block *succ = bb->successors[BLOCK_DEFAULT_TARGET];
			int bb_nbr = succ->output_visit_number;
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, " br label %%bb%d", bb_nbr));
		}
		else if (bb->termination == TERM_KIND_EXIT)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "  exit"));
		}
		else
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "  %s", term_kind_name));
		}
	}
	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "\n"));

	return MEM_OK;
}

memerr lir_function_to_txt(FILE *out, mempool *pool, symbol *function, target_descriptor *desc)
{
	lir_printer_context ctx;
	const char *fun_name;
	control_flow_graph *graph;
	unsigned i;
	char *res_str;

	ctx.pool = pool;
	ctx.function = function;
	ctx.desc = desc;
	ESSL_CHECK(ctx.strbuf = _essl_new_string_buffer(pool));
	ESSL_CHECK(_essl_ptrset_init(&ctx.visited, pool));
	ESSL_CHECK(_essl_unique_name_init(&ctx.node_names, pool, "%"));

	ESSL_CHECK(fun_name = _essl_string_to_cstring(pool, function->name));
	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "%s:\n", fun_name));

	graph = function->control_flow_graph;
	for (i = 0; i < graph->n_blocks; i++)
	{
		basic_block *bb = graph->output_sequence[i];
		control_dependent_operation *g;
		preallocated_var *p;
		phi_list *phi;
		const char *term_kind_name;

		if (i != 0)
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "\n"));
		}
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "bb%d ():  ; (po%d)\n",
										i,
										bb->postorder_visit_number));

		for (p = bb->preallocated_defs; p != NULL; p = p->next)
		{
			ESSL_CHECK(process_node(&ctx, bb, p->var));
		}

		for (g = bb->control_dependent_ops; g != 0; g = g->next)
		{
			ESSL_CHECK(process_node(&ctx, bb, g->op));
		}

		/* Phi-nodes are written as they are used during the normal
		 * node processing, but we need to pass over the list of phi-nodes
		 * here so that the phi-nodes unused in this basic block still
		 * get printed here. */
		for (phi = bb->phi_nodes; phi != 0; phi = phi->next)
		{
			ESSL_CHECK(process_node(&ctx, bb, phi->phi_node));
		}

		for (p = bb->preallocated_uses; p != NULL; p = p->next)
		{
			const char *s;
			ESSL_CHECK(process_node(&ctx, bb, p->var));
			ESSL_CHECK(s = _essl_unique_name_get_or_create(&ctx.node_names, p->var));
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "  prealloc_use %s\n", s));
		}

		assert(bb->branch_instruction == 0);
		ESSL_CHECK(term_kind_name = _essl_get_term_kind_name(bb->termination));
		if (bb->source != 0)
		{
			const char *src_name;
			ESSL_CHECK(process_node(&ctx, bb, bb->source));
			ESSL_CHECK(src_name = _essl_unique_name_get_or_create(&ctx.node_names, bb->source));

			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "  %s %s", term_kind_name, src_name));
			if (bb->termination == TERM_KIND_JUMP)
			{
				int bb_true_nbr = get_bb_nbr(graph, bb->successors[BLOCK_TRUE_TARGET]);
				int bb_false_nbr = get_bb_nbr(graph, bb->successors[BLOCK_DEFAULT_TARGET]);
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, ", T=bb%d, F=bb%d", bb_true_nbr, bb_false_nbr));
			}
			else if (bb->termination == TERM_KIND_DISCARD)
			{
				int bb_nbr = get_bb_nbr(graph, bb->successors[BLOCK_DEFAULT_TARGET]);
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, ", bb%d", bb_nbr));
			}
		}
		else
		{
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "  %s", term_kind_name));
			if (bb->termination == TERM_KIND_JUMP ||
				bb->termination == TERM_KIND_DISCARD)
			{
				int bb_nbr = get_bb_nbr(graph, bb->successors[BLOCK_DEFAULT_TARGET]);
				ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, " bb%d", bb_nbr));
			}
		}
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "\n"));
	}

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "\n\n"));

	ESSL_CHECK(res_str = _essl_string_buffer_to_string(ctx.strbuf, pool));
	fputs(res_str, out);
	ESSL_CHECK(_essl_string_buffer_reset(ctx.strbuf));

	return MEM_OK;
}

memerr ast_function_to_txt(FILE *out, mempool *pool, symbol *function, target_descriptor *desc)
{
	lir_printer_context ctx;
	const char *fun_name;
	char *res_str;

	ctx.pool = pool;
	ctx.function = function;
	ctx.desc = desc;
	ESSL_CHECK(ctx.strbuf = _essl_new_string_buffer(pool));
	ESSL_CHECK(_essl_ptrset_init(&ctx.visited, pool));
	ESSL_CHECK(_essl_unique_name_init(&ctx.node_names, pool, "%"));

	ESSL_CHECK(fun_name = _essl_string_to_cstring(pool, function->name));
	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "%s:\n", fun_name));

	ESSL_CHECK(process_node(&ctx, NULL, function->body));

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "\n\n"));

	ESSL_CHECK(res_str = _essl_string_buffer_to_string(ctx.strbuf, pool));
	fputs(res_str, out);
	ESSL_CHECK(_essl_string_buffer_reset(ctx.strbuf));

	return MEM_OK;
}


static memerr 
print_variables(lir_printer_context *ctx, mempool *pool, symbol_list *sl)
{
	for ( ; sl != 0; sl = sl->next)
	{
		symbol *variable = sl->sym;
		const char *addr_space_str = get_addr_space_name(variable->address_space);
		const char *scope_str = get_scope_kind_name(variable->scope);
		if (!addr_space_str)
		{
			addr_space_str = "UNKNOWN_ADDRESS_SPACE";
		}
		if (!scope_str)
		{
			scope_str = "UNKNOWN_SCOPE";
		}
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf,
													 "@%s = addrspace(%d) ",
													 _essl_string_to_cstring(pool, variable->name),
													 variable->address_space));
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf,
													 "scope(%d) ",
													 variable->scope));
		ESSL_CHECK(print_type(ctx, variable->type));
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, " ;; %s %s\n",
													 addr_space_str, scope_str));
	}

	return MEM_OK;
}

static memerr 
lir_variables_to_txt(FILE *out, mempool *pool, translation_unit *tu)
{
	lir_printer_context ctx;
	char *res_str;

	ctx.pool = pool;
	ctx.function = NULL;
	ctx.desc = NULL;
	ESSL_CHECK(ctx.strbuf = _essl_new_string_buffer(pool));

	ESSL_CHECK(print_variables(&ctx, pool, tu->globals));
	ESSL_CHECK(print_variables(&ctx, pool, tu->uniforms));
	ESSL_CHECK(print_variables(&ctx, pool, tu->vertex_attributes));
	ESSL_CHECK(print_variables(&ctx, pool, tu->vertex_varyings));
	ESSL_CHECK(print_variables(&ctx, pool, tu->fragment_varyings));
	ESSL_CHECK(print_variables(&ctx, pool, tu->fragment_specials));
	ESSL_CHECK(print_variables(&ctx, pool, tu->fragment_outputs));

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx.strbuf, "\n"));

	ESSL_CHECK(res_str = _essl_string_buffer_to_string(ctx.strbuf, pool));
	fputs(res_str, out);

	return MEM_OK;
}

memerr lir_translation_unit_to_txt(FILE *out, mempool *pool, translation_unit *tu, target_descriptor *desc)
{
	symbol_list *sl;

	ESSL_CHECK(lir_variables_to_txt(out, pool, tu));

	for (sl = tu->functions; sl != 0; sl = sl->next)
	{
		symbol *function = sl->sym;

		ESSL_CHECK(lir_function_to_txt(out, pool, function, desc));
	}

	return MEM_OK;
}

memerr ast_translation_unit_to_txt(FILE *out, mempool *pool, translation_unit *tu, target_descriptor *desc)
{
	symbol_list *sl;

	ESSL_CHECK(lir_variables_to_txt(out, pool, tu));

	for (sl = tu->functions; sl != 0; sl = sl->next)
	{
		symbol *function = sl->sym;

		ESSL_CHECK(ast_function_to_txt(out, pool, function, desc));
	}

	return MEM_OK;
}
