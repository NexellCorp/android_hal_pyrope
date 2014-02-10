/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef FRONTEND_GLOBAL_VARIABLE_INLINING_H
#define FRONTEND_GLOBAL_VARIABLE_INLINING_H

#include "common/essl_common.h"
#include "common/essl_mem.h"
#include "common/translation_unit.h"
#include "common/ptrset.h"

memerr _essl_inline_global_variables(mempool *pool, translation_unit *tu, ptrset *vars_to_inline);


#endif /* FRONTEND_GLOBAL_VARIABLE_INLINING_H */

