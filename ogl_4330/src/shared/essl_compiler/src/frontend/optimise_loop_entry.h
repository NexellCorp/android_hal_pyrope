/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MIDDLE_OPTIMISE_LOOP_ENTRY
#define MIDDLE_OPTIMISE_LOOP_ENTRY

#include "common/essl_mem.h"
#include "common/essl_node.h"
#include "common/essl_symbol.h"
#include "common/essl_target.h"

memerr _essl_optimise_loop_entry(mempool *pool, symbol *function, target_descriptor *desc);

#endif
