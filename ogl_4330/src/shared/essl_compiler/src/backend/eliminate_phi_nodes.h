/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BACKEND_ELIMINATE_PHI_NODES_H
#define BACKEND_ELIMINATE_PHI_NODES_H

#include "common/basic_block.h"
#include "common/ptrdict.h"
#include "backend/liveness.h"

typedef memerr (*insert_move_callback)(void *ctx, node *src, node *dst, int earliest, int latest, basic_block *block, essl_bool backward,
									   int *src_position_p, int *dst_position_p, node ***src_ref_p, node ***dst_ref_p);

memerr _essl_eliminate_phi_nodes(mempool *pool, liveness_context *ctx, insert_move_callback insert_move, void *callback_ctx);


#endif
