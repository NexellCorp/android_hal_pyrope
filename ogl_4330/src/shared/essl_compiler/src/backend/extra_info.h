/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef CALCULATE_EXTRA_INFO_H
#define CALCULATE_EXTRA_INFO_H

#include "common/basic_block.h"
#include "common/essl_target.h"

typedef enum {                         /** same as hardware saturate mode for mul & add */
	M200_OUTPUT_NORMAL      = 0,       /* no extra function applied */
	M200_OUTPUT_CLAMP_0_1   = 1,       /* _sat */
	M200_OUTPUT_CLAMP_0_INF = 2,       /* _pos */
	M200_OUTPUT_TRUNCATE    = 3        /* _trunc */
} m200_output_mode;


typedef struct
{
	int exponent_adjust;
	node *trans_node;
	swizzle_pattern swizzle;
	m200_output_mode mode;
} m200_output_modifier_set;



/** Extra information attached to a node temporarily */
typedef struct _tag_node_extra {
	int scheduled_use_count; /**< Number of pointers at this node from scheduled instructions */
	int unscheduled_use_count; /**< Number of pointers at this node from unscheduled LIR nodes */
	int original_use_count; /**< Number of pointers at this node from LIR nodes at the beginning of scheduling */
	int operation_depth; /**< Longest path from the method exit to this node */
	int earliest_use; /**< Subcycle that contains the earliest use of the value created by this node */
	int latest_use; /**< Subcycle that contains the latest use of the value created by this node */

	struct {
		/* Mali200 output transformation */
		m200_output_modifier_set m200_modifiers;
	} u;


	/* Address calculation for load/store nodes */
	/*@null@*/ const symbol_list *address_symbols;
	int address_offset:14;
	int address_multiplier:5;
	int is_indexed:1;

	/* Bookkeeping for the extra_info pass */
	unsigned visited:1;

	/* Register uniforms */
	unsigned is_reg_uniform:1; /**< The uniform is preloaded into a register specified by out_reg and reg_swizzle */

	/* Register allocation */
	unsigned reg_allocated:1; /**< Set when node has a register allocated */
	int out_reg:8; /**< Register allocated for the output of this node */
	swizzle_pattern reg_swizzle; /**< Swizzle for the register allocation */
	union {
		symbol *spill_symbol; /**< For spilled variables: The symbols used for the stack slot */
		struct { void *word; basic_block *block; } sv; /**< For spill variables: The word where the spill occurs */
	} spill;

} node_extra;


typedef struct _tag_extra_info_context extra_info_context;

struct _tag_extra_info_context {
	mempool *pool;
	control_flow_graph *cfg;
	ptrdict *control_dependence; /**< Map from control-dependent nodes to their control_dependent_operation structures */
	op_weight_fun op_weight; /**< Function to specify weight of operations */
	basic_block *current_block; /**< Block being handled at the moment */
};

memerr _essl_calculate_extra_info(control_flow_graph *cfg, op_weight_fun op_weight, mempool *pool);
#if 0
/*@null@*/ node_extra *_essl_new_extra_info(mempool *pool, node *n);
/*@null@*/ node_extra *_essl_get_extra_info(control_flow_graph *cfg, node *n);
/*@null@*/ node_extra *_essl_get_or_create_extra_info(
							 mempool *pool, control_flow_graph *cfg, node *n);
#endif

/*@null@*/ node_extra *_essl_create_extra_info(mempool *pool, node *n);

#define HAS_EXTRA_INFO(n) ((n)->expr.info != 0)
#define EXTRA_INFO(n) (assert((n)->expr.info != 0), (n)->expr.info)
#define CREATE_EXTRA_INFO(pool, n) (_essl_create_extra_info(pool, n))

essl_bool _essl_address_symbol_lists_equal(const symbol_list *a, const symbol_list *b);

#endif
