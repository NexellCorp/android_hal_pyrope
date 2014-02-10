/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#define NEEDS_STDIO  /* for snprintf */

#include "common/essl_system.h"
#include "common/essl_mem.h"
#include "common/essl_list.h"
#include "frontend/token.h"
#include "frontend/preprocessor.h"
#include "common/essl_common.h"

/** Code to access the error context for this module */
#define ERROR_CONTEXT ctx->err_context

/** Code to retrieve the source offset for this module */
#define SOURCE_OFFSET _essl_scanner_get_source_offset(ctx->scan_context)

/** Error code to use for syntax errors in this module */
#define SYNTAX_ERROR_CODE ERR_PP_SYNTAX_ERROR

/** Discards tokens until next newline in input file */
#define DISCARD_REST_OF_LINE() do { ctx->remaining_replacements = 0; while (1) { Token _tmp_tok = read_scanner_token(ctx, 0, IGNORE_WHITESPACE); if (_tmp_tok == TOK_NEWLINE || _tmp_tok == TOK_END_OF_FILE) break; } } while (0)

/*@null@*/ static list_ends *object_macro_replacement(preprocessor_context *ctx, string macroname, macro_def *def, /*@null@*/ dict *replaced_by);
/*@null@*/ static list_ends *function_macro_replacement(preprocessor_context *ctx, string macroname, macro_def *def, /*@null@*/ dict *replaced_by, int in_preprocessing_directive);

static int divide(int a, int n);
static int modulo(int a, int n);

typedef /*@null@*/ pp_token_list *pp_token_list_ptr;




/** Helper function for adding a predefined macro */
static int add_predefined_macro(preprocessor_context *ctx, const char *name, predefined_macro macroid)
{
	string identifier = _essl_cstring_to_string_nocopy(name);
	macro_def *def = _essl_mempool_alloc(ctx->pool, sizeof(macro_def));
	ESSL_CHECK_OOM(def);
	def->identifier = identifier;
	def->replist = 0;
	def->args = 0;
	def->predefined = macroid;
	ESSL_CHECK_OOM(_essl_dict_insert(&ctx->defines, identifier, def));
	return 1;
}

/**
 * Returns a friendly name/text for a given token.
 * Currently we only make sure that TOK_NEWLINE isn't returned as '\n',
 * since this will look silly in error messages.
 * @param tok Token to get name for
 * @param str Token text string
 * @return the friendly name/text for token
 */
static string get_friendly_token_name(Token tok, string str)
{
	string empty;
	empty.ptr = "";
	empty.len = 0;

	if (tok == TOK_NEWLINE)
	{
		return empty;
	}

	return str;
}


/**
 * Check if specified token is end of line (or end of file).
 * @param tok Token to check
 * @param ESSL_TRUE if token is TOK_NEWLINE or TOK_END_OF_FILE
 */
static essl_bool token_is_end_of_line(Token tok)
{
	return (tok == TOK_NEWLINE || tok == TOK_END_OF_FILE);
}

static memerr push_if_stack_entry(preprocessor_context *ctx)
{
	if_stack_level *entry;
	ESSL_CHECK_OOM(entry = _essl_mempool_alloc(ctx->pool, sizeof(*entry)));
	entry->state = IF_STATE_IF;
	entry->previous = ctx->if_stack;
	ctx->if_stack = entry;
	return MEM_OK;
}

static memerr pop_if_stack_entry(preprocessor_context *ctx)
{
	ctx->if_stack = ctx->if_stack->previous;
	return MEM_OK;
}



/** Helper function for adding a predefined macro for an extension (e.g. GL_OES_shadow) */
int _essl_preprocessor_extension_macro_add(preprocessor_context *ctx, string identifier)
{
	macro_def *def = _essl_mempool_alloc(ctx->pool, sizeof(macro_def));
	replacement_list *value_entry = _essl_mempool_alloc(ctx->pool, sizeof(replacement_list));

	ESSL_CHECK_OOM(def);
	ESSL_CHECK_OOM(value_entry);

	def->identifier = identifier;
	def->replist = value_entry;
	def->args = NULL;
	def->predefined = PREDEFINED_NONE;

	value_entry->next = NULL;
	value_entry->tok = TOK_INTCONSTANT;
	value_entry->tokstr = _essl_cstring_to_string_nocopy("1");

	ESSL_CHECK_OOM(_essl_dict_insert(&ctx->defines, identifier, def));

	return 1;
}


/** Helper function for removing a predefined macro for an extension (e.g. GL_OES_shadow) */
int _essl_preprocessor_extension_macro_remove(preprocessor_context *ctx, string identifier)
{
	(void)_essl_dict_remove(&ctx->defines, identifier); /* Ok if it doesn't exist */
	return 1;
}


/** Returns nonzero if the token is a valid preprocessor identifier */
static int is_identifier(Token t)
{
	return (int)(t >= IDENTIFIER_KEYWORD_START && t <= IDENTIFIER_KEYWORD_END);
}

/** Returns a token from the scanner, possibly using a prefetched and/or skipping whitespace */
static Token read_scanner_token(preprocessor_context *ctx, /*@null@*/ /*@out@*/ string *s, whitespace_handling whitespace)
{
	Token tok;
	string str = {"<dummy>", 7};
	if (ctx->prev_token != TOK_UNKNOWN)
	{
		tok = ctx->prev_token;
		ctx->prev_token = TOK_UNKNOWN;
		if (tok != TOK_WHITESPACE || whitespace != IGNORE_WHITESPACE)
		{
			if (s != 0) *s = ctx->prev_string;
			return tok;
		}
	}
	tok = _essl_scanner_get_token(ctx->scan_context, &str);
	if (tok == TOK_WHITESPACE)
	{
		/* Combine multiple whitespace tokens into one */
		do
		{
			ctx->prev_token = _essl_scanner_get_token(ctx->scan_context, &ctx->prev_string);
		} while (ctx->prev_token == TOK_WHITESPACE);
		if (whitespace == IGNORE_WHITESPACE)
		{
			tok = ctx->prev_token;
			ctx->prev_token = TOK_UNKNOWN;
			if (s != 0) *s = ctx->prev_string;
			return tok;
		}
		else
		{
			str = _essl_cstring_to_string_nocopy(" ");
		}
	}
	if (s) *s = str;
	return tok;
}

/** Returns the next preprocessing token, either from replaced macros or new tokens based on the user source code */
static pp_token get_pp_token(preprocessor_context *ctx)
{
	pp_token tok;
	tok.tokstr.ptr = "<dummy>";
	tok.tokstr.len = 7;
	if (ctx->remaining_replacements)
	{
		tok = ctx->remaining_replacements->token;
		LIST_REMOVE(&ctx->remaining_replacements);
	}
	else
	{
		tok.tok = read_scanner_token(ctx, &tok.tokstr, IGNORE_WHITESPACE);
		tok.replaced_by = 0;
	}
	return tok;
}

/** Looks at the next preprocessing token without removing it */
static pp_token peek_pp_token(preprocessor_context *ctx)
{
	if (!ctx->remaining_replacements)
	{
		pp_token_list *newlist = LIST_NEW(ctx->pool, pp_token_list);
		if (!newlist)
		{
			pp_token pptok;
			INTERNAL_ERROR_OUT_OF_MEMORY();
			pptok.tok = TOK_UNKNOWN;
			pptok.tokstr = _essl_cstring_to_string_nocopy("");
			pptok.replaced_by = NULL;
			return pptok;
		}
		newlist->token = get_pp_token(ctx);
		LIST_INSERT_FRONT(&ctx->remaining_replacements, newlist);
		assert(ctx->remaining_replacements != 0);
	}
	return ctx->remaining_replacements->token;
}

/** Returns zero if the identifier is a reserved name and thus illegal to define in shaders */
static essl_bool is_macro_name_legal(string identifier)
{
	int i;

	assert(identifier.len > 0 && identifier.ptr != 0);

	if (identifier.len >= 3)
	{
		essl_bool ok = ESSL_FALSE;
		for (i = 0; i < 3; ++i)
		{
			if (identifier.ptr[i] != "GL_"[i])
			{
				ok = ESSL_TRUE;
			}
		}
		if (!ok)
		{
			return ESSL_FALSE;
		}
	}

	for (i = 0; i < identifier.len - 1; ++i)
	{
		if (identifier.ptr[i] == '_' && identifier.ptr[i + 1] == '_')
		{
			return ESSL_FALSE;
		}
	}

	return ESSL_TRUE;
}

/** Handles a 'define' preprocessing directive, storing an object-life or function-like macro */
static int define(preprocessor_context *ctx, string identifier)
{
	Token tok;
	string tokstr;
	replacement_list *replist = 0;
	replacement_list *lastitem = 0;
	replacement_list *curitem;
	replacement_list *args = 0;
	macro_def *def;

	tok = read_scanner_token(ctx, &tokstr, INCLUDE_WHITESPACE);
	if (tok == TOK_LEFT_PAREN)
	{
		int want_arg = -1;
		while (1)
		{
			tok = read_scanner_token(ctx, &tokstr, IGNORE_WHITESPACE);
			if (tok == TOK_RIGHT_PAREN || token_is_end_of_line(tok))
			{
				break;
			}
			if (want_arg)
			{
				if (!is_identifier(tok))
				{
					SYNTAX_ERROR_STRING("Expected identifier, found '%s'\n", tokstr);
					DISCARD_REST_OF_LINE();
					return 0;
				}
				want_arg = 0;
			}
			else if (tok == TOK_COMMA)
			{
				want_arg = 1;
				continue;
			}
			else
			{
				SYNTAX_ERROR_STRING("Unexpected token '%s' in argument list\n", tokstr);
				DISCARD_REST_OF_LINE();
				return 0;
			}

			/* Check for repeated argument name */
			for (curitem = args; curitem; curitem = curitem->next)
			{
				if (!_essl_string_cmp(curitem->tokstr, tokstr))
				{
					SYNTAX_ERROR_STRING("Token '%s' repeated in argument list\n", tokstr);
					DISCARD_REST_OF_LINE();
					return 0;
				}
			}

			curitem = LIST_NEW(ctx->pool, replacement_list);
			ESSL_CHECK_OOM(curitem);
			curitem->tok = tok;
			curitem->tokstr = tokstr;
			if (lastitem)
			{
				LIST_INSERT_BACK(&lastitem->next, curitem);
			}
			else
			{
				args = curitem;
			}
			lastitem = curitem;
		}
		if (tok != TOK_RIGHT_PAREN || want_arg == 1)
		{
			SYNTAX_ERROR_STRING("Unexpected end of argument list\n", tokstr);
			DISCARD_REST_OF_LINE();
			return 0;
		}

		/* Create fake arg to flag definition as function-like macro */
		if (!args)
		{
			args = LIST_NEW(ctx->pool, replacement_list);
			ESSL_CHECK_OOM(args);
			args->tok = TOK_UNKNOWN;
			args->tokstr = _essl_cstring_to_string_nocopy("%empty%");
		}

		tok = read_scanner_token(ctx, &tokstr, IGNORE_WHITESPACE);
	}

	if (tok == TOK_WHITESPACE)
	{
		tok = read_scanner_token(ctx, &tokstr, IGNORE_WHITESPACE);
	}

	lastitem = 0;
	while (1)
	{
		if (token_is_end_of_line(tok))
		{
			break;
		}
		curitem = LIST_NEW(ctx->pool, replacement_list);
		ESSL_CHECK_OOM(curitem);
		curitem->tok = tok;
		curitem->tokstr = tokstr;
		if (lastitem)
		{
			LIST_INSERT_BACK(&lastitem->next, curitem);
		}
		else
		{
			replist = curitem;
		}
		lastitem = curitem;

		tok = read_scanner_token(ctx, &tokstr, INCLUDE_WHITESPACE);
	}

	if (!is_macro_name_legal(identifier))
	{
		SYNTAX_ERROR_STRING("Macro name '%s' reserved\n", identifier);
		return 0;
	}

	/* Check for redefinition */
	if ((def = _essl_dict_lookup(&ctx->defines, identifier)))
	{
		replacement_list *olditer;
		replacement_list *newiter;

		newiter = args;
		olditer = def->args;
		while (newiter != 0 && olditer != 0) {
			if ((newiter->tok != olditer->tok) || _essl_string_cmp(newiter->tokstr, olditer->tokstr))
			{
				SYNTAX_ERROR_STRING("Macro '%s' redefined\n", identifier);
				return 0;
			}

			newiter = newiter->next;
			olditer = olditer->next;
		}
		if (newiter || olditer)
		{
			SYNTAX_ERROR_STRING("Macro '%s' redefined\n", identifier);
			return 0;
		}

		newiter = replist;
		olditer = def->replist;
		while (newiter != 0 && olditer != 0) {
			if ((newiter->tok != olditer->tok) || _essl_string_cmp(newiter->tokstr, olditer->tokstr))
			{
				SYNTAX_ERROR_STRING("Macro '%s' redefined\n", identifier);
				return 0;
			}

			newiter = newiter->next;
			olditer = olditer->next;
		}
		if (newiter || olditer)
		{
			SYNTAX_ERROR_STRING("Macro '%s' redefined\n", identifier);
			return 0;
		}
	}

	def = _essl_mempool_alloc(ctx->pool, sizeof(macro_def));
	ESSL_CHECK_OOM(def);
	def->identifier = identifier;
	def->replist = replist;
	def->args = args;
	def->predefined = PREDEFINED_NONE;

	ESSL_CHECK_OOM(_essl_dict_insert(&ctx->defines, identifier, def));

	return 1;
}

/** Handles a 'undef' preprocessing directive, checking for predefined macros */
static int undef(preprocessor_context *ctx, string identifier)
{
	macro_def *def = _essl_dict_lookup(&ctx->defines, identifier);
	int ret;
	Token tok;

	if (def != NULL && def->predefined != PREDEFINED_NONE)
	{
		SYNTAX_ERROR_STRING("Cannot undefine a predefined macro '%s'\n", identifier);
		ret = 0;
	}
	else
	{
		(void)_essl_dict_remove(&ctx->defines, identifier);
		ret = 1;
	}

	tok = read_scanner_token(ctx, 0, IGNORE_WHITESPACE);
	if (!token_is_end_of_line(tok))
	{
		SYNTAX_ERROR_MSG("Unexpected text found after #undef directive\n");
		DISCARD_REST_OF_LINE();
		ret = 0;
	}

	return ret;
}


/** Creates a new preprocessing token using the specified token code and string contents */
static pp_token new_pp_token(Token tok, string value)
{
	pp_token pptok;
	pptok.tok = tok;
	pptok.tokstr = value;
	pptok.replaced_by = 0;
	return pptok;
}

/** Handles the 'defined' operator, returning a '1' or '0' preprocessing token */
static int defined_operator(preprocessor_context *ctx, pp_token *ret_token)
{
	pp_token pptok;
	string is_defined_str;
	if (peek_pp_token(ctx).tok == TOK_LEFT_PAREN)
	{
		/* defined ( TOK_IDENTIFIER ) */
		(void)get_pp_token(ctx);
		pptok = get_pp_token(ctx);
		if (get_pp_token(ctx).tok != TOK_RIGHT_PAREN)
		{
			SYNTAX_ERROR_MSG("Illegal use of 'defined' operator\n");
			DISCARD_REST_OF_LINE();
			return 0;
		}
	}
	else
	{
		/* defined TOK_IDENTIFIER */
		pptok = get_pp_token(ctx);
	}
	if (!is_identifier(pptok.tok))
	{
		SYNTAX_ERROR_MSG("Identifier required after 'defined' operator\n");
		DISCARD_REST_OF_LINE();
		return 0;
	}

	if (_essl_dict_lookup(&ctx->defines, pptok.tokstr))
	{
		is_defined_str = _essl_cstring_to_string_nocopy("1");
	}
	else
	{
		is_defined_str = _essl_cstring_to_string_nocopy("0");
	}

	if (ret_token)
	{
		*ret_token = new_pp_token(TOK_INTCONSTANT, is_defined_str);
	}

	return 1;
}

static memerr generate_integer_token(preprocessor_context *ctx, int value, pp_token *token_out)
{
	char *buf;
	ESSL_CHECK(buf = _essl_mempool_alloc(ctx->pool, 20));
	(void)snprintf(buf, 20, "%d", value);
	*token_out = new_pp_token(TOK_INTCONSTANT, _essl_cstring_to_string_nocopy(buf));
	return MEM_OK;
}

/** Replaces a macro call with the appropriate list of preprocessing tokens */
static int replace_macro(preprocessor_context *ctx, macro_def *def, string macroname, list_ends *result, /*@null@*/ dict *replaced_by, int in_preprocessing_directive)
{
	list_ends *listends;
	if (def->predefined != PREDEFINED_NONE)
	{
		pp_token_list *newlist;
		pp_token pptok;

		listends = _essl_mempool_alloc(ctx->pool, sizeof(list_ends));
		ESSL_CHECK_OOM(listends);

		newlist = LIST_NEW(ctx->pool, pp_token_list);
		ESSL_CHECK_OOM(newlist);
		switch (def->predefined)
		{
		case PREDEFINED_NONE:
			assert(0);
			break;
		case PREDEFINED_LINE:
			{
				int source_offset = _essl_preprocessor_get_source_offset(ctx);
				int linenum;
				int _tmp;
				_essl_error_get_position(ctx->err_context, source_offset, &_tmp, &linenum);
				ESSL_CHECK(generate_integer_token(ctx, linenum, &pptok));
			}
			break;
		case PREDEFINED_FILE:
			{
				int source_offset = _essl_preprocessor_get_source_offset(ctx);
				int srcstring;
				int _tmp;
				_essl_error_get_position(ctx->err_context, source_offset, &srcstring, &_tmp);
				ESSL_CHECK(generate_integer_token(ctx, srcstring, &pptok));
			}
			break;
		case PREDEFINED_VERSION:
			pptok = new_pp_token(TOK_INTCONSTANT, _essl_get_language_version(ctx->lang_descriptor));
			break;
		case PREDEFINED_GL_ES:
		case PREDEFINED_GL_FRAGMENT_PRECISION_HIGH:
			pptok = new_pp_token(TOK_INTCONSTANT, _essl_cstring_to_string_nocopy("1"));
			break;
		case PREDEFINED_MALI:
			ESSL_CHECK(generate_integer_token(ctx, 2, &pptok));
			break;
		case PREDEFINED_MALI_HW_REV_MAJOR:
			ESSL_CHECK(generate_integer_token(ctx, ctx->lang_descriptor->target_desc->options->hw_rev >> 16, &pptok));
			break;
		case PREDEFINED_MALI_HW_REV_MINOR:
			ESSL_CHECK(generate_integer_token(ctx, ctx->lang_descriptor->target_desc->options->hw_rev & 0xffff, &pptok));
			break;
		}
		newlist->token = pptok;
		listends->first = newlist;
		listends->last = newlist;
	}
	else
	{
		if (!def->args)
		{
			listends = object_macro_replacement(ctx, macroname, def, replaced_by);
		}
		else
		{
			listends = function_macro_replacement(ctx, macroname, def, replaced_by, in_preprocessing_directive);
		}
		ESSL_CHECK_NONFATAL(listends);
	}

	if (!result->first)
	{
		assert(result->first == 0 && result->last == 0);
		result->first = listends->first;
		result->last = listends->last;
	}
	else
	{
		assert(result->first != 0 && result->last != 0);
		result->last->next = listends->first;
		result->last = listends->last;
	}
	return 1;
}

/** Macro expands a preprocessing directive line. Handles the 'defined' operator and identifier replacement in 'if' expressions */
static memerr macro_expand_preprocessing_directive(preprocessor_context *ctx, int if_expression, list_ends *result)
{
	pp_token pptok;
	pp_token_list *newlist;
	essl_bool special_case = ESSL_FALSE;

	result->first = NULL;
	result->last = NULL;

	do
	{
		pptok = peek_pp_token(ctx);
		if (pptok.tok == TOK_UNKNOWN)
		{
			return MEM_ERROR;
		}
		if (token_is_end_of_line(pptok.tok))
		{
			break; /* Directive lasts until end of line */
		}
		(void)get_pp_token(ctx);
		if (is_identifier(pptok.tok))
		{
			if (if_expression && !_essl_string_cmp(pptok.tokstr, _essl_cstring_to_string_nocopy("defined")))
			{
				ESSL_CHECK_NONFATAL(defined_operator(ctx, &pptok));
			}
			else
			{
				macro_def *def = _essl_dict_lookup(&ctx->defines, pptok.tokstr);
				if (def && (!pptok.replaced_by || !_essl_dict_has_key(pptok.replaced_by, pptok.tokstr)))
				{
					if (!def->args || peek_pp_token(ctx).tok == TOK_LEFT_PAREN)
					{
						list_ends res;
						res.first = 0;
						res.last = 0;
						ESSL_CHECK_NONFATAL(replace_macro(ctx, def, pptok.tokstr, &res, 0, 1));
						if (res.last != NULL)
						{
							res.last->next = ctx->remaining_replacements;
							ctx->remaining_replacements = res.first;
						}
						continue;
					}
				}
				else if (if_expression)
				{
					if (!_essl_string_cmp(pptok.tokstr, _essl_cstring_to_string_nocopy("true")))
					{
						pptok = new_pp_token(TOK_INTCONSTANT, _essl_cstring_to_string_nocopy("1"));
					}
					else if (!_essl_string_cmp(pptok.tokstr, _essl_cstring_to_string_nocopy("false")))
					{
						pptok = new_pp_token(TOK_INTCONSTANT, _essl_cstring_to_string_nocopy("0"));
					}else if(special_case)
					{
						pptok = new_pp_token(TOK_UNDEFINEDCONST, pptok.tokstr);
					}
					else
					{
						SYNTAX_ERROR_STRING("Error parsing constant expression, unknown identifier '%s'\n", pptok.tokstr);
						DISCARD_REST_OF_LINE();
						return MEM_ERROR;
					}
				}
			}
		}else if(pptok.tok == TOK_AND_OP || pptok.tok == TOK_OR_OP)
		{
			special_case = ESSL_TRUE;
		}

		newlist = LIST_NEW(ctx->pool, pp_token_list);
		ESSL_CHECK_OOM(newlist);
		newlist->token = pptok;
		if (!result->first)
		{
			result->first = newlist;
			result->last = newlist;
		}
		else
		{
			assert(result->last != 0);
			LIST_INSERT_FRONT(&result->last->next, newlist);
			result->last = newlist;
		}
	} while (1);

	return MEM_OK;
}

/** Looks at the next token in a constant expression without removing it */
static Token peek_ce_token(pp_token_list_ptr *tokenptr, int *is_ok, /*@null@*/ /*@out@*/ string *retstr)
{
	assert(tokenptr != 0 && is_ok != 0);
	if (*tokenptr == 0)
	{ 
		*is_ok = 0;
		return TOK_END_OF_FILE;
	}
	if (*is_ok == 0) return TOK_END_OF_FILE;

	if (retstr) *retstr = (*tokenptr)->token.tokstr;
	return (*tokenptr)->token.tok;
}

/** Returns the next token from a constant expression */
static Token get_ce_token(pp_token_list_ptr *tokenptr, int *is_ok, /*@null@*/ /*@out@*/ string *retstr)
{
	Token tok = peek_ce_token(tokenptr, is_ok, retstr);
	assert(*tokenptr != 0);
	*tokenptr = ((*tokenptr)->next);
	return tok;
}

static int logical_inclusive_or(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok);

/** @defgroup PreprocConstExprParser Functions used for the recursive-descent constant expression parser.
    @{
 */
static int parenthetical_grouping(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	string tokstr = {0,0};
	Token nexttok = peek_ce_token(tokenptr, is_ok, 0);
	if (nexttok == TOK_LEFT_PAREN)
	{
		int val;
		(void)get_ce_token(tokenptr, is_ok, 0);
		val = logical_inclusive_or(ctx, tokenptr, is_ok);
		if (get_ce_token(tokenptr, is_ok, 0) == TOK_RIGHT_PAREN)
		{
			return val;
		}
	}
	else if (nexttok == TOK_INTCONSTANT)
	{
		int val = 0;
		(void)get_ce_token(tokenptr, is_ok, &tokstr);
		assert(tokstr.ptr != 0);

		if (_essl_string_to_integer(tokstr, 0, &val) != 0)
		{
			return val;
		}
	}else if (nexttok == TOK_UNDEFINEDCONST)
	{
		(void)get_ce_token(tokenptr, is_ok, &tokstr);
		assert(tokstr.ptr != 0);
		*is_ok = 0;
		return 0;
	}


	/* Error */
	*is_ok = 0;
	return 0;
}

static int unary(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	Token nexttok = peek_ce_token(tokenptr, is_ok, 0);
	if (*tokenptr && (nexttok == TOK_PLUS || nexttok == TOK_DASH || nexttok == TOK_TILDE || nexttok == TOK_BANG))
	{
		int val;
		(void)get_ce_token(tokenptr, is_ok, 0);
		val = unary(ctx, tokenptr, is_ok);
		switch (nexttok)
		{
		case TOK_PLUS:
			val = +val;
			break;
		case TOK_DASH:
			val = -val;
			break;
		case TOK_TILDE:
			val = ~val;
			break;
		case TOK_BANG:
			val = !val;
			break;
		default:
			assert(0);
			break;
		}
		return val;
	}
	else
	{
		return parenthetical_grouping(ctx, tokenptr, is_ok);
	}
}

static int multiplicative(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = unary(ctx, tokenptr, is_ok);
	Token nexttok;
	if (!(*tokenptr)) return first;
	nexttok = peek_ce_token(tokenptr, is_ok, 0);
	while (*tokenptr && (nexttok == TOK_STAR || nexttok == TOK_SLASH || nexttok == TOK_PERCENT))
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = unary(ctx, tokenptr, is_ok);
		switch (nexttok)
		{
		case TOK_STAR:
			first = (first * next);
			break;
		case TOK_SLASH:
			if (next == 0)
			{
				*is_ok = 0;
				return 0;
			}
			else
			{
				first = divide(first, next);
			}
			break;
		case TOK_PERCENT:
			if (next == 0)
			{
				*is_ok = 0;
				return 0;
			}
			else
			{
				first = modulo(first, next);
			}
			break;
		default:
			assert(0);
			break;
		}
		if (*tokenptr)
		{
			nexttok = peek_ce_token(tokenptr, is_ok, 0);
		}
	}
	return first;
}

static int additive(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = multiplicative(ctx, tokenptr, is_ok);
	Token nexttok;
	if (!(*tokenptr)) return first;
	nexttok = peek_ce_token(tokenptr, is_ok, 0);
	while (*tokenptr && (nexttok == TOK_PLUS || nexttok == TOK_DASH))
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = multiplicative(ctx, tokenptr, is_ok);
		if (nexttok == TOK_PLUS)
		{
			first = (first + next);
		}
		else
		{
			first = (first - next);
		}
		if (*tokenptr)
		{
			nexttok = peek_ce_token(tokenptr, is_ok, 0);
		}
	}
	return first;
}

static int bitwise_shift(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = additive(ctx, tokenptr, is_ok);
	Token nexttok;
	if (!(*tokenptr)) return first;
	nexttok = peek_ce_token(tokenptr, is_ok, 0);
	while (*tokenptr && (nexttok == TOK_LEFT_OP || nexttok == TOK_RIGHT_OP))
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = additive(ctx, tokenptr, is_ok);
		if(next < 0)
		{
			/* cannot have negative shifts */
			SYNTAX_WARNING_MSG("Right operand of shift has negative value\n");
		}
		if (nexttok == TOK_LEFT_OP)
		{
			first = (int)(first << ((unsigned)next));
		}
		else
		{
			first = (int)(first >> ((unsigned)next));
		}
		if (*tokenptr)
		{
			nexttok = peek_ce_token(tokenptr, is_ok, 0);
		}
	}
	return first;
}

static int relational(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = bitwise_shift(ctx, tokenptr, is_ok);
	Token nexttok;
	if (!(*tokenptr)) return first;
	nexttok = peek_ce_token(tokenptr, is_ok, 0);
	while (*tokenptr && (nexttok == TOK_LEFT_ANGLE || nexttok == TOK_RIGHT_ANGLE || nexttok == TOK_LE_OP || nexttok == TOK_GE_OP))
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = bitwise_shift(ctx, tokenptr, is_ok);
		switch (nexttok)
		{
		case TOK_LEFT_ANGLE:
			first = (first < next);
			break;
		case TOK_RIGHT_ANGLE:
			first = (first > next);
			break;
		case TOK_LE_OP:
			first = (first <= next);
			break;
		case TOK_GE_OP:
			first = (first >= next);
			break;
		default:
			assert(0);
			break;
		}
		if (*tokenptr)
		{
			nexttok = peek_ce_token(tokenptr, is_ok, 0);
		}
	}
	return first;
}

static int equality(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = relational(ctx, tokenptr, is_ok);
	Token nexttok;
	if (!(*tokenptr)) return first;
	nexttok = peek_ce_token(tokenptr, is_ok, 0);
	while (*tokenptr && (nexttok == TOK_EQ_OP || nexttok == TOK_NE_OP))
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = relational(ctx, tokenptr, is_ok);
		if (nexttok == TOK_EQ_OP)
		{
			first = (first == next);
		}
		else
		{
			first = (first != next);
		}
		if (*tokenptr)
		{
			nexttok = peek_ce_token(tokenptr, is_ok, 0);
		}
	}
	return first;
}

static int bitwise_and(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = equality(ctx, tokenptr, is_ok);
	while (*tokenptr && peek_ce_token(tokenptr, is_ok, 0) == TOK_AMPERSAND)
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = equality(ctx, tokenptr, is_ok);
		first = first & next;
	}
	return first;
}

static int bitwise_exclusive_or(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = bitwise_and(ctx, tokenptr, is_ok);
	while (*tokenptr && peek_ce_token(tokenptr, is_ok, 0) == TOK_CARET)
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = bitwise_and(ctx, tokenptr, is_ok);
		first = first ^ next;
	}
	return first;
}

static int bitwise_inclusive_or(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = bitwise_exclusive_or(ctx, tokenptr, is_ok);
	while (*tokenptr && peek_ce_token(tokenptr, is_ok, 0) == TOK_VERTICAL_BAR)
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = bitwise_exclusive_or(ctx, tokenptr, is_ok);
		first = first | next;
	}
	return first;
}

static int logical_and(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = bitwise_inclusive_or(ctx, tokenptr, is_ok);
	while (*tokenptr && peek_ce_token(tokenptr, is_ok, 0) == TOK_AND_OP)
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = bitwise_inclusive_or(ctx, tokenptr, is_ok);
		if(first == 0)
		{
			*is_ok = ESSL_TRUE;
			first = 0;
		}else
		{
			first = first && next;
		}
	}
	return first;
}

static int logical_inclusive_or(preprocessor_context *ctx, pp_token_list_ptr *tokenptr, int *is_ok)
{
	int first = logical_and(ctx, tokenptr, is_ok);
	while (*tokenptr && peek_ce_token(tokenptr, is_ok, 0) == TOK_OR_OP)
	{
		int next;
		(void)get_ce_token(tokenptr, is_ok, 0);
		next = logical_and(ctx, tokenptr, is_ok);
		if(first == 1)
		{
			*is_ok = ESSL_TRUE;
			first = 1;
		}else
		{
			first = first || next;
		}
	}
	return first;
}
/** @} */

/** Parses a constant expression in a preprocessing directive.
    If 'ends' contains tokens, they are parsed instead of the source file.
    The remaining tokens are returned in 'ends'.
*/
static int directive_constant_expression(preprocessor_context *ctx, int *result, int if_expression, list_ends *ends)
{
	list_ends parsed_ends;
	list_ends *listends;

	if (ends != NULL && ends->first != NULL)
	{
		listends = ends;
	}
	else
	{
		ESSL_CHECK_NONFATAL(macro_expand_preprocessing_directive(ctx, if_expression, &parsed_ends));
		listends = &parsed_ends;
	}
	ESSL_CHECK_NONFATAL(listends);

	{
		pp_token_list *tokenptr = listends->first;
		int is_ok = 1;
		int value = logical_inclusive_or(ctx, &tokenptr, &is_ok);
		if (is_ok && (!tokenptr || ends))
		{
			if (ends)
			{
				ends->first = tokenptr;
				ends->last = listends->last;
			}
			if (result)
			{
				*result = value;
			}
			return 1;
		}
		else
		{
			SYNTAX_ERROR_MSG("Error parsing constant expression\n");
			return 0;
		}
	}
}

static const struct string_command {
	string str;
	preprocessor_command command;
} command_strings[] = {
	{ { "define", 6 }, PREPROCESSOR_DEFINE },
	{ { "undef", 5 }, PREPROCESSOR_UNDEF },
	{ { "if", 2 }, PREPROCESSOR_IF },
	{ { "ifdef", 5 }, PREPROCESSOR_IFDEF },
	{ { "ifndef", 6 }, PREPROCESSOR_IFNDEF },
	{ { "elif", 4 }, PREPROCESSOR_ELIF },
	{ { "else", 4 }, PREPROCESSOR_ELSE },
	{ { "endif", 5 }, PREPROCESSOR_ENDIF },
	{ { "error", 5 }, PREPROCESSOR_ERROR },
	{ { "pragma", 6 }, PREPROCESSOR_PRAGMA },
	{ { "extension", 9 }, PREPROCESSOR_EXTENSION },
	{ { "version", 7 }, PREPROCESSOR_VERSION },
	{ { "line", 4 }, PREPROCESSOR_LINE }
};

static preprocessor_command parse_command(string cmdstr)
{
	unsigned int i;
	for (i = 0 ; i < sizeof(command_strings)/sizeof(struct string_command) ; i++)
	{
		if (!_essl_string_cmp(cmdstr, command_strings[i].str))
		{
			return command_strings[i].command;
		}
	}
	return PREPROCESSOR_UNKNOWN;
}

static preprocessor_command encounter_command(string cmdstr)
{
	preprocessor_command cmd = parse_command(cmdstr);
	return cmd;
}

static int skip_tokens(preprocessor_context *ctx, unsigned actions);

#define STOP_ON_ENDIF      1
#define STOP_ON_TRUE_ELSE  2
#define IGNORE_SYNTAX_ERRORS 4


static int skip_tokens(preprocessor_context *ctx, unsigned actions)
{
	Token tok;
	string tokstr;
	preprocessor_command cmd;
	do
	{
		tok = read_scanner_token(ctx, &tokstr, IGNORE_WHITESPACE);
		if (tok == TOK_HASH)
		{
			tok = read_scanner_token(ctx, &tokstr, IGNORE_WHITESPACE);
			cmd = encounter_command(tokstr);
			switch (cmd)
			{
			case PREPROCESSOR_IF:
			case PREPROCESSOR_IFDEF:
			case PREPROCESSOR_IFNDEF:
				DISCARD_REST_OF_LINE();
				ESSL_CHECK(push_if_stack_entry(ctx));

				ESSL_CHECK_NONFATAL(skip_tokens(ctx, STOP_ON_ENDIF|IGNORE_SYNTAX_ERRORS));
				tok = TOK_NEWLINE; /* make sure we don't skip important tokens after this */
				break;
			case PREPROCESSOR_ENDIF:
				tok = read_scanner_token(ctx, 0, IGNORE_WHITESPACE);
				if (!token_is_end_of_line(tok))
				{
					if(!(actions & IGNORE_SYNTAX_ERRORS))
					{
						SYNTAX_ERROR_MSG("Expected end of line\n");
					}
					DISCARD_REST_OF_LINE();
				}
				if (ctx->if_stack == NULL)
				{
					SYNTAX_ERROR_MSG("#endif directive found outside if-section\n");
					DISCARD_REST_OF_LINE();
					return 0;
				}
				ESSL_CHECK_NONFATAL(pop_if_stack_entry(ctx));

				if(actions & STOP_ON_ENDIF)
				{
					return 1;
				}
				break;
			case PREPROCESSOR_ELSE:
				tok = read_scanner_token(ctx, 0, IGNORE_WHITESPACE);
				if (!token_is_end_of_line(tok))
				{
					if(!(actions & IGNORE_SYNTAX_ERRORS))
					{
						SYNTAX_ERROR_MSG("Expected end of line\n");
					}
					DISCARD_REST_OF_LINE();
				}

				if (ctx->if_stack == NULL)
				{
					SYNTAX_ERROR_MSG("#else directive found outside if-section\n");
					DISCARD_REST_OF_LINE();
					return 0;
				}
				if(ctx->if_stack->state == IF_STATE_ELSE)
				{
					SYNTAX_ERROR_MSG("Illegal use of else\n");
					DISCARD_REST_OF_LINE();
					return 0;
				}
				ctx->if_stack->state = IF_STATE_ELSE;

				if(actions & STOP_ON_TRUE_ELSE)
				{
					return 1;
				}

				break;
			case PREPROCESSOR_ELIF:
				if (actions & STOP_ON_TRUE_ELSE)
				{
					int do_elif = 0;

					ESSL_CHECK_NONFATAL(directive_constant_expression(ctx, &do_elif, 1, 0));

					tok = get_pp_token(ctx).tok;
					assert(token_is_end_of_line(tok));

					if (do_elif)
					{
						return 1;
					}
				}
				if (ctx->if_stack == NULL)
				{
					SYNTAX_ERROR_MSG("#elif directive found outside if-section\n");
					DISCARD_REST_OF_LINE();
					return 0;
				}
				if(ctx->if_stack->state == IF_STATE_ELSE)
				{
					SYNTAX_ERROR_MSG("Illegal use of elif\n");
					DISCARD_REST_OF_LINE();
					return 0;
				}
				if((actions & STOP_ON_ENDIF) && !(actions & STOP_ON_TRUE_ELSE))
				{
					DISCARD_REST_OF_LINE();
					ESSL_CHECK_NONFATAL(skip_tokens(ctx, STOP_ON_ENDIF|IGNORE_SYNTAX_ERRORS));
					return 1;
				}

				break;
			default:
				break;
			}
		}

		while (tok != TOK_UNKNOWN && !token_is_end_of_line(tok))
		{
			tok = read_scanner_token(ctx, &tokstr, IGNORE_WHITESPACE);
		}
	} while (tok != TOK_UNKNOWN && tok != TOK_END_OF_FILE);
	return 0;

}


/** Processes a preprocessing directive line.
    @param ctx The preprocessor context.
    @param directive The preprocessing directive.
 */
static int handle_directive(preprocessor_context *ctx, string directive)
{
	preprocessor_command cmd = encounter_command(directive);
	switch (cmd)
	{
	case PREPROCESSOR_DEFINE:
	case PREPROCESSOR_UNDEF:
	{
		Token tok;
		string tokstr;
		tok = read_scanner_token(ctx, &tokstr, IGNORE_WHITESPACE);
		if (!is_identifier(tok))
		{
			SYNTAX_ERROR_STRING("Invalid identifier '%s'\n", tokstr);
			DISCARD_REST_OF_LINE();
			return 0;
		}

		if (cmd == PREPROCESSOR_DEFINE)
		{
			ESSL_CHECK_NONFATAL(define(ctx, tokstr));
		}
		else
		{
			ESSL_CHECK_NONFATAL(undef(ctx, tokstr));
		}
		return 1;
	}
	case PREPROCESSOR_IF:
	case PREPROCESSOR_IFDEF:
	case PREPROCESSOR_IFNDEF:
	{
		int do_include = 0;
		ESSL_CHECK(push_if_stack_entry(ctx));

		if (cmd == PREPROCESSOR_IF)
		{
			ESSL_CHECK_NONFATAL(directive_constant_expression(ctx, &do_include, 1, 0));
			assert(token_is_end_of_line(peek_pp_token(ctx).tok));
			(void)get_pp_token(ctx);
		}
		else
		{
			Token tok;
			string tokstr;
			tok = read_scanner_token(ctx, &tokstr, IGNORE_WHITESPACE);
			if (!is_identifier(tok))
			{
				SYNTAX_ERROR_STRING("Invalid identifier '%s'\n", tokstr);
				DISCARD_REST_OF_LINE();
				return 0;
			}
			
			do_include = !!_essl_dict_lookup(&ctx->defines, tokstr);

			if (cmd == PREPROCESSOR_IFNDEF)
			{
				do_include = !do_include;
			}

			tok = read_scanner_token(ctx, 0, IGNORE_WHITESPACE);
			if (!token_is_end_of_line(tok))
			{
				SYNTAX_ERROR_STRING("Unexpected text found after #%s directive\n", directive);
				DISCARD_REST_OF_LINE();
			}
		}

		if (do_include)
		{
		}
		else
		{
			ESSL_CHECK_NONFATAL(skip_tokens(ctx, STOP_ON_ENDIF|STOP_ON_TRUE_ELSE));
		}
		return 1;
	}
	case PREPROCESSOR_ELIF:
	case PREPROCESSOR_ELSE:
	{
		
		/* We don't care about the state or the validity of the #elif
		 * bit, we've already encountered the true part of this #if
		 * chain */
		DISCARD_REST_OF_LINE(); 

		if (ctx->if_stack == NULL)
		{
			SYNTAX_ERROR_STRING("#%s directive found outside if-section\n", directive);
			return 0;
		}
		if(ctx->if_stack->state == IF_STATE_ELSE)
		{
			SYNTAX_ERROR_STRING("Illegal use of %s\n", directive);
			DISCARD_REST_OF_LINE();
			return 0;
		}
		if(cmd == PREPROCESSOR_ELSE)
		{
			ctx->if_stack->state = IF_STATE_ELSE;
		}

		ESSL_CHECK_NONFATAL(skip_tokens(ctx, STOP_ON_ENDIF));


		return 1;
	}
	case PREPROCESSOR_ENDIF:
	{
		Token tok;

		if (ctx->if_stack == NULL)
		{
			SYNTAX_ERROR_MSG("#endif directive found outside if-section\n");
			DISCARD_REST_OF_LINE();
			return 0;
		}
		ESSL_CHECK_NONFATAL(pop_if_stack_entry(ctx));

		tok = read_scanner_token(ctx, 0, IGNORE_WHITESPACE);
		if (!token_is_end_of_line(tok))
		{
			SYNTAX_ERROR_MSG("Expected end of line\n");
			DISCARD_REST_OF_LINE();
		}
		return 1;
	}
	case PREPROCESSOR_ERROR:
	{
		char *buf;
		size_t bufsize = 256;
		Token tok;
		string tokstr;
		size_t buf_filled = 0;
		buf = _essl_mempool_alloc(ctx->pool, bufsize + 1);
		ESSL_CHECK_OOM(buf);
		while (1)
		{
			tok = read_scanner_token(ctx, &tokstr, INCLUDE_WHITESPACE);
			if (token_is_end_of_line(tok))
			{
				break;
			}
			assert(tokstr.ptr != 0);
			while ((int)bufsize - (int)buf_filled < tokstr.len)
			{
				char *newbuf;
				bufsize *= 2;
				newbuf = _essl_mempool_alloc(ctx->pool, bufsize + 1);
				ESSL_CHECK_OOM(newbuf);
				memcpy(newbuf, buf, buf_filled);
				buf = newbuf;
			}
			memcpy(&buf[buf_filled], tokstr.ptr, (size_t)tokstr.len);
			buf_filled += tokstr.len;
		}
		buf[buf_filled] = 0;
		ERROR_CODE_PARAM(ERR_PP_USER_ERROR, "#error:%s\n", buf);
		return 0;
	}
	case PREPROCESSOR_PRAGMA:
	{
		char *buf;
		size_t bufsize = 16;
		size_t buf_fill = 0;
		Token tok;
		Token lasttok = TOK_END_OF_FILE;
		string tokstr;
		int source_offset = _essl_preprocessor_get_source_offset(ctx);
		buf = _essl_mempool_alloc(ctx->pool, bufsize);
		ESSL_CHECK_OOM(buf);
		buf[0] = 0;
		tok = read_scanner_token(ctx, &tokstr, IGNORE_WHITESPACE);
		while (1)
		{
			if (token_is_end_of_line(tok))
			{
				break;
			}
			lasttok = tok;
			if (buf_fill + tokstr.len + 1 >= bufsize)
			{
				char *new_buf;
				while (buf_fill + tokstr.len + 1 >= bufsize)
				{
					bufsize *= 2;
				}
				new_buf = _essl_mempool_alloc(ctx->pool, bufsize);
				ESSL_CHECK_OOM(new_buf);
				memcpy(new_buf, buf, buf_fill+1);
				buf = new_buf;
				
			}
			assert(tokstr.ptr != 0);
			strncat(buf, tokstr.ptr, (size_t)tokstr.len);
			buf_fill += tokstr.len;
			buf[buf_fill] = 0;
			tok = read_scanner_token(ctx, &tokstr, INCLUDE_WHITESPACE);
		}

		if (lasttok == TOK_WHITESPACE)
		{
			buf_fill -= 1;
		}

		tokstr.ptr = buf;
		tokstr.len = (int)buf_fill;

		ESSL_CHECK_NONFATAL(_essl_set_pragma(ctx->lang_descriptor, tokstr, source_offset));

		return 1;
	}
	case PREPROCESSOR_EXTENSION:
	{
		string extension_name;
		string str;
		Token tok;
		int source_offset = _essl_preprocessor_get_source_offset(ctx);

		if (ctx->non_preprocessor_token_found)
		{
			/* A non-preprocessor token has already been found! #extension must occur before any such! */
			ERROR_CODE_MSG(ERR_PP_EXTENSION_AFTER_NONPREPROCESSOR_TOKEN, "Extension directive must occur before any non-preprocessor tokens\n");
			DISCARD_REST_OF_LINE();
			return 0;
		}

		tok = read_scanner_token(ctx, &extension_name, IGNORE_WHITESPACE);

		if (tok != TOK_IDENTIFIER)
		{
			SYNTAX_ERROR_STRING("Expected extension name after #extension directive, found '%s'\n", get_friendly_token_name(tok, extension_name));
			DISCARD_REST_OF_LINE();
			return 0;
		}

		tok = read_scanner_token(ctx, &str, IGNORE_WHITESPACE);
		if (tok != TOK_COLON)
		{
			SYNTAX_ERROR_STRING("Expected ':' in #extension directive, found '%s'\n", get_friendly_token_name(tok, str));
			DISCARD_REST_OF_LINE();
			return 0;
		}

		tok = read_scanner_token(ctx, &str, IGNORE_WHITESPACE);

		if (!_essl_string_cmp(str, _essl_cstring_to_string_nocopy("require")))
		{
			if (!_essl_string_cmp(extension_name, _essl_cstring_to_string_nocopy("all")))
			{
				ERROR_CODE_STRING(ERR_PP_EXTENSION_UNSUPPORTED, "Required extensions need to be listed explicitly, '%s' is not allowed\n", extension_name);
				DISCARD_REST_OF_LINE();
				return 0;
			}
			if (!_essl_set_extension(ctx, extension_name, BEHAVIOR_ENABLE, source_offset))
			{
				ERROR_CODE_STRING(ERR_PP_EXTENSION_UNSUPPORTED, "Extension '%s' not supported\n", extension_name);
				DISCARD_REST_OF_LINE();
				return 0;
			}
		}
		else if (!_essl_string_cmp(str, _essl_cstring_to_string_nocopy("enable")))
		{
			if (!_essl_string_cmp(extension_name, _essl_cstring_to_string_nocopy("all")))
			{
				ERROR_CODE_STRING(ERR_PP_EXTENSION_UNSUPPORTED, "Enabled extensions need to be listed explicitly, '%s' is not allowed\n", extension_name);
				DISCARD_REST_OF_LINE();
				return 0;
			}
			if (!_essl_set_extension(ctx, extension_name, BEHAVIOR_ENABLE, source_offset))
			{
				WARNING_CODE_STRING(ERR_PP_EXTENSION_UNSUPPORTED, "Extension '%s' not supported\n", extension_name);
			}
		}
		else if (!_essl_string_cmp(str, _essl_cstring_to_string_nocopy("warn")))
		{
			if (!_essl_set_extension(ctx, extension_name, BEHAVIOR_WARN, source_offset))
			{
				WARNING_CODE_STRING(ERR_PP_EXTENSION_UNSUPPORTED, "Extension '%s' not supported\n", extension_name);
			}
		}
		else if (!_essl_string_cmp(str, _essl_cstring_to_string_nocopy("disable")))
		{
			if (!_essl_set_extension(ctx, extension_name, BEHAVIOR_DISABLE, source_offset))
			{
				WARNING_CODE_STRING(ERR_PP_EXTENSION_UNSUPPORTED, "Extension '%s' not supported\n", extension_name);
			}
		}
		else
		{
			SYNTAX_ERROR_STRING("Unknown extension behavior '%s', expected one of: require, enable, warn, disable\n", get_friendly_token_name(tok, str));
			DISCARD_REST_OF_LINE();
			return 0;
		}

		tok = read_scanner_token(ctx, 0, IGNORE_WHITESPACE);
		if (!token_is_end_of_line(tok))
		{
			SYNTAX_ERROR_MSG("Unexpected text found after #extension directive\n");
			DISCARD_REST_OF_LINE();
		}
		return 1;
	}
	case PREPROCESSOR_VERSION:
	{
		string version_str;
		Token tok;

		if (ctx->lang_descriptor->lang_version != VERSION_UNKNOWN)
		{
			ERROR_CODE_MSG(ERR_PP_VERSION_NOT_FIRST, "#version must be the first directive/statement in a program\n");
		}

		tok = read_scanner_token(ctx, &version_str, IGNORE_WHITESPACE);
		if (token_is_end_of_line(tok))
		{
			SYNTAX_ERROR_MSG("Missing version after #version directive\n");
			return 0;
		}

		ESSL_CHECK_NONFATAL(_essl_set_language_version(ctx->lang_descriptor, version_str, _essl_preprocessor_get_source_offset(ctx)));

		tok = read_scanner_token(ctx, 0, IGNORE_WHITESPACE);
		if (!token_is_end_of_line(tok))
		{
			SYNTAX_ERROR_MSG("Unexpected text found after #version directive\n");
			DISCARD_REST_OF_LINE();
		}

		return 1;
	}
	case PREPROCESSOR_LINE:
	{
		int linenum;
		int sourcestring;
		int *srcptr = 0;
		int source_offset = _essl_preprocessor_get_source_offset(ctx);
		list_ends listends;

		listends.first = 0;
		listends.last = 0;

		ESSL_CHECK_NONFATAL(directive_constant_expression(ctx, &linenum, 0, &listends));

		if (listends.first)
		{
			ESSL_CHECK_NONFATAL(directive_constant_expression(ctx, &sourcestring, 0, &listends));
			srcptr = &sourcestring;
		}
		linenum--;
		ESSL_CHECK_OOM(_essl_error_set_position(ctx->err_context, source_offset, srcptr, &linenum));

		if (listends.first)
		{
			SYNTAX_ERROR_MSG("Unexpected text found after #line directive\n");
			/* no need to DISCARD_REST_OF_LINE(); cos listends contains "rest of line" */
		}

		assert(token_is_end_of_line(peek_pp_token(ctx).tok));
		(void)get_pp_token(ctx);

		return 1;
	}

	default:
		SYNTAX_ERROR_STRING("Unknown preprocessing directive '%s'\n", directive);
		DISCARD_REST_OF_LINE();
		return 0;
	}

}

/** Performs a macro replacement for an object-like macro. */
static list_ends *object_macro_replacement(preprocessor_context *ctx, string macroname, macro_def *def, dict *replaced_by)
{
	/* Object-like macro replacement */
	pp_token_list *start_spot = 0;
	pp_token_list **insertion_spot = &start_spot;
	replacement_list *repl = def->replist;
	pp_token_list *newlist;
	list_ends *listends;
	pp_token_list *last = 0;
	while (repl)
	{
		pp_token tok;
		tok.tok = repl->tok;
		tok.tokstr = repl->tokstr;
		repl = repl->next;
		if (tok.tok == TOK_WHITESPACE)
		{
			continue;
		}

		tok.replaced_by = _essl_mempool_alloc(ctx->pool, sizeof(dict));
		ESSL_CHECK_OOM(tok.replaced_by);
		ESSL_CHECK_OOM(_essl_dict_init(tok.replaced_by, ctx->pool));
		ESSL_CHECK_OOM(_essl_dict_insert(tok.replaced_by, macroname, (void*)1));

		if (replaced_by)
		{
			/* Copy list of macros that have already been used in replacements */
			dict_iter it;
			string oldstr;
			_essl_dict_iter_init(&it, replaced_by);
			while (1)
			{
				oldstr = _essl_dict_next(&it, NULL);
				if (!oldstr.ptr) break;
				ESSL_CHECK_OOM(_essl_dict_insert(tok.replaced_by, oldstr, (void*)1));
			}
		}

		newlist = LIST_NEW(ctx->pool, pp_token_list);
		ESSL_CHECK_OOM(newlist);
		newlist->token = tok;
		LIST_INSERT_FRONT(insertion_spot, newlist);
		last = *insertion_spot;
		insertion_spot = &((*insertion_spot)->next);
	}
	listends = _essl_mempool_alloc(ctx->pool, sizeof(list_ends));
	ESSL_CHECK_OOM(listends);
	listends->first = start_spot;
	listends->last = last;
	return listends;
}

/** Parses a parameter for a function-like macro invocation */
static list_ends *parse_arg(preprocessor_context *ctx, int in_preprocessing_directive)
{
	pp_token pptok;
	int curlevel = 0;
	list_ends *final_result;
	list_ends tokenlist;
	pp_token_list *newlist;

	final_result = _essl_mempool_alloc(ctx->pool, sizeof(list_ends));
	ESSL_CHECK_OOM(final_result);
	final_result->first = 0;
	final_result->last = 0;

	tokenlist.first = 0;
	tokenlist.last = 0;

	do
	{
		pptok = peek_pp_token(ctx);
		if (pptok.tok == TOK_UNKNOWN)
		{
			return NULL;
		}
		else if (pptok.tok == TOK_END_OF_FILE)
		{
			SYNTAX_ERROR_STRING("Unexpected end of file found\n", pptok.tokstr);
			return NULL;
		}

		if (curlevel == 0 && (pptok.tok == TOK_COMMA || pptok.tok == TOK_RIGHT_PAREN))
		{
			break;
		}

		(void)get_pp_token(ctx);
		if (pptok.tok == TOK_NEWLINE)
		{
			if (!in_preprocessing_directive)
			{
				/* Newline counts as normal whitespace in macro invocation arguments */
				continue;
			}
			else
			{
				/* Newline marks end of preprocessing directive */
				SYNTAX_ERROR_MSG("Unterminated argument list\n");
				return 0;
			}
		}


		if (is_identifier(pptok.tok))
		{
			if (in_preprocessing_directive && !_essl_string_cmp(pptok.tokstr, _essl_cstring_to_string_nocopy("defined")))
			{
				ESSL_CHECK_NONFATAL(defined_operator(ctx, &pptok));
			}
		}
		else if (pptok.tok == TOK_LEFT_PAREN)
		{
			curlevel++;
		}
		else if (pptok.tok == TOK_RIGHT_PAREN)
		{
			curlevel--;
		}
		newlist = LIST_NEW(ctx->pool, pp_token_list);
		ESSL_CHECK_OOM(newlist);
		newlist->token = pptok;

		if (!tokenlist.first)
		{
			assert(tokenlist.first == 0 && tokenlist.last == 0);
			tokenlist.first = newlist;
			tokenlist.last = newlist;
		}
		else
		{
			assert(tokenlist.first != 0 && tokenlist.last != 0);
			tokenlist.last->next = newlist;
			tokenlist.last = newlist;
		}
	} while (1);

	assert(tokenlist.first != NULL);

	newlist = LIST_NEW(ctx->pool, pp_token_list);
	ESSL_CHECK_OOM(newlist);
	newlist->token = new_pp_token(TOK_END_OF_FILE, _essl_cstring_to_string_nocopy(""));
	tokenlist.last->next = newlist;
	tokenlist.last = newlist;

	/* Macro expansion */
	{
		pp_token_list *old_list = ctx->remaining_replacements;
		ctx->remaining_replacements = tokenlist.first;

		while (1)
		{
			pptok = get_pp_token(ctx);
			if (pptok.tok == TOK_END_OF_FILE) break;

			if (is_identifier(pptok.tok))
			{

				macro_def *def = _essl_dict_lookup(&ctx->defines, pptok.tokstr);
				if (def && (!pptok.replaced_by || !_essl_dict_has_key(pptok.replaced_by, pptok.tokstr)))
				{
					if (!in_preprocessing_directive)
					{
						Token temptok = peek_pp_token(ctx).tok;
						while (temptok == TOK_NEWLINE)
						{
							(void)get_pp_token(ctx);
							temptok = peek_pp_token(ctx).tok;
						}
					}
					if (!def->args || peek_pp_token(ctx).tok == TOK_LEFT_PAREN)
					{
						list_ends result;
						result.first = 0;
						result.last = 0;
						ESSL_CHECK_NONFATAL(replace_macro(ctx, def, pptok.tokstr, &result, pptok.replaced_by, in_preprocessing_directive));
	
						if (result.last != NULL)
						{
							result.last->next = ctx->remaining_replacements;
							ctx->remaining_replacements = result.first;
						}

						continue;
					}
				}
			}
	
			newlist = LIST_NEW(ctx->pool, pp_token_list);
			ESSL_CHECK_OOM(newlist);
			newlist->token = pptok;
			if (!final_result->first)
			{
				assert(final_result->first == 0 && final_result->last == 0);
				final_result->first = newlist;
				final_result->last = newlist;
			}
			else
			{
				assert(final_result->first != 0 && final_result->last != 0);
				final_result->last->next = newlist;
				final_result->last = newlist;
			}
		}

		ctx->remaining_replacements = old_list;
	}

	return final_result;
}

/** Performs a macro replacement for a function-like macro. */
static list_ends *function_macro_replacement(preprocessor_context *ctx, string macroname, macro_def *def, dict *replaced_by, int in_preprocessing_directive)
{
	dict arg_values;
	list_ends *listends;
	{
		pp_token pptok = get_pp_token(ctx);
		int want_arg = -1;
		replacement_list *argiter = def->args;
		ESSL_CHECK_OOM(_essl_dict_init(&arg_values, ctx->pool));
		assert(pptok.tok == TOK_LEFT_PAREN);
		while (1)
		{
			pptok = peek_pp_token(ctx);

			if (!in_preprocessing_directive)
			{
				while (pptok.tok == TOK_NEWLINE)
				{
					(void)get_pp_token(ctx);
					pptok = peek_pp_token(ctx);
				}
			}

			if (pptok.tok == TOK_UNKNOWN || pptok.tok == TOK_END_OF_FILE || pptok.tok == TOK_RIGHT_PAREN)
			{
				break;
			}
	
			if (want_arg)
			{
				list_ends *argtokens;

				if (pptok.tok == TOK_COMMA)
				{
					SYNTAX_ERROR_STRING("Missing argument before ',' in '%s' macro invocation\n", macroname);
					DISCARD_REST_OF_LINE();
					return NULL;
				}

				if (!argiter || argiter->tok == TOK_UNKNOWN) {
					SYNTAX_ERROR_STRING("Too many arguments for '%s' macro invocation\n", macroname);
					DISCARD_REST_OF_LINE();
					return NULL;
				}

				ESSL_CHECK_NONFATAL(argtokens = parse_arg(ctx, in_preprocessing_directive));
				ESSL_CHECK_OOM(_essl_dict_insert(&arg_values, argiter->tokstr, argtokens));
				argiter = argiter->next;
				want_arg = 0;
			}
			else
			{
				assert(pptok.tok == TOK_COMMA);
				get_pp_token(ctx);
				want_arg = 1;
				continue;
			}
		}
		if (pptok.tok != TOK_RIGHT_PAREN || want_arg == 1 || (argiter != NULL && argiter->tok != TOK_UNKNOWN))
		{
			SYNTAX_ERROR_MSG("Unexpected end of macro invocation\n");
			DISCARD_REST_OF_LINE();
			return NULL;
		}
		get_pp_token(ctx);
	}

	/* Insert the replacement list */
	{
		pp_token_list *start_spot = 0;
		pp_token_list_ptr *insertion_spot = &start_spot;
		replacement_list *repl = def->replist;
		pp_token_list *newlist;
		pp_token_list *last = 0;
		list_ends *arg_toklist;
		while (repl)
		{
			pp_token tok;
			tok.tok = repl->tok;
			tok.tokstr = repl->tokstr;
			repl = repl->next;
			if (tok.tok == TOK_WHITESPACE)
			{
/*				fprintf(stderr, "skipping whitespace replacement: %s -> %s\n", _essl_string_to_cstring(ctx->pool, macroname), _essl_string_to_cstring(ctx->pool, tok.tokstr)); */
				continue;
			}
	
			if (is_identifier(tok.tok) && (arg_toklist = _essl_dict_lookup(&arg_values, tok.tokstr)) != 0)
			{
				pp_token_list *iter;
				for (iter = arg_toklist->first; iter != 0; iter = iter->next)
				{
					pp_token repltok = iter->token;

					repltok.replaced_by = _essl_mempool_alloc(ctx->pool, sizeof(dict));
					ESSL_CHECK_OOM(repltok.replaced_by);
					ESSL_CHECK_OOM(_essl_dict_init(repltok.replaced_by, ctx->pool));
					ESSL_CHECK_OOM(_essl_dict_insert(repltok.replaced_by, macroname, (void*)1));

					if (replaced_by)
					{
						/* Copy list of macros that have already been used in replacements */
						dict_iter it;
						string oldstr;
						_essl_dict_iter_init(&it, replaced_by);
						while (1)
						{
							oldstr = _essl_dict_next(&it, NULL);
							if (!oldstr.ptr) break;
							ESSL_CHECK_OOM(_essl_dict_insert(repltok.replaced_by, oldstr, (void*)1));
						}
					}
					if (iter->token.replaced_by)
					{
						/* Copy list of macros that have already been used in replacements for this token */
						dict_iter it;
						string oldstr;
						_essl_dict_iter_init(&it, iter->token.replaced_by);
						while (1)
						{
							oldstr = _essl_dict_next(&it, NULL);
							if (!oldstr.ptr) break;
							ESSL_CHECK_OOM(_essl_dict_insert(repltok.replaced_by, oldstr, (void*)1));
						}
					}
			
					newlist = LIST_NEW(ctx->pool, pp_token_list);
					ESSL_CHECK_OOM(newlist);
					newlist->token = repltok;
					LIST_INSERT_FRONT(insertion_spot, newlist);
					last = *insertion_spot;
					insertion_spot = &((*insertion_spot)->next);
			/*		fprintf(stderr, "macro replacement: %s -> %s\n", _essl_string_to_cstring(ctx->pool, macroname), _essl_string_to_cstring(ctx->pool, tok.tokstr)); */
				}
			}
			else
			{
				tok.replaced_by = _essl_mempool_alloc(ctx->pool, sizeof(dict));
				ESSL_CHECK_OOM(tok.replaced_by);
				ESSL_CHECK_OOM(_essl_dict_init(tok.replaced_by, ctx->pool));
				ESSL_CHECK_OOM(_essl_dict_insert(tok.replaced_by, macroname, (void*)1));

				if (replaced_by)
				{
					/* Copy list of macros that have already been used in replacements */
					dict_iter it;
					string oldstr;
					_essl_dict_iter_init(&it, replaced_by);
					while (1)
					{
						oldstr = _essl_dict_next(&it, NULL);
						if (!oldstr.ptr) break;
						ESSL_CHECK_OOM(_essl_dict_insert(tok.replaced_by, oldstr, (void*)1));
					}
				}
		
				newlist = LIST_NEW(ctx->pool, pp_token_list);
				ESSL_CHECK_OOM(newlist);
				newlist->token = tok;
				LIST_INSERT_FRONT(insertion_spot, newlist);
				last = *insertion_spot;
				insertion_spot = &((*insertion_spot)->next);
			}
		}
		listends = _essl_mempool_alloc(ctx->pool, sizeof(list_ends));
		ESSL_CHECK_OOM(listends);
		listends->first = start_spot;
		listends->last = last;
	}

	return listends;
}

memerr _essl_preprocessor_init(preprocessor_context *ctx, mempool *pool, error_context *e_ctx, scanner_context *s_ctx, language_descriptor *lang_desc)
{
	ctx->prev_token = TOK_UNKNOWN;
	ctx->prev_string.ptr = "<dummy>";
	ctx->prev_string.len = 7;
	ctx->position_type = -1;
	ctx->pool = pool;
	ctx->scan_context = s_ctx;
	ctx->err_context = e_ctx;
	ctx->lang_descriptor = lang_desc;
	ctx->remaining_replacements = 0;
	ctx->if_stack = NULL;
	ctx->non_preprocessor_token_found = ESSL_FALSE;
	ctx->source_body_start = 0;

	ESSL_CHECK_OOM(_essl_dict_init(&ctx->defines, ctx->pool));

	ESSL_CHECK_OOM(add_predefined_macro(ctx, "__LINE__", PREDEFINED_LINE));
	ESSL_CHECK_OOM(add_predefined_macro(ctx, "__FILE__", PREDEFINED_FILE));
	ESSL_CHECK_OOM(add_predefined_macro(ctx, "__VERSION__", PREDEFINED_VERSION));
	ESSL_CHECK_OOM(add_predefined_macro(ctx, "GL_ES", PREDEFINED_GL_ES));
	ESSL_CHECK_OOM(add_predefined_macro(ctx, "__ARM_MALI__", PREDEFINED_MALI));
	ESSL_CHECK_OOM(add_predefined_macro(ctx, "__ARM_MALI_HW_REV_MAJOR__", PREDEFINED_MALI_HW_REV_MAJOR));
	ESSL_CHECK_OOM(add_predefined_macro(ctx, "__ARM_MALI_HW_REV_MINOR__", PREDEFINED_MALI_HW_REV_MINOR));


	if(lang_desc->target_desc->fragment_side_has_high_precision)
	{
	   ESSL_CHECK_OOM(add_predefined_macro(ctx, "GL_FRAGMENT_PRECISION_HIGH", PREDEFINED_GL_FRAGMENT_PRECISION_HIGH));
	}

	ESSL_CHECK_OOM(_essl_load_extension_macros(ctx));

	return MEM_OK;
}

int _essl_preprocessor_get_source_offset(preprocessor_context *ctx)
{
	return _essl_scanner_get_source_offset(ctx->scan_context);
}

/* Mark that we have encountered a non-version token */
static void set_default_language_version(preprocessor_context *ctx)
{
	if (_essl_preprocessor_get_source_offset(ctx) > ctx->source_body_start)
	{
		if (ctx->lang_descriptor->lang_version == VERSION_UNKNOWN)
		{
			ctx->lang_descriptor->lang_version = VERSION_DEFAULT;
		}
	}
}

Token _essl_preprocessor_get_token(preprocessor_context *ctx, string *token_str, whitespace_handling whitespace)
{
	pp_token pptok;

	if (ctx->position_type != 0)
	{
		/* At start of line */
		pptok.tok = read_scanner_token(ctx, &pptok.tokstr, IGNORE_WHITESPACE);
		pptok.replaced_by = 0;
		if (pptok.tok == TOK_HASH)
		{
			/* Preprocessing directive */
			pptok.tok = read_scanner_token(ctx, &pptok.tokstr, IGNORE_WHITESPACE);
			if (pptok.tok == TOK_NEWLINE)
			{
				ctx->position_type = 1;
				return _essl_preprocessor_get_token(ctx, token_str, whitespace);
			}

			if (!handle_directive(ctx, pptok.tokstr))
			{
				/* Error occured */
				ctx->position_type = 1;
				set_default_language_version(ctx);
				return _essl_preprocessor_get_token(ctx, token_str, whitespace);
			}
			else
			{
				set_default_language_version(ctx);
				return _essl_preprocessor_get_token(ctx, token_str, whitespace);
			}
		}
		ctx->position_type = 0;
	}
	else
	{
		pptok = get_pp_token(ctx);
	}

	if (pptok.tok == TOK_NEWLINE)
	{
		ctx->position_type = 1;
		if (whitespace != INCLUDE_NEWLINE)
		{
			return _essl_preprocessor_get_token(ctx, token_str, whitespace);
		}
	}
	else
	{
		set_default_language_version(ctx);
		if (is_identifier(pptok.tok))
		{
			macro_def *def = _essl_dict_lookup(&ctx->defines, pptok.tokstr);
			if (def && (!pptok.replaced_by || !_essl_dict_has_key(pptok.replaced_by, pptok.tokstr)))
			{
				int is_invocation = 0;
				if (def->args)
				{
					while (peek_pp_token(ctx).tok == TOK_NEWLINE)
					{
						ctx->position_type = 1;
						(void)get_pp_token(ctx);
					}
					if (peek_pp_token(ctx).tok == TOK_LEFT_PAREN)
					{
						is_invocation = 1;
						ctx->position_type = 0;
					}
				}
				else
				{
					is_invocation = 1;
				}
				if (is_invocation)
				{
					list_ends result;
					result.first = 0;
					result.last = 0;
					if (!replace_macro(ctx, def, pptok.tokstr, &result, pptok.replaced_by, 0))
					{
						/* Error occured */
						return _essl_preprocessor_get_token(ctx, token_str, whitespace);
					}
					if (result.last != NULL)
					{
						result.last->next = ctx->remaining_replacements;
						ctx->remaining_replacements = result.first;
					}

					return _essl_preprocessor_get_token(ctx, token_str, whitespace);
				}
			}
		}
	}

	if(pptok.tok == TOK_END_OF_FILE)
	{
		if(ctx->if_stack != NULL)
		{
			SYNTAX_ERROR_MSG("Unterminated #if/#ifdef/#ifndef\n");

		}
	}

	if (token_str) *token_str = pptok.tokstr;
	return pptok.tok;
}


/**
	Calculates a divided by b (round towards zero)
	Divisor of 0 is not allowed.
	@param a Dividend
	@param n Divisor
	@return Result of a divided by b
*/
int divide(int a, int n)
{
	unsigned int absa;
	unsigned int absn;
	unsigned int res;

	assert(n != 0);

	/* Integer divisions is converted to positive numbers, so we ensure we round towards 0 on all platforms. */
	if (a < 0)
	{
		absa = -a;
	}
	else
	{
		absa = a;
	}

	if (n < 0)
	{
		absn = -n;
	}
	else
	{
		absn = n;
	}

	res = absa / absn;

	/* if one, and only one of the components was unsigned */
	if ((a < 0 && n >= 0) || (n < 0 && a >= 0))
	{
		return -(int)res;
	}

	/* none or both of the components was unsigned */
	return (int)res;
}


/**
	Calculates the modulo
	Implemented to match the behavior defined in C99.
	Result has the same sign as the dividend (a).
	Divisor of 0 is not allowed.
	@param a Dividend
	@param n Divisor
	@return Result of modulo operation
*/
int modulo(int a, int n)
{
	unsigned int absa;
	unsigned int absn;
	unsigned int res;

	assert(n != 0);

	/* Integer divisions is converted to positive numbers, so we ensure we round towards 0 on all platforms. */
	if (a < 0)
	{
		absa = -a;
	}
	else
	{
		absa = a;
	}

	if (n < 0)
	{
		absn = -n;
	}
	else
	{
		absn = n;
	}

	res = absa - (absa / absn) * absn;

	if (a < 0)
	{
		return -(int)res;
	}

	return (int)res;
}




#ifdef UNIT_TEST


int main(void)
{
	typedef struct {
		Token t;
		const char *str;
	} FullToken;
	typedef struct {
		const char *str;
		FullToken tokens[100];
		FullToken args[40];
	} FullReplist;
	typedef struct {
		const char *input_string;

		const char *error_log;
		FullToken tokens[1000];
		FullReplist lists[10];
	} test_case;

	test_case test_cases[] = 
		{
			{
				"abc  = b + d * e;",
				"",
				{ { TOK_IDENTIFIER, "abc"},
				  { TOK_ASSIGN, "="},
				  { TOK_IDENTIFIER, "b"},
				  { TOK_PLUS, "+"},
				  { TOK_IDENTIFIER, "d"},
				  { TOK_STAR, "*"},
				  { TOK_IDENTIFIER, "e"},
				  { TOK_SEMICOLON, ";"},
				  { TOK_END_OF_FILE, ""}
				},
			},
			{
				"0 1 00 0.0 .0 1e4 1.0e5 0X45ABad 0x1337",
				"",
				{ { TOK_INTCONSTANT, "0"},
				  { TOK_INTCONSTANT, "1"},
				  { TOK_INTCONSTANT, "00"},
				  { TOK_FLOATCONSTANT, "0.0"},
				  { TOK_FLOATCONSTANT, ".0"},
				  { TOK_FLOATCONSTANT, "1e4"},
				  { TOK_FLOATCONSTANT, "1.0e5"},
				  { TOK_INTCONSTANT, "0X45ABad"},
				  { TOK_INTCONSTANT, "0x1337"},
				  { TOK_END_OF_FILE, ""}				  
				}
			},
			{
				 "return vec4(a, b, c, d);",
				"",
				{ { TOK_RETURN, "return"},
				  { TOK_VEC4, "vec4"},
				  { TOK_LEFT_PAREN, 0},
				  { TOK_IDENTIFIER, "a"},
				  { TOK_COMMA, 0},
				  { TOK_IDENTIFIER, "b"},
				  { TOK_COMMA, 0},
				  { TOK_IDENTIFIER, "c"},
				  { TOK_COMMA, 0},
				  { TOK_IDENTIFIER, "d"},
				  { TOK_RIGHT_PAREN, 0},
				  { TOK_SEMICOLON, 0},
				  { TOK_END_OF_FILE, ""},
				}
			},
			{
				"true false",
				"",
				{ { TOK_TRUE, "true" },
				  { TOK_FALSE, "false" },
				  { TOK_END_OF_FILE, "" },
				}
			},
			{
				"precision invariant /* comment */ mediump lowp",
				"",
				{
					{ TOK_PRECISION, "precision" },
					{ TOK_INVARIANT, "invariant" },
					{ TOK_MEDIUMP, "mediump" },
					{ TOK_LOWP, "lowp" },
					{ TOK_END_OF_FILE, "" },
				}
			},
			{
				"a // comment \n//more comment\n\n \n b",
				"",
				{
					{ TOK_IDENTIFIER, "a" },
					{ TOK_IDENTIFIER, "b" },
					{ TOK_END_OF_FILE, "" },
				}
			},

			{
				"a/*moo*/b",
				"",
				{
					{ TOK_IDENTIFIER, "a" },
					{ TOK_IDENTIFIER, "b" },
					{ TOK_END_OF_FILE, "" },
				}
			},

			{
				"   # \n #\n",
				"",
				{
					{ TOK_END_OF_FILE, "" },
				}
			},

			{
				"   # version   100   \n #\n",
				"",
				{
					{ TOK_END_OF_FILE, "" },
				}
			},

			{
				"   # version   101   \n #\n",
				"",
				{
					{ TOK_END_OF_FILE, "" },
				}
			},

			{
				"# version 200",
				"0:1: P0003: Language version '200' unknown, this compiler only supports up to version 101\n",
				{
					{ TOK_END_OF_FILE, "" },
				}
			},

			{
				"#error This program does not   work\n555 =apples",
				"0:1: P0002: #error: This program does not work\n",
				{
					{ TOK_INTCONSTANT, "555" },
					{ TOK_ASSIGN, "="},
					{ TOK_IDENTIFIER, "apples" },
					{ TOK_END_OF_FILE, "" },
				}
			},

			{
				"#define stuff the moon( is,made of, cheddar)",
				"",
				{
					{ TOK_END_OF_FILE, "" },
				},
				{
					{
						"stuff",
						{
							{ TOK_IDENTIFIER, "the" },
							{ TOK_WHITESPACE, " " },
							{ TOK_IDENTIFIER, "moon" },
							{ TOK_LEFT_PAREN, "(" },
							{ TOK_WHITESPACE, " " },
							{ TOK_IDENTIFIER, "is" },
							{ TOK_COMMA, "," },
							{ TOK_IDENTIFIER, "made" },
							{ TOK_WHITESPACE, " " },
							{ TOK_IDENTIFIER, "of" },
							{ TOK_COMMA, "," },
							{ TOK_WHITESPACE, " " },
							{ TOK_IDENTIFIER, "cheddar" },
							{ TOK_RIGHT_PAREN, ")" },
						}
					}
				}
			},

			{
				"#define kill(gibber) call(stuff, gibber)",
				"",
				{
					{ TOK_END_OF_FILE, ""},
				},
				{
					{
						"kill",
						{
							{ TOK_IDENTIFIER, "call" },
							{ TOK_LEFT_PAREN, "(" },
							{ TOK_IDENTIFIER, "stuff" },
							{ TOK_COMMA, "," },
							{ TOK_WHITESPACE, " " },
							{ TOK_IDENTIFIER, "gibber" },
							{ TOK_RIGHT_PAREN, ")" },
						},
						{
							{ TOK_IDENTIFIER, "gibber" },
						}
					}
				}
			},

			{
				"#define silly(a, b,a) nope\n",
				"0:1: P0001: Token 'a' repeated in argument list\n",
				{
					{ TOK_END_OF_FILE, ""},
				},
			},

			{
				"#define silly [a, b]\n#define silly [ a, b ]",
				"0:2: P0001: Macro 'silly' redefined\n",
				{
					{ TOK_END_OF_FILE, ""},
				},
			},

			{
				"#define redef [/*moo*/a, b ]\n#define redef [ a, b ]\n\n",
				"",
				{
					{ TOK_END_OF_FILE, ""},
				},
			},

			{
				"#define repl new stuff\nTest of repl replacement\n",
				"",
				{
					{ TOK_IDENTIFIER, "Test" },
					{ TOK_IDENTIFIER, "of" },
					{ TOK_IDENTIFIER, "new" },
					{ TOK_IDENTIFIER, "stuff" },
					{ TOK_IDENTIFIER, "replacement" },
					{ TOK_END_OF_FILE, ""},
				},
			},

			{
				"#define self replacing self\nTest of self replacement\n",
				"",
				{
					{ TOK_IDENTIFIER, "Test" },
					{ TOK_IDENTIFIER, "of" },
					{ TOK_IDENTIFIER, "replacing" },
					{ TOK_IDENTIFIER, "self" },
					{ TOK_IDENTIFIER, "replacement" },
					{ TOK_END_OF_FILE, ""},
				},
			},

			{
				"#define max(a, b) ((a) > (b) ? (a) : (b))\n",
				"",
				{
					{ TOK_END_OF_FILE, ""},
				},
				{
					{
						"max",
						{
							{ TOK_LEFT_PAREN, "(" },
							{ TOK_LEFT_PAREN, "(" },
							{ TOK_IDENTIFIER, "a" },
							{ TOK_RIGHT_PAREN, ")" },
							{ TOK_WHITESPACE, " " },
							{ TOK_RIGHT_ANGLE, ">" },
							{ TOK_WHITESPACE, " " },
							{ TOK_LEFT_PAREN, "(" },
							{ TOK_IDENTIFIER, "b" },
							{ TOK_RIGHT_PAREN, ")" },
							{ TOK_WHITESPACE, " " },
							{ TOK_QUESTION, "?" },
							{ TOK_WHITESPACE, " " },
							{ TOK_LEFT_PAREN, "(" },
							{ TOK_IDENTIFIER, "a" },
							{ TOK_RIGHT_PAREN, ")" },
							{ TOK_WHITESPACE, " " },
							{ TOK_COLON, ":" },
							{ TOK_WHITESPACE, " " },
							{ TOK_LEFT_PAREN, "(" },
							{ TOK_IDENTIFIER, "b" },
							{ TOK_RIGHT_PAREN, ")" },
							{ TOK_RIGHT_PAREN, ")" },
						},
						{
							{ TOK_IDENTIFIER, "a" },
							{ TOK_IDENTIFIER, "b" },
						}
					}
				}
			},

			{
				"#define max(a, b) ((a) > (b) ? (a) : (b))\nb = max(5, a)\n",
				"",
				{
					{ TOK_IDENTIFIER, "b" },
					{ TOK_ASSIGN, "=" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "5" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_ANGLE, ">" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_IDENTIFIER, "a" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_QUESTION, "?" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "5" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_COLON, ":" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_IDENTIFIER, "a" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_END_OF_FILE, ""},
				},
			},

			{
				"#define x    3\n#define f(a) f(x * (a))\n#undef  x\n#define x    2\n#define g    f\n#define z    z[0]\n#define h    g(~\n#define m(a) a(w)\n#define w    0,1\n#define t(a) a\n\nf(y+1) + f(f(z)) % t(t(g)(0) + t)(1);\ng(x+(3,4)-w) | h 5) & m\n        (f)^m(m);\n",
				"",
				{
/*
f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);
f(2 * (2+(3,4)-0,1)) | f(2 * (~ 5)) & f(2 * (0,1))^m(0,1);
*/

					{ TOK_IDENTIFIER, "f" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "2" },
					{ TOK_STAR, "*" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_IDENTIFIER, "y" },
					{ TOK_PLUS, "+" },
					{ TOK_INTCONSTANT, "1" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_PAREN, ")" },

					{ TOK_PLUS, "+" },

					{ TOK_IDENTIFIER, "f" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "2" },
					{ TOK_STAR, "*" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_IDENTIFIER, "f" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "2" },
					{ TOK_STAR, "*" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_IDENTIFIER, "z" },
					{ TOK_LEFT_BRACKET, "[" },
					{ TOK_INTCONSTANT, "0" },
					{ TOK_RIGHT_BRACKET, "]" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_PAREN, ")" },

					{ TOK_PERCENT, "%" },

					{ TOK_IDENTIFIER, "f" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "2" },
					{ TOK_STAR, "*" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "0" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_PLUS, "+" },
					{ TOK_IDENTIFIER, "t" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "1" },
					{ TOK_RIGHT_PAREN, ")" },

					{ TOK_SEMICOLON, ";" },


					{ TOK_IDENTIFIER, "f" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "2" },
					{ TOK_STAR, "*" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "2" },
					{ TOK_PLUS, "+" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "3" },
					{ TOK_COMMA, "," },
					{ TOK_INTCONSTANT, "4" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_DASH, "-" },
					{ TOK_INTCONSTANT, "0" },
					{ TOK_COMMA, "," },
					{ TOK_INTCONSTANT, "1" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_PAREN, ")" },

					{ TOK_VERTICAL_BAR, "|" },

					{ TOK_IDENTIFIER, "f" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "2" },
					{ TOK_STAR, "*" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_TILDE, "~" },
					{ TOK_INTCONSTANT, "5" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_PAREN, ")" },

					{ TOK_AMPERSAND, "&" },

					{ TOK_IDENTIFIER, "f" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "2" },
					{ TOK_STAR, "*" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "0" },
					{ TOK_COMMA, "," },
					{ TOK_INTCONSTANT, "1" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_RIGHT_PAREN, ")" },
					{ TOK_CARET, "^" },
					{ TOK_IDENTIFIER, "m" },
					{ TOK_LEFT_PAREN, "(" },
					{ TOK_INTCONSTANT, "0" },
					{ TOK_COMMA, "," },
					{ TOK_INTCONSTANT, "1" },
					{ TOK_RIGHT_PAREN, ")" },

					{ TOK_SEMICOLON, ";" },

					{ TOK_END_OF_FILE, ""},
				},
			},
		};

	int n_test_cases = sizeof(test_cases)/sizeof(test_cases[0]);
	int i;
	scanner_context scan;
	preprocessor_context context;
	mempool_tracker tracker;
	mempool pool;
	error_context err;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	for(i = 0; i < n_test_cases; ++i)
	{
		test_case *tc = &test_cases[i];
		Token t;
		FullToken *expected_token = &tc->tokens[0];
		string s;
		int lens[1];
		char *error_log;
		compiler_options* opts;
		target_descriptor *trgdesc;
		language_descriptor *lang_desc;

		opts = _essl_new_compiler_options(&pool);
		trgdesc = _essl_new_target_descriptor(&pool, TARGET_FRAGMENT_SHADER, opts);
		assert(trgdesc);

		lang_desc = _essl_create_language_descriptor(&pool, &err, trgdesc);
		assert(lang_desc);

		lens[0] = strlen(tc->input_string);

		_essl_error_init(&err, &pool, tc->input_string, &lens[0], 1);
		_essl_scanner_init(&scan, &pool, &err, tc->input_string, &lens[0], 1);
		_essl_preprocessor_init(&context, &pool, &err, &scan, lang_desc);

		do {
			t = _essl_preprocessor_get_token(&context, &s, IGNORE_NEWLINE);
			if (t != expected_token->t || (expected_token->str && (!s.ptr || strcmp(_essl_string_to_cstring(&pool, s), expected_token->str))))
			{
				fprintf(stderr, "\nTest %d: [%s]\n", i, tc->input_string);
				fprintf(stderr, "Test %d: Expected token '%s', got '%s'\n", i, _essl_token_to_str(expected_token->t), _essl_token_to_str(t));
				if (expected_token->str)
				{
					fprintf(stderr, "Test %d: Expected string '%s', got '%s'\n", i, expected_token->str, _essl_string_to_cstring(&pool, s));
				}
				assert(t == expected_token->t);
				assert(!expected_token->str || (!s.ptr || !strcmp(_essl_string_to_cstring(&pool, s), expected_token->str)));
			}

			++expected_token;

		} while(t != TOK_END_OF_FILE);

		do {
			int j;
			for(j = 0; j < sizeof(test_cases[i].lists)/sizeof(test_cases[i].lists[0]); ++j)
			{
				if (test_cases[i].lists[j].str)
				{
					const char *str = test_cases[i].lists[j].str;
					macro_def *def = _essl_dict_lookup(&context.defines, _essl_cstring_to_string_nocopy(str));
					replacement_list *repitem = 0;
					replacement_list *argitem = 0;
					string identifier;
					int is_ok = 1;
					int k;
					if (def)
					{
						repitem = def->replist;
						argitem = def->args;
						identifier = def->identifier;
					}
					else
					{
						is_ok = 0;
					}
					for(k = 0; is_ok && k < sizeof(test_cases[i].lists[j].tokens)/sizeof(test_cases[i].lists[j].tokens[0]); ++k)
					{
						if (test_cases[i].lists[j].tokens[k].str)
						{
							expected_token = &test_cases[i].lists[j].tokens[k];
							if (!repitem)
							{
								fprintf(stderr, "\n");
								fprintf(stderr, "Test %d: Expected token '%s', got end of replacements\n", i, _essl_token_to_str(expected_token->t));
								fprintf(stderr, "Test %d: Expected string '%s', got end of replacements\n", i, expected_token->str);
								is_ok = 0;
							}
							else
							{
								t = repitem->tok;
								s = repitem->tokstr;
								repitem = repitem->next;
								if (t != expected_token->t || ((s.ptr || expected_token->str) && strcmp(_essl_string_to_cstring(&pool, s), expected_token->str)))
								{
									fprintf(stderr, "\n");
									fprintf(stderr, "Test %d: Expected token '%s', got '%s'\n", i, _essl_token_to_str(expected_token->t), _essl_token_to_str(t));
									fprintf(stderr, "Test %d: Expected string '%s', got '%s'\n", i, expected_token->str, _essl_string_to_cstring(&pool, s));
									is_ok = 0;
								}
							}
						}
						if (test_cases[i].lists[j].args[k].str)
						{
							expected_token = &test_cases[i].lists[j].args[k];
							if (!argitem)
							{
								fprintf(stderr, "\n");
								fprintf(stderr, "Test %d: Expected replacement token '%s', got end of args\n", i, _essl_token_to_str(expected_token->t));
								fprintf(stderr, "Test %d: Expected string '%s', got end of args\n", i, expected_token->str);
								is_ok = 0;
							}
							else
							{
								t = argitem->tok;
								s = argitem->tokstr;
								argitem = argitem->next;
								if (t != expected_token->t || ((s.ptr || expected_token->str) && strcmp(_essl_string_to_cstring(&pool, s), expected_token->str)))
								{
									fprintf(stderr, "\nTest %d: [%s]\n", i, tc->input_string);
									fprintf(stderr, "Test %d: Expected param token '%s', got '%s'\n", i, _essl_token_to_str(expected_token->t), _essl_token_to_str(t));
									fprintf(stderr, "Test %d: Expected string '%s', got '%s'\n", i, expected_token->str, _essl_string_to_cstring(&pool, s));
									is_ok = 0;
								}
							}
						}
					}
					if (!is_ok)
					{
						fprintf(stderr, "Test %d: [%s]\n", i, tc->input_string);
						fprintf(stderr, "Failed to match '%s' in test case:\n", str);
						if (def)
						{
							fprintf(stderr, "Dumping '%s' macro:\n", _essl_string_to_cstring(&pool, identifier));
							repitem = def->args;
							for(; repitem; repitem = repitem->next)
							{
								fprintf(stderr, "  Param token '%s': '%s'\n", _essl_token_to_str(repitem->tok), _essl_string_to_cstring(context.pool, repitem->tokstr));
							}
							repitem = def->replist;
							for(; repitem; repitem = repitem->next)
							{
								fprintf(stderr, "  Replacement token '%s': '%s'\n", _essl_token_to_str(repitem->tok), _essl_string_to_cstring(context.pool, repitem->tokstr));
							}
						} else {
							fprintf(stderr, "  no macro found\n");
						}
						assert(0);
					}
				}
			}
		} while (0);

		error_log = _essl_mempool_alloc(&pool, _essl_error_get_log_size(context.err_context));
		assert(error_log);
		_essl_error_get_log(context.err_context, error_log, _essl_error_get_log_size(context.err_context));

		if (strcmp(tc->error_log, error_log))
		{
			fprintf(stderr, "\nTest %d: [%s]\n", i, tc->input_string);
			fprintf(stderr, "\nExpected error log:\n%s\n", tc->error_log);
			fprintf(stderr, "\nActual error log:\n%s\n", error_log);
			assert(!strcmp(tc->error_log, error_log));
		}
	}
	_essl_mempool_destroy(&pool);
	fprintf(stderr, "All tests OK!\n");
	return 0;
}


#endif /* UNIT_TEST */
