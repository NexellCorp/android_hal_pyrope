/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef LIR_PRINTER_H
#define LIR_PRINTER_H

#ifndef NEEDS_STDIO
#define NEEDS_STDIO
#endif

#include "common/essl_system.h"
#include "common/essl_mem.h"
#include "common/essl_symbol.h"
#include "common/translation_unit.h"
#include "common/ptrset.h"
#include "common/unique_names.h"
#include "common/basic_block.h"
#include "common/essl_stringbuffer.h"

typedef struct
{
	mempool *pool;
	string_buffer *strbuf;
	ptrset visited;
	unique_name_context node_names;
	symbol *function;
	target_descriptor *desc;
} lir_printer_context;

memerr lir_translation_unit_to_txt(FILE *out, mempool *pool, translation_unit *tu, target_descriptor *desc);
memerr ast_translation_unit_to_txt(FILE *out, mempool *pool, translation_unit *tu, target_descriptor *desc);
memerr lir_function_to_txt(FILE *out, mempool *pool, symbol *function, target_descriptor *desc);

memerr _essl_lir_basic_block_to_txt(lir_printer_context *ctx, basic_block *bb);
#endif /* LIR_PRINTER_H */
