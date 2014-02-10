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
#include "shadergen_state.h"
#include "shadergen_mali200/shadergen_mali200.h"

#include "shadergen_mali200/shadergen_mali200_target.h"
#include "shadergen_mali200/shadergen_mali200_driver.h"

#include "common/basic_block.h"
#include "common/symbol_table.h"
#include "common/essl_profiling.h"
#include "backend/extra_info.h"
#include "mali200/mali200_external.h"
#define M200_SCALAR_SIZE SIZE_FP16

typedef enum {
	UNIFORM_CONSTANT_COLOR,
	UNIFORM_CONSTANT_0,
	UNIFORM_CONSTANT_1,
	UNIFORM_CONSTANT_2,
	UNIFORM_CONSTANT_3,
	UNIFORM_CONSTANT_4,
	UNIFORM_CONSTANT_5,
	UNIFORM_CONSTANT_6,
	UNIFORM_CONSTANT_7,
	UNIFORM_SAMPLER_0,
	UNIFORM_SAMPLER_1,
	UNIFORM_SAMPLER_2,
	UNIFORM_SAMPLER_3,
	UNIFORM_SAMPLER_4,
	UNIFORM_SAMPLER_5,
	UNIFORM_SAMPLER_6,
	UNIFORM_SAMPLER_7,
	UNIFORM_SAMPLER_8,
	UNIFORM_SAMPLER_9,
	UNIFORM_SAMPLER_10,
	UNIFORM_SAMPLER_11,
	UNIFORM_SAMPLER_12,
	UNIFORM_SAMPLER_13,
	UNIFORM_SAMPLER_14,
	UNIFORM_SAMPLER_15,
	UNIFORM_CLIP_PLANE_TIE,
	UNIFORM_FOG_COLOR,
	UNIFORM_POINTCOORD_SCALEBIAS,
	UNIFORM_ADD_CONSTANT_TO_RES,
	VARYING_TEXCOORD_0,
	VARYING_TEXCOORD_1,
	VARYING_TEXCOORD_2,
	VARYING_TEXCOORD_3,
	VARYING_TEXCOORD_4,
	VARYING_TEXCOORD_5,
	VARYING_TEXCOORD_6,
	VARYING_TEXCOORD_7,
	VARYING_PRIMARY_COLOR,
	VARYING_SPECULAR_COLOR,
	VARYING_PRIMARY_COLOR_TWOSIDED,
	VARYING_SPECULAR_COLOR_TWOSIDED,
	VARYING_CLIP_DISTANCE,
	VARYING_FOG_DISTANCE,
	EXTERNAL_COEFFICIENTS,
	FF_N_SYMBOLS

} ff_symbol;

typedef struct {
	const fragment_shadergen_state *state;
	mempool *pool;
	translation_unit *tu;
	typestorage_context *ts_ctx;
	control_flow_graph *cfg;
	basic_block *curr_block;
	node *node_cache[ARG_N_STAGES][OPERAND_N_OPERANDS];
	symbol *symbols[FF_N_SYMBOLS];
	ptrset used_symbols;

} codegen_context;


static const struct {
	const char *name;
	type_basic basic_type;
	int vec_size;
	int array_size;
	address_space_kind address_space;
	int address;
	int list_address;
	essl_bool flatshade;
} items[] = 
{
	{"StageSamplerNormal0", TYPE_SAMPLER_2D, 2, 0, ADDRESS_SPACE_UNIFORM,  0, UNIFORM_SAMPLER_0, ESSL_FALSE},
	{"StageSamplerNormal1", TYPE_SAMPLER_2D, 2, 0, ADDRESS_SPACE_UNIFORM,  1, UNIFORM_SAMPLER_1, ESSL_FALSE},
	{"StageSamplerNormal2", TYPE_SAMPLER_2D, 2, 0, ADDRESS_SPACE_UNIFORM,  2, UNIFORM_SAMPLER_2, ESSL_FALSE},
	{"StageSamplerNormal3", TYPE_SAMPLER_2D, 2, 0, ADDRESS_SPACE_UNIFORM,  3, UNIFORM_SAMPLER_3, ESSL_FALSE},
	{"StageSamplerNormal4", TYPE_SAMPLER_2D, 2, 0, ADDRESS_SPACE_UNIFORM,  4, UNIFORM_SAMPLER_4, ESSL_FALSE},
	{"StageSamplerNormal5", TYPE_SAMPLER_2D, 2, 0, ADDRESS_SPACE_UNIFORM,  5, UNIFORM_SAMPLER_5, ESSL_FALSE},
	{"StageSamplerNormal6", TYPE_SAMPLER_2D, 2, 0, ADDRESS_SPACE_UNIFORM,  6, UNIFORM_SAMPLER_6, ESSL_FALSE},
	{"StageSamplerNormal7", TYPE_SAMPLER_2D, 2, 0, ADDRESS_SPACE_UNIFORM,  7, UNIFORM_SAMPLER_7, ESSL_FALSE},

	{"StageSamplerExternal0", TYPE_SAMPLER_EXTERNAL, 3, 0, ADDRESS_SPACE_UNIFORM, 8, UNIFORM_SAMPLER_8, ESSL_FALSE},
	{"StageSamplerExternal1", TYPE_SAMPLER_EXTERNAL, 3, 0, ADDRESS_SPACE_UNIFORM, 9, UNIFORM_SAMPLER_9, ESSL_FALSE},
	{"StageSamplerExternal2", TYPE_SAMPLER_EXTERNAL, 3, 0, ADDRESS_SPACE_UNIFORM, 10, UNIFORM_SAMPLER_10, ESSL_FALSE},
	{"StageSamplerExternal3", TYPE_SAMPLER_EXTERNAL, 3, 0, ADDRESS_SPACE_UNIFORM, 11, UNIFORM_SAMPLER_11, ESSL_FALSE},
	{"StageSamplerExternal4", TYPE_SAMPLER_EXTERNAL, 3, 0, ADDRESS_SPACE_UNIFORM, 12, UNIFORM_SAMPLER_12, ESSL_FALSE},
	{"StageSamplerExternal5", TYPE_SAMPLER_EXTERNAL, 3, 0, ADDRESS_SPACE_UNIFORM, 13, UNIFORM_SAMPLER_13, ESSL_FALSE},
	{"StageSamplerExternal6", TYPE_SAMPLER_EXTERNAL, 3, 0, ADDRESS_SPACE_UNIFORM, 14, UNIFORM_SAMPLER_14, ESSL_FALSE},
	{"StageSamplerExternal7", TYPE_SAMPLER_EXTERNAL, 3, 0, ADDRESS_SPACE_UNIFORM, 15, UNIFORM_SAMPLER_15, ESSL_FALSE},

	{"ConstantColor",  TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 32, UNIFORM_CONSTANT_COLOR, ESSL_FALSE},
	{"FogColor",       TYPE_FLOAT, 3, 0, ADDRESS_SPACE_UNIFORM, 36, UNIFORM_FOG_COLOR,  ESSL_FALSE},
	{"ClipPlaneTie",   TYPE_FLOAT, 1, 0, ADDRESS_SPACE_UNIFORM, 39, UNIFORM_CLIP_PLANE_TIE, ESSL_FALSE},
	{"gl_mali_PointCoordScaleBias",
		TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 40, UNIFORM_POINTCOORD_SCALEBIAS, ESSL_FALSE},
	{"StageConstant0", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 44, UNIFORM_CONSTANT_0, ESSL_FALSE},
	{"StageConstant1", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 48, UNIFORM_CONSTANT_1, ESSL_FALSE},
	{"StageConstant2", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 52, UNIFORM_CONSTANT_2, ESSL_FALSE},
	{"StageConstant3", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 56, UNIFORM_CONSTANT_3, ESSL_FALSE},
	{"StageConstant4", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 60, UNIFORM_CONSTANT_4, ESSL_FALSE},
	{"StageConstant5", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 64, UNIFORM_CONSTANT_5, ESSL_FALSE},
	{"StageConstant6", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 68, UNIFORM_CONSTANT_6, ESSL_FALSE},
	{"StageConstant7", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 72, UNIFORM_CONSTANT_7, ESSL_FALSE},
	{"AddConstToRes",  TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM, 76, UNIFORM_ADD_CONSTANT_TO_RES, ESSL_FALSE},
	{EXTERNAL_COEFFICIENTS_NAME, TYPE_FLOAT, 4, 32, ADDRESS_SPACE_UNIFORM, 80, EXTERNAL_COEFFICIENTS, ESSL_FALSE},

	{"var_TexCoord0", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_TEXCOORD_0, ESSL_FALSE},
	{"var_TexCoord1", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_TEXCOORD_1, ESSL_FALSE},
	{"var_TexCoord2", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_TEXCOORD_2, ESSL_FALSE},
	{"var_TexCoord3", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_TEXCOORD_3, ESSL_FALSE},
	{"var_TexCoord4", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_TEXCOORD_4, ESSL_FALSE},
	{"var_TexCoord5", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_TEXCOORD_5, ESSL_FALSE},
	{"var_TexCoord6", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_TEXCOORD_6, ESSL_FALSE},
	{"var_TexCoord7", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_TEXCOORD_7, ESSL_FALSE},

	{"var_PrimaryColor", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_PRIMARY_COLOR, ESSL_TRUE},
	{"var_PrimaryColorTwosided", TYPE_FLOAT, 4, 2, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_PRIMARY_COLOR_TWOSIDED, ESSL_TRUE},
	{"var_SpecularColor", TYPE_FLOAT, 4, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_SPECULAR_COLOR, ESSL_TRUE},
	{"var_SpecularColorTwosided", TYPE_FLOAT, 4, 2, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_SPECULAR_COLOR_TWOSIDED, ESSL_TRUE},
	{"var_ClipPlaneSignedDist", TYPE_FLOAT, 1, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_CLIP_DISTANCE, ESSL_FALSE},
	{"var_FogDist", TYPE_FLOAT, 1, 0, ADDRESS_SPACE_FRAGMENT_VARYING, -1, VARYING_FOG_DISTANCE, ESSL_FALSE}

};


static node *get_source(codegen_context *ctx, arg_source src, arg_operand op, int stage_index);

static essl_bool operand_is_inverted(arg_operand op)
{
	return op & 1;
}

static essl_bool operand_without_invert(arg_operand op)
{
	return op & ~1;
}

static translation_unit *make_translation_unit(mempool *pool, compiler_options *opts) {
	translation_unit *tu;
	target_descriptor *desc;
	control_flow_graph *cfg;
	symbol *function;
	string function_name = { "__start", 7 };
	type_specifier *ret_type;
	qualifier_set medp_qual;
	_essl_init_qualifier_set(&medp_qual);
	medp_qual.precision = PREC_MEDIUM;

	/* Create target descriptor */
	ESSL_CHECK(desc = _essl_shadergen_mali200_new_target_descriptor(pool, TARGET_FRAGMENT_SHADER, opts));

	/* Create function */
	ESSL_CHECK(cfg = _essl_mempool_alloc(pool, sizeof(control_flow_graph)));
	ESSL_CHECK(ret_type = _essl_new_type(pool));
	ret_type->u.basic.vec_size = 4;
	ret_type->basic_type = TYPE_FLOAT;
	ESSL_CHECK(function = _essl_new_function_symbol(pool, function_name, ret_type, medp_qual, UNKNOWN_SOURCE_OFFSET));
	function->control_flow_graph = cfg;
	ESSL_CHECK(_essl_ptrdict_init(&cfg->control_dependence, pool));
	/* Create translation unit */
	ESSL_CHECK(tu = _essl_mempool_alloc(pool, sizeof(translation_unit)));
	ESSL_CHECK(tu->functions = LIST_NEW(pool, symbol_list));
	tu->functions->sym = function;
	tu->entry_point = function;
	tu->desc = desc;

	return tu;
}
/*@null@*/ static node *create_float_constant(codegen_context *ctx, float value, unsigned vec_size)
{
	node *n;
	unsigned i;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, vec_size));
	n->expr.u.value[0] = ctx->tu->desc->float_to_scalar(value);
	for(i = 1; i < vec_size; ++i)
	{
		n->expr.u.value[i] = n->expr.u.value[0];
	}

	ESSL_CHECK(n->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, vec_size, M200_SCALAR_SIZE));
	return n;
}

static node *generate_load(codegen_context *ctx, ff_symbol src, node *addr_offset)
{
	symbol_list *lst;
	symbol *s;
	node *n;
	node_extra *ne;
	assert(src < FF_N_SYMBOLS);
	ESSL_CHECK(s = ctx->symbols[src]);
	ESSL_CHECK(n = _essl_new_load_expression(ctx->pool, s->address_space, addr_offset));
	n->hdr.type = s->type;
	if(n->hdr.type->basic_type == TYPE_ARRAY_OF)
	{
		n->hdr.type = n->hdr.type->child_type;
	}
	ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, n));
	ESSL_CHECK(lst = _essl_mempool_alloc(ctx->pool, sizeof(*lst)));
	lst->sym = s;
	ne->address_symbols = lst;
	ne->address_multiplier = (int)ctx->tu->desc->get_address_multiplier(ctx->tu->desc, n->hdr.type, n->expr.u.load_store.address_space);
	ne->address_offset = 0;
	return n;
}

/**
 * Taken from mali200/mali200_preschedule.rw
 */
static memerr append_child_to_combiner(mempool *pool, node *cmb, node *in, unsigned start_offset, unsigned len)
{
	unsigned i;
	node *swz = NULL;
	unsigned child_idx = GET_N_CHILDREN(cmb);
	node *assign = NULL;
	if(start_offset == 0 && _essl_get_type_size(cmb->hdr.type) == len)
	{
		/* no swizzle needed */
		assign = in;
	} else {
		ESSL_CHECK(swz = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, in));
		_essl_ensure_compatible_node(swz, cmb);
		assign = swz;
	}
	for(i = start_offset; i < start_offset + len; ++i)
	{
		if(swz != NULL)
		{
			swz->expr.u.swizzle.indices[i] = i - start_offset;
		}
		cmb->expr.u.combiner.mask[i] = child_idx;
	}
	ESSL_CHECK(APPEND_CHILD(cmb, assign, pool));
	return MEM_OK;
}

/**
 * Taken from mali200/mali200_preschedule.rw
 */
static node *create_external_lookup(codegen_context *ctx, expression_operator res_op, node *coord, int stage_index)
{

/*
we'll expand samplerExternal lookups like this:

vec4 textureExternal(vec2 tc)
{
	float external_components[3];
	vec ra_lookup;
	external_components[0] = ra_lookup = texture2D(smp_y, tc).ra;
	external_components[1]             = texture2D(smp_u, tc).g;
	external_components[2]             = texture2D(smp_v, tc).b;
	vec4 ycoef = offset_ycoef;
	vec3 y_scaled = vec3(abs(ycoef.a) * ra_lookup.x, max(ycoef.aa * ra_lookup.xx, 0.0));
	vec3 rgb_unclamped = ycoef.rgb + y_scaled + ucoef * external_components[1] + vcoef * external_components[2];
	vec4 rgba = vec4(clamp(rgb_unclamped, 0.0, 1.0), ra_lookup.y);

	return rgba;
}


 */

	node *external_components[3];
	node *coefficients[3];
	unsigned i;
	expression_operator op = EXPR_OP_UNKNOWN;
	node *ycoef;
	node *ucoef;
	node *vcoef;
	node *rgb_unclamped;
	node *rgba = NULL;
	node *y_scaled;
	node *zero;
	node *one;
	symbol_list *lst;
	symbol *sym;
	control_dependent_operation *tex_control_dep_op;
	const type_specifier *type;
	
	switch (res_op)
	{
	case EXPR_OP_FUN_TEXTUREEXTERNAL:
		op = EXPR_OP_FUN_TEXTURE2D;
		break;
	case EXPR_OP_FUN_TEXTUREEXTERNALPROJ:
		op = EXPR_OP_FUN_TEXTURE2DPROJ;
		break;
	case EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W:
		op = EXPR_OP_FUN_TEXTURE2DPROJ_W;
		break;
	default:
		assert(0 && "Unknown external sampler function");
		break;
	}

	ESSL_CHECK(zero = create_float_constant(ctx, 0.0, 1));
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));

	ESSL_CHECK(type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));
	sym = ctx->symbols[UNIFORM_SAMPLER_8 + stage_index];
	ESSL_CHECK(_essl_ptrset_insert(&ctx->used_symbols, sym));
	for(i = 0; i < 3; ++i)
	{
		node *tex_lookup;
		node_extra *ne;
		ESSL_CHECK(tex_lookup = _essl_new_builtin_function_call_expression(ctx->pool, op, NULL, coord, NULL));
		tex_lookup->hdr.type = type;
		ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, tex_lookup));
		ne->address_multiplier = 1;
		ne->address_offset += stage_index * 2 + i * ne->address_multiplier;
		ESSL_CHECK(lst = _essl_mempool_alloc(ctx->pool, sizeof(*lst)));
		lst->sym = sym;
		ne->address_symbols = lst;

		/* make it control dependent */
		tex_lookup->hdr.is_control_dependent = 1;
		ESSL_CHECK(tex_control_dep_op = LIST_NEW(ctx->pool, control_dependent_operation));
		tex_control_dep_op->op = tex_lookup;
		tex_control_dep_op->block = ctx->curr_block;
		LIST_INSERT_BACK(&ctx->curr_block->control_dependent_ops, tex_control_dep_op);
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->cfg->control_dependence, tex_lookup, tex_control_dep_op));
		
		ESSL_CHECK(external_components[i] = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, tex_lookup));
		_essl_ensure_compatible_node(external_components[i], tex_lookup);
		external_components[i]->expr.u.swizzle.indices[0] = i;
		ESSL_CHECK(external_components[i]->hdr.type = _essl_get_type_with_given_vec_size(ctx->ts_ctx, external_components[i]->hdr.type, 1));
		if(i == 0)
		{
			/* ra_lookup              = texture2D(smp_y, tc).ra; */
			external_components[i]->expr.u.swizzle.indices[1] = 3;
			ESSL_CHECK(external_components[i]->hdr.type = _essl_get_type_with_given_vec_size(ctx->ts_ctx, external_components[i]->hdr.type, 2));
		}
	}

	{
		symbol *coefficients_symbol;
		symbol_list *address_list;
		ESSL_CHECK(coefficients_symbol = ctx->symbols[EXTERNAL_COEFFICIENTS]);
		ESSL_CHECK(_essl_ptrset_insert(&ctx->used_symbols, coefficients_symbol));
		
		ESSL_CHECK(address_list = _essl_mempool_alloc(ctx->pool, sizeof(*address_list)));
		address_list->sym = coefficients_symbol;

		for(i = 0; i < 3; ++i)
		{
			node *load;
			node_extra *ne;
			ESSL_CHECK(load = _essl_new_load_expression(ctx->pool, ADDRESS_SPACE_UNIFORM, NULL));
			load->hdr.type = coefficients_symbol->type->child_type;
			ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, load));
			ne->address_multiplier = 4;
			ne->address_symbols = address_list;
			ne->address_offset += i * ne->address_multiplier;

			/* NO prescheduling, this node is already done */
			
			coefficients[i] = load;
		}
		ycoef = coefficients[0];

		ESSL_CHECK(ucoef = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, coefficients[1]));
		_essl_ensure_compatible_node(ucoef, coefficients[1]);
		ucoef->expr.u.swizzle = _essl_create_identity_swizzle(3);
		

		ESSL_CHECK(vcoef = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, coefficients[2]));
		_essl_ensure_compatible_node(vcoef, coefficients[2]);
		vcoef->expr.u.swizzle = _essl_create_identity_swizzle(3);
		
	}

	{ 
		/* vec3 y_scaled = vec3(abs(ycoef.a) * external_components[0].x, max(ycoef.aa * external_components[0].xx, 0.0)); */
		node *ycoef_a;
		node *ycoef_aa;
		node *abs_ycoef_a;
		node *abs_ycoef_a_times_y;
		node *ycoef_aa_times_y;
		node *positive_ycoef_aa_times_y;
		node *ra_lookup_x;
		node *ra_lookup_xx;

		ESSL_CHECK(ycoef_a = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, ycoef));
		ycoef_a->hdr.type = type;
		ycoef_a->expr.u.swizzle.indices[0] = 3;
		ESSL_CHECK(ycoef_a->hdr.type = _essl_get_type_with_given_vec_size(ctx->ts_ctx, ycoef_a->hdr.type, 1));

		ESSL_CHECK(abs_ycoef_a = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ABS, ycoef_a, NULL, NULL));
		_essl_ensure_compatible_node(abs_ycoef_a, ycoef_a);

		ESSL_CHECK(ra_lookup_x = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, external_components[0]));
		_essl_ensure_compatible_node(ra_lookup_x, external_components[0]);
		ra_lookup_x->expr.u.swizzle.indices[0] = 0;
		ESSL_CHECK(ra_lookup_x->hdr.type = _essl_get_type_with_given_vec_size(ctx->ts_ctx, ra_lookup_x->hdr.type, 1));

		ESSL_CHECK(abs_ycoef_a_times_y = _essl_new_binary_expression(ctx->pool, abs_ycoef_a, EXPR_OP_MUL, ra_lookup_x));
		_essl_ensure_compatible_node(abs_ycoef_a_times_y, abs_ycoef_a);

		ESSL_CHECK(ycoef_aa = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, ycoef));
		ycoef_aa->hdr.type = type;
		ycoef_aa->expr.u.swizzle.indices[0] = 3;
		ycoef_aa->expr.u.swizzle.indices[1] = 3;
		ESSL_CHECK(ycoef_aa->hdr.type = _essl_get_type_with_given_vec_size(ctx->ts_ctx, ycoef_aa->hdr.type, 2));

		ESSL_CHECK(ra_lookup_xx = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, external_components[0]));
		_essl_ensure_compatible_node(ra_lookup_xx, external_components[0]);
		ra_lookup_xx->expr.u.swizzle.indices[0] = 0;
		ra_lookup_xx->expr.u.swizzle.indices[1] = 0;
		ESSL_CHECK(ra_lookup_xx->hdr.type = _essl_get_type_with_given_vec_size(ctx->ts_ctx, ra_lookup_xx->hdr.type, 2));

		ESSL_CHECK(ycoef_aa_times_y = _essl_new_binary_expression(ctx->pool, ycoef_aa, EXPR_OP_MUL, ra_lookup_xx));
		_essl_ensure_compatible_node(ycoef_aa_times_y, ycoef_aa);

		ESSL_CHECK(positive_ycoef_aa_times_y = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, ycoef_aa_times_y, zero, NULL));
		_essl_ensure_compatible_node(positive_ycoef_aa_times_y, ycoef_aa_times_y);

		ESSL_CHECK(y_scaled = _essl_new_vector_combine_expression(ctx->pool, 0));
		y_scaled->hdr.type = type;
		ESSL_CHECK(y_scaled->hdr.type = _essl_get_type_with_given_vec_size(ctx->ts_ctx, y_scaled->hdr.type, 3));
		
		ESSL_CHECK(append_child_to_combiner(ctx->pool, y_scaled, abs_ycoef_a_times_y, 0, 1));
		ESSL_CHECK(append_child_to_combiner(ctx->pool, y_scaled, positive_ycoef_aa_times_y, 1, 2));

	}

	{
		/* vec3 rgb_unclamped = ycoef.rgb + y_scaled + ucoef * external_components[1] + vcoef * external_components[2]; */
		node *ycoef_rgb;
		node *ucoef_u;
		node *vcoef_v;
		node *add1;
		node *add2;
		ESSL_CHECK(ycoef_rgb = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, ycoef));
		_essl_ensure_compatible_node(ycoef_rgb, y_scaled);
		ycoef_rgb->expr.u.swizzle = _essl_create_identity_swizzle(3);

		ESSL_CHECK(ucoef_u = _essl_new_binary_expression(ctx->pool, ucoef, EXPR_OP_MUL, external_components[1]));
		_essl_ensure_compatible_node(ucoef_u, ucoef);

		ESSL_CHECK(vcoef_v = _essl_new_binary_expression(ctx->pool, vcoef, EXPR_OP_MUL, external_components[2]));
		_essl_ensure_compatible_node(vcoef_v, vcoef);


		ESSL_CHECK(add1 = _essl_new_binary_expression(ctx->pool, ycoef_rgb, EXPR_OP_ADD, y_scaled));
		_essl_ensure_compatible_node(add1, y_scaled);

		ESSL_CHECK(add2 = _essl_new_binary_expression(ctx->pool, add1, EXPR_OP_ADD, ucoef_u));
		_essl_ensure_compatible_node(add2, add1);


		ESSL_CHECK(rgb_unclamped = _essl_new_binary_expression(ctx->pool, add2, EXPR_OP_ADD, vcoef_v));
		_essl_ensure_compatible_node(rgb_unclamped, add2);

	}

	{
		/* vec4 rgba = vec4(clamp(rgb_unclamped, 0.0, 1.0), ra_lookup.y); */
		node *rgb;
		node *ra_lookup_yyyy;
		ESSL_CHECK(rgb = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_CLAMP, rgb_unclamped, zero, one));
		_essl_ensure_compatible_node(rgb, rgb_unclamped);

		ESSL_CHECK(ra_lookup_yyyy = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, external_components[0]));
		_essl_ensure_compatible_node(ra_lookup_yyyy, external_components[2]);
		ra_lookup_yyyy->expr.u.swizzle.indices[0] = 1;
		ra_lookup_yyyy->expr.u.swizzle.indices[1] = 1;
		ra_lookup_yyyy->expr.u.swizzle.indices[2] = 1;
		ra_lookup_yyyy->expr.u.swizzle.indices[3] = 1;
		ESSL_CHECK(ra_lookup_yyyy->hdr.type = _essl_get_type_with_given_vec_size(ctx->ts_ctx, external_components[0]->hdr.type, 3));

		
		ESSL_CHECK(rgba = _essl_new_vector_combine_expression(ctx->pool, 0));
		rgba->hdr.type = type;
		
		ESSL_CHECK(append_child_to_combiner(ctx->pool, rgba, rgb, 0, 3));
		ESSL_CHECK(append_child_to_combiner(ctx->pool, rgba, ra_lookup_yyyy, 3, 1));
	}

	return rgba; /* the traversal of control dependent ops will detect that add isn't control dependent and remove res from consideration */


}

static node *generate_texture_lookup(codegen_context *ctx, int stage_index)
{
	node *coordinate;
	node *tex;
	node_extra *te;
	symbol_list *lst;
	texturing_kind tkind;
	control_dependent_operation *tex_control_dep_op;
	expression_operator op;
	assert(stage_index >= 0 && stage_index < MAX_TEXTURE_STAGES);
	if(fragment_shadergen_decode(ctx->state,FRAGMENT_SHADERGEN_STAGE_TEXTURE_POINT_COORDINATE(stage_index)) )
	{
		node *original_pc;

		ESSL_CHECK(original_pc = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_POINT, NULL, NULL, NULL));
		ESSL_CHECK(original_pc->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 2, M200_SCALAR_SIZE));

		if (ctx->tu->desc->options->mali200_pointcoord_scalebias)
		{
			/* Flip point coordinate to compensate for missing flipping support in Mali200 */
			node *pc_scalebias, *pc_scale, *pc_bias;
			node *scaled_pc;

			ESSL_CHECK(pc_scalebias = generate_load(ctx, UNIFORM_POINTCOORD_SCALEBIAS, NULL));

			ESSL_CHECK(pc_scale = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, pc_scalebias));
			pc_scale->expr.u.swizzle.indices[0] = 0;
			pc_scale->expr.u.swizzle.indices[1] = 1;
			pc_scale->expr.u.swizzle.indices[2] = -1;
			pc_scale->expr.u.swizzle.indices[3] = -1;
			_essl_ensure_compatible_node(pc_scale, original_pc);
			ESSL_CHECK(pc_bias = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, pc_scalebias));
			pc_bias->expr.u.swizzle.indices[0] = 2;
			pc_bias->expr.u.swizzle.indices[1] = 3;
			pc_scale->expr.u.swizzle.indices[2] = -1;
			pc_scale->expr.u.swizzle.indices[3] = -1;
			_essl_ensure_compatible_node(pc_bias, original_pc);
			ESSL_CHECK(scaled_pc = _essl_new_binary_expression(ctx->pool, original_pc, EXPR_OP_MUL, pc_scale));
			_essl_ensure_compatible_node(scaled_pc, original_pc);
			ESSL_CHECK(coordinate = _essl_new_binary_expression(ctx->pool, scaled_pc, EXPR_OP_ADD, pc_bias));
			_essl_ensure_compatible_node(coordinate, original_pc);
		}
		else
		{
			coordinate = original_pc;
		}
	} else {
		ESSL_CHECK(coordinate = generate_load(ctx,  (ff_symbol)(VARYING_TEXCOORD_0 + stage_index), NULL));
	}
	tkind = (texturing_kind)fragment_shadergen_decode( ctx->state, FRAGMENT_SHADERGEN_STAGE_TEXTURE_DIMENSIONALITY(stage_index));
	switch(tkind)
	{
	case TEXTURE_2D:
	case TEXTURE_EXTERNAL_NO_TRANSFORM:
		op = EXPR_OP_FUN_TEXTURE2D;
		break;
	case TEXTURE_2D_PROJ_W:
	case TEXTURE_EXTERNAL_NO_TRANSFORM_PROJ_W:
		op = EXPR_OP_FUN_TEXTURE2DPROJ_W;
		assert(!fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_TEXTURE_POINT_COORDINATE(stage_index) ) &&  "cannot combine projective texturing and point coordinates");
		break;
	case TEXTURE_EXTERNAL:
		op = EXPR_OP_FUN_TEXTUREEXTERNAL;
		break;
	case TEXTURE_EXTERNAL_PROJ_W:
		op = EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W;
		assert(!fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_TEXTURE_POINT_COORDINATE(stage_index) ) &&  "cannot combine projective texturing and point coordinates");
		break;
	case TEXTURE_CUBE:
		op = EXPR_OP_FUN_TEXTURECUBE;
		break;
	default:
		assert(0);
		return 0;

	}

	if(op != EXPR_OP_FUN_TEXTUREEXTERNAL && op != EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W)
	{
		ESSL_CHECK(tex = _essl_new_builtin_function_call_expression(ctx->pool, op, NULL, coordinate, NULL));
		ESSL_CHECK(tex->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));
		ESSL_CHECK(te = CREATE_EXTRA_INFO(ctx->pool, tex));
		te->address_offset = 0;
		te->address_multiplier = 1;
		ESSL_CHECK(lst = _essl_mempool_alloc(ctx->pool, sizeof(*lst)));
		if(tkind == TEXTURE_EXTERNAL_NO_TRANSFORM || tkind == TEXTURE_EXTERNAL_NO_TRANSFORM_PROJ_W)
		{
			/* SamplerExternalOES */
			lst->sym = ctx->symbols[UNIFORM_SAMPLER_8 + stage_index];
		}else
		{
			/* Normal samplers */
			lst->sym = ctx->symbols[UNIFORM_SAMPLER_0 + stage_index];
		}
		ESSL_CHECK(_essl_ptrset_insert(&ctx->used_symbols, lst->sym));
		te->address_symbols = lst;


		/* make it control dependent */
		tex->hdr.is_control_dependent = 1;
		ESSL_CHECK(tex_control_dep_op = LIST_NEW(ctx->pool, control_dependent_operation));
		tex_control_dep_op->op = tex;
		tex_control_dep_op->block = ctx->curr_block;
		LIST_INSERT_BACK(&ctx->curr_block->control_dependent_ops, tex_control_dep_op);
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->cfg->control_dependence, tex, tex_control_dep_op));
	}else
	{
		tex = create_external_lookup(ctx, op, coordinate, stage_index);
	}
	
	return tex;
}

static unsigned get_n_sources_needed(texture_combiner combiner)
{
	switch(combiner)
	{
	case COMBINE_REPLACE:
		return 1;
	case COMBINE_MODULATE:
	case COMBINE_ADD:
	case COMBINE_ADD_SIGNED:
	case COMBINE_SUBTRACT:
	case COMBINE_DOT3_RGB:
	case COMBINE_DOT3_RGBA:
		return 2;
	case COMBINE_INTERPOLATE:
		return 3;
	}
	assert(0);
	return 0;
}

static node *ensure_proper_width(codegen_context *ctx, node *n, unsigned wanted_width)
{
	node *swz;
	unsigned curr_width;
	unsigned i;
	if(n == NULL) return NULL;
	curr_width = _essl_get_type_size(n->hdr.type);
	assert(wanted_width >= curr_width);
	if(wanted_width == curr_width) return n;
	assert(curr_width == 1);
	ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, n));
	ESSL_CHECK(swz->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, wanted_width, M200_SCALAR_SIZE));
	for(i = 0; i < wanted_width; ++i)
	{
		swz->expr.u.swizzle.indices[i] = 0;
	}
	for(; i < 4; ++i)
	{
		swz->expr.u.swizzle.indices[i] = -1;
	}
	return swz;
}

static node *place_alpha_in_w(codegen_context *ctx, node *n)
{
	node *swz;
	unsigned i;
	if(n == NULL) return NULL;
	assert(_essl_get_type_size(n->hdr.type) == 1);
	ESSL_CHECK(swz = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, n));
	ESSL_CHECK(swz->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));

	for(i = 0; i < 3; ++i)
	{
		swz->expr.u.swizzle.indices[i] = -1;
	}
	swz->expr.u.swizzle.indices[3] = 0;
	return swz;
}

static node *generate_combiner(codegen_context *ctx, texture_combiner combiner, const arg_source sources[3], const arg_operand operands[3], int stage_index)
{
	node *args[3];
	node *result = NULL;
	node *tmp;
	node *tmp2;
	node *tmp3;
	node *constant;
	unsigned i;
	unsigned n_sources_needed = get_n_sources_needed(combiner);
	unsigned result_width = 1;
	const type_specifier *result_type;

	for(i = 0; i < n_sources_needed; ++i)
	{
		ESSL_CHECK(args[i] = get_source(ctx, sources[i], operands[i], stage_index));
		result_width = ESSL_MAX(result_width, GET_NODE_VEC_SIZE(args[i]));
	}

	ESSL_CHECK(result_type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, result_width, M200_SCALAR_SIZE));
	switch(combiner)
	{
	case COMBINE_REPLACE:
		result = args[0];
		break;

	case COMBINE_MODULATE:
		ESSL_CHECK(result = _essl_new_binary_expression(ctx->pool, args[0], EXPR_OP_MUL, args[1]));
		break;
	case COMBINE_ADD:
		ESSL_CHECK(result = _essl_new_binary_expression(ctx->pool, args[0], EXPR_OP_ADD, args[1]));
		break;
	case COMBINE_ADD_SIGNED:
		ESSL_CHECK(tmp = _essl_new_binary_expression(ctx->pool, args[0], EXPR_OP_ADD, args[1]));
		tmp->hdr.type = result_type;
		ESSL_CHECK(constant = create_float_constant(ctx, -0.5, 1));
		ESSL_CHECK(result = _essl_new_binary_expression(ctx->pool, tmp, EXPR_OP_ADD, constant));
		break;
	case COMBINE_SUBTRACT:
		ESSL_CHECK(tmp = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, args[1]));
		_essl_ensure_compatible_node(tmp, args[1]);

		ESSL_CHECK(result = _essl_new_binary_expression(ctx->pool, args[0], EXPR_OP_ADD, tmp));
		break;
	case COMBINE_DOT3_RGB:
	case COMBINE_DOT3_RGBA:
		/* 4 * dot(arg0 - 0.5, arg1 -  0.5). The 4 is performed afterwards */
		if(result_width == 1)
		{
			result_width = 3;
			ESSL_CHECK(result_type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, result_width, M200_SCALAR_SIZE));
		}
		assert(result_width == 3);
		ESSL_CHECK(constant = create_float_constant(ctx, -0.5, 1));
		ESSL_CHECK(tmp = _essl_new_binary_expression(ctx->pool, args[0], EXPR_OP_ADD, constant));
		_essl_ensure_compatible_node(tmp, args[0]);
		ESSL_CHECK(tmp2 = _essl_new_binary_expression(ctx->pool, args[1], EXPR_OP_ADD, constant));
		_essl_ensure_compatible_node(tmp2, args[1]);

		ESSL_CHECK(tmp3 = _essl_new_binary_expression(ctx->pool, tmp, EXPR_OP_MUL, tmp2));
		tmp3->hdr.type = result_type;

		result_width = 1;
		ESSL_CHECK(result_type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, result_width, M200_SCALAR_SIZE));
		ESSL_CHECK(result = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_HADD, tmp3, NULL, NULL));
		break;

	case COMBINE_INTERPOLATE:
		ESSL_CHECK(tmp = _essl_new_binary_expression(ctx->pool, args[0], EXPR_OP_MUL, args[2]));
		tmp->hdr.type = result_type;
		ESSL_CHECK(tmp3 = get_source(ctx, sources[2], (arg_operand)(operands[2] ^ 1), stage_index)); /* get 1.0 - source 2 */
		ESSL_CHECK(tmp2 = _essl_new_binary_expression(ctx->pool, args[1], EXPR_OP_MUL, tmp3));
		tmp2->hdr.type = result_type;

		ESSL_CHECK(result = _essl_new_binary_expression(ctx->pool, tmp, EXPR_OP_ADD, tmp2));
		
		break;
	default:
		assert(0);
		return 0;
	}
	result->hdr.type = result_type;


	return result;
}

static unsigned extract_scale_factor(scale_kind scale)
{
	switch(scale)
	{
	case SCALE_ONE:
		return 1;
	case SCALE_TWO:
		return 2;
	case SCALE_FOUR:
		return 4;
	}
	assert(0);
	return 0;
}

static node *scale_result(codegen_context *ctx, node *n, unsigned scale)
{
	node *mul, *constant;
	if(n == NULL) return NULL;

	if(scale == 1) return n;
	ESSL_CHECK(constant = create_float_constant(ctx, (float)scale, 1));
	ESSL_CHECK(mul = _essl_new_binary_expression(ctx->pool, n, EXPR_OP_MUL, constant));
	_essl_ensure_compatible_node(mul, n);
	return mul;

}

static node *combine_rgb_alpha(codegen_context *ctx, node *rgb, node *alpha)
{
	node *combiner;
	if(rgb == NULL || alpha == NULL) return NULL;
	ESSL_CHECK(rgb = ensure_proper_width(ctx, rgb, 3));
	ESSL_CHECK(alpha = place_alpha_in_w(ctx, alpha));


	ESSL_CHECK(combiner = _essl_new_vector_combine_expression(ctx->pool, 2));
	SET_CHILD(combiner, 0, rgb);
	SET_CHILD(combiner, 1, alpha);
	ESSL_CHECK(combiner->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));
	combiner->expr.u.combiner.mask[0] = 0;
	combiner->expr.u.combiner.mask[1] = 0;
	combiner->expr.u.combiner.mask[2] = 0;
	combiner->expr.u.combiner.mask[3] = 1;
	return combiner;
}

static node *modify_with_operand(codegen_context *ctx, arg_operand op, node *n)
{
	node *node_with_op = 0, *node_with_invert_op;
	int op_without_invert = operand_without_invert(op);
	essl_bool has_invert = operand_is_inverted(op);
	if(n == NULL) return NULL;
	if(_essl_get_type_size(n->hdr.type) == 1)
	{
		ESSL_CHECK(n = ensure_proper_width(ctx, n, 4));
	}
	assert(_essl_get_type_size(n->hdr.type) == 4);
	switch(op_without_invert)
	{
	case OPERAND_SRC_COLOR:
	case OPERAND_SRC_ALPHA:
		ESSL_CHECK(node_with_op = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, n));
		if(op_without_invert == OPERAND_SRC_COLOR)
		{
			node_with_op->expr.u.swizzle.indices[0] = 0;
			node_with_op->expr.u.swizzle.indices[1] = 1;
			node_with_op->expr.u.swizzle.indices[2] = 2;
			node_with_op->expr.u.swizzle.indices[3] = -1;
			ESSL_CHECK(node_with_op->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 3, M200_SCALAR_SIZE));
		} else {
			node_with_op->expr.u.swizzle.indices[0] = 3;
			node_with_op->expr.u.swizzle.indices[1] = -1;
			node_with_op->expr.u.swizzle.indices[2] = -1;
			node_with_op->expr.u.swizzle.indices[3] = -1;
			ESSL_CHECK(node_with_op->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 1, M200_SCALAR_SIZE));
		}
		break;

	case OPERAND_SRC:
		node_with_op = n;
		break;
	default:
		assert(0);
		return 0;
	}
	
	node_with_invert_op = node_with_op;
	if(has_invert)
	{
		node *one, *negate;
		ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));
		ESSL_CHECK(negate = _essl_new_unary_expression(ctx->pool, EXPR_OP_NEGATE, node_with_op));
		_essl_ensure_compatible_node(negate, node_with_op);
		ESSL_CHECK(node_with_invert_op = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_ADD, negate));
		_essl_ensure_compatible_node(node_with_invert_op, node_with_op);
	}
	return node_with_invert_op;
}

static node *clamp_to_0_1(codegen_context *ctx, node *n)
{
	node *zero;
	node *one;
	node *clamped;
	if(n == NULL) return NULL;
	ESSL_CHECK(zero = create_float_constant(ctx, 0.0, 1));
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));
		
	ESSL_CHECK(clamped = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_CLAMP, n, zero, one));
	_essl_ensure_compatible_node(clamped, n);
	return clamped;
}


/* Check if we need to add a clamp to the tgb result. */
static int need_clamp(node *n)
{
	if (n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
		(n->expr.operation == EXPR_OP_FUN_TEXTURE1D ||
		 n->expr.operation == EXPR_OP_FUN_TEXTURE2D ||
		 n->expr.operation == EXPR_OP_FUN_TEXTURE1DPROJ ||
		 n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJ ||
		 n->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJ_W ||
		 n->expr.operation == EXPR_OP_FUN_TEXTURECUBE))
	{
		/* All supported texture formats have values in the range 0-1. */
		return 0;
	}

	if (n->hdr.kind == EXPR_KIND_VECTOR_COMBINE)
	{
		int i;
		for (i = 0; i < n->hdr.n_children; i++)
		{
			node *t = GET_CHILD(n, 0);
			if (need_clamp(t))
			{
				return 1;
			}
		}
		return 0;
	}

	if (n->hdr.kind == EXPR_KIND_UNARY &&
		n->expr.operation == EXPR_OP_SWIZZLE)
	{
		return need_clamp(GET_CHILD(n, 0));
	}

	if (n->hdr.kind == EXPR_KIND_LOAD)
	{
		/* All supported loads have values in the range 0-1. */
		return 0;
	}

	if (n->hdr.kind == EXPR_KIND_BINARY &&
		n->expr.operation == EXPR_OP_MUL)
	{
		node *left = GET_CHILD(n, 0);
		node *right = GET_CHILD(n, 1);
		return need_clamp(left) || need_clamp(right);
	}

	return 1;
}


static node *generate_stage_result(codegen_context *ctx, int stage_index)
{
	node *rgb_cmb = NULL;
	node *alpha_cmb = NULL;
	unsigned rgb_scale, alpha_scale;
	essl_bool common_combiner = ESSL_FALSE;
	arg_operand common_operands[3];
	essl_bool is_dot3_rgba;
	essl_bool is_dot3;
	node *res = NULL;
	texture_combiner rgb_combiner, alpha_combiner;
	arg_source rgb_sources[3];
	arg_operand rgb_operands[3];
	arg_source alpha_sources[3];
	arg_operand alpha_operands[3];

	essl_bool rgb_enable = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_RGB_ENABLE(stage_index));
	essl_bool alpha_enable = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_ALPHA_ENABLE(stage_index));

	assert(stage_index >= 0 && stage_index < MAX_TEXTURE_STAGES);

	if (rgb_enable)
	{
		rgb_combiner = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_RGB_COMBINER(stage_index));
		rgb_scale = extract_scale_factor(fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_RGB_SCALE(stage_index)));

		rgb_sources[0] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stage_index, 0) );
		rgb_sources[1] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stage_index, 1) );
		rgb_sources[2] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stage_index, 2) );
		rgb_operands[0] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_RGB_OPERAND(stage_index, 0) );
		rgb_operands[1] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_RGB_OPERAND(stage_index, 1) );
		rgb_operands[2] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_RGB_OPERAND(stage_index, 2) );
	} else {
		rgb_combiner = COMBINE_REPLACE;
		rgb_scale = 1;

		rgb_sources[0] = ARG_PREVIOUS_STAGE_RESULT;
		rgb_operands[0] = OPERAND_SRC_COLOR;
	}

	if (alpha_enable)
	{
		alpha_combiner = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_ALPHA_COMBINER(stage_index));
		alpha_scale = extract_scale_factor(fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SCALE(stage_index)));

		alpha_sources[0] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stage_index, 0) );
		alpha_sources[1] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stage_index, 1) );
		alpha_sources[2] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stage_index, 2) );
		alpha_operands[0] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_ALPHA_OPERAND(stage_index, 0) );
		alpha_operands[1] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_ALPHA_OPERAND(stage_index, 1) );
		alpha_operands[2] = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_STAGE_ALPHA_OPERAND(stage_index, 2) );
	} else {
		alpha_combiner = COMBINE_REPLACE;
		alpha_scale = 1;

		alpha_sources[0] = ARG_PREVIOUS_STAGE_RESULT;
		alpha_operands[0] = OPERAND_SRC_ALPHA;
	}
	
	is_dot3_rgba = (rgb_combiner == COMBINE_DOT3_RGBA);
	is_dot3 = (rgb_combiner == COMBINE_DOT3_RGB || rgb_combiner == COMBINE_DOT3_RGBA);
	
	if(rgb_combiner == alpha_combiner)
	{
		unsigned i;
		unsigned n_sources_needed = get_n_sources_needed(rgb_combiner);
		common_combiner = ESSL_TRUE;
		for(i = 0; i < n_sources_needed; ++i)
		{
			arg_source rgb_source = rgb_sources[i];
			arg_operand rgb_operand = rgb_operands[i];
			
			arg_source alpha_source = alpha_sources[i];
			arg_operand alpha_operand = alpha_operands[i];
			
			if(rgb_source != alpha_source)
			{
				common_combiner = ESSL_FALSE;
				break;
			}
			if(operand_is_inverted(rgb_operand) != operand_is_inverted(alpha_operand))
			{
				common_combiner = ESSL_FALSE;
				break;
			}
			if (rgb_enable != alpha_enable)
			{
				common_combiner = ESSL_FALSE;
				break;
			}
			if(operand_without_invert(rgb_operand) == OPERAND_SRC_ALPHA)
			{
				common_operands[i] = OPERAND_SRC_ALPHA;
			} else {
				common_operands[i] = OPERAND_SRC;
			}
			if(operand_is_inverted(rgb_operand))
			{
				common_operands[i] |= 1;
			}
		}

	}

	
	
	if(common_combiner)
	{
		ESSL_CHECK(rgb_cmb = generate_combiner(ctx, rgb_combiner, rgb_sources, common_operands, stage_index));
	} else {
		ESSL_CHECK(rgb_cmb = generate_combiner(ctx, rgb_combiner, rgb_sources, rgb_operands, stage_index));
		if(is_dot3_rgba)
		{
			alpha_cmb = rgb_cmb;
		} else {
			ESSL_CHECK(alpha_cmb = generate_combiner(ctx, alpha_combiner, alpha_sources, alpha_operands, stage_index));
		}
	}

	if(is_dot3)
	{
		rgb_scale *= 4;
		if(is_dot3_rgba)
		{
			alpha_scale *= 4;
		}
	}

	if(rgb_scale == alpha_scale && rgb_enable == alpha_enable)
	{
		node *scale_source;
		if(common_combiner || is_dot3_rgba)
		{
			scale_source = rgb_cmb;
		} else {
			ESSL_CHECK(scale_source = combine_rgb_alpha(ctx, rgb_cmb, alpha_cmb));
		}
		ESSL_CHECK(res = scale_result(ctx, scale_source, rgb_scale));
		if (rgb_enable)
		{
			if (need_clamp(res))
			{
				ESSL_CHECK(res = clamp_to_0_1(ctx, res));
			}
		}
	} else {
		if(common_combiner)
		{
			ESSL_CHECK(alpha_cmb = modify_with_operand(ctx, OPERAND_SRC_ALPHA, rgb_cmb));
			ESSL_CHECK(rgb_cmb = modify_with_operand(ctx, OPERAND_SRC_COLOR, rgb_cmb));
		} else if(is_dot3_rgba)
		{
			alpha_cmb = rgb_cmb;
		}

		ESSL_CHECK(rgb_cmb = scale_result(ctx, rgb_cmb, rgb_scale));
		ESSL_CHECK(alpha_cmb = scale_result(ctx, alpha_cmb, alpha_scale));

		if (rgb_enable == alpha_enable)
		{
			ESSL_CHECK(res = combine_rgb_alpha(ctx, rgb_cmb, alpha_cmb));
			if (rgb_enable)
			{
				ESSL_CHECK(res = clamp_to_0_1(ctx, res));
			}
		} else {
			if (rgb_enable)
			{
				ESSL_CHECK(rgb_cmb = clamp_to_0_1(ctx, rgb_cmb));
			}
			if (alpha_enable)
			{
				ESSL_CHECK(alpha_cmb = clamp_to_0_1(ctx, alpha_cmb));
			}
			ESSL_CHECK(res = combine_rgb_alpha(ctx, rgb_cmb, alpha_cmb));	
		}
	}

	return res;
}

static node dummy_node;

static node *get_source(codegen_context *ctx, arg_source src, arg_operand op, int stage_index)
{
	essl_bool should_cache;
	node *res;

	if (src == ARG_PREVIOUS_STAGE_RESULT)
	{
		return get_source(ctx, ARG_STAGE_RESULT_0+(stage_index-1), op, stage_index);
	}

	res = ctx->node_cache[src][op];
	if(res == &dummy_node)
	{
		assert(0 && "cycle detected in fragment shader expression tree");
		return NULL;
	}
	if(res != NULL)
	{
		return res;
	}

	switch(src)
	{
	case ARG_CONSTANT_0:
	case ARG_CONSTANT_1:
	case ARG_CONSTANT_2:
	case ARG_CONSTANT_3:
	case ARG_CONSTANT_4:
	case ARG_CONSTANT_5:
	case ARG_CONSTANT_6:
	case ARG_CONSTANT_7:
	case ARG_CONSTANT_COLOR:
		should_cache = ESSL_FALSE;
		break;
	default:
		should_cache = ESSL_TRUE;
		break;
	}

	if(should_cache)
	{
		ctx->node_cache[src][op] = &dummy_node;
	}

	if(op != OPERAND_SRC)
	{
		ESSL_CHECK(res = modify_with_operand(ctx, op, get_source(ctx, src, OPERAND_SRC, stage_index)));
	} else {
		node *tmp = 0;
		switch(src)
		{
		case ARG_DISABLE:
		case ARG_N_STAGES:
			assert(0);
			break;
			
		case ARG_CONSTANT_0:
		case ARG_CONSTANT_1:
		case ARG_CONSTANT_2:
		case ARG_CONSTANT_3:
		case ARG_CONSTANT_4:
		case ARG_CONSTANT_5:
		case ARG_CONSTANT_6:
		case ARG_CONSTANT_7:
			ESSL_CHECK(res = generate_load(ctx, (ff_symbol)(src - ARG_CONSTANT_0 + UNIFORM_CONSTANT_0), NULL));
			break;
		case ARG_CONSTANT_COLOR:
			ESSL_CHECK(res = generate_load(ctx, UNIFORM_CONSTANT_COLOR, NULL));
			break;
		case ARG_PRIMARY_COLOR:
		case ARG_SPECULAR_COLOR:
			if(fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_TWOSIDED_LIGHTING ))
			{
				ESSL_CHECK(tmp = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_M200_MISC_VAL, NULL, NULL, NULL));
				ESSL_CHECK(tmp->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 1, M200_SCALAR_SIZE));
				ESSL_CHECK(res = generate_load(ctx, (ff_symbol)(src - ARG_PRIMARY_COLOR + VARYING_PRIMARY_COLOR_TWOSIDED), tmp));
			} else {
				ESSL_CHECK(res = generate_load(ctx, (ff_symbol)(src - ARG_PRIMARY_COLOR + VARYING_PRIMARY_COLOR), tmp));
			}
			
			break;
		case ARG_TEXTURE_0:
		case ARG_TEXTURE_1:
		case ARG_TEXTURE_2:
		case ARG_TEXTURE_3:
		case ARG_TEXTURE_4:
		case ARG_TEXTURE_5:
		case ARG_TEXTURE_6:
		case ARG_TEXTURE_7:
			ESSL_CHECK(res = generate_texture_lookup(ctx, src - ARG_TEXTURE_0));
			break;
			
		case ARG_INPUT_COLOR:
			ESSL_CHECK(res = get_source(ctx, fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE), op, stage_index));
			break;

		case ARG_STAGE_RESULT_0:
		case ARG_STAGE_RESULT_1:
		case ARG_STAGE_RESULT_2:
		case ARG_STAGE_RESULT_3:
		case ARG_STAGE_RESULT_4:
		case ARG_STAGE_RESULT_5:
		case ARG_STAGE_RESULT_6:
		case ARG_STAGE_RESULT_7:
			ESSL_CHECK(res = generate_stage_result(ctx, src - ARG_STAGE_RESULT_0));
			break;

		}
	}
	if(should_cache)
	{
		ctx->node_cache[src][op] = res;
	}
	return res;
}

/* Small simple shaders with fogging may generate more efficient code by
 * reordering some operations, and doing the fog multiplication earlier. */
static node *fog_mult(codegen_context *ctx, node *f, node *rgb)
{
	node *res;

	if (rgb->hdr.kind == EXPR_KIND_UNARY &&
		rgb->expr.operation == EXPR_OP_SWIZZLE)
	{
		node *tmp = GET_CHILD(rgb, 0);
		if (tmp->hdr.kind == EXPR_KIND_BINARY &&
			tmp->expr.operation == EXPR_OP_MUL)
		{
			node *tmp1;
			tmp1 = GET_CHILD(tmp, 0);

			if (tmp1->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
				(tmp1->expr.operation == EXPR_OP_FUN_TEXTURE1D ||
				 tmp1->expr.operation == EXPR_OP_FUN_TEXTURE2D ||
				 tmp1->expr.operation == EXPR_OP_FUN_TEXTURE1DPROJ ||
				 tmp1->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJ ||
				 tmp1->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJ_W ||
				 tmp1->expr.operation == EXPR_OP_FUN_TEXTURECUBE))
			{
				ESSL_CHECK(res = _essl_new_binary_expression(ctx->pool, f, EXPR_OP_MUL, tmp1));
				_essl_ensure_compatible_node(res, tmp1);
				SET_CHILD(tmp, 0, res);

				return rgb;
			}
		}
		else if (tmp->hdr.kind == EXPR_KIND_VECTOR_COMBINE &&
				 tmp->hdr.n_children == 2 &&
				 tmp->expr.u.combiner.mask[0] == 0 &&
				 tmp->expr.u.combiner.mask[1] == 0 &&
				 tmp->expr.u.combiner.mask[2] == 0 &&
				 tmp->expr.u.combiner.mask[3] == 1)
		{
			node *tmp1;
			tmp1 = GET_CHILD(tmp, 0);

			if (tmp1->hdr.kind == EXPR_KIND_BINARY &&
				tmp1->expr.operation == EXPR_OP_MUL)
			{
				node *tmpa;
				tmpa = GET_CHILD(tmp1, 0);

				if (tmpa->hdr.kind == EXPR_KIND_UNARY &&
					tmpa->expr.operation == EXPR_OP_SWIZZLE)
				{
					node *tmpc = GET_CHILD(tmpa, 0);
					if (tmpc->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
						(tmpc->expr.operation == EXPR_OP_FUN_TEXTURE1D ||
						 tmpc->expr.operation == EXPR_OP_FUN_TEXTURE2D ||
						 tmpc->expr.operation == EXPR_OP_FUN_TEXTURE1DPROJ ||
						 tmpc->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJ ||
						 tmpc->expr.operation == EXPR_OP_FUN_TEXTURE2DPROJ_W ||
						 tmpc->expr.operation == EXPR_OP_FUN_TEXTURECUBE))
					{
						ESSL_CHECK(res = _essl_new_binary_expression(ctx->pool, f, EXPR_OP_MUL, tmpc));
						_essl_ensure_compatible_node(res, tmpc);
						SET_CHILD(tmpa, 0, res);

						return rgb;
					}
				}
			}
		}
	}

	ESSL_CHECK(res = _essl_new_binary_expression(ctx->pool, f, EXPR_OP_MUL, rgb));
	_essl_ensure_compatible_node(res, rgb);

	return res;
}

#if defined(USING_MALI400) || defined(USING_MALI450)
static node * generate_logic_ops_rounding_code(codegen_context *ctx, node *input) 
{
	node *res;

	if (fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_LOGIC_OPS_ROUNDING_ENABLE) ) 
	{
		node *add_const;
		ESSL_CHECK(add_const = generate_load(ctx, UNIFORM_ADD_CONSTANT_TO_RES, NULL));

		ESSL_CHECK(res = _essl_new_binary_expression(ctx->pool, input, EXPR_OP_ADD, add_const));
		_essl_ensure_compatible_node(res, add_const);
	}
	else
	{
		res = input;
	}
	return res;
}
#endif

static node *generate_fog_code(codegen_context *ctx)
{
	node *texture_stage_result = 0;
	node *f = NULL;
	node *clamped_f;
	node *fog_distance;
	node *fog_color;
	node *result;
	fog_kind fogmode; 
	ESSL_CHECK(texture_stage_result = get_source(ctx, fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_RESULT_COLOR_SOURCE ), OPERAND_SRC, MAX_TEXTURE_STAGES));
	fogmode = fragment_shadergen_decode( ctx->state, FRAGMENT_SHADERGEN_FOG_MODE);
	if( fogmode == FOG_NONE)
	{
		ESSL_CHECK(texture_stage_result = ensure_proper_width(ctx, texture_stage_result, 4));
		return texture_stage_result;
	}
	ESSL_CHECK(fog_distance = generate_load(ctx, VARYING_FOG_DISTANCE, NULL));
	switch(fogmode)
	{
	case FOG_NONE:
		assert(0);
		return NULL;
	case FOG_LINEAR:
		f = fog_distance;
		break;
	case FOG_EXP2:
 	    {
			node *tmp;
			ESSL_CHECK(tmp = _essl_new_binary_expression(ctx->pool, fog_distance, EXPR_OP_MUL, fog_distance));
			_essl_ensure_compatible_node(tmp, fog_distance);
			fog_distance = tmp;
		}
	/*@fallthrough@*/
	case FOG_EXP:
 	    {
			node *negate;
			ESSL_CHECK(negate = _essl_new_unary_expression(ctx->pool,  EXPR_OP_NEGATE, fog_distance));
			_essl_ensure_compatible_node(negate, fog_distance);

			ESSL_CHECK(f = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_EXP2, negate, NULL, NULL));
			_essl_ensure_compatible_node(f, fog_distance);
			break;
		}
	}
	/* we've got the interpolation factor f, now we need to clamp it to 0..1 */
	ESSL_CHECK(clamped_f = clamp_to_0_1(ctx, f));

	/* all that remains now is to interpolate between the result and the fog color using f */
	ESSL_CHECK(fog_color = generate_load(ctx, UNIFORM_FOG_COLOR, NULL));
	{
		node *texres_rgb;
		node *texres_alpha;
		node *t1;
		node *t2;
		node *fogres_rgb;
		node *one;
		node *negated_clamped_f;
		node *one_minus_clamped_f;

		ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));
		ESSL_CHECK(negated_clamped_f = _essl_new_unary_expression(ctx->pool,  EXPR_OP_NEGATE, clamped_f));
		_essl_ensure_compatible_node(negated_clamped_f, clamped_f);
		ESSL_CHECK(one_minus_clamped_f = _essl_new_binary_expression(ctx->pool, one, EXPR_OP_ADD, negated_clamped_f));
		_essl_ensure_compatible_node(one_minus_clamped_f, negated_clamped_f);
		ESSL_CHECK(t1 = _essl_new_binary_expression(ctx->pool, one_minus_clamped_f, EXPR_OP_MUL, fog_color));
		_essl_ensure_compatible_node(t1, fog_color);

		ESSL_CHECK(texres_rgb = modify_with_operand(ctx, OPERAND_SRC_COLOR, texture_stage_result));
		ESSL_CHECK(t2 = fog_mult(ctx, clamped_f, texres_rgb));

		ESSL_CHECK(fogres_rgb = _essl_new_binary_expression(ctx->pool, t1, EXPR_OP_ADD, t2));
		_essl_ensure_compatible_node(fogres_rgb, fog_color);

		ESSL_CHECK(texres_alpha = modify_with_operand(ctx, OPERAND_SRC_ALPHA, texture_stage_result));

		ESSL_CHECK(result = combine_rgb_alpha(ctx, fogres_rgb, texres_alpha));
	}
	ESSL_CHECK(result = ensure_proper_width(ctx, result, 4));

	return result;
}

static memerr generate_clip_plane_code(codegen_context *ctx) {
	if (fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_CLIP_PLANE_ENABLE(0) ) ) {
		node *clip_plane_varying;

		node *clip_plane_tie;
		node *clip_plane_is_zero;
		node *clip_plane_dist;
		node *clip_plane_multiplied;

		node *zero;
		node *comparison;
		ESSL_CHECK(zero = create_float_constant(ctx, 0.0, 1));
		ESSL_CHECK(clip_plane_varying = generate_load(ctx, VARYING_CLIP_DISTANCE, NULL));

		ESSL_CHECK(clip_plane_tie = generate_load(ctx, UNIFORM_CLIP_PLANE_TIE, NULL));
		ESSL_CHECK(clip_plane_multiplied = _essl_new_binary_expression(ctx->pool, clip_plane_varying, EXPR_OP_MUL, clip_plane_tie));
		_essl_ensure_compatible_node(clip_plane_multiplied, clip_plane_varying);

		ESSL_CHECK(clip_plane_is_zero = _essl_new_binary_expression(ctx->pool, clip_plane_varying, EXPR_OP_EQ, zero));
		ESSL_CHECK(clip_plane_is_zero->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_BOOL, 1, M200_SCALAR_SIZE));
		ESSL_CHECK(clip_plane_dist = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL_SELECT, clip_plane_is_zero, clip_plane_tie, clip_plane_multiplied));
		_essl_ensure_compatible_node(clip_plane_dist, clip_plane_varying);

		ESSL_CHECK(comparison = _essl_new_binary_expression(ctx->pool, clip_plane_dist, EXPR_OP_LT, zero));
		ESSL_CHECK(comparison->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_BOOL, 1, M200_SCALAR_SIZE));
		ctx->cfg->entry_block->termination = TERM_KIND_DISCARD;
		ctx->cfg->entry_block->n_successors = 2;
		ctx->cfg->entry_block->successors[BLOCK_TRUE_TARGET] = ctx->cfg->exit_block;
		ctx->cfg->entry_block->source = comparison;
	}
	return MEM_OK;
}

/*
static node *generate_clip_plane_test_code(codegen_context *ctx) {
	node *clip_plane_varying;
	node *flip_const;
	node *result;
	ESSL_CHECK(clip_plane_varying = generate_load(ctx, VARYING_CLIP_DISTANCE, NULL));
	ESSL_CHECK(flip_const = _essl_new_constant_expression(ctx->pool, 4));
	flip_const->expr.u.value[0] = ctx->tu->desc->float_to_scalar(100.0f);
	flip_const->expr.u.value[1] = ctx->tu->desc->float_to_scalar(-100.0f);
	flip_const->expr.u.value[2] = ctx->tu->desc->float_to_scalar(0.0f);
	flip_const->expr.u.value[3] = ctx->tu->desc->float_to_scalar(0.0f);
	ESSL_CHECK(flip_const->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));
	ESSL_CHECK(result = _essl_new_binary_expression(ctx->pool, clip_plane_varying, EXPR_OP_MUL, flip_const));
	_essl_ensure_compatible_node(result, flip_const);
	return result;
}
*/
static memerr generate_code(codegen_context *ctx) 
{
	phi_source *src;
	phi_list *phi;
	ESSL_CHECK(phi = LIST_NEW(ctx->pool, phi_list));
	ESSL_CHECK(phi->phi_node = _essl_new_phi_expression(ctx->pool, ctx->cfg->postorder_sequence[0]));
	ESSL_CHECK(phi->phi_node->hdr.type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));
	ctx->cfg->postorder_sequence[0]->source = phi->phi_node;
	LIST_INSERT_FRONT(&ctx->cfg->postorder_sequence[0]->phi_nodes, phi);
	ESSL_CHECK(src = LIST_NEW(ctx->pool, phi_source));
	src->join_block = ctx->curr_block = ctx->cfg->output_sequence[1];

	ESSL_CHECK(src->source = generate_fog_code(ctx));
#if defined(USING_MALI400) || defined(USING_MALI450)
	ESSL_CHECK(src->source = generate_logic_ops_rounding_code(ctx, src->source));
#endif

	LIST_INSERT_FRONT(&phi->phi_node->expr.u.phi.sources, src);
	ESSL_CHECK(generate_clip_plane_code(ctx));
/*
	ESSL_CHECK(src->source = generate_clip_plane_test_code(ctx));
*/
	return MEM_OK;
}

static memerr init_blocks(codegen_context *ctx)
{
	unsigned i;
	basic_block *entry_block, *body_block, *exit_block;
	control_flow_graph *cfg = ctx->cfg;
	ESSL_CHECK(entry_block = _essl_new_basic_block(ctx->pool));
	ESSL_CHECK(body_block = _essl_new_basic_block(ctx->pool));
	ESSL_CHECK(exit_block = _essl_new_basic_block(ctx->pool));
	entry_block->termination = TERM_KIND_JUMP;
	entry_block->source = 0;
	entry_block->cost = 1.0;
	body_block->termination = TERM_KIND_JUMP;
	body_block->source = 0;
	body_block->cost = 0.5;
	exit_block->termination = TERM_KIND_EXIT;
	exit_block->source = 0;
	exit_block->cost = 0.5;
	entry_block->next = body_block;
	entry_block->n_successors = 1;
	entry_block->successors[BLOCK_DEFAULT_TARGET] = body_block;
	body_block->next = exit_block;
	body_block->n_successors = 1;
	body_block->successors[BLOCK_DEFAULT_TARGET] = exit_block;

	cfg->entry_block = entry_block;
	cfg->exit_block = exit_block;
	cfg->n_blocks = 3;
	ESSL_CHECK(cfg->output_sequence = _essl_mempool_alloc(ctx->pool, cfg->n_blocks*sizeof(basic_block *)));
	cfg->output_sequence[0] = entry_block;
	cfg->output_sequence[1] = body_block;
	cfg->output_sequence[2] = exit_block;
	ESSL_CHECK(cfg->postorder_sequence = _essl_mempool_alloc(ctx->pool, cfg->n_blocks*sizeof(basic_block *)));
	cfg->postorder_sequence[0] = exit_block;
	cfg->postorder_sequence[1] = body_block;
	cfg->postorder_sequence[2] = entry_block;
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		cfg->postorder_sequence[i]->postorder_visit_number = i;
		cfg->output_sequence[i]->output_visit_number = i;
		if(i > 0)
		{
			predecessor_list *pred;
			cfg->output_sequence[i]->immediate_dominator = cfg->output_sequence[i-1];
			ESSL_CHECK(pred = LIST_NEW(ctx->pool, predecessor_list));
			cfg->output_sequence[i]->predecessor_count = 1;
			pred->block = cfg->output_sequence[i-1];
			cfg->output_sequence[i]->predecessors = pred;
		}
	}

	return MEM_OK;
}

static memerr init_symbols(codegen_context *ctx)
{
	size_t i;
	
	for(i = 0; i < sizeof(items)/sizeof(items[0]); ++i)
	{
		string str;
		symbol *sym;
		symbol_list *sl;
		const type_specifier *t;
		symbol_list **lst = 0;
		qualifier_set qual;
		unsigned vs;
		type_basic bt;

		_essl_init_qualifier_set(&qual);
		qual.variable = VAR_QUAL_VARYING;
		qual.precision = PREC_MEDIUM;
		
		str = _essl_cstring_to_string(ctx->pool, items[i].name);
		ESSL_CHECK(str.ptr);
		bt = items[i].basic_type;
		vs = items[i].vec_size;
		if(i < 8)
		{
			if(fragment_shadergen_decode( ctx->state, FRAGMENT_SHADERGEN_STAGE_TEXTURE_DIMENSIONALITY(i)) == TEXTURE_CUBE)
			{
				bt = TYPE_SAMPLER_CUBE;
				vs = 3;
			}
		}
		ESSL_CHECK(t = _essl_get_type_with_size(ctx->ts_ctx, bt, vs, M200_SCALAR_SIZE));

		if(items[i].array_size > 0)
		{
			ESSL_CHECK(t = _essl_new_array_of_type(ctx->pool, t, items[i].array_size));
		}

		qual.varying = fragment_shadergen_decode(ctx->state, FRAGMENT_SHADERGEN_FLATSHADED_COLORS) && items[i].flatshade ? VARYING_QUAL_FLAT : VARYING_QUAL_NONE;

		ESSL_CHECK(sym = _essl_new_variable_symbol(ctx->pool, str, t, qual, SCOPE_GLOBAL, items[i].address_space, UNKNOWN_SOURCE_OFFSET));
		sym->opt.is_indexed_statically = ESSL_FALSE;
		sym->address = items[i].address;
		ctx->symbols[items[i].list_address] = sym;
		ESSL_CHECK(sl = LIST_NEW(ctx->pool, symbol_list));
		sl->sym = sym;
		switch(items[i].address_space)
		{
		case ADDRESS_SPACE_UNIFORM:
			lst = &ctx->tu->uniforms;
			break;
		case ADDRESS_SPACE_FRAGMENT_VARYING:
			lst = &ctx->tu->fragment_varyings;
			break;
		case ADDRESS_SPACE_FRAGMENT_SPECIAL:
			lst = &ctx->tu->fragment_specials;
			break;
		default:
			assert(0);
			break;

		}
		LIST_INSERT_BACK(lst, sl);
	}

#define SET_SYMBOL_USED(sym_list_address) {symbol *sym; sym = ctx->symbols[sym_list_address]; ESSL_CHECK(_essl_ptrset_insert(&ctx->used_symbols, sym));}
	SET_SYMBOL_USED(UNIFORM_CONSTANT_COLOR);
	SET_SYMBOL_USED(UNIFORM_FOG_COLOR);
	SET_SYMBOL_USED(UNIFORM_CLIP_PLANE_TIE);
	SET_SYMBOL_USED(UNIFORM_POINTCOORD_SCALEBIAS);
	SET_SYMBOL_USED(UNIFORM_CONSTANT_0);
	SET_SYMBOL_USED(UNIFORM_CONSTANT_1);
	SET_SYMBOL_USED(UNIFORM_CONSTANT_2);
	SET_SYMBOL_USED(UNIFORM_CONSTANT_3);
	SET_SYMBOL_USED(UNIFORM_CONSTANT_4);
	SET_SYMBOL_USED(UNIFORM_CONSTANT_5);
	SET_SYMBOL_USED(UNIFORM_CONSTANT_6);
	SET_SYMBOL_USED(UNIFORM_CONSTANT_7);
	SET_SYMBOL_USED(UNIFORM_ADD_CONSTANT_TO_RES);

	return MEM_OK;
}

static void clean_unused_symbols(codegen_context *ctx)
{
	symbol_list *lst;

	for(lst = ctx->tu->uniforms; lst != NULL; lst = lst->next)
	{
		if(!_essl_ptrset_has(&ctx->used_symbols, lst->sym))
		{
			lst->sym->address = -1;
		}
	}
	return;
}


static unsigned *generate_shader(mempool *pool, const fragment_shadergen_state *state, unsigned *size_out, translation_unit **res_tu, unsigned int hw_rev) 
{
	error_context *err_ctx = 0; /* Not used by the Mali200 backend anyway */
	typestorage_context ts_context, *ts_ctx = &ts_context;
	translation_unit *tu;
	output_buffer out_buf;
	codegen_context *ctx;
	compiler_options *opts;
	unsigned *result;

	TIME_PROFILE_START("_total");

	TIME_PROFILE_START("shadergen_init");
	ESSL_CHECK(ctx = _essl_mempool_alloc(pool, sizeof(*ctx)));

	ESSL_CHECK(opts = _essl_new_compiler_options(pool));

	_essl_set_compiler_options_for_hw_rev(opts, hw_rev);

	opts->mali200_add_with_scale_overflow_workaround = ESSL_FALSE; /* Fixed-function has a lot of use for this functionality and can't hit the problem, so we turn off the workaround */

	ESSL_CHECK(_essl_typestorage_init(ts_ctx, pool));

	ESSL_CHECK(_essl_output_buffer_init(&out_buf, pool));

	ESSL_CHECK(tu = make_translation_unit(pool, opts));
	ctx->state = state;
	ctx->pool = pool;
	ctx->tu = tu;
	ctx->ts_ctx = ts_ctx;
	ctx->cfg = tu->entry_point->control_flow_graph;
	memset(ctx->node_cache, 0, sizeof(ctx->node_cache));
	memset(ctx->symbols, 0, sizeof(ctx->symbols));
	ESSL_CHECK(_essl_ptrset_init(&ctx->used_symbols, ctx->pool));
	TIME_PROFILE_STOP("shadergen_init");

	TIME_PROFILE_START("shadergen_codegen");
	ESSL_CHECK(init_blocks(ctx));
	ESSL_CHECK(init_symbols(ctx));
	ESSL_CHECK(generate_code(ctx));
	clean_unused_symbols(ctx);
	TIME_PROFILE_STOP("shadergen_codegen");

	ESSL_CHECK(tu->desc->driver(pool, err_ctx, ts_ctx, tu->desc, tu, &out_buf, opts));

	if(res_tu != NULL)
	{
		*res_tu = tu;
	}

	TIME_PROFILE_START("shadergen_copy_result");
	*size_out = _essl_output_buffer_get_size(&out_buf)*4;
	result = pool->tracker->alloc(*size_out);
	if (result) {
		memcpy(result, _essl_output_buffer_get_raw_pointer(&out_buf), *size_out);
	}
	TIME_PROFILE_STOP("shadergen_copy_result");

	TIME_PROFILE_STOP("_total");

	return result;
}


/* this is used by shader generator testing */
translation_unit *_fragment_shadergen_internal_generate_shader(mempool *pool, const fragment_shadergen_state *state, unsigned **data_out, unsigned *size_out, unsigned int hw_rev)
{
	unsigned *ptr;
	translation_unit *tu = 0;
	ESSL_CHECK(ptr = generate_shader(pool, state, size_out, &tu, hw_rev));
	*data_out = ptr;
	return tu;
}


unsigned *_fragment_shadergen_generate_shader(const fragment_shadergen_state *state, unsigned *size_out, unsigned int hw_rev,
											  alloc_func alloc, free_func free)
{
	mempool_tracker tracker;
	mempool pool;
	unsigned *ptr;
	_essl_mempool_tracker_init(&tracker, alloc, free);
	ESSL_CHECK(_essl_mempool_init(&pool, 0, &tracker));
	ptr = generate_shader(&pool, state, size_out, NULL, hw_rev);
	_essl_mempool_destroy(&pool);
	return ptr;
}

