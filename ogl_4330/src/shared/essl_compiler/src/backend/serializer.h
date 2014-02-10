/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef BACKEND_SERIALIZER_H
#define BACKEND_SERIALIZER_H

#include "common/output_buffer.h"
#include "common/translation_unit.h"

typedef enum {
	SERIALIZER_HOST_ENDIAN,
	SERIALIZER_LITTLE_ENDIAN
} serializer_endianness;

typedef memerr (*shader_binary_writer)(error_context *e_ctx, output_buffer *buf, translation_unit *tu);
typedef memerr (*shader_binary_func_writer)(error_context *e_ctx, output_buffer *buf, translation_unit *tu, symbol *func);
memerr _essl_serialize_translation_unit(mempool *pool, error_context *e_ctx, output_buffer *buf, 
				translation_unit *tu, 
				shader_binary_writer writer, 
				shader_binary_func_writer func_writer,
				serializer_endianness endianness);





#endif /* BACKEND_SERIALIZER_H */
