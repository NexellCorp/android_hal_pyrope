/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI200_SCHEDULER_H
#define MALI200_SCHEDULER_H

#include "common/basic_block.h"
#include "common/essl_target.h"
#include "common/error_reporting.h"
#include "backend/abstract_scheduler.h"
#include "backend/extra_info.h"
#include "mali200/mali200_instruction.h"
#include "mali200/mali200_relocation.h"

typedef struct _tag_mali200_scheduler_context
{
	mempool *pool;
	scheduler_context schedule_ctx;
	scheduler_context *sctx;
	target_descriptor *desc;
	translation_unit *tu;
	control_flow_graph *cfg;
	symbol *function;
	essl_bool can_add_cycles;
	essl_bool program_does_conditional_discard;
	int earliest_cycle;
	/*@null@*/ m200_instruction_word *earliest_instruction_word;
	/*@null@*/ m200_instruction_word *latest_instruction_word;

	mali200_relocation_context *relocation_context;
	error_context *error_context;
} mali200_scheduler_context;

memerr _essl_mali200_schedule_function(mempool *pool, translation_unit *tu, symbol *function, mali200_relocation_context *relocation_context, error_context *error_context);
int _essl_mali200_op_weight(node *op);

#endif
