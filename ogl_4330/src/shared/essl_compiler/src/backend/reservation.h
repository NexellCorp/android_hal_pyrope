/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BACKEND_RESERVATION_H
#define BACKEND_RESERVATION_H

#include "backend/liveness.h"
#include "backend/interference_graph.h"

#define RES_DEFAULT_NUM_REGS 8

/** A register reservation interval delimiter.
 *  Indicates the register components that are reserved
 *  between this delimiter and the next.
 */
typedef struct _tag_reg_reservation {
	struct _tag_reg_reservation *next; /**< Next interval delimiter */
	int start_position; /**< Start position for the interval denoted by this delimiter */
	unsigned char masks[RES_DEFAULT_NUM_REGS]; /**< A component mask for each register indicating reserved components */
} reg_reservation;

typedef struct _tag_reservation_context {
	mempool *pool;
	int numregs;

	unsigned permutation_masks[16][16]; /* variable mask,reservation mask -> permutation mask */
	swizzle_pattern permutation_swizzles[24]; /* permutation index -> reservation swizzle */

	reg_reservation *reservations;

	/* For work register read/write integration bug workaround */
	interference_graph_context *conflict_vars;
	ptrdict var_to_reg;
} reservation_context;

reservation_context *_essl_create_reservation_context(mempool *pool, int numregs, int first_position, interference_graph_context *conflict_vars);
int _essl_reservation_find_available_reg(reservation_context *ctx, live_range *range, swizzle_pattern *swz_out /* var to reg */);
memerr _essl_reservation_allocate_reg(reservation_context *ctx, live_range *range, int reg, swizzle_pattern *swz);


#endif /* BACKEND_RESERVATION_H */
