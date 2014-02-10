/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BACKEND_STATIC_CYCLE_COUNT_H
#define BACKEND_STATIC_CYCLE_COUNT_H

#include "common/translation_unit.h"
#include "common/basic_block.h"
#include "common/essl_target.h"


typedef struct {
	int min_number_of_cycles;
	int max_number_of_cycles;
	int n_instruction_words;
	essl_bool max_cycles_unknown;
} static_cycle_counts;

memerr _essl_calc_static_cycle_counts(mempool *pool, translation_unit *tu, /*@out@*/ static_cycle_counts *result);


#endif /* BACKEND_STATIC_CYCLE_COUNT_H */
