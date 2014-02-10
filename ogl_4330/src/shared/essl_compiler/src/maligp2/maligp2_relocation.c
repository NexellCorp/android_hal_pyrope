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
#include "maligp2/maligp2_relocation.h"
#include "backend/address_allocator.h"

memerr _essl_maligp2_add_address_offset_relocation(maligp2_relocation_context *ctx, symbol *source_symbol, maligp2_instruction *instr)
{
	maligp2_relocation *reloc;
	ESSL_CHECK(reloc = LIST_NEW(ctx->pool, maligp2_relocation));
	reloc->source_symbol = source_symbol;

	reloc->target = MALIGP2_RELOC_ADDRESS_OFFSET;
	reloc->u.instr = instr;
	LIST_INSERT_FRONT(&ctx->normal_relocations, reloc);
	source_symbol->is_used_by_machine_instructions = ESSL_TRUE;
	return MEM_OK;

}

#if 0 /* Only needed for function call support */
memerr _essl_maligp2_add_symbol_address_relocation(maligp2_relocation_context *ctx, const symbol *source_symbol, symbol *dest_symbol)
{

	maligp2_relocation *reloc;
	ESSL_CHECK(reloc = LIST_NEW(ctx->pool, maligp2_relocation));
	reloc->source_symbol = source_symbol;

	reloc->target = MALIGP2_RELOC_SYMBOL_ADDRESS;
	reloc->u.dest_symbol = dest_symbol;
	LIST_INSERT_FRONT(&ctx->symbol_relocations, reloc);
	return MEM_OK;
}
#endif

memerr _essl_maligp2_add_constant_relocation(maligp2_relocation_context *ctx, symbol *source_symbol, node *dest_constant, unsigned index, int subcycle)
{
	maligp2_relocation *reloc;
	assert(index < 4);

	ESSL_CHECK(reloc = LIST_NEW(ctx->pool, maligp2_relocation));
	reloc->source_symbol = source_symbol;

	reloc->target = MALIGP2_RELOC_CONSTANT;
	reloc->u.constant.dest_constant = dest_constant;
	reloc->u.constant.index = index;
	reloc->u.constant.subcycle = subcycle;
	LIST_INSERT_FRONT(&ctx->normal_relocations, reloc);
	source_symbol->is_used_by_machine_instructions = ESSL_TRUE;
	return MEM_OK;

}


memerr _essl_maligp2_relocations_init(/*@out@*/ maligp2_relocation_context *ctx, mempool *pool, translation_unit *tu, error_context *err_ctx, compiler_options *opts)
{
	ctx->tu = tu;
	ctx->pool = pool;
	ctx->normal_relocations = 0;
	ctx->symbol_relocations = 0;
	ctx->err_ctx = err_ctx;
	ctx->opts = opts;
	return MEM_OK;
}


static memerr resolve_relocations(maligp2_relocation_context *ctx, maligp2_relocation *reloc)
{
	for(; reloc != 0; reloc = reloc->next)
	{
		int tmp;
		switch(reloc->target)
		{
		case MALIGP2_RELOC_ADDRESS_OFFSET:
		{
			reloc->u.instr->address_offset += reloc->source_symbol->address;
		}
		break;
		case MALIGP2_RELOC_CONSTANT:
			tmp = ctx->tu->desc->scalar_to_int(reloc->u.constant.dest_constant->expr.u.value[reloc->u.constant.index]);
			tmp += reloc->source_symbol->address;
			reloc->u.constant.dest_constant->expr.u.value[reloc->u.constant.index] = ctx->tu->desc->int_to_scalar(tmp/4);
			break;
		case MALIGP2_RELOC_SYMBOL_ADDRESS:
			reloc->u.dest_symbol->address += reloc->source_symbol->address;
			break;
		}


	}
	return MEM_OK;
}

static int compare_by_address(generic_list *va, generic_list *vb)
{
	symbol_list *al = (symbol_list*)va;
	symbol_list *bl = (symbol_list*)vb;
	symbol *a = al->sym;
	symbol *b = bl->sym;
	int address_a;
	int address_b;
	assert(a != NULL);
	assert(b != NULL);
	address_a = a->address;
	address_b = b->address;
	return address_a - address_b;
}


static memerr allocate_addresses(maligp2_relocation_context *ctx, struct interference_graph_context *interference)
{
	ptrset vertex_varyings;
	ptrset vertex_attributes;
	ptrset uniforms;
	ptrset globals;
	size_t i;
	maligp2_relocation *reloc;
	symbol_list *sl;
	symbol *sym;
	ESSL_CHECK(_essl_ptrset_init(&vertex_varyings, ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&vertex_attributes, ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&uniforms, ctx->pool));
	ESSL_CHECK(_essl_ptrset_init(&globals, ctx->pool));

	for(sl = ctx->tu->vertex_varyings; sl != NULL; sl = sl->next)
	{
		sym = sl->sym;
		if(sym->is_used_by_machine_instructions || sym->keep_symbol)
		{
			ESSL_CHECK(_essl_ptrset_insert(&vertex_varyings, (void *)sym));
		}
	}
	for(sl = ctx->tu->vertex_attributes; sl != NULL; sl = sl->next)
	{
		sym = sl->sym;
		if(sym->is_used_by_machine_instructions || sym->keep_symbol)
		{
			ESSL_CHECK(_essl_ptrset_insert(&vertex_attributes, (void *)sym));
		}
	}
	for(sl = ctx->tu->globals; sl != NULL; sl = sl->next)
	{
		sym = sl->sym;
		if(sym->is_used_by_machine_instructions || sym->keep_symbol)
		{
			if (!sym->is_persistent_variable)
			{
				ESSL_CHECK(_essl_ptrset_insert(&globals, (void *)sym));
				break;
			}else
			{
				/* Treat persistent variables as uniforms */
				ESSL_CHECK(_essl_ptrset_insert(&uniforms, (void *)sym));
			}
		}
	}
	for(sl = ctx->tu->uniforms; sl != NULL; sl = sl->next)
	{
		sym = sl->sym;
		if(sym->is_used_by_machine_instructions || sym->keep_symbol)
		{
			ESSL_CHECK(_essl_ptrset_insert(&uniforms, (void *)sym));
		}
	}
	for(i = 0; i < 2; ++i)
	{
		reloc = (i == 0) ? ctx->symbol_relocations : ctx->normal_relocations;
		for( ; reloc != 0; reloc = reloc->next)
		{

			if(reloc->source_symbol->kind == SYM_KIND_VARIABLE)
			{
				
				switch(reloc->source_symbol->address_space)
				{
				case ADDRESS_SPACE_THREAD_LOCAL:
					if((scope_kind)reloc->source_symbol->scope == SCOPE_ARGUMENT)
					{
						/* calculated from formal + stack_frame_size */
						break;
					}
					/*else fallthrough*/
				case ADDRESS_SPACE_GLOBAL:
					if (!reloc->source_symbol->is_persistent_variable)
					{
						ESSL_CHECK(_essl_ptrset_insert(&globals, (void *)reloc->source_symbol));
						break;
					}
					/* Treat persistent variables as uniforms */

				case ADDRESS_SPACE_UNIFORM:
					ESSL_CHECK(_essl_ptrset_insert(&uniforms, (void *)reloc->source_symbol));
					break;

				case ADDRESS_SPACE_VERTEX_VARYING:
					ESSL_CHECK(_essl_ptrset_insert(&vertex_varyings, (void *)reloc->source_symbol));
					break;

				case ADDRESS_SPACE_ATTRIBUTE:
					ESSL_CHECK(_essl_ptrset_insert(&vertex_attributes, (void *)reloc->source_symbol));
					break;

				case ADDRESS_SPACE_UNKNOWN:
				case ADDRESS_SPACE_FRAGMENT_VARYING:
				case ADDRESS_SPACE_FRAGMENT_SPECIAL:
				case ADDRESS_SPACE_FRAGMENT_OUT:
					assert(0);
					break;



				}

			}

		}

	}

	{
		int n_uniforms = -1;
		int n_total_globals = -1;
		ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, 0, ctx->opts->n_maligp2_constant_registers*MALIGP2_NATIVE_VECTOR_SIZE, &uniforms, &n_uniforms, interference));
		ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, n_uniforms, ctx->opts->n_maligp2_constant_registers*MALIGP2_NATIVE_VECTOR_SIZE, &globals, &n_total_globals, interference));
		if (n_total_globals > ctx->opts->n_maligp2_constant_registers*MALIGP2_NATIVE_VECTOR_SIZE)
		{
			REPORT_ERROR(ctx->err_ctx, ERR_LINK_TOO_MANY_UNIFORMS, 0, "Out of space for uniforms, globals and temporary variables. %s provides space for %d vec4s, this shader uses %d vec4s.\n", _essl_mali_core_name(ctx->tu->desc->core), ctx->opts->n_maligp2_constant_registers, (n_total_globals+3)/4);
			return MEM_ERROR;
		}
	}

	
	{
		int n_attributes = -1;
		ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, 0, MALIGP2_NUM_INPUT_REGS*MALIGP2_NATIVE_VECTOR_SIZE, &vertex_attributes, &n_attributes, NULL));
		if (n_attributes > MALIGP2_NUM_INPUT_REGS*MALIGP2_NATIVE_VECTOR_SIZE)
		{
			REPORT_ERROR(ctx->err_ctx, ERR_LINK_TOO_MANY_ATTRIBUTES, 0, "Out of attribute space. %s provides space for %d attribute vec4s, this shader uses %d attribute vec4s.\n", _essl_mali_core_name(ctx->tu->desc->core), MALIGP2_NUM_INPUT_REGS, (n_attributes+3)/4);
			return MEM_ERROR;
		}
	}
	
	{
		int n_varyings = -1;
		ESSL_CHECK(_essl_allocate_addresses_for_set(ctx->pool, ctx->tu->desc, 0, MALIGP2_NUM_OUTPUT_REGS*MALIGP2_NATIVE_VECTOR_SIZE, &vertex_varyings, &n_varyings, NULL));
		if (n_varyings > MALIGP2_NUM_OUTPUT_REGS*MALIGP2_NATIVE_VECTOR_SIZE)
		{
			REPORT_ERROR(ctx->err_ctx, ERR_LINK_TOO_MANY_VARYINGS, 0, "Out of varying space. %s provides space for %d varying vec4s, this shader uses %d varying vec4s.\n",  _essl_mali_core_name(ctx->tu->desc->core), MALIGP2_NUM_OUTPUT_REGS, (n_varyings+3)/4);
			return MEM_ERROR;
		}
	}

	/* Driver issue 1909. The reamp algorithm expects 
	 * that attributes will come sorted according to the allocated addresses. 
	 * But it is not guaranteed anywwhere in the compiler */
	ctx->tu->vertex_attributes = LIST_SORT(ctx->tu->vertex_attributes, compare_by_address, symbol_list);

	return MEM_OK;
}


maligp2_relocation *_essl_maligp2_get_normal_relocations(maligp2_relocation_context *ctx)
{
	return ctx->normal_relocations;
}

memerr _essl_maligp2_relocations_resolve(maligp2_relocation_context *ctx, struct interference_graph_context *constant_register_interference)
{
	ESSL_CHECK_NONFATAL(allocate_addresses(ctx, constant_register_interference));
	ESSL_CHECK(resolve_relocations(ctx, ctx->symbol_relocations));
	ESSL_CHECK(resolve_relocations(ctx, ctx->normal_relocations));

	return MEM_OK;
}
