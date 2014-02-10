/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BACKEND_LIVENESS_H
#define BACKEND_LIVENESS_H

#include "common/essl_node.h"
#include "common/basic_block.h"
#include "common/essl_mem.h"
#include "common/essl_target.h"
#include "common/essl_list.h"
#include "common/error_reporting.h"

typedef enum {
	LIVE_UNKNOWN, /**< Uninitialized delimiter kind */
	LIVE_DEF, /**< Definition (and possibly lifetime start) */
	LIVE_USE, /**< Use (and possibly lifetime end) */
	LIVE_STOP, /**< Start of lifetime hole */
	LIVE_RESTART /**< End of lifetime hole */
} live_delimiter_kind;

/** Liveness interval delimiter point.
 *  This represents a place where there is a definition or use of a variable or
 *  the liveness of the variable changes because of control flow.
 */
typedef struct _tag_live_delimiter {
	struct _tag_live_delimiter *next; /**< The next delimiter point in this liveness range */
	live_delimiter_kind kind:4; /**< The kind of delimiter represented */
	unsigned mask:N_COMPONENTS; /**< The use or def mask of the delimiter */
	unsigned live_mask:N_COMPONENTS; /**< The component liveness mask of the variable before this delimiter */
	unsigned locked:1; /**< Are the components of this use or def locked, i.e. cannot be swizzled?
						  For MaliGP2 scalar values: Does this delimiter correspond to a move reservation? */
	unsigned use_accepts_lut:1; /**< Can this use consume a value produced by the LUT unit? (MaliGP2 only) */
	signed position; /**< The position of the delimiter */
	node **var_ref; /**< The variable reference in the instruction which causes the definition or use.
					   Only used for LIVE_DEF and LIVE_USE delimiters. */
} live_delimiter;
ASSUME_ALIASING(live_delimiter, generic_list);

/** Liveness range.
 *  Represents the (not necessarily contiguous) range in which a variable is live.
 */
typedef struct _tag_live_range {
	struct _tag_live_range *next; /**< Range for next variable */
	node *var; /**< The variable to which the range belongs */
	signed start_position; /**< Earliest position at which the variable is live */
	unsigned mask:N_COMPONENTS; /**< Union of live masks of all liveness intervals */
	unsigned locked:1; /**< Are the components of the variable locked (i.e. are any uses or defs locked)? */
	unsigned allocated:1; /**< Has the variable been allocated to a register? */
	unsigned potential_spill:1; /**< Is this range marked by the graph coloring as a potential spill? */
	unsigned spilled:1; /**< Has this range been spilled to memory? */
	unsigned spill_range:1; /**< Is this a range connecting a spill load/store to its use/def? */
	unsigned fixed:1; /**< Is this range going to be allocated into a specific register? */
	unsigned unspillable:1; /**< Is this range of a kind that cannot be spilled? */
	unsigned sort_weight:3; /**< Used for storing sort weight when sorting ranges prior to allocation */
	live_delimiter *points; /**< The interval delimiter points of the range */
} live_range;
ASSUME_ALIASING(live_range, generic_list);

struct _tag_liveness_context;

/** Callback to mark all definitions and uses of variables in a block.
 *  Should traverse all definitions and uses in the block in reverse order and call
 *  _essl_liveness_mark_def or _essl_liveness_mark_use for each.
 *  The last argument to the function is the context pointer passed to _essl_liveness_calculate_live_ranges.
 */
typedef memerr (*block_liveness_handler)(struct _tag_liveness_context *, basic_block *, void *);

/* Add one cycle to all instructions in the block whose subcycle
   ends at or before the given position */
typedef void (*insert_cycle_callback)(basic_block *block, int position);
typedef unsigned (*mask_from_node_fun)(node *n);

/** Context information used by the liveness calculator and result of the liveness calculation. */
typedef struct _tag_liveness_context {
	/* Information passed in */
	mempool *pool;
	control_flow_graph *cfg;
	target_descriptor *desc;
	block_liveness_handler block_func;
	void *block_func_data;
	error_context *error_context;
	mask_from_node_fun mask_from_node;

	/* Liveness result */
	live_range *var_ranges; /**< List of liveness ranges for all variables. */
	ptrdict var_to_range; /**< Map from variables to their liveness ranges.
						   During liveness calculation, this maps to the last delimiter
						   to be encountered (earliest in the program no earlier than
						   the current position). */
} liveness_context;

/* Position/cycle conversion macros */
#define POSITIONS_PER_CYCLE 10
#define START_OF_CYCLE(cycle) ((cycle)*10+9)
#define END_OF_CYCLE(cycle) ((cycle)*10)
#define START_OF_SUBCYCLE(subcycle) ((subcycle)*5/4*2+2)
#define END_OF_SUBCYCLE(subcycle) ((subcycle)*5/4*2+1)
#define POSITION_TO_CYCLE(pos) ((pos)/10)
#define POSITION_TO_RELATIVE_POSITION(pos) ((pos) % 10)

/** Called by the liveness handler callback to mark a use of a variable.
 *  @param var_ref pointer to the variable reference in the instruction which causes the use
 *  @param position the position of the use
 *  @param mask the components that are used
 *  @param locked indicates that the component cannot be swizzled by the register allocator
 *  @param accepts_lut (MaliGP2 only) this use accepts the output from the LUT unit as input
 */
memerr _essl_liveness_mark_use(liveness_context *ctx, node **var_ref, int position, unsigned mask, essl_bool locked, essl_bool accepts_lut);

/** Called by the liveness handler callback to mark a definition of a variable.
 *  @param var_ref pointer to the variable reference in the instruction which causes the definition
 *  @param position the position of the definition
 *  @param mask the components that are defined
 *  @param locked indicates that the component cannot be swizzled by the register allocator
 */
memerr _essl_liveness_mark_def(liveness_context *ctx, node **var_ref, int position, unsigned mask, essl_bool locked);

/** Create a liveness context object for performing liveness calculation.
 *  @param block_func callback to find defs and uses in a block
 *  @param block_func_data passed to the callback function
 */
liveness_context *_essl_liveness_create_context(mempool *pool, control_flow_graph *cfg, target_descriptor *desc,
												block_liveness_handler block_func, void *block_func_data, mask_from_node_fun mask_from_node,
												error_context *error_context);

/** Perform the liveness calculation.
 *  The result is available in the var_ranges and var_to_range fields of the liveness context.
 */
memerr _essl_liveness_calculate_live_ranges(liveness_context *ctx);

/** Remove the given range from this liveness context */
void _essl_liveness_remove_range(liveness_context *ctx, live_range *range);

/** Insert a new range into the liveness context */
memerr _essl_liveness_insert_range(liveness_context *ctx, live_range *range);


/** Sort the given liveness ranges in an order suitable for allocation. */
live_range *_essl_liveness_sort_live_ranges(live_range *ranges);

/** Merge two liveness ranges into one, if possible.
 *  If the two ranges have overlapping liveness, ESSL_FALSE is returned.
 *  Otherwise, the ranges are merged into range1 (range2 becomes empty), and ESSL_TRUE is returned.
 */
essl_bool _essl_liveness_merge_live_ranges(live_range *range1, live_range *range2);

/** Insert short liveness intervals (terminated by STOP delimiters) after dead definitions
 *  to make sure these ranges will conflict with other ranges live at the same position.
 *  If the var_refs set is supplied, only fix definitions at those specific intructions.
 */
memerr _essl_liveness_fix_dead_definitions(mempool *pool, live_range *ranges, ptrset *var_refs);

/** Create a new liveness delimiter */
live_delimiter *_essl_liveness_new_delimiter(mempool *pool, node **var_ref, live_delimiter_kind kind, int position);

/** Create a new liveness range from the given delimiter list.
 *  The mask and locked fields are derived from the delimiters.
 */
live_range *_essl_liveness_new_live_range(mempool *pool, node *var, live_delimiter *points);

/** Create a liveness range with a single liveness span from pos1 to pos2. */
live_range *_essl_liveness_make_simple_range(mempool *pool, control_flow_graph *cfg, node *var,
											 int pos1, int pos2, unsigned int live_mask, unsigned int locked);

/** Insert new_delim into the delimiter list given by first_delim.
 *  Is it assumed that new_delim is positioned after the start of first_delim.
 */
void _essl_liveness_insert_delimiter(live_delimiter *first_delim, live_delimiter *new_delim);

/** Parameter to _essl_liveness_split_range indicating which part of the range
 *  should remain.
 */
typedef enum {
	LIVENESS_SPLIT_KEEP_TO_START,
	LIVENESS_SPLIT_KEEP_ALL_EXCEPT_PHI_USE
} liveness_split_mode;

/** Find latest point before given position where the liveness mask of a variable
 *  does not match the given mask.
 *  @param range the liveness range for the variable
 *  @param position the position to look backwards from
 *  @param mask the liveness mask to look for
 */
live_delimiter *_essl_liveness_find_preceding_liveness(live_range *range, int position, int mask);

/** Adjust all cycle, subcycle and position indicators to accommodate insertion
 *  of an extra instruction word after the specified position in the specified block
 *  @param insert_into_instructions callback to adjust positions in instructions
 *         and instruction words
 */
memerr _essl_liveness_insert_cycle(liveness_context *ctx,
								   int position, basic_block *insertion_block,
								   insert_cycle_callback insert_into_instructions);

/** Calculate all live masks from the delimiter masks */
void _essl_liveness_correct_live_range(live_range *range);

/** Mark all ranges containing preallocated delimiters as fixed */
memerr _essl_liveness_mark_fixed_ranges(liveness_context *ctx);

#ifndef NDEBUG
/** Assert that two liveness contexts contain the exact same ranges */
void _essl_liveness_assert_identical(liveness_context *ctx1, liveness_context *ctx2);
#define LIVENESS_ASSERT_IDENTICAL(ctx1, ctx2) _essl_liveness_assert_identical(ctx1, ctx2)
#else
#define LIVENESS_ASSERT_IDENTICAL(ctx1, ctx2)
#endif

#endif
