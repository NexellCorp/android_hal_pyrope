/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALIGP2_SCHEDULER_H
#define MALIGP2_SCHEDULER_H

#include "common/basic_block.h"
#include "common/essl_target.h"
#include "common/translation_unit.h"
#include "backend/abstract_scheduler.h"
#include "backend/extra_info.h"
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_relocation.h"

typedef struct _tag_maligp2_scheduler_context
{
	mempool *pool;
	scheduler_context schedule_ctx;
	scheduler_context *sctx;
	target_descriptor *desc;
	translation_unit *tu;
	control_flow_graph *cfg;
	symbol *function;
	essl_bool can_add_cycles;
	int earliest_cycle;
	/*@null@*/ maligp2_instruction_word *earliest_instruction_word;
	/*@null@*/ maligp2_instruction_word *latest_instruction_word;

	maligp2_relocation_context *relocation_context;
	error_context *error_context;
} maligp2_scheduler_context;

memerr _essl_maligp2_schedule_function(mempool *pool, translation_unit *tu, symbol *function, maligp2_relocation_context *relocation_context, error_context *error_context);
int _essl_maligp2_op_weight_scheduler(node *op);
int _essl_maligp2_op_weight_realistic(node *op);

#endif
