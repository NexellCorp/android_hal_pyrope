/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "shadergen_maligp2/select_pieces.h"
#include "shadergen_maligp2/glue_pieces.h"
#include "shadergen_state.h"
#include "common/output_buffer.h"
#include "common/essl_profiling.h"

unsigned *_vertex_shadergen_generate_shader(const struct vertex_shadergen_state* state, unsigned *size_out,
											alloc_func alloc, free_func free) {
	piece_select pieces[MAX_SELECTED_PIECES];
	unsigned n_pieces;
	unsigned *data = NULL;
	essl_bool res;
	TIME_PROFILE_START("_total");

	TIME_PROFILE_START("select_pieces");
	res = _vertex_shadergen_select_pieces(state, pieces, &n_pieces);
	TIME_PROFILE_STOP("select_pieces");
	if(res) 
	{
		
		TIME_PROFILE_START("glue_pieces");
		data = _vertex_shadergen_glue_pieces(pieces, n_pieces, size_out, alloc, free);
		TIME_PROFILE_STOP("glue_pieces");
		if(data != 0)
		{
			TIME_PROFILE_START("byteswap");
			_essl_buffer_native_to_le_byteswap(data, *size_out/sizeof(u32));
			TIME_PROFILE_STOP("byteswap");
		}
	}
	TIME_PROFILE_STOP("_total");
	return data;
}
