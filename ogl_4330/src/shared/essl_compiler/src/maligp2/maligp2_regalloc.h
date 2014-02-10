/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef MALIGP2_REGALLOC_H
#define MALIGP2_REGALLOC_H

#include "maligp2/maligp2_liveness.h"
#include "common/translation_unit.h"
#include "maligp2/maligp2_relocation.h"
#include "common/unique_names.h"
#include "common/error_reporting.h"
#include "common/compiler_options.h"

memerr _essl_maligp2_allocate_registers(mempool *pool, symbol *function, maligp2_relocation_context *rel_ctx, translation_unit *tu, typestorage_context *ts_ctx, error_context *err, compiler_options *opts, unique_name_context *names);
memerr _essl_maligp2_fixup_constants(mempool *pool, maligp2_relocation_context *rel_ctx, translation_unit *tu, typestorage_context *ts_ctx);


#endif /* MALIGP2_REGALLOC_H */
