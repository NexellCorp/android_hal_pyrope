/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI200_EXTERNAL_IMPLEMENTATION_H
#define MALI200_EXTERNAL_IMPLEMENTATION_H

#include "common/essl_mem.h"
#include "common/essl_target.h"
#include "common/ptrdict.h"
#include "common/translation_unit.h"

memerr _essl_rewrite_sampler_external_accesses(mempool *pool, symbol *fun, target_descriptor *desc, typestorage_context *ts_ctx, translation_unit *tu, ptrdict *fixup_symbols);

#endif /* MALI200_EXTERNAL_IMPLEMENTATION_H */
