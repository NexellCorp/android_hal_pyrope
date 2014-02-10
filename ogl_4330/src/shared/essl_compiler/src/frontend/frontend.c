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
#include "frontend/frontend.h"
#include "common/essl_common.h"
#include "common/translation_unit.h"
#include "common/ptrdict.h"
#include "frontend/callgraph.h"
#include "common/essl_profiling.h"
#include "frontend/global_variable_inlining.h"

typedef enum { NOT_VISITED, PARTIALLY_VISITED, FULLY_VISITED } visit_status;

typedef struct 
{
	mempool *pool;
	ptrdict visited;
	translation_unit *tu;
	essl_bool recursion_found;
} partial_sort_context;

static memerr function_partial_sort(partial_sort_context *ctx, symbol *fun)
{
	struct call_graph *it;
	symbol_list *sl;

	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, fun, (void*)PARTIALLY_VISITED));
	for(it = fun->calls_to; it != 0; it = it->next)
	{
		visit_status v = (visit_status) _essl_ptrdict_lookup(&ctx->visited, it->func);
		if(v == PARTIALLY_VISITED) ctx->recursion_found = ESSL_TRUE;
		else if(v == NOT_VISITED)
		{
			ESSL_CHECK(function_partial_sort(ctx, it->func));
		}
	}
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited, fun, (void*)FULLY_VISITED));
	ESSL_CHECK(sl = LIST_NEW(ctx->pool, symbol_list));
	sl->sym = fun;
	LIST_INSERT_FRONT(&ctx->tu->functions, sl);

	return MEM_OK;
}


static memerr fixup_functions(mempool *pool, translation_unit *tu, error_context *err_ctx)
{
	partial_sort_context partial_sort_ctx;
	partial_sort_ctx.pool = pool;
	partial_sort_ctx.tu = tu;
	ESSL_CHECK(_essl_ptrdict_init(&partial_sort_ctx.visited, pool));
	partial_sort_ctx.recursion_found = ESSL_FALSE;

	ESSL_CHECK(function_partial_sort(&partial_sort_ctx, tu->entry_point));
	tu->functions = LIST_REVERSE(tu->functions, symbol_list);
	assert(tu->functions != 0);
	if(partial_sort_ctx.recursion_found)
	{
		(void)REPORT_ERROR(err_ctx, ERR_SEM_RECURSION_FOUND, 0, "Shader contains static recursion\n");
	}

	return MEM_OK;
}

static memerr make_function_list(mempool *pool, translation_unit *tu) {
	node *decl_list = tu->root;
	unsigned i;
	for (i = 0 ; i < GET_N_CHILDREN(decl_list) ; i++)
	{
		node *decl = GET_CHILD(decl_list, i);
		if (decl != 0 && decl->hdr.kind == DECL_KIND_FUNCTION) {
			symbol_list *sl;
			ESSL_CHECK(sl = LIST_NEW(pool, symbol_list));
			sl->sym = decl->decl.sym;
			LIST_INSERT_BACK(&tu->functions, sl);
		}
	}
	return MEM_OK;
}


memerr _essl_insert_global_variable_initializers(mempool *pool, node *root, node *compound)
{
	unsigned i;
	for(i = 0; i < GET_N_CHILDREN(root); ++i)
	{
		node *n = GET_CHILD(root, i);
		if(n != 0 && n->hdr.kind == DECL_KIND_VARIABLE)
		{
			/* regular global variable */
			node *initializer = GET_CHILD(n, 0);
			
			if(initializer != 0)
			{
				if(n->decl.sym->qualifier.variable != VAR_QUAL_CONSTANT)
				{
					node *var_ref;
					node *assign;
					assert(n->decl.sym->qualifier.variable == VAR_QUAL_NONE);

					ESSL_CHECK(var_ref = _essl_new_variable_reference_expression(pool, n->decl.sym));
					_essl_ensure_compatible_node(var_ref, n);

					ESSL_CHECK(assign = _essl_new_assign_expression(pool, var_ref, EXPR_OP_ASSIGN, initializer));
					_essl_ensure_compatible_node(assign, n);

					ESSL_CHECK(APPEND_CHILD(compound, assign, pool));
					
				}
			}

		}
		
	}
	return MEM_OK;
}




static /*@null@*/ translation_unit *translation_unit_from_root_node(mempool *pool, target_descriptor *desc, language_descriptor *lang, /*@unused@*/ node *root)
{
	translation_unit *tu;
	scope *global_scope = root->stmt.child_scope;
	scope_iter it;
	symbol *sym;
	ESSL_CHECK(tu = _essl_mempool_alloc(pool, sizeof(translation_unit)));
	tu->entry_point = 0;
	tu->desc = desc;
	tu->lang_desc = lang;
	tu->root = root;
	tu->buffer_usage.color_write = ESSL_TRUE;
	_essl_symbol_table_iter_init(&it, global_scope);
	while(0 != (sym = _essl_symbol_table_next(&it)))
	{
		if(sym->kind == SYM_KIND_VARIABLE)
		{
			symbol_list *elem;
			ESSL_CHECK(elem = LIST_NEW(pool, symbol_list));
			elem->sym = sym;
			switch(sym->address_space)
			{
			case ADDRESS_SPACE_GLOBAL:
				if (!sym->is_persistent_variable)
				{
					LIST_INSERT_BACK(&tu->globals, elem);
					break;
				}
				/* Treat persistent variables as uniforms */

			case ADDRESS_SPACE_UNIFORM:
				LIST_INSERT_BACK(&tu->uniforms, elem);
				break;

			case ADDRESS_SPACE_ATTRIBUTE:
				LIST_INSERT_BACK(&tu->vertex_attributes, elem);
				break;

			case ADDRESS_SPACE_VERTEX_VARYING:
				LIST_INSERT_BACK(&tu->vertex_varyings, elem);
				break;

			case ADDRESS_SPACE_FRAGMENT_VARYING:
				LIST_INSERT_BACK(&tu->fragment_varyings, elem);
				break;

			case ADDRESS_SPACE_FRAGMENT_SPECIAL:
				LIST_INSERT_BACK(&tu->fragment_specials, elem);
				break;

			case ADDRESS_SPACE_FRAGMENT_OUT:
				LIST_INSERT_BACK(&tu->fragment_outputs, elem);
				break;

			default:
				assert(0);
				break;

			}

		}

	}


	return tu;
}

translation_unit *_essl_run_frontend(frontend_context *ctx)
{
	node *root = 0;
	string main_name = {"main", 4};
	symbol *main_symbol = 0;
	translation_unit *tu = 0;
	TIME_PROFILE_START("parse");
	ESSL_CHECK_NONFATAL(root = _essl_parse_translation_unit(&ctx->parse_context));
	TIME_PROFILE_STOP("parse");
	if(_essl_error_get_n_errors(ctx->err_context) || _essl_mempool_get_tracker(ctx->pool)->out_of_memory_encountered) return 0;

	TIME_PROFILE_START("typecheck");
	ESSL_CHECK_NONFATAL(root = _essl_typecheck(&ctx->typecheck_context, root, NULL));
	TIME_PROFILE_STOP("typecheck");
	if(_essl_error_get_n_errors(ctx->err_context) || _essl_mempool_get_tracker(ctx->pool)->out_of_memory_encountered) return 0;

	TIME_PROFILE_START("translation_unit");
	ESSL_CHECK(tu = translation_unit_from_root_node(ctx->pool, ctx->desc, ctx->lang_desc, root));
	TIME_PROFILE_STOP("translation_unit");

	assert(root->stmt.child_scope != 0);

	if (ctx->desc->has_entry_point)
	{
		compiler_options *opts = ctx->desc->options;
		main_symbol = _essl_symbol_table_lookup(root->stmt.child_scope, main_name);
		if(!main_symbol)
		{
			(void)REPORT_ERROR(ctx->err_context, ERR_LINK_NO_ENTRY_POINT, 0, "Missing main() function for shader\n");
			return 0;
		}
	
		if(main_symbol->next)
		{
			(void)REPORT_ERROR(ctx->err_context, ERR_SEM_MAIN_SIGNATURE_MISMATCH, 0, "main() has been overloaded\n");
			return 0;
		}

		if(main_symbol->type->basic_type != TYPE_VOID || main_symbol->parameters)
		{
			(void)REPORT_ERROR(ctx->err_context, ERR_SEM_MAIN_SIGNATURE_MISMATCH, 0, "Signature mismatch for main()\n");
			return 0;
		}

		if(ctx->desc->kind == TARGET_FRAGMENT_SHADER)
		{
			string gl_FragColor_name = {"gl_FragColor", 12};
			string gl_FragData_name = {"gl_FragData", 11};
			symbol *fragcolor_symbol = 0;
			symbol *fragdata_symbol = 0;
			ESSL_CHECK(fragcolor_symbol = _essl_symbol_table_lookup(root->stmt.child_scope, gl_FragColor_name));
			ESSL_CHECK(fragdata_symbol = _essl_symbol_table_lookup(root->stmt.child_scope, gl_FragData_name));
			if(fragcolor_symbol->is_used && fragdata_symbol->is_used)
			{
				(void)REPORT_ERROR(ctx->err_context, ERR_SEM_GL_FRAGDATA_GL_POSITION_MIX, 0, "gl_FragColor and gl_FragData both used in the same fragment shader\n");
			}

		} else if(ctx->desc->kind == TARGET_VERTEX_SHADER)
		{
			string gl_Position_name = {"gl_Position", 11};
			symbol *position_symbol = 0;
			ESSL_CHECK(position_symbol = _essl_symbol_table_lookup(root->stmt.child_scope, gl_Position_name));
			if(!position_symbol->is_used)
			{
				(void)REPORT_WARNING(ctx->err_context, ERR_WARNING, 0, "Vertex shader where gl_Position isn't written\n");
			}


		}


		TIME_PROFILE_START("entry_point");
		ESSL_CHECK(tu->entry_point = ctx->desc->insert_entry_point(ctx->pool, ctx->typestor_context, tu, root, main_symbol));
		TIME_PROFILE_STOP("entry_point");

		TIME_PROFILE_START("callgraph");
		ESSL_CHECK(_essl_make_callgraph(ctx->pool, root));
		TIME_PROFILE_STOP("callgraph");

		if(!fixup_functions(ctx->pool, tu, ctx->err_context)) return 0;

		if(opts != NULL && opts->optimise_global_variables != 0)
		{
			ptrset global_vars_to_inline;
			symbol_list *sl;
			ESSL_CHECK(_essl_ptrset_init(&global_vars_to_inline, ctx->pool));

			for(sl = tu->globals; sl != NULL; sl = sl->next)
			{
				if(!_essl_is_type_control_dependent(sl->sym->type, sl->sym->opt.is_indexed_statically) && sl->sym->qualifier.variable != VAR_QUAL_CONSTANT && !sl->sym->is_persistent_variable)
				{
					ESSL_CHECK(_essl_ptrset_insert(&global_vars_to_inline, sl->sym));
				}
			}
			for(sl = tu->fragment_outputs; sl != NULL; sl = sl->next)
			{
				if(!_essl_is_type_control_dependent(sl->sym->type, sl->sym->opt.is_indexed_statically) && sl->sym->qualifier.variable != VAR_QUAL_CONSTANT)
				{
					ESSL_CHECK(_essl_ptrset_insert(&global_vars_to_inline, sl->sym));
				}
			}

			ESSL_CHECK(_essl_inline_global_variables(ctx->pool, tu, &global_vars_to_inline));

		}

	}
	else
	{
		ESSL_CHECK(make_function_list(ctx->pool, tu));
	}

	if(_essl_error_get_n_errors(ctx->err_context) || _essl_mempool_get_tracker(ctx->pool)->out_of_memory_encountered) return 0;

	return tu;
}



frontend_context *_essl_new_frontend(mempool *pool, target_descriptor *desc, const char *input_string, const int *source_string_lengths, unsigned int n_source_strings)
{
	frontend_context *ctx = _essl_mempool_alloc(pool, sizeof(frontend_context));
	error_context *e_ctx = _essl_mempool_alloc(pool, sizeof(error_context));
	typestorage_context *ts_ctx = _essl_mempool_alloc(pool, sizeof(typestorage_context));
	language_descriptor *lang_desc = _essl_create_language_descriptor(pool, e_ctx, desc);

	if (ctx == 0) return 0;
	if (e_ctx == 0) return 0;
	if (ts_ctx == 0) return 0;
	if (lang_desc == 0) return 0;

	TIME_PROFILE_START("error_init");
	ESSL_CHECK(_essl_error_init(e_ctx, pool, input_string, source_string_lengths, n_source_strings));
	TIME_PROFILE_STOP("error_init");
	
	TIME_PROFILE_START("typestorage_init");
	ESSL_CHECK(_essl_typestorage_init(ts_ctx, pool));
	TIME_PROFILE_STOP("typestorage_init");

	TIME_PROFILE_START("scanner_init");
	ESSL_CHECK(_essl_scanner_init(&ctx->scan_context, pool, e_ctx, input_string, source_string_lengths, n_source_strings));
	TIME_PROFILE_STOP("scanner_init");
	
	TIME_PROFILE_START("preprocessor_init");
	ESSL_CHECK(_essl_preprocessor_init(&ctx->prep_context, pool, e_ctx, &ctx->scan_context, lang_desc));
	TIME_PROFILE_STOP("preprocessor_init");

	TIME_PROFILE_START("parser_init");
	ESSL_CHECK(_essl_parser_init(&ctx->parse_context, pool, &ctx->prep_context, e_ctx, ts_ctx, desc, lang_desc));
	TIME_PROFILE_STOP("parser_init");

	TIME_PROFILE_START("typecheck_init");
	ESSL_CHECK(_essl_typecheck_init(&ctx->typecheck_context, pool, e_ctx, ts_ctx, desc, lang_desc));
	TIME_PROFILE_STOP("typecheck_init");

	ctx->err_context = e_ctx;
	ctx->typestor_context = ts_ctx;
	ctx->desc = desc;
	ctx->pool = pool;
	ctx->lang_desc = lang_desc;
	return ctx;
}
