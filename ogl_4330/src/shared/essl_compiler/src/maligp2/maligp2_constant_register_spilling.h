/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALIGP2_CONSTANT_REGISTER_SPILLING
#define MALIGP2_CONSTANT_REGISTER_SPILLING

#include "backend/liveness.h"
#include "maligp2/maligp2_virtual_regs.h"
#include "maligp2/maligp2_relocation.h"

memerr _essl_maligp2_constant_register_spilling(mempool *pool, virtual_reg_context *vr_ctx, control_flow_graph *cfg,
												typestorage_context *ts_ctx, maligp2_relocation_context *rel_ctx,
												liveness_context *liv_ctx);

#endif
