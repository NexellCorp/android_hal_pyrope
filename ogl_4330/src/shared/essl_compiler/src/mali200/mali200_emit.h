/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
/* Make the final, binary, mali200 program,
given a series of 'm200_instruction_word'-s */

#ifndef MALI200_EMIT_H
#define MALI200_EMIT_H

#include "common/essl_node.h"
#include "common/translation_unit.h"
#include "common/output_buffer.h"
#include "mali200/mali200_instruction.h"


typedef struct _tag_mali200_emit_context
{
	output_buffer *output_buf;
	error_context* err_ctx;            /**< where to put internal error messages etc. */
	m200_instruction_word *word;       /**< the current word being emitted.... */
	essl_bool store_emitted;
	translation_unit *tu;
} mali200_emit_context;

memerr _essl_mali200_emit_function(error_context *err_ctx, output_buffer *buf, translation_unit *tu, symbol *function);

memerr _essl_mali200_emit_translation_unit(error_context *err_ctx, output_buffer *buf, translation_unit *tu);


#endif /* MALI200_EMIT_H */
