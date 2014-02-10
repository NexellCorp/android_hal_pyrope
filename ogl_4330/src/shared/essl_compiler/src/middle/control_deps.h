/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_GLOBAL_DEPS_H
#define COMMON_GLOBAL_DEPS_H
#include "common/lir_pass_run_manager.h"

memerr _essl_forward_stores_to_loads_and_elide_stores(pass_run_context *pr_ctx, symbol *func);

memerr _essl_control_dependencies_calc(pass_run_context *pr_ctx, symbol *func);





#endif /* COMMON_GLOBAL_DEPS_H */
