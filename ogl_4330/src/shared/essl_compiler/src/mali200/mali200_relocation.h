/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef MALI200_RELOCATION_H
#define MALI200_RELOCATION_H

#include "common/essl_symbol.h"
#include "common/essl_list.h"
#include "common/translation_unit.h"
#include "mali200/mali200_instruction.h"


typedef enum {
	M200_RELOC_UNKNOWN,
	M200_RELOC_VARIABLE,
	M200_RELOC_FUNCTION_STACK_SIZE

} mali200_relocation_source;


typedef enum {
	M200_RELOC_ADDRESS_OFFSET,
	M200_RELOC_SYMBOL_ADDRESS
} mali200_relocation_target;

typedef struct _tag_mali200_relocation
{
	struct _tag_mali200_relocation *next;
	mali200_relocation_source source;
	const symbol *source_symbol;
	int multiplier;
	int divisor;
	const symbol *function_symbol;
	mali200_relocation_target target;
	union {
		/*@null@*/ m200_instruction *instr;
		symbol *dest_symbol;
	} u;

} mali200_relocation;
ASSUME_ALIASING(generic_list, mali200_relocation);


typedef struct _tag_mali200_relocation_context
{
	/*@null@*/ mali200_relocation *symbol_relocations;
	/*@null@*/ mali200_relocation *normal_relocations;

	translation_unit *tu;
	mempool *pool;
	error_context *err_ctx;
	ptrdict external_symbols_fixup;
} mali200_relocation_context;



memerr _essl_mali200_add_address_offset_relocation(mali200_relocation_context *ctx, mali200_relocation_source source, const symbol *sym, const symbol *function_symbol, int multiplier, int divisor, m200_instruction *instr);

memerr _essl_mali200_add_symbol_address_relocation(mali200_relocation_context *ctx, mali200_relocation_source source, const symbol *sym, const symbol *function_symbol, int divisor, symbol *dest_symbol);

memerr _essl_mali200_relocations_resolve(mali200_relocation_context *ctx);

memerr _essl_mali200_relocations_init(/*@out@*/ mali200_relocation_context *ctx, mempool *pool, translation_unit *tu, error_context *err_ctx);

essl_bool _essl_mali200_same_address(mali200_relocation_context *ctx, m200_instruction *inst1, m200_instruction *inst2);

#endif /* MALI200_RELOCATION_H */

