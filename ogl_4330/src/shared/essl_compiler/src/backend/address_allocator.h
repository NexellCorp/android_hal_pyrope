/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef BACKEND_ADDRESS_ALLOCATOR_H
#define BACKEND_ADDRESS_ALLOCATOR_H



#include "common/translation_unit.h"
#include "common/ptrset.h"

struct interference_graph_context;

#define ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS -1
memerr _essl_allocate_addresses(mempool *pool, target_descriptor *desc, int start_address, int max_end_address, symbol *symbols, /*@null@*/ int *actual_end_address, /*@null@*/ struct interference_graph_context *interference);
memerr _essl_allocate_addresses_for_set(mempool *pool, target_descriptor *desc, int start_address, int max_end_address, ptrset *symbols, /*@null@*/ int *actual_end_address, /*@null@*/ struct interference_graph_context *interference);

memerr _essl_allocate_addresses_for_optimized_samplers(mempool *pool, target_descriptor *desc, int start_address, ptrset *symbols, /*@null@*/ int *actual_end_address);







#endif /* BACKEND_ADDRESS_ALLOCATOR_H */
