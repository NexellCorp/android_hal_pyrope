/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/middle_printer.h"
#include "common/essl_common.h"
#include "common/essl_stringbuffer.h"
#include "common/ptrdict.h"
#include "common/ptrset.h"
#include "common/basic_block.h"
#include "common/unique_names.h"
#include "backend/extra_info.h"

typedef struct 
{
	mempool *pool;
	string_buffer *strbuf;
	unique_name_context node_names;
	unique_name_context subgraph_names;
	ptrset visited;
	ptrdict block_to_nodes;
	target_descriptor *desc;
	control_flow_graph *cfg;
	essl_bool display_extra_info;
} function_to_dot_context;



static const char* describe_type_specifier(mempool *pool, const type_specifier *ts)
{
	const char *base = NULL;
	const char *pre = NULL;

	if (ts == NULL)
	{
		return "<null type>";
	}

	switch (ts->basic_type)
	{
	case TYPE_MATRIX_OF:
	{
		unsigned n_columns = GET_MATRIX_N_COLUMNS(ts);
		unsigned n_rows = GET_MATRIX_N_ROWS(ts);
		size_t buf_size = 50;
		char *buf;

		buf = _essl_mempool_alloc(pool, buf_size);
		if (buf == NULL)
		{
			return "<error: out of memory while describing type>";
		}
		if(n_columns == n_rows)
		{
			(void)snprintf(buf, buf_size, "mat%u", n_columns);
		} else {
			(void)snprintf(buf, buf_size, "mat%ux%u", n_rows, n_columns);
		}
		return buf;
	}
	case TYPE_UNKNOWN:
		base = "unknown";
		break;
	case TYPE_VOID:
		base = "void";
		break;
	case TYPE_FLOAT:
		base = "float";
		pre = "vec";
		break;
	case TYPE_INT:
		base = "int";
		pre = "ivec";
		break;
	case TYPE_BOOL:
		base = "bool";
		pre = "bvec";
		break;
	case TYPE_SAMPLER_2D:
		base = "sampler2D";
		break;
	case TYPE_SAMPLER_3D:
		base = "sampler3D";
		break;
	case TYPE_SAMPLER_CUBE:
		base = "samplerCube";
		break;
	case TYPE_SAMPLER_2D_SHADOW:
		base = "sampler2DShadow";
		break;
	case TYPE_SAMPLER_EXTERNAL:
		base = "samplerExternalOES";
		break;
	case TYPE_STRUCT:
		base = _essl_string_to_cstring(pool, ts->name);
		break;

	case TYPE_ARRAY_OF:
	case TYPE_UNRESOLVED_ARRAY_OF:
	{
		const char *s;
		size_t buf_size;
		char *buf;
		s = describe_type_specifier(pool, ts->child_type);
		buf_size = strlen(s) + 20;

		buf = _essl_mempool_alloc(pool, buf_size);
		if (buf == NULL)
		{
			return "<error: out of memory while describing type>";
		}
		if(ts->basic_type == TYPE_ARRAY_OF)
		{
			(void)snprintf(buf, buf_size, "%s[%d]", s, ts->u.array_size);
		} else {
			(void)snprintf(buf, buf_size, "%s[]", s);
		}
		return buf;
	}


	}

	if (base != NULL)
	{
		if (pre == NULL || GET_VEC_SIZE(ts) == 1)
		{
			return base;
		}
		else
		{
			size_t buf_size;
			char *buf;

			assert(pre != NULL);
			assert(GET_VEC_SIZE(ts) > 1 && GET_VEC_SIZE(ts) < 100);

			buf_size = strlen(pre) + 3;
			buf = _essl_mempool_alloc(pool, buf_size);
			if (buf == NULL)
			{
				return "<error: out of memory while describing type>";
			}
			(void)snprintf(buf, buf_size, "%s%u", pre, GET_VEC_SIZE(ts));
			return buf;
		}
	}

	return "<unknown type>";
}



const char *_essl_label_for_node(mempool *pool, target_descriptor *desc, node *n)
{
	char *buf;
	const char *cbuf;
	size_t i;
	switch(n->hdr.kind)
	{
	case EXPR_KIND_UNARY:
	case EXPR_KIND_BINARY:
		switch(n->expr.operation)
		{
		case EXPR_OP_MEMBER:
			return "member";
		case EXPR_OP_SWIZZLE:
			buf = _essl_mempool_alloc(pool, N_COMPONENTS+2);
			if(!buf) return "<error>";
			buf[0] = '.';
			buf[N_COMPONENTS+1] = '\0';
			for(i = 0; i < N_COMPONENTS; ++i)
			{
				char c;
				switch(n->expr.u.swizzle.indices[i])
				{
				case -1: c = '?'; break;
				case  0: c = 'x'; break;
				case  1: c = 'y'; break;
				case  2: c = 'z'; break;
				case  3: c = 'w'; break;
				case  4: c = '4'; break;
				case  5: c = '5'; break;
				case  6: c = '6'; break;
				case  7: c = '7'; break;
				case  8: c = '8'; break;
				case  9: c = '9'; break;
				case 10: c = 'a'; break;
				case 11: c = 'b'; break;
				case 12: c = 'c'; break;
				case 13: c = 'd'; break;
				case 14: c = 'e'; break;
				case 15: c = 'f'; break;
				default: c = '!'; break;
					
				}
				buf[i+1] = c;

			}
			for(i = N_COMPONENTS+1; i --> 1; )
			{
				if(buf[i] != '?') break;
				buf[i] = '\0';
			}
			return buf;
		case EXPR_OP_NOT:
			return "!";
		case EXPR_OP_NEGATE:
			return "-";
		case EXPR_OP_ADD:
			return "+";
		case EXPR_OP_SUB:
			return "-";
		case EXPR_OP_MUL:
			return "*";
		case EXPR_OP_DIV:
			return "/";
			
		case EXPR_OP_LT:
			return "&lt;";
		case EXPR_OP_LE:
			return "&lt;=";
		case EXPR_OP_EQ:
			return "==";
		case EXPR_OP_NE:
			return "!=";
		case EXPR_OP_GE:
			return "&gt;=";
		case EXPR_OP_GT:
			return "&gt;";
			
		case EXPR_OP_INDEX:
			return "[]";

		case EXPR_OP_LOGICAL_AND:
			return "AND";
		case EXPR_OP_LOGICAL_OR:
			return "OR";
		case EXPR_OP_LOGICAL_XOR:
			return "^^";
		case EXPR_OP_CHAIN:
			return ",";

		case EXPR_OP_SUBVECTOR_ACCESS:
			return "subvector access";
		case EXPR_OP_IDENTITY:
			return "identity";
		default:
			return "unknown expression";
		}

	case EXPR_KIND_VECTOR_COMBINE:
		buf = _essl_mempool_alloc(pool, 100);
		if(!buf) return "<error>";
		(void)snprintf(buf, 100, "combine %d%d%d%d", 
					   n->expr.u.combiner.mask[0],
					   n->expr.u.combiner.mask[1],
					   n->expr.u.combiner.mask[2],
					   n->expr.u.combiner.mask[3]);

		return buf;
	case EXPR_KIND_TERNARY:
		if(n->expr.operation == EXPR_OP_SUBVECTOR_UPDATE)
		{
			return "subvector update";
		} else if(n->expr.operation == EXPR_OP_CONDITIONAL_SELECT)
		{
			return "csel";
		}
		return "unknown ternary operation";
	case EXPR_KIND_ASSIGN:
		return "=";
	case EXPR_KIND_LOAD:
		switch(n->expr.u.load_store.address_space)
		{
		case ADDRESS_SPACE_THREAD_LOCAL:
			return "load local";
		case ADDRESS_SPACE_GLOBAL:
			return "load global";
		case ADDRESS_SPACE_UNIFORM:
			return "load uniform";
		case ADDRESS_SPACE_ATTRIBUTE:
			return "load attribute";
		case ADDRESS_SPACE_VERTEX_VARYING:
			return "load varying";
		case ADDRESS_SPACE_FRAGMENT_VARYING:
			return "load varying";
		case ADDRESS_SPACE_FRAGMENT_SPECIAL:
			return "load special";
		case ADDRESS_SPACE_FRAGMENT_OUT:
			return "load fragment out";
		default:
			assert(0);
			return "load unknown";

		}
	case EXPR_KIND_STORE:
		return "store";
	case EXPR_KIND_VARIABLE_REFERENCE:
	    {
			const char *nm = _essl_string_to_cstring(pool, n->expr.u.sym->name);
			if(nm == 0) return "ERROR";
			return nm;
		}
	case EXPR_KIND_CONSTANT:
		assert(n->hdr.type != 0);
	    {
			char buf2[1000];
			char *bufptr = buf2;
			unsigned m = _essl_get_type_size(n->hdr.type);
			unsigned j;
			for(j = 0; j < m; ++j)
			{
				type_basic bt = n->hdr.type->basic_type;
				assert(bufptr + 50 < buf2 + 1000);

				/* Use target descriptor for printing if available */
				if (desc) {
					if(bt == TYPE_BOOL)
					{
						(void)snprintf(bufptr, 50, "%s", desc->scalar_to_bool(n->expr.u.value[j]) ? "true" : "false");
					} else if(bt == TYPE_INT)
					{
						(void)snprintf(bufptr, 50, "%i", desc->scalar_to_int(n->expr.u.value[j]));
					} else
					{
						(void)snprintf(bufptr, 50, "%f", desc->scalar_to_float(n->expr.u.value[j]));
					}
				} else {
					(void)snprintf(bufptr, 50, "(%08x)", *((unsigned *)&n->expr.u.value[j]));
				}
				bufptr += strlen(bufptr);
				if(j+1 < m)
				{
					assert(bufptr + 5 < buf2 + 1000);
					(void)snprintf(bufptr, 5, ", ");
					bufptr += strlen(bufptr);
				}
				
			}
			bufptr = _essl_mempool_alloc(pool, strlen(buf2)+1);
			if(bufptr == 0) return "ERROR";
			memcpy(bufptr, buf2, strlen(buf2)+1);
			return bufptr;
	}
	case EXPR_KIND_FUNCTION_CALL:
		return "call";
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		switch(n->expr.operation)
		{
		case EXPR_OP_FUN_RADIANS:
			return "radians";
		case EXPR_OP_FUN_DEGREES:
			return "degrees";

		case EXPR_OP_FUN_SIN:
			return "sin";
		case EXPR_OP_FUN_COS:
			return "cos";
		case EXPR_OP_FUN_TAN:
			return "tan";
		case EXPR_OP_FUN_ASIN:
			return "asin";
		case EXPR_OP_FUN_ACOS:
			return "acos";
		case EXPR_OP_FUN_ATAN:
			return "atan";

		case EXPR_OP_FUN_POW:
			return "pow";
		case EXPR_OP_FUN_EXP:
			return "exp";
		case EXPR_OP_FUN_LOG:
			return "log";
		case EXPR_OP_FUN_EXP2:
			return "exp2";
		case EXPR_OP_FUN_LOG2:
			return "log2";
		case EXPR_OP_FUN_SQRT:
			return "sqrt";
		case EXPR_OP_FUN_INVERSESQRT:
			return "inversesqrt";

		case EXPR_OP_FUN_ABS:
			return "abs";
		case EXPR_OP_FUN_SIGN:
			return "sign";
		case EXPR_OP_FUN_FLOOR:
			return "floor";
		case EXPR_OP_FUN_CEIL:
			return "ceil";
		case EXPR_OP_FUN_FRACT:
			return "fract";
		case EXPR_OP_FUN_MOD:
			return "mod";
		case EXPR_OP_FUN_MIN:
			return "min";
		case EXPR_OP_FUN_MAX:
			return "max";
		case EXPR_OP_FUN_CLAMP:
			return "clamp";
		case EXPR_OP_FUN_MIX:
			return "mix";
		case EXPR_OP_FUN_STEP:
			return "step";
		case EXPR_OP_FUN_SMOOTHSTEP:
			return "smoothstep";

		case EXPR_OP_FUN_LENGTH: return "length";
		case EXPR_OP_FUN_DISTANCE: return "distance";
		case EXPR_OP_FUN_DOT: return "dot";
		case EXPR_OP_FUN_CROSS: return "cross";
		case EXPR_OP_FUN_NORMALIZE: return "normalize";
		case EXPR_OP_FUN_FACEFORWARD: return "faceforward";
		case EXPR_OP_FUN_REFLECT: return "reflect";
		case EXPR_OP_FUN_REFRACT: return "refract";

/* matrix functions */
		case EXPR_OP_FUN_MATRIXCOMPMULT: return "matrixCompMult";

/* vector relational functions */
		case EXPR_OP_FUN_LESSTHAN: return "lessThan";
		case EXPR_OP_FUN_LESSTHANEQUAL: return "lessThanEqual";
		case EXPR_OP_FUN_GREATERTHAN: return "greaterThan";
		case EXPR_OP_FUN_GREATERTHANEQUAL: return "greaterThanEqual";
		case EXPR_OP_FUN_EQUAL: return "equal";
		case EXPR_OP_FUN_NOTEQUAL: return "notEqual";
		case EXPR_OP_FUN_ANY: return "any";
		case EXPR_OP_FUN_ALL: return "all";
		case EXPR_OP_FUN_NOT: return "not";

			/* texture lookup functions */
		case EXPR_OP_FUN_TEXTURE2D: return "texture2D";
		case EXPR_OP_FUN_TEXTURE2DPROJ: return "texture2DProj";
		case EXPR_OP_FUN_TEXTURE2DPROJ_W: return "texture2DProj";
		case EXPR_OP_FUN_TEXTURE2DLOD: return "texture2DLod";
		case EXPR_OP_FUN_TEXTURE2DPROJLOD: return "texture2DProjLod";
		case EXPR_OP_FUN_TEXTURE2DPROJLOD_W: return "texture2DProjLod";
		case EXPR_OP_FUN_TEXTURE3D: return "texture3D";
		case EXPR_OP_FUN_TEXTURE3DPROJ: return "texture3DProj";
		case EXPR_OP_FUN_TEXTURE3DLOD: return "texture3DLod";
		case EXPR_OP_FUN_TEXTURE3DPROJLOD: return "texture3DProjLod";
		case EXPR_OP_FUN_TEXTURECUBE: return "textureCube";
		case EXPR_OP_FUN_TEXTURECUBELOD: return "textureCubeLod";
		case EXPR_OP_FUN_TEXTUREEXTERNAL: return "texture2D (external)";
		case EXPR_OP_FUN_TEXTUREEXTERNALPROJ: return "texture2DProj (external)";
		case EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W: return "texture2DProj (external)";
		case EXPR_OP_FUN_TEXTURE2DLOD_EXT: return "texture2DLodEXT";
		case EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT: return "texture2DProjLodEXT";
		case EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT: return "texture2DProjLodEXT";
		case EXPR_OP_FUN_TEXTURECUBELOD_EXT: return "textureCubeLodEXT";
		case EXPR_OP_FUN_TEXTURE2DGRAD_EXT: return "texture2DGradEXT";
		case EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT: return "texture2DProjGradEXT";
		case EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT: return "texture2DProjGradEXT_w";
		case EXPR_OP_FUN_TEXTURECUBEGRAD_EXT: return "textureCubeGradEXT";

/* fragment processin functions */
		case EXPR_OP_FUN_DFDX: return "dDdx";
		case EXPR_OP_FUN_DFDY: return "dDdy";
		case EXPR_OP_FUN_FWIDTH: return "fwidth";

/* noise functions */
		case EXPR_OP_FUN_M200_HADD: return "hadd";
		case EXPR_OP_FUN_M200_HADD_PAIR: return "hadd_pair";
		case EXPR_OP_FUN_M200_ATAN_IT1: return "atan_it1";
		case EXPR_OP_FUN_M200_ATAN_IT2: return "atan_it2";
		case EXPR_OP_FUN_M200_MOV_CUBEMAP: return "move_cube_map";
		case EXPR_OP_FUN_SIN_0_1: return "sin_0_1";
		case EXPR_OP_FUN_COS_0_1: return "cos_0_1";
		case EXPR_OP_FUN_RCP: return "rcp";
		case EXPR_OP_FUN_RCC: return "rcc";
		case EXPR_OP_FUN_TRUNC: return "trunc";
		case EXPR_OP_FUN_MALIGP2_MUL_EXPWRAP: return "mul_expwrap";


		default:
			return "unknown built-in function";
		}
	case EXPR_KIND_BUILTIN_CONSTRUCTOR:
	case EXPR_KIND_STRUCT_CONSTRUCTOR:
	case EXPR_KIND_TYPE_CONVERT:
		assert(n->hdr.type != 0);
		cbuf =  describe_type_specifier(pool, n->hdr.type);
		if(!cbuf) return "ERROR";
		return cbuf;
	case EXPR_KIND_PHI:
		return "phi";
	case EXPR_KIND_DONT_CARE:
		return "don't care";
	case EXPR_KIND_TRANSFER:
		return "transfer";
	case EXPR_KIND_DEPEND:
		return "depend";
	default:
		return "unknown";
	};
}

static const char *dummy_style = "style=invis";
static const char *glob_op_style = "style=filled,fillcolor=orangered";
static const char *phi_style = "style=filled,fillcolor=lightyellow";
static const char *node_style = "style=filled,fillcolor=lightskyblue";


static const char *term_kind_text(basic_block *b)
{
	switch(b->termination)
	{
	case TERM_KIND_UNKNOWN:
		return "";
	case TERM_KIND_JUMP:
		return b->source ? "conditional" : "jump";
	case TERM_KIND_DISCARD:
		return b->source ? "conditional discard" : "discard";
	case TERM_KIND_EXIT:
		return "exit";
	default:
		assert(0);
	}
	return "<error>";
}

static memerr emit_node(function_to_dot_context *ctx, node *n, const char *style)
{
	char buf[50];
	const char *extra_text = "";
	const char *nm = _essl_unique_name_get_or_create(&ctx->node_names, n);
	int i;
	char liveness[N_COMPONENTS+1] = "xyzw";
	unsigned vec_size = 1;
	for (i = 0 ; i < N_COMPONENTS ; i++) {
		if ((n->hdr.live_mask & (1 << i)) == 0) {
			liveness[i] = '-';
		}
	}
	if(n->hdr.type->child_type == NULL && n->hdr.type->basic_type != TYPE_STRUCT && n->hdr.type->basic_type != TYPE_MATRIX_OF) vec_size = GET_NODE_VEC_SIZE(n);
	liveness[vec_size] = 0;

	ESSL_CHECK(nm);
	if(ctx->display_extra_info)
	{
		node_extra *ex;

		if(HAS_EXTRA_INFO(n))
		{
			ex = EXTRA_INFO(n);
			(void)snprintf(buf, 50, "pri %d %d-%d-%d %s", ex->operation_depth,
						   n->expr.earliest_block ? n->expr.earliest_block->output_visit_number : -1, 
						   n->expr.best_block ? n->expr.best_block->output_visit_number : -1, 
						   n->expr.latest_block ? n->expr.latest_block->output_visit_number : -1,
						   liveness);
			extra_text = buf;
		} else {
			(void)snprintf(buf, 50, "pri ? %d-%d-%d %s",
			               n->expr.earliest_block ? n->expr.earliest_block->output_visit_number : -1, 
			               n->expr.best_block ? n->expr.best_block->output_visit_number : -1, 
			               n->expr.latest_block ? n->expr.latest_block->output_visit_number : -1,
			               liveness);
			extra_text = buf;
		}
	} else {
		(void)snprintf(buf, 50, "%s", liveness);
		extra_text = buf;
	}

	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, 
	                                  "%s [label=<<table cellpadding=\"0\" border=\"0\" cellspacing=\"0\"><tr><td>%s</td></tr>",
	                                  nm, _essl_label_for_node(ctx->pool, ctx->desc, n)));
	if(extra_text != NULL && strcmp(extra_text, ""))
	{
		ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf,
		                                  "<tr><td><font point-size=\"10\" color=\"darkslategray\">%s</font></td></tr>",
		                                  extra_text));
	}
	ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "</table> >, %s];\n", style));

	return MEM_OK;
}

static node *get_node(node *n)
{
	if(n->hdr.kind == EXPR_KIND_TRANSFER)
    {
		node *child = GET_CHILD(n, 0);
		assert(child != 0);
		return get_node(child);
	}
	return n;

}

static memerr process_node(function_to_dot_context *ctx, node *n)
{

	unsigned i;
	const char *name;
	const char *nm;
	if(_essl_ptrset_has(&ctx->visited, n)) return MEM_OK;
	ESSL_CHECK(_essl_ptrset_insert(&ctx->visited, n));

	if(!(name = _essl_unique_name_get(&ctx->node_names, n)))
	{
		ESSL_CHECK(emit_node(ctx, n, node_style));
		ESSL_CHECK(name = _essl_unique_name_get(&ctx->node_names, n));
	}

	for(i = 0; i < GET_N_CHILDREN(n); ++i)
	{
		node *child = GET_CHILD(n, i);
		if(child)
		{
			node *d = get_node(child);
			ESSL_CHECK(process_node(ctx, d));
			ESSL_CHECK(nm = _essl_unique_name_get(&ctx->node_names, d));
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s -> %s;\n", nm, name));
		}
	}

	if(n->hdr.kind == EXPR_KIND_PHI)
	{
		phi_source *p; 
		for(p = n->expr.u.phi.sources; p != 0; p = p->next)
		{
			const char *cnode;
			const char *nm2;
			node *m;
			node *d = get_node(p->source);
			ESSL_CHECK(process_node(ctx, d));
			ESSL_CHECK(nm = _essl_unique_name_get(&ctx->node_names, d));
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s -> %s;\n", nm, name));
			ESSL_CHECK(m = _essl_ptrdict_lookup(&ctx->block_to_nodes, p->join_block));
			ESSL_CHECK(cnode = _essl_unique_name_get_or_create(&ctx->node_names, m));
			ESSL_CHECK(nm = _essl_unique_name_get_or_create(&ctx->node_names, d));
			ESSL_CHECK(nm2 = _essl_unique_name_get_or_create(&ctx->subgraph_names, p->join_block));
			ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s -> %s [lhead=%s,color=green,dir=none];\n", nm, cnode, nm2));
			

		}
	} else 	if(n->hdr.kind == EXPR_KIND_FUNCTION_CALL)
	{
		parameter *p; 
		for(p = n->expr.u.fun.arguments; p != 0; p = p->next)
		{
			if(p->store)
			{
				storeload_list* l = p->store;
				while(0 != l)
				{
					node *d = get_node(l->n);

					ESSL_CHECK(process_node(ctx, d));
					ESSL_CHECK(nm = _essl_unique_name_get(&ctx->node_names, d));
					ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s -> %s [color=yellow];\n", nm, name));
					
					l = l->next;
				}
			}

			if(p->load)
			{
				storeload_list* l = p->load;
				while(0 != l)
				{
					node *d = get_node(l->n);

					ESSL_CHECK(process_node(ctx, d));
					ESSL_CHECK(nm = _essl_unique_name_get(&ctx->node_names, d));
					ESSL_CHECK(_essl_string_buffer_put_formatted(ctx->strbuf, "%s -> %s [color=yellow];\n", name, nm));
					
					l = l->next;
				}
			}
			

		}
	}

	return MEM_OK;
}

static memerr debug_function_to_dot_buf(string_buffer *strbuf, mempool *pool, target_descriptor *desc, symbol *function, int display_extra_info)
{
	function_to_dot_context ctx;
	basic_block *b;
	const char *nm, *nm2;
	const char *fun_name = _essl_string_to_cstring(pool, function->name); /* OPT: could use full signature here to cope with overloading */
	control_flow_graph *graph = function->control_flow_graph;
	unsigned i;
	ESSL_CHECK(fun_name);

	ctx.pool = pool;
	ctx.strbuf = strbuf;
	ctx.desc = desc;
	ctx.cfg = graph;
	ctx.display_extra_info = display_extra_info;
	ESSL_CHECK(_essl_unique_name_init(&ctx.node_names, pool, "node"));
	ESSL_CHECK(_essl_unique_name_init(&ctx.subgraph_names, pool, "cluster"));
	ESSL_CHECK(_essl_ptrdict_init(&ctx.block_to_nodes, pool));
	
	ESSL_CHECK(_essl_ptrset_init(&ctx.visited, pool));
	ESSL_CHECK(_essl_string_buffer_put_formatted(strbuf, "digraph \"%s\" {\ncompound=true;\nsize=\"7.5,10\";\n ratio=fill;\n label=\"%s\";\n", fun_name, fun_name));
	for(i = 0; i < graph->n_blocks; ++i)
	{
		control_dependent_operation *g;
		operation_dependency *d;
		phi_list *lst;
		void *block_node = 0;
		const char *extra_info = "";
		char buf[50];
		b = graph->output_sequence[i];

		ESSL_CHECK(nm = _essl_unique_name_get_or_create(&ctx.subgraph_names, b));


		ESSL_CHECK(_essl_string_buffer_put_formatted(strbuf, "subgraph %s {\n", nm));
		if(display_extra_info)
		{
			if (b->top_depth == 0 || b->bottom_depth == 0) {
				snprintf(buf, 50, "undefined priorities");
			} else {
				node_extra *top_ex, *bottom_ex;
				ESSL_CHECK(top_ex = EXTRA_INFO(b->top_depth));
				ESSL_CHECK(bottom_ex = EXTRA_INFO(b->bottom_depth));
				snprintf(buf, 50, "priorities %d-%d", top_ex->operation_depth, bottom_ex->operation_depth);
			}
			extra_info = buf;
		}

		ESSL_CHECK(_essl_string_buffer_put_formatted(strbuf, 
		                                  "label=<"
		                                  "<table cellpadding=\"0\" border=\"0\" cellspacing=\"0\">"
		                                  "<tr><td align=\"left\">%s%s %d</td></tr>"
		                                  "<tr><td align=\"left\"><font point-size=\"10\" color=\"darkslategray\">postorder %d</font></td></tr>"
		                                  "<tr><td align=\"left\"><font point-size=\"10\" color=\"darkslategray\">cost %g</font></td></tr>"
		                                  "<tr><td align=\"left\"><font point-size=\"10\" color=\"darkslategray\">%s </font></td></tr>"
		                                  "</table> >"
		                                  ";\n", 
												b->output_visit_number == 0 ? "entry " : "", term_kind_text(b), b->output_visit_number, b->postorder_visit_number, b->cost, extra_info));


		ESSL_CHECK(_essl_string_buffer_put_str(strbuf, "style=filled;fillcolor=lightgrey;\n"));
		for(lst = b->phi_nodes; lst != 0; lst = lst->next)
		{
			ESSL_CHECK(emit_node(&ctx, lst->phi_node, phi_style));
		}
		for(g = b->control_dependent_ops; g != 0; g = g->next)
		{
			ESSL_CHECK(emit_node(&ctx, g->op, glob_op_style));
			for(d = g->dependencies; d != 0; d = d->next)
			{
				ESSL_CHECK(nm = _essl_unique_name_get_or_create(&ctx.node_names, d->dependent_on->op));
				ESSL_CHECK(nm2 =_essl_unique_name_get_or_create(&ctx.node_names, g->op));	
				ESSL_CHECK(_essl_string_buffer_put_formatted(strbuf, "%s -> %s [color=orangered];\n", nm, nm2));
			}
		}

		block_node = _essl_mempool_alloc(pool, 1);
		ESSL_CHECK(block_node);
		ESSL_CHECK(nm = _essl_unique_name_get_or_create(&ctx.node_names, block_node));
		ESSL_CHECK(_essl_string_buffer_put_formatted(strbuf, "%s [label=\"\", %s];\n", nm, dummy_style));

		ESSL_CHECK(_essl_ptrdict_insert(&ctx.block_to_nodes, b, block_node));


		ESSL_CHECK(_essl_string_buffer_put_str(strbuf, "}\n"));
		
		
	}
	
	for(i = 0; i < graph->n_blocks; ++i)
	{
		phi_list *lst;
		unsigned j;
		control_dependent_operation *g;
		b = graph->output_sequence[i];
		if(b->source != 0)
		{
			const char *bnode;
			node *m = get_node(b->source);
			node *nn;
			ESSL_CHECK(process_node(&ctx, m));
			ESSL_CHECK(nn = _essl_ptrdict_lookup(&ctx.block_to_nodes, b));
			ESSL_CHECK(bnode = _essl_unique_name_get_or_create(&ctx.node_names, nn));

			ESSL_CHECK(nm = _essl_unique_name_get_or_create(&ctx.node_names, m));
			ESSL_CHECK(nm2 = _essl_unique_name_get_or_create(&ctx.subgraph_names, b));

			ESSL_CHECK(_essl_string_buffer_put_formatted(strbuf, "%s -> %s [lhead=%s,color=blue];\n", nm, bnode, nm2));
		}
		for(g = b->control_dependent_ops; g != 0; g = g->next)
		{
			ESSL_CHECK(process_node(&ctx, get_node(g->op)));
		}

	
		for(lst = b->phi_nodes; lst != 0; lst = lst->next)
		{
			node *m = get_node(lst->phi_node);
			ESSL_CHECK(process_node(&ctx, m));
		}
		
		for(j = 0; j < b->n_successors; ++j)
		{
			basic_block *c = b->successors[j];
			const char *src, *dst;
			node *srcnode, *dstnode;
			ESSL_CHECK(srcnode = _essl_ptrdict_lookup(&ctx.block_to_nodes, b));
			ESSL_CHECK(src = _essl_unique_name_get_or_create(&ctx.node_names, srcnode));
			ESSL_CHECK(dstnode = _essl_ptrdict_lookup(&ctx.block_to_nodes, c));
			ESSL_CHECK(dst = _essl_unique_name_get_or_create(&ctx.node_names, dstnode));
			ESSL_CHECK(nm = _essl_unique_name_get_or_create(&ctx.subgraph_names, b));
			ESSL_CHECK(nm2 = _essl_unique_name_get_or_create(&ctx.subgraph_names, c));
			ESSL_CHECK(_essl_string_buffer_put_formatted(strbuf, "%s -> %s [ltail=%s,lhead=%s,color=red];\n", src, dst, nm, nm2));
			
		}
	}
		
	ESSL_CHECK(_essl_string_buffer_put_str(strbuf, "}\n"));

	return MEM_OK;
}



char *_essl_debug_translation_unit_to_dot_buf(mempool *pool, target_descriptor *desc, translation_unit *tu, int display_extra_info)
{
	char *ret = NULL;
	string_buffer *strbuf = _essl_new_string_buffer(pool);

	if (strbuf != NULL)
	{
		symbol_list *sl;
		for(sl = tu->functions; sl != 0; sl = sl->next)
		{
			symbol *function = sl->sym;
			assert(function->body != 0);
			if (debug_function_to_dot_buf(strbuf, pool, desc, function, display_extra_info) == MEM_OK)
			{
				ret = _essl_string_buffer_to_string(strbuf, pool);
			}
		}

	}

	return ret;
}



#ifdef DEBUGPRINT

memerr _essl_debug_function_to_dot(FILE *out, mempool *pool, target_descriptor *desc, symbol *function, int display_extra_info)
{
	memerr ret = MEM_ERROR;
	string_buffer *strbuf;
	
	/* If no file, no output */
	if (out == NULL)
	{
		return MEM_OK;
	}

	strbuf = _essl_new_string_buffer(pool);

	if (strbuf != NULL)
	{
		ret = debug_function_to_dot_buf(strbuf, pool, desc, function, display_extra_info);
		if (ret == MEM_OK)
		{
			char *result_string = _essl_string_buffer_to_string(strbuf, pool);
			if (result_string != NULL)
			{
				fputs(result_string, out);
			}
		}
	}

	return ret;
}


memerr _essl_debug_translation_unit_to_dot(FILE *out, mempool *pool, target_descriptor *desc, translation_unit *tu, int display_extra_info)
{
	symbol_list *sl;
	
	for(sl = tu->functions; sl != 0; sl = sl->next)
	{
		symbol *function = sl->sym;
		assert(function->body != 0);
		ESSL_CHECK(_essl_debug_function_to_dot(out, pool, desc, function, display_extra_info));
	}

	return MEM_OK;
}

#endif
