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
#include "common/essl_symbol.h"

#include "common/essl_mem.h"
#include "common/essl_str.h"
#include "common/symbol_table.h"
#include "mali200/mali200_entry_point.h"
#include "mali200/mali200_external.h"
#include "mali200/mali200_grad.h"
#include "frontend/frontend.h"

#define M200_SCALAR_SIZE SIZE_FP16

static symbol * add_uniform_with_specific_type(mempool *pool, translation_unit *tu, scope *global_scope,
						  const char *name, const type_specifier *uniform_type)
{
	string uniform_name;
	symbol *uniform_symbol;
	symbol_list *sl;
	qualifier_set uniform_medp_qual;
	uniform_name = _essl_cstring_to_string(pool, name);
	ESSL_CHECK(uniform_name.ptr);
	_essl_init_qualifier_set(&uniform_medp_qual);
	uniform_medp_qual.variable = VAR_QUAL_UNIFORM;
	uniform_medp_qual.precision = PREC_MEDIUM;
	ESSL_CHECK(uniform_symbol = _essl_new_variable_symbol(pool, uniform_name, uniform_type, uniform_medp_qual, SCOPE_GLOBAL, ADDRESS_SPACE_UNIFORM, UNKNOWN_SOURCE_OFFSET));
	uniform_symbol->opt.is_indexed_statically = ESSL_FALSE;

	ESSL_CHECK(sl = LIST_NEW(pool, symbol_list));
	sl->sym = uniform_symbol;
	LIST_INSERT_FRONT(&tu->uniforms, sl);

	ESSL_CHECK(_essl_symbol_scope_insert(global_scope, uniform_name, uniform_symbol));

	return uniform_symbol;
}

static symbol * add_uniform(mempool *pool, typestorage_context *ts_ctx, translation_unit *tu, scope *global_scope,
						  const char *name, int size)
{
	const type_specifier *uniform_type;
	ESSL_CHECK(uniform_type = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, size, M200_SCALAR_SIZE));
	return add_uniform_with_specific_type(pool, tu, global_scope, name, uniform_type);
}

static unsigned count_specified_samplers(translation_unit *tu, type_basic sampler_type)
{
	symbol_list *sl;
	unsigned res = 0;
	for(sl = tu->uniforms; sl != NULL; sl = sl->next)
	{
		if(sl->sym->is_used)
		{
			res += _essl_get_specified_samplers_num(sl->sym->type, sampler_type);

		}
	}
	return res;
}

symbol *_essl_mali200_insert_entry_point(mempool *pool, typestorage_context *ts_ctx, translation_unit *tu, node *root, symbol *main)
{
	/* produce a function like this:

	   vec4 __start(void) { glob_var_1 = initializer; glob_var_2 = initializer; main(); return gl_FragColor; }

	*/
	const type_specifier *vec4;
	string start_name = {"__start", 7};
	scope *global_scope = root->stmt.child_scope;
	node *call = 0, *load = 0, *ret = 0, *compound = 0, *decl;
	symbol *entry_point;
	qualifier_set medp_qual;
	_essl_init_qualifier_set(&medp_qual);
	medp_qual.precision = PREC_MEDIUM;
	ESSL_CHECK(global_scope);
	ESSL_CHECK(vec4 = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));
	
	ESSL_CHECK(call = _essl_new_function_call_expression(pool, main, 0));
	call->hdr.type = main->type;
	{
		string fragcolor_name = {"gl_FragColor", 12};
		string fragdata_name = {"gl_FragData", 11};
		symbol *gl_frag_sym;
		ESSL_CHECK(gl_frag_sym = _essl_symbol_table_lookup(global_scope, fragdata_name));
		assert(gl_frag_sym->type->basic_type == TYPE_ARRAY_OF && gl_frag_sym->type->u.array_size == 1 && "This code does not support multiple render targets yet.");
		if(gl_frag_sym->is_used)
		{
			node *var_ref, *constant;

			ESSL_CHECK(var_ref = _essl_new_variable_reference_expression(pool, gl_frag_sym));
			ESSL_CHECK(var_ref->hdr.type = _essl_get_unqualified_type(pool, gl_frag_sym->type));
			
			ESSL_CHECK(constant = _essl_new_constant_expression(pool, 1));
			ESSL_CHECK(constant->hdr.type = _essl_get_type_with_size(ts_ctx, TYPE_INT, 1, M200_SCALAR_SIZE));
			constant->expr.u.value[0] = tu->desc->int_to_scalar(0);

			ESSL_CHECK(load = _essl_new_binary_expression(pool, var_ref, EXPR_OP_INDEX, constant));
			load->hdr.type = vec4;
			
			
		} else {
		

			ESSL_CHECK(gl_frag_sym = _essl_symbol_table_lookup(global_scope, fragcolor_name));
	
			ESSL_CHECK(load = _essl_new_variable_reference_expression(pool, gl_frag_sym));
			load->hdr.type = vec4;
		}
	}

	ESSL_CHECK(ret = _essl_new_flow_control_statement(pool, STMT_KIND_RETURN, load));
	ret->hdr.type = load->hdr.type;
	ESSL_CHECK(compound = _essl_new_compound_statement(pool));

	ESSL_CHECK(_essl_insert_global_variable_initializers(pool, root, compound));

	ESSL_CHECK(APPEND_CHILD(compound, call, pool));
	ESSL_CHECK(APPEND_CHILD(compound, ret, pool));

	ESSL_CHECK(compound->stmt.child_scope = _essl_symbol_table_begin_scope(global_scope));

	ESSL_CHECK(entry_point = _essl_new_function_symbol(pool, start_name, vec4, medp_qual, UNKNOWN_SOURCE_OFFSET));

	ESSL_CHECK(decl = _essl_new_function_declaration(pool, entry_point));
	SET_CHILD(decl, 0, compound);
	entry_point->body = compound;

	ESSL_CHECK(APPEND_CHILD(root, decl, pool));

	/* Add pointcoord scale and bias uniform */
	ESSL_CHECK(add_uniform(pool, ts_ctx, tu, global_scope, "gl_mali_PointCoordScaleBias", 4));
	/* Add fragcoord scale uniform */
	ESSL_CHECK(add_uniform(pool, ts_ctx, tu, global_scope, "gl_mali_FragCoordScale", 3));
	/* Add derivative scale uniform */
	ESSL_CHECK(add_uniform(pool, ts_ctx, tu, global_scope, "gl_mali_DerivativeScale", 2));

	/* EXTERNAL stuff */
	{
		unsigned n_external_samplers = count_specified_samplers(tu, TYPE_SAMPLER_EXTERNAL);
		const type_specifier *vec4;
		const type_specifier *array_vec4;
		qualifier_set medp_qual;
		string external_sampler_start = _essl_cstring_to_string_nocopy(NEGATIVE_EXTERNAL_SAMPLER_START_NAME);
		symbol *external_sampler_start_symbol;

		ESSL_CHECK(vec4 = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));

		ESSL_CHECK(array_vec4 = _essl_new_array_of_type(pool, vec4, 4*n_external_samplers));
		ESSL_CHECK(add_uniform_with_specific_type(pool, tu, global_scope, EXTERNAL_COEFFICIENTS_NAME, array_vec4));

		_essl_init_qualifier_set(&medp_qual);
		medp_qual.precision = PREC_MEDIUM;
		ESSL_CHECK(external_sampler_start_symbol = _essl_new_variable_symbol(pool, external_sampler_start, vec4, medp_qual, SCOPE_GLOBAL, ADDRESS_SPACE_FRAGMENT_SPECIAL, UNKNOWN_SOURCE_OFFSET));
		ESSL_CHECK(_essl_symbol_scope_insert(global_scope, external_sampler_start, external_sampler_start_symbol));
	}
	/* Grad stuff */
	{
		unsigned n_grad_samplers = count_specified_samplers(tu, TYPE_SAMPLER_2D) + count_specified_samplers(tu, TYPE_SAMPLER_CUBE);
		const type_specifier *vec2;
		const type_specifier *array_vec2;

		const type_specifier *vec4;
		const type_specifier *array_vec4;
		symbol *enc_const;
		node *ini;
		int stride;

		ESSL_CHECK(vec2 = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, 2, M200_SCALAR_SIZE));

		ESSL_CHECK(array_vec2 = _essl_new_array_of_type(pool, vec2, n_grad_samplers));
		ESSL_CHECK(add_uniform_with_specific_type(pool, tu, global_scope, TEXTURE_GRADEXT_SIZES_NAME, array_vec2));

		ESSL_CHECK(vec4 = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, 4, M200_SCALAR_SIZE));

		ESSL_CHECK(array_vec4 = _essl_new_array_of_type(pool, vec4, 6));
		ESSL_CHECK(enc_const = add_uniform_with_specific_type(pool, tu, global_scope, TEXTURE_CUBEGRADEXT_ENC_CONST, array_vec4));

		/*4.1. creating a constant to encode what indices to take depending on face value */
		/* +x:1, +y:2: +z:3; -x:-1; -y:-2; -z:-3  */

		/* +x (-T.z, -T.y) T.x : (-3, -2, 1, -1) */
		/* -x (T.z, -T.y)  T.x : ( 3, -2, 1, -1) */
		/* +y (T.x, T.z)   T.y : ( 1,  3, 2, -1) */
		/* -y (T.x, -T.z)  T.y : ( 1, -3, 2, -1) */
		/* +z (T.x, -T.y)  T.z : ( 1, -2, 3, -1) */
		/* -z (-T.x, -T.y) T.z : (-1, -2, 3, -1) */
	
		ESSL_CHECK(ini = _essl_new_constant_expression(pool, 24));
		/* +x face*/
		stride = 0;
		ini->expr.u.value[stride + 0] = tu->desc->float_to_scalar(-3);
		ini->expr.u.value[stride + 1] = tu->desc->float_to_scalar(-2);
		ini->expr.u.value[stride + 2] = tu->desc->float_to_scalar(1);
		ini->expr.u.value[stride + 3] = tu->desc->float_to_scalar(-1);

		/* -x face*/
		stride = 4;
		ini->expr.u.value[stride + 0] = tu->desc->float_to_scalar(3);
		ini->expr.u.value[stride + 1] = tu->desc->float_to_scalar(-2);
		ini->expr.u.value[stride + 2] = tu->desc->float_to_scalar(1);
		ini->expr.u.value[stride + 3] = tu->desc->float_to_scalar(-1);

		/* +y face*/
		stride = 8;
		ini->expr.u.value[stride + 0] = tu->desc->float_to_scalar(1);
		ini->expr.u.value[stride + 1] = tu->desc->float_to_scalar(3);
		ini->expr.u.value[stride + 2] = tu->desc->float_to_scalar(2);
		ini->expr.u.value[stride + 3] = tu->desc->float_to_scalar(-1);

		/* -y face*/
		stride = 12;
		ini->expr.u.value[stride + 0] = tu->desc->float_to_scalar(1);
		ini->expr.u.value[stride + 1] = tu->desc->float_to_scalar(-3);
		ini->expr.u.value[stride + 2] = tu->desc->float_to_scalar(2);
		ini->expr.u.value[stride + 3] = tu->desc->float_to_scalar(-1);

		/* +z face*/
		stride = 16;
		ini->expr.u.value[stride + 0] = tu->desc->float_to_scalar(1);
		ini->expr.u.value[stride + 1] = tu->desc->float_to_scalar(-2);
		ini->expr.u.value[stride + 2] = tu->desc->float_to_scalar(3);
		ini->expr.u.value[stride + 3] = tu->desc->float_to_scalar(-1);

		/* -z face*/
		stride = 20;
		ini->expr.u.value[stride + 0] = tu->desc->float_to_scalar(-1);
		ini->expr.u.value[stride + 1] = tu->desc->float_to_scalar(-2);
		ini->expr.u.value[stride + 2] = tu->desc->float_to_scalar(3);
		ini->expr.u.value[stride + 3] = tu->desc->float_to_scalar(-1);
	
		ESSL_CHECK(ini->hdr.type = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, 24, M200_SCALAR_SIZE));
		enc_const->body = ini;



	}

	return entry_point;
}
