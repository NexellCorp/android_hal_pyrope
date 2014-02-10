/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef COMPILER_CONTEXT_H
#define COMPILER_CONTEXT_H
#include "compiler.h"
#include "common/essl_mem.h"
#include "common/essl_target.h"
#include "frontend/frontend.h"
#include "common/output_buffer.h"
#include "common/compiler_options.h"

struct _compiler_context
{
	compiler_options *options;
	frontend_context *frontend_ctx;
	mempool_tracker mem_tracker;
	mempool pool;
	target_descriptor *desc;
	translation_unit *tu;
	output_buffer out_buf;
};

#endif /* COMPILER_CONTEXT_H */
