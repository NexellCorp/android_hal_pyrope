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
/*#define DEBUG_PARSING*/

#ifdef UNIT_TEST
#include "frontend/frontend.h"
#endif /* UNIT_TEST */

#include "frontend/token.h"
#include "frontend/parser.h"
#include "common/essl_node.h"
#include "common/error_reporting.h"
#include "common/essl_common.h"
#include "frontend/data_type.h"
#include "common/essl_profiling.h"


/** typedef to tell splint that the pointer can be null */
typedef /*@null@*/ node *node_ptr;


static node *expression(parser_context *ctx);
static node *assignment_expression(parser_context *ctx);
static node *constant_expression(parser_context *ctx);
static node *simple_statement(parser_context *ctx, /*@null@*/ node *parent);
static int load_builtin_variables(parser_context *ctx);
static int load_builtin_functions(parser_context *ctx);

/** Scope creation flags used as input for statement parsing */
typedef enum 
{
	SCOPE_COMPOUND_CREATE, /** Create scope if the statement is a compound statement */
	SCOPE_MUST_KEEP  /** Don't create a new scope */
} scope_creation_kind;

/*@null@*/ static node *statement(parser_context *ctx, scope_creation_kind new_scope, /*@null@*/ node *parent);
/*@null@*/ static node *declaration(parser_context *ctx, /*@null@*/ node *parent);



/** flag that tells specified_type() what kind of type to parse */
typedef enum 
{
	TYPESPEC_UNKNOWN,
	TYPESPEC_CONSTRUCTOR_IDENTIFIER,
	TYPESPEC_PARAMETER_DECLARATION,
	TYPESPEC_FULLY_SPECIFIED_TYPE,
	TYPESPEC_TYPE_SPECIFIER
} type_specifier_kind;

/*@null@*/ static type_specifier *specified_type(parser_context *ctx, type_specifier_kind spectype, qualifier_set *ret_qual);


#ifdef DEBUG_PARSING
#define LOGMSG(a) fprintf(stderr, "%s (%d)\n", a, __LINE__); fflush(stderr)
#define LOGPRINT(a, b) fprintf(stderr, a, b); fflush(stderr)
#else
#define LOGMSG(a)
#define LOGPRINT(a, b)
#endif

/*
  The call macros are used for encapsulating calling functions with a context parameter and propagating the possible error condition.
*/

#define CALL(res, fun) if(!(res = fun(ctx))) return 0

#define CALL_PARAM(res, fun, param) if((res = fun(ctx, param)) == 0) return 0

#define CALL_TWOPARAMS(res, fun, param1, param2) if((res = fun(ctx, param1, param2)) == 0) return 0

#define CALL_THREEPARAMS(res, fun, param1, param2, param3) if(!(res = fun(ctx, param1, param2, param3))) return 0

#define CALL_FOURPARAMS(res, fun, param1, param2, param3, param4) if(!(res = fun(ctx, param1, param2, param3, param4))) return 0


/*
   Handy macros for emitting error messages
*/

/** Code to access the error context for this module */
#define ERROR_CONTEXT ctx->err_context

/** Code to retrieve the source offset for this module */
#define SOURCE_OFFSET _essl_preprocessor_get_source_offset(ctx->prep_context)

/** Error code to use for syntax errors in this module */
#define SYNTAX_ERROR_CODE ERR_LP_SYNTAX_ERROR

#define SYNTAX_ERROR_TOKEN(a, b) ERROR_CODE_TWOPARAMS(ERR_LP_SYNTAX_ERROR, "Expected token '%s', found '%s'\n", _essl_token_to_str(a), _essl_token_to_str(b))



/*
   macros for getting a token and reporting an error if the wrong kind of token is returned
*/
#ifdef DEBUG_PARSING
#define MATCH(token) \
if (peek_token(ctx, NULL) != token) { \
	fprintf(stderr, "Unexpected token, found '%s'\n", _essl_token_to_str(get_token(ctx, NULL))); \
	fprintf(stderr, "   expected '%s'\n", _essl_token_to_str(token)); \
	fprintf(stderr, "   [line %d]\n", __LINE__); \
	return 0; \
} \
else \
{ \
	get_token(ctx, NULL); \
}
#else
#define MATCH(token) { Token _match_token; if ((_match_token = get_token(ctx, NULL)) != token) { SYNTAX_ERROR_TOKEN(token, _match_token); return 0; } }
#endif

#define MATCH_TEXT(token, toktext) { Token _match_token; if ((_match_token = get_token(ctx, toktext)) != token) { SYNTAX_ERROR_TOKEN(token, _match_token); return 0; } }
#define MATCH_OR_UNWIND_SCOPE(token, oldscope) { Token _match_token; if ((_match_token = get_token(ctx, NULL)) != token) { SYNTAX_ERROR_TOKEN(token, _match_token); ctx->current_scope = oldscope; return 0; } }




/* eat a token, it's an internal error if the token is not of the expected kind */
#define MUST_MATCH(token) if (get_token(ctx, NULL) != token) { assert(0 && "MUST_MATCH(" #token ")"); }



/* update a node with the current input string position */
#define SET_NODE_POSITION(_node) _essl_set_node_position(_node, _essl_preprocessor_get_source_offset(ctx->prep_context))



#define DISCARD_REST_OF_LINE() do { while (1) { Token tok = get_token(ctx, NULL); if (tok == TOK_NEWLINE || tok == TOK_END_OF_FILE) break; } } while (0)

#define DISCARD_REST_OF_COMPOUND() do { while (1) { Token tok = get_token(ctx, NULL); if (tok == TOK_RIGHT_BRACE || tok == TOK_END_OF_FILE) break; } } while (0)


static Token get_fresh_token(parser_context *ctx, /*@null@*/string *s)
{
	Token ret = _essl_preprocessor_get_token(ctx->prep_context, s, IGNORE_NEWLINE);
	if (!ctx->prep_context->non_preprocessor_token_found)
	{
		ctx->prep_context->non_preprocessor_token_found = ESSL_TRUE;
		TIME_PROFILE_START("-builtin_functions");
		if(!load_builtin_functions(ctx))
		{
			INTERNAL_ERROR_OUT_OF_MEMORY();
			return TOK_END_OF_FILE;
		}
		TIME_PROFILE_STOP("-builtin_functions");
		TIME_PROFILE_START("-builtin_variables");
		if(!load_builtin_variables(ctx))
		{
			INTERNAL_ERROR_OUT_OF_MEMORY();
			return TOK_END_OF_FILE;
		}
		TIME_PROFILE_STOP("-builtin_variables");

		if(!_essl_scanner_load_extension_keywords(ctx->prep_context->scan_context, ctx->lang_desc))
		{
			INTERNAL_ERROR_OUT_OF_MEMORY();
			return TOK_END_OF_FILE;
		}

	}

	return ret;
}

/** 
Retrieves a new token, either by using a previous lookahead or fetching a fresh token from the preprocessor .

*/
static Token get_token(parser_context *ctx, /*@null@*/string *s)
{
	Token tok;
	if (ctx->prev_token2 != TOK_UNKNOWN)
	{
		Token t = ctx->prev_token2;
		if(s) *s = ctx->prev_string2;
		ctx->prev_token2 = TOK_UNKNOWN;
#ifdef DEBUG_PARSING
		fprintf(stderr, "  eating %s\n", _essl_token_to_str(t));
#endif
		return t;
	}
	if (ctx->prev_token != TOK_UNKNOWN)
	{
		Token t = ctx->prev_token;
		if(s) *s = ctx->prev_string;
		ctx->prev_token = TOK_UNKNOWN;
#ifdef DEBUG_PARSING
		fprintf(stderr, "  eating %s\n", _essl_token_to_str(t));
#endif
		return t;
	}
	tok = get_fresh_token(ctx, s);
#ifdef DEBUG_PARSING
	fprintf(stderr, "  eating %s\n", _essl_token_to_str(tok));
#endif
	return tok;
}


/**
   Fetches the next token in the token stream, but does not remove it from the stream
*/

static Token peek_token(parser_context *ctx, /*@null@*/string *s)
{
	if (ctx->prev_token2 != TOK_UNKNOWN)
	{
		if(s) *s = ctx->prev_string2;
		return ctx->prev_token2;
	}
	if(ctx->prev_token == TOK_UNKNOWN)
	{
		ctx->prev_token = get_fresh_token(ctx, &ctx->prev_string);
	}
	
	if (s) *s = ctx->prev_string;
	return ctx->prev_token;
}

/**
   Fetches the token after the next token in the token stream, but does not remove any tokens from the stream
*/

static Token peek_token2(parser_context *ctx, /*@null@*/string *s)
{
	if(ctx->prev_token == TOK_UNKNOWN)
	{
		ctx->prev_token = get_fresh_token(ctx, &ctx->prev_string);
	}
	if(ctx->prev_token2 == TOK_UNKNOWN)
	{
		ctx->prev_token2 = ctx->prev_token;
		ctx->prev_string2 = ctx->prev_string;
		ctx->prev_token = get_fresh_token(ctx, &ctx->prev_string);
	}
	
	if (s) *s = ctx->prev_string;
	return ctx->prev_token;
}

#define BULTIN_FUNCTION_ARG_NUM 4

/* 
   helper data structure for registering built-in functions 
*/
typedef struct {
	expression_operator op;
	char *name;
	int var_type[BULTIN_FUNCTION_ARG_NUM + 1]; /* +1 for result */
	essl_bool in_vertex_shader;
	essl_bool in_fragment_shader;
	extension extension_required;
} builtin_function_spec;

/* Use existing type_basic enums, and negative values for special types */
#define genType           -1
#define vec               -2
#define vec2              -3
#define vec3              -4
#define vec4              -5
#define bvec              -6
#define ivec              -7
#define mat               -8

static const builtin_function_spec builtin_functions[] = {
#define PROCESS(op, ret, name, arg1, arg2, arg3, arg4, in_vertex, in_fragment, ext) { op, name, { ret, arg1, arg2, arg3, arg4 }, in_vertex, in_fragment, ext },
#include "frontend/builtin_function_data.h"
#undef PROCESS
};
static const size_t n_builtin_function_entries = sizeof(builtin_functions)/sizeof(builtin_functions[0]);

static int builtin_function_entries_with_same_name(parser_context *ctx, size_t start_i, symbol **sym_out, symbol *succ_sym)
{
	string fun_name = _essl_cstring_to_string_nocopy(builtin_functions[start_i].name);
	string next_fun_name;
	symbol *first_sym;
	symbol **next_sym = &first_sym;
	size_t i;
	for (i = start_i ;
		 i < n_builtin_function_entries &&
			 ((next_fun_name = _essl_cstring_to_string_nocopy(builtin_functions[i].name)),
			  _essl_string_cmp(fun_name, next_fun_name) == 0) ;
		 i++)
	{
		int j;
		unsigned int cur_size;
		unsigned int minsize = 1;
		unsigned int maxsize = 1;
#ifndef NDEBUG
		int has_special_type = 0;
#endif
		if (ctx->target_desc->kind == TARGET_VERTEX_SHADER && !builtin_functions[i].in_vertex_shader)
		{
			continue;
		}
		else if (ctx->target_desc->kind == TARGET_FRAGMENT_SHADER && !builtin_functions[i].in_fragment_shader)
		{
			continue;
		}

		/* skip function if correct extension isn't enabled */
		if (_essl_get_extension_behavior(ctx->prep_context->lang_descriptor, builtin_functions[i].extension_required) == BEHAVIOR_DISABLE)
		{
			continue;
		}

		for (j = 0; j < BULTIN_FUNCTION_ARG_NUM + 1; ++j)
		{
			switch (builtin_functions[i].var_type[j])
			{
			case genType:
#ifndef NDEBUG
				assert(has_special_type == 0 || has_special_type == 1);
				has_special_type = 1;
#endif
				maxsize = 4;
				break;
			case vec:
			case bvec:
			case ivec:
#ifndef NDEBUG
				assert(has_special_type == 0 || has_special_type == 2);
				has_special_type = 2;
#endif
				minsize = 2;
				maxsize = 4;
				break;
			case mat:
#ifndef NDEBUG
				assert(has_special_type == 0 || has_special_type == 3);
				has_special_type = 3;
#endif
				minsize = 2;
				maxsize = 4;
				break;
			default:
				break;
			}
		}

		for (cur_size = minsize; cur_size <= maxsize; ++cur_size)
		{
			symbol *sym;
			const type_specifier *return_type = 0;
			single_declarator *param = 0;
			
			for (j = BULTIN_FUNCTION_ARG_NUM; j >= 0; --j)
			{
				const type_specifier *type = 0;
				switch (builtin_functions[i].var_type[j])
				{
				case 0:
					break;
				case genType:
				case vec:
					ESSL_CHECK_NONFATAL(type = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, cur_size));
					break;
				case vec2:
					ESSL_CHECK_NONFATAL(type = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 2));
					break;
				case vec3:
					ESSL_CHECK_NONFATAL(type = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 3));
					break;
				case vec4:
					ESSL_CHECK_NONFATAL(type = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 4));
					break;
				case bvec:
					ESSL_CHECK_NONFATAL(type = _essl_get_type(ctx->typestor_context, TYPE_BOOL, cur_size));
					break;
				case ivec:
					ESSL_CHECK_NONFATAL(type = _essl_get_type(ctx->typestor_context, TYPE_INT, cur_size));
					break;
				case mat:
					ESSL_CHECK_NONFATAL(type = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, cur_size));
					ESSL_CHECK_NONFATAL(type = _essl_new_matrix_of_type(ctx->pool, type, cur_size));
					break;
				default:
					/* Basic types */
					ESSL_CHECK_NONFATAL(type = _essl_get_type(ctx->typestor_context, (type_basic)builtin_functions[i].var_type[j], 1));
					break;
				}
				if (type)
				{
					if (j == 0)
					{
						/* return type */
						return_type = type;
					}
					else
					{
						single_declarator *prev_param = param;
						qualifier_set qual;
						_essl_init_qualifier_set(&qual);
						qual.direction = DIR_IN;

						ESSL_CHECK_NONFATAL(param = _essl_new_single_declarator(ctx->pool, type, qual, 0, NULL, UNKNOWN_SOURCE_OFFSET));
						param->next = prev_param;
					}
				}
			}
			assert(return_type != 0);

			ESSL_CHECK_OOM(sym = _essl_new_builtin_function_symbol(ctx->pool, fun_name, return_type, UNKNOWN_SOURCE_OFFSET));
			sym->parameters = param;
			sym->builtin_function = builtin_functions[i].op;
			*next_sym = sym;
			next_sym = &sym->next;
		}
	}

	*next_sym = succ_sym;
	*sym_out = first_sym;
	return 1;
}

/** 
	Loads the built-in functions into the global scope.
	The function data is defined in frontend/builtin_function_data.h
	

	@note Adding built-in functions to the global scope lets us do overloading and redefinition checking easily, although the spec says they should really be in a separate built-in scope.

*/
static int load_builtin_functions(parser_context *ctx)
{
	size_t i;
	string prev_name = {"<dummy>", 7};
	for (i = 0; i < n_builtin_function_entries; ++i)
	{
		string fun_name = _essl_cstring_to_string_nocopy(builtin_functions[i].name);
		if (_essl_string_cmp(fun_name, prev_name) != 0)
		{
			/* Put placeholder symbol, encoding table index in the source_offset field */
			symbol *sym;
			ESSL_CHECK_OOM(sym = _essl_new_builtin_function_name_symbol(ctx->pool, fun_name, i));
			ESSL_CHECK_OOM(_essl_symbol_scope_insert(ctx->global_scope, fun_name, sym));
			prev_name = fun_name;
		}
	}
	return 1;
}

#undef genType
#undef vec
#undef vec2
#undef vec3
#undef vec4
#undef bvec
#undef ivec
#undef mat

/**
 *  Check if the symbol found is a built-in function name placeholder,
 *  and replace it by a list of all overloads.
 */
static int filter_symbol(parser_context *ctx, symbol **sym_out, symbol *sym)
{
	if (sym != NULL && sym->kind == SYM_KIND_BUILTIN_FUNCTION_NAME)
	{
		/* Modify symbol to contain all overloads of built-in function */
		symbol *function_sym = NULL;
		ESSL_CHECK_OOM(builtin_function_entries_with_same_name(ctx, sym->source_offset, &function_sym, sym->next));
		if (function_sym == NULL)
		{
			/* Built-in function does not exist because needed extension is not enabled.
			   This situation will not be cached, but should be fairly rare.
			*/
			*sym_out = sym->next;
			return 1;
		}
		*sym = *function_sym;
	}
	*sym_out = sym;
	return 1;
}


/**
  Inserts a built-in variable symbol into the global scope
*/

static const type_specifier *get_type_with_set_precision(parser_context *ctx, const type_specifier *t, const qualifier_set *qual)
{
	/* if there's not a precision qualifier then set the precision to the default */
	precision_qualifier pq = PREC_UNKNOWN;
	scalar_size_specifier sz;
	type_specifier *nt;
	
	if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *sd;
		ESSL_CHECK(nt = _essl_clone_type(ctx->pool, t));
		for(sd = nt->members; sd != NULL; sd = sd->next)
		{
			ESSL_CHECK_NONFATAL(sd->type = get_type_with_set_precision(ctx, sd->type, &sd->qualifier));
		}
		t = nt;
	} else if(t->child_type != NULL)
	{
		ESSL_CHECK(nt = _essl_clone_type(ctx->pool, t));
		ESSL_CHECK_NONFATAL(nt->child_type = get_type_with_set_precision(ctx, t->child_type, qual));
		t = nt;
	} else {
		if(qual != NULL)
		{
			pq = qual->precision;
		}
		/* if the precision is not resolved then report an error */
		if(PREC_UNKNOWN != pq)
		{
			sz = ctx->target_desc->get_size_for_type_and_precision;
			ESSL_CHECK(t = _essl_get_type_with_given_size(ctx->typestor_context, t, sz));
		}

	}


	return t;
}

/*@null@*/ static symbol *insert_builtin_var(parser_context *ctx, string name, const type_specifier *type, qualifier_set qual, address_space_kind address_space)
{
	symbol *sym;

	ESSL_CHECK(type = get_type_with_set_precision(ctx, type, &qual));


	ESSL_CHECK_OOM(sym = _essl_new_variable_symbol(ctx->pool, name, type, qual, SCOPE_GLOBAL, address_space, UNKNOWN_SOURCE_OFFSET));
	ESSL_CHECK_OOM(_essl_symbol_scope_insert(ctx->global_scope, name, sym));
	return sym;
}

/**
  Inserts built-in variables into the global scope
*/
static int load_builtin_variables(parser_context *ctx)
{
#define MAX_VERTEX_ATTRIBS 16
#define MAX_VERTEX_UNIFORM_VECTORS 256
#define MAX_VARYING_VECTORS 12
#define MAX_VERTEX_TEXTURE_IMAGE_UNITS 0
#define MAX_COMBINED_TEXTURE_IMAGE_UNITS 8
#define MAX_TEXTURE_IMAGE_UNITS 8
#define MAX_FRAGMENT_UNIFORM_VECTORS 256
#define MAX_DRAW_BUFFERS 1
/*

const mediump int gl_MaxVertexAttribs = 8;
const mediump int gl_MaxVertexUniformVectors = 128;
const mediump int gl_MaxVaryingVectors = 8;
const mediump int gl_MaxVertexTextureImageUnits = 0;
const mediump int gl_MaxCombinedTextureImageUnits = 2;
const mediump int gl_MaxTextureImageUnits = 2;
const mediump int gl_MaxFragmentUniformVectors = 16;
const mediump int gl_MaxDrawBuffers = 1;


*/
	static const struct {
		const char *name;
		int value;
	} data[] = {
		{"gl_MaxVertexAttribs", MAX_VERTEX_ATTRIBS },
		{"gl_MaxVertexUniformVectors", MAX_VERTEX_UNIFORM_VECTORS },
		{"gl_MaxVaryingVectors", MAX_VARYING_VECTORS },
		{"gl_MaxVertexTextureImageUnits", MAX_VERTEX_TEXTURE_IMAGE_UNITS },
		{"gl_MaxCombinedTextureImageUnits", MAX_COMBINED_TEXTURE_IMAGE_UNITS },
		{"gl_MaxTextureImageUnits", MAX_TEXTURE_IMAGE_UNITS },
		{"gl_MaxFragmentUniformVectors", MAX_FRAGMENT_UNIFORM_VECTORS },
		{"gl_MaxDrawBuffers", MAX_DRAW_BUFFERS },
	};
	{
		unsigned i;
		for(i = 0; i < sizeof(data)/sizeof(data[0]); ++i)
		{
			symbol *sym;
			const type_specifier *type_int;
			node *constant;
			qualifier_set qual;
			_essl_init_qualifier_set(&qual);
			ESSL_CHECK_NONFATAL(type_int = _essl_get_type(ctx->typestor_context, TYPE_INT, 1));
			qual.precision = PREC_MEDIUM;
			qual.variable = VAR_QUAL_CONSTANT;

			ESSL_CHECK(sym = insert_builtin_var(ctx, _essl_cstring_to_string_nocopy(data[i].name), type_int, qual, ADDRESS_SPACE_GLOBAL));
			ESSL_CHECK(sym->body = constant = _essl_new_constant_expression(ctx->pool, 1));
			constant->hdr.type = type_int;
			constant->expr.u.value[0] = ctx->target_desc->int_to_scalar(data[i].value);

		}
	}


	if (ctx->target_desc->kind == TARGET_VERTEX_SHADER)
	{
		/*
		vertex shader:
		at global scope
			highp vec4 gl_Position; writable
			mediump float gl_PointSize; writable
		*/
		const type_specifier *type_vec4;
		const type_specifier *type_float;
		qualifier_set highp_varying_qual;
		qualifier_set medp_varying_qual;
		_essl_init_qualifier_set(&highp_varying_qual);
		_essl_init_qualifier_set(&medp_varying_qual);

		ESSL_CHECK_NONFATAL(type_vec4 = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 4));
		highp_varying_qual.variable = VAR_QUAL_VARYING;
		highp_varying_qual.precision = PREC_HIGH;
		ESSL_CHECK_NONFATAL(type_float = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 1));
		medp_varying_qual.variable = VAR_QUAL_VARYING;
		medp_varying_qual.precision = PREC_MEDIUM;

		ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_Position"), type_vec4, highp_varying_qual, ADDRESS_SPACE_VERTEX_VARYING));
		ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_PointSize"), type_float, medp_varying_qual, ADDRESS_SPACE_VERTEX_VARYING));
	}
	else if (ctx->target_desc->kind == TARGET_FRAGMENT_SHADER)
	{
		/*
		fragment shader:
		at global scope
		mediump vec4 gl_FragColor output
		mediump vec4 gl_FragData[gl_MaxDrawBuffers] output
		can't write to both
		irrelevant if 'discard' is executed
		
		mediump vec4 gl_FragCoord read-only, window-relative coords
		bool gl_FrontFacing read-only
		mediump vec2 gl_PointCoord read-only

		framebuffer read extension:
		mediump vec4 gl_FBColor read-only, current framebuffer color at this coordinate
		mediump float gl_FBDepth read-only, current framebuffer depth at this coordinate
		mediump float gl_FBStencil read-only, current framebuffer stencil value at this coordinate
		
		*/
		const type_specifier *type_vec4;
		const type_specifier *type_vec4_readonly_base;
		type_specifier *type_vec4_readonly;
		const type_specifier *type_vec2_readonly_base;
		type_specifier *type_vec2_readonly;
		const type_specifier *type_float_readonly_base;
		type_specifier *type_float_readonly;
		const type_specifier *type_bool_readonly_base;
		type_specifier *type_bool_readonly;
		const type_specifier *type_vec4_array_base;
		const type_specifier *type_vec4_array;
		extension_behavior extension;
		qualifier_set medp_qual;
		qualifier_set default_qual;

		_essl_init_qualifier_set(&medp_qual);
		medp_qual.precision = PREC_MEDIUM;

		
		_essl_init_qualifier_set(&default_qual);
		
		ESSL_CHECK_NONFATAL(type_vec4 = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 4));


		ESSL_CHECK_NONFATAL(type_vec4_readonly_base = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 4));
		ESSL_CHECK_NONFATAL(type_vec4_readonly = _essl_clone_type(ctx->pool, type_vec4_readonly_base));
		type_vec4_readonly->type_qual = TYPE_QUAL_NOMODIFY;
		

		ESSL_CHECK_NONFATAL(type_vec2_readonly_base = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 2));
		ESSL_CHECK_NONFATAL(type_vec2_readonly = _essl_clone_type(ctx->pool, type_vec2_readonly_base));
		type_vec2_readonly->type_qual = TYPE_QUAL_NOMODIFY;
		
		ESSL_CHECK_NONFATAL(type_float_readonly_base = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 1));
		ESSL_CHECK_NONFATAL(type_float_readonly = _essl_clone_type(ctx->pool, type_float_readonly_base));
		type_float_readonly->type_qual = TYPE_QUAL_NOMODIFY;
		
		
		ESSL_CHECK_NONFATAL(type_vec4_array_base = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 4));
		ESSL_CHECK_NONFATAL(type_vec4_array = _essl_new_array_of_type(ctx->pool, type_vec4_array_base, MAX_DRAW_BUFFERS));

		ESSL_CHECK_NONFATAL(type_bool_readonly_base = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1));
		ESSL_CHECK_NONFATAL(type_bool_readonly = _essl_clone_type(ctx->pool, type_bool_readonly_base));
		type_bool_readonly->type_qual = TYPE_QUAL_NOMODIFY;
		
		ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_FragColor"), type_vec4, medp_qual, ADDRESS_SPACE_FRAGMENT_OUT));
		ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_FragData"), type_vec4_array, medp_qual, ADDRESS_SPACE_FRAGMENT_OUT));
		ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_FragCoord"), type_vec4_readonly, medp_qual, ADDRESS_SPACE_FRAGMENT_SPECIAL));
		ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_FrontFacing"), type_bool_readonly, default_qual, ADDRESS_SPACE_FRAGMENT_SPECIAL));
		ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_PointCoord"), type_vec2_readonly, medp_qual, ADDRESS_SPACE_FRAGMENT_SPECIAL));

		extension = _essl_get_extension_behavior(ctx->prep_context->lang_descriptor, EXTENSION_GL_ARM_FRAMEBUFFER_READ);
		if (extension == BEHAVIOR_ENABLE || extension == BEHAVIOR_WARN)
		{
			ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_FBColor"), type_vec4_readonly, medp_qual, ADDRESS_SPACE_FRAGMENT_SPECIAL));
			ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_FBDepth"), type_float_readonly, medp_qual, ADDRESS_SPACE_FRAGMENT_SPECIAL));
			ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_FBStencil"), type_float_readonly, medp_qual, ADDRESS_SPACE_FRAGMENT_SPECIAL));
		}
	}

	{ 
/* 
//
// Depth range in window coordinates
//
struct gl_DepthRangeParameters {
highp float near; // n
highp float far; // f
highp float diff; // f - n
};
uniform gl_DepthRangeParameters gl_DepthRange;
 */
		const char *member_names[3] = {"near", "far", "diff"};
		int i;
		precision_qualifier wanted_precision = ctx->target_desc->has_high_precision ? PREC_HIGH : PREC_MEDIUM;
		type_specifier *struct_type;
		single_declarator **member;
		const type_specifier *scalar_type;
		symbol *sym;
		qualifier_set uniform_qual;
		_essl_init_qualifier_set(&uniform_qual);
		uniform_qual.variable = VAR_QUAL_UNIFORM;
		
		ESSL_CHECK(struct_type = _essl_new_type(ctx->pool));
		struct_type->basic_type = TYPE_STRUCT;
		struct_type->members = 0;
		struct_type->name = _essl_cstring_to_string_nocopy("gl_DepthRangeParameters");
		member = &struct_type->members;
		ESSL_CHECK(scalar_type = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 1));
		for(i = 0; i < 3; ++i)
		{
			single_declarator *curr;
			string nm = _essl_cstring_to_string_nocopy(member_names[i]);
			qualifier_set qual;
			_essl_init_qualifier_set(&qual);
			qual.precision = wanted_precision;
			ESSL_CHECK(curr = _essl_new_single_declarator(ctx->pool, scalar_type, qual, &nm, struct_type, UNKNOWN_SOURCE_OFFSET));
			curr->index = i;
			curr->next = 0;
			*member = curr;
			member = &curr->next;

		}
		
		sym = _essl_new_type_symbol(ctx->pool, struct_type->name, struct_type, UNKNOWN_SOURCE_OFFSET);
		ESSL_CHECK_OOM(sym);
		ESSL_CHECK_OOM(_essl_symbol_scope_insert(ctx->current_scope, struct_type->name, sym));

		ESSL_CHECK_NONFATAL(insert_builtin_var(ctx, _essl_cstring_to_string_nocopy("gl_DepthRange"), struct_type, uniform_qual, ADDRESS_SPACE_UNIFORM));

	}

	return 1;
}


/** Check if identifier name is allowed (to be used by shader.
  * Identifiers starting with 'gl_' is not allowed (unless this is allowed in the target descriptor).
  * Identifiers containing '__' is never allowed.
  * @param ctx Parser context
  * @param identifier Identifier name to check.
  * @return ESSL_TRUE if identifier is valid, ESSL_FALSE if not.
  */
static essl_bool valid_identifier_name(parser_context *ctx, string identifier)
{
	int i;

	if (ctx->lang_desc->allow_gl_names)
	{
		return ESSL_TRUE;
	}

	for (i = 0; i < identifier.len - 1; ++i)
	{
		if (identifier.ptr[i] == '_' && identifier.ptr[i + 1] == '_')
		{
			return ESSL_FALSE;
		}
	}

	return (_essl_string_cstring_count_cmp(identifier, "gl_", 3) != 0);
}


/**
	Used by various functions to see if the next token could be part of a type 
*/
static int type_lookahead(parser_context *ctx)
{
	string toktext = {"<dummy>", 7};
	switch(peek_token(ctx, &toktext))
	{
	case TOK_CONST:
	case TOK_CENTROID:
	case TOK_FLAT:
	case TOK_ATTRIBUTE:
	case TOK_VARYING:
	case TOK_UNIFORM:
	case TOK_VOID:
	case TOK_FLOAT:
	case TOK_INT:
	case TOK_BOOL:
	case TOK_VEC2:
	case TOK_VEC3:
	case TOK_VEC4:
	case TOK_BVEC2:
	case TOK_BVEC3:
	case TOK_BVEC4:
	case TOK_IVEC2:
	case TOK_IVEC3:
	case TOK_IVEC4:
	case TOK_MAT2:
	case TOK_MAT3:
	case TOK_MAT4:
/*	case TOK_SAMPLER1D: */
	case TOK_SAMPLER2D:
	case TOK_SAMPLER3D:
	case TOK_SAMPLERCUBE:
/*	case TOK_SAMPLER1DSHADOW: */
	case TOK_SAMPLER2DSHADOW:
	case TOK_SAMPLEREXTERNAL:
	case TOK_LOWP:
	case TOK_MEDIUMP:
	case TOK_HIGHP:
	case TOK_STRUCT:
	case TOK_PERSISTENT:
		return 1;

	case TOK_IDENTIFIER:
		{
			symbol *sym;
			sym = _essl_symbol_table_lookup(ctx->current_scope, toktext);
			if (sym != 0 && sym->kind == SYM_KIND_TYPE)
			{
				return 1;
			}
			else
			{
				return 0;
			}
		}
	default:
		return 0;
	}
}

/*@null@*/ static node *integer_expression(parser_context *ctx)
{
	node *tmp;
	CALL(tmp, expression);
	return tmp;
}

int _essl_string_to_float(parser_context *ctx, const string str, /*@out@*/ float *retval)
{
	char *buf;
	double curval;
	essl_bool is_conv;
	assert(str.len > 0 && str.ptr != 0);
	if(retval)
	{
		*retval = 0.0f;
	}
	ESSL_CHECK_OOM(buf = _essl_mempool_alloc(ctx->pool, (size_t)str.len + 1));
	strncpy(buf, str.ptr, (size_t)str.len);
	buf[str.len] = '\0';
	is_conv = _essl_convert_string_to_double(ctx->pool, buf, &curval);
	if (!is_conv)
	{
		/* conversion failed before end of string */
		SYNTAX_ERROR_PARAM("Error while parsing floating point literal '%s'\n", buf);
		return 0;
	}
	if (retval)
	{
		*retval = (float)curval;
	}
	return 1;
}

static memerr push_struct_declaration_stack_entry(parser_context *ctx)
{
	struct_declaration_stack_level *entry;
	ESSL_CHECK_OOM(entry = _essl_mempool_alloc(ctx->pool, sizeof(*entry)));
	entry->prev = ctx->struct_declaration_stack;
	ctx->struct_declaration_stack = entry;
	return MEM_OK;
}

static memerr pop_struct_declaration_stack_entry(parser_context *ctx)
{
	if(ctx->struct_declaration_stack != NULL)
	{
		ctx->struct_declaration_stack = ctx->struct_declaration_stack->prev;
	}
	return MEM_OK;
}


/**
	Parses a struct declaration and adds it to the current scope.
	@param ctx Parse context
	@param struct_type On input, the qualifiers for the current type. On output, the full type of the struct

	@return The struct type
*/
/*@null@*/ static type_specifier *struct_declaration(parser_context *ctx, type_specifier *struct_type)
{
	single_declarator *first_member = 0;
	single_declarator *current_member = 0;
	single_declarator *next_member;
	single_declarator *tmp;
	int source_offset = _essl_preprocessor_get_source_offset(ctx->prep_context);
	LOGMSG("=> struct_declaration");
	MATCH(TOK_STRUCT);
	if(ctx->struct_declaration_stack != NULL)
	{
		SYNTAX_ERROR_MSG("Embedded structure definition is not allowed\n");
	}
	ESSL_CHECK_NONFATAL(push_struct_declaration_stack_entry(ctx));
	if (peek_token(ctx, NULL) == TOK_IDENTIFIER)
	{
		string toktext= {"<dummy>", 7};
		MATCH_TEXT(TOK_IDENTIFIER, &toktext);
		if (!valid_identifier_name(ctx, toktext))
		{
			ERROR_CODE_STRING(ERR_LP_ILLEGAL_IDENTIFIER_NAME, "Illegal identifier name '%s'\n", toktext);
		}
		struct_type->name = toktext;
	}

	MATCH(TOK_LEFT_BRACE);
	if(peek_token(ctx, NULL) != TOK_RIGHT_BRACE)
	{
		int index = 0;
		do
		{
			type_specifier *initial_member_type;
			qualifier_set qual;
			_essl_init_qualifier_set(&qual);
			CALL_TWOPARAMS(initial_member_type, specified_type, TYPESPEC_TYPE_SPECIFIER, &qual);
			for(;;) {
				string member_name = {"<dummy>", 7};
				type_specifier *member_type = initial_member_type;
				int source_offset = _essl_preprocessor_get_source_offset(ctx->prep_context);
				MATCH_TEXT(TOK_IDENTIFIER, &member_name);
				if (!valid_identifier_name(ctx, member_name))
				{
					ERROR_CODE_STRING(ERR_LP_ILLEGAL_IDENTIFIER_NAME, "Illegal identifier name '%s'\n", member_name);
				}

				if (peek_token(ctx, NULL) == TOK_LEFT_BRACKET)
				{
					node *constexpr;
					MUST_MATCH(TOK_LEFT_BRACKET);
					CALL(constexpr, constant_expression);
					ESSL_CHECK_OOM(member_type = _essl_new_unresolved_array_of_type(ctx->pool, initial_member_type, constexpr));
					MATCH(TOK_RIGHT_BRACKET);
				}
				tmp = first_member;
				while(tmp) /* see if the member name has been reused */
				{
					if(!_essl_string_cmp(member_name, tmp->name))
					{
						ERROR_CODE_STRING(ERR_SEM_NAME_REDEFINITION, "Duplicate struct member '%s'\n", member_name);
						return 0;
					}
					tmp = tmp->next;
				}
				next_member = _essl_new_single_declarator(ctx->pool, member_type, qual, &member_name, struct_type, source_offset);
				ESSL_CHECK_OOM(next_member);
				next_member->index = index++;
				if (current_member)
				{
					current_member->next = next_member;
					assert(!next_member->next);
					current_member = next_member;
				}
				else
				{
					first_member = next_member;
					assert(!next_member->next);
					current_member = next_member;
				}
				if (peek_token(ctx, NULL) != TOK_COMMA) break;
				MUST_MATCH(TOK_COMMA);
			} 
			MATCH(TOK_SEMICOLON);
		} while (peek_token(ctx, NULL) != TOK_RIGHT_BRACE);
	}
	MUST_MATCH(TOK_RIGHT_BRACE);
	struct_type->members = first_member;

	if (struct_type->members == NULL)
	{
		SYNTAX_ERROR_STRING("Struct '%s' has no members\n", struct_type->name);
	}

	if (struct_type->name.len)
	{
		symbol *sym;
		string struct_name = struct_type->name;
		if (_essl_symbol_table_lookup_current_scope(ctx->current_scope, struct_name))
		{
			ERROR_CODE_STRING(ERR_SEM_NAME_REDEFINITION, "Symbol '%s' redeclared\n", struct_name);
		}
		else
		{
			sym = _essl_new_type_symbol(ctx->pool, struct_name, struct_type, source_offset);
			ESSL_CHECK_OOM(sym);
			ESSL_CHECK_OOM(_essl_symbol_scope_insert(ctx->current_scope, struct_name, sym));
		}
	}
	ESSL_CHECK_NONFATAL(pop_struct_declaration_stack_entry(ctx));
	LOGMSG("<= struct_declaration");
	return struct_type;
}

/** Parses a type including qualifiers. */
static type_specifier *specified_type(parser_context *ctx, type_specifier_kind spectype, qualifier_set *qual)
{
	type_specifier *type;
	essl_bool precision_parsed = ESSL_FALSE;
	LOGMSG("=> type_specifier");
	type = _essl_new_type(ctx->pool);
	ESSL_CHECK_OOM(type);
	type->basic_type = TYPE_UNKNOWN;
	type->u.basic.vec_size = 1;
	if (spectype == TYPESPEC_FULLY_SPECIFIED_TYPE || spectype == TYPESPEC_PARAMETER_DECLARATION)
	{
		/* Type qualifier */
		switch (peek_token(ctx, NULL))
		{
		case TOK_CONST:
			MUST_MATCH(TOK_CONST);
			if (spectype == TYPESPEC_FULLY_SPECIFIED_TYPE)
			{
				/* compile-time constant */
				qual->variable = VAR_QUAL_CONSTANT;
			}
			else if (spectype == TYPESPEC_PARAMETER_DECLARATION)
			{
				/* function parameter */
				type->type_qual |= TYPE_QUAL_NOMODIFY;
			}
			break;
		case TOK_ATTRIBUTE:
			MUST_MATCH(TOK_ATTRIBUTE);
			if(ctx->current_scope != ctx->global_scope)
			{
				ERROR_CODE_MSG(ERR_SEM_ATTRIBUTE_INSIDE_FUNCTION, "Attribute variable declared inside a function\n");
				return NULL;
			}
			qual->variable = VAR_QUAL_ATTRIBUTE;
			break;
		case TOK_FLAT:
			MUST_MATCH(TOK_FLAT);
			assert(qual->varying == VARYING_QUAL_NONE);
			MATCH(TOK_VARYING);
				
			qual->varying = VARYING_QUAL_FLAT;
			qual->variable = VAR_QUAL_VARYING;
			break;

		case TOK_CENTROID:
			MUST_MATCH(TOK_CENTROID);
			assert(qual->varying == VARYING_QUAL_NONE);
			MATCH(TOK_VARYING);
				
			qual->varying = VARYING_QUAL_CENTROID;
			qual->variable = VAR_QUAL_VARYING;
			break;

		case TOK_VARYING:
			MUST_MATCH(TOK_VARYING);
			if(ctx->current_scope != ctx->global_scope)
			{
				ERROR_CODE_MSG(ERR_SEM_VARYING_INSIDE_FUNCTION, "Varying variable declared inside a function\n");
				return NULL;
			}
			qual->variable = VAR_QUAL_VARYING;
			break;
		case TOK_UNIFORM:
			MUST_MATCH(TOK_UNIFORM);
			if(ctx->current_scope != ctx->global_scope)
			{
				ERROR_CODE_MSG(ERR_SEM_UNIFORM_INSIDE_FUNCTION, "Uniform variable declared inside a function\n");
				return NULL;
			}
			qual->variable = VAR_QUAL_UNIFORM;

			{
				extension_behavior grouped_ext = _essl_get_extension_behavior(ctx->prep_context->lang_descriptor, EXTENSION_GL_ARM_GROUPED_UNIFORMS);
				if (grouped_ext == BEHAVIOR_ENABLE || grouped_ext == BEHAVIOR_WARN)
				{
					if (peek_token(ctx, NULL) == TOK_GROUP)
					{
						string group = { "", 0 };
						MUST_MATCH(TOK_GROUP);
						MATCH(TOK_LEFT_PAREN);
						if (peek_token(ctx, NULL) == TOK_IDENTIFIER)
						{
							MATCH_TEXT(TOK_IDENTIFIER, &group);
						}
						MATCH(TOK_RIGHT_PAREN);

						qual->group = group;

						if (grouped_ext == BEHAVIOR_WARN)
						{
							WARNING_CODE_STRING(ERR_WARNING, "Extension 'GL_ARM_grouped_uniforms' used, group '%s' declared for uniform variable\n", group);
						}
					}
				}
			}
			break;
		case TOK_PERSISTENT:
			{
				extension_behavior persistent_ext = _essl_get_extension_behavior(ctx->prep_context->lang_descriptor, EXTENSION_GL_ARM_PERSISTENT_GLOBALS);
				if (persistent_ext == BEHAVIOR_ENABLE || persistent_ext == BEHAVIOR_WARN)
				{
					MATCH(TOK_PERSISTENT);
					if (persistent_ext == BEHAVIOR_WARN)
					{
						WARNING_CODE_MSG(ERR_WARNING, "Extension 'GL_ARM_persistent_globals' used\n");
					}
					qual->variable = VAR_QUAL_PERSISTENT;
				}
			}
			break;
			
		default:
			break;
		}
	}

	if (spectype == TYPESPEC_PARAMETER_DECLARATION)
	{
		/* Parameter qualifier */
		switch (peek_token(ctx, NULL))
		{
		case TOK_IN:
			MUST_MATCH(TOK_IN);
			/*@fallthrough@*/
		default:
			qual->direction = DIR_IN;
			break;
		case TOK_OUT:
			MUST_MATCH(TOK_OUT);
			qual->direction = DIR_OUT;
			break;
		case TOK_INOUT:
			MUST_MATCH(TOK_INOUT);
			qual->direction = DIR_INOUT;
			break;
		}
	}

	if (spectype != TYPESPEC_CONSTRUCTOR_IDENTIFIER)
	{
		/* Precision qualifier */
		switch (peek_token(ctx, NULL))
		{
		case TOK_LOWP:
			MUST_MATCH(TOK_LOWP);
			qual->precision = PREC_LOW;
			precision_parsed = ESSL_TRUE;
			break;
		case TOK_MEDIUMP:
			MUST_MATCH(TOK_MEDIUMP);
			qual->precision = PREC_MEDIUM;
			precision_parsed = ESSL_TRUE;
			break;
		case TOK_HIGHP:
			MUST_MATCH(TOK_HIGHP);
			if (ctx->lang_desc->target_desc->has_high_precision == 0 && !ctx->have_reported_highp_warning)
			{
				WARNING_CODE_MSG(ERR_PP_NO_HIGHP, "High precision not supported, instead compiling high precision as medium precision\n");
				ctx->have_reported_highp_warning = ESSL_TRUE;
			}
			qual->precision = PREC_HIGH;
			precision_parsed = ESSL_TRUE;
			break;
		default:
			break;
		}
	}

	/* Type specifier no_prec */
	switch (peek_token(ctx, NULL))
	{
	case TOK_VOID:
		MUST_MATCH(TOK_VOID);
		type->basic_type = TYPE_VOID;
		LOGMSG("<= type_specifier");
		return type;
	case TOK_FLOAT:
		MUST_MATCH(TOK_FLOAT);
		type->basic_type = TYPE_FLOAT;
		LOGMSG("<= type_specifier");
		return type;
	case TOK_INT:
		MUST_MATCH(TOK_INT);
		type->basic_type = TYPE_INT;
		LOGMSG("<= type_specifier");
		return type;
	case TOK_BOOL:
		MUST_MATCH(TOK_BOOL);
		type->basic_type = TYPE_BOOL;
		if(precision_parsed == ESSL_TRUE)
		{
			ERROR_CODE_MSG(ERR_LP_SYNTAX_ERROR, "Boolean variable can't have a precision qualifier\n");
			return MEM_ERROR;
		}
		LOGMSG("<= type_specifier");
		return type;
	case TOK_VEC2:
		MUST_MATCH(TOK_VEC2);
		type->basic_type = TYPE_FLOAT;
		type->u.basic.vec_size = 2;
		LOGMSG("<= type_specifier");
		return type;
	case TOK_VEC3:
		MUST_MATCH(TOK_VEC3);
		type->basic_type = TYPE_FLOAT;
		type->u.basic.vec_size = 3;
		LOGMSG("<= type_specifier");
		return type;
	case TOK_VEC4:
		MUST_MATCH(TOK_VEC4);
		type->basic_type = TYPE_FLOAT;
		type->u.basic.vec_size = 4;
		LOGMSG("<= type_specifier");
		return type;

	case TOK_BVEC2:
		MUST_MATCH(TOK_BVEC2);
		type->basic_type = TYPE_BOOL;
		type->u.basic.vec_size = 2;
		if(precision_parsed == ESSL_TRUE)
		{
			ERROR_CODE_MSG(ERR_LP_SYNTAX_ERROR, "Boolean variable can't have a precision qualifier\n");
			return MEM_ERROR;
		}
		LOGMSG("<= type_specifier");
		return type;
	case TOK_BVEC3:
		MUST_MATCH(TOK_BVEC3);
		type->basic_type = TYPE_BOOL;
		type->u.basic.vec_size = 3;
		if(precision_parsed == ESSL_TRUE)
		{
			ERROR_CODE_MSG(ERR_LP_SYNTAX_ERROR, "Boolean variable can't have a precision qualifier\n");
			return MEM_ERROR;
		}
		LOGMSG("<= type_specifier");
		return type;
	case TOK_BVEC4:
		MUST_MATCH(TOK_BVEC4);
		type->basic_type = TYPE_BOOL;
		type->u.basic.vec_size = 4;
		if(precision_parsed == ESSL_TRUE)
		{
			ERROR_CODE_MSG(ERR_LP_SYNTAX_ERROR, "Boolean variable can't have a precision qualifier\n");
			return MEM_ERROR;
		}
		LOGMSG("<= type_specifier");
		return type;

	case TOK_IVEC2:
		MUST_MATCH(TOK_IVEC2);
		type->basic_type = TYPE_INT;
		type->u.basic.vec_size = 2;
		LOGMSG("<= type_specifier");
		return type;
	case TOK_IVEC3:
		MUST_MATCH(TOK_IVEC3);
		type->basic_type = TYPE_INT;
		type->u.basic.vec_size = 3;
		LOGMSG("<= type_specifier");
		return type;
	case TOK_IVEC4:
		MUST_MATCH(TOK_IVEC4);
		type->basic_type = TYPE_INT;
		type->u.basic.vec_size = 4;
		LOGMSG("<= type_specifier");
		return type;

	case TOK_MAT2:
		MUST_MATCH(TOK_MAT2);
		type->basic_type = TYPE_FLOAT;
		type->u.basic.vec_size = 2;
		ESSL_CHECK(type = _essl_new_matrix_of_type(ctx->pool, type, 2));
		LOGMSG("<= type_specifier");
		return type;
	case TOK_MAT3:
		MUST_MATCH(TOK_MAT3);
		type->basic_type = TYPE_FLOAT;
		type->u.basic.vec_size = 3;
		ESSL_CHECK(type = _essl_new_matrix_of_type(ctx->pool, type, 3));
		LOGMSG("<= type_specifier");
		return type;
	case TOK_MAT4:
		MUST_MATCH(TOK_MAT4);
		type->basic_type = TYPE_FLOAT;
		type->u.basic.vec_size = 4;
		ESSL_CHECK(type = _essl_new_matrix_of_type(ctx->pool, type, 4));
		LOGMSG("<= type_specifier");
		return type;


	case TOK_SAMPLER2D:
		MUST_MATCH(TOK_SAMPLER2D);
		type->basic_type = TYPE_SAMPLER_2D;
		LOGMSG("<= type_specifier");
		return type;
	case TOK_SAMPLER3D:
		{
			extension_behavior ext = _essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_TEXTURE_3D);
			if (ext == BEHAVIOR_ENABLE || ext == BEHAVIOR_WARN)
			{
				MUST_MATCH(TOK_SAMPLER3D);
				type->basic_type = TYPE_SAMPLER_3D;
				LOGMSG("<= type_specifier");
				return type;
			}
		}
		break;


	case TOK_SAMPLERCUBE:
		MUST_MATCH(TOK_SAMPLERCUBE);
		type->basic_type = TYPE_SAMPLER_CUBE;
		LOGMSG("<= type_specifier");
		return type;

	case TOK_SAMPLER2DSHADOW:
		{
			extension_behavior ext = _essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_EXT_SHADOW_SAMPLERS);
			if (ext == BEHAVIOR_ENABLE || ext == BEHAVIOR_WARN)
			{
				MUST_MATCH(TOK_SAMPLER2DSHADOW);
				type->basic_type = TYPE_SAMPLER_2D_SHADOW;
				LOGMSG("<= type_specifier");
				return type;
			}
		}
		break;


	case TOK_SAMPLEREXTERNAL:
		{
			extension_behavior ext = _essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_TEXTURE_EXTERNAL);
			if (ext == BEHAVIOR_ENABLE || ext == BEHAVIOR_WARN)
			{
				MUST_MATCH(TOK_SAMPLEREXTERNAL);
				type->basic_type = TYPE_SAMPLER_EXTERNAL;
				LOGMSG("<= type_specifier");
				return type;
			}
		}
		break;


	case TOK_STRUCT:
		{
			type->basic_type = TYPE_STRUCT;
			CALL_PARAM(type, struct_declaration, type);
			LOGMSG("<= type_specifier");
			return type;
		}
		
	case TOK_IDENTIFIER:
		{
			string toktext = {"<dummy>", 7};
			symbol *sym;
			if (!type_lookahead(ctx))
			{
				string tokstr = {"<dummy>", 7};
				(void)get_token(ctx, &tokstr);
				ERROR_CODE_STRING(ERR_LP_SYNTAX_ERROR, "Typename expected, found '%s'\n", tokstr);
				return type;
			}
			MATCH_TEXT(TOK_IDENTIFIER, &toktext);
			sym = _essl_symbol_table_lookup(ctx->current_scope, toktext);

			/* type_lookahead() only returns true when the assert below is true (for TOK_IDENTIFIERs) */
			assert(sym != NULL && sym->kind == SYM_KIND_TYPE);

			sym->is_used = 1;
			type->basic_type = TYPE_STRUCT;
			type->name = sym->type->name;
			type->members = sym->type->members;
		}
		LOGMSG("<= type_specifier");
		return type;
	default:
		break;
	}

	{
		string tokstr = {"<dummy>", 7};
		(void)get_token(ctx, &tokstr);
		ERROR_CODE_STRING(ERR_LP_SYNTAX_ERROR, "Typename expected, found '%s'\n", tokstr);
		return NULL;
	}
}

/** @defgroup ParserRecursiveDescentFunctions Functions used for the main recursive-descent parser.
    @{
 */
/*@null@*/ static node *primary_expression(parser_context *ctx)
{
	string toktext = {"<dummy>", 7};
	Token t;
	float fval;
	node *prim;
	const type_specifier *primtype;
	LOGMSG("=> primary_expression");
	switch(peek_token(ctx, NULL))
	{
	case TOK_INTCONSTANT:
		{
			int ival = 0;
			MATCH_TEXT(TOK_INTCONSTANT, &toktext);
			assert(toktext.len > 0 && toktext.ptr != 0);
			if (_essl_string_to_integer(toktext, 0, &ival) == 0)
			{
				SYNTAX_ERROR_STRING("Error while parsing integer literal '%s'\n", toktext);
				return 0;
			}
			prim = _essl_new_constant_expression(ctx->pool, 1);
			ESSL_CHECK_OOM(prim);
			SET_NODE_POSITION(prim);
			primtype = _essl_get_type(ctx->typestor_context, TYPE_INT, 1);
			ESSL_CHECK_OOM(primtype);
			prim->expr.u.value[0] = ctx->target_desc->int_to_scalar(ival);
			prim->hdr.type = primtype;
			LOGMSG("<= primary_expression");
			return prim;
		}

	case TOK_FLOATCONSTANT:
		MATCH_TEXT(TOK_FLOATCONSTANT, &toktext);
		ESSL_CHECK_NONFATAL(_essl_string_to_float(ctx, toktext, &fval));
		prim = _essl_new_constant_expression(ctx->pool, 1);
		ESSL_CHECK_OOM(prim);
		SET_NODE_POSITION(prim);
		primtype = _essl_get_type(ctx->typestor_context, TYPE_FLOAT, 1);
		ESSL_CHECK_OOM(primtype);
		prim->expr.u.value[0] = ctx->target_desc->float_to_scalar( fval);
		prim->hdr.type = primtype;
		LOGMSG("<= primary_expression");
		return prim;

	case TOK_TRUE:
	case TOK_FALSE:
		t = get_token(ctx, NULL);
		prim = _essl_new_constant_expression(ctx->pool, 1);
		ESSL_CHECK_OOM(prim);
		SET_NODE_POSITION(prim);
		primtype = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1);
		ESSL_CHECK_OOM(primtype);
		prim->hdr.type = primtype;
		prim->expr.u.value[0] = ctx->target_desc->bool_to_scalar((int)(t == TOK_TRUE));
		LOGMSG("<= primary_expression");
		return prim;
		
	case TOK_LEFT_PAREN:
		MATCH(TOK_LEFT_PAREN);
		{
			node *res;
			CALL(res, expression);
			MATCH(TOK_RIGHT_PAREN);
			LOGMSG("<= primary_expression");
			return res;
		}
	default:
		t = peek_token(ctx, &toktext);
		SYNTAX_ERROR_PARAM("Expected literal or '(', got '%s'\n", _essl_token_to_str(t));
		return 0;
	}
}

static /*@null@*/ node *function_call_body(parser_context *ctx, node *parent)
{
	node *n;
	MATCH(TOK_LEFT_PAREN);
	if (peek_token(ctx, NULL) == TOK_VOID)
	{
		MATCH(TOK_VOID);
		MATCH(TOK_RIGHT_PAREN);
		return parent;
	}
	if (peek_token(ctx, NULL) == TOK_RIGHT_PAREN)
	{
		MATCH(TOK_RIGHT_PAREN);
		return parent;
	}
	CALL(n, assignment_expression);
	ESSL_CHECK_OOM(APPEND_CHILD(parent, n, ctx->pool));

	while (peek_token(ctx, NULL) == TOK_COMMA)
	{
		MATCH(TOK_COMMA);
		CALL(n, assignment_expression);
		ESSL_CHECK_OOM(APPEND_CHILD(parent, n, ctx->pool));

	}
	MATCH(TOK_RIGHT_PAREN);
	return parent;
}

/*@null@*/ static node *postfix_expression(parser_context *ctx)
{
	node *post = NULL;
	LOGMSG("=> postfix_expression");
	switch(peek_token(ctx, NULL))
	{
	case TOK_IDENTIFIER:
		if (!type_lookahead(ctx))
		{
			post = NULL;
			break;
		}
		/*@fallthrough@*/
	case TOK_FLOAT:
	case TOK_INT:
	case TOK_BOOL:
	case TOK_VEC2:
	case TOK_VEC3:
	case TOK_VEC4:
	case TOK_BVEC2:
	case TOK_BVEC3:
	case TOK_BVEC4:
	case TOK_IVEC2:
	case TOK_IVEC3:
	case TOK_IVEC4:
	case TOK_MAT2:
	case TOK_MAT3:
	case TOK_MAT4:
		{
			type_specifier *ctype;
			node *constr;
			int is_builtin = 1;
			qualifier_set qual;
			LOGMSG(" > constructor");
			if (peek_token(ctx, NULL) == TOK_IDENTIFIER)
			{
				is_builtin = 0;
			}
			CALL_TWOPARAMS(ctype, specified_type, TYPESPEC_CONSTRUCTOR_IDENTIFIER, &qual);
			if(is_builtin)
			{
				ESSL_CHECK_OOM(constr = _essl_new_builtin_constructor_expression(ctx->pool, 0));
			} else {
				ESSL_CHECK_OOM(constr = _essl_new_struct_constructor_expression(ctx->pool, 0));
			}
			CALL_PARAM(constr, function_call_body, constr);

			SET_NODE_POSITION(constr);
			constr->hdr.type = ctype;
			post = constr;
			LOGMSG(" < constructor");
			break;
		}
	default:
		post = NULL;
		break;
	}

	if (!post)
	{
		if (TOK_IDENTIFIER == peek_token(ctx, NULL))
		{
			string name = {"<dummy>", 7};
			/*
			  variable references have been moved from primary_expression to this rule in order to avoid using extra lookaheads.
			  
			  The following can be either a variable reference or a function call, check if the next token is ( to determine which it is */
			MATCH_TEXT(TOK_IDENTIFIER, &name);
			if (peek_token(ctx, NULL) == TOK_LEFT_PAREN)
			{
				symbol *sym;

				ESSL_CHECK_OOM(filter_symbol(ctx, &sym, _essl_symbol_table_lookup(ctx->current_scope, name)));
				if (!sym)
				{
					ERROR_CODE_STRING(ERR_LP_UNDEFINED_IDENTIFIER, "No matching function for call to '%s'\n", name);
					return NULL;
				}

				ESSL_CHECK_OOM(post = _essl_new_function_call_expression(ctx->pool, sym, 0));
				SET_NODE_POSITION(post);

				if(function_call_body(ctx, post) == NULL)
				{
					return NULL;
				}
	
				sym->is_used = 1;
				if (sym->kind != SYM_KIND_FUNCTION && sym->kind != SYM_KIND_BUILTIN_FUNCTION)
				{
					ERROR_CODE_STRING(ERR_SEM_TYPE_MISMATCH, "'%s' is not a function\n", name);
					return NULL;
				}
			} else {
				symbol *sym;
	
				ESSL_CHECK_OOM(filter_symbol(ctx, &sym, _essl_symbol_table_lookup(ctx->current_scope, name)));
				if(!sym)
				{
					ERROR_CODE_STRING(ERR_LP_UNDEFINED_IDENTIFIER, "Undeclared variable '%s'\n", name); 
					return NULL;
					/* 
					   OPT: insert an int variable to allow us to continue parsing.
					*/
				}
				sym->is_used = 1;

				if(sym->kind == SYM_KIND_TYPE)
				{
					ERROR_CODE_STRING(ERR_SEM_TYPE_USED_AS_NONTYPE, "Type '%s' referred to as a variable\n", name);
					return NULL;
				}
				if(sym->kind != SYM_KIND_VARIABLE)
				{
					SYNTAX_ERROR_PARAM("Symbol '%s' can't be referenced as a variable\n", _essl_string_to_cstring(ctx->pool,  name));
				}
				post = _essl_new_variable_reference_expression(ctx->pool, sym);
				ESSL_CHECK_OOM(post);
				SET_NODE_POSITION(post);
			}
		} else {
			CALL(post, primary_expression);
		}
	}

	for(;;)
	{
		Token tok = peek_token(ctx, NULL);
		switch(tok)
		{
		case TOK_LEFT_BRACKET:
			MATCH(TOK_LEFT_BRACKET);
			{
				node *idx;
				CALL(idx, integer_expression);
				MATCH(TOK_RIGHT_BRACKET);
				post = _essl_new_binary_expression(ctx->pool, post, EXPR_OP_INDEX, idx);
				ESSL_CHECK_OOM(post);
				SET_NODE_POSITION(post);
			}
			break;

		case TOK_DOT:
			MATCH(TOK_DOT);
			{
				string member = {"<dummy>", 7};
				MATCH_TEXT(TOK_IDENTIFIER, &member);
				post = _essl_new_unary_expression(ctx->pool, EXPR_OP_MEMBER_OR_SWIZZLE, post);
				ESSL_CHECK_OOM(post);
				SET_NODE_POSITION(post);
				post->expr.u.member_string = member;
			}
			break;

		case TOK_INC_OP:
		case TOK_DEC_OP:
			{
				expression_operator op = EXPR_OP_UNKNOWN;
				switch (get_token(ctx, NULL))
				{
				case TOK_INC_OP:
					op = EXPR_OP_POST_INC;
					break;
				case TOK_DEC_OP:
					op = EXPR_OP_POST_DEC;
					break;
				default:
					assert(0);
					break;
				}
				post = _essl_new_unary_expression(ctx->pool, op, post);
				ESSL_CHECK_OOM(post);
				SET_NODE_POSITION(post);
				LOGMSG("<= postfix_expression");
				return post;
			}

		default:
			LOGMSG("<= postfix_expression");
			return post;
		}
	}
}


/*@null@*/ static node *unary_expression(parser_context *ctx)
{
	node *tmp;
	expression_operator op;
	LOGMSG("=> unary_expression");
	switch(peek_token(ctx, NULL))
	{
	case TOK_PLUS:
		MATCH(TOK_PLUS);
		op = EXPR_OP_PLUS;
		break;
	case TOK_INC_OP:
		MATCH(TOK_INC_OP);
		op = EXPR_OP_PRE_INC;
		break;
	case TOK_DEC_OP:
		MATCH(TOK_DEC_OP);
		op = EXPR_OP_PRE_DEC;
		break;
	case TOK_DASH:
		MATCH(TOK_DASH);
		op = EXPR_OP_NEGATE;
		break;
	case TOK_BANG:
		MATCH(TOK_BANG);
		op = EXPR_OP_NOT;
		break;
	default:
		CALL(tmp, postfix_expression);
		LOGMSG("<= unary_expression");
		return tmp;
	}
	CALL(tmp, unary_expression);
	tmp = _essl_new_unary_expression(ctx->pool, op, tmp);
	ESSL_CHECK_OOM(tmp);
	SET_NODE_POSITION(tmp);
	LOGMSG("<= unary_expression");
	return tmp;
}

/*@null@*/ static node *multiplicative_expression(parser_context *ctx)
{
	node *tmp;
	node *tmp2;
	expression_operator op;
	LOGMSG("=> multiplicative_expression");
	CALL(tmp, unary_expression);
	for(;;)
	{
		switch (peek_token(ctx, NULL))
		{
			case TOK_STAR: op = EXPR_OP_MUL; break;
			case TOK_SLASH: op = EXPR_OP_DIV; break;
			default:
			{
				LOGMSG("<= multiplicative_expression");
				return tmp;
			}
		}
		(void)get_token(ctx, NULL);
		CALL(tmp2, unary_expression);
		tmp = _essl_new_binary_expression(ctx->pool, tmp, op, tmp2);
		ESSL_CHECK_OOM(tmp);
		SET_NODE_POSITION(tmp);
	}
}

/*@null@*/ static node *additive_expression(parser_context *ctx)
{
	node *tmp;
	node *tmp2;
	expression_operator op;
	LOGMSG("=> additive_expression");
	CALL(tmp, multiplicative_expression);
	for(;;)
	{
		switch (peek_token(ctx, NULL))
		{
			case TOK_PLUS: op = EXPR_OP_ADD; break;
			case TOK_DASH: op = EXPR_OP_SUB; break;
			default: return tmp;
		}
		(void)get_token(ctx, NULL);
		CALL(tmp2, multiplicative_expression);
		tmp = _essl_new_binary_expression(ctx->pool, tmp, op, tmp2);
		ESSL_CHECK_OOM(tmp);
		SET_NODE_POSITION(tmp);
	}
}

/*@null@*/ static node *relational_expression(parser_context *ctx)
{
	node *tmp;
	node *tmp2;
	expression_operator op;
	LOGMSG("=> relational_expression");
	CALL(tmp, additive_expression);
	for(;;)
	{
		switch (peek_token(ctx, NULL))
		{
			case TOK_LEFT_ANGLE: op = EXPR_OP_LT; break;
			case TOK_RIGHT_ANGLE: op = EXPR_OP_GT; break;
			case TOK_LE_OP: op = EXPR_OP_LE; break;
			case TOK_GE_OP: op = EXPR_OP_GE; break;
			default: return tmp;
		}
		(void)get_token(ctx, NULL);
		CALL(tmp2, additive_expression);
		tmp = _essl_new_binary_expression(ctx->pool, tmp, op, tmp2);
		ESSL_CHECK_OOM(tmp);
		SET_NODE_POSITION(tmp);
	}
}

/*@null@*/ static node *equality_expression(parser_context *ctx)
{
	node *tmp;
	node *tmp2;
	expression_operator op;
	LOGMSG("=> equality_expression");
	CALL(tmp, relational_expression);
	for(;;)
	{
		switch (peek_token(ctx, NULL))
		{
			case TOK_EQ_OP: op = EXPR_OP_EQ; break;
			case TOK_NE_OP: op = EXPR_OP_NE; break;
			default: return tmp;
		}
		(void)get_token(ctx, NULL);
		CALL(tmp2, relational_expression);
		tmp = _essl_new_binary_expression(ctx->pool, tmp, op, tmp2);
		ESSL_CHECK_OOM(tmp);
		SET_NODE_POSITION(tmp);
	}
}

#define SIMPLE_BINARY_EXPRESSION(name, chained, binary_op, token_op) \
/*@null@*/ static node *name(parser_context *ctx) \
{ \
	node *tmp; \
	LOGMSG("=> " #name); \
	CALL(tmp, chained); \
	while(peek_token(ctx, NULL) == token_op) \
	{ \
		node *tmp2; \
		MATCH(token_op); \
		CALL(tmp2, chained); \
		tmp = _essl_new_binary_expression(ctx->pool, tmp, binary_op, tmp2); \
		ESSL_CHECK_OOM(tmp); \
		SET_NODE_POSITION(tmp); \
	} \
	return tmp; \
}

SIMPLE_BINARY_EXPRESSION(logical_and_expression, equality_expression, EXPR_OP_LOGICAL_AND, TOK_AND_OP)
SIMPLE_BINARY_EXPRESSION(logical_xor_expression, logical_and_expression, EXPR_OP_LOGICAL_XOR, TOK_XOR_OP)
SIMPLE_BINARY_EXPRESSION(logical_or_expression, logical_xor_expression, EXPR_OP_LOGICAL_OR, TOK_OR_OP)

/*@null@*/ static node *conditional_expression(parser_context *ctx)
{
	node *tmp;
	LOGMSG("=> conditional_expression");
	CALL(tmp, logical_or_expression);
	if (peek_token(ctx, NULL) == TOK_QUESTION)
	{
		node *truepart;
		node *falsepart;
		MATCH(TOK_QUESTION);
		CALL(truepart, expression);
		MATCH(TOK_COLON);
		CALL(falsepart, assignment_expression);
		tmp = _essl_new_ternary_expression(ctx->pool, EXPR_OP_CONDITIONAL, tmp, truepart, falsepart);
		ESSL_CHECK_OOM(tmp);
		SET_NODE_POSITION(tmp);
	}
	LOGMSG("<= conditional_expression");
	return tmp;
}

/*@null@*/ static node *assignment_expression(parser_context *ctx)
{
	node *lhs;
	node *rhs;
	expression_operator op;
	LOGMSG("=> assignment_expression");
	CALL(lhs, conditional_expression);
	switch(peek_token(ctx, NULL))
	{
	case TOK_ASSIGN:
		op = EXPR_OP_ASSIGN;
		break;
	case TOK_MUL_ASSIGN:
		op = EXPR_OP_MUL_ASSIGN;
		break;
	case TOK_DIV_ASSIGN:
		op = EXPR_OP_DIV_ASSIGN;
		break;
	case TOK_ADD_ASSIGN:
		op = EXPR_OP_ADD_ASSIGN;
		break;
	case TOK_SUB_ASSIGN:
		op = EXPR_OP_SUB_ASSIGN;
		break;
	default:
		return lhs;
	}


	(void)get_token(ctx, NULL); /* consume operator token */
	CALL(rhs, assignment_expression);
	lhs = _essl_new_assign_expression(ctx->pool, lhs, op, rhs);
	ESSL_CHECK_OOM(lhs);
	SET_NODE_POSITION(lhs);
	return lhs;
}

/*@null@*/ static node *expression(parser_context *ctx)
{
	node *tmp;
	node *tmp2;
	LOGMSG("=> expression");
	CALL(tmp, assignment_expression);
	while (peek_token(ctx, NULL) == TOK_COMMA)
	{
		MUST_MATCH(TOK_COMMA);
		CALL(tmp2, assignment_expression);
		tmp = _essl_new_binary_expression(ctx->pool, tmp, EXPR_OP_CHAIN, tmp2);
		ESSL_CHECK_OOM(tmp);
		SET_NODE_POSITION(tmp);
	}
	return tmp;
}

/*@null@*/ static node *initializer(parser_context *ctx)
{
	node *tmp;
	LOGMSG("=> initializer");
	CALL(tmp, assignment_expression);
	return tmp;
}


/*@null@*/ static node *expression_statement(parser_context *ctx)
{
	node *expr = 0;
	LOGMSG("=> expression_statement");
	if (peek_token(ctx, NULL) != TOK_SEMICOLON)
	{
		CALL(expr, expression);
	}
	else
	{
		ESSL_CHECK_OOM(expr = _essl_new_compound_statement(ctx->pool));
	}
	MATCH(TOK_SEMICOLON);
	LOGMSG("<= expression_statement");
	return expr;
}

/*@null@*/ static node *condition(parser_context *ctx)
{
	LOGMSG("=> condition");
	if (type_lookahead(ctx) && TOK_LEFT_PAREN != peek_token2(ctx, NULL))
	{
		node *init;
		type_specifier *type;
		string toktext = {"<dummy>", 7};
		symbol *sym = 0;
		node *cond;
		int source_offset;
		qualifier_set qual;
		_essl_init_qualifier_set(&qual);
		CALL_TWOPARAMS(type, specified_type, TYPESPEC_FULLY_SPECIFIED_TYPE, &qual);
		
		source_offset = _essl_preprocessor_get_source_offset(ctx->prep_context);
		MATCH_TEXT(TOK_IDENTIFIER, &toktext);

		MATCH(TOK_ASSIGN);
		CALL(init, initializer);

		if (_essl_symbol_table_lookup_current_scope(ctx->current_scope, toktext) != NULL)
		{
			ERROR_CODE_STRING(ERR_SEM_NAME_REDEFINITION, "Symbol '%s' redeclared\n", toktext);
			return 0;
		}
		sym = _essl_new_variable_symbol(ctx->pool, toktext, type, qual, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, source_offset);
		ESSL_CHECK_OOM(sym);
		ESSL_CHECK_OOM(_essl_symbol_scope_insert(ctx->current_scope, toktext, sym));
		cond = _essl_new_variable_declaration(ctx->pool, sym, init);
		ESSL_CHECK_OOM(cond);
		SET_NODE_POSITION(cond);
		return cond;
	} else {
		node *c;
		CALL(c, expression);
		return c;
	}

}

/*@null@*/ static node *constant_expression(parser_context *ctx)
{
	node *n;
	CALL(n, conditional_expression);

	return n;
}

/**
Parses a function prototype
Example: int myfunc(int parama, vec4 paramb) { ... }
(int parama, vec4 paramb) is handled by function_prototype
'int' gets specified in the return_type and 'myfunc' in fun_name
This function is used both for declarations: int foo(void); and definitions: int foo(void) { ... },
it only consumes tokens up to the final ')'

The user function is stored in 'sym', params are stored as a linked list starting in 'parameters'
first_child will be filled later if this is a definition
*/
/*@null@*/ static node *function_prototype(parser_context *ctx, const type_specifier *return_type, qualifier_set return_qualifier, const string fun_name)
{
	node *func;
	symbol *sym;
	symbol *prev_sym;
	single_declarator *first_param = 0;
	unsigned dummy_name_count = 0;
	int source_offset = _essl_preprocessor_get_source_offset(ctx->prep_context);
	LOGMSG("=> function_prototype");
	MATCH(TOK_LEFT_PAREN);

	if (ctx->current_scope != ctx->global_scope)
	{
		ERROR_CODE_STRING(ERR_SEM_FUNCTION_INSIDE_FUNCTION, "Function '%s' declared or defined inside function\n", fun_name);
		return NULL;
	}

	if (peek_token(ctx, NULL) == TOK_VOID)
	{
		MUST_MATCH(TOK_VOID);
	} else if (peek_token(ctx, NULL) == TOK_RIGHT_PAREN)
	{
	} else {
		single_declarator *next_param;
		single_declarator *current_param = 0;
		for(;;)
		{
			qualifier_set qual;
			string parm_name = {"<dummy>", 7};
			type_specifier *type;
			int source_offset = 0;
			_essl_init_qualifier_set(&qual);

			CALL_TWOPARAMS(type, specified_type, TYPESPEC_PARAMETER_DECLARATION, &qual);
			ESSL_CHECK_OOM(type);
			if (peek_token(ctx, NULL) == TOK_IDENTIFIER)
			{
				source_offset = _essl_preprocessor_get_source_offset(ctx->prep_context);
				MATCH_TEXT(TOK_IDENTIFIER, &parm_name);
				if (!valid_identifier_name(ctx, parm_name))
				{
					ERROR_CODE_STRING(ERR_LP_ILLEGAL_IDENTIFIER_NAME, "Illegal identifier name '%s'\n", parm_name);
				}
			} else {
				char *dummy_name_buf;
				ESSL_CHECK_OOM(dummy_name_buf = _essl_mempool_alloc(ctx->pool, 18));
				snprintf(dummy_name_buf, 18, "<dummy>%u", dummy_name_count++);
				parm_name.ptr = dummy_name_buf;
				parm_name.len = strlen(parm_name.ptr);
			}

			if (peek_token(ctx, NULL) == TOK_LEFT_BRACKET)
			{
				node *constant;
				type_specifier *arrtype;
				MUST_MATCH(TOK_LEFT_BRACKET);
				CALL(constant, constant_expression);
				ESSL_CHECK_OOM(arrtype = _essl_new_unresolved_array_of_type(ctx->pool, type, constant));
				type = arrtype;
				MATCH(TOK_RIGHT_BRACKET);
			}

			next_param = _essl_new_single_declarator(ctx->pool, type, qual, &parm_name, NULL, source_offset);
			ESSL_CHECK_OOM(next_param);
			if (current_param)
			{
				current_param->next = next_param;
				assert(!next_param->next);
				current_param = next_param;
			}
			else
			{
				first_param = next_param;
				assert(!next_param->next);
				current_param = next_param;
			}
			
			if (peek_token(ctx, NULL) != TOK_COMMA) break;
			MUST_MATCH(TOK_COMMA);
		}
	}
	MATCH(TOK_RIGHT_PAREN);

	sym = _essl_new_function_symbol(ctx->pool, fun_name, return_type, return_qualifier, source_offset);
	ESSL_CHECK_OOM(sym);
	sym->parameters = first_param;

	/* Not enough information yet to fully check for duplicates, so we just insert every declaration */
	ESSL_CHECK_OOM(filter_symbol(ctx, &prev_sym, _essl_symbol_table_lookup_current_scope(ctx->current_scope, fun_name)));
	sym->next = prev_sym;
	ESSL_CHECK_OOM(_essl_symbol_scope_insert(ctx->current_scope, fun_name, sym));

	if (prev_sym && prev_sym->kind != SYM_KIND_FUNCTION && prev_sym->kind != SYM_KIND_BUILTIN_FUNCTION)
	{
		ERROR_CODE_STRING(ERR_SEM_NAME_REDEFINITION, "Symbol '%s' redeclared as a function\n", fun_name);
		return 0;
	}

	func = _essl_new_function_declaration(ctx->pool, sym);
	ESSL_CHECK_OOM(func);
	SET_NODE_POSITION(func);
	return func;
}

/*@null@*/ static node *if_statement(parser_context *ctx)
{
	node *cond;
	node *truepart = 0;
	node *falsepart = 0;
	node *ifstmt;
	LOGMSG("=> if_statement");
	MATCH(TOK_IF);
	MATCH(TOK_LEFT_PAREN);
	CALL(cond, expression);
	MATCH(TOK_RIGHT_PAREN);
	CALL_TWOPARAMS(truepart, statement, SCOPE_COMPOUND_CREATE, 0);
	if (peek_token(ctx, NULL) == TOK_ELSE)
	{
		MUST_MATCH(TOK_ELSE);
		CALL_TWOPARAMS(falsepart, statement, SCOPE_COMPOUND_CREATE, 0);
	}
	ifstmt = _essl_new_if_statement(ctx->pool, cond, truepart, falsepart);
	ESSL_CHECK_OOM(ifstmt);
	SET_NODE_POSITION(ifstmt);
	return ifstmt;
}

/*@null@*/ static node *iteration_statement(parser_context *ctx)
{
	int prev_iteration = ctx->current_iteration;
	LOGMSG("=> iteration_statement");
	switch(peek_token(ctx, NULL))
	{
	case TOK_WHILE:
		MATCH(TOK_WHILE);
		{
			scope *newscope;
			node *cond;
			node *body = 0;
			node *whilestmt;
			scope *oldscope = ctx->current_scope;

			newscope = _essl_symbol_table_begin_scope(ctx->current_scope);
			ESSL_CHECK_OOM(newscope);

			ctx->current_scope = newscope;
			MATCH_OR_UNWIND_SCOPE(TOK_LEFT_PAREN, oldscope);

			if (!(cond = condition(ctx)))
			{
				ctx->current_scope = oldscope;
				ctx->current_iteration = prev_iteration;
				return 0;
			}

			MATCH_OR_UNWIND_SCOPE(TOK_RIGHT_PAREN, oldscope);

			LOGMSG("Reading body");
			ctx->current_iteration = 1;
			if ((body = statement(ctx, SCOPE_MUST_KEEP, 0)) == 0)
			{
				ctx->current_scope = oldscope;
				ctx->current_iteration = prev_iteration;
				LOGMSG("Body failed");
				return 0;
			}

			ctx->current_iteration = prev_iteration;
			ctx->current_scope = oldscope;

			whilestmt = _essl_new_while_statement(ctx->pool, cond, body);
			ESSL_CHECK_OOM(whilestmt);

			whilestmt->stmt.child_scope = newscope;
			SET_NODE_POSITION(whilestmt);
			LOGMSG("<= iteration_statement");
			return whilestmt;
		}
	case TOK_DO:
		MATCH(TOK_DO);
		{
			scope *newscope;
			node *body = 0;
			node *cond;
			node *dostmt;
			scope *oldscope = ctx->current_scope;
			ctx->current_iteration = 1;
			newscope = _essl_symbol_table_begin_scope(ctx->current_scope);
			ESSL_CHECK_OOM(newscope);
			ctx->current_scope = newscope;

			if((body = statement(ctx, SCOPE_MUST_KEEP, 0)) == 0)
			{
				ctx->current_scope = oldscope;
				ctx->current_iteration = prev_iteration;
				return 0;
			}

			ctx->current_scope = oldscope;

			MATCH_OR_UNWIND_SCOPE(TOK_WHILE, oldscope);
			MATCH_OR_UNWIND_SCOPE(TOK_LEFT_PAREN, oldscope);

			if (!(cond = expression(ctx)))
			{
				ctx->current_scope = oldscope;
				ctx->current_iteration = prev_iteration;
				return 0;
			}

			MATCH_OR_UNWIND_SCOPE(TOK_RIGHT_PAREN, oldscope);
			MATCH_OR_UNWIND_SCOPE(TOK_SEMICOLON, oldscope);

			ctx->current_iteration = prev_iteration;

			dostmt = _essl_new_do_statement(ctx->pool, body, cond);
			ESSL_CHECK_OOM(dostmt);

			dostmt->stmt.child_scope = newscope;
			SET_NODE_POSITION(dostmt);
			LOGMSG("<= iteration_statement");
			return dostmt;
		}
	case TOK_FOR:
		MATCH(TOK_FOR);
		{
			scope *newscope;
			node *init = 0;
			node *cond = 0;
			node *iter = 0;
			node *body = 0;
			node *forstmt;
			scope *oldscope = ctx->current_scope;

			newscope = _essl_symbol_table_begin_scope(ctx->current_scope);
			ESSL_CHECK_OOM(newscope);

			ctx->current_scope = newscope;

			MATCH_OR_UNWIND_SCOPE(TOK_LEFT_PAREN, oldscope);

			if(peek_token(ctx, NULL) != TOK_SEMICOLON)
			{
				/* Make sure it's not a constructor */
				if ((type_lookahead(ctx) && TOK_LEFT_PAREN != peek_token2(ctx, NULL)) || peek_token(ctx, NULL) == TOK_PRECISION)
				{
					if ((init = declaration(ctx, 0)) == 0)
					{
						ctx->current_scope = oldscope;
						return 0;
					}
					if(init->hdr.kind == STMT_KIND_COMPOUND && GET_N_CHILDREN(init) == 0)
					{
						init = 0;
					}
				}
				else
				{
					/* Expression statement */
					if ((init = expression(ctx)) == 0)
					{
						ctx->current_scope = oldscope;
						return 0;
					}
				}
			}

			MATCH_OR_UNWIND_SCOPE(TOK_SEMICOLON, oldscope);
			if(peek_token(ctx, NULL) != TOK_SEMICOLON)
			{
				if (!(cond = condition(ctx)))
				{
					ctx->current_scope = oldscope;
					return 0;
				}
			} else {
				/* empty condition, insert true here */
				cond = _essl_new_constant_expression(ctx->pool, 1);
				ESSL_CHECK_OOM(cond);
				SET_NODE_POSITION(cond);
				cond->hdr.type = _essl_get_type(ctx->typestor_context, TYPE_BOOL, 1);
				ESSL_CHECK_OOM(cond->hdr.type);
				cond->expr.u.value[0] = ctx->target_desc->bool_to_scalar(1);
			}
			MATCH_OR_UNWIND_SCOPE(TOK_SEMICOLON, oldscope);
			if(peek_token(ctx, NULL) != TOK_RIGHT_PAREN)
			{
				if (!(iter = expression(ctx)))
				{
					ctx->current_scope = oldscope;
					return 0;
				}
			}
			MATCH_OR_UNWIND_SCOPE(TOK_RIGHT_PAREN, oldscope);
			ctx->current_iteration = 1;
			if ((body = statement(ctx, SCOPE_MUST_KEEP, 0)) == 0)
			{
				ctx->current_iteration = prev_iteration;
				ctx->current_scope = oldscope;
				return 0;
			}
			ctx->current_iteration = prev_iteration;

			ctx->current_scope = oldscope;


			forstmt = _essl_new_for_statement(ctx->pool, init, cond, iter, body);
			ESSL_CHECK_OOM(forstmt);
			forstmt->stmt.child_scope = newscope;
			SET_NODE_POSITION(forstmt);
			LOGMSG("<= iteration_statement");
			return forstmt;
		}

	default:
		INTERNAL_ERROR_MSG("Unexpected iteration token\n");
		return 0;
	}
}

/*@null@*/ static node *flow_control_statement(parser_context *ctx)
{
	node *returnvalue = 0;
	node *flow;
	const type_specifier *type = 0;
	node_kind stmtop;
	Token tok = get_token(ctx, NULL);
	LOGMSG("=> flow_control_statement");
	switch(tok)
	{
	case TOK_CONTINUE:
		stmtop = STMT_KIND_CONTINUE;
		if(ctx->current_iteration == 0)
		{
			SYNTAX_ERROR_MSG("continue used outside of loop body\n");
			return 0;
		}
		break;
	case TOK_BREAK:
		stmtop = STMT_KIND_BREAK;
		if(ctx->current_iteration == 0)
		{
			SYNTAX_ERROR_MSG("break used outside of loop body\n");
			return 0;
		}

		break;
	case TOK_DISCARD:
		stmtop = STMT_KIND_DISCARD;
		break;

	case TOK_RETURN:
		stmtop = STMT_KIND_RETURN;
		assert(ctx->current_function != 0);
		type = ctx->current_function->decl.sym->type;
		if (peek_token(ctx, NULL) != TOK_SEMICOLON)
		{
			CALL(returnvalue, expression);
		}
		break;

	default:
		INTERNAL_ERROR_MSG("Unexpected flow control token\n");
		return 0;
	}
	MATCH(TOK_SEMICOLON);
	flow = _essl_new_flow_control_statement(ctx->pool, stmtop, returnvalue);
	ESSL_CHECK_OOM(flow);
	flow->hdr.type = type;
	SET_NODE_POSITION(flow);
	return flow;
}

static /*@null@*/ node *compound_statement(parser_context *ctx, scope_creation_kind new_scope)
{
	node *compound_stmt = 0;
	scope *oldscope = ctx->current_scope;

	ESSL_CHECK_OOM(compound_stmt = _essl_new_compound_statement(ctx->pool));
	SET_NODE_POSITION(compound_stmt);
	if (new_scope != SCOPE_MUST_KEEP)
	{
		scope *newscope;
		newscope = _essl_symbol_table_begin_scope(ctx->current_scope);
		ESSL_CHECK_OOM(newscope);
		ctx->current_scope = newscope;
	}

	MATCH(TOK_LEFT_BRACE);

	while(peek_token(ctx, NULL) != TOK_RIGHT_BRACE && peek_token(ctx, NULL) != TOK_END_OF_FILE)
	{
		if ((statement(ctx, SCOPE_COMPOUND_CREATE, compound_stmt)) == 0)
		{
			Token tok = peek_token(ctx, NULL);
			int nested_level = 0;
			while (tok != TOK_END_OF_FILE)
			{
				(void)get_token(ctx, NULL);
				if (tok == TOK_SEMICOLON)
				{
					if (nested_level <= 0)
					{
						break;
					}
				}
				else if (tok == TOK_RIGHT_BRACE)
				{
					nested_level--;
					if (nested_level <= 0)
					{
						break;
					}
				}
				else if (tok == TOK_LEFT_BRACE)
				{
					nested_level++;
				}
				tok = peek_token(ctx, NULL);
			}
		}
	}

	MATCH(TOK_RIGHT_BRACE);

	if (new_scope != SCOPE_MUST_KEEP)
	{
		scope *compound_scope = ctx->current_scope;
		ctx->current_scope = oldscope;
		compound_stmt->stmt.child_scope = compound_scope;
	}

	return compound_stmt;
}


/**
   Used by declaration().
   Takes in a type that has already been parsed, and returns the variables and initializers that are declared with this type 
*/

/*@null@*/ static node *init_declarator_list(parser_context *ctx, const type_specifier *initial_type, qualifier_set initial_qual, string name, int is_invariant, /*@null@*/ node *parent)
{
	node *init = 0;
	node *tmp;
	const type_specifier *type;
	symbol *sym = 0;

	LOGMSG("=> init_declarator_list");
	type = initial_type;
	if(parent == 0)
	{
		ESSL_CHECK_OOM(parent = _essl_new_compound_statement(ctx->pool));
	}

	for(;;)
	{
		/* since we receive a half-parsed type as an argument, this loop is a bit backwards */
		int source_offset = _essl_preprocessor_get_source_offset(ctx->prep_context);
		switch (peek_token(ctx, NULL))
		{
		case TOK_LEFT_BRACKET:
			{
				node *const_expr;
				type_specifier *newtype;
				MUST_MATCH(TOK_LEFT_BRACKET);
				CALL(const_expr, constant_expression);
				ESSL_CHECK_OOM(newtype = _essl_new_unresolved_array_of_type(ctx->pool, initial_type, const_expr));
				type = newtype;
				MATCH(TOK_RIGHT_BRACKET);
				break;
			}
		case TOK_ASSIGN:
			MUST_MATCH(TOK_ASSIGN);
			CALL(init, initializer);
			break;
		default:
			break;
		}

		if (_essl_symbol_table_lookup_current_scope(ctx->current_scope, name))
		{
			ERROR_CODE_STRING(ERR_SEM_NAME_REDEFINITION, "Symbol '%s' redeclared\n", name);
		} else {
			sym = _essl_new_variable_symbol(ctx->pool, name, type, initial_qual, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, source_offset);
			ESSL_CHECK_OOM(sym);
			if (is_invariant)
			{
				sym->is_invariant = 1;
			}
			
			ESSL_CHECK_OOM(_essl_symbol_scope_insert(ctx->current_scope, name, sym));

			ESSL_CHECK_OOM(tmp = _essl_new_variable_declaration(ctx->pool, sym, init));
			SET_NODE_POSITION(tmp);
			ESSL_CHECK_OOM(APPEND_CHILD(parent, tmp, ctx->pool));
		}

		if (peek_token(ctx, NULL) != TOK_COMMA) break;
		MUST_MATCH(TOK_COMMA);
		type = initial_type;
		MATCH_TEXT(TOK_IDENTIFIER, &name);

		init = 0;
	}
	LOGMSG("<= init_declarator_list");
	return parent;
}

static node *declaration(parser_context *ctx, node *parent)
{
	node *d;
	string toktext = {"<dummy>", 7};
	const type_specifier *type;
	int invariant = 0;
	qualifier_set qual;
	LOGMSG("=> declaration");
	_essl_init_qualifier_set(&qual);

	if (peek_token(ctx, NULL) == TOK_PRECISION)
	{
		precision_qualifier precision;
		type_basic b_type;
		essl_bool accepted_type_found = ESSL_FALSE;

		MUST_MATCH(TOK_PRECISION);

		switch (peek_token(ctx, NULL))
		{
		case TOK_LOWP:
			MUST_MATCH(TOK_LOWP);
			precision = PREC_LOW;
			break;
		case TOK_MEDIUMP:
			MUST_MATCH(TOK_MEDIUMP);
			precision = PREC_MEDIUM;
			break;
		case TOK_HIGHP:
			MUST_MATCH(TOK_HIGHP);
			if (ctx->lang_desc->target_desc->has_high_precision == 0 && !ctx->have_reported_highp_warning)
			{
				WARNING_CODE_MSG(ERR_PP_NO_HIGHP, "High precision not supported, instead compiling high precision as medium precision\n");
				ctx->have_reported_highp_warning = ESSL_TRUE;
			}
			precision = PREC_HIGH;
			break;
		default:
			(void)peek_token(ctx, &toktext);
			SYNTAX_ERROR_STRING("Expected precision qualifier, got '%s'\n", toktext);
			return NULL;
		}

		switch (peek_token(ctx, NULL))
		{
		case TOK_INT:
			MUST_MATCH(TOK_INT);
			b_type = TYPE_INT;
			accepted_type_found = ESSL_TRUE;
			break;
		case TOK_FLOAT:
			MUST_MATCH(TOK_FLOAT);
			b_type = TYPE_FLOAT;
			accepted_type_found = ESSL_TRUE;
			break;
		case TOK_SAMPLER2D:
			MUST_MATCH(TOK_SAMPLER2D);
			b_type = TYPE_SAMPLER_2D;
			accepted_type_found = ESSL_TRUE;
			break;
		case TOK_SAMPLERCUBE:
			MUST_MATCH(TOK_SAMPLERCUBE);
			b_type = TYPE_SAMPLER_CUBE;
			accepted_type_found = ESSL_TRUE;
			break;
		case TOK_SAMPLER3D:
			{
				extension_behavior ext = _essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_TEXTURE_3D);
				if (ext == BEHAVIOR_ENABLE || ext == BEHAVIOR_WARN)
				{
					MUST_MATCH(TOK_SAMPLER3D);
					b_type = TYPE_SAMPLER_3D;
					accepted_type_found = ESSL_TRUE;
				}
			}
			break;
		case TOK_SAMPLER2DSHADOW:
			{
				extension_behavior ext = _essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_EXT_SHADOW_SAMPLERS);
				if (ext == BEHAVIOR_ENABLE || ext == BEHAVIOR_WARN)
				{
					MUST_MATCH(TOK_SAMPLER2DSHADOW);
					b_type = TYPE_SAMPLER_2D_SHADOW;
					accepted_type_found = ESSL_TRUE;
				}
			}
			break;
		case TOK_SAMPLEREXTERNAL:
			{
				extension_behavior ext = _essl_get_extension_behavior(ctx->lang_desc, EXTENSION_GL_OES_TEXTURE_EXTERNAL);
				if (ext == BEHAVIOR_ENABLE || ext == BEHAVIOR_WARN)
				{
					MUST_MATCH(TOK_SAMPLEREXTERNAL);
					b_type = TYPE_SAMPLER_EXTERNAL;
					accepted_type_found = ESSL_TRUE;
				}
			}
			break;
		default:
			accepted_type_found = ESSL_FALSE;
		}

		if (!accepted_type_found)
		{
			(void)get_token(ctx, &toktext);
			SYNTAX_ERROR_STRING("Expected type for precision qualifier, got '%s'\n", toktext);
			return NULL;
		}

		d = _essl_new_precision_declaration(ctx->pool, b_type, precision);
		ESSL_CHECK_OOM(d);
		SET_NODE_POSITION(d);

		LOGMSG("<= declaration");
		if(parent != NULL)
		{
			ESSL_CHECK_OOM(APPEND_CHILD(parent, d, ctx->pool));
			return parent;
		} else {
			return d;
		}
	}
	if (peek_token(ctx, NULL) == TOK_INVARIANT)
	{
		MUST_MATCH(TOK_INVARIANT);

		if (ctx->current_scope != ctx->global_scope)
		{
			ERROR_CODE_MSG(ERR_SEM_INVARIANT_LOCAL_SCOPE, "All uses of invariant must be at global scope\n");
			return NULL;
		}

		invariant = 1;
		if (peek_token(ctx, NULL) != TOK_VARYING)
		{
			/* Handle cases like:
				varying mediump vec3 Color;
				invariant Color;    <------------
			*/

			symbol *sym;
			do {
				MATCH_TEXT(TOK_IDENTIFIER, &toktext);
				sym = _essl_symbol_table_lookup(ctx->current_scope, toktext);
				if (!sym || sym->kind != SYM_KIND_VARIABLE)
				{
					ERROR_CODE_STRING(ERR_SEM_INVARIANT_ONLY_OUTPUT_VARIABLES, "Non-variable '%s' declared invariant\n", toktext);
					return NULL;
				}
				/* user-declared varyings haven't been turned into address_space varying just yet, but built-in vertex varyings have, that's why we have a double test for varyings */
				if (! (sym->qualifier.variable == VAR_QUAL_VARYING || sym->address_space == ADDRESS_SPACE_VERTEX_VARYING || sym->address_space == ADDRESS_SPACE_FRAGMENT_VARYING || sym->address_space == ADDRESS_SPACE_FRAGMENT_SPECIAL || sym->address_space == ADDRESS_SPACE_FRAGMENT_OUT))
				{
					ERROR_CODE_MSG(ERR_SEM_INVARIANT_ONLY_OUTPUT_VARIABLES, "Variables of this kind cannot be declared invariant\n");
					return NULL;
				}
				if(sym->address_space == ADDRESS_SPACE_FRAGMENT_SPECIAL && !_essl_string_cmp(sym->name, _essl_cstring_to_string_nocopy("gl_FrontFacing")))
				{
					ERROR_CODE_MSG(ERR_SEM_INVARIANT_ONLY_OUTPUT_VARIABLES, "'gl_FrontFacing' cannot be declared invariant\n");
					return NULL;
				}

				if (sym->is_used != 0)
				{
					ERROR_CODE_STRING(ERR_SEM_INVARIANT_DECLARED_AFTER_USE, "invariant qualifier must be specified before any use of variable '%s'\n", toktext);
					return NULL;
				}
				sym->is_invariant = 1;
				if (peek_token(ctx, NULL) != TOK_COMMA)
				{
					if(!parent)
					{
						ESSL_CHECK_OOM(parent = _essl_new_compound_statement(ctx->pool));
					}
					return parent;
				}
				MUST_MATCH(TOK_COMMA);
			} while (1);
		}
	}
	CALL_TWOPARAMS(type, specified_type, TYPESPEC_FULLY_SPECIFIED_TYPE, &qual);
	if (peek_token(ctx, NULL) != TOK_IDENTIFIER)
	{
		/* This should be a struct declaration */
		if (type->basic_type == TYPE_STRUCT)
		{
			if(!parent)
			{
				ESSL_CHECK_OOM(parent = _essl_new_compound_statement(ctx->pool));
			}
			LOGMSG("<= declaration");
			return parent;
		}
		else
		{
			(void)peek_token(ctx, &toktext);
			SYNTAX_ERROR_STRING("Expected identifier, found '%s'\n", toktext);
			return NULL;
		}
	}

	MATCH_TEXT(TOK_IDENTIFIER, &toktext);

	if (!valid_identifier_name(ctx, toktext))
	{
		ERROR_CODE_STRING(ERR_LP_ILLEGAL_IDENTIFIER_NAME, "Illegal identifier name '%s'\n", toktext);
	}

	if (peek_token(ctx, NULL) == TOK_LEFT_PAREN)
	{
		if (invariant != 0)
		{
			ERROR_CODE_MSG(ERR_SEM_INVARIANT_ONLY_OUTPUT_VARIABLES, "Only output values can be declared invariant\n");
			return NULL;
		}
		ESSL_CHECK_NONFATAL(d = function_prototype(ctx, type, qual, toktext));

		LOGMSG("<= declaration");
		if(parent != NULL)
		{
			ESSL_CHECK_OOM(APPEND_CHILD(parent, d, ctx->pool));
			return parent;
		} else {
			return d;
		}
	} else {
		assert(invariant == 0 || qual.variable == VAR_QUAL_VARYING);
		ESSL_CHECK_NONFATAL(parent = init_declarator_list(ctx, type, qual, toktext, invariant, parent));

		LOGMSG("<= declaration");
		return parent;
	}
}

/*@null@*/ static node *declaration_statement(parser_context *ctx, /*@null@*/ node *parent)
{
	node *res;
	LOGMSG("=> declaration_statement");
	CALL_PARAM(res, declaration, parent);
	MATCH(TOK_SEMICOLON);
	return res;
}

/*@null@*/ static node *statement(parser_context *ctx, scope_creation_kind new_scope, node *parent)
{
	if (peek_token(ctx, NULL) == TOK_LEFT_BRACE)
	{
		node *n;
		CALL_PARAM(n, compound_statement, new_scope);
		if(parent)
		{
			ESSL_CHECK_OOM(APPEND_CHILD(parent, n, ctx->pool));
			return parent;
		} else {
			return n;
		}
	}
	else
	{
		return simple_statement(ctx, parent);
	}
}

/** 
	parse a statement that isn't a compound statement
*/
/*@null@*/ static node *simple_statement(parser_context *ctx, /*@null@*/ node *parent)
{
	node *stmt = 0;
	LOGMSG("=> simple_statement");

	/* If it's a type, make sure it's not a constructor */
	if ((type_lookahead(ctx) && TOK_LEFT_PAREN != peek_token2(ctx, NULL)) || peek_token(ctx, NULL) == TOK_PRECISION || peek_token(ctx, NULL) == TOK_INVARIANT)
	{
		CALL_PARAM(stmt, declaration_statement, parent);
		LOGMSG("<= simple_statement");
		return stmt;
	} else {
	
		switch (peek_token(ctx, NULL))
		{
		case TOK_IF:
			CALL(stmt, if_statement);
			break;

		case TOK_FOR:
		case TOK_WHILE:
		case TOK_DO:
			CALL(stmt, iteration_statement);
			break;

		case TOK_CONTINUE:
		case TOK_BREAK:
		case TOK_RETURN:
		case TOK_DISCARD:
			CALL(stmt, flow_control_statement);
			break;

		default:
			CALL(stmt, expression_statement);
			break;

		}
	}
	assert(stmt != 0);
	LOGMSG("<= simple_statement");
	if(parent)
	{
		ESSL_CHECK_OOM(APPEND_CHILD(parent, stmt, ctx->pool));
		return parent;
	}
	return stmt;
}

/**
   parse a declaration at the global scope 
*/
/*@null@*/ static node *external_declaration(parser_context *ctx, node *parent)
{
	node *decl = 0;
	scope *oldscope = ctx->current_scope;
	unsigned int n_children;
	LOGMSG("=> external_declaration");
	n_children = GET_N_CHILDREN(parent);
	if(declaration(ctx, parent) == NULL)
	{
		return NULL;
	}
	if (GET_N_CHILDREN(parent) > n_children)
	{
		/* we've got new stuff */
		decl = GET_CHILD(parent, GET_N_CHILDREN(parent) - 1);
		assert(decl != 0);
		if (peek_token(ctx, NULL) == TOK_LEFT_BRACE && decl->hdr.kind == DECL_KIND_FUNCTION)
		{
			node *compound = 0;
			single_declarator *sd;
			scope *child_scope = 0;

			assert(ctx->current_function == NULL);

			child_scope = _essl_symbol_table_begin_scope(ctx->current_scope);
			ESSL_CHECK_OOM(child_scope);
			
			sd = decl->decl.sym->parameters;
			while (sd)
			{
				if (sd->name.ptr != 0)
				{
					if (_essl_symbol_table_lookup_current_scope(child_scope, sd->name))
					{
						ERROR_CODE_STRING(ERR_SEM_VARIABLE_REDEFINITION, "Name '%s' used more than once as a parameter\n", sd->name);
						DISCARD_REST_OF_COMPOUND();
						return 0;
					}
					
					
					sd->sym = _essl_new_variable_symbol(ctx->pool, sd->name, sd->type, sd->qualifier, SCOPE_FORMAL, ADDRESS_SPACE_THREAD_LOCAL, sd->source_offset);
					ESSL_CHECK_OOM(sd->sym);
					ESSL_CHECK_OOM(_essl_symbol_scope_insert(child_scope, sd->name, sd->sym));
				}
				
				sd = sd->next;
			}
			
			ctx->current_scope = child_scope;
			ctx->current_function = decl;
			
			if ((compound = compound_statement(ctx, SCOPE_MUST_KEEP)) == 0)
			{
				ctx->current_scope = oldscope;
				ctx->current_function = NULL;
				return 0;
			}

			assert(GET_NODE_KIND(compound->hdr.kind) == NODE_KIND_STATEMENT);
			compound->stmt.child_scope = child_scope;
			SET_CHILD(decl, 0, compound);
			decl->decl.sym->body = compound;
			ctx->current_scope = oldscope;

			ctx->current_function = NULL;
			LOGMSG("<= external_declaration");
			return parent;
		}
	}

	MATCH(TOK_SEMICOLON);
	LOGMSG("<= external_declaration");
	return parent;
}

/*@null@*/ static node *parse_translation_unit(parser_context *ctx)
{
	node *tu = 0;
	LOGMSG("=> translation_unit");
	ESSL_CHECK_OOM(tu = _essl_new_translation_unit(ctx->pool, ctx->global_scope));

	while (peek_token(ctx, NULL) != TOK_END_OF_FILE)
	{
		node *tmp = external_declaration(ctx, tu);
		if (!tmp)
		{
			break;
		}
	}

	{
		scope_iter it;
		symbol *sym;
		_essl_symbol_table_iter_init(&it, ctx->global_scope);
		while((sym = _essl_symbol_table_next(&it)) != 0)
		{
			if(sym->kind == SYM_KIND_VARIABLE && sym->address_space == ADDRESS_SPACE_THREAD_LOCAL)
			{
				switch(sym->qualifier.variable)
				{
				case VAR_QUAL_NONE:
				case VAR_QUAL_CONSTANT:
					sym->address_space = ADDRESS_SPACE_GLOBAL;
					break;

				case VAR_QUAL_ATTRIBUTE:
					sym->address_space = ADDRESS_SPACE_ATTRIBUTE;
					break;

				case VAR_QUAL_VARYING:
					switch(ctx->target_desc->kind)
					{
					case TARGET_VERTEX_SHADER:
						sym->address_space = ADDRESS_SPACE_VERTEX_VARYING;
						break;

					case TARGET_FRAGMENT_SHADER:
						sym->address_space = ADDRESS_SPACE_FRAGMENT_VARYING;
						break;
					case TARGET_UNKNOWN:
						assert(0);
						break;
					}
					break;

				case VAR_QUAL_UNIFORM:
					sym->address_space = ADDRESS_SPACE_UNIFORM;
					break;

				case VAR_QUAL_PERSISTENT:
					sym->is_persistent_variable = 1;
					sym->address_space = ADDRESS_SPACE_GLOBAL;
					break;

				}
			}

		}


	}
	
	ESSL_CHECK_OOM(tu);
	SET_NODE_POSITION(tu);
	LOGMSG("<= translation_unit");
	return tu;
}
/** @} */

node *_essl_parse_translation_unit(parser_context *ctx)
{
	return parse_translation_unit(ctx);
}

node *_essl_parse_expression(parser_context *ctx)
{
	return expression(ctx);
}

memerr _essl_parser_init(parser_context *ctx, mempool *pool, preprocessor_context *p_ctx, error_context *e_ctx, typestorage_context *ts_ctx, target_descriptor *desc, language_descriptor *lang)
{
	ctx->pool = pool;
	ctx->prep_context = p_ctx;
	ctx->err_context = e_ctx;
	ctx->typestor_context = ts_ctx;
	ctx->prev_token = TOK_UNKNOWN;
	ctx->prev_token2 = TOK_UNKNOWN;
	ctx->prev_string.ptr = "<dummy>";
	ctx->prev_string.len = 7;
	ctx->prev_string2.ptr = "<dummy>";
	ctx->prev_string2.len = 7;
	ctx->target_desc = desc;
	ctx->have_reported_highp_warning = ESSL_FALSE;
	ctx->struct_declaration_stack = NULL;
	ESSL_CHECK_NONFATAL(ctx->global_scope = _essl_mempool_alloc(pool, sizeof(scope)));
	ESSL_CHECK_NONFATAL(_essl_symbol_scope_init(ctx->global_scope, pool));
	
	ctx->current_scope = ctx->global_scope;
	ctx->current_function = 0;
	ctx->current_iteration = 0;
	ctx->lang_desc = lang;

	return MEM_OK;
}

#undef CALL



#ifdef UNIT_TEST


typedef struct _tag_answer_type
{
	node_kind kind;
	expression_operator expr_op;
} answer_type;



static void scan_test(mempool *pool, target_descriptor *desc)
{
	frontend_context *frontend;
	parser_context *ctx;
	char buf[] = "vec4 in mediump test true False Vec4";
	int len = strlen(buf);

	frontend = _essl_new_frontend(pool, desc, buf, &len, 1);
	assert(frontend);

	ctx = &frontend->parse_context;

	assert(TOK_IN == peek_token2(ctx, NULL));
	assert(TOK_VEC4 == peek_token(ctx, NULL));
	assert(TOK_IN == peek_token2(ctx, NULL));
	assert(TOK_VEC4 == peek_token(ctx, NULL));

	assert(TOK_VEC4 == get_token(ctx, NULL));
	assert(TOK_IN == peek_token(ctx, NULL));

	assert(TOK_IN == get_token(ctx, NULL));
	assert(TOK_MEDIUMP == peek_token(ctx, NULL));
	assert(TOK_IDENTIFIER == peek_token2(ctx, NULL));
	assert(TOK_MEDIUMP == peek_token(ctx, NULL));
	assert(TOK_IDENTIFIER == peek_token2(ctx, NULL));

	assert(TOK_MEDIUMP == get_token(ctx, NULL));
	assert(TOK_IDENTIFIER == peek_token(ctx, NULL));
	assert(TOK_TRUE == peek_token2(ctx, NULL));
}



static int check_node(node *n, answer_type *answer, int index)
{
	int current_index = index;
	if (n != NULL)
	{
		unsigned int i;

		if (answer[index].kind != n->hdr.kind)
		{
			return -1;
		}

		if (GET_NODE_KIND(answer[index].kind) == NODE_KIND_EXPRESSION)
		{
			if (answer[index].expr_op != n->expr.operation)
			{
				return -1;
			}
		}

		current_index++;

		/* ok, I'm good, check my childs */

		for (i = 0; i < GET_N_CHILDREN(n); i++)
		{
			node *child = GET_CHILD(n, i);
			if (child != NULL)
			{
				current_index = check_node(child, answer, current_index);
				if (current_index < 0)
				{
					return -1;
				}
			}
		}
	}

	if (index == 0)
	{
		/* we have checked the entire node tree, verify that answer now ends with NODE_KIND_UNKNOWN */
		if (answer[current_index].kind != NODE_KIND_UNKNOWN)
		{
			return -1;
		}
	}

	return current_index;
}


static int simple_test(mempool *pool, target_descriptor *desc)
{
	char *input[] = {
		"2 + 7",
		"6 + 7 * 3 + 8.33 - 1 - (8 - 2)",
		"2 + 09",
		"vec4 user_func(int param);",
		"void user_func2(void);",
		"bool user_func3(ivec3, in int inparm, inout bool, out bvec2 outvec);",
		"void simple_func_def(void) {}",
		"float ret_func(vec2 hello) { hello[0] *= 2; hello[0] += hello[1]; return 2.0 * hello[0]; }",
	};

	answer_type answer0[] =
	{
		{ EXPR_KIND_BINARY, EXPR_OP_ADD },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ NODE_KIND_UNKNOWN, EXPR_OP_UNKNOWN }
	};

	answer_type answer1[] =
	{
		{ EXPR_KIND_BINARY, EXPR_OP_SUB },
		{ EXPR_KIND_BINARY, EXPR_OP_SUB },
		{ EXPR_KIND_BINARY, EXPR_OP_ADD },
		{ EXPR_KIND_BINARY, EXPR_OP_ADD },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_BINARY, EXPR_OP_MUL },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_BINARY, EXPR_OP_SUB },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ NODE_KIND_UNKNOWN, EXPR_OP_UNKNOWN }
	};

	answer_type answer2[] =
	{
		{ NODE_KIND_UNKNOWN, EXPR_OP_UNKNOWN }
	};

	answer_type answer3[] =
	{
		{ NODE_KIND_TRANSLATION_UNIT, EXPR_OP_UNKNOWN },
		{ DECL_KIND_FUNCTION, EXPR_OP_UNKNOWN },
		{ NODE_KIND_UNKNOWN, EXPR_OP_UNKNOWN }
	};
	
	answer_type answer4[] =
	{
		{ NODE_KIND_TRANSLATION_UNIT, EXPR_OP_UNKNOWN },
		{ DECL_KIND_FUNCTION, EXPR_OP_UNKNOWN },
		{ NODE_KIND_UNKNOWN, EXPR_OP_UNKNOWN }
	};

	answer_type answer5[] =
	{
		{ NODE_KIND_TRANSLATION_UNIT, EXPR_OP_UNKNOWN },
		{ DECL_KIND_FUNCTION, EXPR_OP_UNKNOWN },
		{ NODE_KIND_UNKNOWN, EXPR_OP_UNKNOWN }
	};

	answer_type answer6[] =
	{
		{ NODE_KIND_TRANSLATION_UNIT, EXPR_OP_UNKNOWN },
		{ DECL_KIND_FUNCTION, EXPR_OP_UNKNOWN },
		{ STMT_KIND_COMPOUND, EXPR_OP_UNKNOWN },
		{ NODE_KIND_UNKNOWN, EXPR_OP_UNKNOWN }
	};

	answer_type answer7[] =
	{
		{ NODE_KIND_TRANSLATION_UNIT, EXPR_OP_UNKNOWN },
		{ DECL_KIND_FUNCTION, EXPR_OP_UNKNOWN },
		{ STMT_KIND_COMPOUND, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_ASSIGN, EXPR_OP_MUL_ASSIGN },
		{ EXPR_KIND_BINARY, EXPR_OP_INDEX },
		{ EXPR_KIND_VARIABLE_REFERENCE, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_ASSIGN, EXPR_OP_ADD_ASSIGN },
		{ EXPR_KIND_BINARY, EXPR_OP_INDEX },
		{ EXPR_KIND_VARIABLE_REFERENCE, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_BINARY, EXPR_OP_INDEX },
		{ EXPR_KIND_VARIABLE_REFERENCE, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ STMT_KIND_RETURN, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_BINARY, EXPR_OP_MUL },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_BINARY, EXPR_OP_INDEX },
		{ EXPR_KIND_VARIABLE_REFERENCE, EXPR_OP_UNKNOWN },
		{ EXPR_KIND_CONSTANT, EXPR_OP_UNKNOWN },
		{ NODE_KIND_UNKNOWN, EXPR_OP_UNKNOWN }
	};

	answer_type *answer_data[] =
	{
		answer0,
		answer1,
		answer2,
		answer3,
		answer4,
		answer5,
		answer6,
		answer7
	};

	const char *answer_errors[] =
	{
		"",
		"",
		"0:1: L0001: Error while parsing integer literal '09'\n",
		"",
		"",
		"",
		"",
		""
	};

	const int numInput = sizeof(input) / sizeof(*input);
	int i;
	int tests_failed = 0;

	assert((sizeof(input) / sizeof(*input)) == (sizeof(answer_data) / sizeof(*answer_data)));
	assert((sizeof(input) / sizeof(*input)) == (sizeof(answer_errors) / sizeof(*answer_errors)));

	for (i = 0; i < numInput; ++i)
	{
		int lengths[1];
		node *expr = 0;
		frontend_context* ctx;
		char *error_log;

		lengths[0] = strlen(input[i]);
		LOGPRINT("== TEST CASE %d ==\n", i);
		LOGPRINT("%s\n", input[i]);

		ctx = _essl_new_frontend(pool, desc, input[i], lengths, 1);
		assert(ctx);

		if (i < 3)
		{
			expr = _essl_parse_expression(&ctx->parse_context);
		}
		else
		{
			expr = _essl_parse_translation_unit(&ctx->parse_context);
		}

		if (check_node(expr, answer_data[i], 0) < 0)
		{
			tests_failed++;
			printf("Simple test #%d failed, data missmatch\n", i);
			assert(0);
		}

		error_log = _essl_mempool_alloc(pool, _essl_error_get_log_size(ctx->err_context));
		assert(error_log);
		_essl_error_get_log(ctx->err_context, error_log,  _essl_error_get_log_size(ctx->err_context));

		if (strcmp(error_log, answer_errors[i]) != 0)
		{
			tests_failed++;
			printf("Simple test #%d failed, error message missmatch\n", i);
			assert(0);
		}
	}

	return tests_failed;
}



int main(void)
{
	mempool_tracker tracker;
	mempool pool;
	compiler_options* opts;
	target_descriptor *desc;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	opts = _essl_new_compiler_options(&pool);
	desc = _essl_new_target_descriptor(&pool, TARGET_FRAGMENT_SHADER, opts);
	scan_test(&pool, desc);
	if(simple_test(&pool, desc)) return 1;
	fprintf(stderr, "All tests OK!\n");
	_essl_mempool_destroy(&pool);
	return 0;
}

#endif /* UNIT_TEST */
