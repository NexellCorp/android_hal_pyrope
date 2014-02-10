/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#if (MALI_PLATFORM_ARM_NEON)
/*
# =============================================================================
# 32-bit full block conversion
# -----------------------------------------------------------------------------
# Prototype:
#
#    void _mali_osu_tex32_l_to_tex32_b_full_block_premult(
#        uint8_t*      dst_pre,
#        uint8_t*      dst_nonpre,
#        const uint8_t *src,
#        uint32_t      src_pitch
#
# -----------------------------------------------------------------------------
# Implementation notes
# -----------------------------------------------------------------------------
# The algorithm for 32-bit texels. It is essentially a 4x4 block algorithm with
# varying write orders.
#
# Texel data from four source rows is loaded into four quad words (4 texels
# linear per quad word), and these are permuted in place to form the block
# repacked data. This is essentially the identical approach to the 16-bit per 
# texel formats, but with half the number of elements at twice the width.
#
# After block permutation alpha premultiplication is done. We avoid FP-
# arithmetic by using widening multiplies and narrowing shifts to get:
# (c*a>>8) + c*a + 128)>>8 ~= round( c*a/255.0f)
#
# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
# ARM (ARM ABI: must save r4-r11, r13-r14):
#     r0  = dst_pre
#     r1  = dst_nonpre
#     r2  = src
#     r3  = src_pitch
#     r12  = src_working
#
# NEON (NEON ABI: must save: d8-d15 / q4-q7):
#     q0-q3   = top left  4x4 block
#     q4-q7   = top right 4x4 block
#     q8-q11  = bottom left  4x4 block
#     q12-q15 = bottom right 4x4 block
#
# =============================================================================
*/
__asm void asm_block_4x4_premult_b0(void)
{
	; Load and swizzle B0
	vld1.8      {q0}, [r12], r3
	vld1.8      {q1}, [r12], r3
	vld1.8      {q2}, [r12], r3
	vld1.8      {q3}, [r12], r3

	vrev64.32   q1, q1
	veor        d1, d1, d2
	veor        d2, d1, d2
	veor        d1, d1, d2
	vrev64.32   q3, q3
	veor        d6, d6, d7
	veor        d7, d6, d7
	veor        d6, d6, d7
	veor        d4, d4, d6
	veor        d6, d4, d6
	veor        d4, d4, d6
	veor        d4, d4, d5
	veor        d5, d4, d5
	veor        d4, d4, d5
	vstm        r1!, {q0, q1, q2, q3}

	bx          lr
}

__asm void asm_block_4x4_premult_b1(void)
{
	;Load and swizzle B1
	vld1.8      {q0}, [r12], r3
	vld1.8      {q1}, [r12], r3
	vld1.8      {q2}, [r12], r3
	vld1.8      {q3}, [r12], r3
	vrev64.32   q1, q1
	veor        d1, d1, d2
	vrev64.32   q3, q3
	veor        d2, d1, d2
	vswp        d6, d7
	veor        d1, d1, d2
	vswp        d4, d6
	veor        d4, d4, d5
	veor        d5, d4, d5
	veor        d4, d4, d5
	vstm        r1!, {q0, q1, q2, q3}

	bx          lr
}

__asm void asm_block_4x4_premult_b2(void)
{
	;Load and swizzle B2
	vld1.8      {q0}, [r12], r3
	vld1.8      {q1}, [r12], r3
	vld1.8      {q2}, [r12], r3
	vld1.8      {q3}, [r12], r3
	vrev64.32   q1, q1
	veor        d1, d1, d2
	vrev64.32   q3, q3
	veor        d2, d1, d2
	vswp        d6, d7
	veor        d1, d1, d2
	vswp        d4, d6
	vswp        d4, d5
	vstm        r1!, {q0, q1, q2, q3}

	bx          lr
}

__asm void asm_block_4x4_premult_b3(void)
{
	;Load and swizzle B3
	vld1.8      {q0}, [r12], r3
	vld1.8      {q1}, [r12], r3
	vld1.8      {q2}, [r12], r3
	vld1.8      {q3}, [r12], r3
	vrev64.32   q1, q1
	veor        d1, d1, d2
	vrev64.32   q3, q3
	veor        d2, d1, d2
	vswp        d6, d7
	veor        d1, d1, d2
	vswp        d4, d6
	vswp        d4, d5
	vstm        r1!, {q0, q1, q2, q3}

	bx          lr
}

__asm void premultiply_block_reverse(void)
{
	; Premultiply alpha reversed and store to r0.
	; Pre-condition:         q0-q3 32-bits RGBA texels.
	; Pre-condition:         q12 - Alpha mask for premultiplication
	; Pre-condition:         q13 - Bit pattern for alpha replication
	; Pre-condition:         q14 - u16 lanes having value #128
	vand        q8, q0, q12
	vand        q9, q1, q12
	vand        q10, q2, q12
	vand        q11, q3, q12
	vmul.u32    q8, q8, q13
	vmul.u32    q9, q9, q13
	vmul.u32    q10, q10, q13
	vmul.u32    q11, q11, q13
	vorr        q8, q12
	vorr        q9, q12
	vorr        q10, q12
	vorr        q11, q12
	vmull.u8    q4, d0, d16
	vmull.u8    q5, d1, d17
	vmull.u8    q6, d2, d18
	vmull.u8    q7, d3, d19
	vmull.u8    q8, d4, d20
	vmull.u8    q9, d5, d21
	vmull.u8    q10, d6, d22
	vmull.u8    q11, d7, d23
	vsra.u16    q4, q4, #8
	vsra.u16    q5, q5, #8
	vsra.u16    q6, q6, #8
	vsra.u16    q7, q7, #8
	vsra.u16    q8, q8, #8
	vsra.u16    q9, q9, #8
	vsra.u16    q10, q10, #8
	vsra.u16    q11, q11, #8
	vadd.u16    q4, q14
	vadd.u16    q5, q14
	vadd.u16    q6, q14
	vadd.u16    q7, q14
	vadd.u16    q8, q14
	vadd.u16    q9, q14
	vadd.u16    q10, q14
	vadd.u16    q11, q14
	vshrn.u16   d0, q4, #8
	vshrn.u16   d1, q5, #8
	vshrn.u16   d2, q6, #8
	vshrn.u16   d3, q7, #8
	vshrn.u16   d4, q8, #8
	vshrn.u16   d5, q9, #8
	vshrn.u16   d6, q10, #8
	vshrn.u16   d7, q11, #8
	vstm        r0!, {q0, q1, q2, q3}

	bx          lr
}

__asm void premultiply_block(void)
{
	; Premultiply alpha and store to r0.
	; Pre-condition:         q0-q3 32-bits ABGR texels.
	; Pre-condition:         q12 - Alpha mask for premultiplication
	; Pre-condition:         q13 - Bit pattern for alpha replication
	; Pre-condition:         q14 - u16 lanes having value #128
	vand        q8, q0, q12
	vand        q9, q1, q12
	vand        q10, q2, q12
	vand        q11, q3, q12
	vshr.u32	q8, #24
	vshr.u32	q9, #24
	vshr.u32	q10, #24
	vshr.u32	q11, #24
	vmul.u32    q8, q8, q13
	vmul.u32    q9, q9, q13
	vmul.u32    q10, q10, q13
	vmul.u32    q11, q11, q13
	vorr        q8, q12
	vorr        q9, q12
	vorr        q10, q12
	vorr        q11, q12
	vmull.u8    q4, d0, d16
	vmull.u8    q5, d1, d17
	vmull.u8    q6, d2, d18
	vmull.u8    q7, d3, d19
	vmull.u8    q8, d4, d20
	vmull.u8    q9, d5, d21
	vmull.u8    q10, d6, d22
	vmull.u8    q11, d7, d23
	vsra.u16    q4, q4, #8
	vsra.u16    q5, q5, #8
	vsra.u16    q6, q6, #8
	vsra.u16    q7, q7, #8
	vsra.u16    q8, q8, #8
	vsra.u16    q9, q9, #8
	vsra.u16    q10, q10, #8
	vsra.u16    q11, q11, #8
	vadd.u16    q4, q14
	vadd.u16    q5, q14
	vadd.u16    q6, q14
	vadd.u16    q7, q14
	vadd.u16    q8, q14
	vadd.u16    q9, q14
	vadd.u16    q10, q14
	vadd.u16    q11, q14
	vshrn.u16   d0, q4, #8
	vshrn.u16   d1, q5, #8
	vshrn.u16   d2, q6, #8
	vshrn.u16   d3, q7, #8
	vshrn.u16   d4, q8, #8
	vshrn.u16   d5, q9, #8
	vshrn.u16   d6, q10, #8
	vshrn.u16   d7, q11, #8
	vstm        r0!, {q0, q1, q2, q3}

	bx          lr
}

__asm void _mali_osu_tex32_l_to_tex32_b_full_block_premult_reverse(void)
{
	push        {r4, lr}
	vpush       {q4-q7}

	vmov.u32    q12, #0xFF
	vmov.u32    q13, #0x01010101
	vmov.u16    q14, #128

	mov         r12, r2
	bl          asm_block_4x4_premult_b0
	bl          premultiply_block_reverse
	sub         r12, r12, r3, LSL #2
	add         r12, #16
	bl          asm_block_4x4_premult_b1
	bl          premultiply_block_reverse
	bl          asm_block_4x4_premult_b2
	bl          premultiply_block_reverse
	sub         r12, #16
	sub         r12, r12, r3, LSL #2
	bl          asm_block_4x4_premult_b3
	bl          premultiply_block_reverse

	add         r12, r2, #32
	bl          asm_block_4x4_premult_b0
	bl          premultiply_block_reverse
	sub         r12, r12, r3, LSL #2
	add         r12, #16
	bl          asm_block_4x4_premult_b1
	bl          premultiply_block_reverse
	bl          asm_block_4x4_premult_b2
	bl          premultiply_block_reverse
	sub         r12, #16
	sub         r12, r12, r3, LSL #2
	bl          asm_block_4x4_premult_b3
	bl          premultiply_block_reverse

	add         r12, r2, #32
	add         r12, r12, r3, LSL #3
	bl          asm_block_4x4_premult_b0
	bl          premultiply_block_reverse
	sub         r12, r12, r3, LSL #2
	add         r12, #16
	bl          asm_block_4x4_premult_b1
	bl          premultiply_block_reverse
	bl          asm_block_4x4_premult_b2
	bl          premultiply_block_reverse
	sub         r12, #16
	sub         r12, r12, r3, LSL #2
	bl          asm_block_4x4_premult_b3
	bl          premultiply_block_reverse

	add         r12, r2, r3, LSL #3
	bl          asm_block_4x4_premult_b0
	bl          premultiply_block_reverse
	sub         r12, r12, r3, LSL #2
	add         r12, #16
	bl          asm_block_4x4_premult_b1
	bl          premultiply_block_reverse
	bl          asm_block_4x4_premult_b2
	bl          premultiply_block_reverse
	sub         r12, #16
	sub         r12, r12, r3, LSL #2
	bl          asm_block_4x4_premult_b3
	bl          premultiply_block_reverse

	vpop        {q4-q7}
	pop         {r4, pc}
}

__asm void _mali_osu_tex32_l_to_tex32_b_full_block_premult(void)
{
	push        {r4, lr}
	vpush       {q4-q7}

	vmov.u32    q12, #0xFF000000
	vmov.u32    q13, #0x01010101
	vmov.u16    q14, #128

	mov         r12, r2
	bl          asm_block_4x4_premult_b0
	bl          premultiply_block
	sub         r12, r12, r3, LSL #2
	add         r12, #16
	bl          asm_block_4x4_premult_b1
	bl          premultiply_block
	bl          asm_block_4x4_premult_b2
	bl          premultiply_block
	sub         r12, #16
	sub         r12, r12, r3, LSL #2
	bl          asm_block_4x4_premult_b3
	bl          premultiply_block

	add         r12, r2, #32
	bl          asm_block_4x4_premult_b0
	bl          premultiply_block
	sub         r12, r12, r3, LSL #2
	add         r12, #16
	bl          asm_block_4x4_premult_b1
	bl          premultiply_block
	bl          asm_block_4x4_premult_b2
	bl          premultiply_block
	sub         r12, #16
	sub         r12, r12, r3, LSL #2
	bl          asm_block_4x4_premult_b3
	bl          premultiply_block

	add         r12, r2, #32
	add         r12, r12, r3, LSL #3
	bl          asm_block_4x4_premult_b0
	bl          premultiply_block
	sub         r12, r12, r3, LSL #2
	add         r12, #16
	bl          asm_block_4x4_premult_b1
	bl          premultiply_block
	bl          asm_block_4x4_premult_b2
	bl          premultiply_block
	sub         r12, #16
	sub         r12, r12, r3, LSL #2
	bl          asm_block_4x4_premult_b3
	bl          premultiply_block

	add         r12, r2, r3, LSL #3
	bl          asm_block_4x4_premult_b0
	bl          premultiply_block
	sub         r12, r12, r3, LSL #2
	add         r12, #16
	bl          asm_block_4x4_premult_b1
	bl          premultiply_block
	bl          asm_block_4x4_premult_b2
	bl          premultiply_block
	sub         r12, #16
	sub         r12, r12, r3, LSL #2
	bl          asm_block_4x4_premult_b3
	bl          premultiply_block

	vpop        {q4-q7}
	pop         {r4, pc}
}

#endif  /* (MALI_PLATFORM_ARM_NEON) */

