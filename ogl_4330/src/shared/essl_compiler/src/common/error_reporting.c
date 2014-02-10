/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#define NEEDS_STDIO  /* for snprintf */

#include "common/essl_system.h"
#include "common/essl_common.h"
#include "common/error_reporting.h"
#include "common/essl_node.h"
#include "compiler_version.h"

#define ERR_FMT "%d:%d: %s: "

static const char *code_to_str(error_code code)
{
	switch(code)
	{
#define PROCESS(v, n) case v: return n;
#include "common/error_names.h"
#undef PROCESS   
	case ERR_END:
		break;
	};
	return "<unknown>";
}

static int increase_buf(error_context *ctx)
{
	char *new_buf = _essl_mempool_alloc(ctx->pool, ctx->buf_size*2);
	if(!new_buf) return 0;
	memcpy(new_buf, ctx->buf, ctx->buf_size);
	ctx->buf = new_buf;
	ctx->buf_size *= 2;
	/* old buffer disappears into thin air */
	return 1;
}

unsigned int _essl_error_get_n_errors(error_context *ctx)
{
	return ctx->n_errors + ctx->n_internal_errors + ctx->out_of_memory;
}
unsigned int _essl_error_get_n_warnings(error_context *ctx)
{
	return ctx->n_warnings;
}
size_t _essl_error_get_log_size(error_context *ctx)
{
	if (ctx->out_of_memory == 0)
	{
		return (size_t)(ctx->buf_idx+1);
	}
	else
	{
		return (size_t)(ctx->buf_idx+1+strlen("0:1: : Out of memory.\n") + strlen(code_to_str(ERR_OUT_OF_MEMORY)));
	}
}
size_t _essl_error_get_log(error_context *ctx, char *log_buf, size_t log_buf_size)
{
	size_t len = (size_t)(ctx->buf_idx);
	if(len >= log_buf_size)
	{
		len = log_buf_size-1;
	}
	memcpy(log_buf, ctx->buf, len);
	log_buf[len] = '\0'; /* make sure their buffer is zero terminated */
	if (ctx->out_of_memory != 0)
	{
		(void)snprintf(&log_buf[len], log_buf_size-len, "0:1: %s: Out of memory.\n", code_to_str(ERR_OUT_OF_MEMORY));
	}
	return len;
}
memerr _essl_error_init(error_context *ctx, mempool *pool, const char *input_string, const int *source_string_lengths, unsigned int n_source_strings)
{
	unsigned i;
	ctx->pool = pool;
	ctx->n_errors = 0;
	ctx->n_warnings = 0;
	ctx->n_internal_errors = 0;
	ctx->buf_size = 128;
	ctx->buf_idx = 0;
	ctx->buf = _essl_mempool_alloc(pool, (size_t)ctx->buf_size);
	if(!ctx->buf) return 0;
	ctx->input_string = input_string;
	ctx->source_string_lengths = source_string_lengths;
	ctx->n_source_strings = n_source_strings;
	ctx->source_string_report_offset = 0;
	ctx->line_statements = 0;
	ctx->out_of_memory = 0;

	ctx->input_string_length = 0;
	for(i = 0; i < n_source_strings; ++i)
	{
		ctx->input_string_length += source_string_lengths[i];
	}

	return MEM_OK;
}

void _essl_error_set_source_string_report_offset(error_context *ctx, int source_string_report_offset)
{
	ctx->source_string_report_offset = source_string_report_offset;
}

void _essl_error_get_position(error_context *ctx, int source_offset, int *source_string, int *line_number)
{
	/* Do a linear scan through the input string in order to find source string number and offset.
	   It will be slow, but error reporting is an exceptional case anyway */
	int sstring = 0;
	int reported_sstring = 0;
	int line = 1;
	int position = 0;
	/* Error reporting is done after consuming tokens, so go back to the previous line if required */
	int at_end_of_line = 0;
	int at_end_of_source_string = 0;
	er_position_list *next_line_statement;
	assert(ctx != NULL);
	next_line_statement = ctx->line_statements;
	do
	{
		int source_string_pos = 0;
		line = 1;
		while(position < source_offset && source_string_pos < ctx->source_string_lengths[sstring])
		{
			while (next_line_statement && next_line_statement->source_offset <= position)
			{
				line = next_line_statement->line_number;
				reported_sstring = next_line_statement->source_string;
				next_line_statement = next_line_statement->next;
			}
			if(ctx->input_string[position] == '\n' || ctx->input_string[position] == '\r')
			{
				if(position+1 < (int)ctx->input_string_length && position+1 < source_offset && (ctx->input_string[position] != ctx->input_string[position+1] && (ctx->input_string[position+1] == '\r' || ctx->input_string[position+1] == '\n')))
				{
					/* \r\n or \n\r, don't emit newline twice */
					++position;
					++source_string_pos;
				}
				++line;
				at_end_of_line = 1;
			}
			else
			{
				at_end_of_line = 0;
				at_end_of_source_string = 0;
			}
			++position;
			++source_string_pos;
		}
		++sstring;
		++reported_sstring;
		at_end_of_source_string = 1;
	} while(position < source_offset && sstring < (int)ctx->n_source_strings);

	if (at_end_of_line)
	{
		--line;
	}

	if (at_end_of_source_string)
	{
		--reported_sstring;
	}

	reported_sstring += ctx->source_string_report_offset;
	if(reported_sstring < 0) reported_sstring = 0;

	if (source_string) *source_string = reported_sstring;
	if (line_number) *line_number = line;
}

memerr _essl_error_set_position(error_context *ctx, int source_offset, int *source_string, int *line_number)
{
	int cur_string;
	int cur_line;
	er_position_list *newlist;
	ESSL_CHECK_NONFATAL(newlist = LIST_NEW(ctx->pool, er_position_list));

	_essl_error_get_position(ctx, source_offset, &cur_string, &cur_line);

	if (source_string)
	{
		cur_string = *source_string;
	}

	if (line_number)
	{
		cur_line = *line_number;
	}

	newlist->source_string = cur_string - ctx->source_string_report_offset;
	newlist->line_number = cur_line;
	newlist->source_offset = source_offset;

	LIST_INSERT_BACK(&ctx->line_statements, newlist);
	return MEM_OK;
}


static memerr write_internal_compiler_error(error_context *ctx, int orig_bufptr)
{
	for(;;)
	{
		int ret = snprintf(&ctx->buf[ctx->buf_idx], ctx->buf_size - ctx->buf_idx, 
						   "            Please contact support-mali@arm.com with the shader causing\n"
						   "            the problem, along with this error message.\n"
#if COMPILER_CONFIGURATION == ONLINE_COMPILER
						   "            Mali online shader compiler " COMPILER_VERSION_STRING ".\n"
#else
						   "            Mali offline shader compiler " COMPILER_VERSION_STRING ".\n"
#endif
						   );

		if(ret >= 0 && (size_t)ret < ctx->buf_size - ctx->buf_idx) { ctx->buf_idx += ret; break; }
		if(increase_buf(ctx) == 0) { ctx->out_of_memory = 1; ctx->buf_idx = orig_bufptr; return 0; }
	}
	return MEM_OK;
}
/* 
   it is hard to pass va_lists around, therefore the common functionality is written as macros 
*/
#define WRITE_HEADER(str, ln, bufptr) \
assert(fmtstr != 0); \
bufptr = ctx->buf_idx; \
for(;;)		\
{ \
	int ret = snprintf(&ctx->buf[ctx->buf_idx], ctx->buf_size - ctx->buf_idx, ERR_FMT, str, ln, code_to_str(code)); \
	if(ret >= 0 && (size_t)ret < ctx->buf_size - ctx->buf_idx) { ctx->buf_idx += ret; break; } \
	if(increase_buf(ctx) == 0) { ctx->out_of_memory = 1; ctx->buf_idx = bufptr; return 0; } \
}

#define WRITE_BODY(bufptr) \
for(;;)			 \
{ \
	int ret; \
	va_list arglist; \
	va_start(arglist, fmtstr); \
	ret = vsnprintf(&ctx->buf[ctx->buf_idx], ctx->buf_size - ctx->buf_idx, fmtstr, arglist); \
	va_end(arglist); \
	if(ret >= 0 && (size_t)ret < ctx->buf_size - ctx->buf_idx) { ctx->buf_idx += ret; break; } \
	if(increase_buf(ctx) == 0) { ctx->out_of_memory = 1; ctx->buf_idx = bufptr; return 0; } \
} \
if(code == ERR_INTERNAL_COMPILER_ERROR) { ESSL_CHECK(write_internal_compiler_error(ctx, bufptr)); }


void _essl_error_out_of_memory(error_context *ctx)
{
	ctx->out_of_memory = 1;
}

memerr _essl_error(error_context *ctx, error_code code, int source_offset, const char *fmtstr, ...)
{
	int source_string, line;
	size_t bufoffset;
	_essl_error_get_position(ctx, source_offset, &source_string, &line);
	if (code == ERR_INTERNAL_COMPILER_ERROR) {
		++ctx->n_internal_errors;
	} else {
		++ctx->n_errors;
	}
	if (ctx->out_of_memory) return 0;
	WRITE_HEADER(source_string, line, bufoffset);
	
	WRITE_BODY(bufoffset);
	return MEM_OK;
}

memerr _essl_warning(error_context *ctx, error_code code, int source_offset, const char *fmtstr, ...)
{
	int source_string, line;
	size_t bufoffset;
	_essl_error_get_position(ctx, source_offset, &source_string, &line);
	++ctx->n_warnings;
	if (ctx->out_of_memory) return 0;
	WRITE_HEADER(source_string, line, bufoffset);
	
	WRITE_BODY(bufoffset);
	return MEM_OK;
}

memerr _essl_note(error_context *ctx, error_code code, int source_offset, const char *fmtstr, ...)
{
	int source_string, line;
	size_t bufoffset;
	_essl_error_get_position(ctx, source_offset, &source_string, &line);
	if (ctx->out_of_memory) return 0;
	WRITE_HEADER(source_string, line, bufoffset);
	
	WRITE_BODY(bufoffset);
	return MEM_OK;
}

memerr _essl_error_node(error_context *ctx, error_code code, node *n, const char *fmtstr, ...)
{
	int source_string, line;
	size_t bufoffset;
	_essl_error_get_position(ctx, n->hdr.source_offset, &source_string, &line);
	++ctx->n_errors;
	if (ctx->out_of_memory) return 0;
	WRITE_HEADER(source_string, line, bufoffset);
	
	WRITE_BODY(bufoffset);
	return MEM_OK;
}

memerr _essl_warning_node(error_context *ctx, error_code code, node *n, const char *fmtstr, ...)
{
	int source_string, line;
	size_t bufoffset;
	_essl_error_get_position(ctx, n->hdr.source_offset, &source_string, &line);
	++ctx->n_warnings;
	if (ctx->out_of_memory) return 0;
	WRITE_HEADER(source_string, line, bufoffset);
	
	WRITE_BODY(bufoffset);
	return MEM_OK;
}

memerr _essl_note_node(error_context *ctx, error_code code, node *n, const char *fmtstr, ...)
{
	int source_string, line;
	size_t bufoffset;
	_essl_error_get_position(ctx, n->hdr.source_offset, &source_string, &line);
	if (ctx->out_of_memory) return 0;
	WRITE_HEADER(source_string, line, bufoffset);
	
	WRITE_BODY(bufoffset);
	return MEM_OK;
}


char *_essl_mali_core_name(mali_core core)
{
#ifdef NEXELL_FEATURE_ENABLE
	switch (core)
	{
	case CORE_MALI_GP:
	case CORE_MALI_55:
	case CORE_MALI_110:
		assert(0 && "Unsupported core");
		return "VR";
	case CORE_MALI_GP2:
		return "VRGP2";
	case CORE_MALI_200:
		return "VR200";
	case CORE_MALI_400_GP:
		return "VR GP";
	case CORE_MALI_400_PP:
		return "VR PP";
	}
	assert(0 && "Invalid core ID");
	return "VR";
#else
	switch (core)
	{
	case CORE_MALI_GP:
	case CORE_MALI_55:
	case CORE_MALI_110:
		assert(0 && "Unsupported core");
		return "Mali";
	case CORE_MALI_GP2:
		return "MaliGP2";
	case CORE_MALI_200:
		return "Mali200";
	case CORE_MALI_400_GP:
		return "Mali-400 GP";
	case CORE_MALI_400_PP:
		return "Mali-400 PP";
	}

	assert(0 && "Invalid core ID");
	return "Mali";
#endif	
}


#ifdef UNIT_TEST
int main(void)
{
	error_context ctx;
	node n;
	char *actual_str;
	const char *input_string = "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "012345678\n" "0123456789"
		"\n\n\r\r\r\r\n\r\n\r\n\n\r\n\r\nzomg";

	int source_string_lengths[] = {50, 60, 80, 35, 125};
	int n_source_strings = 5;

	const char *expected_str = 
		"4:5: S0001: this is a 5 test\n" 
		"3:2: L0002: this is another \"one\"\n" 
		"1:3: P0001: preprocessor error\n" 
		"1:3: Warning: this is a warning node text\n"
		"1:3: : this is a note node text\n"
		"1:2: S0007: "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"long error\n"
		"4:11: S0001: end of ordinary newlines error\n"
		"4:21: S0001: test of universal newlines\n";

	mempool_tracker tracker;
	mempool pool;

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	_essl_error_init(&ctx, &pool, input_string, source_string_lengths, n_source_strings);

	REPORT_ERROR(&ctx, ERR_SEM_TYPE_MISMATCH, 265, "this is a %d test\n", 5); 
	REPORT_ERROR(&ctx, ERR_LP_UNDEFINED_IDENTIFIER, 205, "this is another \"%s\"\n", "one"); 
	n.hdr.source_offset = 76;
	REPORT_ERROR_NODE(&ctx, ERR_PP_SYNTAX_ERROR, &n, "preprocessor error\n"); 
	REPORT_WARNING_NODE(&ctx, ERR_WARNING, &n , "this is a warning node text\n");
	REPORT_NOTE_NODE(&ctx, ERR_NOTE, &n , "this is a note node text\n");
	REPORT_ERROR(&ctx, ERR_SEM_CONSTRUCTOR_WRONG_ARGS, 67,
		    "really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"really really really really really really really really really really really really really really really "
			"long error\n");

	REPORT_ERROR(&ctx, ERR_SEM_TYPE_MISMATCH, 329, "end of ordinary newlines error\n"); 
	REPORT_ERROR(&ctx, ERR_SEM_TYPE_MISMATCH, 349, "test of universal newlines\n"); 

	actual_str = _essl_mempool_alloc(&pool, _essl_error_get_log_size(&ctx));
	assert(actual_str);
	_essl_error_get_log(&ctx, actual_str, _essl_error_get_log_size(&ctx));

	if(strcmp(expected_str, actual_str))
	{
		fprintf(stderr, "Expected output: %lu\n%s", (long)strlen(expected_str), expected_str);
		fprintf(stderr, "Actual output: %lu\n%s", (long)strlen(actual_str), actual_str);
	}

	assert(!strcmp(expected_str, actual_str));

	assert(_essl_error_get_n_errors(&ctx) == 6);
	assert(_essl_error_get_n_warnings(&ctx) == 1);


	fprintf(stderr, "All tests OK!\n");
	_essl_mempool_destroy(&pool);

	return 0;
}

#endif /* UNIT_TEST */
