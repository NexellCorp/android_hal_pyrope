/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "backend/instruction.h"


void *_essl_instruction_word_at_cycle(mempool *pool, control_flow_graph *cfg, int cycle, basic_block **block_out)
{
	if (cycle > cfg->n_instructions ||
		((instruction_word *)cfg->instruction_sequence[cycle].word)->cycle != cycle)
	{
		/* Cache not valid. Regenerate it. */
		unsigned i;
		instruction_word *first_word = 0;
		for(i = 0; i < cfg->n_blocks; ++i)
		{
			basic_block *block = cfg->output_sequence[i];
			first_word = block->earliest_instruction_word;
			if (first_word) break;
		}
		assert(first_word);

		cfg->n_instructions = first_word->cycle;
		cfg->instruction_sequence = _essl_mempool_alloc(pool, (cfg->n_instructions+1)*sizeof(word_block));
		if (cfg->instruction_sequence == 0)
		{
			cfg->n_instructions = 0;
		}
		for(i = 0; i < cfg->n_blocks; ++i)
		{
			basic_block *block = cfg->output_sequence[i];
			instruction_word *word;
			for (word = block->earliest_instruction_word ; word != 0 ; word = word->successor)
			{
				if (cfg->instruction_sequence != 0) {
					assert(word->cycle <= cfg->n_instructions);
					assert(cfg->instruction_sequence[word->cycle].word == 0);
					cfg->instruction_sequence[word->cycle].word = word;
					cfg->instruction_sequence[word->cycle].block = block;
				} else if (word->cycle == cycle) {
					*block_out = block;
					return word;
				}
			}
		}
	}
	assert(cfg->instruction_sequence != 0);

	/* Cache for instruction is valid */
	*block_out = cfg->instruction_sequence[cycle].block;
	return cfg->instruction_sequence[cycle].word;
}
