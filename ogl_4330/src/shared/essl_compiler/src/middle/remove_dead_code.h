/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MIDDLE_REMOVE_DEAD_CODE_H
#define MIDDLE_REMOVE_DEAD_CODE_H

#include "common/essl_symbol.h"
#include "common/essl_mem.h"

memerr _essl_remove_dead_code(mempool *pool, symbol *function, typestorage_context *ts_ctx);


#endif

