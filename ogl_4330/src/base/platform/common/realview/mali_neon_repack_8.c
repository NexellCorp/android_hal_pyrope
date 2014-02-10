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
; =============================================================================
; 8-bit full block conversion
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_tex8_l_to_tex8_b_full_block(
;        u8*      dst,
;        const u8 *src,
;        u32      src_pitch
;
; -----------------------------------------------------------------------------
; Implementation notes
; -----------------------------------------------------------------------------
; The algorithm for 8-bit texels operates on two 16x8 blocks. We will
; load the upper 16x8 blocks, then do 2x2, 4x4, 8x8 reordering. The last 16x16-
; reordering is done on write back. The same for the lower 16x8 blocks.
;
; -----------------------------------------------------------------------------
; Register usage:
; -----------------------------------------------------------------------------
; ARM (ARM ABI: must save r4-r11, r13-r14):
;     r0  = dst
;     r1  = src
;     r2  = src_pitch
;     r3  = src_working
;
; NEON (NEON ABI: must save: d8-d15 / q4-q7):
;     d16-d23 = block1 data
;     d24-d31 = block2 data
;
; =============================================================================
*/
__asm void _mali_osu_tex8_l_to_tex8_b_full_block(void)
{
	; Loads upper 16x8 interleaved with
	; 2x2, 4x4 and 8x8 reordering.
	vld1.8    {q0}, [r1], r2
	vld1.8    {q1}, [r1], r2
	vrev16.8  q1, q1
	vld1.8    {q2}, [r1], r2
	vrev32.16 q2, q2
	vld1.8    {q3}, [r1], r2
	vrev16.8  q3, q3
	vrev32.16 q3, q3
	vld1.8    {q12}, [r1], r2
	vrev64.32 q12, q12
	vld1.8    {q13}, [r1], r2
	vrev16.8  q13, q13
	vrev64.32 q13, q13
	vld1.8    {q14}, [r1], r2
	vrev32.16 q14, q14
	vrev64.32 q14, q14
	vld1.8    {q15}, [r1], r2
	vrev16.8  q15, q15
	vrev32.16 q15, q15
	vrev64.32 q15, q15

	; Writes upper 16x8 interleaved with 16x16
	; reordering and component interleaving.
	vzip.16   q0,q1
	vzip.16   q2,q3
	vswp      d1, d4
	vst1.8    {q0}, [r0]!
	vswp      d3, d6
	vst1.8    {q2}, [r0]!
	vzip.16   q12,q13
	vzip.16   q14,q15
	vswp      d25, d28
	vswp      d27, d30
	vst1.8    {q12}, [r0]!
	vst1.8    {q14}, [r0]!
	vst1.8    {q1}, [r0]!
	vst1.8    {q3}, [r0]!
	vst1.8    {q13}, [r0]!
	vst1.8    {q15}, [r0]!

	; Loads lower 16x8 interleaved with
	; 2x2, 4x4 and 8x8 reordering.
	vld1.8    {q8}, [r1], r2
	vld1.8    {q9}, [r1], r2
	vrev16.8  q9, q9
	vld1.8    {q10}, [r1], r2
	vrev32.16 q10, q10
	vld1.8    {q11}, [r1], r2
	vrev16.8  q11, q11
	vrev32.16 q11, q11
	vld1.8    {q12}, [r1], r2
	vrev64.32 q12, q12
	vld1.8    {q13}, [r1], r2
	vrev16.8  q13, q13
	vrev64.32 q13, q13
	vld1.8    {q14}, [r1], r2
	vrev32.16 q14, q14
	vrev64.32 q14, q14
	vld1.8    {q15}, [r1], r2
	vrev16.8  q15, q15
	vrev32.16 q15, q15
	vrev64.32 q15, q15

	; Writes lower 16x8 interleaved with 16x16
	; reordering and component interleaving.
	vzip.16   q8,q9
	vzip.16   q10,q11
	vswp      d17, d20
	vswp      d19, d22
	vst1.8    {q9}, [r0]!
	vst1.8    {q11}, [r0]!
	vzip.16   q12,q13
	vzip.16   q14,q15
	vswp      d25, d28
	vswp      d27, d30
	vst1.8    {q13}, [r0]!
	vst1.8    {q15}, [r0]!
	vst1.8    {q8}, [r0]!
	vst1.8    {q10}, [r0]!
	vst1.8    {q12}, [r0]!
	vst1.8    {q14}, [r0]!

	bx        lr
}
#endif  /* (MALI_PLATFORM_ARM_NEON) */
