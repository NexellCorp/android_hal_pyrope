/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef STRING_BUFFER_H
#define STRING_BUFFER_H

#include "common/essl_system.h"
#include "common/essl_str.h"

/* Actual type is opaque */

/**
 *  Environment
 */
#define STRINGBUFFER_BLOCK_SIZE 2048

typedef struct string_buffer_block
{
	/*@null@*/ struct string_buffer_block* next_block;
	int used;
	char buffer[STRINGBUFFER_BLOCK_SIZE];
} string_buffer_block;

typedef struct string_buffer
{
	mempool *buffer_pool;
	/*@ null @*/ string_buffer_block* first_block;
	/*@ null @*/ string_buffer_block* last_block;
} string_buffer;


/**
 *	Create a new, empty string buffer in the given memory pool.
 *
 *	\param pool	The memory pool in which to allocate the string buffer
 *	\return		A pointer to a new, empty string buffer, or NULL if out of memory.
 */
/*@ null @*/ string_buffer* _essl_new_string_buffer(mempool* pool);


/**
 *	Reset a string buffer to the empty state.
 *
 *	\param buffer	The buffer to reset.
 */
memerr _essl_string_buffer_reset(string_buffer* buffer);

/**
 *	Append a null-terminated string to the buffer.
 *
 *	\param buffer	The buffer to append to.
 *	\param str		The string to append.
 */
memerr _essl_string_buffer_put_str(string_buffer* buffer, /*@null@*/const char* str);

/**
 *	Append a counted string to the buffer
 *
 *	\param buffer	The buffer to append to.
 *	\param str		The string to append.
 */
memerr _essl_string_buffer_put_string(string_buffer* buffer, string* str);

/**
 *	Append an int to the buffer
 *
 *	\param buffer	The buffer to append to.
 *	\param i		The int to append.
 */
memerr _essl_string_buffer_put_int(string_buffer* buffer, int i);

/**
 *	Append an unsigned int to the buffer
 *
 *	\param buffer	The buffer to append to.
 *	\param i		The unsigned int to append.
 */
void _essl_string_buffer_put_unsigned_int(string_buffer* buffer, unsigned int i);

/**
 *	Append a float to the buffer (in %g format)
 *
 *	\param buffer	The buffer to append to.
 *	\param f		The float to append.
 */
void _essl_string_buffer_put_float(string_buffer* buffer, float f);

/**
 *	Append a formatted string to the buffer. This uses sprintf internally
 *	so the format string follows sprintf definitions. This also uses an
 *	internal buffer, so there is a maximum length of formatted string that
 *	can be appended.
 *
 *	\param buffer	The buffer to append to.
 *	\param fmt		The format string to use for the subsequent operations.
 */

memerr _essl_string_buffer_put_formatted(string_buffer* buffer, const char* fmt, ...);

/**
 *	Copy the contents of the string buffer to a dynamically allocated string in
 *	the nominated memory pool. This does not affect the internal state of the buffer
 *	and you may therefore continue to append stuff to it if you wish.
 *
 *	\param buffer	The buffer to convert.
 *	\param pool		The memory pool in which to allocate the resulting string.
 *	\return 		The string, allocated from pool, or NULL if out of memory.
 */
 char* _essl_string_buffer_to_string(string_buffer* buffer, mempool *pool);

/**
 *	Copy the contents of the two string buffer to a dynamically allocated string in
 *	the nominated memory pool. This does not affect the internal state of the buffers
 *	and you may therefore continue to append stuff to it if you wish.
 *
 *	\param buffer1	The frist buffer to convert.
 *	\param buffer2	The second buffer to convert (concatenated).
 *	\param pool		The memory pool in which to allocate the resulting string.
 *	\return 		The string, allocated from pool, or NULL if out of memory.
 */
char* _essl_string_buffers_to_string(string_buffer* buffer1, string_buffer* buffer2, mempool *pool);

/**
 *  Print the contents of the string buffer to the specified file (or console)
 *
 *  @param buffer The buffer to print
 *  @param ofile  The file to print to (use stdout for stdout and so on)
 *  @return MEM_OK for success, MEM_ERROR for failure
 */
/*
memerr _essl_string_buffer_print(const string_buffer* buffer, FILE* ofile);
*/


#endif /* STRING_BUFFER_H */
