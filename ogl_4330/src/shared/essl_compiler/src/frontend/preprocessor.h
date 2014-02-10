/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef FRONTEND_PREPROCESSOR_H
#define FRONTEND_PREPROCESSOR_H

#include "frontend/token.h"
#include "frontend/scanner.h"
#include "frontend/lang.h"
#include "common/essl_dict.h"
#include "common/essl_str.h"
#include "common/essl_list.h"
#include "common/error_reporting.h"

/*
An ESSL preprocessor.
Takes input from the scanner, and emits preprocessed tokens ready for consumption by the parser
*/



/** A preprocessing token.
*/
typedef struct {
	Token tok;
	string tokstr;
	/*@null@*/ dict *replaced_by;      /**< holds information about what macros have been used
										  to process this token, so that we don't
										  expand a macro twice. */
} pp_token;


/** List of preprocessing tokens.
*/
typedef struct _tag_pp_token_list {
	struct _tag_pp_token_list *next;
	pp_token token;
} pp_token_list;
ASSUME_ALIASING(pp_token_list, generic_list);


/** List of preprocessor commands */
typedef enum {
	PREPROCESSOR_UNKNOWN,
	PREPROCESSOR_DEFINE,
	PREPROCESSOR_UNDEF,
	PREPROCESSOR_IF,
	PREPROCESSOR_IFDEF,
	PREPROCESSOR_IFNDEF,
	PREPROCESSOR_ELIF,
	PREPROCESSOR_ELSE,
	PREPROCESSOR_ENDIF,
	PREPROCESSOR_ERROR,
	PREPROCESSOR_PRAGMA,
	PREPROCESSOR_EXTENSION,
	PREPROCESSOR_VERSION,
	PREPROCESSOR_LINE
} preprocessor_command;


typedef enum {
	IF_STATE_IF,
	IF_STATE_ELSE
} if_state;

typedef struct if_stack_level {
	struct if_stack_level *previous;
	if_state state;
} if_stack_level;

/** Context holding the required state for preprocessing.
*/
typedef struct _tag_preprocessor_context{
	Token prev_token;
	string prev_string;
	int position_type;                 /**< position flag, used for knowing whether # version
										  and other preprocessor directives are legal.
										  -1 for start of file, 1 for start of line,
										  0 elsewhere */
	dict defines;
	/*@null@*/ pp_token_list *remaining_replacements; /**< current pending replacements */
	if_stack_level *if_stack;                      /**< current # if nesting level */

	mempool *pool;
	scanner_context *scan_context;
	error_context *err_context;
	language_descriptor *lang_descriptor;
	essl_bool non_preprocessor_token_found; /**< ESSL_TRUE if a non-preprocessor token has been parsed/found */
	int source_body_start; /**< Position at which the actual source text (excluding pre-defines) starts */
} preprocessor_context;


/** List of whitespace handling modes. Used for filtering out unwanted tokens when they aren't needed */
typedef enum {
	IGNORE_WHITESPACE,
	INCLUDE_WHITESPACE,
	IGNORE_NEWLINE,
	INCLUDE_NEWLINE
} whitespace_handling;

/** Macro replacement token list.
*/
typedef struct _tag_replacement_list {
	struct _tag_replacement_list *next;
	Token tok;
	string tokstr;
} replacement_list;
ASSUME_ALIASING(replacement_list, generic_list);

/** List of predefined macros. */
typedef enum {
	PREDEFINED_NONE,
	PREDEFINED_LINE,
	PREDEFINED_FILE,
	PREDEFINED_VERSION,
	PREDEFINED_GL_ES,
	PREDEFINED_GL_FRAGMENT_PRECISION_HIGH,
	PREDEFINED_MALI,
	PREDEFINED_MALI_HW_REV_MAJOR,
	PREDEFINED_MALI_HW_REV_MINOR
} predefined_macro;

/** Stores a macro definition.
*/
typedef struct {
	string identifier;
	/*@null@*/ replacement_list *replist;
	/*@null@*/ replacement_list *args; /* Set if this is a function-like macro,
										  tok = TOK_UNKNOWN for the first argument
										  if the argument list is empty */
	predefined_macro predefined;
} macro_def;


/** Helper structure storing the first and last preprocessing token in a list.
*/
typedef struct {
	/*@null@*/ pp_token_list *first;
	/*@null@*/ pp_token_list *last;
} list_ends;


/** Initializes a preprocessing context.
*/
memerr _essl_preprocessor_init(/*@out@*/ preprocessor_context *ctx, mempool *pool,
							   error_context *e_ctx, scanner_context *s_ctx,
							   language_descriptor *lang_desc);


/** Returns the next token from the source file.
*/
Token _essl_preprocessor_get_token(preprocessor_context *ctx, /*@null@*/ string *token_str,
								   whitespace_handling whitespace);


/** Returns the current source file offset.
*/
int _essl_preprocessor_get_source_offset(preprocessor_context *ctx);

int _essl_preprocessor_extension_macro_add(preprocessor_context *ctx, string name);
int _essl_preprocessor_extension_macro_remove(preprocessor_context *ctx, string name);

#endif /* FRONTEND_PREPROCESSOR_H */
