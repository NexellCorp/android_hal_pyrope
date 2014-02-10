/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALIGP2_INSTRUCTION_H
#define MALIGP2_INSTRUCTION_H

#include "common/essl_system.h"
#include "common/essl_node.h"
#include "common/basic_block.h"
#include "backend/instruction.h"

struct _tag_maligp2_instruction;
struct _tag_maligp2_scheduler_context;


typedef enum {
	MALIGP2_UNKNOWN,

	/* special */
	MALIGP2_NOP,
	MALIGP2_NEXT_CYCLE,
	MALIGP2_FINISH,
	MALIGP2_CONSTANT,
	MALIGP2_LOCKED_CONSTANT,
	MALIGP2_SCHEDULE_EXTRA,

	/* several units */
	MALIGP2_OUTPUT_0,
	MALIGP2_OUTPUT_1,
	MALIGP2_NEGATE,

	/* mov pseudo-unit */
	MALIGP2_MOV,
	MALIGP2_RESERVE_MOV, /* move reservation, goes into the reserved_moves slots and competes for space in the adders, multipliers and misc units */
	MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD, /* move reservation, but only if the node being reserved has uses in subsequent instruction words */
	MALIGP2_MOV_MISC, /* A mov specifically placed into the misc unit. Typically used in conjunction with JMP_IF_MISC */
	MALIGP2_MOV_LUT, /* A mov specifically placed into the LUT unit. Typically used during bypass allocation */
	MALIGP2_RESERVE_MOV_MISC, /* An oddball. Treated like a MOV_MISC during scheduling, then turned into a RESERVE_MOV after scheduling and before register allocation */


	/* add units */
	MALIGP2_ADD,
	MALIGP2_FLOOR,
	MALIGP2_SIGN,
	MALIGP2_SET_IF_BOTH,
	MALIGP2_SGE,
	MALIGP2_SLT,
	MALIGP2_MIN,
	MALIGP2_MAX,

	/* mul units */
	MALIGP2_MUL,
	MALIGP2_MUL_ADD,
	MALIGP2_MUL_EXPWRAP,
	MALIGP2_CSEL,

	/* lut unit  */
	MALIGP2_RCC,
	MALIGP2_EX2,
	MALIGP2_LG2,
	MALIGP2_RSQ,
	MALIGP2_RCP,
	MALIGP2_LOG,
	MALIGP2_EXP,

	/* lut/misc unit */
	MALIGP2_SET_ADDRESS,
	MALIGP2_SET_LOAD_ADDRESS,
	MALIGP2_SET_STORE_ADDRESS,

	/* misc unit */
	MALIGP2_EXP_FLOOR,
	MALIGP2_FLOOR_LOG,
	MALIGP2_DIV_EXP_FLOOR_LOG,
	MALIGP2_FLOAT_TO_FIXED,
	MALIGP2_FIXED_TO_FLOAT,
	MALIGP2_CLAMP,
	MALIGP2_FRAC_FIXED_TO_FLOAT,




	/* branch unit */
    MALIGP2_RET,       /* function call return */
	MALIGP2_CALL,      /* function call. */
	MALIGP2_JMP_COND_NOT_CONSTANT, /* jump if not const.x */
	MALIGP2_JMP_COND,  /* jump if misc_res */
	MALIGP2_JMP,       /* unconditional jump */
	MALIGP2_CALL_COND, /* conditional function call */
	MALIGP2_LOOP_START,
	MALIGP2_REPEAT_START,
	MALIGP2_LOOP_END,
	MALIGP2_REPEAT_END,
	MALIGP2_STORE_SELECT_ADDRESS_REG,

	/* load units */
	MALIGP2_LOAD_INPUT_REG,
	MALIGP2_LOAD_WORK_REG,
	MALIGP2_LOAD_CONSTANT,
	MALIGP2_LOAD_CONSTANT_INDEXED,


	MALIGP2_STORE_WORK_REG,
	MALIGP2_STORE_OUTPUT_REG,
	MALIGP2_STORE_CONSTANT


} maligp2_opcode;



typedef unsigned int maligp2_schedule_classes;
	/* Concrete scheduling classes */
#define MALIGP2_SC_LOAD0      (1<< 0)
#define MALIGP2_SC_LOAD1      (1<< 1)
#define MALIGP2_SC_LOAD_CONST (1<< 2)
#define MALIGP2_SC_ADD0       (1<< 3)
#define MALIGP2_SC_ADD1       (1<< 4)
#define MALIGP2_SC_MUL0       (1<< 5)
#define MALIGP2_SC_MUL1       (1<< 6)
#define MALIGP2_SC_LUT        (1<< 7)
#define MALIGP2_SC_MISC       (1<< 8)
#define MALIGP2_SC_BRANCH     (1<< 9)
#define MALIGP2_SC_STORE_XY   (1<<10)
#define MALIGP2_SC_STORE_ZW   (1<<11)

	/* address register reservations. these are reserved for the _middle_ cycle (cycle 1) */
#define MALIGP2_SC_A0_X       (1<<12)
#define MALIGP2_SC_A0_Y       (1<<13)
#define MALIGP2_SC_A0_Z       (1<<14)
#define MALIGP2_SC_A0_W       (1<<15)


/*
  feedback network select

*/
#define MALIGP2_REG_UNKNOWN                -1
#define MALIGP2_INPUT_REGISTER_0_X          0
#define MALIGP2_INPUT_REGISTER_0_Y          1
#define MALIGP2_INPUT_REGISTER_0_Z          2
#define MALIGP2_INPUT_REGISTER_0_W          3
#define MALIGP2_INPUT_REGISTER_1_X          4
#define MALIGP2_INPUT_REGISTER_1_Y          5
#define MALIGP2_INPUT_REGISTER_1_Z          6
#define MALIGP2_INPUT_REGISTER_1_W          7

#define MALIGP2_RAM_X                       8
#define MALIGP2_RAM_Y                       9
#define MALIGP2_RAM_Z                      10
#define MALIGP2_RAM_W                      11

#define MALIGP2_CONSTANT_REGISTER_X        12
#define MALIGP2_CONSTANT_REGISTER_Y        13
#define MALIGP2_CONSTANT_REGISTER_Z        14
#define MALIGP2_CONSTANT_REGISTER_W        15

#define MALIGP2_ADD0_DELAY0                16
#define MALIGP2_ADD1_DELAY0                17
#define MALIGP2_MUL0_DELAY0                18
#define MALIGP2_MUL1_DELAY0                19
#define MALIGP2_MISC_DELAY0                20

#define MALIGP2_PREVIOUS_VALUE             21
#define MALIGP2_LUT                        22

#define MALIGP2_MISC_DELAY1                23
#define MALIGP2_ADD0_DELAY1                24
#define MALIGP2_ADD1_DELAY1                25
#define MALIGP2_MUL0_DELAY1                26
#define MALIGP2_MUL1_DELAY1                27


#define MALIGP2_INPUT_REGISTER_0_X_DELAY0  28
#define MALIGP2_INPUT_REGISTER_0_Y_DELAY0  29
#define MALIGP2_INPUT_REGISTER_0_Z_DELAY0  30
#define MALIGP2_INPUT_REGISTER_0_W_DELAY0  31



/*
   output value select
*/
#define MALIGP2_OUTPUT_ADD0              0
#define MALIGP2_OUTPUT_ADD1              1
#define MALIGP2_OUTPUT_MUL0              2
#define MALIGP2_OUTPUT_MUL1              3
#define MALIGP2_OUTPUT_MISC              4
#define MALIGP2_OUTPUT_MUL0_DELAY1       5
#define MALIGP2_OUTPUT_LUT_RES0          6
#define MALIGP2_OUTPUT_DISABLE           7


/*
   address register select
 */
#define MALIGP2_ADDR_UNKNOWN            -1
#define MALIGP2_ADDR_A0_X                0
#define MALIGP2_ADDR_A0_Y                1
#define MALIGP2_ADDR_A0_Z                2
#define MALIGP2_ADDR_A0_W                3
#define MALIGP2_ADDR_AL                  6
#define MALIGP2_ADDR_NOT_INDEXED         7


typedef struct _tag_maligp2_input_argument
{
	/*@null@*/ node *arg;

	int reg_index;           /* may be MALIGP2_REG_UNKNOWN: no such argument */
	unsigned negate:1;
} maligp2_input_argument;

/* The default values of these are defined in common/compiler_options.c
#define MALIGP2_MAX_INSTRUCTION_WORDS 512
#define MALIGP2_NUM_CONSTANT_REGS 304
#define MALIGP2_NUM_WORK_REGS 16
*/
#define MALIGP2_NUM_INPUT_REGS 16
#define MALIGP2_NUM_OUTPUT_REGS 16
#define MALIGP2_NATIVE_VECTOR_SIZE 4
#define MALIGP2_MAX_INPUT_ARGS 2
#define MALIGP2_MAX_INSTRUCTIONS_PER_BATCH 16
#define MALIGP2_MAX_MOVES 5 /* add0, add1, mul0, mul1, misc. lut does not count as a real mov */

/** Convert from cycle and relative subcycle to subcycle.
 *  Relative subcycle can be negative, indicating highest
 *  subcycle of successor cycle.
 */
#define MALIGP2_CYCLE_TO_SUBCYCLE(cycle, rel_subcycle) (((cycle) << 2) + (rel_subcycle))
/** The cycle containing this subcycle */
#define MALIGP2_SUBCYCLE_TO_CYCLE(subcycle) ((subcycle) >> 2)
/** The relative subcycle of this subcycle */
#define MALIGP2_SUBCYCLE_TO_RELATIVE_SUBCYCLE(subcycle) ((subcycle) & 3)


typedef struct _tag_maligp2_instruction
{
	maligp2_opcode opcode;

	node *instr_node;        /**< the node that corresponds to the current instruction */

	maligp2_schedule_classes schedule_class; /**< Scheduling class for this instruction */
	maligp2_input_argument args[MALIGP2_MAX_INPUT_ARGS];

	int address_offset;
	int address_reg;

	/*@null@*/ struct _tag_basic_block *jump_target; /**< target basic block of a jump inst */
	/*@null@*/ symbol *call_target; /**< target function of a call instruction */
	int subcycle;
} maligp2_instruction;


#define MALIGP2_N_INSTRUCTIONS_IN_WORD 21


typedef struct _tag_maligp2_instruction_word
{
	/* instruction words are chained together as a double-linked list */
	struct _tag_maligp2_instruction_word *predecessor;
	struct _tag_maligp2_instruction_word *successor;

	short cycle; /**< the cycle of this instruction word, counted _backwards_ */
	short original_cycle;

	/* these slots (and pseudo-slots) are occupied */
	maligp2_schedule_classes used_slots;
	maligp2_schedule_classes used_slots_before_reg_alloc;

	/* subcycle 2 */
	/* since a load produces four values, we have four slots, although we only have one reservation bit per load */
	union
	{
		struct
		{
			/* subcycle 2 */
			/* since a load produces four values, we have four slots, although we only have one reservation bit per load */
			maligp2_instruction *load0[4];
			maligp2_instruction *load1[4];
			maligp2_instruction *load_const[4];

			/* subcycle 1 */
			maligp2_instruction *add0;
			maligp2_instruction *add1;

			maligp2_instruction *mul0;
			maligp2_instruction *mul1;

			maligp2_instruction *lut;
			maligp2_instruction *misc;

			/* subcycle 0 */
			maligp2_instruction *store_xy;
			maligp2_instruction *store_zw;
			maligp2_instruction *branch; /* does branches and stores to constant registers. If you store to a constant register, you need to reserve at least one or more of the store units */
		}real_slots;
		struct
		{
			/* number of slots must be equal to the number in real slots */
			maligp2_instruction *slot[MALIGP2_N_INSTRUCTIONS_IN_WORD];
		}dummy_slots;
	}u;

	maligp2_opcode add_opcodes[2];
	maligp2_opcode mul_opcodes[2];

	/* reservations for A0.x, A0.y, A0.z in each of the subcycles. A0.w is always used for stores and does not need an explicit reservation? */
	node *reserved_moves[MALIGP2_MAX_MOVES];
	essl_bool move_reservation_fulfilled[MALIGP2_MAX_MOVES];
	node *embedded_constants[4];
	essl_bool embedded_constants_locked;

	int n_moves_available:4;
	int n_moves_available_before_reg_alloc:4;
	int n_moves_reserved:4;
	int n_embedded_constants:4;
	int emit_address; /**< the address this word was emitted at, in instruction words. used only by the emitter */

} maligp2_instruction_word;
ASSUME_ALIASING(maligp2_instruction_word, instruction_word);


#define MALIGP2_FIRST_INSTRUCTION_IN_WORD(word) (&(word)->u.dummy_slots.slot[0])

#define MALIGP2_SUBCYCLES_PER_CYCLE 4

/*@null@*/ maligp2_instruction *_essl_new_maligp2_instruction(mempool *pool,
                maligp2_schedule_classes sc, maligp2_opcode opcode, int subcycle);
/*@null@*/ maligp2_instruction_word *_essl_new_maligp2_instruction_word(mempool *pool, int cycle);
maligp2_instruction *_essl_maligp2_create_slot_instruction(mempool *pool, maligp2_instruction_word *word,
														   maligp2_schedule_classes mask, maligp2_opcode opcode, node *out, int *subcycle, unsigned vec_idx, essl_bool *failed_alloc);


memerr _essl_maligp2_write_instructions(struct _tag_maligp2_scheduler_context *ctx,
										maligp2_instruction_word *word, int *slots, node **transfer_nodes, int address_reg, va_list arglist);

int _essl_maligp2_get_mul_slot_opcode(maligp2_opcode op0, maligp2_opcode op1);
int _essl_maligp2_get_add_slot_opcode(maligp2_opcode op0, maligp2_opcode op1);
essl_bool _essl_maligp2_add_slot_move_needs_two_inputs(int common_opcode);

essl_bool _essl_maligp2_inseparable(maligp2_instruction_word *w2);
essl_bool _essl_maligp2_inseparable_from_successor(maligp2_instruction_word *w);
essl_bool _essl_maligp2_inseparable_from_predecessor(maligp2_instruction_word *w);


#endif /* MALIGP2_INSTRUCTION_H */

