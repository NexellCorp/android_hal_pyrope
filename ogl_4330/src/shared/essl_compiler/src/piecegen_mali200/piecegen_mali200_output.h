/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef PIECEGEN_MALI200_OUTPUT_H
#define PIECEGEN_MALI200_OUTPUT_H

#include "common/translation_unit.h"
#include "common/output_buffer.h"

memerr _essl_piecegen_mali200_output_pieces(mempool *pool, error_context *e_ctx, output_buffer *out_buf, translation_unit *tu,
											char *h_file_name, char *c_file_name);

#endif
