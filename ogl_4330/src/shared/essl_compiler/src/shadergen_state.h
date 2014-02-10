/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef SHADERGEN_STATE_H
#define SHADERGEN_STATE_H

#include "common/essl_system.h"

#define MAX_TEXTURE_STAGES 8
#define MAX_TEXTURE_STAGE_MASK 0xFFFF  /* 8 bits for RGB enables, 8 bits for ALPHA enables */

typedef int shadergen_boolean;

/**********************************
 * VERTEX SHADERGEN STATE GETTERS *
 **********************************/


typedef enum
{
    TEX_IDENTITY,   /* texture coordinates are transformed by the identity transform. No transform needed */
    TEX_TRANSFORM   /* texture coordinates are transformed by a real matrix. Transform is needed */
} texcoord_mode;

typedef enum
{
    TEXGEN_REFLECTIONMAP,    /* generates reflection mapped texture coordinates */
    TEXGEN_NORMALMAP         /* generates normal mapped texture coordinates */
} texgen_mode;

typedef struct vertex_shadergen_state
{
	/**
	 * The vertex state contains 2 words. 
	 *
	 * WORD 0: Reserved for feature bits
	 *
	 *    All the feature bits are automatically allocated to bit-slots by the piece generator 
	 *    found inside the piecegen_maligp2 folder. The final result of this generation process 
	 *    can be read as the shadergen_maligp2/shader_pieces.h file. Notably of interest, the 
	 *    VERTEX_SHADERGEN_FEATURE* #define in that file can be used with the 
	 *    vertex_shadergen_encode / vertex_shadergen_decode functions below. 
	 *
	 *    Also check that file for additional value ENUMs associated with featurebits. 
	 *
	 * WORD 1: Per stage information. 
	 *
	 *    The first 8 bits hold the stage enable bits
	 *      7-0: VERTEX_SHADERGEN_STAGE_ENABLE, stage 7-0
	 *
	 *    The next 8 bits hold the texture transform bits. If the texture transform is 
	 *    an identity matrix, we can skip out of the entire transform work. 
	 *    See the enum texcoord_mode for details. 
	 *     15-8: VERTEX_SHADERGEN_STAGE_ENABLE, stage 7-0
	 *
	 *    The next 8 bits hold the texture generation enable info, 1 bit per stage. 
	 *    23-16: VERTEX_SHADERGEN_STAGE_TEXGEN_ENABLE, stage 7-0
	 *
	 *    The final 8 bits hold the texture generation mode info, 1 bit per stage. 
	 *    See the enum above for values.
	 *    31-24: VERTEX_SHADERGEN_STAGE_TEXGEN_MODE, stage 7-0
	 *
	 **/

	unsigned int bits[2]; /* need 2 words to hold all state. Expand as needed */
} vertex_shadergen_state;

/* Access to the bits work pretty much like rsw setters.
 *
 * vertex_shadergen_encode(state, SOME_ENUM, value);
 *
 * The SOME_ENUM is actually three parameters: sub-byte, mask and shift.
 *
 * Example:
 * 	vertex_shadergen_encode(state, VERTEX_SHADERGEN_STAGE_ENABLE(2), MALI_TRUE);
 * 	will expand to
 *  vertex_shadergen_encode(state, 1, 0x1, 2, MALI_TRUE);
 * 	which will set the second bit in subword 1 byte to true.
 *
 * Getters work in the same fashion:
 */


#define VERTEX_SHADERGEN_STAGE_ENABLE(stage)                 1, 0x1, (stage)
#define VERTEX_SHADERGEN_STAGE_TRANSFORM(stage)              1, 0x1, ((stage)+8)
#define VERTEX_SHADERGEN_STAGE_TEXGEN_ENABLE(stage)          1, 0x1, ((stage)+16)
#define VERTEX_SHADERGEN_STAGE_TEXGEN_MODE(stage)            1, 0x1, ((stage)+24)

ESSL_STATIC_FORCE_INLINE void vertex_shadergen_encode( vertex_shadergen_state* state, unsigned int subword, unsigned int mask, unsigned int shift, unsigned int value )
{
#if !defined(__SYMBIAN32__) || !defined(MALI_DEBUG_ASSERT) /* This is used in gles, where assert is not available on Symbian. */
	assert( (value & ~mask) == 0 );
#else
	MALI_DEBUG_ASSERT( (value & ~mask) == 0, ("(value & ~mask) != 0") );
#endif
	state->bits[ subword ] &= ~( mask << shift);
	state->bits[ subword ] ^=  ( value << shift);
}

ESSL_STATIC_FORCE_INLINE unsigned int vertex_shadergen_decode( const vertex_shadergen_state* state, unsigned int subword, unsigned int mask, unsigned int shift)
{
	return (( state->bits[ subword ] & ( mask << shift)) >> shift);
}

/************************************
 * FRAGMENT SHADERGEN STATE GETTERS *
 ************************************/

typedef enum {
	FOG_NONE,
	FOG_LINEAR,
	FOG_EXP,
	FOG_EXP2
} fog_kind;

typedef enum
{
	ARG_DISABLE,
	ARG_CONSTANT_0,
	ARG_CONSTANT_1,
	ARG_CONSTANT_2,
	ARG_CONSTANT_3,
	ARG_CONSTANT_4,
	ARG_CONSTANT_5,
	ARG_CONSTANT_6,
	ARG_CONSTANT_7,
	ARG_CONSTANT_COLOR,
	ARG_PRIMARY_COLOR,
	ARG_SPECULAR_COLOR,
	ARG_TEXTURE_0,
	ARG_TEXTURE_1,
	ARG_TEXTURE_2,
	ARG_TEXTURE_3,
	ARG_TEXTURE_4,
	ARG_TEXTURE_5,
	ARG_TEXTURE_6,
	ARG_TEXTURE_7,
	ARG_INPUT_COLOR, /* Must be just before stage result 0 */
	ARG_STAGE_RESULT_0,
	ARG_STAGE_RESULT_1,
	ARG_STAGE_RESULT_2,
	ARG_STAGE_RESULT_3,
	ARG_STAGE_RESULT_4,
	ARG_STAGE_RESULT_5,
	ARG_STAGE_RESULT_6,
	ARG_STAGE_RESULT_7,
	ARG_N_STAGES,
	ARG_PREVIOUS_STAGE_RESULT = ARG_N_STAGES,
	ARG_DIFFUSE_COLOR = ARG_PRIMARY_COLOR,
	ARG_TFACTOR = ARG_CONSTANT_0


} arg_source;

typedef enum
{
	/* ONE_MINUS variants must have odd values */
	OPERAND_SRC_COLOR = 0,
	OPERAND_ONE_MINUS_SRC_COLOR = 1,
	OPERAND_SRC_ALPHA = 2,
	OPERAND_ONE_MINUS_SRC_ALPHA = 3,
	/* Internal values below */
	OPERAND_SRC = 4,
	OPERAND_ONE_MINUS_SRC = 5,
	OPERAND_N_OPERANDS
} arg_operand;


typedef enum
{
	COMBINE_REPLACE,     /* Arg0 */
	COMBINE_MODULATE,    /* Arg0 * Arg1 */
	COMBINE_ADD,         /* Arg0 + Arg1 */
	COMBINE_ADD_SIGNED,  /* Arg0 + Arg1 - 0.5 */
	COMBINE_INTERPOLATE, /* Arg0 * Arg2 + Arg1 * (1 - Arg2) */
	COMBINE_SUBTRACT,    /* Arg0 - Arg1 */
	COMBINE_DOT3_RGB,    /* 4 * dot(arg0 - 0.5, arg1 -  0.5) */
	COMBINE_DOT3_RGBA    /* 4 * dot(arg0 - 0.5, arg1 -  0.5) */
} texture_combiner;

typedef enum
{
	SCALE_ONE,
	SCALE_TWO,
	SCALE_FOUR

} scale_kind;


typedef enum
{
	TEXTURE_2D,
	TEXTURE_2D_PROJ_W, /* divide by w */
	TEXTURE_EXTERNAL,
	TEXTURE_EXTERNAL_PROJ_W, /* divide by w */
	TEXTURE_CUBE,
	TEXTURE_EXTERNAL_NO_TRANSFORM,
	TEXTURE_EXTERNAL_NO_TRANSFORM_PROJ_W
} texturing_kind;

typedef struct fragment_shadergen_state
{
	/* The stagebits consists of 1+(2*MAX_TEXTURE_STAGES) words.
	 *
	 * Word 0 is where all non-stage state resides, and it also contains the enable bits per stage.
	 *
	 *     2*MAX_TEXTURE_STAGES bits for stage enable bits (interleaved rgb + alpha).
	 *        1 bit stage 0 rgb enable
	 *        1 bit stage 0 alpha enable
	 *        1 bit stage 1 rgb enable
	 *        1 bit stage 1 alpha enable
	 *        ...
	 *        1 bit stage MAX_TEXTURE_STAGES-1 rgb enable
	 *        1 bit stage MAX_TEXTURE_STAGES-1 alpha enable
	 *     5 bits for input color source
	 *     5 bits for result color source
	 *     1 bit for flatshading
	 *     2 bits for fog mode
	 *     1 bit for twosided lightning
	 *     1 bit to enable logic ops rounding fix
	 *     1 bit for clip plane enable vector (one component)
	 *     no bits available for future use
	 *
	 * Each stage is described by 64 bits (2 words) and each stage follow immediately after word 0.
	 *
	 *    bit  7- 0: OOOSSSSS  (operand and source, RGB source 0)
	 *    bit 15- 8: OOOSSSSS  (operand and source, RGB source 1)
	 *    bit 23-16: OOOSSSSS  (operand and source, RGB source 2)
	 *    bit 31-24: various state
	 *      3 bits RGB Combiner
	 *      2 bits RGB scale
	 *      3 bits texture kind
	 *    bit 39-32: OOOSSSSS  (operand and source, ALPHA source 0)
	 *    bit 47-40: OOOSSSSS  (operand and source, ALPHA source 1)
	 *    bit 55-48: OOOSSSSS  (operand and source, ALPHA source 2)
	 *    bit 63-56: various state
	 *      3 bits ALPHA Combiner
	 *      2 bits ALPHA scale
	 *      1 bit point coord replace
	 *      2 bits available for future use
	 *
	 */

	unsigned int bits[2*MAX_TEXTURE_STAGES + 1];

} fragment_shadergen_state;

/* Access to the bits work pretty much like rsw setters.
 *
 * fragment_shadergen_encode(state, SOME_ENUM, value);
 *
 * The SOME_ENUM is actually three parameters: sub-byte, mask and shift.
 *
 * Example:
 * 	FRAGMENT_SHADERGEN_ENCODE(state, FRAGMENT_SHADERGEN_STAGE_ENABLE(2), MALI_TRUE);
 * 	will expand to
 * 	FRAGMENT_SHADERGEN_ENCODE(state, (8*MAX_TEXTURE_STAGES), 0x1, 2, MALI_TRUE);
 * 	which will set the second bit in the 64th byte to true.
 * 	In other words - this will set the bit for stage 2.
 *
 * Getters work in the same fashion:
 *
 * mali_bool value = FRAGMENT_SHADERGEN_DECODE(state, SOME_ENUM)
 */

#define FRAGMENT_SHADERGEN_SOURCE_OFFSET(channel) ((8*(channel)))
#define FRAGMENT_SHADERGEN_SOURCE_MASK 0x1f
#define FRAGMENT_SHADERGEN_OPERAND_OFFSET(channel) (5+ (8*(channel)))
#define FRAGMENT_SHADERGEN_OPERAND_MASK 0x7
#define FRAGMENT_SHADERGEN_COMBINER_OFFSET 24
#define FRAGMENT_SHADERGEN_COMBINER_MASK 0x7
#define FRAGMENT_SHADERGEN_SCALE_OFFSET 27
#define FRAGMENT_SHADERGEN_SCALE_MASK 0x3


/* list of enums */
#define FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stage, channel) (2*(stage) + 1), FRAGMENT_SHADERGEN_SOURCE_MASK, FRAGMENT_SHADERGEN_SOURCE_OFFSET(channel)
#define FRAGMENT_SHADERGEN_STAGE_RGB_OPERAND(stage, channel) (2*(stage) + 1), FRAGMENT_SHADERGEN_OPERAND_MASK, FRAGMENT_SHADERGEN_OPERAND_OFFSET(channel)
#define FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stage, channel) (2*(stage) + 2), FRAGMENT_SHADERGEN_SOURCE_MASK, FRAGMENT_SHADERGEN_SOURCE_OFFSET(channel)
#define FRAGMENT_SHADERGEN_STAGE_ALPHA_OPERAND(stage, channel) (2*(stage) + 2), FRAGMENT_SHADERGEN_OPERAND_MASK, FRAGMENT_SHADERGEN_OPERAND_OFFSET(channel)
#define FRAGMENT_SHADERGEN_STAGE_RGB_COMBINER(stage) (2*(stage) + 1), FRAGMENT_SHADERGEN_COMBINER_MASK, FRAGMENT_SHADERGEN_COMBINER_OFFSET
#define FRAGMENT_SHADERGEN_STAGE_ALPHA_COMBINER(stage) (2*(stage) + 2), FRAGMENT_SHADERGEN_COMBINER_MASK, FRAGMENT_SHADERGEN_COMBINER_OFFSET
#define FRAGMENT_SHADERGEN_STAGE_RGB_SCALE(stage) (2*(stage) + 1), FRAGMENT_SHADERGEN_SCALE_MASK, FRAGMENT_SHADERGEN_SCALE_OFFSET
#define FRAGMENT_SHADERGEN_STAGE_ALPHA_SCALE(stage) (2*(stage) + 2), FRAGMENT_SHADERGEN_SCALE_MASK, FRAGMENT_SHADERGEN_SCALE_OFFSET
#define FRAGMENT_SHADERGEN_STAGE_TEXTURE_POINT_COORDINATE(stage) ((2*(stage)) + 2), 0x1, 29
#define FRAGMENT_SHADERGEN_STAGE_TEXTURE_DIMENSIONALITY(stage) ((2*(stage)) + 1), 0x7, 29
#define FRAGMENT_SHADERGEN_ALL_STAGE_RGB_BITS(stage) (2*(stage) + 1), 0x1FFFFFFF, 0
#define FRAGMENT_SHADERGEN_ALL_STAGE_ALPHA_BITS(stage) (2*(stage) + 2), 0x1FFFFFFF, 0
#define FRAGMENT_SHADERGEN_ALL_STAGE_ENABLE_BITS 0, MAX_TEXTURE_STAGE_MASK, 0
#define FRAGMENT_SHADERGEN_STAGE_RGB_ENABLE(stage) 0, 0x1, (2*(stage))
#define FRAGMENT_SHADERGEN_STAGE_ALPHA_ENABLE(stage) 0, 0x1, (2*(stage) + 1)
#define FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE 0, 0x1f, (2*MAX_TEXTURE_STAGES+0)
#define FRAGMENT_SHADERGEN_RESULT_COLOR_SOURCE 0, 0x1f, (2*MAX_TEXTURE_STAGES+5)
#define FRAGMENT_SHADERGEN_FLATSHADED_COLORS 0, 0x1, (2*MAX_TEXTURE_STAGES+10)
#define FRAGMENT_SHADERGEN_FOG_MODE 0, 0x3, (2*MAX_TEXTURE_STAGES+11)
#define FRAGMENT_SHADERGEN_TWOSIDED_LIGHTING 0, 0x1, (2*MAX_TEXTURE_STAGES+13)

/* temporary until we've migrated all of the users away */
#define FRAGMENT_SHADERGEN_TWOSIDED_LIGHTNING FRAGMENT_SHADERGEN_TWOSIDED_LIGHTING

#define FRAGMENT_SHADERGEN_LOGIC_OPS_ROUNDING_ENABLE 0, 0x1, (2*MAX_TEXTURE_STAGES+14)

#define FRAGMENT_SHADERGEN_CLIP_PLANE_ENABLE(planeid) 0, 0x1, (2*MAX_TEXTURE_STAGES+15+(planeid))

ESSL_STATIC_FORCE_INLINE void fragment_shadergen_encode( fragment_shadergen_state* state, unsigned int subword, unsigned int mask, unsigned int shift, unsigned int value )
{
#if !defined(__SYMBIAN32__) || !defined(MALI_DEBUG_ASSERT) /* This is used in gles, where assert is not available on Symbian. */
	assert( (value & ~mask) == 0 );
#else
	MALI_DEBUG_ASSERT( (value & ~mask) == 0, ("(value & ~mask) != 0") );
#endif
	state->bits[ subword ] &= ~( mask << shift);
    state->bits[ subword ] ^=  ( value << shift);
}

ESSL_STATIC_FORCE_INLINE unsigned int fragment_shadergen_decode( const fragment_shadergen_state* state, unsigned int subword, unsigned int mask, unsigned int shift)
{
	return (( state->bits[ subword ] & ( mask << shift)) >> shift);
}

/* constructor for data matching the FRAGMENT_SHADERGEN_ALL_STAGE_RGB_BITS and FRAGMENT_SHADERGEN_ALL_STAGE_ALPHA_BITS patterns */
#define MAKE_STAGE_DATA(arg0, arg1, arg2, op0, op1, op2, combinemode, scale) \
	(                                                                        \
	   ( (arg0) << FRAGMENT_SHADERGEN_SOURCE_OFFSET(0) )  |                    \
	   ( (arg1) << FRAGMENT_SHADERGEN_SOURCE_OFFSET(1) )  |                    \
	   ( (arg2) << FRAGMENT_SHADERGEN_SOURCE_OFFSET(2) )  |                    \
	   ( (op0) << FRAGMENT_SHADERGEN_OPERAND_OFFSET(0) )  |                    \
	   ( (op1) << FRAGMENT_SHADERGEN_OPERAND_OFFSET(1) )  |                    \
	   ( (op2) << FRAGMENT_SHADERGEN_OPERAND_OFFSET(2) )  |                    \
	   ( (combinemode)  << FRAGMENT_SHADERGEN_COMBINER_OFFSET )  |             \
	   ( (scale) << FRAGMENT_SHADERGEN_SCALE_OFFSET )                          \
	)


#endif /* SHADERGEN_STATE_H */

