/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_COMPILER_OPTIONS_H
#define COMMON_COMPILER_OPTIONS_H

#include "common/essl_system.h"
#include "common/essl_mem.h"
#include "compiler.h"


typedef struct _tag_compiler_options
{
	unsigned int hw_rev;
#define COMPILER_OPTION(name,field,type,defval) type field;
#include "common/compiler_option_definitions.h"
#undef COMPILER_OPTION
} compiler_options;


/*@null@*/ compiler_options *_essl_new_compiler_options(mempool *pool);

essl_bool _essl_set_compiler_option_value(compiler_options *opts, compiler_option option, int value);

essl_bool _essl_validate_hw_rev(unsigned int hw_rev);
void _essl_set_compiler_options_for_hw_rev(compiler_options *opts, unsigned int hw_rev);

#endif /* COMMON_COMPILER_OPTIONS_H */
