/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/output_buffer.h"

#define OUTPUT_BUFFER_INITIAL_SIZE 32

memerr _essl_output_buffer_init(output_buffer *buf, mempool *pool)
{
	buf->pool = pool;
	buf->current_word_index = 0;
	buf->current_bit_index = 0;
	buf->capacity = 0;
	buf->capacity = OUTPUT_BUFFER_INITIAL_SIZE;
	ESSL_CHECK(buf->buf = _essl_mempool_alloc(pool, buf->capacity*sizeof(u32)));
	buf->buf[0] = 0;
	return MEM_OK;
}

static memerr increase_buffer(output_buffer *buf)
{
	size_t new_size;
	u32 *new_buf;
	assert(buf != 0);
	new_size = buf->capacity*2;

	ESSL_CHECK((new_buf = _essl_mempool_alloc(buf->pool, new_size*sizeof(u32))));
	memcpy(new_buf, buf->buf, buf->capacity*sizeof(u32));
	buf->buf = new_buf;
	buf->capacity = new_size;
	return MEM_OK;
}



void _essl_output_buffer_replace_bits(output_buffer *buf, size_t word_position, size_t bit_position, size_t n_bits, u32 value)
{
	u32 mask;
	assert(n_bits <= 32);
	assert(bit_position < 32);
	mask = (unsigned)((int)(1 << n_bits) - 1);
	if(n_bits == 32)
	{
		mask = (u32)-1;
	}
	/*NOTE: This assert is useful, but it gets in the way when experimenting with more than 16 work registers on GP2 */
	/*assert((value&~mask) == 0);*/

	buf->buf[word_position] = (buf->buf[word_position] & ~(mask<<bit_position)) | ((value&mask)<<bit_position);

	if(bit_position + n_bits > 32)
	{
		buf->buf[word_position+1] = (buf->buf[word_position+1] & ~(mask>>(32 - (u32)bit_position))) | ((value&mask)>>(32 - (u32)bit_position));
	}

}

u32 _essl_output_buffer_retrieve_bits(output_buffer *buf, size_t word_position, size_t bit_position, size_t n_bits)
{
	u32 mask;
	assert(n_bits <= 32);
	assert(bit_position < 32);
	assert(bit_position + n_bits <= 32);
	mask = (unsigned)((int)(1 << n_bits) - 1);
	if(n_bits == 32)
	{
		mask = (u32)-1;
	}

	return (buf->buf[word_position] >> bit_position) & mask;
}


memerr _essl_output_buffer_append_bits(output_buffer *buf, size_t n_bits, u32 value)
{
	if(buf->current_bit_index + n_bits < 32)
	{
		_essl_output_buffer_replace_bits(buf, buf->current_word_index, buf->current_bit_index, n_bits, value);
		buf->current_bit_index += n_bits;

	} else {
		if(buf->current_word_index + 1 >= buf->capacity)
		{
			ESSL_CHECK(increase_buffer(buf));
		}
		buf->buf[buf->current_word_index + 1] = 0;
		_essl_output_buffer_replace_bits(buf, buf->current_word_index, buf->current_bit_index, n_bits, value);
		++buf->current_word_index;
		buf->current_bit_index = (size_t)((int)buf->current_bit_index + (int)n_bits - 32);
		
	}

	return MEM_OK;
}

memerr _essl_output_buffer_append_int8(output_buffer *buf, u32 value)
{
	assert(buf->current_bit_index % 8 == 0);
	ESSL_CHECK(_essl_output_buffer_append_bits(buf, 8, value));
	return MEM_OK;
}

memerr _essl_output_buffer_append_int16(output_buffer *buf, u32 value)
{
	assert(buf->current_bit_index % 16 == 0);
	ESSL_CHECK(_essl_output_buffer_append_bits(buf, 16, value));
	return MEM_OK;
}

memerr _essl_output_buffer_append_int32(output_buffer *buf, u32 value)
{
	assert(buf->current_bit_index == 0);
	ESSL_CHECK(_essl_output_buffer_append_bits(buf, 32, value));
	return MEM_OK;
}

size_t _essl_output_buffer_get_word_position(output_buffer *buf)
{
	return buf->current_word_index;
}

size_t _essl_output_buffer_get_bit_position(output_buffer *buf)
{
	return buf->current_bit_index;
}

size_t _essl_output_buffer_get_byte_position(output_buffer *buf)
{
	assert(buf->current_bit_index % 8 == 0);
	return buf->current_word_index*4 + buf->current_bit_index / 8;
}

size_t _essl_output_buffer_get_size(output_buffer *buf)
{
	return buf->current_word_index;

}

u32 *_essl_output_buffer_get_raw_pointer(output_buffer *buf)
{
	return buf->buf;
}


void _essl_buffer_native_to_le_byteswap(u32 *ptr, size_t n_words)
{
	size_t i;
	for(i = 0; i < n_words; ++i)
	{
		u32 word = ptr[i];
		unsigned char *cptr = (unsigned char*)&ptr[i];
		cptr[0] = (unsigned char)((word>> 0) & 0xff);
		cptr[1] = (unsigned char)((word>> 8) & 0xff);
		cptr[2] = (unsigned char)((word>>16) & 0xff);
		cptr[3] = (unsigned char)((word>>24) & 0xff);
		
	}

}

void _essl_output_buffer_native_to_le_byteswap(output_buffer *buf)
{
	
	size_t n = _essl_output_buffer_get_size(buf);
	u32 *ptr = _essl_output_buffer_get_raw_pointer(buf);
	_essl_buffer_native_to_le_byteswap(ptr, n);
}
