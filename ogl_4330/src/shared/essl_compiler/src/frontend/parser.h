/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_PARSER_H
#define COMMON_PARSER_H

#include "common/essl_node.h"
#include "common/essl_mem.h"
#include "frontend/preprocessor.h"
#include "common/symbol_table.h"
#include "common/error_reporting.h"
#include "common/essl_target.h"


/**
	parser.c/parser.h implements a recursive descent parser for the ESSL language.
	Most of the functions in parser.c directly correspond to a rule in the grammar,
	and the rules can be found in the language specification.
*/



typedef struct struct_declaration_stack_level {
	struct struct_declaration_stack_level *prev;
} struct_declaration_stack_level;


/**
   struct that holds the context for the parser
 */
typedef struct {
	mempool *pool; /**< The memory pool to use for allocating new nodes and types */
	preprocessor_context *prep_context; /**< Preprocessor context for retrieving new tokens */
	error_context *err_context; /**< Context for error reporting */
	typestorage_context *typestor_context; /**< Context for caching types */

	Token prev_token;
	string prev_string;
	Token prev_token2;
	string prev_string2;

	scope *global_scope; /**< Pointer to the global scope for the compilation unit */
	scope *current_scope; /**< Pointer to the current scope */
	target_descriptor *target_desc; /**< Target descriptor - used for creating scalars */
	/*@null@*/ node *current_function; /**< The current function we are parsing, used for return matching */
	int current_iteration; /**< The current loop nesting level, used for determining whether break and continue are allowed */
	language_descriptor *lang_desc; /**< Source language descriptor */
	essl_bool have_reported_highp_warning;
	struct_declaration_stack_level *struct_declaration_stack;
} parser_context;


/**
  Initialize a new parsing context.
*/
memerr _essl_parser_init(/*@out@*/ parser_context *ctx, mempool *pool, preprocessor_context *p_ctx, error_context *e_ctx, typestorage_context *ts_ctx, target_descriptor *desc, language_descriptor *lang);


/**
   Parse a translation unit.
   @param ctx The context to use
   @return Zero if a fatal error occurred, otherwise the new root node is returned.
   This function can return a root node when a recoverable parsing error has occurred.
   Query the error context to find out whether there were any errors.
*/
/*@null@*/ node *_essl_parse_translation_unit(parser_context *ctx);

/**
   Parse an expression.
   @param ctx The context to use
   @return Zero if a fatal error occurred, otherwise the new root node is returned.
   This function can return a root node when a recoverable parsing error has occurred.
   Query the error context to find out whether there were any errors.
*/
/*@null@*/ node *_essl_parse_expression(parser_context *ctx);

#endif /* COMMON_PARSER_H */
