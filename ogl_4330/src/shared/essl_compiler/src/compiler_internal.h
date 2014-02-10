/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMPILER_INTERNAL_H
#define COMPILER_INTERNAL_H

#include "compiler.h"
#include "common/essl_target.h"

compiler_context *_essl_new_compiler_for_target(target_descriptor *desc, const char *concatenated_input_string, const int *source_string_lengths, unsigned int n_source_strings, alloc_func alloc, free_func free);

#endif
