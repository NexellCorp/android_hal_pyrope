/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_COMMON_H
#define COMMON_COMMON_H

#include "common/essl_system.h"
#include "common/essl_str.h"


#ifndef ESSL_MIN
/** Macro for choosing the lower value of a and b,
 *  can evaluate expressions twice */
#define ESSL_MIN(a,b) ((a) < (b) ? (a) : (b))
#endif


#ifndef ESSL_MAX
/** Macro for choosing the higher value of a and b,
 *  can evaluate expressions twice */
#define ESSL_MAX(a,b) ((a) > (b) ? (a) : (b))
#endif



int _essl_string_to_integer(const string str, int base, /*@out@*/ int *retval);


/** Checks that a condition is true and returns otherwise, without considering it fatal */
#define ESSL_CHECK_NONFATAL(v) if((v) == 0) return 0

/** Checks that a condition is true, reports OOM and returns otherwise */
#define ESSL_CHECK_OOM(a) if (!(a)) { INTERNAL_ERROR_OUT_OF_MEMORY(); return 0; }


/*
   Handy macros for emitting error messages.
*/

/** Reports an error */
#define ERROR_CODE_MSG(code, a) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_ERROR(ERROR_CONTEXT, code, SOURCE_OFFSET, a)

/** Reports an error with one parameter */
#define ERROR_CODE_PARAM(code, a, b) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_ERROR(ERROR_CONTEXT, code, SOURCE_OFFSET, a, b)

/** Reports an error with two parameters */
#define ERROR_CODE_TWOPARAMS(a, b, c, d) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_ERROR(ERROR_CONTEXT, a, SOURCE_OFFSET, b, c, d)

/** Reports an error with two parameters */
#define ERROR_CODE_THREEPARAMS(a, b, c, d, e) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_ERROR(ERROR_CONTEXT, a, SOURCE_OFFSET, b, c, d, e)

/** Reports an error with a string parameter */
#define ERROR_CODE_STRING(code, a, b) do { const char *cbuf = _essl_string_to_cstring(ERROR_CONTEXT->pool, b); if (cbuf) { ERROR_CODE_PARAM(code, a, cbuf); } else { INTERNAL_ERROR_OUT_OF_MEMORY(); } } while(0)


/** Reports a warning */
#define WARNING_CODE_MSG(code, a) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_WARNING(ERROR_CONTEXT, code, SOURCE_OFFSET, a)

/** Reports a warning with one parameter */
#define WARNING_CODE_PARAM(code, a, b) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_WARNING(ERROR_CONTEXT, code, SOURCE_OFFSET, a, b)

/** Reports a warning with two parameters */
#define WARNING_CODE_TWOPARAMS(a, b, c, d) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_WARNING(ERROR_CONTEXT, a, SOURCE_OFFSET, b, c, d)

/** Reports a warning with a string parameter */
#define WARNING_CODE_STRING(code, a, b) do { const char *cbuf = _essl_string_to_cstring(ERROR_CONTEXT->pool, b); if (cbuf) { WARNING_CODE_PARAM(code, a, cbuf); } else { INTERNAL_ERROR_OUT_OF_MEMORY(); } } while(0)


/** Reports a note */
#define NOTE_CODE_MSG(code, a) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_NOTE(ERROR_CONTEXT, code, SOURCE_OFFSET, a)

/** Reports a note with one parameter */
#define NOTE_CODE_PARAM(code, a, b) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_NOTE(ERROR_CONTEXT, code, SOURCE_OFFSET, a, b)

/** Reports a note with two parameters */
#define NOTE_CODE_TWOPARAMS(a, b, c, d) RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_NOTE(ERROR_CONTEXT, a, SOURCE_OFFSET, b, c, d)

/** Reports a note with a string parameter */
#define NOTE_CODE_STRING(code, a, b) do { const char *cbuf = _essl_string_to_cstring(ERROR_CONTEXT->pool, b); if (cbuf) { NOTE_CODE_PARAM(code, a, cbuf); } else { INTERNAL_ERROR_OUT_OF_MEMORY(); } } while(0)


/** Reports a syntax error */
#define SYNTAX_ERROR_MSG(a) ERROR_CODE_MSG(SYNTAX_ERROR_CODE, a)

/** Reports a syntax error with one parameter */
#define SYNTAX_ERROR_PARAM(a, b) ERROR_CODE_PARAM(SYNTAX_ERROR_CODE, a, b)

/** Reports a syntax error with two parameters */
#define SYNTAX_ERROR_TWOPARAMS(a, b, c) ERROR_CODE_TWOPARAMS(SYNTAX_ERROR_CODE, a, b, c)

/** Reports a syntax error with a string parameter */
#define SYNTAX_ERROR_STRING(a, b) ERROR_CODE_STRING(SYNTAX_ERROR_CODE, a, b)


/** Reports a syntax warning with a string parameter */
#define SYNTAX_WARNING_MSG(a) WARNING_CODE_MSG(SYNTAX_ERROR_CODE, a)

/** Reports a syntax warning with one parameter */
#define SYNTAX_WARNING_PARAM(a, b) WARNING_CODE_PARAM(SYNTAX_ERROR_CODE, a, b)

/** Reports a syntax error with two parameters */
#define SYNTAX_WARNING_TWOPARAMS(a, b, c) WARNING_CODE_TWOPARAMS(SYNTAX_ERROR_CODE, a, b, c)

/** Reports a syntax warning with a string parameter */
#define SYNTAX_WARNING_STRING(a, b) WARNING_CODE_STRING(SYNTAX_ERROR_CODE, a, b)


/** Reports that an internal error occured */
#define INTERNAL_ERROR_MSG(a) ERROR_CODE_MSG(ERR_INTERNAL_COMPILER_ERROR, a)

/** Reports an internal error with one parameter */
#define INTERNAL_ERROR_PARAM(a, b) ERROR_CODE_PARAM(ERR_INTERNAL_COMPILER_ERROR, a, b)

/** Reports an internal error with two parameters */
#define INTERNAL_ERROR_TWOPARAMS(a, b, c) ERROR_CODE_TWOPARAMS(ERR_INTERNAL_COMPILER_ERROR, a, b, c)

/** Reports an internal error with a string parameter */
#define INTERNAL_ERROR_STRING(a, b) ERROR_CODE_STRING(ERR_INTERNAL_COMPILER_ERROR, a, b)


/** Reports an 'out of memory' error */
#define INTERNAL_ERROR_OUT_OF_MEMORY() RECORD_ERROR_LOCATION(ERROR_CONTEXT); REPORT_OOM(ERROR_CONTEXT)

#ifdef NDEBUG
#define DO_ASSERT(x) (void)(x)
#else
#define DO_ASSERT(x) assert(x)
#endif

#endif /* COMMON_COMMON_H */
