/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef MIDDLE_OPTIMISE_CSE_H
#define MIDDLE_OPTIMISE_CSE_H


#include "common/essl_mem.h"
#include "common/essl_target.h"
#include "common/basic_block.h"
#include "common/compiler_options.h"




memerr _essl_optimise_cse(mempool *pool, target_descriptor *desc, typestorage_context *ts_ctx, control_flow_graph *cfg);








#endif /* MIDDLE_OPTIMISE_CSE_H */

