/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MIDDLE_PRINTER_H
#define MIDDLE_PRINTER_H

#ifndef NEEDS_STDIO
#define NEEDS_STDIO
#endif

#include "common/essl_system.h"
#include "common/essl_mem.h"
#include "common/essl_symbol.h"
#include "common/essl_target.h"
#include "common/translation_unit.h"


memerr _essl_debug_function_to_dot(FILE *out, mempool *pool, target_descriptor *desc, symbol *function, int display_extra_info);
memerr _essl_debug_translation_unit_to_dot(FILE *out, mempool *pool, target_descriptor *desc, translation_unit *tu, int display_extra_info);


char *_essl_debug_translation_unit_to_dot_buf(mempool *pool, target_descriptor *desc, translation_unit *tu, int display_extra_info);

const char *_essl_label_for_node(mempool *pool, target_descriptor *desc, node *n);



#endif /* MIDDLE_PRINTER_H */
