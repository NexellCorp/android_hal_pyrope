/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _DUMP_FRAG_SHADER_INFO_H_
#define _DUMP_FRAG_SHADER_INFO_H_

#include "common/essl_system.h"
#include "shadergen_state.h"

#include <shared/binary_shader/bs_loader.h>

void dump_frag_shader_info(const fragment_shadergen_state *state, struct bs_shader* shader);

#endif
