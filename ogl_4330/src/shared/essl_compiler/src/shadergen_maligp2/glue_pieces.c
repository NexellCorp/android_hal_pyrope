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
#include "shadergen_maligp2/select_pieces.h"
#include "shadergen_maligp2/shader_pieces.h"


unsigned *_vertex_shadergen_glue_pieces(const piece_select *pieces, unsigned n_pieces, unsigned *size_out,
										alloc_func alloc, free_func free) {
	unsigned n_instructions = 0;
	const unsigned *serialized_data;
	unsigned n_serialized_data_words;
	unsigned *data;
	unsigned size;
	instruction *instruction_ptr;
	unsigned i;
	unsigned j;
	unsigned k;
	IGNORE_PARAM(free);
	for (i = 0 ; i < n_pieces ; i++) {
		n_instructions += pieces[i].piece->len;
		if (pieces[i].merge_fields) {
			n_instructions -= 1;
		}
	}

	/* Allocate memory */
	serialized_data = _piecegen_get_serialized_data(&n_serialized_data_words);
	size = n_serialized_data_words*sizeof(unsigned) + n_instructions*sizeof(instruction);
	data = alloc(size);
	if (!data) return 0;

	/* Copy header */
	memcpy(data, serialized_data, n_serialized_data_words*sizeof(unsigned));
	/* Offset 1 and 3 contain the sizes of the data after these, respectively */
	data[1] = size-8;
	data[3] = size-16;
	data[n_serialized_data_words-1] = n_instructions*sizeof(instruction);

	/* Put instructions */
	instruction_ptr = (instruction *)&data[n_serialized_data_words];
	j = 0;
	for (i = 0 ; i < n_pieces ; i++) {
		unsigned n = pieces[i].piece->len;
		k = 0;
		if (pieces[i].merge_fields) {
			assert(j > 0);
			_shadergen_maligp2_merge_instructions(&instruction_ptr[j-1], pieces[i].piece->ptr, pieces[i].merge_fields);
			k = 1;
		}
		for (; k < n ; k++) {
			assert(j < n_instructions);
			instruction_ptr[j] = pieces[i].piece->ptr[k];
			_shadergen_maligp2_correct_flow_address(&instruction_ptr[j], j);
			j++;
		}
	}
	assert(j == n_instructions);

	*size_out = size;
	return data;
}
