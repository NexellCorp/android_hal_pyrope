/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef MALIGP2_PRESCHEDULE_H
#define MALIGP2_PRESCHEDULE_H


#include "common/essl_mem.h"
#include "common/essl_target.h"
#include "common/basic_block.h"
#include "common/compiler_options.h"
#include "common/error_reporting.h"




memerr _essl_maligp2_preschedule(mempool *pool, target_descriptor *desc, typestorage_context *ts_ctx, control_flow_graph *cfg,
								 compiler_options *opts, error_context *error_context);








#endif /* MALIGP2_PRESCHEDULE_H */

