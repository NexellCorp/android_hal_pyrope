/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
/* Make the final, binary, maligp2 program,
given a series of 'maligp2_instruction_word'-s */

#ifndef MALIGP2_EMIT_H
#define MALIGP2_EMIT_H

#include "common/essl_node.h"
#include "common/translation_unit.h"


memerr _essl_maligp2_emit_translation_unit(error_context *err_ctx, output_buffer *buf, translation_unit *tu, essl_bool relative_jumps);
memerr _essl_maligp2_emit_function(error_context *err_ctx, output_buffer *buf, translation_unit *tu, symbol *function);

#endif /* MALIGP2_EMIT_H */
