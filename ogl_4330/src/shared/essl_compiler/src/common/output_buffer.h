/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef COMMON_OUTPUT_BUFFER_H
#define COMMON_OUTPUT_BUFFER_H

#include "common/essl_mem.h"

typedef unsigned int u32;
typedef struct output_buffer {
	mempool *pool;
	size_t current_word_index;         /**< word index (in  buf) for current free bit */
	size_t current_bit_index;             /**< bit number of current free bit */
	size_t capacity;                      /**< buffer capacity: count of int32-s  */
	u32* buf;                          /**< buffer storage */
} output_buffer;


memerr _essl_output_buffer_init(/*@out@*/ output_buffer *buf, mempool *pool);


memerr _essl_output_buffer_append_bits(output_buffer *buf, size_t n_bits, u32 value);
void _essl_output_buffer_replace_bits(output_buffer *buf, size_t word_position, size_t bit_position, size_t n_bits, u32 value);

memerr _essl_output_buffer_append_int8(output_buffer *buf, u32 value);
memerr _essl_output_buffer_append_int16(output_buffer *buf, u32 value);
memerr _essl_output_buffer_append_int32(output_buffer *buf, u32 value);

u32 _essl_output_buffer_retrieve_bits(output_buffer *buf, size_t word_position, size_t bit_position, size_t n_bits);

size_t _essl_output_buffer_get_word_position(output_buffer *buf);
size_t _essl_output_buffer_get_bit_position(output_buffer *buf);

size_t _essl_output_buffer_get_byte_position(output_buffer *buf);

/* size in words */
size_t _essl_output_buffer_get_size(output_buffer *buf);
u32 *_essl_output_buffer_get_raw_pointer(output_buffer *buf);

void _essl_output_buffer_native_to_le_byteswap(output_buffer *buf);
void _essl_buffer_native_to_le_byteswap(u32 *ptr, size_t n_words);




#endif /* COMMON_OUTPUT_BUFFER_H */





