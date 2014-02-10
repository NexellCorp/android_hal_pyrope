/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_log.h"
#include "base/mali_debug.h"
#include "base/mali_runtime.h"

#include "base/mali_runtime_stdarg.h" /* for vsnprintf */

#include "mali_instrumented_context.h"

MALI_STATIC char* log_level_enum_to_string(mali_log_level level)
{
	switch (level)
	{
		case MALI_LOG_LEVEL_ERROR:
			return "Error     ";
		case MALI_LOG_LEVEL_WARNING:
			return "Warning   ";
		case MALI_LOG_LEVEL_EVERYTHING:
			return "Everything";
		case MALI_LOG_LEVEL_DEBUG:
			return "Debug     ";
		case MALI_LOG_LEVEL_NONE:
			return "None      ";
	}

	MALI_DEBUG_ASSERT(0, ("Unknown level to log_level_enum_to_string"));

	return "Unknown level";
}

MALI_EXPORT
void _instrumented_log(mali_instrumented_context* ctx, mali_log_level level, char *format, ...)
{
	u32 i, len;
	mali_instrumented_tls_struct* tlsblock;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	if (ctx->log_level == MALI_LOG_LEVEL_NONE || level > ctx->log_level) return;

	tlsblock = _instrumented_get_tls();
	if (tlsblock == NULL) return;

#ifdef __SYMBIAN32__
	_mali_sys_snprintf(tlsblock->log_message, INSTRUMENTED_LOG_BUFFER_SIZE, "%s\t0x%08x\t%d\t%Lu\t",
#else
	_mali_sys_snprintf(tlsblock->log_message, INSTRUMENTED_LOG_BUFFER_SIZE, "%s\t0x%08x\t%d\t%llu\t",
#endif
	                   log_level_enum_to_string(level),
	                   ctx,
	                   ctx->current_frame_no,
	                   _mali_sys_get_time_usec());

	len = _mali_sys_strlen(tlsblock->log_message);

	{
		/* assemble the log message */
		va_list arguments;
		va_start (arguments, format);
		_mali_sys_vsnprintf(tlsblock->log_message + len, INSTRUMENTED_LOG_BUFFER_SIZE - len, format, arguments);
		va_end(arguments);
	}

	/* pass the message to the callbacks */
	for (i = 0; i < ctx->numcallbacks; ++i)
	{
		ctx->callbacks[i](ctx, tlsblock->log_message);
	}

}

MALI_EXPORT
mali_err_code _instrumented_register_log_callback(mali_instrumented_context *ctx, mali_log_callback callback)
{
	void *newcallbacks;
	MALI_DEBUG_ASSERT_POINTER(ctx);

	newcallbacks = _mali_sys_realloc(ctx->callbacks, sizeof(mali_log_callback) * (ctx->numcallbacks + 1));
	if (NULL == newcallbacks) return MALI_ERR_OUT_OF_MEMORY;

	ctx->callbacks = newcallbacks;
	ctx->callbacks[ctx->numcallbacks] = callback;

	ctx->numcallbacks++;

	MALI_SUCCESS;
}

MALI_EXPORT
void _instrumented_set_log_level(mali_instrumented_context* ctx, mali_log_level level)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	ctx->log_level = level;
}

MALI_EXPORT
mali_err_code _instrumented_open_log_file(mali_instrumented_context *ctx, const char *filename)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	ctx->log_file = _mali_sys_fopen(filename, "a");
	if (NULL == ctx->log_file) return MALI_ERR_FUNCTION_FAILED;

	MALI_SUCCESS;
}

MALI_EXPORT
void _instrumented_write_to_log_file(mali_instrumented_context *ctx, char *message)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->log_file);

	_mali_sys_fprintf(ctx->log_file, "%s\n", message);
}

MALI_EXPORT
void _instrumented_close_log_file(mali_instrumented_context *ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	if (NULL != ctx->log_file) _mali_sys_fclose(ctx->log_file);
	ctx->log_file = NULL;
}
