/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali200/mali200_external.h"
#include "mali200/mali200_external_implementation.h"
#include "common/basic_block.h"
#include "common/essl_node.h"
#include "common/symbol_table.h"
#include "common/ptrdict.h"
#include "middle/dominator.h"
#include "backend/extra_info.h"


#define M200_SCALAR_SIZE SIZE_FP16

typedef struct
{
	mempool *pool;						/*!< Pointer to the memory pool to use during the process */
	mempool temp_pool;
	translation_unit *tu;
	typestorage_context *typestor_ctx;
	control_flow_graph *cfg;
	target_descriptor *desc;
	ptrdict sampler_subst_dict;
	ptrdict bb_subst_dict;
	ptrdict visited;
	ptrdict* fixup_symbols;
} rewrite_sampler_external_accesses_context;

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

static node *create_float_constant(rewrite_sampler_external_accesses_context *ctx, float value, unsigned vec_size)
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

static node *make_color_space_conversion(rewrite_sampler_external_accesses_context *ctx,
										node *external_components[3],
										node *coeff[3])
{
	node *rgb_unclamped;
	node *rgba = NULL;
	node *ycoef;
	node *ucoef;
	node *vcoef;
	node *zero;
	node *one;
	node *y_scaled;

	/**
	 * we'll transform samplerExternal lookups like this:
	 * vec4 textureExternal(vec2 tc)
	 * {
	 *    	float external_components[3];
	 *    	external_components[0]  = texture2D(smp_y, tc).r;
	 *    	external_components[1]  = texture2D(smp_u, tc).g;
	 *    	external_components[2]  = texture2D(smp_v, tc).b;
	 *    	vec4 ycoef = offset_ycoef;
	 *    	vec3 y_scaled = vec3(abs(ycoef.a) * external_components[0].x, max(ycoef.aa * external_components[0].xx, 0.0));
	 *    	vec3 rgb_unclamped = ycoef.rgb + y_scaled + ucoef * external_components[1] + vcoef * external_components[2];
	 *    	vec4 rgba = vec4(clamp(rgb_unclamped, 0.0, 1.0), 1.0);
	 *    	return rgba;
	 * }
 	*/

	ESSL_CHECK(zero = create_float_constant(ctx, 0.0, 1));
	ESSL_CHECK(one = create_float_constant(ctx, 1.0, 1));

	ycoef = coeff[0];

	ESSL_CHECK(ucoef = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, coeff[1]));
	_essl_ensure_compatible_node(ucoef, coeff[1]);
	ucoef->expr.u.swizzle = _essl_create_identity_swizzle(3);
		
	ESSL_CHECK(vcoef = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, coeff[2]));
	_essl_ensure_compatible_node(vcoef, coeff[2]);
	vcoef->expr.u.swizzle = _essl_create_identity_swizzle(3);

	{ 
		/* vec3 y_scaled = vec3(abs(ycoef.a) * external_components[0].x, max(ycoef.aa * external_components[0].xx, 0.0)); */
		node *ycoef_a;
		node *ycoef_aa;
		node *abs_ycoef_a;
		node *abs_ycoef_a_times_y;
		node *ycoef_aa_times_y;
		node *positive_ycoef_aa_times_y;
		node *r_lookup_x;
		node *r_lookup_xx;

		ESSL_CHECK(ycoef_a = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, ycoef));
		_essl_ensure_compatible_node(ycoef_a, external_components[0]);
		ycoef_a->expr.u.swizzle.indices[0] = 3;
		ESSL_CHECK(ycoef_a->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, ycoef_a->hdr.type, 1));

		ESSL_CHECK(abs_ycoef_a = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_ABS, ycoef_a, NULL, NULL));
		_essl_ensure_compatible_node(abs_ycoef_a, ycoef_a);

		ESSL_CHECK(r_lookup_x = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, external_components[0]));
		_essl_ensure_compatible_node(r_lookup_x, external_components[0]);
		r_lookup_x->expr.u.swizzle.indices[0] = 0;
		ESSL_CHECK(r_lookup_x->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, r_lookup_x->hdr.type, 1));

		ESSL_CHECK(abs_ycoef_a_times_y = _essl_new_binary_expression(ctx->pool, abs_ycoef_a, EXPR_OP_MUL, r_lookup_x));
		_essl_ensure_compatible_node(abs_ycoef_a_times_y, abs_ycoef_a);

		ESSL_CHECK(ycoef_aa = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, ycoef));
		_essl_ensure_compatible_node(ycoef_aa, external_components[0]);
		ycoef_aa->expr.u.swizzle.indices[0] = 3;
		ycoef_aa->expr.u.swizzle.indices[1] = 3;
		ESSL_CHECK(ycoef_aa->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, ycoef_aa->hdr.type, 2));

		ESSL_CHECK(r_lookup_xx = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, external_components[0]));
		_essl_ensure_compatible_node(r_lookup_xx, external_components[0]);
		r_lookup_xx->expr.u.swizzle.indices[0] = 0;
		r_lookup_xx->expr.u.swizzle.indices[1] = 0;
		ESSL_CHECK(r_lookup_xx->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, r_lookup_xx->hdr.type, 2));

		ESSL_CHECK(ycoef_aa_times_y = _essl_new_binary_expression(ctx->pool, ycoef_aa, EXPR_OP_MUL, r_lookup_xx));
		_essl_ensure_compatible_node(ycoef_aa_times_y, ycoef_aa);

		ESSL_CHECK(positive_ycoef_aa_times_y = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_MAX, ycoef_aa_times_y, zero, NULL));
		_essl_ensure_compatible_node(positive_ycoef_aa_times_y, ycoef_aa_times_y);

		ESSL_CHECK(y_scaled = _essl_new_vector_combine_expression(ctx->pool, 0));
		_essl_ensure_compatible_node(y_scaled, external_components[0]);
		ESSL_CHECK(y_scaled->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, y_scaled->hdr.type, 3));

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
		/* vec4 rgba = vec4(clamp(rgb_unclamped, 0.0, 1.0), 1.0); */
		node *rgb;
		ESSL_CHECK(rgb = _essl_new_builtin_function_call_expression(ctx->pool, EXPR_OP_FUN_CLAMP, rgb_unclamped, zero, one));
		_essl_ensure_compatible_node(rgb, rgb_unclamped);

		ESSL_CHECK(rgba = _essl_new_vector_combine_expression(ctx->pool, 0));
		_essl_ensure_compatible_node(rgba, rgb_unclamped);
		ESSL_CHECK(rgba->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, rgba->hdr.type, 4));
				
		ESSL_CHECK(append_child_to_combiner(ctx->pool, rgba, rgb, 0, 3));
		ESSL_CHECK(append_child_to_combiner(ctx->pool, rgba, one, 3, 1));
	}

	return rgba;
}


/** Remove the control dependent operation that references the specified node from the list.
 */
static void remove_control_dependent_op_node(control_dependent_operation **list, node *n)
{
	/* Find the CDO on the graph */
	if((*list) != 0 && (*list)->op == n)
		(*list) = (*list)->next;
	else
	{
		while((*list) != 0 && (*list)->next != 0 && (*list)->next->op != n)	list = &((*list)->next);
		assert((*list) != 0 && (*list)->next->op == n);
		LIST_REMOVE(&((*list)->next));
	}

	return;
}


static memerr add_color_space_conversion_code(rewrite_sampler_external_accesses_context *ctx, control_dependent_operation *tex_node)
{
	control_flow_graph *cfg;
	basic_block *true_bb;
	basic_block *false_bb;
	scope *global_scope;
	node *ld_op;
	node *need_transform;
	node *need_transform_cmp;
	node *yuv_res;
	node *phi;
	node *sampler;
	node *sampler_pred;
	symbol *sampler_sym;
	node *tex_lookup[3];
	node *external_components[3];
	node *n;
	basic_block *bb;
	node *coord;
	expression_operator op;
	control_dependent_operation *cd_op[3];
	unsigned j;
	node *zero;
	node_extra *sampler_extra = NULL;
	node *coeff[3];
	symbol *coeff_sym;
	symbol *negativ_external_sampler_start_symbol;
	symbol *external_last_row_symbol = NULL;
	symbol_list* tmp0;
	symbol_list* tmp1;
	symbol_list * address_list;

	n = tex_node->op;
	bb = tex_node->block;
	cfg = ctx->cfg;
	ESSL_CHECK(false_bb = _essl_split_basic_block(ctx->pool, bb, tex_node));
	if(cfg->exit_block == bb)
	{
		cfg->exit_block = false_bb;
	}

	ESSL_CHECK(true_bb = _essl_split_basic_block(ctx->pool, bb, tex_node));
	true_bb->source = NULL;
	bb->successors[BLOCK_DEFAULT_TARGET] = false_bb;
	bb->successors[BLOCK_TRUE_TARGET] = true_bb;
	bb->n_successors = 2;

	if(cfg->exit_block == bb)
	{
		cfg->exit_block = false_bb;
		false_bb->termination = TERM_KIND_EXIT;
	}
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->bb_subst_dict, bb, false_bb));

	/* make source for branching */
	global_scope = ctx->tu->root->stmt.child_scope;

	sampler_sym = NULL;
	sampler = GET_CHILD(n, 0);
	assert(sampler->hdr.kind == EXPR_KIND_UNARY && sampler->expr.operation == EXPR_OP_SWIZZLE);


	sampler_pred = GET_CHILD(sampler, 0);

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
			sampler_extra = load_extra;
			sampler_sym = load_extra->address_symbols->sym;
			sampler = GET_CHILD(sampler_pred, 0);
		}
	}

	coord = GET_CHILD(n, 1);

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

	/* reading  EXTERNAL_COEFFICIENTS[i*4]*/
	/* addressing in EXTERNAL_COEFFICIENTS is tricky.
	 * This symbol is a 4 vec4 for each sampler_external. 
	 * Address for a ld_uniform is calculated during resolve_relocations and is actually a sum of
	 * 'address_offset'  set within this function (several lines down where this load is created)
	 * and (sym->address * sym->address_multiplier) for each symbol listed in extra_info->address_symbols for the load.
	 *
	 * 'address_offset' set here is a relative offset within a cell in EXTERNAL_COEFFICIENTS for this sampler. (so it can 0,4,8,12)
	 * 
	 * coeff_sym->address will contain the beginning of EXTERNAL_COEFFICIENTS and the multiplier will be 1. (see 'handle_output' for mali200)
	 * 
	 * we also add 'sampler_extra' symbol which actually is a sampler symbol and is responsible for the absolute offset in EXTERNAL_COEFFICIENTS
	 * NEGATIVE_EXTERNAL_SAMPLER_START needed to adjust the address to the beginning of EXTERNAL_COEFFICIENTS because samplers addresses can begin not from 0.
	 *
	 * The tricky thing is that sampler_extra has type of TYPE_SAMPLER_EXTERNAL which contains 3 elements
	 * while we assume that there are 4 elements for each sampler in EXTERNAL_COEFFICIENTS,
	 * so we need to add 1 more fixup symbol for each sampler. It will be a vec4 (address multiplier will be 4)
	 * The address of this fixup symbol will depend on the address of the correspondent sampler and will be calculated during address allocation
	 *
	 */
	ESSL_CHECK(coeff_sym = _essl_symbol_table_lookup(global_scope, _essl_cstring_to_string_nocopy(EXTERNAL_COEFFICIENTS_NAME)));

	ESSL_CHECK(negativ_external_sampler_start_symbol = _essl_symbol_table_lookup(global_scope, _essl_cstring_to_string_nocopy(NEGATIVE_EXTERNAL_SAMPLER_START_NAME)));
	if(sampler_sym != NULL)
	{
		if(sampler_sym->type->basic_type == TYPE_SAMPLER_EXTERNAL)
		{
			external_last_row_symbol = _essl_ptrdict_lookup(ctx->fixup_symbols, sampler_sym);
			if(external_last_row_symbol == NULL)
			{
				char buf[100] = {0};
				const type_specifier *vec4;
				string external_last_row;
				qualifier_set medp_qual;
				const char *sampler_name;
				ESSL_CHECK(vec4 = _essl_get_type_with_size(ctx->typestor_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));

				_essl_init_qualifier_set(&medp_qual);
				medp_qual.precision = PREC_MEDIUM;
				ESSL_CHECK(sampler_name = _essl_string_to_cstring(ctx->pool, sampler_sym->name));
				snprintf(buf, 100, "?external_last_row_fixup_%s", sampler_name);
				buf[100-1] = '\0';
				external_last_row = _essl_cstring_to_string(ctx->pool, buf);
				ESSL_CHECK(external_last_row.ptr);
				ESSL_CHECK(external_last_row_symbol = _essl_new_variable_symbol(ctx->pool, external_last_row, vec4, medp_qual, SCOPE_GLOBAL, ADDRESS_SPACE_FRAGMENT_SPECIAL, UNKNOWN_SOURCE_OFFSET));
				ESSL_CHECK(_essl_ptrdict_insert(ctx->fixup_symbols, sampler_sym, external_last_row_symbol));
			}
		}
	}
	ESSL_CHECK(tmp0 = _essl_mempool_alloc(ctx->pool, sizeof(*tmp0)));
	tmp0->sym = negativ_external_sampler_start_symbol;
	ESSL_CHECK(tmp1 = _essl_mempool_alloc(ctx->pool, sizeof(*tmp1)));
	if(external_last_row_symbol != NULL)
	{
		tmp1->sym = external_last_row_symbol;
		LIST_INSERT_BACK(&tmp0, tmp1);
		if(sampler_extra != NULL)
		{
			tmp1->next = (symbol_list*)sampler_extra->address_symbols;
		}
	}else
	{
		if(sampler_extra != NULL)
		{
			tmp0->next = (symbol_list*)sampler_extra->address_symbols;
		}

	}
	ESSL_CHECK(address_list = _essl_mempool_alloc(ctx->pool, sizeof(*address_list)));
	address_list->sym = coeff_sym;
	address_list->next = tmp0;

	for(j = 0; j < 3; ++j)
	{
		node *load;
		node_extra *ne;
		ESSL_CHECK(load = _essl_new_load_expression(ctx->pool, ADDRESS_SPACE_UNIFORM, sampler));
		_essl_ensure_compatible_node(load, n);
		load->hdr.type = coeff_sym->type->child_type;
		ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, load));
		ne->address_multiplier = 4;
		ne->address_symbols = address_list;
		if(sampler_extra != NULL)
		{
			ne->address_offset = sampler_extra->address_offset;
		}
		ne->address_offset += j * ne->address_multiplier;

		/* NO prescheduling, this node is already done */

		coeff[j] = load;
	}

	ESSL_CHECK(ld_op = _essl_new_load_expression(ctx->pool, ADDRESS_SPACE_UNIFORM, sampler));
	_essl_ensure_compatible_node(ld_op, n);
	ld_op->hdr.type = coeff_sym->type->child_type;
	{
		node_extra *ne = CREATE_EXTRA_INFO(ctx->pool, ld_op);
		ne->address_multiplier = 4;
		ne->address_symbols = address_list;
		if(sampler_extra != NULL)
		{
			ne->address_offset = sampler_extra->address_offset;
		}
		ne->address_offset += 3 * ne->address_multiplier;
	}

	ESSL_CHECK(need_transform = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, ld_op));
	_essl_ensure_compatible_node(need_transform, ld_op);
	ESSL_CHECK(need_transform->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, ld_op->hdr.type, 1));
	need_transform->expr.u.swizzle = _essl_create_scalar_swizzle(0);

	ESSL_CHECK(zero = create_float_constant(ctx, 0, 1));

	ESSL_CHECK(need_transform_cmp = _essl_new_binary_expression(ctx->pool, need_transform, EXPR_OP_NE, zero));
	_essl_ensure_compatible_node(need_transform_cmp, need_transform);

	bb->termination = TERM_KIND_JUMP;
	bb->source = need_transform_cmp;
	true_bb->cost = 0.4 * bb->cost;

	op = n->expr.operation;
	switch (op)
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


	for(j = 0; j < 3; ++j)
	{
		node_extra *ne;
		ESSL_CHECK(tex_lookup[j] = _essl_new_builtin_function_call_expression(ctx->pool, op, sampler, coord, NULL));
		_essl_ensure_compatible_node(tex_lookup[j], n);
		ESSL_CHECK(ne = CREATE_EXTRA_INFO(ctx->pool, tex_lookup[j]));
		ne->address_multiplier = 1;
		if(sampler_extra != NULL)
		{
			ne->address_symbols = sampler_extra->address_symbols;
			ne->address_offset = sampler_extra->address_offset;
		}
		ne->address_offset += j * ne->address_multiplier;
		tex_lookup[j]->hdr.is_control_dependent = 1;
		ESSL_CHECK(cd_op[j] = _essl_clone_control_dependent_op(n, tex_lookup[j], ctx->cfg, ctx->pool));
	}

	remove_control_dependent_op_node(&bb->control_dependent_ops, cd_op[1]->op);
	remove_control_dependent_op_node(&bb->control_dependent_ops, cd_op[2]->op);

	cd_op[1]->block = true_bb;
	cd_op[2]->block = true_bb;

	LIST_INSERT_FRONT(&true_bb->control_dependent_ops, cd_op[1]);
	LIST_INSERT_FRONT(&true_bb->control_dependent_ops, cd_op[2]);


	ESSL_CHECK(phi = _essl_new_phi_expression(ctx->pool, false_bb));
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->sampler_subst_dict, n, phi));
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, n, phi));
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, phi, phi));
	phi->hdr.type = n->hdr.type;

	for(j = 0; j < 3 ; ++j)
	{
		ESSL_CHECK(external_components[j] = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, tex_lookup[j]));
		_essl_ensure_compatible_node(external_components[j], n);
		external_components[j]->expr.u.swizzle.indices[0] = j;
		ESSL_CHECK(external_components[j]->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_ctx, external_components[j]->hdr.type, 1));
	}

	ESSL_CHECK(yuv_res = make_color_space_conversion(ctx, external_components, coeff));

	{
		phi_source *src;
		phi_list *phi_l;
		ESSL_CHECK(phi_l = LIST_NEW(ctx->pool, phi_list));
		ESSL_CHECK(src = LIST_NEW(ctx->pool, phi_source));
		src->join_block = bb;
		src->source = tex_lookup[0];
		LIST_INSERT_FRONT(&phi->expr.u.phi.sources, src);

		ESSL_CHECK(src = LIST_NEW(ctx->pool, phi_source));
		src->join_block = true_bb;
		src->source = yuv_res;
		LIST_INSERT_FRONT(&phi->expr.u.phi.sources, src);
		phi_l->phi_node = phi;

		LIST_INSERT_FRONT(&false_bb->phi_nodes, phi_l);

	}

	remove_control_dependent_op_node(&bb->control_dependent_ops, tex_node->op);


	return MEM_OK;
}

static node *subst_external_use_w(rewrite_sampler_external_accesses_context *ctx, node *n)
{

	node *nn;

	if((nn = _essl_ptrdict_lookup(&ctx->sampler_subst_dict, n)) != 0)
	{
		return nn;
	}
	return n;
}

static basic_block *find_last_created_bb(rewrite_sampler_external_accesses_context *ctx, basic_block *bb)
{
	basic_block *res, *new_bb;
	res = bb;
	new_bb = _essl_ptrdict_lookup(&ctx->bb_subst_dict, bb);
	while(new_bb != NULL)
	{
		res = new_bb;
		new_bb =  _essl_ptrdict_lookup(&ctx->bb_subst_dict, new_bb);
	}

	return res;
}

static node * subst_external_use(rewrite_sampler_external_accesses_context *ctx, node *n)
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
		basic_block *new_join_block;
		for(src = n->expr.u.phi.sources; src != 0; src = src->next)
		{
			if(src->source->hdr.kind != EXPR_KIND_PHI)
			{
				node *tmp;
				ESSL_CHECK(tmp = subst_external_use(ctx, src->source));
				src->source = tmp;
			}
			new_join_block = find_last_created_bb(ctx, src->join_block);
			if(new_join_block != NULL)
			{
				src->join_block = new_join_block;
			}

		}

	} else {
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			node *child = GET_CHILD(n, i);
			if(child != NULL && child->hdr.kind != EXPR_KIND_PHI)
			{
				ESSL_CHECK(child = subst_external_use(ctx, child));
				SET_CHILD(n, i, child);
			}
		}
	}	
	ESSL_CHECK(nn = subst_external_use_w(ctx, n));
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, n, nn));

	return nn;
}



static memerr subst_external_uses(rewrite_sampler_external_accesses_context *ctx,
								basic_block *b)
{

	phi_list *phi;
	control_dependent_operation *c_op;
	if(b->source)
	{
		node *tmp;
		ESSL_CHECK(tmp = subst_external_use(ctx, b->source));
		b->source = tmp;
	}
	for(c_op = b->control_dependent_ops; c_op != 0; c_op = c_op->next)
	{
		node *n = c_op->op;
		assert(n->hdr.is_control_dependent);
		ESSL_CHECK(subst_external_use(ctx, n));
	}

	for(phi = b->phi_nodes; phi != 0; phi = phi->next)
	{		
		node *tmp;
		ESSL_CHECK(tmp = subst_external_use(ctx, phi->phi_node));
		phi->phi_node = tmp;
	}

	return MEM_OK;
}


static memerr handle_block_external(basic_block *b,
									ptrset *sampler_external_set)
{
	control_dependent_operation *c_op;
	node *n;

	for (c_op = b->control_dependent_ops; c_op != 0; c_op = c_op->next)
	{
		n = c_op->op;
		if(n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
		   (n->expr.operation == EXPR_OP_FUN_TEXTUREEXTERNAL ||
			n->expr.operation == EXPR_OP_FUN_TEXTUREEXTERNALPROJ ||
			n->expr.operation == EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W))
		{
			ESSL_CHECK(_essl_ptrset_insert(sampler_external_set, c_op));
		}
	}

	return MEM_OK;
}


static memerr handle_function(rewrite_sampler_external_accesses_context *ctx, symbol *fun)
{
	unsigned i;
	ptrset sampler_external_set;
	ptrset_iter it;
	control_dependent_operation *tex_node;
	control_flow_graph *cfg = ctx->cfg;

	ESSL_CHECK(_essl_ptrset_init(&sampler_external_set, ctx->pool));

	for (i = 0; i < cfg->n_blocks; i++)
	{
		ESSL_CHECK(handle_block_external(cfg->postorder_sequence[i], &sampler_external_set));
	}
	_essl_ptrset_iter_init(&it, &sampler_external_set);
	while ((tex_node = _essl_ptrset_next(&it)) != 0)
	{
		ESSL_CHECK(add_color_space_conversion_code(ctx, tex_node));
	}

	ESSL_CHECK(_essl_compute_dominance_information(ctx->pool, fun));

	for (i = 0; i < cfg->n_blocks; i++)
	{
		ESSL_CHECK(subst_external_uses(ctx, cfg->postorder_sequence[i]));
	}

	return MEM_OK;
}


memerr _essl_rewrite_sampler_external_accesses(mempool *pool, symbol *fun, target_descriptor *desc, typestorage_context *ts_ctx, translation_unit *tu, ptrdict *fixup_symbols)
{
	rewrite_sampler_external_accesses_context context;
	rewrite_sampler_external_accesses_context *ctx = &context;

	/* temporary pool */
	ESSL_CHECK(_essl_mempool_init(&ctx->temp_pool, 0, _essl_mempool_get_tracker(pool)));
	ctx->pool = pool;
	ctx->typestor_ctx = ts_ctx;
	ctx->tu = tu;
	ctx->desc = desc;
	ctx->cfg = fun->control_flow_graph;
	if( _essl_ptrdict_init(&ctx->visited, &ctx->temp_pool) == MEM_ERROR)
	{
		_essl_mempool_destroy(&ctx->temp_pool);
		return MEM_ERROR;
	}
	if( _essl_ptrdict_init(&ctx->sampler_subst_dict, &ctx->temp_pool) == MEM_ERROR)
	{
		_essl_mempool_destroy(&ctx->temp_pool);
		return MEM_ERROR;
	}
	if( _essl_ptrdict_init(&ctx->bb_subst_dict, &ctx->temp_pool) == MEM_ERROR)
	{
		_essl_mempool_destroy(&ctx->temp_pool);
		return MEM_ERROR;
	}
	ctx->fixup_symbols = fixup_symbols;

	if( handle_function(ctx, fun) == MEM_ERROR)
	{
		_essl_mempool_destroy(&ctx->temp_pool);
		return MEM_ERROR;
	}

	_essl_mempool_destroy(&ctx->temp_pool);
	return MEM_OK;
}

