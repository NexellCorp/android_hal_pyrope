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
; 32-bit full block conversion
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_tex32_l_to_tex32_b_[premult_]full_block(
;        u8*      dst,
;        const u8 *src,
;        u32      src_pitch,
;        [u8*     dst_nonpre]
;        )
; -----------------------------------------------------------------------------
; Implementation notes
; -----------------------------------------------------------------------------
; The algorithm for 32-bit texels operates on four 8x8 blocks, although
; internally is it essentially a 4x4 block algorithm with varying write orders.
;
; Texel data from four source rows is loaded into four quad words (4 texels
; linear per quad word), and these are permuted in place to form the block
; repacked data. This is essentially the identical approach to the 16-bit per 
; texel formats, but with half the number of elements at twice the width.
;
; -----------------------------------------------------------------------------
; Register usage:
; -----------------------------------------------------------------------------
; ARM (ARM ABI: must save r4-r11, r13-r14):
;     r0  = dst
;     r1  = src
;     r2  = src_pitch
;     [r3  = dst_norm] 
;     r12 = src_working
;
; NEON (NEON ABI: must save: d8-d15 / q4-q7):
;     q0-q3   = top left  4x4 block
;     q4-q7   = top right 4x4 block
;     q8-q11  = bottom left  4x4 block
;     q12-q15 = bottom right 4x4 block
;
; =============================================================================
*/

__asm void asm_block_load_8x8_xbgr(void)
{
	vld1.8      {q0}, [r12], r2
	vorr.i32    q0, #0xFF000000
	vld1.8      {q1}, [r12], r2
	vorr.i32    q1, #0xFF000000
	vld1.8      {q2}, [r12], r2
	vorr.i32    q2, #0xFF000000
	vld1.8      {q3}, [r12], r2
	vorr.i32    q3, #0xFF000000

	vrev64.32   q1, q1
	vld1.8      {q12}, [r12], r2
	vorr.i32    q12, #0xFF000000
	veor        d1, d1, d2
	vld1.8      {q13}, [r12], r2
	vorr.i32    q13, #0xFF000000
	veor        d2, d1, d2
	vld1.8      {q14}, [r12], r2
	vorr.i32    q14, #0xFF000000
	veor        d1, d1, d2
	vld1.8      {q15}, [r12], r2
	vorr.i32    q15, #0xFF000000
	sub         r12, r12, r2, LSL #3
	vrev64.32   q3, q3
	add         r12, #16
	veor        d6, d6, d7
	vld1.8      {q4}, [r12], r2
	vorr.i32    q4, #0xFF000000
	veor        d7, d6, d7
	vld1.8      {q5}, [r12], r2
	vorr.i32    q5, #0xFF000000
	veor        d6, d6, d7
	vld1.8      {q6}, [r12], r2
	vorr.i32    q6, #0xFF000000
	veor        d4, d4, d6
	vld1.8      {q7}, [r12], r2
	vorr.i32    q7, #0xFF000000
	veor        d6, d4, d6
	vld1.8      {q8}, [r12], r2
	vorr.i32    q8, #0xFF000000
	veor        d4, d4, d6
	vld1.8      {q9}, [r12], r2
	vorr.i32    q9, #0xFF000000
	veor        d4, d4, d5
	vld1.8      {q10}, [r12], r2
	vorr.i32    q10, #0xFF000000
	veor        d5, d4, d5
	vld1.8      {q11}, [r12], r2
	vorr.i32    q11, #0xFF000000
	veor        d4, d4, d5

	bx          lr
}

__asm void asm_block_load_8x8_rgbx(void)
{
	vld1.8      {q0}, [r12], r2
	vorr.i32    q0, #0xFF
	vld1.8      {q1}, [r12], r2
	vorr.i32    q1, #0xFF
	vld1.8      {q2}, [r12], r2
	vorr.i32    q2, #0xFF
	vld1.8      {q3}, [r12], r2
	vorr.i32    q3, #0xFF

	vrev64.32   q1, q1
	vld1.8      {q12}, [r12], r2
	vorr.i32    q12, #0xFF
	veor        d1, d1, d2
	vld1.8      {q13}, [r12], r2
	vorr.i32    q13, #0xFF
	veor        d2, d1, d2
	vld1.8      {q14}, [r12], r2
	vorr.i32    q14, #0xFF
	veor        d1, d1, d2
	vld1.8      {q15}, [r12], r2
	vorr.i32    q15, #0xFF
	sub         r12, r12, r2, LSL #3
	vrev64.32   q3, q3
	add         r12, #16
	veor        d6, d6, d7
	vld1.8      {q4}, [r12], r2
	vorr.i32    q4, #0xFF
	veor        d7, d6, d7
	vld1.8      {q5}, [r12], r2
	vorr.i32    q5, #0xFF
	veor        d6, d6, d7
	vld1.8      {q6}, [r12], r2
	vorr.i32    q6, #0xFF
	veor        d4, d4, d6
	vld1.8      {q7}, [r12], r2
	vorr.i32    q7, #0xFF
	veor        d6, d4, d6
	vld1.8      {q8}, [r12], r2
	vorr.i32    q8, #0xFF
	veor        d4, d4, d6
	vld1.8      {q9}, [r12], r2
	vorr.i32    q9, #0xFF
	veor        d4, d4, d5
	vld1.8      {q10}, [r12], r2
	vorr.i32    q10, #0xFF
	veor        d5, d4, d5
	vld1.8      {q11}, [r12], r2
	vorr.i32    q11, #0xFF
	veor        d4, d4, d5

	bx          lr
}

__asm void asm_block_store_8x8(void)
{
	; Expects full 8x8 block loaded in {q0-q15}
	; Optimized version, blurring together blocks to gain 10-15% performance
	; But not idea starting point for things like color conversion code

	vstm        r0!, {q0, q1, q2, q3}
	vrev64.32   q5, q5
	veor        d9, d9, d10
	vrev64.32   q7, q7
	veor        d10, d9, d10
	vswp        d14, d15
	veor        d9, d9, d10
	vswp        d12, d14
	veor        d12, d12, d13
	vrev64.32   q9, q9
	veor        d17, d17, d18
	vrev64.32   q11, q11
	veor        d18, d17, d18
	vswp        d22, d23
	veor        d17, d17, d18
	vswp        d20, d22
	veor        d13, d12, d13
	veor        d12, d12, d13
	vstm        r0!, {q4, q5, q6, q7}
	vswp        d20, d21
	vrev64.32   q13, q13
	veor        d25, d25, d26
	vrev64.32   q15, q15
	vstm        r0!, {q8, q9, q10, q11}
	veor        d26, d25, d26
	vswp        d30, d31
	veor        d25, d25, d26
	vswp        d28, d30
	vswp        d28, d29
	vstm        r0!, {q12, q13, q14, q15}

	bx          lr
}

__asm void asm_block_store_premult_8x8( void )
{
	; Expects full 8x8 block loaded in {q0-q15}
	; Alpha is one so we can store nonpre as premult

	vstm        r3!, {q0,q1,q2,q3}
	vstm        r3!, {q4,q5,q6,q7}
	vstm        r3!, {q8,q9,q10,q11}
	vstm        r3!, {q12,q13,q14,q15}

	bx          lr
}

__asm void _mali_osu_tex32_l_to_tex32_b_rgbx_full_block(void)
{
	push        {lr}
	vpush       {q4-q7}

	; Block 1
	mov         r12, r1
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8

	; Block 2
	add         r12, r1, #32   
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8

	; Block 3
	add         r12, r1, #32
	add         r12, r12, r2, LSL #3
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8

	; Block 4
	add         r12, r1, r2, LSL #3
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8

	vpop        {q4-q7}
	pop         {pc}
} 

__asm void _mali_osu_tex32_l_to_tex32_b_xbgr_full_block(void)
{
	push        {lr}
	vpush       {q4-q7}

	; Block 1
	mov         r12, r1
	bl          asm_block_load_8x8_xbgr
	bl          asm_block_store_8x8

	; Block 2
	add         r12, r1, #32    
	bl          asm_block_load_8x8_xbgr
	bl          asm_block_store_8x8

	; Block 3
	add         r12, r1, #32
	add         r12, r12, r2, LSL #3
	bl          asm_block_load_8x8_xbgr
	bl          asm_block_store_8x8

	; Block 4
	add         r12, r1, r2, LSL #3
	bl          asm_block_load_8x8_xbgr
	bl          asm_block_store_8x8

	vpop        {q4-q7}
	pop         {pc}
}

__asm void _mali_osu_tex32_l_to_tex32_b_xbgr_premult_full_block( void )
{
	push        {lr}
	vpush       {q4-q7}

	; Block 1
	mov         r12, r1
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8
	bl          asm_block_store_premult_8x8

	; Block 2
	add         r12, r1, #32   
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8
	bl          asm_block_store_premult_8x8

	; Block 3
	add         r12, r1, #32
	add         r12, r12, r2, LSL #3
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8
	bl          asm_block_store_premult_8x8

	; Block 4
	add         r12, r1, r2, LSL #3
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8
	bl          asm_block_store_premult_8x8

	vpop        {q4-q7}
	pop         {pc}
}

__asm void _mali_osu_tex32_l_to_tex32_b_rgbx_premult_full_block( void )
{
	push        {lr}
	vpush       {q4-q7}

	; Block 1
	mov         r12, r1
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8
	bl          asm_block_store_premult_8x8

	; Block 2
	add         r12, r1, #32   
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8
	bl          asm_block_store_premult_8x8

	; Block 3
	add         r12, r1, #32
	add         r12, r12, r2, LSL #3
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8
	bl          asm_block_store_premult_8x8

	; Block 4
	add         r12, r1, r2, LSL #3
	bl          asm_block_load_8x8_rgbx
	bl          asm_block_store_8x8
	bl          asm_block_store_premult_8x8

	vpop        {q4-q7}
	pop         {pc}
}

#endif  /* (MALI_PLATFORM_ARM_NEON) */

