/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef MALIGP2_RELOCATION_H
#define MALIGP2_RELOCATION_H

#include "common/essl_symbol.h"
#include "common/essl_list.h"
#include "common/translation_unit.h"
#include "maligp2/maligp2_instruction.h"
#include "common/compiler_options.h"

struct interference_graph_context;

typedef enum {
	MALIGP2_RELOC_ADDRESS_OFFSET,
	MALIGP2_RELOC_CONSTANT,
	MALIGP2_RELOC_SYMBOL_ADDRESS
} maligp2_relocation_target;

typedef struct _tag_maligp2_relocation
{
	struct _tag_maligp2_relocation *next;
	const symbol *source_symbol;
	maligp2_relocation_target target;
	union {
		/*@null@*/ maligp2_instruction *instr;
		symbol *dest_symbol;
		struct {
			/*@null@*/ node *dest_constant;
			unsigned index;
			int subcycle; /* used for bug workarounds that need to identify constant register stores */
		} constant;
	} u;

} maligp2_relocation;
ASSUME_ALIASING(generic_list, maligp2_relocation);


typedef struct _tag_maligp2_relocation_context
{
	/*@null@*/ maligp2_relocation *symbol_relocations;
	/*@null@*/ maligp2_relocation *normal_relocations;

	translation_unit *tu;
	mempool *pool;
	error_context *err_ctx;
	compiler_options *opts;
} maligp2_relocation_context;



memerr _essl_maligp2_add_address_offset_relocation(maligp2_relocation_context *ctx,  symbol *source_symbol, maligp2_instruction *instr);


/*memerr _essl_maligp2_add_symbol_address_relocation(maligp2_relocation_context *ctx, symbol *source_symbol, symbol *dest_symbol);*/

memerr _essl_maligp2_add_constant_relocation(maligp2_relocation_context *ctx,  symbol *source_symbol, node *dest_node, unsigned index, int subcycle);

maligp2_relocation *_essl_maligp2_get_normal_relocations(maligp2_relocation_context *ctx);

memerr _essl_maligp2_relocations_resolve(maligp2_relocation_context *ctx, struct interference_graph_context *constant_register_interference);

memerr _essl_maligp2_relocations_init(/*@out@*/ maligp2_relocation_context *ctx, mempool *pool, translation_unit *tu, error_context *err_ctx, compiler_options *opts);

#endif /* MALIGP2_RELOCATION_H */

