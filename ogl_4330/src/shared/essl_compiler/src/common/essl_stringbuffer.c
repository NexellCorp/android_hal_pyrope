/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#define NEEDS_STDIO  /* for snprintf */
 
#include "common/essl_system.h"
#include "common/essl_stringbuffer.h" 
#include "common/essl_mem.h"
#include "common/essl_common.h"
#include "common/error_reporting.h"

/* 
 *	Due to stupidity in Windows, snprintf does *not* return the actual number of
 *	characters required to hold the output. We are therefore forced to use fixed-
 *	size buffers for rendering numbers into.
 */
#define	DUMMY_BUFFER_LENGTH_INT		32		/* >= strlen("-2000000000") +1 */
#define	DUMMY_BUFFER_LENGTH_FLOAT	32		/* >= strlen("+3.1415926e+00") + 1 */
#define	DUMMY_BUFFER_LENGTH_STRING	2048	/* = a big value */

/*
 *	Private functions
 */
 
/**
 *	Reserves space in the buffer for a number of characters, and returns a pointer
 *	to the reserved space. The pointer returned should always point to a nul character
 *	and the reserved space is already terminated by a nul character also.
 *
 *	\param	buffer	The string buffer to append to
 *	\param	size	The number of characters to append
 *	\return			Pointer to the appended space, or NULL if out of memory.
 */
/*@ null @*/ static char* _essl_string_buffer_reserve(string_buffer* buffer, size_t size)
{
	char* free_space;

	if(buffer->last_block==NULL || buffer->last_block->used + size >= STRINGBUFFER_BLOCK_SIZE - 1)
	{
		/* Check for oversize blocks */
		size_t oversize = (size >= STRINGBUFFER_BLOCK_SIZE) ? size - STRINGBUFFER_BLOCK_SIZE + 1 : 0;
		
		string_buffer_block* new_block = _essl_mempool_alloc(buffer->buffer_pool, sizeof(string_buffer_block) + oversize);
		
		ESSL_CHECK(new_block);
		
		new_block->buffer[0]=0;
		new_block->used = 0;
		new_block->next_block = NULL;
		
		if(buffer->last_block!=NULL)
		{
			buffer->last_block->next_block = new_block;
		}
		else
		{
			buffer->first_block = new_block;
		}
		
		buffer->last_block = new_block;
	}
	
	free_space = &buffer->last_block->buffer[buffer->last_block->used];
	buffer->last_block->used += size;
	buffer->last_block->buffer[buffer->last_block->used] = 0;
	
	return free_space;
}

/**
 *	Public functions
 */
 
string_buffer* _essl_new_string_buffer(mempool* pool)
{
	string_buffer* buffer = (string_buffer*) _essl_mempool_alloc(pool, sizeof(string_buffer) );
	ESSL_CHECK(buffer);
	buffer->buffer_pool = pool;
	buffer->first_block = NULL;
	buffer->last_block = NULL;

	return buffer;
}


memerr _essl_string_buffer_reset(string_buffer* buffer)
{
	buffer->first_block = NULL;
	buffer->last_block = NULL;
	
	return MEM_OK;
}


memerr _essl_string_buffer_put_str(string_buffer* buffer, const char* str)
{
	size_t len;
	char* dest;
	if (str==NULL)  str = "(null)";
	len = strlen(str);
	dest = _essl_string_buffer_reserve(buffer, len);
	ESSL_CHECK(dest);
	strncpy(dest, str, len);
	return MEM_OK;
}


memerr _essl_string_buffer_put_string(string_buffer* buffer, string* str)
{
	if(str==NULL || str->ptr==NULL) {
		ESSL_CHECK(_essl_string_buffer_put_str(buffer,"(null)"));
	} else {
		char* dest;
		size_t len = (size_t) str->len;
		dest = _essl_string_buffer_reserve(buffer, len);
		ESSL_CHECK(dest);
		strncpy(dest, str->ptr, len);
	}
	return MEM_OK;
}


memerr _essl_string_buffer_put_int(string_buffer* buffer, int i)
{
	char* dest;
	char dummy[DUMMY_BUFFER_LENGTH_INT];
	int len = (int) snprintf(dummy, sizeof(dummy), "%d", i);
	if(len > 0)
	{
		dest = _essl_string_buffer_reserve(buffer, len);
		ESSL_CHECK(dest);
		(void) snprintf(dest, len+1, "%d", i);
	}
	return MEM_OK;
}


void _essl_string_buffer_put_unsigned_int(string_buffer* buffer, unsigned int i)
{
	char dummy[DUMMY_BUFFER_LENGTH_INT];
	
	int len = (int) snprintf(dummy, sizeof(dummy), "%u", i);
	if(len > 0)
	{

		char* dest = _essl_string_buffer_reserve(buffer, len);
	
		if(dest!=NULL)
			(void) snprintf(dest, len+1, "%u", i);
	}
}

void _essl_string_buffer_put_float(string_buffer* buffer, float f)
{
	char dummy[DUMMY_BUFFER_LENGTH_FLOAT];
	
	size_t len = (size_t) snprintf(dummy, sizeof(dummy), "%.5e", f);
	char *dest;
	/* msvc printf is slightly different from gcc/C99 printf, look for these patterns and adjust them */
	if(strstr(dummy, "1.#INF")) 
	{
		strncpy(dummy, "inf", DUMMY_BUFFER_LENGTH_FLOAT);
	} else if(strstr(dummy, "-1.#INF")) 
	{
		strncpy(dummy, "-inf", DUMMY_BUFFER_LENGTH_FLOAT);
	} else if(strstr(dummy, "NAN")) 
	{
		strncpy(dummy, "NaN", DUMMY_BUFFER_LENGTH_FLOAT);
	} else {
		/* change 1.234e+056 to 1.234e+56 */
		if(len >= 5 && dummy[len-3] == '0' && dummy[len-5] == 'e')
		{
			dummy[len-3] = dummy[len-2];
			dummy[len-2] = dummy[len-1];
			dummy[len-1] = '\0';
		}
	}
#ifdef __SYMBIAN32__
	// Symbian produces an upper-case 'e' for the exponent:
	if (dummy[len-4] == 'E') {
		dummy[len-4] = 'e';
	}
#endif
	len = strlen(dummy);
	dest = _essl_string_buffer_reserve(buffer, len);

	
	if(dest!=NULL)
		(void) snprintf(dest, len+1, "%s", dummy);
}

memerr _essl_string_buffer_put_formatted(string_buffer* buffer, const char* fmt, ...)
{
	char dummy[DUMMY_BUFFER_LENGTH_STRING];
	va_list arglist;
	int len;
	char* dest;

	va_start(arglist, fmt);
	len = vsnprintf(dummy, sizeof(dummy), fmt, arglist);
	va_end(arglist);

	ESSL_CHECK(len >= 0);

	dest = _essl_string_buffer_reserve(buffer, (size_t)len);
	ESSL_CHECK(dest);

	memcpy(dest, dummy, (size_t)len);

	return MEM_OK;
}

char* _essl_string_buffers_to_string(string_buffer* buffer1, string_buffer* buffer2, mempool *pool)
{
	if((buffer1 != NULL && buffer1->last_block != NULL) || (buffer2 != NULL && buffer2->last_block != NULL))
	{
		string_buffer_block* block;
		size_t length = 0;
		char* outbuffer;
		char* outptr;
		
		if (buffer1 != NULL)
		{
			block = buffer1->first_block;
			while(block!=NULL)
			{
				length += block->used;	
				block = block->next_block;
			}
		}

		if (buffer2 != NULL)
		{
			block = buffer2->first_block;
			while(block!=NULL)
			{
				length += block->used;	
				block = block->next_block;
			}
		}
		
		outbuffer = outptr = _essl_mempool_alloc(pool, length+1 );
		
		if(outbuffer!=NULL)
		{
			outbuffer[0]=0;
			
			if (buffer1 != NULL)
			{
				block = buffer1->first_block;
				while(block!=NULL)
				{
					strcpy(outptr, block->buffer);
					outptr += block->used;	
					block = block->next_block;
				}
			}

			if (buffer2 != NULL)
			{
				block = buffer2->first_block;
				while(block!=NULL)
				{
					strcpy(outptr, block->buffer);
					outptr += block->used;	
					block = block->next_block;
				}
			}
		}		
		return outbuffer;
	}
	return NULL;
}

char* _essl_string_buffer_to_string(string_buffer* buffer, mempool *pool)
{
	return _essl_string_buffers_to_string(buffer, NULL, pool);
}

/*
memerr _essl_string_buffer_print(const string_buffer* buffer, FILE* ofile)
{
	const string_buffer_block* block = buffer->first_block;

	while(block!=NULL)
	{
		if (fwrite(block->buffer, sizeof(*block->buffer), block->used, ofile) != block->used)
		{
			return MEM_ERROR;
		}
		block = block->next_block;
	}

	return MEM_OK;
}
*/


        /* Lest we forget another approach: 
		 * The following macro package presumes that two variables 
		 * are declared and initialized:   char* buf; size_t bufZ; 
		 *
        ** Copy the parameter C-string into buf: *
++define BUF_STR(STR)        { const char* name = STR;\
                              size_t nameZ = strlen(name);\
                              if (bufZ<nameZ) return 0;\
                              memcpy(buf, name, nameZ);\
                              buf += nameZ; bufZ -= nameZ; }
        ** Copy 2 parameter C-strings into buf: *
++define BUF2STR(STR1,STR2)    BUF_STR(STR1) BUF_STR(STR2)
        ** Copy the decimal representation of  0<=SINT<1000  into buf, etc.: *
++define BUF_SINT(SINT,STR2) { int s = SINT;\
                              if (s>1000 || s<0) return 0;\
                              if (bufZ<3) return 0;\
                              if (s>=100) { buf[0] = '0'+s/100; ++buf; --bufZ; s = s%100; }\
                              if (s>=10) { buf[0] = '0'+s/10; ++buf; --bufZ; s = s%10; }\
                              buf[0] = '0'+s; ++buf; --bufZ; BUF_STR(STR2)}
        ** Adjust  buf  and  bufZ  after appending SZ chars, etc.: *
++define ADJUST_BUF(SZ,STR2) { size_t z = SZ; buf+=z; bufZ-=z; BUF_STR(STR2)}
        ** Append the standard C-string zero termination char: *
++define BUF_TERMINATE       { if (bufZ<1) return 0; buf[0]=0; }
		*/
