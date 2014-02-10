/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_NODE_H
#define COMMON_NODE_H


#include "common/essl_str.h"
#include "common/scalar.h"
#include "common/essl_common.h"
#include "common/essl_type.h"



typedef union _tag_node  node;
typedef /*@null@*/ node*  nodeptr;

struct _tag_scope;
struct _tag_symbol;
struct _tag_mempool;
struct _tag_type_specifier;
struct _tag_basic_block;
struct _tag_phi_source;
struct _tag_parameter;
struct _tag_node_extra;
struct _tag_target_descriptor;

/** Enumeration representing the different types of node which may appear in the parse tree.
 */
typedef enum
{
	NODE_KIND_MASK 				= 0xE0,

	NODE_KIND_UNKNOWN 			= 0x00,
	NODE_KIND_EXPRESSION 		= 0x20,
	NODE_KIND_STATEMENT 		= 0x40,
	NODE_KIND_DECLARATION 		= 0x60,
	NODE_KIND_TRANSLATION_UNIT 	= 0x80,/**< translation unit.
										  children are the top-level declarations. */

	EXPR_KIND_UNKNOWN			= NODE_KIND_EXPRESSION,
	EXPR_KIND_UNARY,		       /**< unary operation. operation holds kind of operation,
					   children[0] usually holds the operand */
	EXPR_KIND_BINARY, 		       /**< binary operation. operation holds kind of operation,
					   children[0] and children[1] holds the operands */
	EXPR_KIND_ASSIGN, 		       /**< binary operation that assigns to a variable. */
	EXPR_KIND_TERNARY, 		       /**< a ? b : c. a in children[0], b in children[1],
									  c in children[2] */
	EXPR_KIND_VARIABLE_REFERENCE,  /**< variable reference.
									  sym holds which variable is being referenced 
									  var_addr_offset.offset holds the offset*/
	EXPR_KIND_CONSTANT, 		   /**< constant/literal. literals are stored in value[].
									  If more than one value is needed, expand value
									  by allocating more space for the node */
	EXPR_KIND_FUNCTION_CALL, 	   /**< function call. sym holds the function to call,
									  children[0] is a collection node that holds
									  the parameter expressions */
	EXPR_KIND_BUILTIN_FUNCTION_CALL,/**< function call. operation holds the function to call,
									   children[] the parameter expressions */
	EXPR_KIND_BUILTIN_CONSTRUCTOR, /**< vec4(a, b). the constructor called is
									  inferred from the type, children[0] is a collection
									  node that holds the parameter expressions */
	EXPR_KIND_STRUCT_CONSTRUCTOR,  /**< user_defined_type(a, b).
									  the constructor called is inferred from the type,
									  children[0] is a collection node that holds
									  the parameter expressions */

	EXPR_KIND_PHI,                 /**< phi node. never emitted by the frontend */
	EXPR_KIND_DONT_CARE,           /**< don't care node. never emitted by the frontend.
									  used by the backend to flag uninitialized values */
	EXPR_KIND_TRANSFER,            /**< transfer node - result is the value of children[0].
									  never emitted by the frontend. */
	EXPR_KIND_LOAD,                /**< load the data at the address from children[0].
									  size of load is given by the type field of the node.
									  memory type in expr.u.address_space */
	EXPR_KIND_STORE,               /**< *a = b; a in children[0], b in children[1].
									  memory type in expr.u.address_space */
	EXPR_KIND_DEPEND,              /**< depend node. never emitted by the frontend, used by
									  the backend to keep track of store nodes by inserting
									  artificial data dependencies on the nodes */

	EXPR_KIND_TYPE_CONVERT,        /**< Type conversion, i.e. float to int via truncation and so forth. One child. Destination type is the type of the node, source type is the type of the child. Operation indicates whether this is an explicit or implicit conversion (for warning purposes) */


	EXPR_KIND_VECTOR_COMBINE,        /**< select individual components from children according to the combiner
									  in n->expr.u.combiner.
									      for(i=0; i < N_COMBINER; ++i)
									          result[i] = combiner.mask[i] != -1 ? children[combiner.mask[i]][i] : undef.
									  Used by SSA transformer rewriting lvalue swizzles and constructors. */

	EXPR_KIND_MALIGP2_STORE_CONSTRUCTOR, /**< MaliGP2 store constructor. Used by the MaliGP2 backend to pipe multiple values into a vector store even though the rest of the backend is scalars only */


	STMT_KIND_UNKNOWN			= NODE_KIND_STATEMENT,
	STMT_KIND_CONTINUE,            /**< continue statement */
	STMT_KIND_BREAK,               /**< break statement */
	STMT_KIND_DISCARD,             /**< discard statement */
	STMT_KIND_RETURN,              /**< return statement,
									  children[0] holds the optional expression */
	STMT_KIND_IF,         	       /**< if statement. children[0] holds the condition,
									  children[1] the then-part,
									  children[2] the optional else-part */
	STMT_KIND_WHILE,      	       /**< while statement. children[0] holds the condition,
									  children[1] the body. */
	STMT_KIND_COND_WHILE,      	   /**< pre-conditioned while statement. children[0] holds the pre-condition,
									  children[1] the body,
									  children[2] the loop condition. */
	STMT_KIND_DO,         	       /**< do-while statement. children[0] the body, children[1] holds the condition. */
	STMT_KIND_FOR,                 /**< for statement. children[0] holds the initial part,
									  children[1] the condition,
									  children[2] the increment-part,
									  children[3] the body.*/
	STMT_KIND_COND_FOR,            /**< pre-conditioned for statement. children[0] holds the initial part,
									  children[1] the pre-condition,
									  children[2] the body,
									  children[3] the increment-part,
									  children[4] the loop condition. */
	STMT_KIND_COMPOUND,   	       /**< explicit compound statement, with actual statements
									  in a collection node at children[0].
									  Used for explicit compound statements and do-while
									  to serve as a delimiter for the scope. */

	DECL_KIND_UNKNOWN			= NODE_KIND_DECLARATION,
	DECL_KIND_VARIABLE,            /**< variable declaration in sym,
									  with optional initializer given in children[0] */
	DECL_KIND_FUNCTION,            /**< function declaration or definition, only valid
									  at top-level.  Function is given in sym.
									  Parameter types and names in sym->parameters.
									  The associated compound statement is stored
									  in children[0] for definitions */
	DECL_KIND_PRECISION            /**< precision declaration. only one valid per scope,
									  uses the type field for int/float and p */
} node_kind;


/** Macro to retrieve the major kind of a node
 */
#define GET_NODE_KIND(kind) 		((kind)&NODE_KIND_MASK)



/** Enumeration representing the different types of expressions.
 */
typedef enum _tag_expression_operator {
	EXPR_OP_UNKNOWN,
	/* unary operations */
	EXPR_OP_IDENTITY,              /**< a. a in children[0]. Copies the operand, and is used for MOV in the backends */
	EXPR_OP_SPILL,                 /**< Spill range value - children[0] is original value */
	EXPR_OP_SPLIT,                 /**< Split range value - children[0] is original value */
	EXPR_OP_SCHEDULER_SPLIT,       /**< Split range value created during scheduling - children[0] is original value */

	EXPR_OP_MEMBER_OR_SWIZZLE,     /**< member dereference or swizzle before type checking.
									  name of dereference in member_string.
									  never given to the backend */
	EXPR_OP_MEMBER,                /**< a.struct_member. a in children[0],
									  struct_member in member */
	EXPR_OP_SWIZZLE,               /**< a.zyxw. a in children[0],
									  swizzle pattern in swizzle */
	EXPR_OP_NOT,                   /**< !a. a in children[0] */
	EXPR_OP_PRE_INC,               /**< ++a. a in children[0] */
	EXPR_OP_POST_INC,              /**< a++. a in children[0] */
	EXPR_OP_PRE_DEC,               /**< --a. a in children[0] */
	EXPR_OP_POST_DEC,              /**< a--. a in children[0] */
	EXPR_OP_NEGATE,                /**< -a. a in children[0] */
	EXPR_OP_PLUS,                  /**< +a. a in children[0]. Does nothing,
									  but needs to be stored in the AST */

	/* binary operations */
	EXPR_OP_ADD,                   /**< a + b. a in children[0], b in children[1]. */
	EXPR_OP_SUB,                   /**< a - b. a in children[0], b in children[1]. */
	EXPR_OP_MUL,                   /**< a * b. a in children[0], b in children[1]. */
	EXPR_OP_DIV,                   /**< a / b. a in children[0], b in children[1]. */
	EXPR_OP_LT,                    /**< a <  b. a in children[0], b in children[1]. */
	EXPR_OP_LE,                    /**< a <= b. a in children[0], b in children[1]. */
	EXPR_OP_EQ,                    /**< a == b. a in children[0], b in children[1]. */
	EXPR_OP_NE,                    /**< a != b. a in children[0], b in children[1]. */
	EXPR_OP_GE,                    /**< a >= b. a in children[0], b in children[1]. */
	EXPR_OP_GT,                    /**< a >  b. a in children[0], b in children[1]. */

	EXPR_OP_INDEX,                 /**< a[b]. a in children[0], b in children[1]. */
	EXPR_OP_CHAIN,                 /**< a, b. a in children[0], b in children[1]. */

	EXPR_OP_LOGICAL_AND,                   /**< a && b. a in children[0], b in children[1]. */
	EXPR_OP_LOGICAL_OR,                    /**< a || b. a in children[0], b in children[1]. */
	EXPR_OP_LOGICAL_XOR,                   /**< a ^^ b. a in children[0], b in children[1]. */

	EXPR_OP_SUBVECTOR_ACCESS,      /**< a[b] where a is a vector */


	/* assign ops */
	EXPR_OP_ASSIGN,                /**< a = b. a in children[0], b in children[1]. */
	EXPR_OP_ADD_ASSIGN,            /**< a += b. a in children[0], b in children[1]. */
	EXPR_OP_SUB_ASSIGN,            /**< a -= b. a in children[0], b in children[1]. */
	EXPR_OP_MUL_ASSIGN,            /**< a *= b. a in children[0], b in children[1]. */
	EXPR_OP_DIV_ASSIGN,            /**< a /= b. a in children[0], b in children[1]. */

	/* ternary operations */
	EXPR_OP_CONDITIONAL,           /**< a ? b : c.  a in children[0],
									  b in children[1], c in children[2] */
	EXPR_OP_CONDITIONAL_SELECT,    /**< a ? b : c, but unlike EXPR_OP_CONDITIONAL,
									  this operator evaluates both b and c,
									  regardless of the value of a. */
	EXPR_OP_SUBVECTOR_UPDATE,      /**< used for rewriting vector element assignments.
									  performs tmp = c, tmp[a] = b, tmp.
									  a, b and c in children. */

	/* type conversion operations */
	EXPR_OP_EXPLICIT_TYPE_CONVERT,
	EXPR_OP_IMPLICIT_TYPE_CONVERT,


	/* angle and trig functions */
	EXPR_OP_FUN_RADIANS,
	EXPR_OP_FUN_DEGREES,
	EXPR_OP_FUN_SIN,
	EXPR_OP_FUN_COS,
	EXPR_OP_FUN_TAN,
	EXPR_OP_FUN_ASIN,
	EXPR_OP_FUN_ACOS,
	EXPR_OP_FUN_ATAN,

	/* exponential functions */
	EXPR_OP_FUN_POW,
	EXPR_OP_FUN_EXP,
	EXPR_OP_FUN_LOG,
	EXPR_OP_FUN_EXP2,
	EXPR_OP_FUN_LOG2,
	EXPR_OP_FUN_SQRT,
	EXPR_OP_FUN_INVERSESQRT,

	/* common functions */
	EXPR_OP_FUN_ABS,
	EXPR_OP_FUN_SIGN,
	EXPR_OP_FUN_FLOOR,
	EXPR_OP_FUN_CEIL,
	EXPR_OP_FUN_FRACT,
	EXPR_OP_FUN_MOD,
	EXPR_OP_FUN_MIN,
	EXPR_OP_FUN_MAX,
	EXPR_OP_FUN_CLAMP,
	EXPR_OP_FUN_MIX,
	EXPR_OP_FUN_STEP,
	EXPR_OP_FUN_SMOOTHSTEP,

	/* geometric functions */
	EXPR_OP_FUN_LENGTH,
	EXPR_OP_FUN_DISTANCE,
	EXPR_OP_FUN_DOT,
	EXPR_OP_FUN_CROSS,
	EXPR_OP_FUN_NORMALIZE,
	EXPR_OP_FUN_FACEFORWARD,
	EXPR_OP_FUN_REFLECT,
	EXPR_OP_FUN_REFRACT,

	/* matrix functions */
	EXPR_OP_FUN_MATRIXCOMPMULT,

	/* vector relational functions */
	EXPR_OP_FUN_LESSTHAN,
	EXPR_OP_FUN_LESSTHANEQUAL,
	EXPR_OP_FUN_GREATERTHAN,
	EXPR_OP_FUN_GREATERTHANEQUAL,
	EXPR_OP_FUN_EQUAL,
	EXPR_OP_FUN_NOTEQUAL,
	EXPR_OP_FUN_ANY,
	EXPR_OP_FUN_ALL,
	EXPR_OP_FUN_NOT,

	/* texture lookup functions */
	EXPR_OP_FUN_TEXTURE1D,
	EXPR_OP_FUN_TEXTURE1DPROJ,
	EXPR_OP_FUN_TEXTURE1DLOD,
	EXPR_OP_FUN_TEXTURE1DPROJLOD,
	EXPR_OP_FUN_TEXTURE2D,
	EXPR_OP_FUN_TEXTURE2DPROJ,
	EXPR_OP_FUN_TEXTURE2DPROJ_W,
	EXPR_OP_FUN_TEXTURE2DLOD,
	EXPR_OP_FUN_TEXTURE2DPROJLOD,
	EXPR_OP_FUN_TEXTURE2DPROJLOD_W,
	EXPR_OP_FUN_TEXTURE3D,
	EXPR_OP_FUN_TEXTURE3DPROJ,
	EXPR_OP_FUN_TEXTURE3DLOD,
	EXPR_OP_FUN_TEXTURE3DPROJLOD,
	EXPR_OP_FUN_TEXTURECUBE,
	EXPR_OP_FUN_TEXTURECUBELOD,
	EXPR_OP_FUN_SHADOW2D,
	EXPR_OP_FUN_SHADOW2DPROJ,
	EXPR_OP_FUN_SHADOW2DLOD,
	EXPR_OP_FUN_SHADOW2DPROJLOD,

	EXPR_OP_FUN_TEXTUREEXTERNAL,
	EXPR_OP_FUN_TEXTUREEXTERNALPROJ,
	EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W,

	EXPR_OP_FUN_TEXTURE2DLOD_EXT,
	EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT,
	EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT,
	EXPR_OP_FUN_TEXTURECUBELOD_EXT,

	EXPR_OP_FUN_TEXTURE2DGRAD_EXT,
	EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT,
	EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT,
	EXPR_OP_FUN_TEXTURECUBEGRAD_EXT,

	/* fragment processing functions */
	EXPR_OP_FUN_DFDX,
	EXPR_OP_FUN_DFDY,
	EXPR_OP_FUN_FWIDTH,

	/* ops needed by backends */
	EXPR_OP_FUN_TRUNC, /**< truncate float to int */
	EXPR_OP_FUN_RCP, /**< inverse */
	EXPR_OP_FUN_RCC, /**< inverse that will never return 0 or +/-infinity */
	EXPR_OP_FUN_SIN_0_1, /**< sin with operand scaled to 0..1 instead of 0..2pi */
	EXPR_OP_FUN_COS_0_1, /**< sin with operand scaled to 0..1 instead of 0..2pi */
	EXPR_OP_FUN_SIN_PI, /**< sin with operand scaled to 0..1 instead of 0..pi */
	EXPR_OP_FUN_COS_PI, /**< sin with operand scaled to 0..1 instead of 0..pi */

	EXPR_OP_FUN_CLAMP_0_1, /**< clamp(a, 0, 1) */
	EXPR_OP_FUN_CLAMP_M1_1, /**< clamp(a, -1, 1) */
	EXPR_OP_FUN_MAX_0, /**< max(a, 0) */
	EXPR_OP_FUN_FMULN, /**< multiply that produces 0 instead of NaN for +/- 0 * +/- inf  */


	EXPR_OP_FUN_M200_ATAN_IT1,
	EXPR_OP_FUN_M200_ATAN_IT2,
	EXPR_OP_FUN_M200_HADD,
	EXPR_OP_FUN_M200_HADD_PAIR,
	EXPR_OP_FUN_M200_POS,
	EXPR_OP_FUN_M200_POINT,
	EXPR_OP_FUN_M200_MISC_VAL,
	EXPR_OP_FUN_M200_LD_RGB,
	EXPR_OP_FUN_M200_LD_ZS,
	EXPR_OP_FUN_M200_DERX, /**< the Mali200 derx instruction */
	EXPR_OP_FUN_M200_DERY, /**< the Mali200 dery instruction */
	EXPR_OP_FUN_M200_MOV_CUBEMAP, /**< the Mali200 move_cube instruction */

	EXPR_OP_FUN_MALIGP2_FIXED_TO_FLOAT, /**< fixed to float conversion */
	EXPR_OP_FUN_MALIGP2_FLOAT_TO_FIXED, /**< float to fixed conversion */
	EXPR_OP_FUN_MALIGP2_FIXED_EXP2, /**< exp2 with fixed-point input */
	EXPR_OP_FUN_MALIGP2_LOG2_FIXED, /**< log2 with fixed-point output */
	EXPR_OP_FUN_MALIGP2_MUL_EXPWRAP,

} expression_operator;


#define N_COMPONENTS 4
/** Swizzle pattern, used for both left-hand and right-hand swizzles
*/
typedef struct {
	signed char indices[N_COMPONENTS];            /** 0 - N_COMPONENTS-1 specifies input element to be redirected
										   to the output element. If we don't care
										   about the element, -1 is used. */
} swizzle_pattern;


        /** Vector combine pattern - used in EXPR_OP_VECTOR_COMBINE
		 * which is generated by the SSA transformation when rewriting lvalue swizzles.
		 * Also for scalar and vector instruction results.
		 */
typedef struct {
	signed char mask[N_COMPONENTS];   /**< In vector combine -
										* 1: from second, 0: from first.
										* In results - 1: to that subregister of output
										* -1 means don't care */
} combine_pattern;


/** An allocation into components of a specific register */
typedef struct {
	int reg;
	swizzle_pattern swizzle;
} register_allocation;

/**************************************/



/** Node header, used for all node types to identify what type it is.
 * Also includes contains information about the children nodes for unified graph traversal
*/
typedef struct
{
	node_kind kind:9;                  /**< Node kind */
	unsigned is_invariant:1;           /**< invariant specifier */
	unsigned is_control_dependent:1;   /**< Is this a control-dependent operation? */
	unsigned pad:5;						/**< Align the enums for short access */
	unsigned live_mask:16;              /**< Component liveness mask of the node */

	/*@null@*/ const type_specifier *type; /**< The type of node, not always
											  filled in for statements */
	unsigned short child_array_size;
	unsigned short n_children;
	nodeptr *children;

	int source_offset;                 /**< byte offset into concatenated source strings */
	int gl_pos_eq;
} node_header;

typedef int essl_address_offset;
#define UNKNOWN_SOURCE_OFFSET 0

/** Node type representing expressions
*/
typedef struct
{
	node_header hdr;                   /**< node header */
	expression_operator operation;     /**< Specific operation for the expression node */

	/* earliest_block dominates best_block dominates latest_block, given that a block dominates itself. */
	struct _tag_basic_block *earliest_block; /**< Earliest possible basic block for this operation. Always possible to schedule into. */
	struct _tag_basic_block *best_block; /**< Lowest-cost basic block for this operation. Note that this block may not be possible to schedule into due to choices made when scheduling the uses. */
	struct _tag_basic_block *latest_block; /**< Latest possible basic block for this operation. Note that this block may not be possible to schedule into due to choices made when scheduling the uses. */

	struct _tag_node_extra *info;      /**< Extra info for this node */
	union {
		struct _tag_symbol *sym;       /**< used by var_ref and function_call */
		string member_string;          /**< used by member dereference/swizzle before
										  type checking.  Never given to the backend */
		single_declarator *member;     /**< used for struct member dereference
										  after type checking */
		swizzle_pattern swizzle;       /**< used by swizzle expressions after type checking*/
		combine_pattern combiner;      /**< used by combine expressions */
		scalar_type value[1];          /**< used by constant */
		struct {
			address_space_kind address_space;        /**< used by load and store for knowing which
											  memory space a memory address refers to */
			register_allocation *alloc;/**< For parameter loads/stores allocated to a register */
		} load_store;
		struct {
			struct _tag_symbol *sym;   /**< symbol of the function we're calling */
			/*@null@*/ struct _tag_parameter *arguments; /**< load and store nodes for the arguments */
		} fun;
		struct {
			struct _tag_basic_block *block; /**< basic block containing
											   this phi node */
			struct _tag_phi_source *sources; /**< values joined by this phi node */
		} phi; /* phi node */
	} u;

} node_expression;



/** Node type representing statements
*/
typedef struct {
	node_header hdr; /**< node header */
	struct _tag_scope *child_scope; /**< if non-zero, the *children* of this node
									   will use this scope */
} node_statement;



/** Node type representing declarations
*/
typedef struct {
	node_header hdr;               /**< node header */
	struct _tag_symbol *sym;       /**< symbol for the declaration */
	type_basic prec_type;          /**< basic type for precision declaration */
	precision_qualifier prec_decl; /**< precision for precision declaration */
} node_declaration;


/** Node representation as a union of all possible node types,
    which can be identified by the header which is located first for all types
*/
union _tag_node
{
	node_header hdr;
	node_expression expr;
	node_statement stmt;
	node_declaration decl;
};



/***************************************/
/* handy functions for creating nodes: */


/** Creates a new node of a specific kind */
/*@null@*/ node *_essl_new_node(struct _tag_mempool *pool, node_kind kind,
								unsigned n_children);
/** Performs a shallow copy of an existing node */
/*@null@*/ node *_essl_clone_node(struct _tag_mempool *pool, node *orig);

/** Creates a new translation unit node using the specified global scope */
/*@null@*/ node *_essl_new_translation_unit(struct _tag_mempool *pool,
											struct _tag_scope *global_scope);

/*@null@*/ node *_essl_new_unary_expression(struct _tag_mempool *pool,
											expression_operator op, node *operand);
/*@null@*/ node *_essl_new_binary_expression(struct _tag_mempool *pool, node *left,
											 expression_operator op,
											 node *right);
/*@null@*/ node *_essl_new_assign_expression(struct _tag_mempool *pool, node *left,
											 expression_operator op,
											 node *right);
/*@null@*/ node *_essl_new_ternary_expression(struct _tag_mempool *pool,
											  expression_operator op, node *a,
											  node *b, node *c);

/*@null@*/ node *_essl_new_vector_combine_expression(struct _tag_mempool *pool, unsigned n_children);

/** Creates a new constant value node */
/*@null@*/ node *_essl_new_constant_expression(struct _tag_mempool *pool,
											   unsigned int vec_size);
/*@null@*/ node *_essl_new_variable_reference_expression(struct _tag_mempool *pool,
														 struct _tag_symbol *sym);

node *_essl_new_builtin_constructor_expression(mempool *pool, unsigned n_args);
node *_essl_new_struct_constructor_expression(mempool *pool, unsigned n_args);

/** Creates a new member reference node */
/*@null@*/ node *_essl_new_member_expression(struct _tag_mempool *pool, node *expr,
											 string name);
/*@null@*/ node *_essl_new_function_call_expression(struct _tag_mempool *pool,
													struct _tag_symbol *sym,
						    unsigned n_args);
/*@null@*/ node *_essl_new_builtin_function_call_expression(struct _tag_mempool *pool,
										expression_operator fun, node *arg0,
										/*@null@*/ node *arg1, /*@null@*/ node *arg2);
/*@null@*/ node *_essl_new_phi_expression(struct _tag_mempool *pool,
										  struct _tag_basic_block *block);
/*@null@*/ node *_essl_new_dont_care_expression(struct _tag_mempool *pool,
												const type_specifier *type);
/*@null@*/ node *_essl_new_transfer_expression(struct _tag_mempool *pool, node *expr);
/*@null@*/ node *_essl_new_load_expression(struct _tag_mempool *pool,
										   address_space_kind address_space, node *address);
/*@null@*/ node *_essl_new_store_expression(struct _tag_mempool *pool,
											address_space_kind address_space,
											node *address, node *expr);
/*@null@*/ node *_essl_new_depend_expression(struct _tag_mempool *pool, unsigned n_children);

/*@null@*/ node *_essl_new_type_convert_expression(struct _tag_mempool *pool, expression_operator op, node *arg);


/*@null@*/ node *_essl_new_flow_control_statement(struct _tag_mempool *pool, node_kind kind,
												  /*@null@*/ node *returnvalue);

/** Creates a new 'if' node:  if (condition) thenpart else elsepart
 */
/*@null@*/ node *_essl_new_if_statement(struct _tag_mempool *pool, node *condition,
										/*@null@*/ node *thenpart, /*@null@*/ node *elsepart);
/** Creates a new 'while' node:   while (condition) body
 */
/*@null@*/ node *_essl_new_while_statement(struct _tag_mempool *pool,
										   node *condition, /*@null@*/ node *body);
/** Creates a new 'do' node:   do body while (condition);
 */
/*@null@*/ node *_essl_new_do_statement(struct _tag_mempool *pool, node *body,
										/*@null@*/ node *condition);
/** Creates a new 'for' node:    for (initial; condition; increment) body
 */
/*@null@*/ node *_essl_new_for_statement(struct _tag_mempool *pool, /*@null@*/ node *initial,
										 node *condition, /*@null@*/ node *increment,
										 /*@null@*/ node *body);

/** Creates a new compound node, used to hold multiple statements and hold child scopes:
    { ... }
 */
/*@null@*/ node *_essl_new_compound_statement(struct _tag_mempool *pool);

/*@null@*/ node *_essl_new_function_declaration(struct _tag_mempool *pool,
												struct _tag_symbol *sym);
/*@null@*/ node *_essl_new_variable_declaration(struct _tag_mempool *pool,
												struct _tag_symbol *sym,
						/*@null@*/ node *initializer);
/*@null@*/ node *_essl_new_precision_declaration(struct _tag_mempool *pool,
												 type_basic basic_type, precision_qualifier precision);


#if defined(NDEBUG) || defined(S_SPLINT_S)
#define GET_N_CHILDREN(n) ((unsigned)((n)->hdr.n_children))
#define SET_N_CHILDREN(n,n_children,pool)  _essl_node_set_n_children((n),(n_children),(pool))
#define GET_CHILD(n, idx) ((n)->hdr.children[(idx)])
#define GET_CHILD_ADDRESS(n, idx) (&((n)->hdr.children[(idx)]))
#define SET_CHILD(n, idx, child) ((n)->hdr.children[(idx)] = (child))
#define APPEND_CHILD(n, child, pool) _essl_node_append_child((n), (child), (pool))
#define PREPEND_CHILD(n, child, pool) _essl_node_prepend_child((n), (child), (pool))

#else

#define GET_N_CHILDREN(n) _essl_node_get_n_children((n))
#define SET_N_CHILDREN(n,n_children,pool)  _essl_node_set_n_children((n),(n_children),(pool))
#define GET_CHILD(n, idx) _essl_node_get_child((n), (idx))
#define GET_CHILD_ADDRESS(n, idx) _essl_node_get_child_address((n), (idx))
#define SET_CHILD(n, idx, child) _essl_node_set_child((n), (idx), (child))
#define APPEND_CHILD(n, child, pool) _essl_node_append_child((n), (child), (pool))
#define PREPEND_CHILD(n, child, pool) _essl_node_prepend_child((n), (child), (pool))

#endif

/** Gets the number of children a node has
*/
unsigned _essl_node_get_n_children(const node *n);


/** Adjusts the number of children a node has
*/
memerr _essl_node_set_n_children(node *n, unsigned n_children, mempool *pool);


/** Gets node child number 'idx'
*/
/*@null@*/ node *_essl_node_get_child(const node *n, unsigned idx);


/** Gets the address of node child number 'idx'
*/
nodeptr *_essl_node_get_child_address(const node *n, unsigned idx);


/** Sets node child number 'idx' to the specified value 'child'
*/
void _essl_node_set_child(node *n, unsigned idx, /*@null@*/ node *child);


/** Puts a new child after the existing children, allocating space if necessary
*/
memerr _essl_node_append_child(node *n, /*@null@*/ node *child, mempool *pool);

/** Puts a new child before the existing children, allocating space if necessary
*/
memerr _essl_node_prepend_child(node *n, /*@null@*/ node *child, mempool *pool);



/** Shallow copies necessary information from oldnode to make sure newnode can
    be plugged in where oldnode used to be
*/
void _essl_ensure_compatible_node(node *newnode, node *oldnode);


/** Registers the position in the source strings that created a specific node */
void _essl_set_node_position(node *n, int source_offset);


/** Returns the symbol associated with a specific node */
/*@null@*/ struct _tag_symbol *_essl_symbol_for_node(node *n);

/** See if the symbol is an optimized sampler */
essl_bool _essl_is_optimized_sampler_symbol(const struct _tag_symbol *s);

/** Returns nonzero if n is a constant expression */
essl_bool _essl_node_is_constant(node *n);

/** Rewrite this node to a transfer node referring to the given child node. */
void _essl_rewrite_node_to_transfer(node *n, node *child);


/* Swizzle helper ops */
swizzle_pattern _essl_create_undef_swizzle(void);
swizzle_pattern _essl_create_scalar_swizzle(unsigned swz_idx);
swizzle_pattern _essl_create_identity_swizzle(unsigned swz_size);
swizzle_pattern _essl_create_identity_swizzle_from_swizzle(swizzle_pattern swz);
swizzle_pattern _essl_create_identity_swizzle_from_mask(unsigned live_mask);
unsigned _essl_mask_from_swizzle_output(swizzle_pattern swz);
unsigned _essl_mask_from_swizzle_input(swizzle_pattern *swz);
swizzle_pattern _essl_combine_swizzles(swizzle_pattern later, swizzle_pattern earlier);
essl_bool _essl_is_identity_swizzle(swizzle_pattern swz);
essl_bool _essl_is_identity_swizzle_sized(swizzle_pattern swz, unsigned size);
void _essl_swizzle_patch_dontcares(swizzle_pattern *swz, unsigned swz_size);

combine_pattern _essl_create_undef_combiner(void);
combine_pattern _essl_create_on_combiner(unsigned cmb_size);


essl_bool _essl_node_is_texture_operation(const node *n);


/* Combiner helper op */
node *_essl_create_vector_combine_for_nodes(mempool *pool, typestorage_context *typestor_context, node *left, node *right, node *n);
swizzle_pattern _essl_create_swizzle_from_combiner(combine_pattern a, int active_component);
swizzle_pattern _essl_invert_swizzle(swizzle_pattern swz);
essl_bool _essl_is_node_all_value(struct _tag_target_descriptor *desc, node *n, float value);
essl_bool _essl_is_node_comparison(const node *n);

#endif /* COMMON_NODE_H */

