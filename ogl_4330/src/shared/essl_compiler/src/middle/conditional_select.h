/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef CONDITIONAL_SELECT_H
#define CONDITIONAL_SELECT_H

#include "common/basic_block.h"
#include "common/essl_target.h"
#include "common/lir_pass_run_manager.h"

memerr _essl_optimise_conditional_selects(pass_run_context *pr_ctx, symbol *func);

#endif
