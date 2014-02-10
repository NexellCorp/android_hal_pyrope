/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_ERROR_REPORTING_H
#define COMMON_ERROR_REPORTING_H

#include "common/error_codes.h"
#include "common/essl_mem.h"
#include "common/essl_node.h"
#include "common/essl_list.h"
#include "common/essl_target.h"

/*#define SHOW_SOURCE_LOCATION*/

/**
   The error reporter is responsible for converting input string offsets into source string and line numbers.
   In order to do this, it needs to know about any line directives the preprocessor encounters.
   The following data structure is used for storing line directives.
*/

typedef struct _tag_er_position_list {
	struct _tag_er_position_list *next;
	int source_offset;
	int source_string;
	int line_number;
} er_position_list;



ASSUME_ALIASING(er_position_list, generic_list);


/**

Error context
*/
typedef struct _tag_error_context {
	mempool *pool; /**< memory pool for error buffer */
	char *buf; /**< buffer containing the error messages */
	size_t buf_idx; /**< index of one past the end of the last error message */
	size_t buf_size; /**< size of the error buffer */
	unsigned int n_internal_errors; /** number of internal compiler errors so far */
	unsigned int n_errors; /**< number of errors so far */
	unsigned int n_warnings; /**< number of warnings so far */
	int out_of_memory; /**< flag indicating whether OOM occured during error reporting */

	/*@null@*/ er_position_list *line_statements; /**< line directive information */

	int source_string_report_offset;
	const char *input_string;  /**< input string, used for scanning for newlines */
	unsigned int input_string_length; /**< length of concatenated input string */
	const int *source_string_lengths; /**< source string lengths */
	unsigned int n_source_strings; /**< number of source strings */
} error_context;


/** Report an error at the position given by the node */
memerr _essl_error_node(error_context *ctx, error_code code, node *n, const char *fmtstr, ...);
/** Report a warning at the position given by the node */
memerr _essl_warning_node(error_context *ctx, error_code code, node *n, const char *fmtstr, ...);
/** Report a note at the position given by the node */
memerr _essl_note_node(error_context *ctx, error_code code, node *n, const char *fmtstr, ...);

/** Report an error at the position given by source_offset */
memerr _essl_error(error_context *ctx, error_code code, int source_offset, const char *fmtstr, ...);
/** Report a warning at the position given by source_offset */
memerr _essl_warning(error_context *ctx, error_code code, int source_offset, const char *fmtstr, ...);
/** Report a note at the position given by source_offset */
memerr _essl_note(error_context *ctx, error_code code, int source_offset, const char *fmtstr, ...);

/** Report an out-of-memory condition */
void _essl_error_out_of_memory(error_context *ctx);

/** Retrieve the number of errors seen so far */
unsigned int _essl_error_get_n_errors(error_context *ctx);
/** Retrieve the number of warnings seen so far */
unsigned int _essl_error_get_n_warnings(error_context *ctx);



/**
  Fetch the size of the error log in bytes
*/
size_t _essl_error_get_log_size(error_context *ctx);

/**
   Fetch the error log
   @param ctx Error context
   @param log_buf Buffer to store the error log into
   @param log_buf_size Size of log_buf, in bytes
   @returns Negative on error, the number of bytes written into the log buffer on success
*/

size_t _essl_error_get_log(error_context *ctx, /*@out@*/char *log_buf, size_t log_buf_size);

/**
   Initialize an error context
*/
memerr _essl_error_init(/*@out@*/ error_context *ctx, mempool *pool, const char *input_string, const int *source_string_lengths, unsigned int n_source_strings);



/*
  Set source string position that errors in the first source string should be reported at. Handy if you inject extra source strings in order to handle -D on the command line.
 */
void _essl_error_set_source_string_report_offset(error_context *ctx, int source_string_report_offset);

/**
   Convert a source string offset into a source string number and a line number
*/
void _essl_error_get_position(error_context *ctx, int source_offset, /*@out@*/ int *source_string, /*@out@*/ int *line_number);

/**
  Record a new line directive.
  Used by the preprocessor.
*/
memerr _essl_error_set_position(error_context *ctx,  /*@null@*/ int source_offset,  /*@null@*/ int *source_string, int *line_number);


/* Get the name of the core for a target */
char *_essl_mali_core_name(mali_core core);


/**
   Macros for the error reporting functions. These are used so that we can redefine error handling to not include error messages if using a C99 preprocessor. This way, we could possibly save memory by omitting error strings.

*/

#define REPORT_ERROR (void)_essl_error
#define REPORT_WARNING (void)_essl_warning
#define REPORT_NOTE (void)_essl_note
#define REPORT_ERROR_NODE (void)_essl_error_node
#define REPORT_WARNING_NODE (void)_essl_warning_node
#define REPORT_NOTE_NODE (void)_essl_note_node
#define REPORT_OOM (void)_essl_error_out_of_memory

#ifdef SHOW_SOURCE_LOCATION
#define RECORD_ERROR_LOCATION(ctx) (void)_essl_note(ctx, 0, 0, "  In %s:%d (%s):  ", __FILE__, __LINE__, __func__)
#else
#define RECORD_ERROR_LOCATION(ctx) while(0)
#endif

#endif /* COMMON_ERROR_REPORTING_H */
