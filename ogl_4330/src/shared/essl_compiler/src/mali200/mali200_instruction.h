/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI200_INSTRUCTION_H
#define MALI200_INSTRUCTION_H

#include "common/essl_system.h"

#include "common/essl_node.h"
#include "common/basic_block.h"
#include "common/error_reporting.h"
#include "backend/extra_info.h"
#include "backend/instruction.h"

struct _tag_m200_instruction;
struct _tag_mali200_scheduler_context;


typedef enum {
	M200_UNKNOWN,

	M200_NOP, /* used for filling the store slot when a function call has been scheduled */

	/* multiply-add instructions unit */

	M200_ADD,    /* add, takes 2 arguments */
    M200_ADD_X2, /* add with result scaling */
    M200_ADD_X4,
    M200_ADD_X8,
    M200_ADD_D2,
    M200_ADD_D4,
    M200_ADD_D8,
    M200_HADD3,  /* horizontal add; takes 1 vector argument */
    M200_HADD4,  /* horizontal add; takes 1 vector argument */
	M200_2X2ADD, /* pair-wise horiz add (opCode 18),  2 vector args */
	M200_MUL,    /* multiply, takes 2 arguments */
    M200_MUL_X2, /* multiply with result scaling */
    M200_MUL_X4,
    M200_MUL_X8,
    M200_MUL_D2,
    M200_MUL_D4,
    M200_MUL_D8,
    M200_MUL_W1, /* multiply: set W of input1 to 1 */
	M200_NOT,    /* boolean NOT operation, 1 argument */
    M200_AND,    /* boolean AND operation, 2 arguments */
	M200_OR,     /* boolean OR operation, 2 arguments */
	M200_XOR,    /* boolean XOR operation, 2 arguments */
	M200_CMP,    /* compare operations. 2 arguments + compare-func */
	M200_MIN,    /* minimum value, 2 arguments */
	M200_MAX,    /* maximum value, 2 arguments */
	M200_SEL,    /* select based on boolean scalar */
	M200_VSEL,   /* select based on boolean vector */
	M200_DERX,   /* derivative in X direction */
	M200_DERY,   /* derivative in Y direction */
	M200_FRAC,   /* fractional value; takes 1 arg */
	M200_FLOOR,  /* floor value (round down); takes 1 arg */
	M200_CEIL,   /* ceiling value (round up); takes 1 arg */
	M200_SWZ,    /* extended swizzle; special arg[1] */
    M200_CONS31, /* scalar+vec3 constructor */
    M200_CONS22, /* vec2+vec2 constructor   */
    M200_CONS13, /* vec3+scalar constructor */

	/* texture instructions.  These take two input arguments:

		0: register to use in following, may be unknown i.e. not present
           sampler to use is at: (address_offset + address_multiplier*argument[0]),
		1: register holding LOD level or offset to use, may be unknown.
		   lod_offset holds the float constant for this instruction.

		opcode_flags contain tex_ignore and tex_no_reset_summation flags
	*/

	M200_TEX,

	/* branch instructions. These take the following arguments:

		0: first argument of the compare operation
		1: second argument of the compare operation
		2: source register for ALT_KILL
        2: target addr reg for JMP_REL & CALL_REL
		target for branch is given by jump_target.
	*/

	M200_JMP,       /* conditional or unconditional jump */
    M200_JMP_REL,   /* register-relative jump */
	M200_CALL,      /* function call. */
	M200_CALL_REL,  /* register-relative function call */
    M200_RET,       /* function call return */
	M200_KILL,      /* pixel kill: takes no destination argument */
    M200_ALT_KILL,  /* alternative pixel kill */
    M200_GLOB_END,  /* end instruction in the branch unit (note: not end flag) */

	/* load/store instructions. These take the following arguments:

		0: scalar register holding  Index,
		   offset and multiplier are found in the m200_instruction
		1: Data register, usually the source value to be stored, the store unit only,
		   same as output for ST_SUBREG.
        2: Base sub-reg for LD_REL/ST_REL, source register for ST_SUBREG.

		Destination for loads in the load unit is the implicit pseudo-reg  #load.
		Special, loads in the store unit:
		  LD_SUBREG, LEA, LD_ZS, LD_RGB, ST_SUBREG: destination in  out_reg  etc.

	*/

	M200_LD_UNIFORM,/* load from uniforms */
	M200_LD_STACK,  /* load from stack */
	M200_LD_REL,    /* load from array (relative address) */
	M200_LD_REG,    /* load from indexed register */
	M200_LD_SUBREG, /* load indexed sub-register */
	M200_LEA,       /* load effective address */
	M200_ST_STACK,  /* store to stack */
	M200_ST_REL,    /* store to array */
	M200_ST_REG,    /* store to indexed register */
	M200_ST_SUBREG, /* store indexed sub-register */
    M200_LD_RGB,    /* framebuffer RGB load */
    M200_LD_ZS,     /* framebuffer Z/stencil load */
	M200_MOV,       /* value->value move (can be executed in several places) */
	M200_REG_MOV,   /* register->value move pseudo-instr, only in scheduler */
	M200_CONST_MOV, /* constant->value move pesudo-instr, only in scheduler */
	M200_LOAD_RET,  /* load return value pseudo-instr, only in scheduler */

	/* LUT unit operations */

	M200_RCP,       /* 1/x */
	M200_RCC,       /* 1/x, but not 0 or inf */
	M200_RSQ,       /* 1/sqrt(x) */
	M200_SQRT,      /* sqrt(x) */
	M200_EXP,       /* 2^x */
	M200_LOG,       /* log2(x) */
	M200_SIN,       /* sin 2*pi*x */
	M200_COS,       /* cos 2*pi*x */
	M200_ATAN1_IT1, /* 1st iteration of 1-argument atan */
	M200_ATAN2_IT1, /* 1st iteration of 2-argument atan */
	M200_ATAN_IT2,  /* 2nd iteration of atan */

	/* varying unit operations

	   input register is in args[0], used for dependent texturing.
	   offset is in  address_offset, size in  address_multiplier.
	   For the move variants, input 0 contain the input argument.
	   opcode_flags contain whether centroid and/or flat shading is used.
	   output specified as usual, may be unknown: result always in implicit reg.
    */

	M200_VAR,       /* evaluate a varying */
	M200_VAR_DIV_Y, /* evaluate a varying and divide the result by the y component*/
	M200_VAR_DIV_Z, /* evaluate a varying and divide the result by the y component*/
	M200_VAR_DIV_W, /* evaluate a varying and divide the result by the w component*/
	M200_VAR_CUBE,
	M200_MOV_DIV_Y,
	M200_MOV_DIV_Z,
	M200_MOV_DIV_W,
	M200_MOV_CUBE,

    M200_POS,    /* evaluate the 'position register' */
	M200_POINT,  /* evaluate a 3x3 matrix-vector transform */
	M200_MISC_VAL, /* evaluate a 3x3 matrix-vector transform */
	M200_NORM3   /* normalize a 3-component vector */

} m200_opcode;



typedef unsigned int m200_schedule_classes;
	/* Concrete scheduling classes */
#define M200_SC_VAR (1<<0)
#define M200_SC_TEX (1<<1)
#define M200_SC_LOAD (1<<2)
#define M200_SC_MUL4 (1<<3)
#define M200_SC_MUL1 (1<<4)
#define M200_SC_ADD4 (1<<5)
#define M200_SC_ADD1 (1<<6)
#define M200_SC_LUT (1<<7)
#define M200_SC_STORE (1<<8)
#define M200_SC_BRANCH (1<<9)
#define M200_SC_LSB_VECTOR_INPUT (1<<10)
#define M200_SC_CONCRETE_MASK ((1<<11)-(1<<0))

	/* Special signal flags */
#define M200_SC_RENDEZVOUS (1<<12) /* Instruction requires rendezvous */
#define M200_SC_NO_RENDEZVOUS (1<<13) /* Instruction requires absence of rendezvous */
#define M200_SC_SKIP_MOV (1<<14) /* Parse a move instruction but don't write it.
								  Instead allocate the out node to the in reg explicitly */
#define M200_SC_DONT_SCHEDULE_MOV (1<<15) /* Do not mark mov instructions as
											scheduled in the generic scheduler. */
#define M200_SC_SKIP_LOAD (1<<16)
#define M200_SC_SKIP_VAR (1<<17)
#define M200_SC_SIGNAL_MASK ((1<<18)-(1<<12))
#define M200_SC_ALLOC_MASK (((1<<10)-(1<<0)) | M200_SC_SKIP_MOV | M200_SC_SKIP_LOAD | M200_SC_SKIP_VAR)

	/* Pseudo scheduling classes */
#define M200_SC_VECTOR_ALU (1<<18) /* Either MUL4 or ADD4 */
#define M200_SC_SCALAR_VECTOR_MUL_LUT (1<<19) /* Either MUL4 or LUT */
#define M200_SC_ALU (1<<20)  /* Scalar or vector alu */
#define M200_SC_MUL (1<<21)  /* Scalar or vector mul */
#define M200_SC_MUL_LUT (1<<22)  /* Scalar mul, vector mul or lut */
#define M200_SC_ADD (1<<23)  /* Scalar or vector add */
#define M200_SC_MOV (1<<24) /* Scalar or vector move */
#define M200_SC_MOV_WITH_MODIFIERS (1<<25) /* Scalar or vector move guaranteed with input and output modifiers */
#define M200_SC_MOV_IF_NOT_SAME_WORD (1<<26) /* Scalar or vector move if not same word */
#define M200_SC_PSEUDO_MASK ((1<<27)-(1<<18))

	/* Subcycle masks */
#define M200_SC_SUBCYCLE_3_MASK (M200_SC_VAR | M200_SC_TEX | M200_SC_LOAD)
#define M200_SC_SUBCYCLE_2_MASK (M200_SC_MUL4 | M200_SC_MUL1)
#define M200_SC_SUBCYCLE_1_MASK (M200_SC_ADD4 | M200_SC_ADD1)
#define M200_SC_SUBCYCLE_0_MASK (M200_SC_LUT | M200_SC_STORE | M200_SC_BRANCH)


typedef enum {            /* compare codes, same as in HW conditional branch instructions */
	M200_CMP_NEVER = 0,
	M200_CMP_LT    = 1,
	M200_CMP_EQ    = 2,
	M200_CMP_LE    = 3,
	M200_CMP_GT    = 4,
	M200_CMP_NE    = 5,
	M200_CMP_GE    = 6,
	M200_CMP_ALWAYS= 7
} m200_comparison;

#define M200_REVERSE_CMP(cmp) ((cmp)^7)


        /* Registers are generally encoded in a 4 bit field in the hardware
		 * instructions.  This means that the m200 assembler can
		 * define LOAD_PSEUDO_REG (-1), which gives same pattern as  M200_HASH_LOAD
		 * Note that  #var  never occurs in 'real' instructions, it's a notation
		 * for 'implicit', i.e. a special, underhand transfer path.
		 * M200_R_IMPLICIT is used for direct transfer from MUL1 to ADD1 and MUL4 to ADD4
		 */
#define M200_REG_UNKNOWN (-1)
#define M200_R0  0
#define M200_R1  1
#define M200_R2  2
#define M200_R3  3
#define M200_R4  4
#define M200_R5  5
#define M200_R6  6
#define M200_R7  7
#define M200_R8  8
#define M200_R9  9
#define M200_R10 10
#define M200_R11 11
#define M200_CONSTANT0 12
#define M200_CONSTANT1 13
#define M200_HASH_TEX  14
#define M200_HASH_LOAD 15
#define M200_R_IMPLICIT -16
#define M200_MAX_REGS 12
#define M200_R_SPILLED 16


/*
  need to handle pseudo-register values that are transferred to real nodes with move somehow
*/


typedef struct _tag_m200_input_argument
{
	/*@null@*/ node *arg;

	int reg_index;           /* may be M200_REG_UNKNOWN: no such argument */
	swizzle_pattern swizzle;
	essl_bool absolute_value;
	essl_bool negate;
} m200_input_argument;

        /* opcode_flag stuff: */
#define M200_VAR_CENTROID 1
#define M200_VAR_FLATSHADING 2
        /* bits specifying the W-component in NORM3 instructions; actually an enumerated */
#define M200_NORM_SQR        8
#define M200_NORM_NORM       16
#define M200_NORM_RCP        32
#define M200_NORM_RSQR       64
        /* for various bits in TEX */
#define M200_TEX_IGNORE 1
#define M200_TEX_NO_RESET_SUMMATION 2
#define M200_TEX_LOD_REPLACE 4
#define M200_TEX_COO_W 8
/* for alu stuff that go into multipliers and is used only in the adders */
#define M200_ALU_DONT_WRITE_REG_FILE 1


        /* end of opcode_flag stuff */



#define M200_MAX_INPUT_ARGS 4
#define M200_N_CONSTANT_REGISTERS 2
#define M200_NATIVE_VECTOR_SIZE 4
#define M200_N_VARYINGS 12
/* This is defined in common/compiler_options.c
#define M200_N_REGISTERS 6
*/

#define M200_SUBCYCLES_PER_CYCLE 4

/** Convert from cycle and relative subcycle to subcycle.
 *  Relative subcycle can be negative, indicating highest
 *  subcycle of successor cycle.
 */
#define CYCLE_TO_SUBCYCLE(cycle, rel_subcycle) (((cycle) << 2) + (rel_subcycle))
/** The cycle containing this subcycle */
#define SUBCYCLE_TO_CYCLE(subcycle) ((subcycle) >> 2)
/** The relative subcycle of this subcycle */
#define SUBCYCLE_TO_RELATIVE_SUBCYCLE(subcycle) ((subcycle) & 3)


typedef struct _tag_m200_instruction
{
	m200_opcode opcode;
	unsigned opcode_flags;   /**< opcode-specific flags that modify the instruction */

	node *instr_node;        /**< the node that corresponds to the current instruction */

	m200_schedule_classes schedule_class; /**< Scheduling class for this instruction */
	m200_input_argument args[M200_MAX_INPUT_ARGS];
	int address_offset;             /**< for VAR: offset to indexing in varyings,
									 * TEX: offset to sampler, LOAD/STORE: address displacement
									 * BRANCH, call & return: frame size  */
	int address_multiplier;
	int out_reg;                    /**< register index for result */
	essl_bool output_reg_prev_undefined; /**< flag from scheduler to register allocator */
	m200_output_mode output_mode;   /**< saturate mode */
	swizzle_pattern output_swizzle; /**< output mask + swizzle */
	float lod_offset;               /**< only used for texture instructions */

	/*@null@*/ struct _tag_basic_block *jump_target; /**< target basic block of a jump inst */
	/*@null@*/ symbol *call_target; /**< target function of a call instruction */
	m200_comparison compare_function;
	int subcycle;
} m200_instruction;


typedef struct _tag_m200_instruction_word
{	/* instruction words are chained together as a double-linked list */
	struct _tag_m200_instruction_word *predecessor;
	struct _tag_m200_instruction_word *successor;

	short cycle; /**< the cycle of this instruction word, counted _backwards_ */
	short original_cycle;

	/* these slots (and pseudo-slots) are occupied */
	m200_schedule_classes used_slots;

	/* subcycle 3 */
	m200_instruction *var;
	m200_instruction *tex;
	m200_instruction *load;

	/* subcycle 2 */
	m200_instruction *mul4;
	m200_instruction *mul1;

	/* subcycle 1 */
	m200_instruction *add4;
	m200_instruction *add1;

	/* subcycle 0 */
	m200_instruction *lut;
	m200_instruction *store;
	m200_instruction *branch;

    /** number of entries in each of the two embedded constants */
	unsigned n_embedded_entries[M200_N_CONSTANT_REGISTERS];

	/** embedded constants */
	float embedded_constants[M200_N_CONSTANT_REGISTERS][M200_NATIVE_VECTOR_SIZE];
	essl_bool is_embedded_constant_shareable[M200_N_CONSTANT_REGISTERS][M200_NATIVE_VECTOR_SIZE];

	essl_bool end_of_program_marker;

	int  emit_adr;                     /**< address of this instruction word when emitted */
	int  emit_siz;                     /**< size of code emitted for this */
	int  bran_wrd;                     /**< for start of cond branch instr, if any */
	int  bran_bit;                     /**< for start of cond branch instr, if any */
} m200_instruction_word;
ASSUME_ALIASING(m200_instruction_word, instruction_word);

#define M200_FIRST_INSTRUCTION_IN_WORD(word) (&(word)->var)
#define M200_N_INSTRUCTIONS_IN_WORD 10

/*@null@*/ m200_instruction *_essl_new_mali200_instruction(mempool *pool,
                m200_schedule_classes sc, m200_opcode opcode, int subcycle);
/*@null@*/ m200_instruction_word *_essl_new_mali200_instruction_word(mempool *pool, int cycle);
/*@null@*/ m200_instruction *_essl_mali200_create_slot_instruction(mempool *pool, m200_instruction_word *word,
																   m200_schedule_classes *maskp, m200_opcode opcode);
void _essl_mali200_remove_empty_instructions(control_flow_graph *cfg);
memerr _essl_mali200_insert_pad_instruction(mempool *pool, control_flow_graph *cfg, error_context *err);

/** Does an instruction with this opcode produce the same result if the operands are swapped? */
essl_bool _essl_mali200_opcode_is_symmetric(m200_opcode opcode);
/** Does an instruction with this opcode have any side effects apart from writing the value
 *  given by instr_node and write_mask?
 */
essl_bool _essl_mali200_opcode_has_side_effects(m200_opcode opcode);

essl_bool _essl_mali200_can_handle_redirection(node *n);
essl_bool _essl_mali200_has_output_modifier_slot(node *n);
essl_bool _essl_mali200_has_output_modifier_and_truncsat_slot(node *n);
essl_bool _essl_mali200_has_output_modifier_and_swizzle_slot(node *n, essl_bool scalar_swizzle);
essl_bool _essl_mali200_output_modifiers_can_be_coalesced(m200_output_modifier_set *a_mods, m200_output_modifier_set *b_mods);

essl_bool _essl_mali200_output_modifier_is_identity(m200_output_modifier_set *mods);

memerr _essl_mali200_write_instructions(struct _tag_mali200_scheduler_context *ctx,
                m200_instruction_word *word, m200_schedule_classes inst_mask, va_list args);



#endif /* MALI200_INSTRUCTION_H */

