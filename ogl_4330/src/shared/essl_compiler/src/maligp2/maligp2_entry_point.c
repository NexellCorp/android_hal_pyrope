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
#include "maligp2/maligp2_entry_point.h"
#include "frontend/frontend.h"

/*@null@*/ static node *create_float_constant(mempool *pool, target_descriptor *desc, typestorage_context *ts_ctx, float value, unsigned vec_size)
{
	node *n;
	unsigned i;
	ESSL_CHECK(n = _essl_new_constant_expression(pool, vec_size));
	n->expr.u.value[0] = desc->float_to_scalar(value);
	for(i = 1; i < vec_size; ++i)
	{
		n->expr.u.value[i] = n->expr.u.value[0];
	}

	ESSL_CHECK(n->hdr.type = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, vec_size, SIZE_FP32));
	return n;
}


/*@null@*/ static node *create_int_constant(mempool *pool, target_descriptor *desc,  typestorage_context *ts_ctx, int value, unsigned vec_size)
{
	node *n;
	unsigned i;
	ESSL_CHECK(n = _essl_new_constant_expression(pool, vec_size));
	n->expr.u.value[0] = desc->int_to_scalar(value);
	for(i = 1; i < vec_size; ++i)
	{
		n->expr.u.value[i] = n->expr.u.value[0];
	}

	ESSL_CHECK(n->hdr.type = _essl_get_type_with_size(ts_ctx, TYPE_INT, vec_size, SIZE_FP32));
	return n;
}


/*@null@*/ static node *do_gl_position(mempool *pool, typestorage_context *ts_ctx, target_descriptor *desc, symbol *global_sym, symbol *var_sym, symbol *viewport_sym)
{

	type_specifier *t;
	unsigned i;
	node *load, *assign, *dest, *pos_w, *pos_xyz, *inv_w, *mul1, *mul2, *add, *one, *view_xyz[2], *cons;
	ESSL_CHECK(load = _essl_new_variable_reference_expression(pool, global_sym));
	load->hdr.type = global_sym->type;
/*
 * Original code:
 *
 * vec4 tmp = gl_Position; // const register
 * float w = clamp(1.0/tmp.w, -1.0e10, +1.0e10);
 * gl_Position = vec4(tmp.xyz * gl_mali_Viewport[0].xyz * w + gl_mali_Viewport[1].xyz, w);
 *
 * MJOLL-2526
 * We found that a demo sets the gl_Position.z = gl_Position.w, willing to always have the normalized z at 1.0 for the skybox.
 * The ESSL compiler, instead of performing gl_Position.z /= gl_Position.w, performs gl_Position.z *= 1.0/gl_Position.w ending up in a precision loss
 *
 * After skybox holes fix:
 *
 * vec4 tmp = gl_Position; // const register
 * float w = clamp(1.0/tmp.w, -1.0e10, +1.0e10);
 * if(gl_Position.z == gl_Position.w)
 * {
 * 		tmp = vec3(tmp.xy, 1.0);
 * 		w_xx = vec3(w.xx, 1.0);
 * 		mul = tmp.xyz * gl_mali_Viewport[0].xyz * w_xx;
 * }else
 * {
 * 		mul = tmp.xyz * gl_mali_Viewport[0].xyz * w;
 * }
 * gl_Position = vec4(mul + gl_mali_Viewport[1].xyz, w);
 *
 * "if"  block will be optimized in optimise_vector_ops.rw (rules eq_of_eq_swz, eq_of_eq_swz_cmb, csel_const_cond)
 * leaving only true or false part of the code

*/
	ESSL_CHECK(pos_xyz = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, load));
	pos_xyz->expr.u.swizzle = _essl_create_identity_swizzle(3);
	ESSL_CHECK(t = _essl_clone_type(pool, load->hdr.type));
	t->u.basic.vec_size = 3;
	pos_xyz->hdr.type = t;
	
	ESSL_CHECK(pos_w = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, load));
	pos_w->expr.u.swizzle = _essl_create_scalar_swizzle(3);
	ESSL_CHECK(t = _essl_clone_type(pool, load->hdr.type));
	t->u.basic.vec_size = 1;
	pos_w->hdr.type = t;
	
	ESSL_CHECK(one = create_float_constant(pool, desc, ts_ctx, 1.0, 1));



	{
		node *rcp;
		node *hi, *lo;
		ESSL_CHECK(rcp = _essl_new_binary_expression(pool, one, EXPR_OP_DIV, pos_w));
		_essl_ensure_compatible_node(rcp, pos_w);
		ESSL_CHECK(lo = create_float_constant(pool, desc, ts_ctx, -1.0e10, 1));
		ESSL_CHECK(hi = create_float_constant(pool, desc, ts_ctx,  1.0e10, 1));
		
		ESSL_CHECK(inv_w = _essl_new_builtin_function_call_expression(pool, EXPR_OP_FUN_CLAMP, rcp, lo, hi));
		_essl_ensure_compatible_node(inv_w, rcp);

	}
	
	for(i = 0; i < 2; ++i)
	{
		node *view, *index, *indexed;
		ESSL_CHECK(view = _essl_new_variable_reference_expression(pool, viewport_sym));
		view->hdr.type = viewport_sym->type;
		ESSL_CHECK(index = create_int_constant(pool, desc, ts_ctx, i, 1));
	
		ESSL_CHECK(indexed = _essl_new_binary_expression(pool, view, EXPR_OP_INDEX, index));
		indexed->hdr.type = viewport_sym->type->child_type;

		ESSL_CHECK(view_xyz[i] = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, indexed));
		view_xyz[i]->expr.u.swizzle = _essl_create_identity_swizzle(3);
		ESSL_CHECK(t = _essl_clone_type(pool, indexed->hdr.type));
		t->u.basic.vec_size = 3;
		view_xyz[i]->hdr.type = t;
	}

	ESSL_CHECK(mul1 = _essl_new_binary_expression(pool, pos_xyz, EXPR_OP_MUL, view_xyz[0]));
	_essl_ensure_compatible_node(mul1, pos_xyz);

	ESSL_CHECK(mul2 = _essl_new_binary_expression(pool, mul1, EXPR_OP_MUL, inv_w));
	_essl_ensure_compatible_node(mul2, mul1);

	{
		node *pos_z;
		node *mul1_pos_xy1;
		node *mul2_pos_xy1;
		node *pos_xy;
		node *arg;
		node *cmp;
		node *inv_w_xx;

		ESSL_CHECK(pos_z = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, pos_xyz));
		pos_z->expr.u.swizzle = _essl_create_scalar_swizzle(2);
		ESSL_CHECK(t = _essl_clone_type(pool, pos_w->hdr.type));
		pos_z->hdr.type = t;

		ESSL_CHECK(cmp = _essl_new_binary_expression(pool, pos_z, EXPR_OP_EQ, pos_w));
		ESSL_CHECK(cmp->hdr.type = _essl_get_type(ts_ctx, TYPE_BOOL, 1));
		cmp->hdr.gl_pos_eq = ESSL_TRUE;

		ESSL_CHECK(pos_xy = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, pos_xyz));
		pos_xy->expr.u.swizzle = _essl_create_identity_swizzle(2);
		ESSL_CHECK(t = _essl_clone_type(pool, pos_xyz->hdr.type));
		t->u.basic.vec_size = 2;
		pos_xy->hdr.type = t;

		ESSL_CHECK(arg = _essl_new_builtin_constructor_expression(pool, 2));
		_essl_ensure_compatible_node(arg, pos_xyz);
		SET_CHILD(arg, 0, pos_xy);
		SET_CHILD(arg, 1, one);

		ESSL_CHECK(mul1_pos_xy1 = _essl_new_binary_expression(pool, arg, EXPR_OP_MUL, view_xyz[0]));
		_essl_ensure_compatible_node(mul1_pos_xy1, arg);

		ESSL_CHECK(inv_w_xx = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, inv_w));
		inv_w_xx->expr.u.swizzle.indices[0] = 0;
		inv_w_xx->expr.u.swizzle.indices[1] = 0;
		ESSL_CHECK(t = _essl_clone_type(pool, inv_w->hdr.type));
		t->u.basic.vec_size = 2;
		inv_w_xx->hdr.type = t;

		ESSL_CHECK(arg = _essl_new_builtin_constructor_expression(pool, 2));
		_essl_ensure_compatible_node(arg, pos_xyz);
		SET_CHILD(arg, 0, inv_w_xx);
		SET_CHILD(arg, 1, one);

		ESSL_CHECK(mul2_pos_xy1 = _essl_new_binary_expression(pool, mul1_pos_xy1, EXPR_OP_MUL, arg));
		_essl_ensure_compatible_node(mul2_pos_xy1, mul1_pos_xy1);
		
		ESSL_CHECK(mul2 = _essl_new_ternary_expression(pool, EXPR_OP_CONDITIONAL, cmp, mul2_pos_xy1, mul2));
		_essl_ensure_compatible_node(mul2, mul2_pos_xy1);
	}

	ESSL_CHECK(add = _essl_new_binary_expression(pool, mul2, EXPR_OP_ADD, view_xyz[1]));
	_essl_ensure_compatible_node(add, mul2);
											
	ESSL_CHECK(cons = _essl_new_builtin_constructor_expression(pool, 2));
	_essl_ensure_compatible_node(cons, load);
	SET_CHILD(cons, 0, add);
	SET_CHILD(cons, 1, inv_w);

	ESSL_CHECK(dest = _essl_new_variable_reference_expression(pool, var_sym));
	dest->hdr.type = global_sym->type;
	
	ESSL_CHECK(assign = _essl_new_assign_expression(pool, dest, EXPR_OP_ASSIGN, cons));
	assign->hdr.type = global_sym->type;
	return assign;
}

/*@null@*/ static node *do_gl_pointsize(mempool *pool, typestorage_context *ts_ctx, symbol *global_sym, symbol *var_sym, symbol *params_sym)
{

	unsigned i;
	node *params_load;
	node *params_components[3];
	node *pointsize_load;
	node *clamp;
	node *mul;
	node *assign;
	node *dest;

/*
  final_gl_PointSize = clamp(gl_PointSize, gl_mali_PointSizeParameters.x, gl_mali_PointSizeParameters.y) * gl_mali_PointSizeParameters.z;
*/

	ESSL_CHECK(pointsize_load = _essl_new_variable_reference_expression(pool, global_sym));
	pointsize_load->hdr.type = global_sym->type;

	ESSL_CHECK(params_load = _essl_new_variable_reference_expression(pool, params_sym));
	params_load->hdr.type = params_sym->type;


	for(i = 0; i < 3; ++i)
	{

		ESSL_CHECK(params_components[i] = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, params_load));
		params_components[i]->expr.u.swizzle = _essl_create_scalar_swizzle(i);
		ESSL_CHECK(params_components[i]->hdr.type = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, 1, SIZE_FP32));
	}
	
	ESSL_CHECK(clamp = _essl_new_builtin_function_call_expression(pool, EXPR_OP_FUN_CLAMP, pointsize_load, params_components[0], params_components[1]));
	_essl_ensure_compatible_node(clamp, pointsize_load);

	ESSL_CHECK(mul = _essl_new_binary_expression(pool, clamp, EXPR_OP_MUL, params_components[2]));
	_essl_ensure_compatible_node(mul, clamp);

	ESSL_CHECK(dest = _essl_new_variable_reference_expression(pool, var_sym));
	dest->hdr.type = global_sym->type;
	
	ESSL_CHECK(assign = _essl_new_assign_expression(pool, dest, EXPR_OP_ASSIGN, mul));
	assign->hdr.type = global_sym->type;
	return assign;
}



symbol *_essl_maligp2_insert_entry_point(mempool *pool, typestorage_context *ts_ctx, translation_unit *tu, node *root, symbol *main)
{
	/* produce a function like this:

	   void __start(void) 
	   { 
	       main();  
		   vec4 tmp = gl_Position; // const register
		   float w = clamp(1.0/tmp.w, -1.0e10, +1.0e10);
		   gl_Position = vec4(tmp.xyz * gl_mali_Viewport[0].xyz * w + gl_mali_Viewport[1].xyz, w);
		   gl_PointSize = clamp(gl_PointSize, gl_mali_PointSizeParameters.x, gl_mali_PointSizeParameters.y) * gl_mali_PointSizeParameters.z;

		   varying1_out = varying1_const;
		   varying2_out = varying2_const;
		   varying3_out = varying3_const;
	       ...
	   }

	*/
	const type_specifier *ret_type;
	string viewport_name            = {"gl_mali_ViewportTransform", 25};
	string pointsizeparameters_name = {"gl_mali_PointSizeParameters", 27};
	string start_name               = {"__start", 7};
	string gl_Position_name         = {"gl_Position", 11};
	string gl_PointSize_name        = {"gl_PointSize", 12};
	scope *global_scope = root->stmt.child_scope;
	node *call = 0, *ret = 0, *compound = 0, *decl;
	symbol *entry_point;
	symbol *viewport_sym;
	symbol *pointsizeparameters_sym;
	qualifier_set ret_qual;
	qualifier_set uniform_highp_qual;

	_essl_init_qualifier_set(&ret_qual);

	_essl_init_qualifier_set(&uniform_highp_qual);
	uniform_highp_qual.variable = VAR_QUAL_UNIFORM;
	uniform_highp_qual.precision = PREC_HIGH;

	ESSL_CHECK(global_scope);
	ESSL_CHECK(ret_type = _essl_get_type_with_default_size_for_target(ts_ctx, TYPE_VOID, 1, tu->desc));
	
	ESSL_CHECK(call = _essl_new_function_call_expression(pool, main, 0));
	call->hdr.type = main->type;

	ESSL_CHECK(compound = _essl_new_compound_statement(pool));

	ESSL_CHECK(_essl_insert_global_variable_initializers(pool, root, compound));

	ESSL_CHECK(APPEND_CHILD(compound, call, pool));

	{
		const type_specifier *tmp_t;
		type_specifier *t;
		node *viewport_decl;
		ESSL_CHECK(tmp_t = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, 4, SIZE_FP32));
		ESSL_CHECK(t = _essl_new_array_of_type(pool, tmp_t, 2));
		ESSL_CHECK(viewport_sym = _essl_new_variable_symbol(pool, viewport_name, t, uniform_highp_qual, SCOPE_GLOBAL, ADDRESS_SPACE_UNIFORM, UNKNOWN_SOURCE_OFFSET));
		ESSL_CHECK(_essl_symbol_scope_insert(root->stmt.child_scope, viewport_name, viewport_sym));
		ESSL_CHECK(viewport_decl = _essl_new_variable_declaration(pool, viewport_sym, 0));
		ESSL_CHECK(APPEND_CHILD(root, viewport_decl, pool));
		{
			symbol_list *sl;
			ESSL_CHECK(sl = LIST_NEW(pool, symbol_list));
			sl->sym = viewport_sym;
			LIST_INSERT_FRONT(&tu->uniforms, sl);
		}
	}


	{
		const type_specifier *vec4;
		node *pointsizeparameters_decl;
		ESSL_CHECK(vec4 = _essl_get_type_with_size(ts_ctx, TYPE_FLOAT, 4, SIZE_FP32));
		ESSL_CHECK(pointsizeparameters_sym = _essl_new_variable_symbol(pool, pointsizeparameters_name, vec4, uniform_highp_qual, SCOPE_GLOBAL, ADDRESS_SPACE_UNIFORM, UNKNOWN_SOURCE_OFFSET));
		ESSL_CHECK(_essl_symbol_scope_insert(root->stmt.child_scope, pointsizeparameters_name, pointsizeparameters_sym));
		ESSL_CHECK(pointsizeparameters_decl = _essl_new_variable_declaration(pool, pointsizeparameters_sym, 0));
		ESSL_CHECK(APPEND_CHILD(root, pointsizeparameters_decl, pool));
		{
			symbol_list *sl;
			ESSL_CHECK(sl = LIST_NEW(pool, symbol_list));
			sl->sym = pointsizeparameters_sym;
			LIST_INSERT_FRONT(&tu->uniforms, sl);
		}
		
		

	}
	/* iterate through the vertex varying symbols and rewrite them */
	{

		/* since we'll be inserting declarations into the root, we need to retrieve the number of children before the insertions begin */
		symbol_list *elem = tu->vertex_varyings;
		tu->vertex_varyings = 0; /* reset the list */
		for(; elem != 0; elem = elem->next)
		{
			symbol *global_sym, *var_sym;
			global_sym = elem->sym;
			if(global_sym->address_space == ADDRESS_SPACE_VERTEX_VARYING)
			{
				symbol_list *var_elem;
				var_sym = global_sym;
				if(global_sym->is_used || _essl_string_cmp(global_sym->name, gl_Position_name) == 0)
				{
					symbol_list *global_elem;
					node *varying_decl;
					ESSL_CHECK(var_sym = _essl_new_variable_symbol(pool, global_sym->name, global_sym->type, 
																		global_sym->qualifier, global_sym->scope, 
																		global_sym->address_space, UNKNOWN_SOURCE_OFFSET));
					*var_sym = *global_sym;
					global_sym->address_space = ADDRESS_SPACE_GLOBAL;
					
					elem->sym = var_sym;
					ESSL_CHECK(global_elem = LIST_NEW(pool, symbol_list));
					global_elem->sym = global_sym;
					LIST_INSERT_FRONT(&tu->globals, global_elem);
					
					ESSL_CHECK(varying_decl = _essl_new_variable_declaration(pool, var_sym, NULL));
					varying_decl->hdr.type = var_sym->type;
					ESSL_CHECK(PREPEND_CHILD(tu->root, varying_decl, pool));
					ESSL_CHECK(_essl_symbol_scope_insert(tu->root->stmt.child_scope, var_sym->name, var_sym));
					if(_essl_string_cmp(global_sym->name, gl_Position_name) == 0 && !tu->lang_desc->disable_vertex_shader_output_rewrites)
					{
						/* gl_Position case */
						node *assign;
						ESSL_CHECK(assign = do_gl_position(pool, ts_ctx, tu->desc, global_sym, var_sym, viewport_sym));
						ESSL_CHECK(APPEND_CHILD(compound, assign, pool));
					} else if(_essl_string_cmp(global_sym->name, gl_PointSize_name) == 0 && !tu->lang_desc->disable_vertex_shader_output_rewrites)
					{
						/* gl_PointSize case */
						node *assign;
						ESSL_CHECK(assign = do_gl_pointsize(pool, ts_ctx, global_sym, var_sym, pointsizeparameters_sym));
						ESSL_CHECK(APPEND_CHILD(compound, assign, pool));
					} else {
						/* normal case */
						node *load, *assign, *dest;
						ESSL_CHECK(load = _essl_new_variable_reference_expression(pool, global_sym));
						load->hdr.type = global_sym->type;
						ESSL_CHECK(dest = _essl_new_variable_reference_expression(pool, var_sym));
						dest->hdr.type = global_sym->type;
						
						ESSL_CHECK(assign = _essl_new_assign_expression(pool, dest, EXPR_OP_ASSIGN, load));
						assign->hdr.type = global_sym->type;
						ESSL_CHECK(APPEND_CHILD(compound, assign, pool));
					}

				}
				ESSL_CHECK(var_elem = LIST_NEW(pool, symbol_list));
				var_elem->sym = var_sym;
				LIST_INSERT_FRONT(&tu->vertex_varyings, var_elem);
					
					
			}

		}
	}

	ESSL_CHECK(ret = _essl_new_flow_control_statement(pool, STMT_KIND_RETURN, 0));
	ret->hdr.type = ret_type;
	ESSL_CHECK(APPEND_CHILD(compound, ret, pool));

	ESSL_CHECK(compound->stmt.child_scope = _essl_symbol_table_begin_scope(global_scope));

	ESSL_CHECK(entry_point = _essl_new_function_symbol(pool, start_name, ret_type, ret_qual, UNKNOWN_SOURCE_OFFSET));

	ESSL_CHECK(decl = _essl_new_function_declaration(pool, entry_point));
	SET_CHILD(decl, 0, compound);
	entry_point->body = compound;

	ESSL_CHECK(APPEND_CHILD(root, decl, pool));
	

	return entry_point;
}
