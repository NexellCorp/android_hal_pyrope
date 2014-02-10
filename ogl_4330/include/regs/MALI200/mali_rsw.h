/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _M200_RSW_H_
#define _M200_RSW_H_

#include <mali_system.h>
#include <string.h>

typedef u32 m200_rsw[16];

/** subword 0, constant color blending */
#define M200_RSW_CONSTANT_BLEND_COLOR_BLUE				0, 0xffff,			0
#define M200_RSW_CONSTANT_BLEND_COLOR_GREEN				0, 0xffff,			16

/** subword 1, constant color blending */
#define M200_RSW_CONSTANT_BLEND_COLOR_RED					1, 0xffff,			0
#define M200_RSW_CONSTANT_BLEND_COLOR_ALPHA				1, 0xffff,			16

/** subword 2, blending parameters */
#define M200_RSW_RGB_BLEND_FUNC							2, 0x7,				0
#define M200_RSW_ALPHA_BLEND_FUNC							2, 0x7,				3
#define M200_RSW_RGB_LOGIC_OP_TRUTH_TABLE                  2, 0xf,             6	/* logicop and framebuffer are mutually exclusive */
#define M200_RSW_RGB_S_SOURCE_SELECT                       2, 0x7,             6
#define M200_RSW_RGB_S_SOURCE_1_M_X						2, 0x1,				9
#define M200_RSW_RGB_S_SOURCE_ALPHA_EXPAND					2, 0x1,				10
#define M200_RSW_RGB_D_SOURCE_SELECT                       2, 0x7,             11
#define M200_RSW_RGB_D_SOURCE_1_M_X						2, 0x1,				14
#define M200_RSW_RGB_D_SOURCE_ALPHA_EXPAND					2, 0x1,				15
#define M200_RSW_ALPHA_LOGIC_OP_TRUTH_TABLE                2, 0xf,             16	/* logicop and framebuffer are mutually exclusive */
#define M200_RSW_ALPHA_S_SOURCE_SELECT                     2, 0x7,             16
#define M200_RSW_ALPHA_S_SOURCE_1_M_X						2, 0x1,				19
#define M200_RSW_ALPHA_D_SOURCE_SELECT                     2, 0x7,             20
#define M200_RSW_ALPHA_D_SOURCE_1_M_X						2, 0x1,				23
#define M200_RSW_RGB_INPUT_COLOR_CLAMP_0_1					2, 0x1,				24
#define M200_RSW_ALPHA_INPUT_COLOR_CLAMP_0_1				2, 0x1,				25
#define M200_RSW_RGB_RESULT_COLOR_CLAMP_0_1				2, 0x1,				26
#define M200_RSW_ALPHA_RESULT_COLOR_CLAMP_0_1				2, 0x1,				27
#define M200_RSW_R_WRITE_MASK                              2, 0x1,             28
#define M200_RSW_G_WRITE_MASK                              2, 0x1,             29
#define M200_RSW_B_WRITE_MASK                              2, 0x1,             30
#define M200_RSW_A_WRITE_MASK                              2, 0x1,             31
#define M200_RSW_ABGR_WRITE_MASK                           2, 0xf,             28 /* combination of the last four */

/* offsets for pre-packing blending:
 * All GLES blending factors are encoded into a "source select", "alpha expand" and "1 minus x".
 * As these fields only depend upon the blending factor and are contiguous we can consider
 * them to be a single five bit field which has direct representations for each of the GLES blend
 * factors.
 *
 * The following defines provide the rsw packing arguments for these five bit fields, as well as
 * the offsets within the field to assist with the construction of the Mali blend factors below.
 *
 * As the alpha blending factors are only four bits (having an implicit 1 for alpha expand), the
 * ALPHA_BLEND_MASK is used when packing mali blend factors for alpha blending.
 */
#define M200_RSW_ONE_1_M_X_OFFSET                            3
#define M200_RSW_ALPHA_EXPAND_OFFSET                         4
#define M200_RSW_ALPHA_BLEND_MASK                            (0xF)
#define M200_RSW_RGB_S_SOURCE_COMBINED                       2, 0x1F,             6
#define M200_RSW_RGB_D_SOURCE_COMBINED                       2, 0x1F,            11
#define M200_RSW_ALPHA_S_SOURCE_COMBINED                     2, 0xF,             16
#define M200_RSW_ALPHA_D_SOURCE_COMBINED                     2, 0xF,             20

/* Mali encodings of GLES blend factors
 */
#define M200_RSW_BLEND_ZERO                     ( 0 << M200_RSW_ALPHA_EXPAND_OFFSET | 0 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_0 )
#define M200_RSW_BLEND_ONE                      ( 0 << M200_RSW_ALPHA_EXPAND_OFFSET | 1 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_0 )
#define M200_RSW_BLEND_SRC_COLOR                ( 0 << M200_RSW_ALPHA_EXPAND_OFFSET | 0 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cs )
#define M200_RSW_BLEND_ONE_MINUS_SRC_COLOR      ( 0 << M200_RSW_ALPHA_EXPAND_OFFSET | 1 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cs )
#define M200_RSW_BLEND_DST_COLOR                ( 0 << M200_RSW_ALPHA_EXPAND_OFFSET | 0 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cd )
#define M200_RSW_BLEND_ONE_MINUS_DST_COLOR      ( 0 << M200_RSW_ALPHA_EXPAND_OFFSET | 1 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cd )
#define M200_RSW_BLEND_SRC_ALPHA                ( 1 << M200_RSW_ALPHA_EXPAND_OFFSET | 0 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cs )
#define M200_RSW_BLEND_ONE_MINUS_SRC_ALPHA      ( 1 << M200_RSW_ALPHA_EXPAND_OFFSET | 1 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cs )
#define M200_RSW_BLEND_DST_ALPHA                ( 1 << M200_RSW_ALPHA_EXPAND_OFFSET | 0 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cd )
#define M200_RSW_BLEND_ONE_MINUS_DST_ALPHA      ( 1 << M200_RSW_ALPHA_EXPAND_OFFSET | 1 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cd )
#define M200_RSW_BLEND_CONSTANT_COLOR           ( 0 << M200_RSW_ALPHA_EXPAND_OFFSET | 0 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cc )
#define M200_RSW_BLEND_ONE_MINUS_CONSTANT_COLOR ( 0 << M200_RSW_ALPHA_EXPAND_OFFSET | 1 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cc )
#define M200_RSW_BLEND_CONSTANT_ALPHA           ( 1 << M200_RSW_ALPHA_EXPAND_OFFSET | 0 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cc )
#define M200_RSW_BLEND_ONE_MINUS_CONSTANT_ALPHA ( 1 << M200_RSW_ALPHA_EXPAND_OFFSET | 1 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_Cc )
#define M200_RSW_BLEND_SRC_ALPHA_SATURATE       ( 0 << M200_RSW_ALPHA_EXPAND_OFFSET | 0 << M200_RSW_ONE_1_M_X_OFFSET | M200_SOURCE_ALPHA_SATURATE )

/** framebuffer blend functions */
#define M200_BLEND_CsS_mCdD                            0
#define M200_BLEND_CdD_mCsS                            1
#define M200_BLEND_CsS_pCdD                            2
#define M200_BLEND_LOGICOP_MODE                        3
#define M200_BLEND_MIN_CsS_pCdD_Cd                     4			/* min( CaA + CsS, Cd ) */
#define M200_BLEND_MAX_CsS_pCdD_Cd                     5			/* max( CaA + CsS, Cd ) */
#define M200_BLEND_MIN_AsSa_pAdDa_Ad					6			/* AsSa + AdDa < Ad ? CsS : CdD */
#define M200_BLEND_MAX_AsSa_pAdDa_Ad					7			/* AsSa + AdDa > Ad ? CsS : CdD */

/** S and D source selections */
#define M200_SOURCE_Cs                                   0
#define M200_SOURCE_Cd                                   1
#define M200_SOURCE_Cc                                   2
#define M200_SOURCE_0                                    3
#define M200_SOURCE_ALPHA_SATURATE                       4
#define M200_SOURCE_Cs2                                  5
/* [ 6, 7 ] undefined */

/** @defgroup logic_op_modes
 *  These are different values for the logic op operations
 *  @{
 */
#define M200_LOGIC_OP_ALWAYS_0      0
#define M200_LOGIC_OP_NOR           1
#define M200_LOGIC_OP_D_NOT_S       2
#define M200_LOGIC_OP_NOT_S         3
#define M200_LOGIC_OP_S_NOT_D       4
#define M200_LOGIC_OP_NOT_D         5
#define M200_LOGIC_OP_XOR           6
#define M200_LOGIC_OP_NAND          7
#define M200_LOGIC_OP_AND           8
#define M200_LOGIC_OP_XNOR          9
#define M200_LOGIC_OP_D             10
#define M200_LOGIC_OP_NOT_NOT_D_S   11
#define M200_LOGIC_OP_S             12
#define M200_LOGIC_OP_NOT_NOT_S_D   13
#define M200_LOGIC_OP_OR            14
#define M200_LOGIC_OP_ALWAYS_1      15
/** @} end of logic_op_modes */

/** subword 3, z operation parameters */
#define M200_RSW_Z_WRITE_MASK								3, 0x1,				0
#define M200_RSW_Z_COMPARE_FUNC							3, 0x7,				1
#define M200_RSW_Z_NEAR_DEPTH_BOUND_OP						3, 0x1,				4
#define M200_RSW_Z_FAR_DEPTH_BOUND_OP						3, 0x1,				5
#define M200_RSW_Z_STENCIL_VALUE_REGISTER					3, 0xf,				6
#define M200_RSW_DST_ENABLE								3, 0x1,				10
#define M200_RSW_SHADER_Z_REPLACE_ENABLE					3, 0x1,				11
#define M200_RSW_SHADER_STENCIL_REPLACE_ENABLE				3, 0x1,				12
/* [ 13, 15] reserved, 0 */
#define M200_RSW_POLYGON_Z_OFFSET_FACTOR					3, 0xff,			16
#define M200_RSW_POLYGON_Z_OFFSET_OFFSET					3, 0xff,			24

/* z bound operations */
#define M200_Z_BOUND_CLAMP								0
#define M200_Z_BOUND_DISCARD							1

/** @defgroup test_funcs
 *  These are different values for the stencil operations
 *  @{
 */
#define M200_TEST_ALWAYS_FAIL                                0
#define M200_TEST_LESS_THAN                                  1
#define M200_TEST_EQUAL                                      2
#define M200_TEST_LESS_THAN_OR_EQUAL                         3
#define M200_TEST_GREATER_THAN                               4
#define M200_TEST_NOT_EQUAL                                  5
#define M200_TEST_GREATER_THAN_OR_EQUAL                      6
#define M200_TEST_ALWAYS_SUCCEED                             7
/** @} end of test_funcs */

/** subword 4, z near and far bounds */
#define M200_RSW_Z_BOUND_MAX								0xffff
#define M200_RSW_Z_NEAR_BOUND								4, 0xffff,			0
#define M200_RSW_Z_FAR_BOUND								4, 0xffff,			16

/** subword 5, front-facing stencil parameters */
#define M200_RSW_STENCIL_FRONT_COMPARE_FUNC				5, 0x7,				0
#define M200_RSW_STENCIL_FRONT_OP_IF_SFAIL                 5, 0x7,             3
#define M200_RSW_STENCIL_FRONT_OP_IF_ZFAIL                 5, 0x7,             6
#define M200_RSW_STENCIL_FRONT_OP_IF_ZPASS                 5, 0x7,             9
/*#define M200_RSW_STENCIL_FRONT_VALUE_REGISTER			5, 0xf,				12*/
/* [ 12, 15] reserved, 0 */

#define M200_RSW_STENCIL_FRONT_REF_VALUE					5, 0xff,			16
#define M200_RSW_STENCIL_FRONT_OP_MASK						5, 0xff,			24

/** subword 6, back-facing stencil parameters */
#define M200_RSW_STENCIL_BACK_COMPARE_FUNC					6, 0x7,				0
#define M200_RSW_STENCIL_BACK_OP_IF_SFAIL                  6, 0x7,             3
#define M200_RSW_STENCIL_BACK_OP_IF_ZFAIL                  6, 0x7,             6
#define M200_RSW_STENCIL_BACK_OP_IF_ZPASS                  6, 0x7,             9
/*#define M200_RSW_STENCIL_BACK_VALUE_REGISTER				6, 0xf,				12*/
/* [ 12, 15] reserved, 0 */
#define M200_RSW_STENCIL_BACK_REF_VALUE					6, 0xff,			16
#define M200_RSW_STENCIL_BACK_OP_MASK						6, 0xff,			24


/** @defgroup stencil_operations
 *  These are different values for the stencil operations
 *  @{
 */
#define M200_STENCIL_OP_KEEP_CURRENT                         0
#define M200_STENCIL_OP_SET_TO_REFERENCE                     1
#define M200_STENCIL_OP_SET_TO_ZERO                          2
#define M200_STENCIL_OP_BITWISE_INVERT                       3
#define M200_STENCIL_OP_INCREMENT                            4
#define M200_STENCIL_OP_DECREMENT                            5
#define M200_STENCIL_OP_SATURATING_INCREMENT                 6
#define M200_STENCIL_OP_SATURATING_DECREMENT                 7

/** @} end of stencil_operations */


/** subword 7, add.stencil parameters, alpha-test reference value */
#define M200_RSW_STENCIL_FRONT_WRITE_MASK					7, 0xff,			0
#define M200_RSW_STENCIL_BACK_WRITE_MASK					7, 0xff,			8
#define M200_RSW_ALPHA_TEST_REF_VALUE						7, 0xffff,			16

/** subword 8, alpha-test function, multisampling parameters */

#define M200_RSW_ALPHA_TEST_FUNC								8, 0x7,				0
#define M200_RSW_MULTISAMPLE_GRID_ENABLE						8, 0x1,				3
#define M200_RSW_4X_SUPERSAMPLING_ENABLE						8, 0x1,				4
#define M200_RSW_MULTISAMPLED_ALPHA_TEST_ENABLE				8, 0x1,				5
#define M200_RSW_MULTISAMPLED_Z_TEST_ENABLE					8, 0x1,				6
#define M200_RSW_SAMPLE_ALPHA_TO_COVERAGE                    8, 0x1,             7
#define M200_RSW_SAMPLE_ALPHA_TO_ONE                         8, 0x1,             8
#define M200_RSW_SAMPLE_COVERAGE_INVERT                      8, 0x1,             9
#define M200_RSW_FLATSHADING_VERTEX_SELECTOR					8, 0x3,				10
#define M200_RSW_MULTISAMPLE_WRITE_MASK						8, 0xf,				12
#define M200_RSW_SAMPLE_0_REGISTER_SELECT					8, 0xf,				16
#define M200_RSW_SAMPLE_1_REGISTER_SELECT					8, 0xf,				20
#define M200_RSW_SAMPLE_2_REGISTER_SELECT					8, 0xf,				24
#define M200_RSW_SAMPLE_3_REGISTER_SELECT					8, 0xf,				28

#define M200_RSW_MULTISAMPLE_WRITE_MASK_ALL 0xf

/** subword 9, shader parameters */
#define M200_RSW_FIRST_SHADER_INSTRUCTION_LENGTH				9, 0x1f,			0
/* 5 undefined */
#define M200_RSW_SHADER_ADDRESS								9, 0xffffffc0,		6 - 6

/** subword 10, Varyings clip base address */
#define M200_RSW_VARYING0_FORMAT 10, 0x7, 0
#define M200_RSW_VARYING1_FORMAT 10, 0x7, 3
#define M200_RSW_VARYING2_FORMAT 10, 0x7, 6
#define M200_RSW_VARYING3_FORMAT 10, 0x7, 9
#define M200_RSW_VARYING4_FORMAT 10, 0x7, 12
#define M200_RSW_VARYING5_FORMAT 10, 0x7, 15
#define M200_RSW_VARYING6_FORMAT 10, 0x7, 18
#define M200_RSW_VARYING7_FORMAT 10, 0x7, 21
#define M200_RSW_VARYING8_FORMAT 10, 0x7, 24
#define M200_RSW_VARYING9_FORMAT 10, 0x7, 27
#define M200_RSW_VARYING10_FORMAT_BITS_0_1 10, 0x3, 30
/* Remaining bit is in subword 15 */

/** @defgroup varying_formats
 *  The different varying formats
 *  @{
 */
#define M200_VARYING_FORMAT_FP32_4C  0
#define M200_VARYING_FORMAT_FP32_2C  1
#define M200_VARYING_FORMAT_FP16_4C  2
#define M200_VARYING_FORMAT_FP16_2C  3
#define M200_VARYING_FORMAT_SFX16_2C 4 /* 1:15 format */
#define M200_VARYING_FORMAT_UFX16_2C 5 /* 0:16 format */
#define M200_VARYING_FORMAT_FX10_3C  6 /* 2:8 per component format */
#define M200_VARYING_FORMAT_FX8_4C   7 /* 2:8 per component format */
/** @} end of varying_formats */

/** subword 11, shader uniforms remapping table pointer */
#define M200_RSW_UNIFORMS_REMAPPING_TABLE_LOG2_SIZE			11, 0xf,				0
#define M200_RSW_UNIFORMS_REMAPPING_TABLE_ADDRESS			11, 0xfffffff0,		4 - 4

/** subword 12, texture descriptor remapping table pointer */
#define M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_LOG2_SIZE	12, 0xf,				0
#define M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_ADDRESS		12, 0xfffffff0,		4 - 4

/** subword 13, vertex data block specifier, performance hints, texture descriptor count */
/* [ 0 ] undefined */
#define M200_RSW_PER_VERTEX_VARYING_BLOCK_SIZE              13, 0x1f,               0
#define M200_RSW_HINT_FETCH_TEX_DESCRIPTOR_REMAPPING_TABLE	13, 0x1,				5
#define M200_RSW_HINT_NO_SHADER_PRESENT						13, 0x1,				6
#define M200_RSW_HINT_FETCH_SHADER_UNIFORMS_REMAPPING_TABLE	13, 0x1,				7
#define M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_TEST				13, 0x1,				8
#define M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_UPDATE			13, 0x1,				9
#define M200_RSW_HINT_SHADER_CONTAINS_DISCARD				13, 0x1,				10
#define M200_RSW_HINT_SHADER_LACKS_RENDEZVOUS				13, 0x1,				11
#define M200_RSW_FORWARD_PIXEL_KILL_PERMISSIBLE				13, 0x1,				12

/* [ 13] undefined  */
#define M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_SIZE			13, 0x3fff,				14
#define M200_RSW_REGISTER_SELECT_FOR_Cs2						13, 0xf,				28

/** subword 14, stencil clear tag, number of uniforms */
#define M200_RSW_STENCIL_TAG_BITS							14, 0xf,				0
#define M200_RSW_STENCIL_CLEAR_TAG							14, 0xff,				4
#define M200_RSW_POLYGON_ORIENTATION_FLAG					14, 0x1,				12
#define M200_RSW_DITHERING_ENABLE							14, 0x1,				13
#define M200_RSW_ROP4_ENABLE								14, 0x1,				14
/* [ 15] undefined  */
#define M200_RSW_UNIFORMS_REMAPPING_TABLE_SIZE				14, 0xffff,				16

/** subword 15, varyings base address */
#define M200_RSW_VARYING10_FORMAT_BIT_2 15, 0x1, 0
#define M200_RSW_VARYING11_FORMAT  15, 0x7, 1
#define M200_RSW_VARYINGS_BASE_ADDRESS						15, 0xfffffff0,		4-4

/* the required alignment is 8, but higher alignment suits better */
#define M200_FRAMEBUFFER_ALIGNMENT      32
#define M200_UNIFORM_BLOCK_ALIGNMENT     ( 4 * 2 )           /* 4-component vector of f16 datatypes */
#define M200_UNIFORM_BLOCK_ALIGNMENT_LOG2	 3
#define M200_TILE_LIST_ALIGNMENT         32
#define M200_RSW_ALIGNMENT               64
#define M200_SHADER_ALIGNMENT            64
#define M200_TEXTURE_ALIGNMENT           64
#define M200_TD_ALIGNMENT                64
#define M200_VERTEX_ALIGNMENT            64
#define M200_VARYINGS_ALIGNMENT          16
#define M200_REMAP_TABLE_ALIGNMENT       16
#define M200_REMAP_TABLE_ALIGNMENT_LOG2	 4

#define M200_POLYGON_ORIENTATION_CLOCKWISE 0
#define M200_POLYGON_ORIENTATION_ANTI_CLOCKWISE 1

/**
 * Encodes a state into the render state word (rsw)
 * @param rsw
 * @param subword denotes which 32 bit word of the total rsw to write
 * @param bitmask the bitmask which will mask out the previous value in the rsw @a subword
 * @param shift the start position of inside the @a subword where your value should be written
 * @note this function is usually used with the macros in m200_rsw.h that incapsulates the parameters
   @a subword @a bitmask and @a shift into a single name.
 * @code Example
 * @code #define M200_RSW_DITHERING_ENABLE 14,0x1,14
 * @code m200_rsw rsw;
 * @code m200_rsw_encode( rsw, M200_RSW_DITHERING_ENABLE, 1 );
 * @code equals
 * @code m200_rsw_encode( rsw, 14, 0x1, 14, 1 );
 */

MALI_STATIC_FORCE_INLINE void __m200_rsw_encode( m200_rsw *rsw, u32 subword, u32 _bitmask, u32 shift, u32 value )
{
	( ( u32 *) rsw ) [ subword ] &= ~( _bitmask << shift);
	( ( u32 *) rsw ) [ subword ] ^=  ( value   << shift);
}

MALI_STATIC_INLINE u32 __m200_rsw_decode( m200_rsw *rsw, u32 subword, u32 _bitmask, u32 shift )
{
	return ( ( ( u32 * ) rsw ) [ subword ] >> shift ) & _bitmask;
}

#endif /* _M200_RSW_H_ */
