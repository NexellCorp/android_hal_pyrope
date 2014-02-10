/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_TYPE_NAME_H
#define COMMON_TYPE_NAME_H

#include "common/essl_node.h"
#include "common/essl_symbol.h"

const char* _essl_get_type_name(mempool *pool, const type_specifier* type);

#endif
