/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef FRONTEND_FRONTEND_H
#define FRONTEND_FRONTEND_H

#include "frontend/scanner.h"
#include "frontend/parser.h"
#include "frontend/typecheck.h"
#include "frontend/lang.h"
#include "common/translation_unit.h"

/**
   The frontend is a facade that creates a single context for scanning, preprocessing and parsing.
*/

typedef struct {
	mempool *pool; /**< memory pool for various needs */
	scanner_context scan_context; /**< scanner context */
	preprocessor_context prep_context; /**< preprocessor context */
	parser_context parse_context; /**< parser context */
	struct _tag_typecheck_context typecheck_context; /**< type checking context */
	typestorage_context *typestor_context; /**< Type storage context */
	error_context *err_context; /**< Context for error reporting */
	target_descriptor *desc; /**< Target CPU descriptor */
	language_descriptor *lang_desc; /**< Source language descriptor */
} frontend_context;


/**
   Allocates and initializes a new frontend context.
   @param pool Memory pool for various needs
   @param desc The target CPU descriptor
   @param input_string Single, concatenated input string
   @param source_string_lengths Array of source string lengths. The size of the array is given by n_source_strings.
   @param n_source_strings Number of source strings
*/
/*@null@*/ frontend_context *_essl_new_frontend(mempool *pool, target_descriptor *desc, const char *input_string, const int *source_string_lengths, unsigned int n_source_strings);
/*@null@*/ translation_unit *_essl_run_frontend(frontend_context *ctx);
memerr _essl_insert_global_variable_initializers(mempool *pool, node *root, node *compound);


#endif /* FRONTEND_FRONTEND_H */
