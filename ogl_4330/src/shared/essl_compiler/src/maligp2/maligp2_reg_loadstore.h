/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALIGP2_REG_LOADSTORE_H
#define MALIGP2_REG_LOADSTORE_H

#include "common/essl_mem.h"
#include "common/priority_queue.h"
#include "common/essl_list.h"
#include "common/essl_target.h"
#include "common/ptrset.h"
#include "backend/liveness.h"
#include "backend/interference_graph.h"
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_virtual_regs.h"

typedef struct _tag_loadstore_allocation {
	struct _tag_loadstore_allocation *next;
	node *var;
	live_delimiter *definition;
	maligp2_instruction_word *def_word;
	maligp2_instruction_word *result_word;
	maligp2_instruction_word *store_word;
	basic_block *block;
	maligp2_instruction *store;
	int index;
} loadstore_allocation;
ASSUME_ALIASING(loadstore_allocation, generic_list);

typedef struct _tag_loadstore_context {
	/* Persistant information */
	mempool *pool;
	control_flow_graph *cfg;
	target_descriptor *desc;
	virtual_reg_context *vr_ctx;
	compiler_options *opts;

	/* Per-run information */
	mempool *temp_pool;
	liveness_context *liv_ctx;
	live_range *unallocated_ranges;

	/* Internal state */
	priqueue var_defs;
	loadstore_allocation *stores;
	basic_block *current_block;
	int next_comp;
} loadstore_context;

loadstore_context *_essl_maligp2_create_loadstore_context(mempool *pool,
														  control_flow_graph *cfg,
														  virtual_reg_context *vr_ctx,
														  compiler_options *opts);

memerr _essl_maligp2_allocate_register_loadstores(mempool *temp_pool,
												  loadstore_context *ctx,
												  liveness_context *liv_ctx,
												  live_range *unallocated_ranges);

memerr _essl_maligp2_produce_conflict_graph(loadstore_context *ctx, liveness_context *liv_ctx);

#endif /* MALIGP2_REG_LOADSTORE_H */
