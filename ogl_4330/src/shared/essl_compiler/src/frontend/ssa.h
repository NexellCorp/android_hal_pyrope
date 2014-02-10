/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_SSA_H
#define COMMON_SSA_H

#include "common/essl_symbol.h"
#include "common/error_reporting.h"

memerr _essl_ssa_transform(mempool *pool, target_descriptor *desc, error_context *err, symbol *function);


#endif /* COMMON_SSA_H */
