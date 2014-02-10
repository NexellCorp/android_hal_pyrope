/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _MALI_TRACE_H_
#define _MALI_TRACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#if MALI_API_TRACE

#include "mali_system.h"

/**
 * Trace options from user, describes what to include in trace output.
 */
enum mali_api_trace_flags
{
	MALI_API_TRACE_FUNCTION_CALLS     = (1<<0), /* only log the function calls */
	MALI_API_TRACE_PARAMETERS         = (1<<1), /* log input parameters and return values */
	MALI_API_TRACE_OUTPUT             = (1<<2), /* Also log output parameters (can be used to verify that we get the exact same result) */
	MALI_API_TRACE_USER_BUFFER_CRC    = (1<<3), /* Use a CRC checksum to skip dumping unchanged user buffers */
	MALI_API_TRACE_TESTING            = (1<<4), /* Copy input buffer to vg/glReadPixels() (only useful for tracing internal API tests) */
};

/**
 * Type of array parameter, input, output or extra data
 */
enum mali_api_trace_direction
{
	MALI_API_TRACE_IN                 = (1<<0), /* Input parameter, copy content in advance */
	MALI_API_TRACE_OUT                = (1<<1), /* Output parameter, copy result if MALI_API_TRACE_OUTPUT is enabled */
	MALI_API_TRACE_ID0                = (1<<2), /* Dummy parameter, not to be used directly as a parameter, but rather to provide extra meta info */
	MALI_API_TRACE_TEST_IN            = (1<<3), /* Like MALI_API_TRACE_IN, except only enabled if MALI_API_TRACE_TESTING flag is set */
};

/**
 * Mark the end of the trace (EGL destroy)
 * Calling API functions after this will result in trace output to a new file (numbered *.0001 and so on)
 */
MALI_IMPORT
void mali_api_trace_mark_for_deinit(void);

/**
 * Check if a particular trace flag is set
 * @see mali_api_trace_flags
 * @param flag Flag to check for
 * @return MALI_TRUE if flag is set, otherwise false
 */
MALI_IMPORT
mali_bool mali_api_trace_flag_enabled(enum mali_api_trace_flags flag);

/**
 * Mark the beginning of a function call
 * Parameters (if any) must be added between mali_api_trace_func_begin() and mali_api_trace_func_end()
 * @param func_name Name of function.
 */
MALI_IMPORT
void mali_api_trace_func_begin(const char* func_name);

/**
 * Mark the end of a function call
 * @param func_name Name of function.
 */
MALI_IMPORT
void mali_api_trace_func_end(const char* func_name);

/**
 * Add a simple signed integer (by value) parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param value Integer value to add to output trace.
 * @type The text representation of the type ("vgint", "glsizei" etc)
 */
MALI_IMPORT
void mali_api_trace_param_signed_integer(s32 value, const char* type);

/**
 * Add a simple unsigned integer (by value) parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param value Integer value to add to output trace.
 * @type The text representation of the type ("vguint", "glenum" etc)
 */
MALI_IMPORT
void mali_api_trace_param_unsigned_integer(u32 value, const char* type);

/**
 * Add a simple floating point (by value) parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param value Floating point value to add to output trace.
 * @type The text representation of the type ("vgfloat", "glfloat" etc)
 */
MALI_IMPORT
void mali_api_trace_param_float(float value, const char* type);

/**
 * Add an array of signed 8-bit integer parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param dir The kind of parameter (output/input/other)
 * @ptr Pointer to the integer array
 * @count Number of elements in the array
 * @offset Indicate that number of bytes before ptr the array actually starts (used for negative stride inputs)
 * @type The text representation of the type (e.g. "vgbytearray")
 * @see mali_api_trace_direction
 */
MALI_IMPORT
void mali_api_trace_param_int8_array(enum mali_api_trace_direction dir, const s8* ptr, u32 count, u32 offset, const char* type);

/**
 * Add an array of unsigned 8-bit integer parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param dir The kind of parameter (output/input/other)
 * @ptr Pointer to the integer array
 * @count Number of elements in the array
 * @offset Indicate that number of bytes before ptr the array actually starts (used for negative stride inputs)
 * @type The text representation of the type (e.g. "vgbytearray")
 * @see mali_api_trace_direction
 */
MALI_IMPORT
void mali_api_trace_param_uint8_array(enum mali_api_trace_direction dir, const u8* ptr, u32 count, u32 offset, const char* type);

/**
 * Add an array of signed 16-bit integer parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param dir The kind of parameter (output/input/other)
 * @ptr Pointer to the integer array
 * @count Number of elements in the array
 * @type The text representation of the type (e.g. "vgshortarray")
 * @see mali_api_trace_direction
 */
MALI_IMPORT
void mali_api_trace_param_int16_array(enum mali_api_trace_direction dir, const s16* ptr, u32 count, const char* type);

/**
 * Add an array of signed 32-bit integer parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param dir The kind of parameter (output/input/other)
 * @ptr Pointer to the integer array
 * @count Number of elements in the array
 * @type The text representation of the type (e.g. "eglintarray")
 * @see mali_api_trace_direction
 */
MALI_IMPORT
void mali_api_trace_param_int32_array(enum mali_api_trace_direction dir, const s32* ptr, u32 count, const char* type);

/**
 * Add an array of unsigned 32-bit integer parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param dir The kind of parameter (output/input/other)
 * @ptr Pointer to the integer array
 * @count Number of elements in the array
 * @type The text representation of the type (e.g. "gluintarray")
 * @see mali_api_trace_direction
 */
MALI_IMPORT
void mali_api_trace_param_uint32_array(enum mali_api_trace_direction dir, const u32* ptr, u32 count, const char* type);

/**
 * Add an array of floating point parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param dir The kind of parameter (output/input/other)
 * @ptr Pointer to the floating point array
 * @count Number of elements in the array
 * @type The text representation of the type (e.g. "vgfloatarray")
 * @see mali_api_trace_direction
 */
MALI_IMPORT
void mali_api_trace_param_float_array(enum mali_api_trace_direction dir, const float* ptr, u32 count, const char* type);

/**
 * Add an array of C strings (null-terminated char arrays) parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * Always treated as an input only parameter, and the type is always "cstrarray"
 * @ptr Pointer to the integer array
 * @count Number of elements (C strings/char pointers) in the array
 */
MALI_IMPORT
void mali_api_trace_param_cstr_array(const char** ptr, u32 count);

/**
 * Add a simple signed integer (by value) return to the trace output.
 * @value The integer value to add to the trace
 * @type The textual representation of the return value type (e.g. "vgint")
 */
MALI_IMPORT
void mali_api_trace_return_signed_integer(s32 value, const char* type);

/**
 * Add a simple unsigned integer (by value) return to the trace output.
 * @value The integer value to add to the trace
 * @type The textual representation of the return value type (e.g. "vgerrorcode")
 */
MALI_IMPORT
void mali_api_trace_return_unsigned_integer(u32 value, const char* type);

/**
 * Add a simple floating point (by value) return to the trace output.
 * @value The floating point value to add to the trace
 * @type The textual representation of the return value type (e.g. "vgfloat")
 */
MALI_IMPORT
void mali_api_trace_return_float(float value, const char* type);

/**
 * Add return data for a previously added output parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param id The index of the parameter (1 is the first, 2 is the second and so on. 0 is for returning an array through the return statement)
 * @ptr Pointer to the integer array with output values
 * @count Number of elements to return (might be less than the count for this parameter originally)
 * @type The text representation of the type (should be the same as used for this parameter originally)
 * @see mali_api_trace_param_uint8_array
 */
MALI_IMPORT
void mali_api_trace_return_uint8_array(u32 id, const u8* ptr, u32 count, const char* type);

/**
 * Add return data for a previously added output parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param id The index of the parameter (1 is the first, 2 is the second and so on. 0 is for returning an array through the return statement)
 * @ptr Pointer to the integer array with output values
 * @count Number of elements to return (might be less than the count for this parameter originally)
 * @type The text representation of the type (should be the same as used for this parameter originally)
 * @see mali_api_trace_param_int32_array
 */
MALI_IMPORT
void mali_api_trace_return_int32_array(u32 id, const s32* ptr, u32 count, const char* type);

/**
 * Add return data for a previously added output parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param id The index of the parameter (1 is the first, 2 is the second and so on. 0 is for returning an array through the return statement)
 * @ptr Pointer to the integer array with output values
 * @count Number of elements to return (might be less than the count for this parameter originally)
 * @type The text representation of the type (should be the same as used for this parameter originally)
 * @see mali_api_trace_param_uint32_array
 */
MALI_IMPORT
void mali_api_trace_return_uint32_array(u32 id, const u32* ptr, u32 count, const char* type);

/**
 * Add return data for a previously added output parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * @param id The index of the parameter (1 is the first, 2 is the second and so on. 0 is for returning an array through the return statement)
 * @ptr Pointer to the float array with output values
 * @count Number of elements to return (might be less than the count for this parameter originally)
 * @type The text representation of the type (should be the same as used for this parameter originally)
 * @see mali_api_trace_param_float_array
 */
MALI_IMPORT
void mali_api_trace_return_float_array(u32 id, const float* ptr, u32 count, const char* type);

/**
 * Add return data for a previously added output parameter to the trace output.
 * This must be called between mali_api_trace_func_begin() and mali_api_trace_func_end().
 * Handles are just unsigned 32-bit values and normally handled through the generic 32-bit integer functions.
 * Returning handles is essensial and must be included even if just input parameter tracing is turned on.
 * That is why we have a seperate return function for handle arrays, so that they will be included even if output parameter tracing is turned off.
 * @param id The index of the parameter (1 is the first, 2 is the second and so on. 0 is for returning an array through the return statement)
 * @ptr Pointer to the integer array with output values
 * @count Number of elements to return (might be less than the count for this parameter originally)
 * @type The text representation of the type (should be the same as used for this parameter originally)
 * @see mali_api_trace_param_uint32_array
 */
MALI_IMPORT
void mali_api_trace_return_handle_array(u32 id, const u32* ptr, u32 count, const char* type);

/**
 * Add a user buffer to the trace output (e.g. vertex attrib buffer).
 * @param buffer Pointer to the user buffer
 * @size Size of user buffer (in bytes)
 */
MALI_IMPORT
void mali_api_trace_dump_user_buffer(const char* buffer, u32 size);

#define MALI_API_TRACE_FUNC_BEGIN() mali_api_trace_func_begin(MALI_FUNCTION)
#define MALI_API_TRACE_FUNC_END() mali_api_trace_func_end(MALI_FUNCTION)

#define MALI_API_TRACE_PARAM_CHAR_ARRAY(dir, ptr, count, offset) mali_api_trace_param_uint8_array(dir, (u8*)ptr, count, offset, "chararray")
#define MALI_API_TRACE_PARAM_CSTR_ARRAY(ptr, count) mali_api_trace_param_cstr_array(ptr, count)

#define MALI_API_TRACE_RETURN_CHAR_ARRAY(id, ptr, count) mali_api_trace_return_uint8_array(id, (u8*)(ptr), count, "chararray")

#else

/* API tracing not enabled, use dummy macros */

#define MALI_API_TRACE_FUNC_BEGIN()
#define MALI_API_TRACE_FUNC_END()

#define MALI_API_TRACE_PARAM_CHAR_ARRAY(dir, ptr, count, offset)
#define MALI_API_TRACE_PARAM_CSTR_ARRAY(ptr, count)

#define MALI_API_TRACE_RETURN_CHAR_ARRAY(id, ptr, count)

#endif

#ifdef __cplusplus
}
#endif

#endif /* _MALI_TRACE_H_ */
