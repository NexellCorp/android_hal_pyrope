/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "backend/static_cycle_count.h"


typedef struct 
{
	int min_number_of_cycles;
	int max_number_of_cycles;
} basic_block_entry;

typedef struct {
	int min_number_of_cycles;
	int max_number_of_cycles;
	int n_instruction_words;
	essl_bool max_cycles_unknown;
	basic_block_entry *entries;
	cycles_for_jump_fun cycles_for_jump;
	cycles_for_block_fun cycles_for_block;
} cycle_count_context;


static void update_entry(basic_block_entry *entry, int cycles, basic_block_entry *dest_entry)
{
	entry->max_number_of_cycles = ESSL_MAX(entry->max_number_of_cycles, cycles + dest_entry->max_number_of_cycles);
	entry->min_number_of_cycles = ESSL_MIN(entry->min_number_of_cycles, cycles + dest_entry->min_number_of_cycles);
}

static void visit(cycle_count_context *ctx, basic_block *b)
{
	int cycles_for_this_block = ctx->cycles_for_block(b);
	basic_block_entry *entry = &ctx->entries[b->output_visit_number];
	entry->max_number_of_cycles = 0;
	entry->min_number_of_cycles = 999999999;
	switch(b->termination)
	{
	case TERM_KIND_UNKNOWN:
		assert(0);
		break;
	case TERM_KIND_JUMP:
		if(b->source != NULL)
		{
			if(b->successors[BLOCK_TRUE_TARGET]->output_visit_number <= b->output_visit_number)
			{
				ctx->max_cycles_unknown = ESSL_TRUE;
			} else {
				update_entry(entry, cycles_for_this_block + ctx->cycles_for_jump(JUMP_TAKEN), &ctx->entries[b->successors[BLOCK_TRUE_TARGET]->output_visit_number]);
			}
			update_entry(entry, cycles_for_this_block + ctx->cycles_for_jump(JUMP_NOT_TAKEN), &ctx->entries[b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number]);
		} else {
			if(b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number <= b->output_visit_number)
			{
				ctx->max_cycles_unknown = ESSL_TRUE;
			} else if(b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number == b->output_visit_number + 1)
			{
				/* no jump used here */
				update_entry(entry, cycles_for_this_block, &ctx->entries[b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number]);

			} else {
				/* jump used */
				update_entry(entry, cycles_for_this_block + ctx->cycles_for_jump(JUMP_TAKEN), &ctx->entries[b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number]);
			}
			
		}
		break;
	case TERM_KIND_DISCARD:
		update_entry(entry, cycles_for_this_block, &ctx->entries[b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number]);
		/* fall-through */
	case TERM_KIND_EXIT:
		entry->min_number_of_cycles = cycles_for_this_block;
		entry->max_number_of_cycles = cycles_for_this_block;
		break;
	}

}

memerr _essl_calc_static_cycle_counts(mempool *pool, translation_unit *tu, /*@out@*/ static_cycle_counts *result)
{
	cycle_count_context context, *ctx = &context;
	unsigned n_blocks = tu->entry_point->control_flow_graph->n_blocks;
	ctx->min_number_of_cycles = 999999999;
	ctx->max_number_of_cycles = 0;
	ctx->n_instruction_words = 0;
	ctx->max_cycles_unknown = 0;
	ctx->cycles_for_jump = tu->desc->cycles_for_jump;
	ctx->cycles_for_block = tu->desc->cycles_for_block;
	if(ctx->cycles_for_jump == NULL || ctx->cycles_for_block == NULL)
	{
		result->n_instruction_words = -1;
		result->min_number_of_cycles = -1;
		result->max_number_of_cycles = -1;
		result->max_cycles_unknown = ESSL_TRUE;
		return MEM_OK;
	}
	if(n_blocks == 0)
	{
		result->n_instruction_words = 0;
		result->min_number_of_cycles = 0;
		result->max_number_of_cycles = 0;
		result->max_cycles_unknown = ESSL_FALSE;
		return MEM_OK;
	}

	ESSL_CHECK(ctx->entries = _essl_mempool_alloc(pool, n_blocks*sizeof(*ctx->entries)));


	{
		int i;
		basic_block **blocks = tu->entry_point->control_flow_graph->output_sequence;
		for(i = n_blocks-1; i >= 0; --i)
		{
			visit(ctx, blocks[i]);
			ctx->n_instruction_words += ctx->cycles_for_block(blocks[i]);
		}

	}

	result->min_number_of_cycles = ctx->entries[0].min_number_of_cycles;
	result->max_number_of_cycles = ctx->entries[0].max_number_of_cycles;
	result->n_instruction_words = ctx->n_instruction_words;
	result->max_cycles_unknown = ctx->max_cycles_unknown;
	if(ctx->max_cycles_unknown)
	{
		result->max_number_of_cycles = -1;
	}
	return MEM_OK;
}
