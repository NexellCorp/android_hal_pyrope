/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/essl_mem.h"
#include "frontend/token.h"
#include "frontend/scanner.h"
#include "frontend/lang.h"
#include "common/essl_common.h"

/** Code to access the error context for this module */
#define ERROR_CONTEXT ctx->err_context

/** Code to retrieve the source offset for this module */
#define SOURCE_OFFSET _essl_scanner_get_source_offset(ctx)

/** Error code to use for syntax errors in this module */
#define SYNTAX_ERROR_CODE ERR_LP_SYNTAX_ERROR

static const string whitespace = { " ", 1 };

memerr _essl_scanner_init(scanner_context *ctx, mempool *pool, error_context *e_ctx, const char *input_string, const int *source_string_lengths, unsigned int n_source_strings)
{
	unsigned int i;
	int acc = 0;
	ctx->input_string = input_string;
	ctx->pool = pool;
	ctx->err_context = e_ctx;
	for(i = 0; i < n_source_strings; ++i)
	{
		acc += source_string_lengths[i];
	}

	ctx->input_string_length = acc;

	ctx->position = 0;

	{
		/* initialize dictionary */
#define INIT(str, token) ESSL_CHECK_NONFATAL(_essl_dict_insert(&ctx->keywords, _essl_cstring_to_string_nocopy(str), (void*)token))
		ESSL_CHECK_NONFATAL(_essl_dict_init(&ctx->keywords, pool));

		INIT("attribute", TOK_ATTRIBUTE);
		INIT("bool", TOK_BOOL);
		INIT("break", TOK_BREAK);
		INIT("bvec2", TOK_BVEC2);
		INIT("bvec3", TOK_BVEC3);
		INIT("bvec4", TOK_BVEC4);
		INIT("const", TOK_CONST);
		INIT("continue", TOK_CONTINUE);
		INIT("discard", TOK_DISCARD);
		INIT("do", TOK_DO);
		INIT("else", TOK_ELSE);
		INIT("false", TOK_FALSE);
		INIT("float", TOK_FLOAT);
		INIT("for", TOK_FOR);
		INIT("highp", TOK_HIGHP);
		INIT("if", TOK_IF);
		INIT("in", TOK_IN);
		INIT("inout", TOK_INOUT);
		INIT("int", TOK_INT);
		INIT("invariant", TOK_INVARIANT);
		INIT("ivec2", TOK_IVEC2);
		INIT("ivec3", TOK_IVEC3);
		INIT("ivec4", TOK_IVEC4);
		INIT("lowp", TOK_LOWP);
		INIT("mat2", TOK_MAT2);
		INIT("mat3", TOK_MAT3);
		INIT("mat4", TOK_MAT4);
		INIT("mediump", TOK_MEDIUMP);
		INIT("out", TOK_OUT);
		INIT("precision", TOK_PRECISION);
		INIT("return", TOK_RETURN);
		INIT("sampler2D", TOK_SAMPLER2D);
		INIT("sampler3D", TOK_SAMPLER3D);
		INIT("samplerCube", TOK_SAMPLERCUBE);
		INIT("struct", TOK_STRUCT);
		INIT("true", TOK_TRUE);
		INIT("uniform", TOK_UNIFORM);
		INIT("varying", TOK_VARYING);
		INIT("vec2", TOK_VEC2);
		INIT("vec3", TOK_VEC3);
		INIT("vec4", TOK_VEC4);
		INIT("void", TOK_VOID);
		INIT("while", TOK_WHILE);

		INIT("asm", TOK_ASM);
		INIT("class", TOK_CLASS);
		INIT("union", TOK_UNION);
		INIT("enum", TOK_ENUM);
		INIT("typedef", TOK_TYPEDEF);
		INIT("template", TOK_TEMPLATE);
		INIT("this", TOK_THIS);
		INIT("packed", TOK_PACKED);
		INIT("goto", TOK_GOTO);
		INIT("switch", TOK_SWITCH);
		INIT("default", TOK_DEFAULT);
		INIT("inline", TOK_INLINE);
		INIT("noinline", TOK_NOINLINE);
		INIT("volatile", TOK_VOLATILE);
		INIT("public", TOK_PUBLIC);
		INIT("static", TOK_STATIC);
		INIT("extern", TOK_EXTERN);
		INIT("external", TOK_EXTERNAL);
		INIT("interface", TOK_INTERFACE);
		INIT("flat", TOK_FLAT);
		/*INIT("centroid", TOK_CENTROID);*/
		INIT("long", TOK_LONG);
		INIT("short", TOK_SHORT);
		INIT("double", TOK_DOUBLE);
		INIT("half", TOK_HALF);
		INIT("fixed", TOK_FIXED);
		INIT("unsigned", TOK_UNSIGNED);
		INIT("superp", TOK_SUPERP);
		INIT("input", TOK_INPUT);
		INIT("output", TOK_OUTPUT);
		INIT("hvec2", TOK_HVEC2);
		INIT("hvec3", TOK_HVEC3);
		INIT("hvec4", TOK_HVEC4);
		INIT("dvec2", TOK_DVEC2);
		INIT("dvec3", TOK_DVEC3);
		INIT("dvec4", TOK_DVEC4);
		INIT("fvec2", TOK_FVEC2);
		INIT("fvec3", TOK_FVEC3);
		INIT("fvec4", TOK_FVEC4);
		INIT("sampler1D", TOK_SAMPLER1D);
		INIT("sampler1DShadow", TOK_SAMPLER1DSHADOW);
		INIT("sampler2DShadow", TOK_SAMPLER2DSHADOW);
		INIT("sampler2DRect", TOK_SAMPLER2DRECT);
		INIT("sampler3DRect", TOK_SAMPLER3DRECT);
		INIT("sampler2DRectShadow", TOK_SAMPLER2DRECTSHADOW);
		INIT("sizeof", TOK_SIZEOF);
		INIT("cast", TOK_CAST);
		INIT("namespace", TOK_NAMESPACE);
		INIT("using", TOK_USING);
#undef INIT
	}

	return MEM_OK;
}


memerr _essl_scanner_load_extension_keywords(scanner_context *ctx, language_descriptor *lang)
{
#define INIT(str, token) ESSL_CHECK_NONFATAL(_essl_dict_insert(&ctx->keywords, _essl_cstring_to_string_nocopy(str), (void*)token))
	if (_essl_get_extension_behavior(lang, EXTENSION_GL_ARM_GROUPED_UNIFORMS) != BEHAVIOR_DISABLE)
	{
		INIT("__groupARM", TOK_GROUP);
	}
	if (_essl_get_extension_behavior(lang, EXTENSION_GL_ARM_PERSISTENT_GLOBALS) != BEHAVIOR_DISABLE)
	{
		INIT("__persistentARM", TOK_PERSISTENT);
	}
	if (_essl_get_extension_behavior(lang, EXTENSION_GL_OES_TEXTURE_EXTERNAL) != BEHAVIOR_DISABLE)
	{
		INIT("samplerExternalOES", TOK_SAMPLEREXTERNAL);
	}
#undef INIT
	return MEM_OK;
}
/**
   Get the next character of the input string 
*/
static int scanner_getchar(scanner_context *ctx)
{
	int c;
	if(ctx->position >= ctx->input_string_length)
	{
		++ctx->position;
		return TOK_END_OF_FILE; /* end of file */
	}


	c = (int)ctx->input_string[ctx->position++];

	return c;
}


/**
   Put back a character 
*/
static void scanner_ungetchar(scanner_context *ctx)
{
	--ctx->position;

}

int _essl_scanner_get_source_offset(scanner_context *ctx)
{
	return ctx->position;
}

Token _essl_scanner_get_token(scanner_context *ctx, string *token_str)
{

	int c;
	int c2;
	int len;

	if(token_str)
	{ 
		token_str->ptr =  &ctx->input_string[ctx->position];
		token_str->len = 1;
	}
	c = scanner_getchar(ctx);

	/* try the easy cases first */
	switch(c) 
	{

	case ';':
	case '{':
	case '}':
	case ':':
	case '(':
	case ')':
	case '[':
	case ']':
	case '~':
	case '?':
	case '#':
	case ',':
	case '\\':
		/* single token */
		return (Token)c;

	case '.':
		c2 = scanner_getchar(ctx);
		if(c2 >= '0' && c2 <= '9')
		{
			/* this is actually the start of a floating point constant,
			 let the state machine deal with it 
			*/
			scanner_ungetchar(ctx);
			break;
		}
		scanner_ungetchar(ctx);
		return (Token)c;


	case ' ':
	case '\t':
	case '\v':
	case '\f':
		/* whitespace */
		len = 0;
		do {
			c2 = scanner_getchar(ctx);
			++len;
		} while(c2 == ' ' || c2 == '\t' || c2 == '\r' || c2 == '\v' || c2 == '\f');
		scanner_ungetchar(ctx);
		if(token_str) token_str->len = len;
		return TOK_WHITESPACE;

	case '\n':
	case '\r':
		len = 1;
		c2 = scanner_getchar(ctx);
		++len;
		if(c != c2 && (c2 == '\n' || c2 == '\r'))
		{
			/* \r\n or \r\n, don't count the newline twice */
		} else {
			--len;
			scanner_ungetchar(ctx);
		}
		if(token_str) token_str->len = len;
		return TOK_NEWLINE;


	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case '^':
	case '|':
	case '<':
	case '>':
	case '=':
	case '!':
		c2 = scanner_getchar(ctx);
		if(c2 == '=')
		{ /* augmented assignment */
			if(token_str) token_str->len = 2;
			/*
			  the tokens are layed out such that the character value
			  plus the augmented assignment offset yields the
			  corresponding augmented assignment token
			 */
			return (Token)(c + AUGASSIGN_OFF);
		}
		if(c == c2)
		{
			int c3;
			if(token_str) token_str->len = 2;
			switch(c2)
			{
			case '<':
			case '>':
				c3 = scanner_getchar(ctx);
				if(c3 == '=')
				{
					if(token_str) token_str->len = 3;
					return c == '<' ? TOK_LEFT_ASSIGN : TOK_RIGHT_ASSIGN;
				}
				scanner_ungetchar(ctx);
				return c == '<' ? TOK_LEFT_OP : TOK_RIGHT_OP;		
			case '+':
				return TOK_INC_OP;
			case '-':
				return TOK_DEC_OP;
			case '&':
				return TOK_AND_OP;
			case '|':
				return TOK_OR_OP;
			case '^':
				return TOK_XOR_OP;
			}
		}
		if(c == '/')
		{
			if(c2 == '/')
			{
				/* c++-style comment */
				int c3 = 0;
				do
				{
					c3 = scanner_getchar(ctx);
					
				} while(c3 != '\n' && c3 != 0);
				
				if(c3 == 0)
				{
					if(token_str) token_str->len = 0;
					return TOK_END_OF_FILE;
				}
				scanner_ungetchar(ctx);
				return _essl_scanner_get_token(ctx, token_str);
			}
			if(c2 == '*')
			{
				/* c-style comment */
				
				int c3;
				int seen_star = 0;
				for(;;)
				{
					c3 = scanner_getchar(ctx);

					if(c3 == 0)
					{
						SYNTAX_ERROR_MSG("Unterminated comment\n");
						if(token_str)
						{ 
							token_str->ptr = &ctx->input_string[ctx->position-1];
							token_str->len = 0;
						}
						return TOK_END_OF_FILE;
					}
					if(seen_star && c3 == '/')
					{
						/* comment done, return a single whitespace token */
						assert(whitespace.ptr != 0);
						if(token_str) *token_str = whitespace;
						return TOK_WHITESPACE;
					} else {
						seen_star = 0;
					}
	
					if(!seen_star && c3 == '*')
					{
						seen_star = 1;
					}
				
				}
				
			}

		}

		scanner_ungetchar(ctx);
		return (Token)c;

	case 0:
		if(token_str) token_str->len = 0;
		return TOK_END_OF_FILE;
	}
	/* now, try the more complex cases where we need to read an unbounded number of characters. we will use a state machine for this task */

	if( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||  (c >= '0' && c <= '9') || c == '_' || c =='.')
	{

		/* 
		   enum for the identifier/keyword/literal matching state machine
		*/
		enum { BEGIN, IDENTKEYWORD, HEXOCTFLOAT, HEX, FLOATMANT, FLOATMINUSEXP, FLOATEXP, INTFLOAT, DONE } state = BEGIN;

		Token token = TOK_END_OF_FILE;
		int idx;
		int startidx;
		string identifier;
		scanner_ungetchar(ctx);
		startidx = idx = ctx->position;
		while(state != DONE)
		{
			c = scanner_getchar(ctx);
			switch(state)
			{
			case BEGIN:
				if( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') state = IDENTKEYWORD;
				else if(c == '0') state = HEXOCTFLOAT;
				else if(c >= '1' && c <= '9') state = INTFLOAT;
				else if(c == '.') state = FLOATMANT;
				break;

			case IDENTKEYWORD:
				token = TOK_IDENTIFIER;
				if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
				{
					state = IDENTKEYWORD;
				}
				else state = DONE;
				break;
				
			case HEXOCTFLOAT:
				token = TOK_INTCONSTANT;
				if(c == 'x' || c == 'X') state = HEX;
				else if(c == '.') state = FLOATMANT;
				else if(c == 'E' || c == 'e') state = FLOATMINUSEXP;
				else if(c >= '0' && c <= '9') state = INTFLOAT;
				else state = DONE;
				break;

			case INTFLOAT:
				token = TOK_INTCONSTANT;
				if(c >= '0' && c <= '9') state = INTFLOAT;
				else if(c == '.') state = FLOATMANT;
				else if(c == 'E' || c == 'e') state = FLOATMINUSEXP;
				else state = DONE;
				break;
			case FLOATMANT:
				token = TOK_FLOATCONSTANT;
				if(c >= '0' && c <= '9') state = FLOATMANT;
				else if(c == 'E' || c == 'e') state = FLOATMINUSEXP;
				else state = DONE;
				break;



			case FLOATMINUSEXP:
				token = TOK_FLOATCONSTANT;
				if(c >= '0' && c <= '9') state = FLOATEXP;
				else if(c == '-' || c == '+') state = FLOATEXP;
				else state = DONE;
				break;
				
			case FLOATEXP:
				token = TOK_FLOATCONSTANT;
				if(c >= '0' && c <= '9') state = FLOATEXP;
				else state = DONE;
				break;
				
			case HEX:
				if((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9'))
				{
					state = HEX;
				}
				else state = DONE;
				break;
	
			case DONE:
				assert(0);
				break;

			}
			if(state != DONE)
			{
				++idx;
			}
		}
		/* end of token */
		scanner_ungetchar(ctx);

		identifier.ptr = &ctx->input_string[startidx];
		identifier.len = idx - startidx;
		if(token_str)
		{
			*token_str = identifier;
		}
		if(token == TOK_IDENTIFIER)
		{
			/* 
			   check if the identifier really is a keyword by looking it up in the keyword dictionary
			*/
			
			if (_essl_dict_has_key(&ctx->keywords, identifier)) {
				return (Token)_essl_dict_lookup(&ctx->keywords, identifier);
			}

		}

		return token;


	}


	SYNTAX_ERROR_TWOPARAMS("Unknown character '%c'(%d)\n", (unsigned char)c, (unsigned char)c);

	return _essl_scanner_get_token(ctx, token_str);


}



const char* _essl_token_to_str(Token t)
{
	switch (t)
	{
#define PROCESS(v, n) case v: return n;
#include "frontend/token_names.h"
#undef PROCESS
	}
	assert(0);
	return "<error>";
}





#ifdef UNIT_TEST

void check_token_name(Token tok, const char *expected_str)
{
	const char *actual_str = _essl_token_to_str(tok);
	if (actual_str != NULL)
	{
		if (strcmp(actual_str, expected_str) != 0)
		{
			fprintf(stderr, "Token string does not match, got '%s', expected '%s'\n", actual_str, expected_str);
			assert(strcmp(actual_str, expected_str) == 0);
		}
	}
	else
	{
		fprintf(stderr, "Could not get token string for token %u, expected '%s'\n", (unsigned int)tok, expected_str);
		assert(actual_str != NULL);
	}
}

int main(void)
{
	typedef struct {
		Token t;
		const char *str;
	} FullToken;
	typedef struct {
		const char *input_string;

		FullToken tokens[1000];

	} test_case;


	test_case test_cases[] = 
		{
			{
				 "abc  = b + d * e;",
				{ { TOK_IDENTIFIER, "abc"},
				  { TOK_WHITESPACE, "  "},
				  { TOK_ASSIGN, "="},
				  { TOK_WHITESPACE, " "},
				  { TOK_IDENTIFIER, "b"},
				  { TOK_WHITESPACE, " "},
				  { TOK_PLUS, "+"},
				  { TOK_WHITESPACE, " "},
				  { TOK_IDENTIFIER, "d"},
				  { TOK_WHITESPACE, " "},
				  { TOK_STAR, "*"},
				  { TOK_WHITESPACE, " "},
				  { TOK_IDENTIFIER, "e"},
				  { TOK_SEMICOLON, ";"},
				  { TOK_END_OF_FILE, ""}
				}
			},
			{
				"0 1 00 0.0 .0 1e4 1.0e5 0X45ABad 0x1337",
				{ { TOK_INTCONSTANT, "0"},
				  { TOK_WHITESPACE, " "},
				  { TOK_INTCONSTANT, "1"},
				  { TOK_WHITESPACE, " "},
				  { TOK_INTCONSTANT, "00"},
				  { TOK_WHITESPACE, " "},
				  { TOK_FLOATCONSTANT, "0.0"},
				  { TOK_WHITESPACE, " "},
				  { TOK_FLOATCONSTANT, ".0"},
				  { TOK_WHITESPACE, " "},
				  { TOK_FLOATCONSTANT, "1e4"},
				  { TOK_WHITESPACE, " "},
				  { TOK_FLOATCONSTANT, "1.0e5"},
				  { TOK_WHITESPACE, " "},
				  { TOK_INTCONSTANT, "0X45ABad"},
				  { TOK_WHITESPACE, " "},
				  { TOK_INTCONSTANT, "0x1337"},
				  { TOK_END_OF_FILE, ""}				  
				}
			},
			{
				 "return vec4(a, b, c, d);",
				{ { TOK_RETURN, "return"},
				  { TOK_WHITESPACE, " "},
				  { TOK_VEC4, "vec4"},
				  { TOK_LEFT_PAREN, "("},
				  { TOK_IDENTIFIER, "a"},
				  { TOK_COMMA, ","},
				  { TOK_WHITESPACE, " "},
				  { TOK_IDENTIFIER, "b"},
				  { TOK_COMMA, ","},
				  { TOK_WHITESPACE, " "},
				  { TOK_IDENTIFIER, "c"},
				  { TOK_COMMA, ","},
				  { TOK_WHITESPACE, " "},
				  { TOK_IDENTIFIER, "d"},
				  { TOK_RIGHT_PAREN, ")"},
				  { TOK_SEMICOLON, ";"},
				  { TOK_END_OF_FILE, ""},
				}
			},
			{
				"true false",
				{ { TOK_TRUE, "true"},
				  { TOK_WHITESPACE, " "},
				  { TOK_FALSE, "false"},
				  { TOK_END_OF_FILE, ""},
				}
			},
			{
				"precision invariant /* comment */ highp mediump lowp",
				{ 
					{ TOK_PRECISION, "precision"},
					{ TOK_WHITESPACE, " "},
					{ TOK_INVARIANT, "invariant"},
					{ TOK_WHITESPACE, " "},
					{ TOK_WHITESPACE, " "},
					{ TOK_WHITESPACE, " "},
					{ TOK_HIGHP, "highp"},
					{ TOK_WHITESPACE, " "},
					{ TOK_MEDIUMP, "mediump"},
					{ TOK_WHITESPACE, " "},
					{ TOK_LOWP, "lowp"},
					{ TOK_END_OF_FILE, ""},
				}
			},
			{
				"a // comment - more comment\n b",
				{ 
					{ TOK_IDENTIFIER, "a"},
					{ TOK_WHITESPACE, " "},
					{ TOK_NEWLINE, "\n"},
					{ TOK_WHITESPACE, " "},
					{ TOK_IDENTIFIER, "b"},
					{ TOK_END_OF_FILE, ""},
				}
			},
			{
				"\n\n\r\r\r\r\n\r\n\r\n\n\r\n\r\n",
				{ 
					{ TOK_NEWLINE, "\n"},
					{ TOK_NEWLINE, "\n\r"},
					{ TOK_NEWLINE, "\r"},
					{ TOK_NEWLINE, "\r"},
					{ TOK_NEWLINE, "\r\n"},
					{ TOK_NEWLINE, "\r\n"},
					{ TOK_NEWLINE, "\r\n"},
					{ TOK_NEWLINE, "\n\r"},
					{ TOK_NEWLINE, "\n\r"},
					{ TOK_NEWLINE, "\n"},
					{ TOK_END_OF_FILE, ""},
				}
			},

		};

	int n_test_cases = sizeof(test_cases)/sizeof(test_cases[0]);
	int i;
	scanner_context context;
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
		lens[0] = (int)strlen(tc->input_string);

		_essl_scanner_init(&context, &pool, &err, tc->input_string, &lens[0], 1);
		_essl_error_init(&err, &pool, tc->input_string, &lens[0], 1);

		do {
			t = _essl_scanner_get_token(&context, &s);
			assert(s.ptr);
			assert(expected_token->str);
			if(t != expected_token->t || strcmp(_essl_string_to_cstring(&pool, s), expected_token->str))
			{
				fprintf(stderr, "Test case %d: expected '%s', got '%s', s.ptr '%s', expected_token->str '%s'\n", i, _essl_token_to_str(expected_token->t), _essl_token_to_str(t), _essl_string_to_cstring(&pool, s), expected_token->str);

				assert(t == expected_token->t);
				assert(!strcmp(_essl_string_to_cstring(&pool, s), expected_token->str));
			}
			
			++expected_token;

		} while(t != TOK_END_OF_FILE);
	}

	/*
	 * Simply use all tokens in token_names.h to get coverage.
	 * Yes, this is a really bad test, cos it will never fail,
	 * but the missing coverage is also missleading.
	 */

#define PROCESS(v, n) check_token_name(v, n);
#include "frontend/token_names.h"
#undef PROCESS

	_essl_mempool_destroy(&pool);
	fprintf(stderr, "All tests OK!\n");
	return 0;
}


#endif /* UNIT_TEST */
