/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef SHADERGEN_MALIGP2_GLUE_PIECES_H
#define SHADERGEN_MALIGP2_GLUE_PIECES_H

#include "shadergen_maligp2/select_pieces.h"

unsigned *_vertex_shadergen_glue_pieces(const piece_select *pieces, unsigned n_pieces, unsigned *size_out,
										alloc_func alloc, free_func free);

#endif
