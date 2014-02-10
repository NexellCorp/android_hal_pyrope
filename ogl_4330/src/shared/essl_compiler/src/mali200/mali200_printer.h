/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI200_PRINTER_H
#define MALI200_PRINTER_H

#ifndef NEEDS_STDIO
#define NEEDS_STDIO
#endif

#include "common/essl_system.h"
#include "common/essl_stringbuffer.h"
#include "common/essl_symbol.h"
#include "common/essl_mem.h"
#include "common/unique_names.h"


memerr _essl_m200_print_function(FILE *out, mempool *pool,
								 symbol *fun, /*@null@*/ unique_name_context **node_name_out);

const char* _essl_reg_name(int reg);



#endif /* MALI200_PRINTER_H */
