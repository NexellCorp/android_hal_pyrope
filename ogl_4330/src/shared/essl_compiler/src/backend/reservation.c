/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "backend/reservation.h"
#include "backend/liveness.h"

static memerr _essl_new_reservation(reservation_context *ctx, reg_reservation **next_ptr, int position);


static const int number_of_ones[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
static const unsigned short perm_swizzles[24] = {
	/* Swizzle position is var component, value is reg component */
	0x0123,0x0213,0x1023,0x1203,0x2013,0x2103, /*122332*/
	0x0132,0x0231,0x1032,0x1230,0x2031,0x2130, /*232443*/
	0x0312,0x0321,0x1302,0x1320,0x2301,0x2310, /*324324*/
	0x3012,0x3021,0x3102,0x3120,0x3201,0x3210  /*433242*/
};

static unsigned int mask_through_swizzle(swizzle_pattern *swz, unsigned var_mask) {
	unsigned int mask = 0;
	unsigned i;
	for (i = 0 ; i < N_COMPONENTS ; i++) {
		if ((var_mask & (1 << i)) && swz->indices[i] >= 0) {
			mask |= 1 << swz->indices[i];
		}
	}
	return mask;
}

static void make_permutations(reservation_context *ctx) {
	unsigned vc,p,vmask,rmask;
	for (p = 0 ; p < 24 ; p++) {
		for (vc = 0 ; vc < 4 ; vc++) {
			ctx->permutation_swizzles[p].indices[vc] = (perm_swizzles[p] >> ((3-vc)*4)) & 3;
		}
	}

	for (vmask = 0 ; vmask < 16 ; vmask++) {
		for (p = 0 ; p < 24 ; p++) {
			int swizzled_vmask = 0;
			for (vc = 0 ; vc < 4 ; vc++) {
				if (vmask & (1 << vc)) {
					swizzled_vmask |= 1 << ctx->permutation_swizzles[p].indices[vc];
				}
			}
			for (rmask = 0 ; rmask < 16 ; rmask++) {
				if ((swizzled_vmask & rmask) == 0) {
					ctx->permutation_masks[vmask][rmask] |= 1 << p;
				}
			}
		}
	}

}

reservation_context *_essl_create_reservation_context(mempool *pool, int numregs, int first_position, interference_graph_context *conflict_vars) {
	reservation_context *ctx;
	ESSL_CHECK(ctx = _essl_mempool_alloc(pool, sizeof(reservation_context)));
	ctx->pool = pool;
	ctx->numregs = numregs;
	make_permutations(ctx);
	ESSL_CHECK(_essl_new_reservation(ctx, &ctx->reservations, first_position+1));
	ctx->conflict_vars = conflict_vars;
	ESSL_CHECK(_essl_ptrdict_init(&ctx->var_to_reg, pool));
	return ctx;
}

static int _essl_reservation_find_available_permutations(reservation_context *ctx, live_range *range, int reg) {
	unsigned perm_mask = 0xffffff;
	live_delimiter *delim;
	reg_reservation *res = ctx->reservations;
	while (res->next != 0 && res->next->start_position >= range->start_position) {
		res = res->next;
	}
	/* Invariant: Interval between res and res->next starts no later than delim
	 * and ends later than delim but no later than delim->next.
	 */
	for (delim = range->points ; delim->next != 0 ; delim = delim->next) {
		if (delim->next->live_mask != 0) {
			/* Update res and mark all register masks of overlapping reservations */
			while (res->next != 0 && res->next->start_position >= delim->next->position) {
				perm_mask &= ctx->permutation_masks[delim->next->live_mask][res->masks[reg]];
				if (perm_mask == 0) return 0;
				res = res->next;
			}
			/* Past last reservation? */
			if (res->next == 0) {
				return perm_mask;
			}
			/* Reservation overlapping both this and next interval? */
			if (res->start_position > delim->next->position) {
				perm_mask &= ctx->permutation_masks[delim->next->live_mask][res->masks[reg]];
				if (perm_mask == 0) return 0;
			}
		} else {
			/* Update res */
			while (res->next != 0 && res->next->start_position >= delim->next->position) {
				res = res->next;
			}
			/* Past last reservation? */
			if (res->next == 0) {
				return perm_mask;
			}
		}
	}
	return perm_mask;
}

/** Find the lowest-numbered register which has enough components available to hold
 *  the given variable throughout its live range.
 *  If the range is marked as locked, it is allocated in the specific components
 *  indicated by the live range.
 *  Unlocked vectors are allocated in the lowest available components.
 *  Unlocked scalars are allocated in the highest available component.
 *  If no register is available to hold the variable, -1 is returned.
 */
int _essl_reservation_find_available_reg(reservation_context *ctx, live_range *range, swizzle_pattern *swz_out /* var to reg */) {
	int reg;
	for (reg = 0 ; reg < ctx->numregs ; reg++) {
		int perm_mask;

		if (ctx->conflict_vars)
		{
			essl_bool conflict = ESSL_FALSE;
			ptrdict *cvars = _essl_interference_graph_get_edges(ctx->conflict_vars, range->var);
			if (cvars != NULL)
			{
				ptrdict_iter cvar_it;
				node *cvar;
				void *dummy;
				_essl_ptrdict_iter_init(&cvar_it, cvars);
				while ((cvar = _essl_ptrdict_next(&cvar_it, &dummy)) != NULL)
				{
					if (_essl_ptrdict_has_key(&ctx->var_to_reg, cvar))
					{
						int creg = (int)(long)_essl_ptrdict_lookup(&ctx->var_to_reg, cvar);
						if (creg == reg)
						{
							conflict = ESSL_TRUE;
							break;
						}
					}
				}
				if (conflict) continue;
			}
			
		}

		perm_mask = _essl_reservation_find_available_permutations(ctx, range, reg);
		if (range->locked) {
			/* Must be allocated into specific components */
			if (perm_mask & 1) {
				/* The requested components are available */
				int vc;
				for (vc = 0 ; vc < N_COMPONENTS ; vc++) {
					if (range->mask & (1 << vc)) {
						swz_out->indices[vc] = vc;
					} else {
						swz_out->indices[vc] = -1;
					}
				}
				return reg;
			}
		} else if (perm_mask != 0) {
			/* There is space here */
			int vc,p;
			if (number_of_ones[range->mask] == 1) {
				/* Scalar - choose highest numbered permutation */
				p = 23;
				while ((perm_mask & (1 << p)) == 0) p--;
			} else {
				/* Vector - choose lowest numbered permutation */
				p = 0;
				while ((perm_mask & (1 << p)) == 0) p++;
			}

			for (vc = 0 ; vc < N_COMPONENTS ; vc++) {
				if (range->mask & (1 << vc)) {
					swz_out->indices[vc] = ctx->permutation_swizzles[p].indices[vc];
				} else {
					swz_out->indices[vc] = -1;
				}					
			}
			return reg;
		}
	}
	return -1;
}

/** Allocate a new reservation entry */
static memerr _essl_new_reservation(reservation_context *ctx, reg_reservation **next_ptr, int position) {
	reg_reservation *new_res;
	ESSL_CHECK(new_res = (reg_reservation *)_essl_mempool_alloc(ctx->pool, sizeof(reg_reservation)+(ctx->numregs-RES_DEFAULT_NUM_REGS)));
	new_res->next = *next_ptr;
	new_res->start_position = position;
	*next_ptr = new_res;
	assert(new_res->next == 0 || new_res->next->start_position < position);
	return MEM_OK;
}

/** Split a reservation entry into two at the given position, duplicating reservation
 *  masks into both intervals.
 */
static memerr split_reservation(reservation_context *ctx, reg_reservation *res, int position) {
	int r;
	assert(position < res->start_position);
	assert(res->next == 0 || position > res->next->start_position);
	ESSL_CHECK(_essl_new_reservation(ctx, &res->next, position));
	for (r = 0 ; r < ctx->numregs ; r++) {
		res->next->masks[r] = res->masks[r];
	}
	return MEM_OK;
}

/** Update the reservation list corresponding to allocating the given live range into the given
 *  register with the given swizzle.
 */
memerr _essl_reservation_allocate_reg(reservation_context *ctx, live_range *range, int reg, swizzle_pattern *swz) {
	reg_reservation *res = ctx->reservations;
	live_delimiter *delim = range->points;
	while (res->next != 0 && res->next->start_position > range->start_position) {
		res = res->next;
	}

	while (delim->next != 0 && delim->next->position == delim->position) {
		delim = delim->next;
	}
	while (delim->next != 0) {
		/* calculate reservation mask */
		unsigned int mask = mask_through_swizzle(swz, delim->next->live_mask);

		/* Split reservation if it overlaps delimiter */
		if (res->next == 0 || res->next->start_position < delim->position) {
			ESSL_CHECK(split_reservation(ctx, res, delim->position));
		}

		/* reserve subregisters in all reservations that are contained within subrange */
		res = res->next;
		while (res->next != 0 && res->next->start_position > delim->next->position) {
			if(reg >= 0 && reg < ctx->numregs)
			{
				assert((res->masks[reg] & mask) == 0);
				res->masks[reg] |= mask;
			}
			res = res->next;
		}

		/* Split reservation if it overlaps delimiter */
		if (res->next == 0 || res->next->start_position < delim->next->position) {
			ESSL_CHECK(split_reservation(ctx, res, delim->next->position));
		}

		/* Mark rest of interval */
		if(reg >= 0 && reg < ctx->numregs)
		{
				assert((res->masks[reg] & mask) == 0);
				res->masks[reg] |= mask;
		}

		/* Next interval */
		delim = delim->next;
		while (delim->next != 0 && delim->next->position == delim->position) {
			delim = delim->next;
		}
	}

	if (ctx->conflict_vars)
	{
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->var_to_reg, range->var, (void *)(long)reg));
	}

	return MEM_OK;
}

