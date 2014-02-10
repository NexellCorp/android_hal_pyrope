/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_BASIC_BLOCKS_H
#define COMMON_BASIC_BLOCKS_H

#include "common/essl_node.h"
#include "common/essl_symbol.h"
#include "common/essl_list.h"
#include "common/ptrset.h"
#include "common/ptrdict.h"

struct _tag_control_dependent_operation;
struct _tag_basic_block;



/** The ways in which a basic block can terminate */
typedef enum {
	/** Uninitialized */
	TERM_KIND_UNKNOWN,
	/** Jump to another block.
		If source is NULL, the jump is unconditional to default_target.
		Otherwise, source is the condition for a conditional jump to true_target (true) or default_target (false). */
	TERM_KIND_JUMP,
	/** Pixel kill.
		If source is NULL, the kill is unconditional, and default_target is the exit block.
		Otherwise, source is the condition for the kill - true: kill pixel, false: jump to default_target. */
	TERM_KIND_DISCARD,
	/** Return from function. Return value is in source, NULL for void return. */
	TERM_KIND_EXIT
} term_kind;




/** A source definition for a phi node */
typedef struct _tag_phi_source {
	struct _tag_phi_source *next; /**< Next phi source in phi node list */
	node *source; /**< The expression containing the source definition */
	struct _tag_basic_block *join_block; /**< The block from which the source definition joins into this phi node.
											This is always a predecessor block of the block containing the phi node. */

} phi_source;
ASSUME_ALIASING(phi_source, generic_list);



/** A phi node as it appears in the basic block list */
typedef struct _tag_phi_list {
	struct _tag_phi_list *next; /**< Next phi node in basic block list */
	/*@null@*/symbol *sym; /**< The variable the phi node joins */
	node *phi_node;   /**< The expression node containing the phi node itself */

} phi_list;
ASSUME_ALIASING(phi_list, generic_list);



/** A control dependency between operations */
typedef struct _tag_operation_dependency {
	struct _tag_operation_dependency *next; /**< Next dependency of this operation */
	struct _tag_control_dependent_operation *dependent_on; /**< The operation depended on */

} operation_dependency;
ASSUME_ALIASING(operation_dependency, generic_list);



/** An operation that is fixed to a specific basic block because of
 *  control dependencies
 */
typedef struct _tag_control_dependent_operation {
	struct _tag_control_dependent_operation *next; /**< Next control-dependent operation in the basic block */
	node *op; /**< Expression node containing the operation */
	operation_dependency *dependencies; /**< Linked list of the operations that must come before this one */
	struct _tag_basic_block *block; /**< Block the operation is linked to */

} control_dependent_operation;
ASSUME_ALIASING(control_dependent_operation, generic_list);



/** A variable pre-allocated into a specific register */
typedef struct _tag_preallocated_var {
	struct _tag_preallocated_var *next;
	node *var;
	register_allocation allocation;
} preallocated_var;
ASSUME_ALIASING(preallocated_var, generic_list);



/** List of local variable loads and stores, used for SSA transformation
 */
typedef struct _tag_local_operation {
	struct _tag_local_operation *next; /**< Next local operation in basic block list */
	node *op; /**< Expression node containing the operation */

} local_operation;
ASSUME_ALIASING(local_operation, generic_list);



#define BLOCK_DEFAULT_TARGET 0
#define BLOCK_TRUE_TARGET 1
#define MIN_SUCCESSORS_ALLOCATED 2

/** A basic block in the dependency graph representation
 */
typedef struct _tag_basic_block {
	/*@null@*/struct _tag_basic_block *next; /**< in control flow graph list */
	/*@null@*/struct _tag_predecessor_list *predecessors;
                                       /**< Linked list of predecessor blocks */
	int predecessor_count;             /**< Only for internal use in make_basic_blocks */


	struct _tag_basic_block **successors; /**< Array of pointers to successor blocks. At least two elements are always allocated */
	unsigned n_successors;             /**< Number of entries in the successor array */
	/*@null@*/phi_list *phi_nodes;     /**< Linked list of phi nodes in this block */

	/*@null@*/local_operation *local_ops; /**< Linked list of local operations in this block */
	/*@null@*/control_dependent_operation *control_dependent_ops;
                                       /**< L.list of control-dependent op.s in this block */
	/*@null@*/preallocated_var *preallocated_defs; /**< List of preallocated defs at the top of this block */
	/*@null@*/preallocated_var *preallocated_uses; /**< List of preallocated uses at the bottom of this block */
	term_kind termination;             /**< Block termination kind */

	/*@null@*/ node *source;           /**< Source for return or jump condition */
	/*@null@*/ void *branch_instruction; /**< Branch instruction for use in iselect + schedule backends. If this is non-zero, the source field should be disregarded */
	essl_bool end_of_program_marker; /** For iselect + schedule backends, this indicates that the block should have the implicit exit flag set */
	/*@null@*/ struct _tag_basic_block *immediate_dominator;
                                       /**< Immediate dominator of the block */
	ptrset dominance_frontier;         /**< The set of basic blocks forming the dominance
										* frontier of this block */
	int postorder_visit_number;        /**< The order the basic block is visited in during
                                        * post-order traversal */
	int output_visit_number;           /**< The order the basic block is visited in during
                                        output traversal */

	float cost;                        /**< The cost of placing instructions in this block. */

	/*@null@*/ node *top_depth;        /**< Placeholder node for representing the operations
                                        depth at the top of the block. */
	/*@null@*/ node *bottom_depth;     /**< Placeholder node for representing the operations
                                        * depth at the bottom of the block. */
	ptrset ready_operations;           /**< Operations ready for scheduling in this block.
                                        * Used by the scheduler. */
	void *earliest_instruction_word;   /**< target-specific scheduled instruction words */
	void *latest_instruction_word;     /**< target-specific scheduled instruction words */

	int top_cycle;                     /**< Function-global cycle count for top of block
										* (earliest instruction) */
	int bottom_cycle;                  /**< Function-global cycle count for bottom of block
										* (latest instruction) */
	ptrdict var_live_mask_at_end;      /**< Live variables at block end. Used by the
										* register allocator. */
} basic_block;
ASSUME_ALIASING(basic_block, generic_list);


typedef struct _tag_predecessor_list {
	struct _tag_predecessor_list *next; /**< Next predecessor block list element */
	basic_block *block; /**< The predecessor block */
} predecessor_list;
ASSUME_ALIASING(predecessor_list, generic_list);

/** Simple node list for store/load information on parameters.
 */
typedef struct _tag_storeload_list {
	struct _tag_storeload_list *next; /**< Next node in the list */
	node *n; /**< Pointer to the node */

} storeload_list;
ASSUME_ALIASING(storeload_list, generic_list);

/** A source definition for a phi node */
typedef struct _tag_parameter {
	struct _tag_parameter *next; /**< Next parameter in the list */
	symbol *sym; /**< The symbol for the parameter */
	/*@null@*/ storeload_list *load;  /**< list of nodes loading the parameter, if any */
	/*@null@*/ storeload_list *store; /**< list of nodes storing the parameter, if any */
} parameter;
ASSUME_ALIASING(parameter, generic_list);


typedef struct _tag_word_block {
	void *word;
	basic_block *block;
} word_block;

typedef struct _tag_control_flow_graph {
	struct _tag_basic_block *entry_block; /**< Entry block for function */
	struct _tag_basic_block *exit_block;  /**< Exit block for function */
	unsigned int n_blocks;                /**< Number of basic blocks in the function */
	struct _tag_basic_block **postorder_sequence;
                                          /**< An array of the basic blocks in the function,
										   * in post-order.  Array size is n_blocks. */
	struct _tag_basic_block **output_sequence;
                                          /**< An array of the basic blocks in the function,
										   * in the order of the final code.
										   * Array size is n_blocks. */
	/*@null@*/ parameter *parameters;     /**< The parameters of the functions, given by
										   * symbol and possible load/store nodes */

	ptrdict control_dependence;           /**< Map from control-dependent nodes to their
										   * control_dependent_operation structures */

	int stack_frame_size;                 /**< target-specific stack frame size for the function */
	int start_address;                    /**< target-specific start address for the function */
	int end_address;                      /**< target-specific end address (exclusive) for the function */

	/* A cache of the cycle -> instruction word mapping.
	   Only for internal use in the mapping code.
	*/
	word_block *instruction_sequence;
	int n_instructions;

} control_flow_graph;


/**
   Create a new basic block with memory from pool.
*/
/*@null@*/ basic_block *_essl_new_basic_block(mempool *pool);
/*@null@*/ basic_block *_essl_new_basic_block_with_n_successors(mempool *pool, unsigned n_successors);

basic_block *_essl_insert_basic_block(mempool *pool, basic_block *pred, basic_block *succ, control_flow_graph *cfg);
memerr _essl_remove_basic_blocks(mempool *pool, control_flow_graph *cfg, essl_bool (*predicate)(basic_block *, void *), void *predicate_data);

basic_block *_essl_common_dominator(basic_block *b1, basic_block *b2);

control_dependent_operation *_essl_clone_control_dependent_op(node *orig, node *clone, control_flow_graph *cfg, mempool *pool);
basic_block * _essl_split_basic_block(mempool *pool, basic_block* block, control_dependent_operation* split_point);

void _essl_remove_control_dependent_op_node(control_dependent_operation **list, node *n);
#ifndef NDEBUG
void _essl_validate_control_flow_graph(control_flow_graph *cfg);
#else
#define _essl_validate_control_flow_graph(cfg) do {} while(0)
#endif


#endif
