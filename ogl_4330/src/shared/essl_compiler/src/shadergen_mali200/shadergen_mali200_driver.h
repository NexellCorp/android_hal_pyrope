/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef SHADERGEN_MALI200_DRIVER_H
#define SHADERGEN_MALI200_DRIVER_H

#include "common/essl_mem.h"
#include "common/essl_target.h"
#include "common/essl_type.h"
#include "common/essl_node.h"
#include "common/translation_unit.h"

memerr _essl_shadergen_mali200_driver(mempool *pool,  error_context *err,
							typestorage_context *ts_ctx,  struct _tag_target_descriptor *desc,
							translation_unit *tu,  output_buffer *out_buf, compiler_options *opts);

#endif /* SHADERGEN_MALI200_DRIVER_H */
