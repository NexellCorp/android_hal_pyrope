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
; 32-bit linear to linear conversion, swap src's components 2 and 4 
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_tex32_l_to_tex32_l_swap_2_4:(
;        uint8_t*      dst,
;        const uint8_t *src)
;
; -----------------------------------------------------------------------------
; Implementation notes
; -----------------------------------------------------------------------------
; The algorithm for 32-bit texels operates on 32x1 blocks.
;
; Input:  0xAARRGGBB 0xAARRGGBB 0xAARRGGBB 0xAARRGGBB
; Output: 0xAABBGGRR 0xAABBGGRR 0xAABBGGRR 0xAABBGGRR
;
; Two NEON 128-bit registers are used to convert 4 texels
;
; The conversion is performed as follows
; 
; 1-Load four texels from the source into the registers q0 and q1
;
; 2-Reverse q1 
;    
;    0xBBGGRRAA 0xBBGGRRAA 0xBBGGRRAA 0xBBGGRRAA  
;
; 3-Shift left each texel in q0 and insert the result in q1
;     
;    0xRRGGBB00 0xRRGGBB00 0xRRGGBB00 0xRRGGBB00
;
;    0xRRGGBBAA 0xRRGGBBAA 0xRRGGBBAA 0xRRGGBBAA
;
; 4-Reverse q1
;    0xAABBGGRR 0xAABBGGRR 0xAABBGGRR 0xAABBGGRR 
;
; 5-Write the texels in q1 to the destination buffer
,
, -----------------------------------------------------------------------------
; Register usage:
; -----------------------------------------------------------------------------
; ARM (ARM ABI: must save r4-r11, r13-r14):
;     r0  = dst
;     r1  = src
;
; =============================================================================
*/
__asm void _mali_osu_tex32_l_to_tex32_l_swap_2_4(void)
{
	push		{r4-r5,lr}
	vpush		{q4-q7}
	mov		r4, r1
	mov		r5, r0
	vldm		r4, {q0}
	vldm		r4!,{q1}
	vldm		r4, {q2}
	vldm		r4!,{q3}
	vldm		r4, {q4}
	vldm		r4!,{q5}
	vldm		r4, {q6}
	vldm		r4!,{q7}
	vldm		r4, {q8}
	vldm		r4!,{q9}
	vldm		r4, {q10}
	vldm		r4!,{q11}
	vldm		r4, {q12}
	vldm		r4!,{q13}
	vldm		r4, {q14}
	vldm		r4!,{q15}
	vrev32.8	q1, q1
	vsli.32		q1, q0,#8
	vrev32.8	q1, q1
	vrev32.8	q3, q3
	vsli.32		q3, q2,#8
	vrev32.8	q3, q3
	vrev32.8	q5, q5
	vsli.32		q5, q4,#8
	vrev32.8	q5, q5
	vrev32.8	q7, q7
	vsli.32		q7, q6,#8
	vrev32.8	q7, q7
	vrev32.8	q9, q9
	vsli.32		q9, q8,#8
	vrev32.8	q9, q9
	vrev32.8	q11, q11
	vsli.32		q11, q10,#8
	vrev32.8	q11, q11
	vrev32.8	q13, q13
	vsli.32		q13, q12,#8
	vrev32.8	q13, q13
	vrev32.8	q15, q15
	vsli.32		q15, q14,#8
	vrev32.8	q15, q15
	vstm		r5!, {q1}
	vstm		r5!, {q3}
	vstm		r5!, {q5}
	vstm		r5!, {q7}
	vstm		r5!, {q9}
	vstm		r5!, {q11}
	vstm		r5!, {q13}
	vstm		r5!, {q15}
	vpop		{q4-q7}
	pop		{r4-r5,pc}
}

/*
; 32-bit linear to linear conversion, swap src's components 1 and 3 
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_tex32_l_to_tex32_l_swap_1_3(
;        uint8_t*      dst,
;        const uint8_t *src)
;
; -----------------------------------------------------------------------------
; Implementation notes:
; -----------------------------------------------------------------------------
; The algorithm for 32-bit texels operates on 32x1 blocks.
;
; Input:  0xAARRGGBB 0xAARRGGBB 0xAARRGGBB 0xAARRGGBB
; Output: 0xGGRRAABB 0xGGRRAABB 0xGGRRAABB 0xGGRRAABB
;
; Two NEON 128-bit registers are used to convert 4 texels
;
; The conversion is performed as follows
; 
; 1-Load four texels from the source into the registers q0 and q1
;
; 2-Reverse q1 
;    
;    0xBBGGRRAA 0xBBGGRRAA 0xBBGGRRAA 0xBBGGRRAA  
;
; 3-Shift right each texel in q0 and insert the result in q1
;     
;    0x00AARRGG 0x00AARRGG 0x00AARRGG 0x00AARRGG
;
;    0xBBAARRGG 0xBBAARRGG 0xBBAARRGG 0xBBAARRGG
;
; 4-Reverse q1
;    0xGGRRAABB 0xGGRRAABB 0xGGRRAABB 0xGGRRAABB 
;
; 5-Write the texels in q1 to the destination buffer
;
; -----------------------------------------------------------------------------
; Register usage:
; -----------------------------------------------------------------------------
; ARM (ARM ABI: must save r4-r11, r13-r14):
;     r0  = dst
;     r1  = src
;
; =============================================================================
*/
__asm void _mali_osu_tex32_l_to_tex32_l_swap_1_3(void)
{
	push		{r4-r5,lr}
	vpush		{q4-q7}
	mov		r4, r1
	mov		r5, r0
	vldm		r4, {q0}
	vldm		r4!,{q1}
	vldm		r4, {q2}
	vldm		r4!,{q3}
	vldm		r4, {q4}
	vldm		r4!,{q5}
	vldm		r4, {q6}
	vldm		r4!,{q7}
	vldm		r4, {q8}
	vldm		r4!,{q9}
	vldm		r4, {q10}
	vldm		r4!,{q11}
	vldm		r4, {q12}
	vldm		r4!,{q13}
	vldm		r4, {q14}
	vldm		r4!,{q15}
	vrev32.8	q1, q1
	vsri.32		q1, q0,#8
	vrev32.8	q1, q1
	vrev32.8	q3, q3
	vsri.32		q3, q2,#8
	vrev32.8	q3, q3
	vrev32.8	q5, q5
	vsri.32		q5, q4,#8
	vrev32.8	q5, q5
	vrev32.8	q7, q7
	vsri.32		q7, q6,#8
	vrev32.8	q7, q7
	vrev32.8	q9, q9
	vsri.32		q9, q8,#8
	vrev32.8	q9, q9
	vrev32.8	q11, q11
	vsri.32		q11, q10,#8
	vrev32.8	q11, q11
	vrev32.8	q13, q13
	vsri.32		q13, q12,#8
	vrev32.8	q13, q13
	vrev32.8	q15, q15
	vsri.32		q15, q14,#8
	vrev32.8	q15, q15
	vstm		r5!, {q1}
	vstm		r5!, {q3}
	vstm		r5!, {q5}
	vstm		r5!, {q7}
	vstm		r5!, {q9}
	vstm		r5!, {q11}
	vstm		r5!, {q13}
	vstm		r5!, {q15}
	vpop		{q4-q7}
	pop		{r4-r5,pc}
}


/*
; =============================================================================
; 32-bit full block conversion
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_tex32_l_to_tex32_b_full_block(
;        u8*      dst,
;        const u8 *src,
;        u32      src_pitch
;
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
;     r3  = src_working
;
; NEON (NEON ABI: must save: d8-d15 / q4-q7):
;     q0-q3   = top left  4x4 block
;     q4-q7   = top right 4x4 block
;     q8-q11  = bottom left  4x4 block
;     q12-q15 = bottom right 4x4 block
;
; =============================================================================
*/
__asm void _mali_osu_tex32_l_to_tex32_b_full_block(void)
{
	mov         r12, lr ; save LR
	vpush       {q4-q7}

	mov         r3, r1
	bl          asm_block_8x8_v2

	add         r3, r1, #32
	bl          asm_block_8x8_v2

	add         r3, r1, #32
	add         r3, r3, r2, LSL #3
	bl          asm_block_8x8_v2

	add         r3, r1, r2, LSL #3
	bl          asm_block_8x8_v2

	vpop        {q4-q7}
	bx          r12     ; return

; -----------------------------------------------------------------------------
; Internal non-ABI conformant pseudo-function for 8x8 block permute.
; -----------------------------------------------------------------------------
asm_block_8x8_v2
	; Optimized version, blurring together blocks to gain 10-15% performance
	; But not idea starting point for things like color conversion code

	; Load b1
	vld1.8      {q0}, [r3], r2
	vld1.8      {q1}, [r3], r2
	vld1.8      {q2}, [r3], r2
	vld1.8      {q3}, [r3], r2

	vrev64.32   q1, q1
	vld1.8      {q12}, [r3], r2
	veor        d1, d1, d2
	vld1.8      {q13}, [r3], r2
	veor        d2, d1, d2
	vld1.8      {q14}, [r3], r2
	veor        d1, d1, d2
	vld1.8      {q15}, [r3], r2
	sub         r3, r3, r2, LSL #3
	vrev64.32   q3, q3
	add         r3, #16
	veor        d6, d6, d7
	vld1.8      {q4}, [r3], r2
	veor        d7, d6, d7
	vld1.8      {q5}, [r3], r2
	veor        d6, d6, d7
	vld1.8      {q6}, [r3], r2
	veor        d4, d4, d6
	vld1.8      {q7}, [r3], r2
	veor        d6, d4, d6
	vld1.8      {q8}, [r3], r2
	veor        d4, d4, d6
	vld1.8      {q9}, [r3], r2
	veor        d4, d4, d5
	vld1.8      {q10}, [r3], r2
	veor        d5, d4, d5
	vld1.8      {q11}, [r3], r2
	veor        d4, d4, d5
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




#endif  /* (MALI_PLATFORM_ARM_NEON) */

