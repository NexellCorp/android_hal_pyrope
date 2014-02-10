/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MIDDLE_MIDDLE_H
#define MIDDLE_MIDDLE_H

#include "common/essl_mem.h"
#include "common/essl_target.h"
#include "common/error_reporting.h"
#include "common/essl_node.h"
#include "common/compiler_options.h"
#include "common/translation_unit.h"

memerr _essl_middle_transform(mempool *pool, error_context *err, typestorage_context *ts_ctx, target_descriptor *desc, /*@null@*/compiler_options* opts, translation_unit *tu);


#endif /* MIDDLE_MIDDLE_H */
