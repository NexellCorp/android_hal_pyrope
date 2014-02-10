/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALIGP2_PRINTER_H
#define MALIGP2_PRINTER_H

#ifndef NEEDS_STDIO
#define NEEDS_STDIO
#endif

#include "common/essl_system.h"
#include "common/essl_stringbuffer.h"
#include "common/essl_symbol.h"
#include "common/essl_mem.h"
#include "common/unique_names.h"
#include "common/essl_target.h"

memerr _essl_maligp2_print_function(FILE *out, mempool *pool, symbol *fun, target_descriptor *desc, unique_name_context *node_names);



#endif /* MALIGP2_PRINTER_H */
