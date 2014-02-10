/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "mali200/mali200_instruction.h"
#include "mali200/mali200_scheduler.h"
#include "mali200/mali200_slot.h"
#include "backend/extra_info.h"

#ifndef ESSL_MIN
#define ESSL_MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/*@null@*/ m200_instruction *_essl_new_mali200_instruction(mempool *pool, 
									m200_schedule_classes sc, m200_opcode opcode, int subcycle)
{
	unsigned i;
	m200_instruction *inst;
	assert(opcode != M200_UNKNOWN);
	ESSL_CHECK(inst = _essl_mempool_alloc(pool, sizeof(m200_instruction)));
	inst->opcode = opcode;
	inst->opcode_flags = 0;
	inst->instr_node = 0;
	inst->schedule_class = sc;
	for (i = 0; i < M200_MAX_INPUT_ARGS; ++i) {
		inst->args[i].reg_index = M200_REG_UNKNOWN;
		inst->args[i].arg = 0;
		inst->args[i].swizzle = _essl_create_undef_swizzle();
	}
	inst->address_offset = inst->address_multiplier = 0;
	inst->out_reg = M200_REG_UNKNOWN;
	inst->lod_offset = 0.0;
	inst->jump_target = 0;
	inst->call_target = 0;
	inst->compare_function = M200_CMP_NEVER;
	inst->subcycle = subcycle;
	inst->output_swizzle = _essl_create_identity_swizzle(M200_NATIVE_VECTOR_SIZE);

	return inst;
}


/*@null@*/ m200_instruction_word *_essl_new_mali200_instruction_word(mempool *pool, int cycle)
{
	m200_instruction_word *word;
	ESSL_CHECK(word = _essl_mempool_alloc(pool, sizeof(m200_instruction_word)));
	word->used_slots = 0;
	word->predecessor = 0;
	word->successor = 0;
	word->cycle = cycle;
	word->emit_adr = word->emit_siz = word->bran_wrd = word->bran_bit = -1;
	return word;
}

static essl_bool instruction_removable(m200_instruction_word *word, basic_block *block, m200_instruction_word *pred) {
	predecessor_list *predl;
	if (word->used_slots != 0) return ESSL_FALSE;
	for (predl = block->predecessors ; predl != 0 ; predl = predl->next) {
		basic_block *pb = predl->block;
		m200_instruction_word *pbi = pb->latest_instruction_word;
		if (pb->output_visit_number + 1 != block->output_visit_number &&
			pb->termination == TERM_KIND_JUMP &&
			pbi->branch &&
			pbi->branch->opcode == M200_JMP) {
			return ESSL_FALSE;
		}
	}
	
	return !(word->end_of_program_marker && (pred == 0 || (pred->branch && pred->branch->opcode == M200_CALL)));
}

void _essl_mali200_remove_empty_instructions(control_flow_graph *cfg) {
	int block_i;
	int cycle = 1;
	for (block_i = (int)cfg->n_blocks-1 ; block_i >= 0 ; block_i--) {
		basic_block *block = cfg->output_sequence[block_i];
		m200_instruction_word *word;
		block->bottom_cycle = cycle;
		for (word = block->latest_instruction_word ; word != 0 ; word = word->predecessor) {
			/* Find predecessor instruction */
			m200_instruction_word *pred = word->predecessor;
			basic_block *pred_block = block;
			while (pred == 0 && pred_block->predecessors && !pred_block->predecessors->next) {
				pred_block = pred_block->predecessors->block;
				pred = pred_block->latest_instruction_word;
			}

			word->cycle = cycle;
			if (instruction_removable(word, block, pred)) {
				if (word->successor) word->successor->predecessor = word->predecessor;
				if (word->predecessor) word->predecessor->successor = word->successor;
				if (word == block->latest_instruction_word) block->latest_instruction_word = word->predecessor;
				if (word == block->earliest_instruction_word) block->earliest_instruction_word = word->successor;
				if (word->end_of_program_marker) {
					assert(pred != NULL); /* cannot happen due to the logic checked in instruction_removable() */
					pred->end_of_program_marker = ESSL_TRUE;
				}
			} else {
				cycle++;
			}
		}
		block->top_cycle = cycle-1;
	}
}

static essl_bool uses_embedded_constants(m200_instruction *inst)
{
	if (inst != 0)
	{
		int i;
		for (i = 0 ; i < M200_MAX_INPUT_ARGS ; i++)
		{
			if (inst->args[i].arg == 0 &&
				(inst->args[i].reg_index == M200_CONSTANT0 ||
				 inst->args[i].reg_index == M200_CONSTANT1))
			{
				return ESSL_TRUE;
			}
		}
	}
	return ESSL_FALSE;
}

memerr _essl_mali200_insert_pad_instruction(mempool *pool, control_flow_graph *cfg, error_context *err)
{
	unsigned int i;
	/* Check for use of embedded constants in the first word */
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *block = cfg->output_sequence[i];
		if(block->earliest_instruction_word != NULL)
		{
			m200_instruction_word *first_word = block->earliest_instruction_word;
			if (uses_embedded_constants(first_word->var) ||
				uses_embedded_constants(first_word->tex) ||
				uses_embedded_constants(first_word->load))
			{
				m200_instruction_word *new_word;
				if (block->top_cycle >= MAX_COMPILER_INSTRUCTION_WORDS)
				{
					REPORT_ERROR(err, ERR_RESOURCES_EXHAUSTED, 0, "Maximum number of compiler supported instructions (%d) exceeded.\n", MAX_COMPILER_INSTRUCTION_WORDS);
					return 0;
				}
				ESSL_CHECK(new_word = _essl_new_mali200_instruction_word(pool, ++block->top_cycle));
				new_word->original_cycle = new_word->cycle;
				new_word->successor = first_word;
				first_word->predecessor = new_word;
				block->earliest_instruction_word = new_word;
			}
			break;
		}
	}
	return MEM_OK;
}


essl_bool _essl_mali200_opcode_is_symmetric(m200_opcode opcode) {
	switch (opcode) {
	case M200_ADD:
	case M200_ADD_X2:
	case M200_ADD_X4:
	case M200_ADD_X8:
	case M200_ADD_D2:
	case M200_ADD_D4:
	case M200_ADD_D8:
	case M200_MUL:
	case M200_MUL_X2:
	case M200_MUL_X4:
	case M200_MUL_X8:
	case M200_MUL_D2:
	case M200_MUL_D4:
	case M200_MUL_D8:
	case M200_AND:
	case M200_OR:
	case M200_XOR:
	case M200_MIN:
	case M200_MAX:
		return ESSL_TRUE;
	default:
		return ESSL_FALSE;
	}
}

essl_bool _essl_mali200_opcode_has_side_effects(m200_opcode opcode) {
	/* The register allocator assumes that any instruction for which
	   this function returns ESSL_TRUE will always have instr_node == 0
	*/
	switch (opcode) {
	case M200_JMP:
	case M200_JMP_REL:
	case M200_CALL:
	case M200_CALL_REL:
	case M200_RET:
	case M200_KILL:
	case M200_ALT_KILL:
	case M200_GLOB_END:
	case M200_ST_STACK:
	case M200_ST_REL:
		return ESSL_TRUE;
	default:
		return ESSL_FALSE;
	}
}


m200_instruction *_essl_mali200_create_slot_instruction(mempool *pool, m200_instruction_word *word,
														m200_schedule_classes *maskp, m200_opcode opcode) {
	m200_instruction *inst;
	m200_instruction **instp = 0;
	m200_schedule_classes sc = 0;
	int rel_subcycle = 0;

	if (*maskp & M200_SC_SKIP_MOV) {
		sc = M200_SC_SKIP_MOV;
		instp = 0;
		rel_subcycle = 0;
	} else if (*maskp & M200_SC_BRANCH) {
		sc = M200_SC_BRANCH;
		instp = &word->branch;
		rel_subcycle = 0;
	} else if (*maskp & M200_SC_STORE) {
		sc = M200_SC_STORE;
		instp = &word->store;
		rel_subcycle = 0;
	} else if (*maskp & M200_SC_LUT) {
		sc = M200_SC_LUT;
		instp = &word->lut;
		rel_subcycle = 0;
	} else if (*maskp & M200_SC_ADD1) {
		sc = M200_SC_ADD1;
		instp = &word->add1;
		rel_subcycle = 1;
	} else if (*maskp & M200_SC_ADD4) {
		sc = M200_SC_ADD4;
		instp = &word->add4;
		rel_subcycle = 1;
	} else if (*maskp & M200_SC_MUL1) {
		sc = M200_SC_MUL1;
		instp = &word->mul1;
		rel_subcycle = 2;
	} else if (*maskp & M200_SC_MUL4) {
		sc = M200_SC_MUL4;
		instp = &word->mul4;
		rel_subcycle = 2;
	} else if (*maskp & M200_SC_TEX) {
		sc = M200_SC_TEX;
		instp = &word->tex;
		rel_subcycle = 3;
	} else if (*maskp & M200_SC_SKIP_VAR) {
		sc = M200_SC_SKIP_VAR;
		instp = 0;
		rel_subcycle = 3;
	} else if (*maskp & M200_SC_VAR) {
		sc = M200_SC_VAR;
		instp = &word->var;
		rel_subcycle = 3;
	} else if (*maskp & M200_SC_SKIP_LOAD) {
		sc = M200_SC_SKIP_LOAD;
		instp = 0;
		rel_subcycle = 3;
	} else if (*maskp & M200_SC_LOAD) {
		sc = M200_SC_LOAD;
		instp = &word->load;
		rel_subcycle = 3;
	} else {
		assert(0); /* Nothing to allocate */
	}

	ESSL_CHECK(inst = _essl_new_mali200_instruction(pool, sc, opcode, CYCLE_TO_SUBCYCLE(word->cycle, rel_subcycle)));
	if (instp != 0) {
		assert(*instp == 0);
		*instp = inst;
	}
	*maskp &= ~sc;
	return inst;
}

static swizzle_pattern *handle_output(mali200_scheduler_context *ctx, m200_instruction *slot, node *out) {
	unsigned i;
	node_extra *info = EXTRA_INFO(out);
	const symbol_list *syms;
	assert(info != 0);
	slot->address_multiplier = info->address_multiplier;
	slot->address_offset = info->address_offset;
	for(syms = info->address_symbols; syms != NULL; syms = syms->next)
	{
		int multiplier = info->address_multiplier;
		if(syms == info->address_symbols) /* first position */
		{
			multiplier = 1;
		}
		ESSL_CHECK(_essl_mali200_add_address_offset_relocation(ctx->relocation_context, M200_RELOC_VARIABLE, syms->sym, ctx->function, multiplier, info->address_multiplier, slot));

	}

	if (info->u.m200_modifiers.trans_node != 0) {
		slot->instr_node = info->u.m200_modifiers.trans_node;
	} else {
		slot->instr_node = out;
	}

	slot->output_swizzle = _essl_create_identity_swizzle_from_swizzle(info->u.m200_modifiers.swizzle);

	for(i = GET_NODE_VEC_SIZE(slot->instr_node); i < M200_NATIVE_VECTOR_SIZE; ++i)
	{
		slot->output_swizzle.indices[i] = -1;
	}

	for(i = 0; i < M200_NATIVE_VECTOR_SIZE; ++i)
	{
		if(slot->output_swizzle.indices[i] == -1)
		{
			info->u.m200_modifiers.swizzle.indices[i] = -1;
		}

	}
	slot->output_mode = (m200_output_mode)info->u.m200_modifiers.mode;
	return &info->u.m200_modifiers.swizzle;
}



essl_bool _essl_mali200_output_modifier_is_identity(m200_output_modifier_set *mods)
{
	return mods->mode == M200_OUTPUT_NORMAL 
		&& mods->exponent_adjust == 0 
		&& _essl_is_identity_swizzle_sized(mods->swizzle, M200_NATIVE_VECTOR_SIZE);

}



essl_bool _essl_mali200_output_modifiers_can_be_coalesced(m200_output_modifier_set *a_mods, m200_output_modifier_set *b_mods)
{
	if(a_mods->mode != M200_OUTPUT_NORMAL || b_mods->mode != M200_OUTPUT_NORMAL) return ESSL_FALSE;
	if(a_mods->exponent_adjust != 0 || b_mods->exponent_adjust != 0) return ESSL_FALSE;
	if(!_essl_is_identity_swizzle(a_mods->swizzle) || !_essl_is_identity_swizzle(b_mods->swizzle)) return ESSL_FALSE;
	return ESSL_TRUE;
}

essl_bool _essl_mali200_can_handle_redirection(node *n)
{

	switch(n->hdr.kind)
	{
	case EXPR_KIND_PHI:
		return ESSL_FALSE;
	case EXPR_KIND_LOAD:
		if(n->expr.u.load_store.address_space == ADDRESS_SPACE_FRAGMENT_VARYING) return ESSL_TRUE;
		return ESSL_FALSE;
	case EXPR_KIND_STORE:
		return ESSL_FALSE;
	case EXPR_KIND_UNARY:
	case EXPR_KIND_BINARY:
	case EXPR_KIND_TERNARY:
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		switch(n->expr.operation)
		{
		case EXPR_OP_FUN_TEXTURE1D:
		case EXPR_OP_FUN_TEXTURE1DPROJ:
		case EXPR_OP_FUN_TEXTURE1DLOD:
		case EXPR_OP_FUN_TEXTURE1DPROJLOD:
		case EXPR_OP_FUN_TEXTURE2D:
		case EXPR_OP_FUN_TEXTURE2DPROJ:
		case EXPR_OP_FUN_TEXTURE2DLOD:
		case EXPR_OP_FUN_TEXTURE2DPROJLOD:
		case EXPR_OP_FUN_TEXTURE2DPROJ_W:
		case EXPR_OP_FUN_TEXTURE2DPROJLOD_W:
		case EXPR_OP_FUN_TEXTURE3D:
		case EXPR_OP_FUN_TEXTURE3DPROJ:
		case EXPR_OP_FUN_TEXTURE3DLOD:
		case EXPR_OP_FUN_TEXTURE3DPROJLOD:
		case EXPR_OP_FUN_TEXTURECUBE:
		case EXPR_OP_FUN_TEXTURECUBELOD:

		case EXPR_OP_FUN_CLAMP: /* clamp does not have a non-modifier scheduler rule, so don't stuff things into it */
			return ESSL_FALSE;
		default:
			return ESSL_TRUE;
		}
	default:
		return ESSL_TRUE;
	}

}

essl_bool _essl_mali200_has_output_modifier_slot(node *n)
{
	if(!_essl_mali200_can_handle_redirection(n)) return ESSL_FALSE;
	switch(n->hdr.kind)
	{
	case EXPR_KIND_CONSTANT:
	case EXPR_KIND_FUNCTION_CALL:
	case EXPR_KIND_BUILTIN_CONSTRUCTOR:
		return ESSL_FALSE;
	case EXPR_KIND_DONT_CARE:
		return ESSL_FALSE;

	case EXPR_KIND_UNARY:
	case EXPR_KIND_BINARY:
	case EXPR_KIND_TERNARY:
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		switch(n->expr.operation)
		{
		case EXPR_OP_SUBVECTOR_ACCESS:
		case EXPR_OP_SUBVECTOR_UPDATE:
		case EXPR_OP_FUN_M200_LD_RGB:
		case EXPR_OP_FUN_M200_LD_ZS:
			return ESSL_FALSE;
		default:
			return ESSL_TRUE;

		}

	default:
		return ESSL_TRUE;


	}
}

essl_bool _essl_mali200_has_output_modifier_and_swizzle_slot(node *n, essl_bool scalar_swizzle)
{
	if(!_essl_mali200_has_output_modifier_slot(n)) return ESSL_FALSE;
	switch(n->hdr.kind)
	{
	case EXPR_KIND_LOAD:
		return ESSL_FALSE;

	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		switch(n->expr.operation)
		{
		case EXPR_OP_FUN_NORMALIZE:
		case EXPR_OP_FUN_M200_ATAN_IT1:
		case EXPR_OP_FUN_M200_POINT:
		case EXPR_OP_FUN_M200_POS:
		case EXPR_OP_FUN_M200_MISC_VAL:
		case EXPR_OP_FUN_M200_HADD_PAIR:
			return ESSL_FALSE;
		case EXPR_OP_FUN_INVERSESQRT:
		case EXPR_OP_FUN_SQRT:
		case EXPR_OP_FUN_EXP:
		case EXPR_OP_FUN_EXP2:
		case EXPR_OP_FUN_LOG:
		case EXPR_OP_FUN_LOG2:
		case EXPR_OP_FUN_SIN:
		case EXPR_OP_FUN_COS:
		case EXPR_OP_FUN_SIN_0_1:
		case EXPR_OP_FUN_COS_0_1:
		case EXPR_OP_FUN_RCC:
		case EXPR_OP_FUN_RCP:
		case EXPR_OP_FUN_M200_ATAN_IT2:
			return scalar_swizzle;
		default:
			return ESSL_TRUE;
		}
	default:
		return ESSL_TRUE;
	}
}

essl_bool _essl_mali200_has_output_modifier_and_truncsat_slot(node *n)
{
	if(!_essl_mali200_has_output_modifier_slot(n)) return ESSL_FALSE;
	switch(n->hdr.kind)
	{
	case EXPR_KIND_LOAD:
		return ESSL_FALSE;
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		switch(n->expr.operation)
		{
		case EXPR_OP_FUN_M200_ATAN_IT2:
			return ESSL_FALSE;
		default:
			return ESSL_TRUE;
		}
	default:
		return ESSL_TRUE;
	}
}



static essl_bool _essl_mali200_has_redirected_output_or_output_modifiers(node *n)
{
	node_extra *ex;
	ex = EXTRA_INFO(n);
	if(!ex) return ESSL_FALSE;
	if((ex->u.m200_modifiers.trans_node != NULL && ex->u.m200_modifiers.trans_node != n) || !_essl_mali200_output_modifier_is_identity(&ex->u.m200_modifiers))
	{
		return ESSL_TRUE;
	}
	return ESSL_FALSE;
}

#define NO_ACCEPT_ABSNEG ESSL_FALSE
#define ACCEPT_ABSNEG ESSL_TRUE
#define NO_ACCEPT_SWIZZLE ESSL_FALSE
#define ACCEPT_SWIZZLE ESSL_TRUE

static memerr handle_input_helper(mali200_scheduler_context *ctx,  m200_instruction_word *word, 
								  m200_input_argument *arg,  /*@null@*/ node *in,
								  /*@null@*/ swizzle_pattern *output_swizzle, int subcycle, /*@null@*/ node *no_schedule_node,
								  essl_bool accept_absneg, essl_bool accept_swizzle, essl_bool accept_scalar_swizzle)
{
	size_t i;
	size_t swz_size = M200_NATIVE_VECTOR_SIZE;
	size_t vec_size = 0;
	arg->arg = in;
	arg->reg_index = M200_REG_UNKNOWN;
	arg->absolute_value = 0;
	arg->negate = 0;


	if(in) swz_size = GET_NODE_VEC_SIZE(in);

	for(i = 0; i < M200_NATIVE_VECTOR_SIZE; ++i)
	{
		int idx = (int)i;
		if(output_swizzle) 
		{
			idx = output_swizzle->indices[idx];
		} else {
			idx = (i == 0) ? 0 : -1; /* force scalar argument */
		}
		if(idx >= 0)
		{
			idx = ESSL_MIN(idx, (int)swz_size-1);
			++vec_size;
		}
		arg->swizzle.indices[i] = idx;
	}
	if(arg->arg == 0) return MEM_OK; /* if no real operand, don't try to recurse on it */
	/* all properly initialized, we can now commence eating input operations */
	if(vec_size == 1) accept_swizzle = accept_scalar_swizzle; /* can always eat swizzles for scalar ops */
	for(;;)
	{
		if(_essl_mali200_has_redirected_output_or_output_modifiers(arg->arg) 
		   && arg->arg != no_schedule_node) break;
		if(accept_swizzle && arg->arg->hdr.kind == EXPR_KIND_UNARY 
		   && arg->arg->expr.operation == EXPR_OP_SWIZZLE)
		{
			arg->swizzle = _essl_combine_swizzles(arg->swizzle, arg->arg->expr.u.swizzle);
		}
		else if(accept_absneg && arg->arg->hdr.kind == EXPR_KIND_UNARY 
				&& arg->arg->expr.operation == EXPR_OP_NEGATE)
		{
			/* negates are cancelled out by absolute values, handle this */
			if(!arg->absolute_value) 
			{
				arg->negate = !arg->negate;
			}
		} else if(accept_absneg && arg->arg->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL 
				  && arg->arg->expr.operation == EXPR_OP_FUN_ABS)
		{
			arg->absolute_value = 1;
		} else {
			break;
		}
		/* eat the node we just processed */
		if(arg->arg != no_schedule_node)
		{
			ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, &arg->arg, subcycle));
		}
		ESSL_CHECK(arg->arg = GET_CHILD(arg->arg, 0));
	}
	if(accept_swizzle && arg->arg->hdr.kind == EXPR_KIND_CONSTANT)
	{
		if(SUBCYCLE_TO_RELATIVE_SUBCYCLE(subcycle) != 3)
		{
			/* yay, we'll try to fit the constant into the constant registers */
			swizzle_pattern swz;
			int res_reg;
			if(_essl_mali200_fit_constants(word, ctx->desc, arg->arg, &swz, &res_reg))
			{
				arg->reg_index = res_reg;
				
				arg->swizzle = _essl_combine_swizzles(arg->swizzle, swz);
				if(arg->arg != no_schedule_node)
				{
					ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, &arg->arg, subcycle));
				}
				arg->arg = 0;
			}
		} else {
			/* if we're in subcycle 3, reading the embedded constants will yield all zeros. Exploit this! */
			essl_bool all_zero = ESSL_TRUE;
			unsigned i;
			union {
				int i;
				float f;
			} v;
			for(i = 0; i < _essl_get_type_size(arg->arg->hdr.type); ++i)
			{
				v.f = ctx->desc->scalar_to_float(arg->arg->expr.u.value[i]);
				if(v.i != 0)
				{
					all_zero = ESSL_FALSE;
					break;
				}
			}
			if(all_zero)
			{
				swizzle_pattern ws = _essl_create_undef_swizzle();
				ws.indices[0] = ws.indices[1] = ws.indices[2] = ws.indices[3] = 2;
				arg->reg_index = M200_CONSTANT1;
				arg->swizzle = _essl_combine_swizzles(arg->swizzle, ws);

				if(arg->arg != no_schedule_node)
				{
					ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, &arg->arg, subcycle));
				}
				arg->arg = NULL;

			}

		}
	}

	return MEM_OK;
}


static memerr handle_input(mali200_scheduler_context *ctx,  m200_instruction_word *word, 
						   m200_instruction *inst,  int arg_idx, /*@null@*/ node *in, 
						   /*@null@*/ swizzle_pattern *output_swizzle, /*@null@*/ node *no_schedule_node)
{
	essl_bool accept_absneg = ESSL_FALSE;
	essl_bool accept_swizzle = ESSL_FALSE;
	essl_bool accept_scalar_swizzle = ESSL_TRUE;
	essl_bool accept_truncsat = ESSL_FALSE;
	essl_bool accept_mask = ESSL_FALSE;
	switch(inst->schedule_class)
	{
	case M200_SC_VAR:
		accept_truncsat = ESSL_TRUE;
		accept_mask = ESSL_TRUE;
		accept_absneg = ESSL_TRUE;
		accept_swizzle = ESSL_TRUE;
		break;

	case M200_SC_MUL4:
	case M200_SC_MUL1:
	case M200_SC_ADD4:
	case M200_SC_ADD1:
		accept_absneg = ESSL_TRUE;
		accept_swizzle = ESSL_TRUE;
		accept_truncsat = ESSL_TRUE;
		accept_mask = ESSL_TRUE;
		if(inst->opcode == M200_DERX || inst->opcode == M200_DERY)
		{
			accept_absneg = ESSL_FALSE;
			accept_swizzle = ESSL_FALSE;
			accept_scalar_swizzle = ESSL_FALSE;
		}
		break;
	case M200_SC_LUT:
		accept_mask = ESSL_TRUE;
		/* opcode conditional */
		switch(inst->opcode)
		{
		case M200_RCP:                     /* 1/x */
		case M200_RCC:                     /* 1/x, but not 0 or inf */
		case M200_RSQ:                     /* 1/sqrt(x) */
		case M200_SQRT:                    /* sqrt(x) */
		case M200_EXP:                     /* 2^x */
		case M200_LOG:                     /* log2(x) */
		case M200_SIN:                     /* sin 2*pi*x */
		case M200_COS:                     /* cos 2*pi*x */
			accept_absneg = ESSL_TRUE;
			accept_swizzle = ESSL_TRUE;
			accept_truncsat = ESSL_TRUE;
			break;
		case M200_ATAN1_IT1:               /* 1st iteration of 1-argument atan */
		case M200_ATAN2_IT1:               /* 1st iteration of 2-argument atan */ 
			accept_absneg = ESSL_TRUE;
			accept_swizzle = ESSL_TRUE;
			break;

		case M200_ATAN_IT2:                /* 2nd iteration of atan, mode 2 */
			accept_swizzle = ESSL_TRUE;
			accept_truncsat = ESSL_TRUE;
			break;

		case M200_MUL:
			accept_swizzle = ESSL_TRUE;
			if(arg_idx == 0)
			{
				accept_absneg = ESSL_TRUE;
			}
			break;
		case M200_MOV:
			accept_swizzle = ESSL_TRUE;
			break;


		default:
			assert(0);
			break;

		}
		break;


	case M200_SC_TEX:
	case M200_SC_LOAD:
	case M200_SC_BRANCH:
		accept_absneg = ESSL_FALSE;
		accept_swizzle = ESSL_FALSE;
		accept_truncsat = ESSL_FALSE;
		accept_mask = ESSL_FALSE;
		break;

	case M200_SC_STORE:
		accept_absneg = ESSL_FALSE;
		accept_swizzle = ESSL_FALSE;
		if (ctx->desc->options->mali200_store_workaround) {
			/* Workaround for bugzilla 3592 */
			accept_scalar_swizzle = arg_idx != 1; /* Swizzle not allowed for data input */
		}
		accept_truncsat = ESSL_FALSE;
		accept_mask = ESSL_FALSE;
		break;

	case M200_SC_SKIP_MOV:
	case M200_SC_SKIP_LOAD:
	case M200_SC_SKIP_VAR:
		/* skip this. just pretend it's okay */
		return MEM_OK;

	default:
		assert(0);
		break;

	}
	assert(accept_truncsat || inst->output_mode == M200_OUTPUT_NORMAL);
	accept_truncsat = accept_truncsat;
	accept_mask = accept_mask;
	return handle_input_helper(ctx,  word, &inst->args[arg_idx], in, output_swizzle, inst->subcycle, no_schedule_node, accept_absneg, accept_swizzle, accept_scalar_swizzle);

}

memerr _essl_mali200_write_instructions(mali200_scheduler_context *ctx, 
				  m200_instruction_word *word, m200_schedule_classes inst_mask, va_list args) {
	m200_instruction *slot;
	size_t i;
	essl_bool have_scheduled_operation = ESSL_FALSE;
	/* Assert that the wanted slots are available */
	assert((inst_mask & M200_SC_CONCRETE_MASK & word->used_slots) == 0);

	/* mark the slots as used */
	word->used_slots |= inst_mask & (M200_SC_CONCRETE_MASK | M200_SC_RENDEZVOUS | M200_SC_NO_RENDEZVOUS);
	assert((word->used_slots & (M200_SC_RENDEZVOUS | M200_SC_NO_RENDEZVOUS)) != (M200_SC_RENDEZVOUS | M200_SC_NO_RENDEZVOUS));

	while ((inst_mask & M200_SC_ALLOC_MASK) != 0) {
		m200_opcode opcode = va_arg(args, m200_opcode);
		node **out_ptr = va_arg(args, node **);
		node *out = out_ptr != NULL ? *out_ptr : NULL;
		int in_reg;
		node *tmp = NULL;
		swizzle_pattern *output_swizzle = NULL;
		ESSL_CHECK(slot = _essl_mali200_create_slot_instruction(ctx->pool, word, &inst_mask, opcode));
		if (out)
		{
			ESSL_CHECK(output_swizzle = handle_output(ctx, slot, out));
			if (inst_mask & M200_SC_DONT_SCHEDULE_MOV &&
				(opcode == M200_MOV
				 || opcode == M200_REG_MOV
				 || opcode == M200_CONST_MOV
				 || opcode == M200_MOV_DIV_Y
				 || opcode == M200_MOV_DIV_Z
				 || opcode == M200_MOV_DIV_W
				 || opcode == M200_MOV_CUBE))
			{

			} else {
				if(!have_scheduled_operation)
				{
					ESSL_CHECK(_essl_scheduler_schedule_operation(ctx->sctx, out, slot->subcycle));
					have_scheduled_operation = ESSL_TRUE;
				} else {
					ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, out_ptr, slot->subcycle));
					out = *out_ptr;
					slot->instr_node = out;
				}
			}
			if(slot->schedule_class == M200_SC_SKIP_VAR)
			{
				assert(word->var != NULL && word->var->instr_node != NULL);
				_essl_rewrite_node_to_transfer(slot->instr_node, word->var->instr_node);
			} else if(slot->schedule_class == M200_SC_SKIP_LOAD)
			{
				assert(word->load != NULL && word->load->instr_node != NULL);
				_essl_rewrite_node_to_transfer(slot->instr_node, word->load->instr_node);
			}
		}
		switch (opcode) {
		case M200_REG_MOV:
			/* Register move instruction. Takes just one input */
			in_reg = va_arg(args, int);
			slot->opcode = M200_MOV;
			/* Write move */
			ESSL_CHECK(handle_input(ctx, word, slot, 0, NULL, output_swizzle, out));
			slot->args[0].reg_index = in_reg;
			break;

		case M200_CONST_MOV:
			/* constant move. zero inputs, as the output is the input */
			slot->opcode = M200_MOV;
			ESSL_CHECK(handle_input(ctx, word, slot, 0, out, output_swizzle, out));
			break;
		case M200_ADD:
		case M200_ADD_X2:
		case M200_ADD_X4:
		case M200_ADD_X8:
		case M200_ADD_D2:
		case M200_ADD_D4:
		case M200_ADD_D8:
		case M200_MUL:
		case M200_MUL_X2:
		case M200_MUL_X4:
		case M200_MUL_X8:
		case M200_MUL_D2:
		case M200_MUL_D4:
		case M200_MUL_D8:
		case M200_MUL_W1:
		case M200_AND:
		case M200_OR:
		case M200_XOR:
		case M200_MIN:
		case M200_MAX:
		case M200_SEL:
		case M200_VSEL:
			/* Ordinary binary operation. Takes just two inputs */
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, output_swizzle, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, output_swizzle, out));

			break;

		case M200_ATAN2_IT1: 
		    {
				swizzle_pattern swz = _essl_create_scalar_swizzle(0);
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &swz, out));
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, &swz, out));
			}
			break;

#if 0 /* Constructor instructions never generated */
		case M200_CONS31:
		    {
				swizzle_pattern first_swizzle  = _essl_create_identity_swizzle(3);
				swizzle_pattern second_swizzle = _essl_create_undef_swizzle();
				second_swizzle.indices[3] = 0;
				for(i = 0; i < M200_NATIVE_VECTOR_SIZE; ++i)
				{
					if(slot->output_mask.mask[i] != 1)
					{
						first_swizzle.indices[i] = -1;
						second_swizzle.indices[i] = -1;
					}
				}
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &first_swizzle, out));
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, &second_swizzle, out));
			}
			break;

		case M200_CONS22:
		    {
				swizzle_pattern first_swizzle  = { { 0,  1, -1, -1}, M200_NATIVE_VECTOR_SIZE};
				swizzle_pattern second_swizzle = { {-1, -1,  0,  1}, M200_NATIVE_VECTOR_SIZE};
				for(i = 0; i < M200_NATIVE_VECTOR_SIZE; ++i)
				{
					if(slot->output_mask.mask[i] != 1)
					{
						first_swizzle.indices[i] = -1;
						second_swizzle.indices[i] = -1;
					}
				}
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &first_swizzle, out));
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, &second_swizzle, out));
			}
			break;
		case M200_CONS13:
		    {
				swizzle_pattern first_swizzle  = { { 0, -1, -1, -1}, M200_NATIVE_VECTOR_SIZE};
				swizzle_pattern second_swizzle = { {-1,  0,  1,  2}, M200_NATIVE_VECTOR_SIZE};
				for(i = 0; i < M200_NATIVE_VECTOR_SIZE; ++i)
				{
					if(slot->output_mask.mask[i] != 1)
					{
						first_swizzle.indices[i] = -1;
						second_swizzle.indices[i] = -1;
					}
				}
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &first_swizzle, out));
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, &second_swizzle, out));
			}
			break;
#endif

		case M200_DERX:
		case M200_DERY:
		case M200_FRAC:
		case M200_FLOOR:
		case M200_CEIL:
		case M200_NORM3:
		case M200_NOT:
			/* Ordinary unary operation. Takes just one input */
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, output_swizzle, out));
			break;

		case M200_RCP:
		case M200_RCC:
		case M200_RSQ:
		case M200_SQRT:
		case M200_EXP:
		case M200_LOG:
		case M200_SIN:
		case M200_COS:
			/* Unary operation with special input. */
		    {
				swizzle_pattern swz = _essl_create_identity_swizzle(1);
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &swz, out));
			}
			break;


		case M200_ATAN_IT2:
		    {
				swizzle_pattern swz = _essl_create_identity_swizzle(3);
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &swz, out));
			}
			break;


		case M200_ATAN1_IT1:
		    {
				swizzle_pattern swz = _essl_create_identity_swizzle(1);
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &swz, out));
			}
			break;


			
		case M200_MOV_DIV_Y:
		case M200_MOV_DIV_Z:
		case M200_MOV_DIV_W:
		case M200_MOV_CUBE:
		case M200_MOV:
			/* Ordinary unary operation. Takes one input */
			tmp = va_arg(args, node *);
			{
				swizzle_pattern swz;
				if(out == 0)
				{
					slot->out_reg = M200_R_IMPLICIT;
					output_swizzle = &swz;
					*output_swizzle = _essl_create_identity_swizzle(GET_NODE_VEC_SIZE(tmp));
				}

				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, output_swizzle, out));
			}
			break;

		case M200_HADD3:
		    {
				swizzle_pattern first_swizzle  = _essl_create_identity_swizzle(3);
				
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &first_swizzle, out));
				

			}
			break;

		case M200_HADD4:
		    {
				swizzle_pattern first_swizzle  = _essl_create_identity_swizzle(4);
				
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &first_swizzle, out));
				

			}
			break;

		case M200_2X2ADD:
		    {
				swizzle_pattern first_swizzle  = _essl_create_identity_swizzle(4);
				swizzle_pattern second_swizzle  = _essl_create_identity_swizzle(2);
				
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, &first_swizzle, out));
				tmp = va_arg(args, node *);
				ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, &second_swizzle, out));
			}
			break;

		case M200_CMP:
			slot->compare_function = va_arg(args, m200_comparison);
			slot->opcode_flags = va_arg(args, unsigned);
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, output_swizzle, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, output_swizzle, out));
			if(out == 0  || slot->opcode_flags & M200_ALU_DONT_WRITE_REG_FILE)
			{
				slot->opcode_flags &= ~M200_ALU_DONT_WRITE_REG_FILE;
				slot->out_reg = M200_R_IMPLICIT;
				slot->instr_node = 0;
			}
			break;

		case M200_TEX:
			slot->opcode_flags = va_arg(args, unsigned);
			
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, NULL, out));
			slot->lod_offset = (float)va_arg(args, double);
			slot->out_reg = M200_HASH_TEX;
			slot->output_swizzle = *output_swizzle;
			break;


		case M200_LD_UNIFORM:
		case M200_LD_STACK:
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			
			break;

		case M200_LD_REL:
			slot->out_reg = M200_HASH_LOAD;

			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));

			/* skip arg 1 */

			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 2, tmp, NULL, out));

			break;

		case M200_LD_SUBREG:
			tmp = va_arg(args, node *);
			/* OPT: handle constants by stuffing them into the address offset */
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			tmp = va_arg(args, node *);
		    {
				swizzle_pattern input_swizzle  = _essl_create_identity_swizzle(4);
				for(i = GET_NODE_VEC_SIZE(tmp); i < M200_NATIVE_VECTOR_SIZE; ++i)
				{
					input_swizzle.indices[i] = -1;
				}
				ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, &input_swizzle, out));
				slot->address_multiplier = 1;
			}
			break;

		case M200_VAR:
		case M200_VAR_DIV_Y:
		case M200_VAR_DIV_Z:
		case M200_VAR_DIV_W:
		case M200_VAR_CUBE:
			slot->opcode_flags = va_arg(args, unsigned);
			
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			break;


		case M200_ST_STACK:
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, output_swizzle, out));
			slot->instr_node = 0; /* no result from a store */
			break;

		case M200_ST_REL:
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, output_swizzle, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 2, tmp, NULL, out));		
			slot->instr_node = 0; /* no result from a store */
			break;

		case M200_ST_SUBREG:
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			/* Argument 1 is implicitly the same as the output */
			ESSL_CHECK(handle_input(ctx, word, slot, 1, slot->instr_node, output_swizzle, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 2, tmp, NULL, out));
			/* OPT: handle constants by stuffing them into the address offset */
			slot->address_multiplier = 1;
			break;

#if 0 /* No function call support */
		case M200_CALL:
			sym = va_arg(args, symbol *);
			slot->compare_function = M200_CMP_ALWAYS;
			slot->address_multiplier = M200_NATIVE_VECTOR_SIZE;
			ESSL_CHECK(_essl_mali200_add_address_offset_relocation(ctx->relocation_context, 
								                          M200_RELOC_FUNCTION_STACK_SIZE, sym, 
													      sym, M200_NATIVE_VECTOR_SIZE, slot));
			if(sym->type->basic_type == TYPE_VOID)
			{
				slot->instr_node = 0;
			} else {
				slot->out_reg = M200_R0;
			}
			slot->call_target = sym;
			/* flag relocations of the parameter symbols */
			{
				parameter *arg;
				single_declarator *parm;
				for(arg = out->expr.u.fun.arguments, parm = sym->parameters; 
					arg != 0 && parm != 0;  arg = arg->next, parm = parm->next)
				{
					parm->sym->address = 0;
					ESSL_CHECK(_essl_mali200_add_symbol_address_relocation(ctx->relocation_context, 
											M200_RELOC_VARIABLE, parm->sym, sym, 1, arg->sym));
					ESSL_CHECK(_essl_mali200_add_symbol_address_relocation(ctx->relocation_context, 
										    M200_RELOC_FUNCTION_STACK_SIZE, sym, 
											sym, 1, arg->sym));
				}
			}
			break;
#endif

		case M200_NOP:
			break;

		case M200_JMP:
			slot->jump_target = va_arg(args, basic_block *);
			slot->compare_function = va_arg(args, m200_comparison);
			slot->address_multiplier = 1;
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, NULL, out));
			slot->instr_node = 0;
			break;

		case M200_KILL:
		case M200_GLOB_END:
			slot->compare_function = va_arg(args, m200_comparison);
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, NULL, out));
			slot->instr_node = 0;
			break;

		case M200_ALT_KILL:
			slot->compare_function = M200_CMP_ALWAYS;
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 2, tmp, NULL, out));
			slot->instr_node = 0;
			break;

#if 0 /* No function call support */
		case M200_RET:
			slot->compare_function = va_arg(args, m200_comparison);
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 0, tmp, NULL, out));
			tmp = va_arg(args, node *);
			ESSL_CHECK(handle_input(ctx, word, slot, 1, tmp, NULL, out));
			slot->instr_node = 0;
			slot->address_multiplier = 4;
			slot->address_offset = 0;
			ESSL_CHECK(_essl_mali200_add_address_offset_relocation(ctx->relocation_context, 
								               M200_RELOC_FUNCTION_STACK_SIZE, ctx->function, 
								               ctx->function, M200_NATIVE_VECTOR_SIZE, slot));
			break;

		case M200_LOAD_RET:
			sym = va_arg(args, symbol *);
			{
				combine_pattern all_on = {{1, 1, 1, 1}, 4};
				assert(sym == ctx->function);
				slot->address_multiplier = 4;
				slot->address_offset = 0;
				slot->opcode = M200_LD_STACK;
				slot->instr_node = 0;
				slot->out_reg = M200_HASH_LOAD;
				slot->output_mask = all_on;
				ESSL_CHECK(_essl_mali200_add_address_offset_relocation(ctx->relocation_context, M200_RELOC_FUNCTION_STACK_SIZE, sym, ctx->function, -M200_NATIVE_VECTOR_SIZE, slot));
			}
			break;
#endif

		case M200_POS:
		case M200_POINT:		
		case M200_LD_RGB:
		case M200_LD_ZS:
		case M200_MISC_VAL:
			/* nullary operations */
			break;
			


		default:
			assert(0); /* Not supported yet */
		}
	}

	return MEM_OK;

}
