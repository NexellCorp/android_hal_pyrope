/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef FRONTEND_SCANNER_H
#define FRONTEND_SCANNER_H

#include "frontend/token.h"
#include "common/essl_dict.h"
#include "common/error_reporting.h"


struct _tag_language_descriptor;

/**

The scanner takes a sequence of source strings concatenated into a string known as the input string.
_essl_scanner_get_token returns the next token from the input string.

*/



typedef struct {
	const char *input_string; /**< The input string is a single string containing all the concatenated source string */
	int input_string_length; /**< Length of the input string */
	int position; /**< position within the input string. */

	dict keywords; /**< helper dictionary used for looking up keywords */

	mempool *pool; /**< memory pool used for allocating the keyword dictionary */
	error_context *err_context; /**< Context for error reporting */
} scanner_context;



/**
   Initialize a scanner context.
   @param ctx The context to initialize
   @param pool Pool to use for keyword dictionary
   @param e_ctx Context for error reporting
   @param input_string Single, concatenated input string
   @param source_string_lengths Array of source string lengths. The size of the array is given by n_source_strings.
   @param n_source_strings Number of source strings
*/
memerr _essl_scanner_init(/*@out@*/ scanner_context *ctx, mempool *pool, error_context *e_ctx, const char *input_string, const int *source_string_lengths, unsigned int n_source_strings);


/**
   Retrieves a new token from the scanner
   @param ctx Scanner context
   @param token_str If non-zero, this pointer will be filled with the string corresponding to the token
   @returns The new token. If there are no more tokens, TOK_END_OF_FILE will be returned
*/
Token _essl_scanner_get_token(scanner_context *ctx, /*@null@*/ string *token_str);


/**
   Retrieves the number of characters from the beginning of the input string to the current position
   @param ctx Scanner context
*/
int _essl_scanner_get_source_offset(scanner_context *ctx);

/**
   Convert a token to its canonical string representation. Used for error reporting.
   This function returns statically allocated strings.
   @param t Token to convert
   @return The string representation
*/
const char* _essl_token_to_str(Token t);

/**
   Initialize keywords for enabled extensions
 */
memerr _essl_scanner_load_extension_keywords(scanner_context *ctx, struct _tag_language_descriptor *lang);

#endif /* FRONTEND_SCANNER_H */
