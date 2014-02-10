/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef FRONTEND_ELIMINATE_COMPLEX_OPS_H
#define FRONTEND_ELIMINATE_COMPLEX_OPS_H

#include "common/essl_node.h"
#include "common/error_reporting.h"
#include "common/translation_unit.h"

/**  Initialize a type checking context
	 @returns zero if successful, non-zero if unsuccessful
 */
memerr _essl_eliminate_complex_ops(mempool *pool, typestorage_context *ts_ctx, translation_unit* tu );
memerr _essl_eliminate_complex_returns(mempool *pool, translation_unit* tu);

#endif /* FRONTEND_ELIMINATE_COMPLEX_OPS_H */
