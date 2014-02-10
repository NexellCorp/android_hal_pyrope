/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MIDDLE_DOMINATOR_H
#define MIDDLE_DOMINATOR_H

#include "common/basic_block.h"


memerr _essl_basic_block_setup_postorder_sequence(control_flow_graph *fb, mempool *pool);


memerr _essl_postorder_visit(/*@null@*/ void *context, control_flow_graph *fb, memerr (*fun)(void *context, basic_block *b));

memerr _essl_reverse_postorder_visit(/*@null@*/ void *context, control_flow_graph *fb, memerr (*fun)(void *context, basic_block *b));


memerr _essl_compute_dominance_information(mempool *pool, symbol *fb);


















#endif /* MIDDLE_DOMINATOR_H */
