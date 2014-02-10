/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef SHADERGEN_MALIGP2_SELECT_PIECES_H
#define SHADERGEN_MALIGP2_SELECT_PIECES_H

#include "common/essl_common.h"
#include "shadergen_state.h"
#include "shadergen_maligp2/shader_pieces.h"
#include "shadergen_maligp2/shadergen_maligp2_instruction.h"

#define MAX_SELECTED_PIECES 42

typedef struct {
	instruction_mask_flag merge_fields;
	const piece *piece;
} piece_select;

essl_bool _vertex_shadergen_select_pieces(const struct vertex_shadergen_state* state, piece_select *out, unsigned *n_pieces);

#endif
