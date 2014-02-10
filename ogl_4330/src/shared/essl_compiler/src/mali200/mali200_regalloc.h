/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI200_REGALLOC_H
#define MALI200_REGALLOC_H

#include "mali200/mali200_liveness.h"
#include "mali200/mali200_relocation.h"
#include "common/error_reporting.h"
#include "backend/reservation.h"
#include "common/ptrset.h"
#include "common/unique_names.h"

#define MALI200_MAX_REGALLOC_ITERATIONS 20

/** The internal state of the register allocator */
typedef struct _tag_regalloc_context {
	mempool *pool; /**< For memory allocations */
	symbol *function; /**< The function to be register allocated */
	control_flow_graph *cfg; /**< The control flow graph of the function */
	int numregs; /**< Total number of registers available to the register allocator for this target */
	int used_regs; /**< Number of registers actually used in this function */
	reservation_context *res_ctx; /**< Reservation context encapsulating currently reserved register components */
	liveness_context *liv_ctx; /**< Liveness information for the function */
	mali200_relocation_context *rel_ctx;
	int next_memory_reg_index;

	ptrset unallocated_ranges; /**< Ranges that were not allocated and should be spilled */
	live_range *failed_range; /**< Spill range that failed allocation because of overpressured word */
	ptrdict hash_load_range; /**< Map from instruction word to the spill range that is allocated into #load at that word. */
} regalloc_context;

memerr _essl_mali200_allocate_registers(mempool *pool, symbol *function, target_descriptor *desc, int numregs, mali200_relocation_context *rel_ctx, unique_name_context *names);

/** Update the reservation list corresponding to allocating the given live range into the given
 *  register with the given swizzle.
 */
memerr _essl_mali200_allocate_reg(regalloc_context *ctx, live_range *range, int reg, swizzle_pattern *swz);

#endif
