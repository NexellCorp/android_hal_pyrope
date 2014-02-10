/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


/*
  The main AST to LIR transformation phase.

  The overall tasks for this phase is to:

  * Create basic blocks for the control flow structure corresponding to
    if/while/do-while/break/continue/return/discard statements,
    conditional expressions and lazy boolean combinators, plus unique
    entry and exit blocks. For conditional expressions, a temporary variable is
    created to connect the values.

  * Make order of evaluation explicit by sequentializing all reads and writes
    to variables and expressing all remaining data flow as direct data
    dependency links.
    Reads and writes to local variables are put into a local_ops list for
    each block to be later joined together as direct data dependencies by
    the SSA transformation phase.
    Reads and writes to global variables, arrays, function arguments and
    function parameters produce explicit LOAD and STORE operations. These,
    along with function call nodes, are put into a control_dependent_ops list
	for each block. Later, in the control dependency calculation phase, a
	dependency graph between these operations is built.


  The transformation traverses the AST in evaluation order and outputs blocks
  and operation lists as it goes along, rewriting expressions where necessary.

  The transformation accepts ASTs where some nodes share children. The execution
  semantics of such a construct is that the shared subexpression is evaluated
  the first time it is encountered, and the computed value is reused on
  subsequent occurrences. Thus, in any such case, the first use must dominate
  any subsequent uses.


  The production of a basic block proceeds in four steps:

  * Create: The block is allocated using _essl_new_basic_block(). This is often
    done some time before the block is actually entered, since earlier blocks
	might need the identity of the block in order to produce jumps to it.

  * Enter: The traversal has reached the operations that occur in this block.
    The block is appended to the block list and is now the current block.
	Signalled by a call to start_new_block().

  * Fill: All local and control dependent operations encountered are added to
    the corresponding lists in the current block. This is done using
    add_local_op() and add_control_dependent_op().

  * Terminate: End of the block. A block can end in four different ways,
    signalled by a call to one of the four block termination functions:
    - term_block_jump(): An unconditional jump to another block (default_target).
      A block where control flows through the end is modelled as an explicit
	  jump to the successor block.
	  Returns are modelled as jumps to the exit block.
    - term_block_conditional(): Jump to one of two blocks (true_target or
	  default_target) depending on the value of the condition expression (source).
    - term_block_discard(): Discard pixel. Has default_target set to the exit
	  block.
    - term_block_exit(): The exit block of the function. Has no targets. Has
	  source set to the return value, or null for a void return.
    The current block is still current after termination, but is now in a
	terminated state where no more operations may be added.


  The translation distinguishes between value context and condition context
  for boolean expression nodes. An expression is in condition context if it is
    - the condition of an if statement, while statement, do-while statement or
	  conditional select expression,
    - a child of a lazy boolean connective (&& or ||),
    - the child of a boolean not (!) which is itself in condition context, or
    - a child of a conditional select expression which is itself in condition
      context.
  Boolean expressions in value context produce values (1 for true and 0 for false).
  Boolean expressions in condition context select between two target blocks.
  When a comparison or boolean connective occurs in value context, it is
  embedded into a conditional select between 1 and 0. When any other
  expression occurs in condition context, it is embedded into a comparison
  against 0.


  The processing of an lvalue expression can operate in two modes: value mode
  and address mode. Value mode produces a read of the variable, while address
  mode produces a calculation of the address of the variable for use in
  assignments to the variable. Address mode is signalled by the is_address
  argument to the processing functions.


  Variables are categorised into three kinds. Access to variables differ in the
  LIR depending on the category:
    - Local variables (accept arrays): Reads are VARIABLE_REFERENCEs, writes
      are assignments to VARIABLE_REFERENCEs.
    - Global read-only variables: Reads become LOADs. Not in the control-dependent
      ops list.
    - Control-dependent variables: Reads become LOADs, writes become STOREs. Put
      onto the control-dependent ops list.


  The translation of an assignment proceeds in six steps.

      Running example: a[f()].x += g();

  * If it is an operator assignment or a pre/post inc/dec
    - The _essl_middle_split_lvalue function is called to duplicate the lvalue into one for reading
      the value and one for writing the result of the computation back. Any side effects
      in the lvalue are shared, so that they are only executed once.
    - The operator assignment is transformed into the operation plus an assignment.

      @temp = f();
      a[@temp].x = a[@temp].x + g();

  * If the left-hand-side is a swizzle or vector indexing, rewrite it into a right-hand-side
    swizzle and/or subvector combine operation. If it was an operation assignment, only the
	lvalue generated for the left-hand-side by split_value will be rewritten.

      @temp = f();
      a[@temp] = combine(0111, a[@temp].x + g(), a[@temp]);

  * If the variable is control-dependent, the assignment is transformed into a STORE.

      @temp = f();
      STORE(a[@temp], combine(0111, a[@temp].x + g(), a[@temp]));

  * Process the calculation of the lvalue address in address mode to produce the calculation
    of the variable address. Embedded index expressions are processed in value mode, since
	they are evaluated normally as part of the address calculation, including side effects.

      @temp = f();
      STORE(a[@temp], combine(0111, a[@temp].x + g(), a[@temp]));

	  Control-dependent ops: f()

  * Process the evaluation of the right-hand-side. If it was an operation assignment,
    side effects will not be repeated since the side-effecting nodes are shared between the
	two lvalues produced by _essl_middle_split_lvalue.

      @temp = f();
      STORE(a[@temp], combine(0111, LOAD(a[@temp]).x + g(), LOAD(a[@temp])));

	  Control-dependent ops: f(), LOAD(a[@temp]), g(), LOAD(a[@temp])

  * Put the actual assignment or STORE into local_ops or control_dependent_ops.

      @temp = f();
      STORE(a[@temp], combine(0111, LOAD(a[@temp]).x + g(), LOAD(a[@temp])));

	  Control-dependent ops: f(), LOAD(a[@temp]), g(), LOAD(a[@temp]), STORE(a[@temp], ...)

  Function arguments with 'in' or 'inout' qualifier are treated as assignments
  from the actual argument expressions to the argument variables. Function
  arguments with 'out' or 'inout' qualifier are treated as assignments from the
  argument variables to the actual argument lvalues.



  The traversal is done in five main functions:

  * memerr make_basic_blocks_stmt(node *n, make_basic_blocks_context *ctx,
                                  basic_block *continue_block, basic_block *break_block)

    Process the statement n.

	Assumptions:
    - All statements up to but not including n have been processed.
    - n has not been processed.
	- If the statement is in a context where a continue statement is legal,
	  continue_block is the block to jump to on a continue.
	- If the statement is in a context where a break statement is legal,
	  break_block is the block to jump to on a break.
    - If the current block is terminated, this statement is unreachable and
	  should be ignored.

    State after:
    - All statements up to and including n have been processed.
    - If the processed statement broke away from the straight control flow
	  (via break, continue, return or discard), the current block has been
	  terminated.

  * node *make_basic_blocks_expr(node *n, make_basic_blocks_context *ctx, int is_address)

    Process the expression n and produce a value.

	Assumptions:
    - n is in value context.
    - n has not been processed.
    - The current block is not terminated.

	State after:
    - n has been processed.
    - The current block is not terminated.

  * memerr make_basic_blocks_cond(node *n, make_basic_blocks_context *ctx,
								  basic_block *true_block, basic_block *false_block);

    Process the expression n and jump to one of the target blocks.

	Assumptions:
    - n is in condition context.
    - n has not been processed.
    - The current block is not terminated.
	- The true_block and false_block arguments specify which blocks should be jumped to
	  if n evaluates to true or false, respectively.

    State after:
    - n has been processed.
    - The current block is terminated.
	
  * node *make_basic_blocks_expr_p(node **np, make_basic_blocks_context *ctx, int is_address)

    A wrapper for make_basic_blocks_expr to update child pointers and memoize previously
    encountered expressions (to enable children sharing).

	Assumptions:
    - np points to a child pointer.
    - *np is in value context.
    - *np has not been processed from the current parent but may have been encountered before.
    - The current block is not terminated.

	State after:
    - *np has been processed.
    - If the node was transformed into a new node, the child pointer has been updated.
    - The current block is not terminated.

  * memerr make_basic_blocks_expr_children(node *n, make_basic_blocks_context *ctx, int is_address)

    A simple wrapper to process all children of a node in evaluation order (left to right)
	using make_basic_blocks_expr_p().



*/


#define NEEDS_STDIO  /* for snprintf */

#include "common/essl_system.h"
#include "frontend/make_basic_blocks.h"
#include "common/symbol_table.h"


#ifndef NDEBUG
/** Check for duplicates in the local_ops and control_dependent_ops lists. This helps catch double visit bugs */
#define DUPLICATE_CHECKING 1
#endif


/** Has this block been terminated yet? */
#define IS_TERMINATED(block) ((block)->termination != TERM_KIND_UNKNOWN)

/** Process the expression n, which is assumed not to occur as a condition */
/*@null@*/ static node *make_basic_blocks_expr(node *n, make_basic_blocks_context *ctx, int is_address);

/** Wrapper for make_basic_blocks_expr that only processes the expression if it has not been
	processed before and writes the result into the child pointer of the parent */
/*@null@*/ static node *make_basic_blocks_expr_p(node **np, make_basic_blocks_context *ctx, int is_address);

/** Process the children of n in left-to-right order */
static memerr make_basic_blocks_expr_children(node *n, make_basic_blocks_context *ctx, int is_address);

/* Process the statement n */
static memerr make_basic_blocks_stmt(node *n, make_basic_blocks_context *ctx,
									 /*@null@*/basic_block *continue_block, /*@null@*/basic_block *break_block);

/* Process the expression n occurring as a condition */
static memerr make_basic_blocks_cond(node *n, make_basic_blocks_context *ctx,
									 basic_block *true_block, basic_block *false_block);


/*@null@*/ static storeload_list *rewrite_assignment(make_basic_blocks_context *ctx, node *n, node *rvalue, var_control_dependent is_control_dependent);

/*@null@*/ static node *handle_variable_explicitly(make_basic_blocks_context *ctx, node *n, need_load_store need_ls, var_control_dependent is_control_dep);

#define LOOP_COST_MULTIPLIER 10.0f
#define IF_COST_MULTIPLIER 0.5f
#define INITIAL_BLOCK_COST 1.0f

/** Make a new basic block current and link it into the block list. */
static void start_new_block(basic_block *block, make_basic_blocks_context *ctx) {
	if (ctx->current) {
		assert(IS_TERMINATED(ctx->current));
		ctx->current->next = block;
	}
	block->cost = ctx->current_block_cost;
	ctx->current = block;
	ctx->next_local_p = &block->local_ops;
	ctx->next_control_dependent_p = &block->control_dependent_ops;
}


/** Add a local operation (local variable reference or assignment to
 *  local variable) to the list of local operations in the current block.
 */
static memerr add_local_op(node *op, make_basic_blocks_context *ctx) {
	local_operation *local_op;
	assert(!IS_TERMINATED(ctx->current));
#if DUPLICATE_CHECKING
	{
		local_operation *p;
		for(p = ctx->current->local_ops; p != NULL; p = p->next)
		{
			assert(p->op != op);
		}
	}
#endif
	ESSL_CHECK(local_op = LIST_NEW(ctx->pool, local_operation));
	local_op->op = op;
	*ctx->next_local_p = local_op;
	ctx->next_local_p = &local_op->next;
	return MEM_OK;
}

/** Add a control-dependent operation (global variable reference, assignment to
 *  global variable, array entry reference, array entry assignment or
 *  function call) to the list of control-dependent operations in the current block.
 */
static memerr add_control_dependent_op(node *op, make_basic_blocks_context *ctx) {
	control_dependent_operation *control_dependent_op;
	assert(!IS_TERMINATED(ctx->current));

#if DUPLICATE_CHECKING
	{
		control_dependent_operation *p;
		essl_bool found_next = (ctx->next_control_dependent_p == &ctx->current->control_dependent_ops);
		for(p = ctx->current->control_dependent_ops; p != NULL; p = p->next)
		{
			assert(p->op != op);
			if (&p->next == ctx->next_control_dependent_p) {
				assert(p->next == NULL);
				found_next = ESSL_TRUE;
			}
		}
		assert(found_next);
	}
#endif

	ESSL_CHECK(control_dependent_op = LIST_NEW(ctx->pool, control_dependent_operation));
	control_dependent_op->op = op;
	control_dependent_op->block = ctx->current;
	assert(*ctx->next_control_dependent_p == NULL);
	*ctx->next_control_dependent_p = control_dependent_op;
	ctx->next_control_dependent_p = &control_dependent_op->next;

	/* Mark op as control-dependent and add link the control_dependent_operation structure to it */
	op->hdr.is_control_dependent = 1;
	assert(ctx->function->control_flow_graph != 0);
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->function->control_flow_graph->control_dependence, op, control_dependent_op));

	return MEM_OK;
}

/** Create a node representing a boolean constant. */
/*@null@*/ static node *create_bool_constant(int value, make_basic_blocks_context *ctx) {
	node *n;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, 1));
	n->expr.u.value[0] = ctx->desc->bool_to_scalar(value);
	ESSL_CHECK(n->hdr.type = _essl_get_type_with_default_size_for_target(ctx->typestor_context, TYPE_BOOL, 1, ctx->desc));
	return n;
}


/** Create a node representing an int or ivec constant. */
/*@null@*/ static node *create_index_int_constant(make_basic_blocks_context *ctx, int value)
{
	node *n;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, 1));
	n->expr.u.value[0] = ctx->desc->int_to_scalar(value);

	ESSL_CHECK(n->hdr.type = _essl_get_type_with_default_size_for_target(ctx->typestor_context, TYPE_INT, 1, ctx->desc));
	return n;
}

/** Create a node representing an float or vec constant. */
/*@null@*/ static node *create_float_constant(make_basic_blocks_context *ctx, float value, unsigned vec_size, scalar_size_specifier scalar_size)
{
	node *n;
	unsigned i;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, vec_size));
	n->expr.u.value[0] = ctx->desc->float_to_scalar(value);
	for(i = 1; i < vec_size; ++i)
	{
		n->expr.u.value[i] = n->expr.u.value[0];
	}

	ESSL_CHECK(n->hdr.type = _essl_get_type_with_size(ctx->typestor_context, TYPE_FLOAT, vec_size, scalar_size));
	return n;
}


/** Create a node representing the value 1 as the given type.
 *  Used for the rewriting of increment/decrement operations.
 */
/*@null@*/ static node *create_one_constant(const type_specifier *t, make_basic_blocks_context *ctx) {
	node *n;
	unsigned i;
	unsigned type_size = _essl_get_type_size(t);
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, type_size));
	switch (t->basic_type) {
	case TYPE_FLOAT:
		n->expr.u.value[0] = ctx->desc->float_to_scalar(1.0f);
		break;
	case TYPE_INT:
		n->expr.u.value[0] = ctx->desc->int_to_scalar(1);
		break;
	default:
		assert(0); /* Only int and float can represent ones */
		break;
	}
	for(i = 1; i < type_size; ++i)
	{
		n->expr.u.value[i] = n->expr.u.value[0];
	}

	n->hdr.type = t;
	return n;
}

/** Terminate the current block with an unconditional jump. */
static void term_block_jump(make_basic_blocks_context *ctx, basic_block *jump_target) {
	basic_block *block = ctx->current;
	assert(!IS_TERMINATED(block));
	block->termination = TERM_KIND_JUMP;
	block->source = 0;
	block->n_successors = 1;
	block->successors[BLOCK_DEFAULT_TARGET] = jump_target;
	++jump_target->predecessor_count;
}

/** Terminate the current block with a conditional jump. */
static void term_block_conditional(make_basic_blocks_context *ctx, node *source, basic_block *true_target, basic_block *false_target) {
	basic_block *block = ctx->current;
	assert(!IS_TERMINATED(block));
	block->termination = TERM_KIND_JUMP;
	block->source = source;
	block->n_successors = 2;
	block->successors[BLOCK_TRUE_TARGET] = true_target;
	block->successors[BLOCK_DEFAULT_TARGET] = false_target;
	++true_target->predecessor_count;
	++false_target->predecessor_count;
}

/** Terminate the current block with a discard. */
static void term_block_discard(make_basic_blocks_context *ctx) {
	basic_block *exit_block = ctx->function->control_flow_graph->exit_block;
	basic_block *block = ctx->current;
	assert(!IS_TERMINATED(block));
	block->termination = TERM_KIND_DISCARD;
	block->n_successors = 1;
	block->successors[BLOCK_DEFAULT_TARGET] = exit_block;
	++exit_block->predecessor_count;
}

/** Terminate the exit block. */
static void term_block_exit(make_basic_blocks_context *ctx, /*@null@*/ node *return_value) {
	basic_block *block = ctx->current;
	assert(!IS_TERMINATED(block));
	block->termination = TERM_KIND_EXIT;
	block->n_successors = 0;
	block->source = return_value;
}

/** Calculates the reverse permutation of the given swizzle.
	Assumes that no component is mentioned twice in the swizzle. */
static swizzle_pattern reverse_swizzle(swizzle_pattern in)
{
	unsigned i;
	swizzle_pattern out = _essl_create_undef_swizzle();
	
	for(i = 0; i < N_COMPONENTS; ++i)
	{ 
		if((int)in.indices[i] >= 0)
		{
			assert(out.indices[(unsigned)in.indices[i]] == -1);
			out.indices[(unsigned)in.indices[i]] = (signed char)i;
		}
	}
	return out;
	
}

/** Calculates a write mask indicating which components contain results
	in the given swizzle. */
static combine_pattern combiner_from_swizzle(swizzle_pattern in)
{
	combine_pattern out = _essl_create_undef_combiner();
	unsigned i = 0;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		out.mask[i] = ((int)in.indices[i] == -1) ? (char)0 : (char)1;
	}
	return out;
}

/** Invert the bits of the combiner, essentially swapping the two arguments. */
static combine_pattern invert_combiner(combine_pattern a)
{
	unsigned i;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(a.mask[i] >= 0) a.mask[i] = 1 - a.mask[i];
	}
	return a;
}


/** Split lvalue into read and write parts for operator assignment
	without duplicating side effects. */
memerr _essl_middle_split_lvalue(node *n, /*@out@*/node **left, /*@out@*/node **right, var_control_dependent is_control_dependent, mempool *pool)
{
	/* the nodes can be EXPR_OP_MEMBER, EXPR_OP_SWIZZLE, EXPR_OP_INDEX and EXPR_KIND_VARIABLE_REFERENCE */
	node *child0;
	*left = n;
	*right = n;
	
	if(is_control_dependent == VAR_IS_CONTROL_DEPENDENT)
	{
		switch(n->hdr.kind)
		{
		case EXPR_KIND_UNARY:
			switch(n->expr.operation)
			{
			case EXPR_OP_SWIZZLE:
				break; /* split this */
			case EXPR_OP_MEMBER:
				return MEM_OK; /* not this */
			default:
				assert(0);
			}
			break;
		case EXPR_KIND_BINARY:
			if(n->expr.operation == EXPR_OP_INDEX)
			{
				child0 = GET_CHILD(n, 0);
				assert(child0 != 0);
				assert(child0->hdr.type != 0);
				if(child0->hdr.type->basic_type == TYPE_ARRAY_OF || child0->hdr.type->basic_type == TYPE_MATRIX_OF)
				{
					return MEM_OK; /* array reference is a control-dependent operation */
				}
			}
			break;
		case EXPR_KIND_VARIABLE_REFERENCE:
			return MEM_OK;
		default:
			assert(0);
			break;
		}

	}

	ESSL_CHECK(*right = _essl_clone_node(pool, n));

	if(n->hdr.kind == EXPR_KIND_BINARY && n->expr.operation == EXPR_OP_INDEX)
	{
		child0 = GET_CHILD(n, 0);
		assert(child0 != 0);
		assert(child0->hdr.type != 0);
		if(child0->hdr.type->basic_type != TYPE_ARRAY_OF && child0->hdr.type->basic_type != TYPE_MATRIX_OF)
		{

			(*right)->expr.operation = EXPR_OP_SUBVECTOR_ACCESS;
		}
	}

	if(GET_N_CHILDREN(n) > 0)
	{
		child0 = GET_CHILD(n, 0);
		if(child0 != 0)
		{
			return _essl_middle_split_lvalue(child0, GET_CHILD_ADDRESS(*left, 0), GET_CHILD_ADDRESS(*right, 0), is_control_dependent, pool);
		}
	}
	return MEM_OK;
}



/** Perform a store of an array/matrix variable split into n_elems elements
 */
static storeload_list* store_array_matrix_variable(make_basic_blocks_context* ctx, node* address, node *ref, unsigned n_elems, var_control_dependent is_control_dependent)
{

	storeload_list* rec = 0;
	unsigned i;
	const type_specifier *elem_type;
	elem_type = address->hdr.type->child_type;
	for(i = 0; i < n_elems; ++i)
	{
		storeload_list *rec2;
		node *ind1, *ind2, *num;
		ESSL_CHECK(num = create_index_int_constant(ctx, i));
		
		ESSL_CHECK(ind1 = _essl_new_binary_expression(ctx->pool, address, EXPR_OP_INDEX, num));
		ind1->hdr.type = elem_type;

		ESSL_CHECK(ind2 = _essl_new_binary_expression(ctx->pool, ref, EXPR_OP_INDEX, num));
		ind2->hdr.type = elem_type;

		ESSL_CHECK(make_basic_blocks_expr_p(&ind1, ctx, 1));
		ESSL_CHECK(make_basic_blocks_expr_p(&ind2, ctx, 0));
		ESSL_CHECK(rec2 = rewrite_assignment(ctx, ind1, ind2, is_control_dependent));

		LIST_INSERT_BACK(&rec, rec2);

	}
	return rec;
}


/** Perform an assign of a struct variable.
 */
static storeload_list* handle_struct_assignment(make_basic_blocks_context* ctx, node* address, node *ref, var_control_dependent is_control_dependent)
{
	const type_specifier *t = address->hdr.type;
	single_declarator *member;
	storeload_list* rec = 0;
	for(member  = t->members; member != 0; member = member->next)
	{
		storeload_list *rec2;
		node *ind1, *ind2;
		ESSL_CHECK(ind1 = _essl_new_unary_expression(ctx->pool, EXPR_OP_MEMBER, address));
		ind1->hdr.type = member->type;
		ind1->expr.u.member = member;
		ESSL_CHECK(ind2 = _essl_new_unary_expression(ctx->pool, EXPR_OP_MEMBER, ref));
		ind2->hdr.type = member->type;
		ind2->expr.u.member = member;
		/* Check for nested structure definitions */
		ESSL_CHECK(make_basic_blocks_expr_p(&ind1, ctx, 1));
		ESSL_CHECK(make_basic_blocks_expr_p(&ind2, ctx, 0));
		ESSL_CHECK(rec2 = rewrite_assignment(ctx, ind1, ind2, is_control_dependent));
		LIST_INSERT_BACK(&rec, rec2);
	}
	return rec;
}

/** Rewrite an assignment where the left-hand-side contains a swizzle or vector indexing
    into a direct assignment with a swizzle and/or vector combine operation on the
	right-hand-side.
	If this is a matrix/array/struct assignment, split into one assignment per column/element/member.
	Additionally, rewrite assignments to a control-dependent variables into STORE operations. */

/*@null@*/ static storeload_list *rewrite_assignment(make_basic_blocks_context *ctx, node *n, node *rvalue, var_control_dependent is_control_dependent)
{
	node *child0, *child1;
	switch(n->hdr.kind)
	{
	case EXPR_KIND_UNARY:
		child0 = GET_CHILD(n, 0);
		assert(child0 != 0);
		if(n->expr.operation == EXPR_OP_SWIZZLE)
		{
			swizzle_pattern rev = reverse_swizzle(n->expr.u.swizzle);
			node *tmp;
			ESSL_CHECK(tmp = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, rvalue));
			assert(n->hdr.type != 0);
			ESSL_CHECK(tmp->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, n->hdr.type, GET_NODE_VEC_SIZE(GET_CHILD(n, 0))));
			tmp->expr.u.swizzle = rev;
			assert(child0->hdr.type != 0);
			if(GET_NODE_VEC_SIZE(n) == GET_NODE_VEC_SIZE(child0))
			{
				/* don't need a combine node at all, just use the reversed swizzle */
				return rewrite_assignment(ctx, child0, tmp, is_control_dependent);
			} else {
				/* combiner needed, create it */
				node *tmp2 = 0, *tmp3 = 0;
				ESSL_CHECK(_essl_middle_split_lvalue(child0, GET_CHILD_ADDRESS(n, 0), &tmp3, is_control_dependent, ctx->pool));
				assert(child0 != 0);
				assert(tmp3 != 0);
				ESSL_CHECK(tmp2 = _essl_new_vector_combine_expression(ctx->pool, 2));
				assert(tmp3->hdr.type != 0);
				SET_CHILD(tmp2, 0, tmp);
				SET_CHILD(tmp2, 1, tmp3);
				ESSL_CHECK(tmp2->hdr.type = _essl_get_unqualified_type(ctx->pool, tmp3->hdr.type));
				tmp2->expr.u.combiner = invert_combiner(combiner_from_swizzle(rev));
				return rewrite_assignment(ctx, child0, tmp2, is_control_dependent);
			}

		}
		break;
	case EXPR_KIND_BINARY:
		child0 = GET_CHILD(n, 0);
		child1 = GET_CHILD(n, 1);
		assert(child0 != 0);
		assert(child1 != 0);
		assert(child0->hdr.type != 0);
		if(n->expr.operation == EXPR_OP_INDEX && child0->hdr.type->child_type == NULL)
		{
			/* vector index */
			node *tmp2 = 0, *tmp3 = 0, *index = child1;
			ESSL_CHECK(_essl_middle_split_lvalue(child0, GET_CHILD_ADDRESS(n, 0), &tmp3, is_control_dependent, ctx->pool));
			ESSL_CHECK(tmp2 = _essl_new_ternary_expression(ctx->pool, EXPR_OP_SUBVECTOR_UPDATE, index, rvalue, tmp3));
			_essl_ensure_compatible_node(tmp2, tmp3);
			return rewrite_assignment(ctx, child0, tmp2, is_control_dependent);
		}
		break;
	default:
		break;

	}

	ESSL_CHECK(make_basic_blocks_expr_p(&n, ctx, 1));

	ESSL_CHECK(make_basic_blocks_expr_p(&rvalue, ctx, 0));

	/* If a struct, must deal with the individual members, recursively */
	if(n->hdr.type->basic_type == TYPE_ARRAY_OF)
	{
		return store_array_matrix_variable(ctx, n, rvalue, n->hdr.type->u.array_size, is_control_dependent);

	} else if(n->hdr.type->basic_type == TYPE_STRUCT)
	{
			
		return handle_struct_assignment(ctx, n, rvalue, is_control_dependent);
	} else if(n->hdr.type->basic_type == TYPE_MATRIX_OF)
	{
		return store_array_matrix_variable(ctx, n, rvalue, GET_MATRIX_N_COLUMNS(n->hdr.type), is_control_dependent);
	}
	else 
	{
		if(is_control_dependent == VAR_IS_CONTROL_DEPENDENT)
		{
			storeload_list *rec;
			node *store;
			symbol *sym = _essl_symbol_for_node(n);
			ESSL_CHECK(sym);
			ESSL_CHECK(rec = _essl_mempool_alloc(ctx->pool, sizeof(*rec)));
			ESSL_CHECK(store = _essl_new_store_expression(ctx->pool, sym->address_space, n, rvalue));
			assert(GET_CHILD(store, 1) != 0);
			rec->n = store;
			rec->next = 0;
			return rec;
		}else
		{
			storeload_list *rec;
			node *assign;
			ESSL_CHECK(rec = _essl_mempool_alloc(ctx->pool, sizeof(*rec)));
			ESSL_CHECK(assign = _essl_new_assign_expression(ctx->pool, n, EXPR_OP_ASSIGN, rvalue));
			assert(GET_CHILD(assign, 1) != 0);
			rec->n = assign;
			rec->next = 0;
			return rec;

		}
	}
}


/** Is access to this variable control dependent? */
var_control_dependent _essl_is_var_ref_control_dependent(symbol *s)
{
	const type_specifier *t = s->type;
	var_control_dependent res = NO_VAR_IS_CONTROL_DEPENDENT;
	if(s->address_space == ADDRESS_SPACE_GLOBAL || 
		s->address_space == ADDRESS_SPACE_VERTEX_VARYING || 
		s->address_space == ADDRESS_SPACE_FRAGMENT_OUT)
	{
		res = VAR_IS_CONTROL_DEPENDENT;
	}else if(s->address_space == ADDRESS_SPACE_THREAD_LOCAL && 
			(_essl_is_type_control_dependent(t, s->opt.is_indexed_statically) ||s->scope == SCOPE_ARGUMENT))
	{
		res = VAR_IS_CONTROL_DEPENDENT;
	}
	return res;
}

/** Is read access to this variable performed using a LOAD? */
essl_bool _essl_is_var_ref_load(symbol *s)
{
	return _essl_is_var_ref_control_dependent(s) == VAR_IS_CONTROL_DEPENDENT || (s->address_space != ADDRESS_SPACE_THREAD_LOCAL);
}




/** 
	Recursively transform a load of a matrix/array variable into a load of its individual elements, and return the elements collected into an matrix or array constructor.
 */
/*@null@*/ static node *load_array_matrix_variable(make_basic_blocks_context* ctx, node* address, need_load_store need_ls, var_control_dependent is_control_dep)
{
	const type_specifier* t = address->hdr.type;
	node* constructor;
	unsigned i;
	const type_specifier *elem_type;
	unsigned n_elems;
	
	elem_type = address->hdr.type->child_type;
	if(t->basic_type == TYPE_ARRAY_OF)
	{
		n_elems = t->u.array_size;
	} else {
		n_elems = GET_MATRIX_N_ROWS(t);
	}

	ESSL_CHECK(constructor = _essl_new_builtin_constructor_expression(ctx->pool, n_elems));
	constructor->hdr.type = t;
	for(i = 0; i < n_elems; ++i)
	{
		node *ind, *num, *sub;
		symbol *s = _essl_symbol_for_node(address);
		ESSL_CHECK(s);
		ESSL_CHECK(num = create_index_int_constant(ctx, i));
		ESSL_CHECK(ind = _essl_new_binary_expression(ctx->pool, address, EXPR_OP_INDEX, num));
		ind->hdr.type = elem_type;
		ESSL_CHECK(sub = handle_variable_explicitly(ctx, ind, need_ls, is_control_dep));
		SET_CHILD(constructor, i, sub);
	}
	return constructor;
}

/** 
	helper function for handle_variable_explicitly: transform a struct access into an access of each member + a struct constructor.
 */
static /*@null@*/ node *handle_struct_variable(make_basic_blocks_context* ctx, node* address, need_load_store need_ls, var_control_dependent is_control_dep)
{
	const type_specifier* t = address->hdr.type;
	single_declarator *member = t->members;
	node* constructor;
	ESSL_CHECK(constructor = _essl_new_struct_constructor_expression(ctx->pool, 0));
	_essl_ensure_compatible_node(constructor, address);
	while(0 != member)
	{
		node *ind;
		node *sub;
		ESSL_CHECK(ind = _essl_new_unary_expression(ctx->pool, EXPR_OP_MEMBER, address));
		ind->expr.u.member = member;
		ind->hdr.type = member->type;
		ESSL_CHECK(sub = handle_variable_explicitly(ctx, ind, need_ls, is_control_dep));
		ESSL_CHECK(APPEND_CHILD(constructor, sub, ctx->pool));
		member = member->next;
	}
	return constructor;
}


/** Recursively resolve member access to a struct constructor to identify and
 * return the constructor argument associated with the member access 
 * \param:	n		Pointer to the node that is accessing the struct constructor.
 * \param:	ctx		Context
 * \return:	The child of the struct constructor that represents the member access 
 *			originally specified.
 */
/*@null@*/ static node *resolve_struct_member(make_basic_blocks_context *ctx, node *n)
{
	node* child;
	ESSL_CHECK(child = GET_CHILD(n, 0));
	if(child->hdr.kind == EXPR_KIND_UNARY && child->expr.operation == EXPR_OP_MEMBER)
	{
		child = resolve_struct_member(ctx, child);
	}
	if(child->hdr.kind == EXPR_KIND_STRUCT_CONSTRUCTOR)
	{
		return GET_CHILD(child, n->expr.u.member->index);
	} else if(child->hdr.kind == EXPR_KIND_DONT_CARE)
	{
		return _essl_new_dont_care_expression(ctx->pool, n->hdr.type);
	} else {
		assert(0);
		return NULL;
	}

}


/** Recursively resolve member access to a array/matrix constructor to identify and
 * return the constructor argument associated with the member access 
 * \param:	n		Pointer to the node that is accessing the array/matrix constructor.
 * \param:	ctx		Context
 * \return:	The child of the array/matrix constructor that represents the access 
 *			originally specified.
 */
/*@null@*/ static node *resolve_array_matrix_access(make_basic_blocks_context *ctx, node *n)
{
	node* big, *index;
	node *res;
	int idx = -1;

	assert(n->hdr.kind == EXPR_KIND_BINARY && n->expr.operation == EXPR_OP_INDEX);

	ESSL_CHECK(big = GET_CHILD(n, 0));
	ESSL_CHECK(index = GET_CHILD(n, 1));

	assert(big->hdr.type->basic_type == TYPE_ARRAY_OF || big->hdr.type->basic_type == TYPE_MATRIX_OF);


	if(big->hdr.kind == EXPR_KIND_DONT_CARE)
	{
		return _essl_new_dont_care_expression(ctx->pool, n->hdr.type);
	}

	if(index->hdr.kind == EXPR_KIND_CONSTANT)
	{
		idx = ctx->desc->scalar_to_int(index->expr.u.value[0]);
		assert(idx >= 0 && 
			   ((big->hdr.type->basic_type == TYPE_ARRAY_OF && idx < (int)big->hdr.type->u.array_size) ||
				(big->hdr.type->basic_type == TYPE_MATRIX_OF && idx < (int)GET_MATRIX_N_COLUMNS(big->hdr.type)))
				   );
	}
	
	if(idx != -1)
	{
		unsigned i;
		switch(big->hdr.kind)
		{
		case EXPR_KIND_BUILTIN_CONSTRUCTOR:
			ESSL_CHECK(res = GET_CHILD(big, idx));
			assert(_essl_type_with_scalar_size_equal(n->hdr.type, res->hdr.type));
			return res;
		case EXPR_KIND_CONSTANT:
			assert(big->hdr.type->basic_type == TYPE_MATRIX_OF);
			ESSL_CHECK(res = _essl_new_constant_expression(ctx->pool, GET_NODE_VEC_SIZE(n)));
			_essl_ensure_compatible_node(res, n);
			for(i = 0; i < GET_NODE_VEC_SIZE(n); ++i)
			{
				res->expr.u.value[i] = big->expr.u.value[idx*GET_MATRIX_N_COLUMNS(big->hdr.type) + i];
			}
			return res;
		default:
			assert(0);
		}
	} else {
		string name = {"%matrix_spill_temp", 18};
		symbol *sym;
		node *var_ref, *assign;

		ESSL_CHECK(sym = _essl_new_variable_symbol_with_default_qualifiers(ctx->pool, name, big->hdr.type, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));
		sym->opt.is_indexed_statically = ESSL_FALSE;

		ESSL_CHECK(var_ref = _essl_new_variable_reference_expression(ctx->pool, sym));
		_essl_ensure_compatible_node(var_ref, n);
		var_ref->hdr.type = big->hdr.type;
		
		ESSL_CHECK(assign = _essl_new_assign_expression(ctx->pool, var_ref, EXPR_OP_ASSIGN, big));
		_essl_ensure_compatible_node(assign, var_ref);
		ESSL_CHECK(make_basic_blocks_expr_p(&assign, ctx, 0));

		SET_CHILD(n, 0, var_ref);
		ESSL_CHECK(make_basic_blocks_expr_p(&n, ctx, 0));
		return n;
	}
	assert(0);
	return NULL;

}




/** Transform a read of a variable into a LOAD if needed, and add it
	to the list of control-dependent operations if it is
	control-dependent. This version takes explicit flags to see if we
	should treat the access as control dependent or needing
	load/store, making it useful for function argument processing
	where loads need to be inserted / handled as control dependent
	even though the variable type does not indicate it.
*/
/*@null@*/ static node *handle_variable_explicitly(make_basic_blocks_context *ctx, node *n, need_load_store need_ls, var_control_dependent is_control_dep)
{

	if(need_ls != NO_NEED_LOAD_STORE)
	{
		/* Check if the variable is a structure */

		if(n->hdr.type->basic_type == TYPE_ARRAY_OF || n->hdr.type->basic_type == TYPE_MATRIX_OF)
		{
			return load_array_matrix_variable(ctx, n, need_ls, is_control_dep);
		} else if(n->hdr.type->basic_type == TYPE_STRUCT)
		{
			return handle_struct_variable(ctx, n, need_ls, is_control_dep);
		}
		else
		{
			symbol *s = _essl_symbol_for_node(n);
			ESSL_CHECK(s);
			ESSL_CHECK(make_basic_blocks_expr_p(&n, ctx, 1));
			ESSL_CHECK(n = _essl_new_load_expression(ctx->pool, s->address_space, n));

			if(is_control_dep != NO_VAR_IS_CONTROL_DEPENDENT)
			{
				ESSL_CHECK(add_control_dependent_op(n, ctx));
			}
			return n;
		}
	} else {
		/* Check if the variable is a structure */
		if(n->hdr.type->basic_type == TYPE_ARRAY_OF || n->hdr.type->basic_type == TYPE_MATRIX_OF)
		{
			return load_array_matrix_variable(ctx, n, need_ls, is_control_dep);
		} else if(n->hdr.type->basic_type == TYPE_STRUCT)
		{
			return handle_struct_variable(ctx, n, need_ls, is_control_dep);
		}

		ESSL_CHECK(add_local_op(n, ctx));
		return n;
	}
}

/** Transform a read of a variable into a LOAD if needed, and add it
	to the list of control-dependent operations if it is
	control-dependent. This version figures out if the access needs
	load/store or is control dependent from the type of the symbol
	loaded, and is a friendlier interface to
	handle_variable_explicitly. */
/*@null@*/ static node *handle_variable(make_basic_blocks_context *ctx, node *n)
{
	need_load_store need_ls = NO_NEED_LOAD_STORE;
	var_control_dependent is_control_dep = NO_VAR_IS_CONTROL_DEPENDENT;
	symbol *s = _essl_symbol_for_node(n);
	if(s != NULL)
	{
		need_ls = _essl_is_var_ref_load( s) ? NEED_LOAD_STORE : NO_NEED_LOAD_STORE;
		is_control_dep = _essl_is_var_ref_control_dependent( s);
	}
	return handle_variable_explicitly(ctx, n, need_ls, is_control_dep);
}

	
/** Record assignments onto the control_dependent_ops or local_ops list. */
/*@null@*/ static storeload_list *record_assignment(storeload_list *n, make_basic_blocks_context *ctx, var_control_dependent is_control_dep) {	
	storeload_list *rec = n;
	while(0 != rec)
	{
		if(is_control_dep)
		{
			ESSL_CHECK(add_control_dependent_op(rec->n, ctx));
		} else {
			ESSL_CHECK(add_local_op(rec->n, ctx));
		}
		rec = rec->next;
	}	
	return n;
}

/**
   Convenience function for rewriting and recording an assignment, as this is what most of the client code needs to do.
 */
static /*@null@*/storeload_list *rewrite_and_record_assignment(make_basic_blocks_context *ctx, node *n, node *rvalue, var_control_dependent is_control_dependent)
{
	storeload_list *lst;
	ESSL_CHECK(lst = rewrite_assignment(ctx, n, rvalue, is_control_dependent));
	return record_assignment(lst, ctx, is_control_dependent);

}

/** Extract all loads in a subtree.
 *  Used to obtain a list of loads for a each function parameter needed for function inlining.
 */
static /*@null@*/storeload_list *extract_loads_from_tree(make_basic_blocks_context *ctx, node *n)
{
	storeload_list *rec = 0;
	unsigned i;
	switch(n->hdr.kind)
	{
	case EXPR_KIND_LOAD:
		ESSL_CHECK(rec = _essl_mempool_alloc(ctx->pool, sizeof(*rec)));
		rec->n = n;
		rec->next = NULL;
		break;
	case EXPR_KIND_PHI:
		assert(0);
		break;
	default:
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			storeload_list *r;
			ESSL_CHECK(r = extract_loads_from_tree(ctx, GET_CHILD(n, i)));
			LIST_INSERT_BACK(&rec, r);
		}

		break;

	}
	return rec;
}

/** Returns the operation performed (add, sub, mul or div) by an
 *  operation assignment or increment/decrement operation.
 */
static expression_operator embedded_op(expression_operator op) {
	switch (op) {
	case EXPR_OP_ADD:
	case EXPR_OP_PRE_INC:
	case EXPR_OP_POST_INC:
	case EXPR_OP_ADD_ASSIGN:
		return EXPR_OP_ADD;
	case EXPR_OP_SUB:
	case EXPR_OP_PRE_DEC:
	case EXPR_OP_POST_DEC:
	case EXPR_OP_SUB_ASSIGN:
		return EXPR_OP_SUB;
	case EXPR_OP_MUL:
	case EXPR_OP_MUL_ASSIGN:
		return EXPR_OP_MUL;
	case EXPR_OP_DIV:
	case EXPR_OP_DIV_ASSIGN:
		return EXPR_OP_DIV;
	default:
		assert(0); /* no embedded operation */
		return EXPR_OP_UNKNOWN;
	}
}

/** Is this a pre-increment or pre-decrement operation? */
static essl_bool is_pre(expression_operator op) {
	switch (op) {
	case EXPR_OP_PRE_INC:
	case EXPR_OP_PRE_DEC:
		return ESSL_TRUE;
	case EXPR_OP_POST_INC:
	case EXPR_OP_POST_DEC:
		return ESSL_FALSE;
	default:
		assert(0); /* Neither pre nor post */
		return ESSL_FALSE;
	}
}


/** Wrapper for the transformation function make_basic_blocks_expr.
 *  If a new node is produced, it is substituted for the old one.
 */ 
static node *make_basic_blocks_expr_p(nodeptr *np, make_basic_blocks_context *ctx, int is_address) {
	node *n = *np;

	/* We need to memoize the expression as an address and the same expression as a value
	   as two different entities. We use two separate dictionaries for this, indexed by
	   is_address. */
	node *nn;
	assert(n != 0);
	if((nn = _essl_ptrdict_lookup(&ctx->visited[is_address], n)) == 0)
	{
		ESSL_CHECK(nn = make_basic_blocks_expr(n, ctx, is_address));
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited[is_address], n, nn));
		if(n != nn)
		{
			ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited[is_address], nn, nn));
		}
	}
	if (nn != n) {
		_essl_ensure_compatible_node(nn, n);
		*np = nn;
	}
	assert(GET_NODE_KIND(nn->hdr.kind) != NODE_KIND_EXPRESSION || nn->hdr.type != 0); /* all expression nodes should have type after type checking */
	return nn;
}

/** Iterate the transformation over all children of the expression node. */
static memerr make_basic_blocks_expr_children(node *n, make_basic_blocks_context *ctx, int is_address) {
	unsigned i;
	for(i = 0; i < GET_N_CHILDREN(n); ++i)
	{
		node **child_addr = GET_CHILD_ADDRESS(n, i);
		if(*child_addr)
		{
			/* The first child of an address is itself an address. The others are not. */
			ESSL_CHECK(make_basic_blocks_expr_p(child_addr, ctx, i == 0 ? is_address : 0));
		}
	}
	return MEM_OK;
}


static memerr append_child_to_combiner(mempool *pool, node *cmb, node *in, unsigned start_offset, unsigned len)
{
	unsigned i;
	node *swz = NULL;
	unsigned child_idx = GET_N_CHILDREN(cmb);
	node *assign = NULL;
	if(start_offset == 0 && _essl_get_type_size(cmb->hdr.type) == len)
	{
		/* no swizzle needed */
		assign = in;
	} else {
		ESSL_CHECK(swz = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, in));
		_essl_ensure_compatible_node(swz, cmb);
		assign = swz;
	}
	for(i = start_offset; i < start_offset + len; ++i)
	{
		if(swz != NULL)
		{
			swz->expr.u.swizzle.indices[i] = i - start_offset;
		}
		cmb->expr.u.combiner.mask[i] = child_idx;
	}
	ESSL_CHECK(APPEND_CHILD(cmb, assign, pool));
	return MEM_OK;
}


/** 
	Helper function for processing constructor. Creates nodes to grab
	the floats at [offset, offset+ESSL_MIN(*slicelen, argsize - offset))
	from the argument and returns them. Somewhat similar to
	arg[offset:offset + slicelen] in python.
*/
/*@null@*/ static node *create_slice_of_arg(make_basic_blocks_context *ctx, unsigned *slicelen, unsigned offset, node *arg)
{
	unsigned argsize = _essl_get_type_size(arg->hdr.type);
	assert(offset < argsize);
	*slicelen = ESSL_MIN(*slicelen, argsize - offset);

	if(arg->hdr.type->basic_type != TYPE_MATRIX_OF)
	{
		if(argsize == *slicelen)
		{
			assert(offset == 0);
			return arg;
		} else {
			/* vector slice, can be done with a swizzle */
			node *res;
			unsigned i;
			ESSL_CHECK(res = _essl_new_unary_expression(ctx->pool, EXPR_OP_SWIZZLE, arg));
			assert(*slicelen <= N_COMPONENTS);

			ESSL_CHECK(res->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, arg->hdr.type, *slicelen));
			for(i = 0; i < *slicelen; ++i)
			{
				res->expr.u.swizzle.indices[i] = (char)(i+offset);
			}
			return res;
		}
	} else {
		/* matrix type */
		const type_specifier *start_type = arg->hdr.type->child_type;
		int n_wanted = *slicelen;
		node *res = NULL;
		unsigned combiner_offset = 0;
		unsigned idx = offset / GET_MATRIX_N_ROWS(arg->hdr.type);
		offset = offset % GET_MATRIX_N_ROWS(arg->hdr.type);
		assert(n_wanted <= 4);

		ESSL_CHECK(res = _essl_new_vector_combine_expression(ctx->pool, 0));
		_essl_ensure_compatible_node(res, arg);
		ESSL_CHECK(res->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, arg->hdr.type->child_type, n_wanted));

		while(n_wanted > 0)
		{
			unsigned n_wanted_in_this_column = ESSL_MIN((unsigned)n_wanted, GET_MATRIX_N_ROWS(arg->hdr.type) - offset);
			node *num;
			node *index;
			node *column;
			assert(idx < GET_MATRIX_N_COLUMNS(arg->hdr.type));
			ESSL_CHECK(num = create_index_int_constant(ctx, idx));
			ESSL_CHECK(index = _essl_new_binary_expression(ctx->pool, arg, EXPR_OP_INDEX, num));
			index->hdr.type = arg->hdr.type->child_type;
			ESSL_CHECK(index = make_basic_blocks_expr_p(&index, ctx, 0));
			ESSL_CHECK(column = create_slice_of_arg(ctx, &n_wanted_in_this_column, offset, index));

			if(!_essl_type_scalar_part_equal(start_type, column->hdr.type))
			{
				node *conv;
				ESSL_CHECK(conv = _essl_new_type_convert_expression(ctx->pool, EXPR_OP_EXPLICIT_TYPE_CONVERT, column));
				_essl_ensure_compatible_node(conv, arg);
				ESSL_CHECK(conv->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, start_type, GET_NODE_VEC_SIZE(column)));
				
				column = conv;
			}
			
			ESSL_CHECK(append_child_to_combiner(ctx->pool, res, column, combiner_offset, n_wanted_in_this_column));
			combiner_offset += n_wanted_in_this_column;

			offset = 0;
			++idx;
			n_wanted -= n_wanted_in_this_column;

		}
		
		if(GET_N_CHILDREN(res) == 1)
		{
			return GET_CHILD(res, 0);
		}
		return res;
	}



}

/** 
	Helper function for processing vector constructors.
*/
/*@null@*/ static node *create_vector_constructor_from_arglist(make_basic_blocks_context *ctx, unsigned veclen, unsigned *arg_offset, unsigned *elem_offset, node *args)
{
	node *res = NULL;
	unsigned n_wanted = veclen;
	const type_specifier *start_type;
	unsigned combiner_offset = 0;
	assert(veclen <= 4);

	ESSL_CHECK(res = _essl_new_vector_combine_expression(ctx->pool, 0));
	_essl_ensure_compatible_node(res, args);

	start_type = args->hdr.type;
	if(start_type->basic_type == TYPE_MATRIX_OF)
	{
		start_type = start_type->child_type;
	}
	ESSL_CHECK(res->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, start_type, veclen));

	while(*arg_offset < GET_N_CHILDREN(args) && n_wanted != 0)
	{
		unsigned n_wanted_from_this_child = n_wanted;
		node *child;
		node *slice;
		ESSL_CHECK(child = GET_CHILD(args, *arg_offset));
		ESSL_CHECK(slice = create_slice_of_arg(ctx, &n_wanted_from_this_child, *elem_offset, child));
		if(!_essl_type_scalar_part_equal(start_type, slice->hdr.type))
		{
			node *conv;
			ESSL_CHECK(conv = _essl_new_type_convert_expression(ctx->pool, EXPR_OP_EXPLICIT_TYPE_CONVERT, slice));
			_essl_ensure_compatible_node(conv, args);
			ESSL_CHECK(conv->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, start_type, GET_NODE_VEC_SIZE(slice)));

			slice = conv;
		}

		ESSL_CHECK(append_child_to_combiner(ctx->pool, res, slice, combiner_offset, n_wanted_from_this_child));

		combiner_offset += n_wanted_from_this_child;

		*elem_offset += n_wanted_from_this_child;
		n_wanted -= n_wanted_from_this_child;
		if(*elem_offset == _essl_get_type_size(child->hdr.type))
		{
			++*arg_offset;
			*elem_offset = 0;
		}

	}
	assert(n_wanted == 0);

	if(GET_N_CHILDREN(res) == 1)
	{
		return GET_CHILD(res, 0);
	}
	return res;
	
}

/** 
	Helper function for processing matrix constructors.
*/
/*@null@*/ static node *create_matrix_constructor_from_arglist(make_basic_blocks_context *ctx, node *args)
{
	node *res;
	unsigned i;
	unsigned arg_offset = 0;
	unsigned elem_offset = 0;
	ESSL_CHECK(res = _essl_new_builtin_constructor_expression(ctx->pool, GET_MATRIX_N_COLUMNS(args->hdr.type)));
	_essl_ensure_compatible_node(res, args);
	if(GET_N_CHILDREN(args) == 1 && GET_CHILD(args, 0)->hdr.type->basic_type == TYPE_MATRIX_OF)
	{
		node *source_matrix = GET_CHILD(args, 0);
		unsigned source_n_columns = GET_MATRIX_N_COLUMNS(source_matrix->hdr.type);
		unsigned source_n_rows = GET_MATRIX_N_ROWS(source_matrix->hdr.type);
		unsigned res_n_columns = GET_MATRIX_N_COLUMNS(res->hdr.type);
		unsigned res_n_rows = GET_MATRIX_N_ROWS(res->hdr.type);
		for(i = 0; i < res_n_columns; ++i)
		{
			node *column;
			if(i < source_n_columns)
			{
				node *mat_column;
				unsigned min_n_rows = ESSL_MIN(source_n_rows, res_n_rows);
				arg_offset = 0;
				elem_offset = i*source_n_rows;
				ESSL_CHECK(mat_column = create_vector_constructor_from_arglist(ctx, min_n_rows, &arg_offset, &elem_offset, args));
				assert(GET_NODE_VEC_SIZE(mat_column) == min_n_rows);
				if(source_n_rows < res_n_rows)
				{
					node *constant;
					ESSL_CHECK(constant = create_float_constant(ctx, 0.0, res_n_rows - source_n_rows, _essl_get_scalar_size_for_type(args->hdr.type)));
					if(i >= source_n_rows)
					{
						constant->expr.u.value[i - source_n_rows] = ctx->desc->float_to_scalar(1.0);
					}
					ESSL_CHECK(column = _essl_create_vector_combine_for_nodes(ctx->pool, ctx->typestor_context, mat_column, constant, args));

				} else {
					column = mat_column;
				}
			} else {
				ESSL_CHECK(column = create_float_constant(ctx, 0.0, res_n_columns, _essl_get_scalar_size_for_type(args->hdr.type)));
				
				column->expr.u.value[i] = ctx->desc->float_to_scalar(1.0);
			}
			
			SET_CHILD(res, i, column);
		}

		
	} else {

		for(i = 0; i < GET_MATRIX_N_COLUMNS(args->hdr.type); ++i)
		{
			node *column;
			ESSL_CHECK(column = create_vector_constructor_from_arglist(ctx, GET_MATRIX_N_ROWS(args->hdr.type), &arg_offset, &elem_offset, args));
			
			SET_CHILD(res, i, column);
		}
	}
	return res;
}





/** Traverse the tree rooted at expression n and create basic blocks.
 *
 *  Returns the transformed tree.
 */
static node *make_basic_blocks_expr(node *n, make_basic_blocks_context *ctx, int is_address) {

	node *first, *second, *third, *fourth;
	node *result;
	basic_block *first_block, *second_block, *third_block;
	symbol *sym;
	essl_bool is_pre_op = ESSL_FALSE;
	node *child0, *child1;
	switch (n->hdr.kind) {

	case EXPR_KIND_UNARY:
		child0 = GET_CHILD(n, 0);
		assert(child0 != 0);
		switch (n->expr.operation) {

			/* Rewrite pre/post-increment/decrement into operator and assignment */
		case EXPR_OP_PRE_INC:
		case EXPR_OP_POST_INC:
		case EXPR_OP_PRE_DEC:
		case EXPR_OP_POST_DEC:
			is_pre_op = is_pre(n->expr.operation);
			ESSL_CHECK(sym = _essl_symbol_for_node(n));
			
			ESSL_CHECK(_essl_middle_split_lvalue(child0, &first, &second, _essl_is_var_ref_control_dependent( sym), ctx->pool));
			assert(first->hdr.type != 0);
			ESSL_CHECK(third = create_one_constant(first->hdr.type, ctx));
			ESSL_CHECK(fourth = _essl_new_binary_expression(ctx->pool, second, embedded_op(n->expr.operation), third));
			_essl_ensure_compatible_node(fourth, second);
			ESSL_CHECK(rewrite_and_record_assignment(ctx, first, fourth, _essl_is_var_ref_control_dependent( sym)));

			/* Get transformed rhs */
			result = _essl_ptrdict_lookup(&ctx->visited[MBB_VISITED_VALUE], fourth);
			assert(result->hdr.kind == EXPR_KIND_BINARY && result->expr.operation == embedded_op(n->expr.operation));
			if(is_pre_op)
			{
				return result;
			} else {
				return GET_CHILD(result, 0);
			}
		case EXPR_OP_MEMBER:
			/* rvalue member expression. should be a load, since local scalar structs have been dissolved by now */
			if(!is_address)
			{
				/* Check if it's a member access ultimately to a struct constructor.
				 * If so, perform the load on the appropriate constructor argument
				 */
				node *top;
				nodeptr *child;
				sym = _essl_symbol_for_node(n);
				if(sym == 0 || _essl_is_var_ref_control_dependent( sym) == NO_VAR_IS_CONTROL_DEPENDENT)
				{
					ESSL_CHECK(child = GET_CHILD_ADDRESS(n,0));
					ESSL_CHECK(make_basic_blocks_expr_p(child, ctx, is_address));
					top = n;
					while(top->hdr.kind == EXPR_KIND_UNARY && top->expr.operation == EXPR_OP_MEMBER)
					{
						top = GET_CHILD(top, 0);
					}

					if(top->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE)
					{
						n = resolve_struct_member(ctx, n);
					} else 
					{
						ESSL_CHECK(n = handle_variable(ctx, n));
					}
				}
				else
				{
					ESSL_CHECK(n = handle_variable(ctx, n));
				}
				return n;
			}
			break;
		case EXPR_OP_PLUS:
			/* unary plus is useless, strip it away */
			ESSL_CHECK(make_basic_blocks_expr_children(n, ctx, is_address));
			return GET_CHILD(n, 0);

			
		default:
			/* Only unary expressions requiring special handling are
			 * pre/post-increment/decrment
			 */
			break;
		}
		/* end of case EXPR_KIND_UNARY */
		break;

	case EXPR_KIND_BINARY:
		child0 = GET_CHILD(n, 0);
		child1 = GET_CHILD(n, 1);
		assert(child0 != 0);
		assert(child1 != 0);
		switch (n->expr.operation) {

			/* Categorise index expression as array or vector indexing */
		case EXPR_OP_INDEX:
			if(!is_address)
			{
				sym = _essl_symbol_for_node(n);
				assert(child0->hdr.type != 0);
				if(child0->hdr.type->child_type == NULL)
				{
					ESSL_CHECK(make_basic_blocks_expr_children(n, ctx, is_address));

					if(child1->hdr.kind == EXPR_KIND_CONSTANT)
					{
						/* turn index with a constant into a swizzle - this is more efficient */
						int idx = ctx->desc->scalar_to_int(child1->expr.u.value[0]);
						assert(idx >= 0 && idx < (int)GET_NODE_VEC_SIZE(child0));
						n->hdr.kind = EXPR_KIND_UNARY;
						n->expr.operation = EXPR_OP_SWIZZLE;
						ESSL_CHECK(SET_N_CHILDREN(n, 1, ctx->pool));
						n->expr.u.swizzle = _essl_create_scalar_swizzle(idx);
						
					} else {
						n->expr.operation = EXPR_OP_SUBVECTOR_ACCESS;
					}
					return n;
						
				} else {
					/* indexing an array or a matrix, not just a vector */
					if(sym != 0 && ctx->desc->is_variable_in_indexable_memory(sym))
					{
						ESSL_CHECK(make_basic_blocks_expr_p(GET_CHILD_ADDRESS(n, 0), ctx, 1));
						ESSL_CHECK(make_basic_blocks_expr_p(GET_CHILD_ADDRESS(n, 1), ctx, 0));
						ESSL_CHECK(n = handle_variable(ctx, n));
						return n;
					} else 
					{
						ESSL_CHECK(make_basic_blocks_expr_p(GET_CHILD_ADDRESS(n, 0), ctx, 0));
						ESSL_CHECK(make_basic_blocks_expr_p(GET_CHILD_ADDRESS(n, 1), ctx, 0));
						return resolve_array_matrix_access(ctx, n);

					}
				}
			}
			break; /* handle like a regular operation */


			/* dissolve chain expressions. If the first expression has no side effects, it will disappear */
		case EXPR_OP_CHAIN:
			ESSL_CHECK(make_basic_blocks_expr_p(GET_CHILD_ADDRESS(n, 0), ctx, 0));
			ESSL_CHECK(child0);
			ESSL_CHECK(second = make_basic_blocks_expr_p(GET_CHILD_ADDRESS(n, 1), ctx, is_address));
			return second;

			/* Rewrite lazy boolean combinators in value context into a selection between
			 * true or false, putting the condition in condition context.
			 */
		case EXPR_OP_LOGICAL_AND:
		case EXPR_OP_LOGICAL_OR:
			/* Allocate blocks for branches and continuation */
			ESSL_CHECK(first_block = _essl_new_basic_block(ctx->pool));
			ESSL_CHECK(second_block = _essl_new_basic_block(ctx->pool));
			ESSL_CHECK(third_block = _essl_new_basic_block(ctx->pool));
			{
				symbol *sym;
				node *ref;
				string varname = {"%and_or_tmp", 11};
				float saved_block_cost;
				ESSL_CHECK(sym = _essl_new_variable_symbol_with_default_qualifiers(ctx->pool, varname, n->hdr.type, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));

				/* Output the blocks */
				saved_block_cost = ctx->current_block_cost;
				ESSL_CHECK(make_basic_blocks_cond(n, ctx, first_block, second_block));
				ctx->current_block_cost = saved_block_cost*IF_COST_MULTIPLIER;
				start_new_block(first_block, ctx);

				ESSL_CHECK(ref = _essl_new_variable_reference_expression(ctx->pool, sym));
				ref->hdr.type = sym->type;
				ESSL_CHECK(first = create_bool_constant(1, ctx));
				ESSL_CHECK(rewrite_and_record_assignment(ctx, ref, first, _essl_is_var_ref_control_dependent( sym)));

				term_block_jump(ctx, third_block);


				ctx->current_block_cost = saved_block_cost*IF_COST_MULTIPLIER;
				start_new_block(second_block, ctx);

				ESSL_CHECK(ref = _essl_new_variable_reference_expression(ctx->pool, sym));
				ref->hdr.type = sym->type;
				ESSL_CHECK(second = create_bool_constant(0, ctx));
				ESSL_CHECK(rewrite_and_record_assignment(ctx, ref, second, _essl_is_var_ref_control_dependent( sym)));

				term_block_jump(ctx, third_block);

				ctx->current_block_cost = saved_block_cost;
				start_new_block(third_block, ctx);


				ESSL_CHECK(third = _essl_new_variable_reference_expression(ctx->pool, sym));
				third->hdr.type = sym->type;
				ESSL_CHECK(third = handle_variable(ctx, third));

				/* Transform the node into a transfer node for the result */
				_essl_rewrite_node_to_transfer(n, third);

				return n;

			}




		default:
			/* Only binary expressions requiring special handling are
			 * index
			 * and/or
			 */
			break;
		}
		/* end of case EXPR_KIND_BINARY */
		break;

	case EXPR_KIND_ASSIGN:
		child0 = GET_CHILD(n, 0);
		child1 = GET_CHILD(n, 1);
		assert(child0 != 0);
		assert(child1 != 0);
		switch (n->expr.operation) {

			/* Note assignment as a local operation or a store */
		case EXPR_OP_ASSIGN:
			ESSL_CHECK(sym = _essl_symbol_for_node(n));
			ESSL_CHECK(rewrite_and_record_assignment(ctx, child0, child1, _essl_is_var_ref_control_dependent( sym)));

			/* Get transformed rhs */
			if(is_address)
			{
				result = _essl_ptrdict_lookup(&ctx->visited[MBB_VISITED_ADDRESS], child0);
			} else {
				result = _essl_ptrdict_lookup(&ctx->visited[MBB_VISITED_VALUE], child1);
			}

			assert(result);
			return result;

		case EXPR_OP_ADD_ASSIGN:
		case EXPR_OP_SUB_ASSIGN:
		case EXPR_OP_MUL_ASSIGN:
		case EXPR_OP_DIV_ASSIGN:			
			assert(0); /* should have been transformed in eliminate_complex_ops */
			break;

		default:
			assert(0); /* There should be no other assignment operators */
			break;
		}
		/* end of case EXPR_OP_ASSIGN */
		break;

	case EXPR_KIND_TERNARY:
		child0 = GET_CHILD(n, 0);
		assert(child0 != 0);
		assert(GET_CHILD(n, 1) != 0);
		assert(GET_CHILD(n, 2) != 0);
		switch(n->expr.operation) {

			/* Produce blocks for true and false branches of the conditional,
			 * make the condition select between them and join the results
			 * with a temporary variable.
			 */
		case EXPR_OP_CONDITIONAL:
			/* Allocate blocks for branches and continuation */
			ESSL_CHECK(first_block = _essl_new_basic_block(ctx->pool));
			ESSL_CHECK(second_block = _essl_new_basic_block(ctx->pool));
			ESSL_CHECK(third_block = _essl_new_basic_block(ctx->pool));
			{
				symbol *sym;
				node *ref;
				string varname = {"%ternary_tmp", 12};
				float saved_block_cost;
				ESSL_CHECK(sym = _essl_new_variable_symbol_with_default_qualifiers(ctx->pool, varname, n->hdr.type, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));
				/* Output the blocks and contents */
				saved_block_cost = ctx->current_block_cost;
				ESSL_CHECK(make_basic_blocks_cond(child0, ctx, first_block, second_block));
				ctx->current_block_cost = saved_block_cost*IF_COST_MULTIPLIER;
				start_new_block(first_block, ctx);
				ESSL_CHECK(first = make_basic_blocks_expr_p(GET_CHILD_ADDRESS(n, 1), ctx, 0));

				ESSL_CHECK(ref = _essl_new_variable_reference_expression(ctx->pool, sym));
				ref->hdr.type = sym->type;
				ESSL_CHECK(rewrite_and_record_assignment(ctx, ref, first, _essl_is_var_ref_control_dependent( sym)));
					  

				term_block_jump(ctx, third_block);

				ctx->current_block_cost = saved_block_cost*IF_COST_MULTIPLIER;
				start_new_block(second_block, ctx);
				ESSL_CHECK(second = make_basic_blocks_expr_p(GET_CHILD_ADDRESS(n, 2), ctx, 0));

				ESSL_CHECK(ref = _essl_new_variable_reference_expression(ctx->pool, sym));
				ref->hdr.type = sym->type;
				ESSL_CHECK(rewrite_and_record_assignment(ctx, ref, second, _essl_is_var_ref_control_dependent( sym)));

				term_block_jump(ctx, third_block);

				ctx->current_block_cost = saved_block_cost;
				start_new_block(third_block, ctx);

				ESSL_CHECK(third = _essl_new_variable_reference_expression(ctx->pool, sym));
				third->hdr.type = sym->type;
				ESSL_CHECK(third = handle_variable(ctx, third));
			}
			return third;

		default:
			/* don't care about the rest of them */
			break;
		}
		/* end of case EXPR_KIND_TERNARY */
		break;

		/* Note variable reference as a local operation or a load */
	case EXPR_KIND_VARIABLE_REFERENCE:
		if(!is_address)
		{
			ESSL_CHECK(n = handle_variable(ctx, n));
		}
		return n;

		/* Note function call as a control-dependent operation */
	case EXPR_KIND_FUNCTION_CALL:
	    {
			struct {
				node *lvalue;
				node *var_ref;
			} *args;
			unsigned i;
			single_declarator *formal_param;
			unsigned n_args = GET_N_CHILDREN(n);
			parameter *firstarg = 0, *prevarg = 0;
			parameter *arg = 0;
			symbol *varsym;
			single_declarator *reversed_param_list;
			parameter *reversed_args_list;


			ESSL_CHECK(args = _essl_mempool_alloc(ctx->pool, sizeof(*args) * n_args));

			/* evaluate and split arguments */
			for(i = 0, formal_param = n->expr.u.fun.sym->parameters; i < n_args; ++i, formal_param = formal_param->next)
			{
				char buf[16];
				string name;
				node *rvalue;
				node *child = GET_CHILD(n, i);
				assert(child != 0);
				(void)snprintf(buf, 16, "argument%u", i+1);
				buf[15] = '\0';
				name = _essl_cstring_to_string(ctx->pool, buf);
				ESSL_CHECK(name.ptr);
				ESSL_CHECK(arg = LIST_NEW(ctx->pool, parameter));
				if(prevarg)
				{
					prevarg->next = arg;
				} else {
					firstarg = arg;
				}
				prevarg = arg;
				

				assert(formal_param != 0);
				assert(child->hdr.type != 0);
				ESSL_CHECK(arg->sym = _essl_new_variable_symbol(ctx->pool, name, child->hdr.type, formal_param->qualifier, SCOPE_ARGUMENT, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));
				arg->sym->opt.is_indexed_statically = ESSL_FALSE;
				rvalue = child;
				if(formal_param->qualifier.direction & DIR_OUT)
				{
					ESSL_CHECK(varsym = _essl_symbol_for_node(child));
					ESSL_CHECK(_essl_middle_split_lvalue(child, &args[i].lvalue, &rvalue, _essl_is_var_ref_control_dependent( varsym), ctx->pool));
				}

				ESSL_CHECK(args[i].var_ref = _essl_new_variable_reference_expression(ctx->pool, arg->sym));
				args[i].var_ref->hdr.type = rvalue->hdr.type;

				if(formal_param->qualifier.direction & DIR_IN)
				{
					ESSL_CHECK(arg->store = rewrite_assignment(ctx, args[i].var_ref, rvalue, _essl_is_var_ref_control_dependent( arg->sym)));
				} else {
					/* evaluate the lvalue before the function call */
					ESSL_CHECK(make_basic_blocks_expr_p(&args[i].lvalue, ctx, 1));
				}
			}

			n->expr.u.fun.arguments = firstarg;
			/* store in arguments into memory */
			for(arg = n->expr.u.fun.arguments; arg != 0; arg = arg->next)
			{
				if(arg->store)
				{
					ESSL_CHECK(arg->store = record_assignment(arg->store, ctx, _essl_is_var_ref_control_dependent( arg->sym)));
				}
			}


			/* the function call itself */
			ESSL_CHECK(add_control_dependent_op(n, ctx));

			
			reversed_param_list = LIST_REVERSE(n->expr.u.fun.sym->parameters, single_declarator);
			reversed_args_list = LIST_REVERSE(n->expr.u.fun.arguments, parameter);
			/* load out variables back from memory */
			for(i = n_args, formal_param = reversed_param_list, arg = reversed_args_list; i != 0; i--, formal_param = formal_param->next, arg = arg->next)
			{
				assert(formal_param != 0);
				if(formal_param->qualifier.direction & DIR_OUT)
				{
					node *load;
					ESSL_CHECK(varsym = _essl_symbol_for_node(args[i-1].lvalue));
					ESSL_CHECK(rewrite_and_record_assignment(ctx, args[i-1].lvalue, args[i-1].var_ref, _essl_is_var_ref_control_dependent( varsym)));
					/* Record the rhs of the assign's as the loads for the parameters, this allows direct rewiring during inlining */
					ESSL_CHECK(load = _essl_ptrdict_lookup(&ctx->visited[MBB_VISITED_VALUE], args[i-1].var_ref));
					ESSL_CHECK(arg->load = extract_loads_from_tree(ctx, load));
				}
			}
						
			n->expr.u.fun.sym->parameters = LIST_REVERSE(reversed_param_list, single_declarator);
			n->expr.u.fun.arguments = LIST_REVERSE(reversed_args_list, parameter);
			ESSL_CHECK(SET_N_CHILDREN(n, 0, ctx->pool));

			return n;
		}


	case EXPR_KIND_BUILTIN_CONSTRUCTOR:


		ESSL_CHECK(make_basic_blocks_expr_children(n, ctx, is_address));
		if(n->hdr.type->basic_type == TYPE_ARRAY_OF)
		{
			/* array constructor, all cool */
			return n;
		}

		
		if(GET_N_CHILDREN(n) == 1 && (child0 = GET_CHILD(n, 0)) != 0 && child0->hdr.type != 0 && _essl_get_type_size(child0->hdr.type) == 1  && n->hdr.type != 0)
		{
			/* scalar -> vector/matrix, rewrite */
			node *scalar = GET_CHILD(n, 0);
			if(n->hdr.type->basic_type == TYPE_MATRIX_OF)
			{
				unsigned i, j;
				node *zero;
				unsigned n_rows = GET_MATRIX_N_ROWS(n->hdr.type);
				unsigned n_columns = GET_MATRIX_N_COLUMNS(n->hdr.type);

				if(!_essl_type_scalar_part_equal(n->hdr.type->child_type, scalar->hdr.type))
				{
					node *conv;
					ESSL_CHECK(conv = _essl_new_type_convert_expression(ctx->pool, EXPR_OP_EXPLICIT_TYPE_CONVERT, scalar));
					_essl_ensure_compatible_node(conv, n);
					ESSL_CHECK(conv->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, n->hdr.type->child_type, 1));
					scalar = conv;
				}
				
				ESSL_CHECK(zero = _essl_new_constant_expression(ctx->pool, 1));
				zero->expr.u.value[0] = ctx->desc->float_to_scalar(0.0);
				ESSL_CHECK(zero->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, n->hdr.type->child_type, 1));
				
				ESSL_CHECK(SET_N_CHILDREN(n, _essl_get_type_size(n->hdr.type), ctx->pool));
				for(i = 0; i < n_columns; ++i)
				{
					for(j = 0; j < n_rows; ++j)
					{
						if(i == j) /* on the diagonal */
						{
							SET_CHILD(n, i*n_rows+j, scalar);
						} else {
							SET_CHILD(n, i*n_rows+j, zero);
						}

					}
				}


			} else if(GET_NODE_VEC_SIZE(n) > 1) 
			{
				unsigned i;

				unsigned len = GET_NODE_VEC_SIZE(n);
				
				if(!_essl_type_scalar_part_equal(n->hdr.type, scalar->hdr.type))
				{
					node *conv;
					ESSL_CHECK(conv = _essl_new_type_convert_expression(ctx->pool, EXPR_OP_EXPLICIT_TYPE_CONVERT, scalar));
					_essl_ensure_compatible_node(conv, n);
					ESSL_CHECK(conv->hdr.type = _essl_get_type_with_given_vec_size(ctx->typestor_context, n->hdr.type, 1));
					scalar = conv;
				}

				/* scalar -> vector, rewrite to swizzle */
				n->hdr.kind = EXPR_KIND_UNARY;
				n->expr.operation = EXPR_OP_SWIZZLE;
				n->expr.u.swizzle = _essl_create_undef_swizzle();
				for(i = 0; i < len; ++i)
				{
					n->expr.u.swizzle.indices[i] = 0;
				}
				SET_CHILD(n, 0, scalar);
				return n;
			}

		}

	

		if(n->hdr.type->basic_type == TYPE_MATRIX_OF)
		{
			ESSL_CHECK(n = create_matrix_constructor_from_arglist(ctx, n));


		} else {
			unsigned arg_offset = 0;
			unsigned elem_offset = 0;
			ESSL_CHECK(n = create_vector_constructor_from_arglist(ctx, GET_NODE_VEC_SIZE(n), &arg_offset, &elem_offset, n));
			

		}
		assert(n->hdr.type->basic_type != TYPE_MATRIX_OF || GET_MATRIX_N_COLUMNS(n->hdr.type) == GET_N_CHILDREN(n));
		return n;
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		{
			if(_essl_node_is_texture_operation(n) ||
			   n->expr.operation == EXPR_OP_FUN_DFDX ||
			   n->expr.operation == EXPR_OP_FUN_DFDY ||
			   n->expr.operation == EXPR_OP_FUN_FWIDTH)
			{
				/* these operations require rendezvous, and aren't safe to move out of the original basic block */
				ESSL_CHECK(make_basic_blocks_expr_children(n, ctx, is_address));
				ESSL_CHECK(add_control_dependent_op(n, ctx));
				return n;
			}
		}
		break;
	case EXPR_KIND_DEPEND:
		ESSL_CHECK(make_basic_blocks_expr_children(n, ctx, is_address));
		ESSL_CHECK(add_control_dependent_op(n, ctx));
		return n;


	case EXPR_KIND_CONSTANT:
	case EXPR_KIND_LOAD:
	case EXPR_KIND_STORE:
		ESSL_CHECK(make_basic_blocks_expr_children(n, ctx, 1));
		return n;
	case EXPR_KIND_TRANSFER:
	case EXPR_KIND_DONT_CARE:
		break;

	case EXPR_KIND_STRUCT_CONSTRUCTOR:
		break;

	case EXPR_KIND_VECTOR_COMBINE:
		break;

	case EXPR_KIND_TYPE_CONVERT:
		break;

	default:
		assert(0); /* No other kinds should ever occur. */
		break;

	}

	/* Nothing special to do for this node. Just visit the children. */
	ESSL_CHECK(make_basic_blocks_expr_children(n, ctx, is_address));
	return n;
} /* static node *make_basic_blocks_expr */



/** Traverse the tree rooted at statement n and create basic blocks.
 *  continue_block is the block to jump to on termination
 *  break_block is the block to jump to on break
 */
static memerr make_basic_blocks_stmt(node *n, make_basic_blocks_context *ctx,
								 basic_block *continue_block, basic_block *break_block) {

	node *init, *cond, *first, *second;
	float saved_block_cost;

	if (IS_TERMINATED(ctx->current)) {
		/* This is unreachable code after a break, continue, discard or return. Ignore it. */
		return MEM_OK;
	}

	if (GET_NODE_KIND(n->hdr.kind) == NODE_KIND_EXPRESSION) {
		/* Expression as statement. Call expression transformer. */
		ESSL_CHECK(make_basic_blocks_expr_p(&n, ctx, 0));
		return MEM_OK;
	}

	switch (n->hdr.kind) {

		/* Jump to current continue block */
	case STMT_KIND_CONTINUE:
		assert(continue_block != 0);
		term_block_jump(ctx, continue_block);
		return MEM_OK;

		/* Jump to current break block */
	case STMT_KIND_BREAK:
		assert(break_block != 0);
		term_block_jump(ctx, break_block);
		return MEM_OK;

		/* Discard pixel */
	case STMT_KIND_DISCARD:
		term_block_discard(ctx);
		return MEM_OK;

		/* Return from function */
	case STMT_KIND_RETURN:
		{
			basic_block *exit_block = ctx->function->control_flow_graph->exit_block;
			assert(exit_block != 0);
			if (GET_CHILD(n, 0)) {
				/* Value return */
				node *ref;
				assert(ctx->return_value_symbol != 0);
				ESSL_CHECK(first = make_basic_blocks_expr_p(GET_CHILD_ADDRESS(n, 0), ctx, 0));

				ESSL_CHECK(ref = _essl_new_variable_reference_expression(ctx->pool, ctx->return_value_symbol));
				ref->hdr.type = ctx->return_value_symbol->type;
				ESSL_CHECK(rewrite_and_record_assignment(ctx, ref, first, _essl_is_var_ref_control_dependent( ctx->return_value_symbol)));
			}
			/* Insert a jump to the exit block */
			term_block_jump(ctx, exit_block);
			return MEM_OK;
		}

	case STMT_KIND_IF:
		assert(GET_CHILD(n, 0) != 0);
		{
			node *then_node;
			node *else_node;
			basic_block *continuation_block;
			basic_block *then_block;
			basic_block *else_block;
			cond = GET_CHILD(n, 0);
			then_node = GET_CHILD(n, 1);
			else_node = GET_CHILD(n, 2);
			if (else_node) {
				/* if () {} else {} */
				ESSL_CHECK(then_block = _essl_new_basic_block(ctx->pool)); /* true branch */
				ESSL_CHECK(else_block = _essl_new_basic_block(ctx->pool)); /* false branch */
				ESSL_CHECK(continuation_block = _essl_new_basic_block(ctx->pool)); /* continuation */
				saved_block_cost = ctx->current_block_cost;
				ESSL_CHECK(make_basic_blocks_cond(cond, ctx, then_block, else_block));
				ctx->current_block_cost = saved_block_cost*IF_COST_MULTIPLIER;
				start_new_block(then_block, ctx);
				if(then_node != 0)
				{
					ESSL_CHECK(make_basic_blocks_stmt(then_node, ctx, continue_block, break_block));
				}
				if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, continuation_block);
				ctx->current_block_cost = saved_block_cost*IF_COST_MULTIPLIER;
				start_new_block(else_block, ctx);
				ESSL_CHECK(make_basic_blocks_stmt(else_node, ctx, continue_block, break_block));
				if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, continuation_block);
				ctx->current_block_cost = saved_block_cost;
				if(continuation_block->predecessor_count > 0)
				{
					start_new_block(continuation_block, ctx);
				}
			} else { 
				/* if () {} */
				ESSL_CHECK(then_block = _essl_new_basic_block(ctx->pool)); /* true branch */
				ESSL_CHECK(continuation_block = _essl_new_basic_block(ctx->pool)); /* continuation */
				saved_block_cost = ctx->current_block_cost;
				ESSL_CHECK(make_basic_blocks_cond(cond, ctx, then_block, continuation_block));

				ctx->current_block_cost = saved_block_cost*IF_COST_MULTIPLIER;
				start_new_block(then_block, ctx);
				if(then_node != 0)
				{
					ESSL_CHECK(make_basic_blocks_stmt(then_node, ctx, continue_block, break_block));
				}
				if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, continuation_block);

				ctx->current_block_cost = saved_block_cost;
				if(continuation_block->predecessor_count > 0)
				{
					start_new_block(continuation_block, ctx);
				}
			}
		}
		return MEM_OK;

	case STMT_KIND_WHILE:
		assert(GET_CHILD(n, 0) != 0);
		assert(GET_CHILD(n, 1) != 0);
		{
			node *body;
			basic_block *body_block;
			basic_block *condition_block;
			basic_block *continuation_block;
			cond = GET_CHILD(n, 0);
			body = GET_CHILD(n, 1);
			saved_block_cost = ctx->current_block_cost;
			ESSL_CHECK(body_block = _essl_new_basic_block(ctx->pool)); /* body */
			ESSL_CHECK(condition_block = _essl_new_basic_block(ctx->pool)); /* condition */
			ESSL_CHECK(continuation_block = _essl_new_basic_block(ctx->pool)); /* continuation */
			term_block_jump(ctx, condition_block);

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			start_new_block(condition_block, ctx);
			ESSL_CHECK(make_basic_blocks_cond(cond, ctx, body_block, continuation_block));

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			start_new_block(body_block, ctx);
			/* continue jumps to condition, break jumps out of loop */
			ESSL_CHECK(make_basic_blocks_stmt(body, ctx, condition_block, continuation_block));
			if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, condition_block);

			ctx->current_block_cost = saved_block_cost;
			if(continuation_block->predecessor_count > 0) /* continuation used */
			{
				start_new_block(continuation_block, ctx);
			}
			return MEM_OK;
		}

	case STMT_KIND_DO:
		assert(GET_CHILD(n, 0) != 0);
		assert(GET_CHILD(n, 1) != 0);
		{
			node *body;
			basic_block *body_block;
			basic_block *condition_block;
			basic_block *continuation_block;
			body = GET_CHILD(n, 0);
			cond = GET_CHILD(n, 1);
			saved_block_cost = ctx->current_block_cost;
			ESSL_CHECK(body_block = _essl_new_basic_block(ctx->pool)); /* body */
			ESSL_CHECK(condition_block = _essl_new_basic_block(ctx->pool)); /* condition */
			ESSL_CHECK(continuation_block = _essl_new_basic_block(ctx->pool)); /* continuation */
			term_block_jump(ctx, body_block);

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			start_new_block(body_block, ctx);
			/* continue jumps to condition, break jumps out of loop */
			ESSL_CHECK(make_basic_blocks_stmt(body, ctx, condition_block, continuation_block));

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			if(condition_block->predecessor_count > 0) /* block is used */
			{
				if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, condition_block);

				start_new_block(condition_block, ctx);
			}

			if(!IS_TERMINATED(ctx->current))
			{
				ESSL_CHECK(make_basic_blocks_cond(cond, ctx, body_block, continuation_block));
			}


			ctx->current_block_cost = saved_block_cost;
			if(continuation_block->predecessor_count > 0) /* continuation used */
			{
				start_new_block(continuation_block, ctx);
			}
			return MEM_OK;
		}

	case STMT_KIND_COND_WHILE:
		assert(GET_CHILD(n, 0) != 0);
		assert(GET_CHILD(n, 1) != 0);
		assert(GET_CHILD(n, 2) != 0);
		{
			node *body, *pre_cond;
			basic_block *body_block;
			basic_block *condition_block;
			basic_block *continuation_block;
			pre_cond = GET_CHILD(n, 0);
			body = GET_CHILD(n, 1);
			cond = GET_CHILD(n, 2);
			saved_block_cost = ctx->current_block_cost;
			ESSL_CHECK(body_block = _essl_new_basic_block(ctx->pool)); /* body */
			ESSL_CHECK(condition_block = _essl_new_basic_block(ctx->pool)); /* condition */
			ESSL_CHECK(continuation_block = _essl_new_basic_block(ctx->pool)); /* continuation */

			ESSL_CHECK(make_basic_blocks_cond(pre_cond, ctx, body_block, continuation_block));

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			start_new_block(body_block, ctx);
			/* continue jumps to condition, break jumps out of loop */
			ESSL_CHECK(make_basic_blocks_stmt(body, ctx, condition_block, continuation_block));

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			if(condition_block->predecessor_count > 0) /* block is used */
			{
				if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, condition_block);

				start_new_block(condition_block, ctx);
			}

			if(!IS_TERMINATED(ctx->current))
			{
				ESSL_CHECK(make_basic_blocks_cond(cond, ctx, body_block, continuation_block));
			}


			ctx->current_block_cost = saved_block_cost;
			if(continuation_block->predecessor_count > 0) /* continuation used */
			{
				start_new_block(continuation_block, ctx);
			}
			return MEM_OK;
		}

	case STMT_KIND_FOR:
		assert(GET_CHILD(n, 1) != 0);
		{
			basic_block *body_block;
			basic_block *update_block;
			basic_block *condition_block;
			basic_block *continuation_block;
			node *update;
			node *body;
			init = GET_CHILD(n, 0);
			cond = GET_CHILD(n, 1);
			update = GET_CHILD(n, 2);
			body = GET_CHILD(n, 3);

			ESSL_CHECK(body_block = _essl_new_basic_block(ctx->pool)); /* body */
			ESSL_CHECK(update_block = _essl_new_basic_block(ctx->pool)); /* update */ 
			ESSL_CHECK(condition_block = _essl_new_basic_block(ctx->pool)); /* condition */
			ESSL_CHECK(continuation_block = _essl_new_basic_block(ctx->pool)); /* continuation */

			saved_block_cost = ctx->current_block_cost;

			/* Initialization cannot contain continue or break */
			if(init != 0)
			{
				ESSL_CHECK(make_basic_blocks_stmt(init, ctx, 0, 0));
			}
			term_block_jump(ctx, condition_block);

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			start_new_block(condition_block, ctx);
			ESSL_CHECK(make_basic_blocks_cond(cond, ctx, body_block, continuation_block));

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			start_new_block(body_block, ctx);
			/* continue jumps to update, break jumps out of loop */
			if(body != 0)
			{
				ESSL_CHECK(make_basic_blocks_stmt(body, ctx, update_block, continuation_block));
			}

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			if(update_block->predecessor_count > 0) /* block is used */
			{
				if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, update_block);

				start_new_block(update_block, ctx);
			}

			if(update != 0)
			{
				ESSL_CHECK(make_basic_blocks_stmt(update, ctx, 0, 0));
			}
			if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, condition_block);

			ctx->current_block_cost = saved_block_cost;
			if(continuation_block->predecessor_count > 0) /* continuation used */
			{
				start_new_block(continuation_block, ctx);
			}
		}
		return MEM_OK;

	case STMT_KIND_COND_FOR:
		assert(GET_CHILD(n, 1) != 0);
		{
			basic_block *body_block;
			basic_block *update_block;
			basic_block *condition_block;
			basic_block *continuation_block;
			node *update;
			node *body;
			node *pre_cond;
			init = GET_CHILD(n, 0);
			pre_cond = GET_CHILD(n, 1);
			body = GET_CHILD(n, 2);
			update = GET_CHILD(n, 3);
			cond = GET_CHILD(n, 4);

			ESSL_CHECK(body_block = _essl_new_basic_block(ctx->pool)); /* body */
			ESSL_CHECK(update_block = _essl_new_basic_block(ctx->pool)); /* update */ 
			ESSL_CHECK(condition_block = _essl_new_basic_block(ctx->pool)); /* condition */
			ESSL_CHECK(continuation_block = _essl_new_basic_block(ctx->pool)); /* continuation */

			saved_block_cost = ctx->current_block_cost;

			/* Initialization cannot contain continue or break */
			if(init != 0)
			{
				ESSL_CHECK(make_basic_blocks_stmt(init, ctx, 0, 0));
			}
			ESSL_CHECK(make_basic_blocks_cond(pre_cond, ctx, body_block, continuation_block));

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			start_new_block(body_block, ctx);
			/* continue jumps to update, break jumps out of loop */
			if(body != 0)
			{
				ESSL_CHECK(make_basic_blocks_stmt(body, ctx, update_block, continuation_block));
			}

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			if(update_block->predecessor_count > 0) /* block is used */
			{
				if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, update_block);

				start_new_block(update_block, ctx);
			}

			if(update != 0)
			{
				ESSL_CHECK(make_basic_blocks_stmt(update, ctx, 0, 0));
			}
			if (!IS_TERMINATED(ctx->current)) term_block_jump(ctx, condition_block);

			ctx->current_block_cost = saved_block_cost*LOOP_COST_MULTIPLIER;
			if(condition_block->predecessor_count > 0) /* block is used */
			{
				start_new_block(condition_block, ctx);
				ESSL_CHECK(make_basic_blocks_cond(cond, ctx, body_block, continuation_block));
			}

			ctx->current_block_cost = saved_block_cost;
			if(continuation_block->predecessor_count > 0) /* continuation used */
			{
				start_new_block(continuation_block, ctx);
			}
		}
		return MEM_OK;

	case STMT_KIND_COMPOUND:

		{
			unsigned k;
			for(k = 0; k < GET_N_CHILDREN(n); ++k)
			{
				if(GET_CHILD(n, k))
				{
					ESSL_CHECK(make_basic_blocks_stmt(GET_CHILD(n, k), ctx, continue_block, break_block));
				}

			}

		}
		return MEM_OK;

	case DECL_KIND_VARIABLE:
		/* Variable declaration with initializer.
		 * Rewrite into assignment. */

		if (n->decl.sym->qualifier.variable == VAR_QUAL_CONSTANT)
		{
			/* Ignore const variables. They are handled by the constant folding */
			return MEM_OK;
		}

		ESSL_CHECK(first = _essl_new_variable_reference_expression(ctx->pool, n->decl.sym));
		_essl_ensure_compatible_node(first, n);

		second = GET_CHILD(n, 0);
		if(second == 0)
		{
			if(_essl_is_var_ref_control_dependent( n->decl.sym) == NO_VAR_IS_CONTROL_DEPENDENT)
			{
				assert(n->hdr.type != 0);
				ESSL_CHECK(second = _essl_new_dont_care_expression(ctx->pool, n->hdr.type));
			}
		}
		if(second != 0)
		{
			ESSL_CHECK(rewrite_and_record_assignment(ctx, first, second, _essl_is_var_ref_control_dependent( n->decl.sym)));
		}
		
		return MEM_OK;

	case DECL_KIND_PRECISION:
		/* Ignore */
		return MEM_OK;

	default:
		assert(0); /* There should be no other statements */
		return MEM_OK;
	}
} /* static memerr make_basic_blocks_stmt */



/** Create blocks for an expression in a conditional.
 *  Returns with the current block terminated, i.e. it assumes
 *  that a new block is created right after the function returns.
 */
static memerr make_basic_blocks_cond(node *n, make_basic_blocks_context *ctx,
								  basic_block *true_block, basic_block *false_block) {
	node *first;
	node *second;
	basic_block *second_block;
	float saved_block_cost;
	assert(!IS_TERMINATED(ctx->current));
	switch (n->hdr.kind) {

	case EXPR_KIND_UNARY:
		assert(GET_CHILD(n, 0) != 0);
		switch (n->expr.operation) {

		case EXPR_OP_NOT:
			return make_basic_blocks_cond(GET_CHILD(n, 0), ctx, false_block, true_block);

		default:
			/* Only boolean negation need special treatment */
			break;
		}
		/* end of case EXPR_KIND_UNARY */
		break;

	case EXPR_KIND_BINARY:
		assert(GET_CHILD(n, 0) != 0);
		assert(GET_CHILD(n, 1) != 0);
		switch (n->expr.operation) {

			/* Produce block for second operand of boolean connective
			 * and connect the blocks.
			 */
		case EXPR_OP_LOGICAL_AND:
			ESSL_CHECK(second_block = _essl_new_basic_block(ctx->pool));
			first = GET_CHILD(n, 0);
			second = GET_CHILD(n, 1);
			ESSL_CHECK(make_basic_blocks_cond(first, ctx, second_block, false_block));
			saved_block_cost = ctx->current_block_cost;
			ctx->current_block_cost = saved_block_cost*IF_COST_MULTIPLIER;
			start_new_block(second_block, ctx);
			ESSL_CHECK(make_basic_blocks_cond(second, ctx, true_block, false_block));
			return MEM_OK;

		case EXPR_OP_LOGICAL_OR:
			ESSL_CHECK(second_block = _essl_new_basic_block(ctx->pool));
			first = GET_CHILD(n, 0);
			second = GET_CHILD(n, 1);
			ESSL_CHECK(make_basic_blocks_cond(first, ctx, true_block, second_block));
			saved_block_cost = ctx->current_block_cost;
			ctx->current_block_cost = saved_block_cost*IF_COST_MULTIPLIER;
			start_new_block(second_block, ctx);
			ESSL_CHECK(make_basic_blocks_cond(second, ctx, true_block, false_block));
			return MEM_OK;

		default:
			/* Only boolean connectives need special treatment */
			break;
		}
		/* end of case EXPR_KIND_BINARY */
		break;

	case DECL_KIND_VARIABLE:
		assert(GET_CHILD(n, 0));
		/* Variable declaration with initializer.
		 * Rewrite into assignment. */
		ESSL_CHECK(first = _essl_new_variable_reference_expression(ctx->pool, n->decl.sym));
		_essl_ensure_compatible_node(first, n);

		{
			storeload_list *rec;
			ESSL_CHECK(rec = rewrite_and_record_assignment(ctx, first, GET_CHILD(n, 0), NO_VAR_IS_CONTROL_DEPENDENT));
			/* Ensure that this is not a struct assignment, typechecking shouldn't allow it but
			 * doesn't hurt to add some more checks.
			 */
			assert(rec->next == 0 && rec->n != 0);
			term_block_conditional(ctx, GET_CHILD(rec->n, 1), true_block, false_block);
		}
		
		return MEM_OK;
		

	default:
		break;
	}

	/* Make conditional jump */
	ESSL_CHECK(first = make_basic_blocks_expr_p(&n, ctx, 0));
	term_block_conditional(ctx, first, true_block, false_block);
	return MEM_OK;
} /* static memerr make_basic_blocks_cond */



memerr _essl_make_basic_blocks_init(make_basic_blocks_context *ctx, 
				    error_context *err, typestorage_context *ts_ctx, mempool *pool, target_descriptor *desc)
{
	ctx->err = err;
	ctx->typestor_context = ts_ctx;
	ctx->pool = pool;
	ctx->desc = desc;
	ctx->function = NULL;
	ctx->current = NULL;
	ctx->next_local_p = NULL;
	ctx->next_control_dependent_p = NULL;
	ctx->current_block_cost = INITIAL_BLOCK_COST;
	ctx->return_value_symbol = NULL;
	ESSL_CHECK(_essl_ptrdict_init(&ctx->visited[MBB_VISITED_VALUE], pool));
	ESSL_CHECK(_essl_ptrdict_init(&ctx->visited[MBB_VISITED_ADDRESS], pool));
	return MEM_OK;
} /* memerr _essl_make_basic_blocks_init */



memerr _essl_make_basic_blocks(make_basic_blocks_context *ctx, symbol *function)
{
	basic_block *first_real_block;
	single_declarator *parm;
	control_flow_graph *g = _essl_mempool_alloc(ctx->pool, sizeof(control_flow_graph));
	parameter **paramptr = &g->parameters;
	parameter *p;
	storeload_list *rec;
	ctx->function = function;
	ctx->current = NULL;
	ctx->next_local_p = NULL;
	ctx->next_control_dependent_p = NULL;
	ctx->current_block_cost = INITIAL_BLOCK_COST;
	assert(function->body != NULL);
	ESSL_CHECK(g);
	function->control_flow_graph = g;
	ESSL_CHECK(_essl_ptrdict_init(&g->control_dependence, ctx->pool));
	ESSL_CHECK(g->entry_block = _essl_new_basic_block(ctx->pool));
	ESSL_CHECK(g->exit_block = _essl_new_basic_block(ctx->pool));
	start_new_block(g->entry_block, ctx);
	ctx->return_value_symbol = NULL;
	if(function->type->basic_type != TYPE_VOID)
	{
		node *var_ref;
		node *dont_care;
		string varname = {"%retval", 7};
		ESSL_CHECK(ctx->return_value_symbol = _essl_new_variable_symbol(ctx->pool, varname, function->type, function->qualifier, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET));
		ESSL_CHECK(var_ref = _essl_new_variable_reference_expression(ctx->pool, ctx->return_value_symbol));
		var_ref->hdr.type = function->type;
		ESSL_CHECK(dont_care = _essl_new_dont_care_expression(ctx->pool, function->type));
		ESSL_CHECK(rewrite_and_record_assignment(ctx, var_ref, dont_care, _essl_is_var_ref_control_dependent( ctx->return_value_symbol)));

	}
	/* load all in parameters */
	for(parm = function->parameters; parm != 0; parm = parm->next)
	{
		node *first = NULL;
		node *second = NULL;
		symbol *sym;
		/* formal parameters without a name and symbol are allowed at this point, just ignore them */
		ESSL_CHECK(*paramptr = LIST_NEW(ctx->pool, parameter));
		p = *paramptr;
		paramptr = &p->next;
		if(parm->sym != 0)
		{
			ESSL_CHECK(sym = parm->sym);
			p->sym = sym;
			
			if(_essl_is_var_ref_control_dependent( sym) == VAR_IS_CONTROL_DEPENDENT)
			{
				continue;
			}
			ESSL_CHECK(first = _essl_new_variable_reference_expression(ctx->pool, sym));
			first->hdr.type = sym->type;
			if(parm->qualifier.direction == DIR_IN || parm->qualifier.direction == DIR_INOUT)
			{
				node *tmp;
				ESSL_CHECK(tmp = _essl_new_variable_reference_expression(ctx->pool, sym));
				tmp->hdr.type = sym->type;
				ESSL_CHECK(second = handle_variable_explicitly(ctx, tmp, NEED_LOAD_STORE, parm->qualifier.direction == DIR_INOUT ? VAR_IS_CONTROL_DEPENDENT : NO_VAR_IS_CONTROL_DEPENDENT));
				ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited[MBB_VISITED_VALUE], tmp, second));
				ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited[MBB_VISITED_VALUE], second, second));

				{
					/* Extract list of loads for the parameters and record it for the function inliner */
					storeload_list *rec;
					ESSL_CHECK(rec = extract_loads_from_tree(ctx, second));
					
					LIST_INSERT_BACK(&p->load, rec);
				}
			} else {
				ESSL_CHECK(second = _essl_new_dont_care_expression(ctx->pool, sym->type));
			}
			ESSL_CHECK(rewrite_and_record_assignment(ctx, first, second, _essl_is_var_ref_control_dependent( sym)));
		}
	}
	ESSL_CHECK(first_real_block = _essl_new_basic_block(ctx->pool));
	term_block_jump(ctx, first_real_block);
	start_new_block(first_real_block, ctx);

	ESSL_CHECK(make_basic_blocks_stmt(function->body, ctx, 0, 0));

	if(!IS_TERMINATED(ctx->current))
	{
		/* if no return, create an implicit one */
		term_block_jump(ctx, g->exit_block);
	}

	assert(ctx->current_block_cost == g->entry_block->cost);
	start_new_block(g->exit_block, ctx);
	
	/* write back out parameters */
	for(p = g->parameters, parm = function->parameters; parm != 0 && p != 0; parm = parm->next, p = p->next)
	{
		node *first, *second;
		symbol *sym;
		/* formal parameters without a name and symbol are allowed at this point, just ignore them */
		if(parm->sym != 0)
		{
			if(parm->qualifier.direction == DIR_OUT || parm->qualifier.direction == DIR_INOUT)
			{
				ESSL_CHECK(sym = parm->sym);
				/* control dependent variables are written out at assignment time, we don't need to explicitly write them back */
				if(_essl_is_var_ref_control_dependent( sym))
				{
					continue;
				}
				ESSL_CHECK(first = _essl_new_variable_reference_expression(ctx->pool, sym));
				first->hdr.type = sym->type;
				ESSL_CHECK(second = _essl_new_variable_reference_expression(ctx->pool, sym));
				second->hdr.type = sym->type;
				ESSL_CHECK(rec = rewrite_assignment(ctx, first, second, VAR_IS_CONTROL_DEPENDENT));
				LIST_INSERT_BACK(&p->store, rec);
				ESSL_CHECK(record_assignment(rec, ctx, VAR_IS_CONTROL_DEPENDENT));
			}
		}
	}

	{
		node *return_value = NULL;
		if(ctx->return_value_symbol != 0)
		{
			node *ref;
			ESSL_CHECK(ref = _essl_new_variable_reference_expression(ctx->pool, ctx->return_value_symbol));
			ref->hdr.type = ctx->return_value_symbol->type;
			ESSL_CHECK(return_value = handle_variable(ctx, ref));
			
		}


		term_block_exit(ctx, return_value);
	}



	return MEM_OK;
} /* memerr _essl_make_basic_blocks */

