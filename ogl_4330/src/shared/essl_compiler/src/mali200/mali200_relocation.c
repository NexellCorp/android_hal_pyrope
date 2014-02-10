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
#include "common/symbol_table.h"
#include "mali200/mali200_relocation.h"
#include "mali200/mali200_external.h"
#include "backend/address_allocator.h"
#include "backend/arithmetic.h"

memerr _essl_mali200_add_address_offset_relocation(mali200_relocation_context *ctx, mali200_relocation_source source, const symbol *source_symbol, const symbol *function_symbol, int multiplier, int divisor, m200_instruction *instr)
{
	mali200_relocation *reloc;
	ESSL_CHECK(reloc = LIST_NEW(ctx->pool, mali200_relocation));
	reloc->source = source;
	reloc->source_symbol = source_symbol;
	reloc->multiplier = multiplier;
	reloc->divisor = divisor;
	reloc->function_symbol = function_symbol;

	reloc->target = M200_RELOC_ADDRESS_OFFSET;
	reloc->u.instr = instr;
	LIST_INSERT_FRONT(&ctx->normal_relocations, reloc);
	return MEM_OK;

}

memerr _essl_mali200_add_symbol_address_relocation(mali200_relocation_context *ctx, mali200_relocation_source source, const symbol *source_symbol, const symbol *function_symbol, int divisor, symbol *dest_symbol)
{

	mali200_relocation *reloc;
	ESSL_CHECK(reloc = LIST_NEW(ctx->pool, mali200_relocation));
	reloc->source = source;
	reloc->source_symbol = source_symbol;
	reloc->multiplier = 1;
	reloc->divisor = divisor;
	reloc->function_symbol = function_symbol;

	reloc->target = M200_RELOC_SYMBOL_ADDRESS;
	reloc->u.dest_symbol = dest_symbol;
	LIST_INSERT_FRONT(&ctx->symbol_relocations, reloc);
	return MEM_OK;


}

essl_bool _essl_mali200_same_address(mali200_relocation_context *ctx, m200_instruction *inst1, m200_instruction *inst2) {
	const symbol *sym1 = 0, *sym2 = 0;
	int div1 = 0, div2 = 0;
	int mul1 = 0, mul2 = 0;
	mali200_relocation *reloc;
	for (reloc = ctx->normal_relocations ; reloc != 0 ; reloc = reloc->next) {
		if (reloc->u.instr == inst1) {
			sym1 = reloc->source_symbol;
			div1 = reloc->divisor;
			mul1 = reloc->multiplier;
			if (sym2) break;
		}
		if (reloc->u.instr == inst2) {
			sym2 = reloc->source_symbol;
			div2 = reloc->divisor;
			mul2 = reloc->multiplier;
			if (sym1) break;
		}
	}
	assert(sym1 && sym2);
	return (essl_bool)(sym1 == sym2 && div1 == div2 && mul1 == mul2 && inst1->address_offset == inst2->address_offset);
}




memerr _essl_mali200_relocations_init(/*@out@*/ mali200_relocation_context *ctx, mempool *pool, translation_unit *tu, error_context *err_ctx)
{
	ctx->tu = tu;
	ctx->pool = pool;
	ctx->normal_relocations = 0;
	ctx->symbol_relocations = 0;
	ctx->err_ctx = err_ctx;
	ESSL_CHECK(_essl_ptrdict_init(&ctx->external_symbols_fixup, pool));

	return MEM_OK;
}


static memerr resolve_relocations(mali200_relocation_context *ctx, mali200_relocation *reloc)
{
	IGNORE_PARAM(ctx);
	for(; reloc != 0; reloc = reloc->next)
	{
		int fixup = 0;
		switch(reloc->source)
		{
		case M200_RELOC_UNKNOWN:
			assert(0);
			break;
		case M200_RELOC_VARIABLE:
			assert(ctx->tu->desc->get_type_alignment(ctx->tu->desc, reloc->source_symbol->type, reloc->source_symbol->address_space) * reloc->multiplier >= (reloc->divisor < 0 ? (unsigned)-reloc->divisor : (unsigned)reloc->divisor));
			assert((reloc->source_symbol->address * reloc->multiplier) % reloc->divisor == 0);
			fixup = reloc->source_symbol->address * reloc->multiplier;
			break;
		case M200_RELOC_FUNCTION_STACK_SIZE:
			assert(reloc->source_symbol->control_flow_graph->stack_frame_size % reloc->divisor == 0);
			fixup = reloc->source_symbol->control_flow_graph->stack_frame_size;
			break;

		}
		switch(reloc->target)
		{
		case M200_RELOC_ADDRESS_OFFSET:
			reloc->u.instr->address_offset += fixup;
			break;
		case M200_RELOC_SYMBOL_ADDRESS:
			reloc->u.dest_symbol->address += fixup;
			break;
		}


	}
	return MEM_OK;
}

static int compare_symbols_by_address( generic_list *va, generic_list *vb)
{
	symbol *a = (symbol*)va;
	symbol *b = (symbol*)vb;

	return a->address - b->address;
}

static void remove_from_func_list(symbol_list **list, symbol *sym)
{
	/* Find the CDO on the graph */
	if((*list) != 0 && (*list)->sym == sym)
		(*list) = (*list)->next;
	else
	{
		while((*list) != 0 && (*list)->next != 0 && (*list)->next->sym != sym)	list = &((*list)->next);
		assert((*list) != 0 && (*list)->next->sym == sym);
		LIST_REMOVE(&((*list)->next));
	}

	return;
}

static int compare_proactive_uniforms(generic_list *a, generic_list *b)
{
	symbol_list *sl1 = (symbol_list*)a;
	symbol_list *sl2 = (symbol_list*)b;
	return sl1->sym->opt.pilot.proactive_uniform_num - sl2->sym->opt.pilot.proactive_uniform_num;
}

static memerr allocate_addresses_for_proactive_uniforms(mali200_relocation_context *ctx, ptrset *proactive_uniforms, unsigned general_uniforms_end)
{
	int uniforms_end_pow2;
	unsigned i;
	ptrset_iter it;
	symbol_list *sl, *prev_sl, *sl_n;
	symbol *s;
	ptrset only_1_proactive_uniform;
	ESSL_CHECK(_essl_ptrset_init(&only_1_proactive_uniform, ctx->pool));
	assert(ctx->tu->proactive_funcs != NULL);
	if(_essl_ptrset_size(proactive_uniforms) != 0)
	{
		/* uniforms_end actually means the first possible address for the next set of uniforms  */
		unsigned row_num = 0;
		unsigned row_num_pow2;
		unsigned i;
		symbol_list *first = NULL;
		symbol_list *uni_list = NULL;


		if(general_uniforms_end > 0)
		{
			row_num = (general_uniforms_end - 1) / 4; /* number of complete uniform rows (registers) used */
		}
		/* Mali-400 requires that proactive uniforms addresses should be a power of 2 value > row_num */
		if(row_num == 0)
		{
			row_num_pow2 = 1; 
		}else
		{
			unsigned clz = 32 - _essl_clz32(row_num);
			row_num_pow2 = 1 << clz;
		}
		ctx->tu->proactive_uniform_init_addr = row_num_pow2;

		_essl_ptrset_iter_init(&it, proactive_uniforms);
		while((s = _essl_ptrset_next(&it)) != NULL)
		{
			ESSL_CHECK(uni_list = LIST_NEW(ctx->pool, symbol_list));
			uni_list->sym = s;
			LIST_INSERT_FRONT(&first, uni_list);
		}
		first = LIST_SORT(first, compare_proactive_uniforms, symbol_list);

		i = 1;
		for(sl = first; sl != NULL; sl = sl->next)
		{
			s = sl->sym;
			ESSL_CHECK(_essl_ptrset_insert(&only_1_proactive_uniform, (void *)s));
			uniforms_end_pow2 = row_num_pow2 * i * 4;
			ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, uniforms_end_pow2, ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS, &only_1_proactive_uniform, 0, NULL));
			i++;
			ESSL_CHECK(_essl_ptrset_clear(&only_1_proactive_uniform));		
		}
	}
	prev_sl = NULL;
	for(i = 0, sl = ctx->tu->proactive_funcs; sl != NULL; sl = sl_n, i++)
	{
		sl_n = sl->next;
		_essl_ptrset_iter_init(&it, proactive_uniforms);
		while((s = _essl_ptrset_next(&it)) != NULL)
		{
			if(s->opt.pilot.proactive_uniform_num == (int)i)
			{
				break;
			}
		}
		if(s == NULL) /* proactive_uniform related to the function was removed, should remove the function too */
		{
			if(prev_sl != NULL)
			{
				prev_sl->next = sl_n;
			}
			if(sl == ctx->tu->proactive_funcs)
			{
				ctx->tu->proactive_funcs = sl_n;
			}
			remove_from_func_list(&ctx->tu->functions, sl->sym);
		}else
		{
			prev_sl = sl;
		}
	}
	
	return MEM_OK;
}
static memerr allocate_addresses(mali200_relocation_context *ctx)
{
	ptrset optimized_sampler_uniforms;
	ptrset optimized_external_sampler_uniforms;
	ptrset unoptimized_external_sampler_uniforms;
	ptrset uniforms;
	ptrset proactive_uniforms;
	ptrset fragment_varyings;
	ptrset globals;
	ptrdict functions;
	size_t i;
	int global_size;
	int optimized_samplers_end;
	int optimized_external_samplers_end;
	int unoptimized_external_samplers_end;
	int unoptimized_external_samplers_end_aligned;
	int optimized_external_samplers_end_aligned;
	int uniforms_end;
	mali200_relocation *reloc;
	ptrset *func_vars;
	ESSL_CHECK(_essl_ptrset_init(&optimized_sampler_uniforms, ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&optimized_external_sampler_uniforms, ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&unoptimized_external_sampler_uniforms, ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&uniforms, ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&fragment_varyings, ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&globals, ctx->pool));
	ESSL_CHECK(_essl_ptrdict_init(&functions, ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&proactive_uniforms, ctx->pool));


	for(i = 0; i < 2; ++i)
	{
		reloc = (i == 0) ? ctx->symbol_relocations : ctx->normal_relocations;
		for( ; reloc != 0; reloc = reloc->next)
		{
			if(reloc->source == M200_RELOC_FUNCTION_STACK_SIZE)
			{
				/* make sure the function has its stack frame calculated */
				func_vars = _essl_ptrdict_lookup(&functions, (void *)reloc->source_symbol);
				if(!func_vars)
				{
					ESSL_CHECK(func_vars = _essl_mempool_alloc(ctx->pool, sizeof(ptrset)));
					ESSL_CHECK(_essl_ptrset_init(func_vars, ctx->pool));
					ESSL_CHECK(_essl_ptrdict_insert(&functions, (void *)reloc->source_symbol, func_vars));
				}

			}

			if(reloc->source_symbol->kind == SYM_KIND_VARIABLE)
			{
				if(reloc->source_symbol->address >= 0) continue; /* the symbol already has an address, skip it */
				switch(reloc->source_symbol->address_space)
				{
				  case ADDRESS_SPACE_THREAD_LOCAL:
					if (SCOPE_ARGUMENT != (scope_kind)reloc->source_symbol->scope)
					{

						func_vars = _essl_ptrdict_lookup(&functions, (void *)reloc->function_symbol);
						if(!func_vars)
						{
							ESSL_CHECK(func_vars = _essl_mempool_alloc(ctx->pool, sizeof(ptrset)));
							ESSL_CHECK(_essl_ptrset_init(func_vars, ctx->pool));
							ESSL_CHECK(_essl_ptrdict_insert(&functions, (void *)reloc->function_symbol, func_vars));
						}
						ESSL_CHECK(_essl_ptrset_insert(func_vars, (void *)reloc->source_symbol));
					}
					/* else calculated from formal + stack_frame_size */
					break;

				  case ADDRESS_SPACE_GLOBAL:
				  case ADDRESS_SPACE_FRAGMENT_OUT:
					ESSL_CHECK(_essl_ptrset_insert(&globals, (void *)reloc->source_symbol));

					break;

				  case ADDRESS_SPACE_FRAGMENT_VARYING:
					ESSL_CHECK(_essl_ptrset_insert(&fragment_varyings, (void *)reloc->source_symbol));
					break;

				  case ADDRESS_SPACE_UNIFORM:
					{
						essl_bool is_external_sampler = (_essl_get_specified_samplers_num(reloc->source_symbol->type, TYPE_SAMPLER_EXTERNAL) != 0);
						if(_essl_is_optimized_sampler_symbol(reloc->source_symbol))
						{
							if(is_external_sampler)
							{
								ESSL_CHECK(_essl_ptrset_insert(&optimized_external_sampler_uniforms, (void *)reloc->source_symbol));
							} else {
								ESSL_CHECK(_essl_ptrset_insert(&optimized_sampler_uniforms, (void *)reloc->source_symbol));
							}

						} else {

							if(is_external_sampler)
							{
								ESSL_CHECK(_essl_ptrset_insert(&unoptimized_external_sampler_uniforms, (void *)reloc->source_symbol));
							} else {
								if(reloc->source_symbol->opt.pilot.proactive_uniform_num > -1)
								{
									ESSL_CHECK(_essl_ptrset_insert(&proactive_uniforms, (void *)reloc->source_symbol));
								}else
								{
									ESSL_CHECK(_essl_ptrset_insert(&uniforms, (void *)reloc->source_symbol));
								}
							}
						}

					}
					break;

				  case ADDRESS_SPACE_FRAGMENT_SPECIAL:
					break; /* this is okay */

				  case ADDRESS_SPACE_UNKNOWN:
				  case ADDRESS_SPACE_ATTRIBUTE:
				  case ADDRESS_SPACE_VERTEX_VARYING:
					assert(0);
					break;



				}

			}

		}

	}
	/* uniform samplers and regular uniforms are the same namespace, but samplers have to be in the beginning as the address is the sampler index for standalone samplers */
	ESSL_CHECK(_essl_allocate_addresses_for_optimized_samplers(ctx->pool, ctx->tu->desc, 0, &optimized_sampler_uniforms, &optimized_samplers_end));
	ESSL_CHECK(_essl_allocate_addresses_for_optimized_samplers(ctx->pool, ctx->tu->desc, optimized_samplers_end, &optimized_external_sampler_uniforms, &optimized_external_samplers_end));
	optimized_external_samplers_end_aligned = (optimized_external_samplers_end+3)/4*4;
	ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, optimized_external_samplers_end_aligned, ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS, &unoptimized_external_sampler_uniforms, &unoptimized_external_samplers_end, NULL));
	unoptimized_external_samplers_end_aligned = (unoptimized_external_samplers_end+3)/4*4;

	/* fixup EXTERNAL symbols */
	if(ctx->tu->root != NULL)
	{
		scope *global_scope = ctx->tu->root->stmt.child_scope;
		symbol *negative_external_sampler_start;

		negative_external_sampler_start = _essl_symbol_table_lookup(global_scope, _essl_cstring_to_string_nocopy(NEGATIVE_EXTERNAL_SAMPLER_START_NAME));

		if(negative_external_sampler_start != NULL)
		{
			/* fill in address */
			negative_external_sampler_start->address = -optimized_samplers_end;
		}

	}

	ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, unoptimized_external_samplers_end_aligned, ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS, &uniforms, &uniforms_end, NULL));
	if(ctx->tu->proactive_funcs != NULL)
	{
		ESSL_CHECK(allocate_addresses_for_proactive_uniforms(ctx, &proactive_uniforms, uniforms_end));
	}


	ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, 0, ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS, &globals, &global_size, NULL));
	{
		int n_varyings = -1;
		ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, 0, M200_N_VARYINGS*M200_NATIVE_VECTOR_SIZE, &fragment_varyings, &n_varyings, NULL));
		if(n_varyings > M200_N_VARYINGS*M200_NATIVE_VECTOR_SIZE)
		{
			REPORT_ERROR(ctx->err_ctx, ERR_LINK_TOO_MANY_VARYINGS, 0, "Out of varying space. %s provides space for %d varying vec4s, this shader uses %d varying vec4s.\n",  _essl_mali_core_name(ctx->tu->desc->core), M200_N_VARYINGS, (n_varyings+3)/4);
			return MEM_ERROR;
		}
	}
	
	{
		symbol_list *sl;
		for(sl = ctx->tu->functions; sl != 0; sl = sl->next)
		{
			sl->sym->control_flow_graph->stack_frame_size = M200_NATIVE_VECTOR_SIZE;
		}

	}
	{
		ptrdict_iter it;
		symbol *fun;
		_essl_ptrdict_iter_init(&it, &functions);
		while((fun = _essl_ptrdict_next(&it, (void_ptr *)(void*)&func_vars)) != 0)
		{
			int stack_frame_size = 0;
			ptrset_iter set_it;
			symbol *s;
			/* the return address of size M200_NATIVE_VECTOR_SIZE goes first */
			ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, M200_NATIVE_VECTOR_SIZE, ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS, func_vars, &stack_frame_size, NULL));
			/* align stack frame size to M200_NATIVE_VECTOR_SIZE */
			stack_frame_size = (stack_frame_size + M200_NATIVE_VECTOR_SIZE-1) & ~(M200_NATIVE_VECTOR_SIZE-1);
			fun->control_flow_graph->stack_frame_size = stack_frame_size;
			assert(stack_frame_size % 4 == 0);
			_essl_ptrset_iter_init(&set_it, func_vars);
			while((s = _essl_ptrset_next(&set_it)) != 0)
			{
				s->address -= stack_frame_size;

			}

		}
	}

	{
		symbol *first = NULL;
		symbol **prev = &first;
		symbol *s;
		ptrset_iter it;
		symbol *external_sym_fixup;
		unsigned i;
		_essl_ptrset_iter_init(&it, &optimized_external_sampler_uniforms);
		while((s = _essl_ptrset_next(&it)) != NULL)
		{
			s->next = NULL;
			*prev = s;
			prev = &s->next;

		}

		/* sort in by symbol addresses */
		i = 0;
		first = LIST_SORT(first, compare_symbols_by_address, symbol);
		for(s = first; s != NULL; /*empty*/)
		{
			external_sym_fixup = _essl_ptrdict_lookup(&ctx->external_symbols_fixup, s);
			if(external_sym_fixup != NULL)
			{
				external_sym_fixup->address = i;
				i++;
			}
			s = s->next;
		}
	}

	{
		symbol *first = NULL;
		symbol **prev = &first;
		symbol *s;
		ptrset_iter it;
		unsigned i;
		_essl_ptrset_iter_init(&it, &optimized_external_sampler_uniforms);
		while((s = _essl_ptrset_next(&it)) != NULL)
		{
			s->next = NULL;
			*prev = s;
			prev = &s->next;

		}

		_essl_ptrset_iter_init(&it, &unoptimized_external_sampler_uniforms);
		while((s = _essl_ptrset_next(&it)) != NULL)
		{
			s->next = NULL;
			*prev = s;
			prev = &s->next;

		}

		/* sort in by symbol addresses */
		i = 0;
		first = LIST_SORT(first, compare_symbols_by_address, symbol);
		for(s = first; s != NULL; /*empty*/)
		{
			s->opt.external_index = i;
			i += _essl_get_specified_samplers_num(s->type, TYPE_SAMPLER_EXTERNAL) * 4;
			s = s->next;
		}


	}


	{
		symbol *first = NULL;
		symbol **prev = &first;
		symbol *s;
		ptrset_iter it;
		unsigned i;
		_essl_ptrset_iter_init(&it, &optimized_sampler_uniforms);
		while((s = _essl_ptrset_next(&it)) != NULL)
		{
			s->next = NULL;
			*prev = s;
			prev = &s->next;

		}

		_essl_ptrset_iter_init(&it, &uniforms);
		while((s = _essl_ptrset_next(&it)) != NULL)
		{
			s->next = NULL;
			*prev = s;
			prev = &s->next;

		}

		/* sort in by symbol addresses */
		i = 0;
		first = LIST_SORT(first, compare_symbols_by_address, symbol);
		for(s = first; s != NULL; /*empty*/)
		{
			/* only sampler2D and samplerCube variables can be used in textureGrad functions */
			unsigned grad_samplers_num = _essl_get_specified_samplers_num(s->type, TYPE_SAMPLER_2D) +  _essl_get_specified_samplers_num(s->type, TYPE_SAMPLER_CUBE);
			if(grad_samplers_num > 0)
			{
				s->opt.grad_index = i;
				i += grad_samplers_num;
			}
			s = s->next;
		}


	}

	/* We do not have function calls, so the stack size is just the stack size of the entry point. */
	{
		unsigned entry_point_stack_size;
		if (ctx->tu->desc->has_entry_point)
		{
			entry_point_stack_size = ctx->tu->entry_point->control_flow_graph->stack_frame_size;
		} else {
			entry_point_stack_size = 0;
		}
		ctx->tu->stack_flags.initial_stack_pointer = (unsigned)(global_size + entry_point_stack_size)/M200_NATIVE_VECTOR_SIZE;
		ctx->tu->stack_flags.stack_size =  (unsigned)(global_size + entry_point_stack_size)/M200_NATIVE_VECTOR_SIZE;
	}
	return MEM_OK;
}


memerr _essl_mali200_relocations_resolve(mali200_relocation_context *ctx)
{
	ESSL_CHECK_NONFATAL(allocate_addresses(ctx));
	ESSL_CHECK(resolve_relocations(ctx, ctx->symbol_relocations));
	ESSL_CHECK(resolve_relocations(ctx, ctx->normal_relocations));

	return MEM_OK;
}
