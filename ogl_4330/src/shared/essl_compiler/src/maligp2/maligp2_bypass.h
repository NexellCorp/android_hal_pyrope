/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef MALIGP2_BYPASS_H
#define MALIGP2_BYPASS_H

#include "maligp2/maligp2_liveness.h"

memerr _essl_maligp2_allocate_bypass_network(mempool *pool, symbol *fun, target_descriptor *desc, live_range *ranges, live_range **out_allocated_ranges, live_range **out_unallocated_ranges);
void _essl_maligp2_rollback_bypass_network(symbol *fun);

memerr _essl_maligp2_integrate_bypass_allocations(mempool *tmp_pool, translation_unit *tu);

#endif /* MALIGP2_BYPASS_H */

