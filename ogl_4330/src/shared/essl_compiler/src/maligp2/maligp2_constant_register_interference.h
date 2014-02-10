/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef MALIGP2_CONSTANT_REGISTER_INTERFERENCE_H
#define MALIGP2_CONSTANT_REGISTER_INTERFERENCE_H

#include "common/translation_unit.h"
#include "maligp2/maligp2_relocation.h"
#include "common/compiler_options.h"

struct interference_graph_context;

struct interference_graph_context *_essl_maligp2_calc_constant_register_interference(mempool *pool, translation_unit *tu, maligp2_relocation_context *rel_ctx);



#endif /* MALIGP2_CONSTANT_REGISTER_INTERFERENCE_H */
