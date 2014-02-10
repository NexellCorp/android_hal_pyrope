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
; 24-bit full block conversion (32bit output)
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_tex24_l_to_tex32_b_full_block(
;        u8*      dst,
;        const u8 *src,
;        u32      src_pitch
;
; -----------------------------------------------------------------------------
; Implementation notes
; -----------------------------------------------------------------------------
; This function works JUST like the 24bit counterpart. The major difference is that 
; the output goes to a 32bit buffer. To that end, all the vst3 instructions have
; been replaced by vst4. 
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
;     d0,d1,d2,d3 = permute_table
;
;     d16,d18,d20 = row_0_data, one byte from each texel in each register
;     d17,d19,d21 = row_1_data, ditto
;     d22,d24,d26 = row_2_data, ditto
;     d23,d25,d27 = row_3_data, ditto
;
;     d4,d5,d6 = out_data
;
; =============================================================================
*/
__asm void _mali_osu_tex24_l_to_tex32_b_full_block(void)
{
	mov         r12, lr ; save LR

	; Load the permute table
	ldr         r3, =permute_table
	vldm        r3, {d0, d1, d2, d3}

	; Permute top left block
	mov         r3, r1
	bl          asm_block_8x8

	; Permute top right block
	add         r3, r1, #24
	bl          asm_block_8x8

	; Permute bottom right block
	add         r3, r1, #24
	add         r3, r3, r2, LSL #3
	bl          asm_block_8x8

    mov         lr, r12 ; restore LR

	; Permute bottom left block
	add         r3, r1, r2, LSL #3

    ; Fall through to asm_block_8x8

; -----------------------------------------------------------------------------
; Internal non-ABI conformant pseudo-function for 8x8 block permute.
; -----------------------------------------------------------------------------
asm_block_8x8

	; Load the upper 8x4 block
	vld3.8      {d16,d18,d20}, [r3], r2
	vld3.8      {d17,d19,d21}, [r3], r2
	vld3.8      {d22,d24,d26}, [r3], r2
	vld3.8      {d23,d25,d27}, [r3], r2

	; Repack the registers so dword registers for each byte are contiguous
	vswp        q9,  q11
	vswp        q10, q11
	vswp        q11, q12

	vtbl.8      d4, {d16, d17, d18, d19}, d0
	vtbl.8      d5, {d20, d21, d22, d23}, d0
	vtbl.8      d6, {d24, d25, d26, d27}, d0
	vst4.8      {d4, d5, d6, d7}, [r0]!

	vtbl.8      d4, {d16, d17, d18, d19}, d1
	vtbl.8      d5, {d20, d21, d22, d23}, d1
	vtbl.8      d6, {d24, d25, d26, d27}, d1
	vst4.8      {d4, d5, d6, d7}, [r0]!

	vtbl.8      d4, {d16, d17, d18, d19}, d2
	vtbl.8      d5, {d20, d21, d22, d23}, d2
	vtbl.8      d6, {d24, d25, d26, d27}, d2
	vst4.8      {d4, d5, d6, d7}, [r0]!

	vtbl.8      d4, {d16, d17, d18, d19}, d3
	vtbl.8      d5, {d20, d21, d22, d23}, d3
	vtbl.8      d6, {d24, d25, d26, d27}, d3
	vst4.8      {d4, d5, d6, d7}, [r0]!

	; Load the lower 8x4 block
	vld3.8      {d16,d18,d20}, [r3], r2
	vld3.8      {d17,d19,d21}, [r3], r2
	vld3.8      {d22,d24,d26}, [r3], r2
	vld3.8      {d23,d25,d27}, [r3], r2

	; Repack the registers so dword registers for each byte are contiguous
	vswp        q9,  q11
	vswp        q10, q11
	vswp        q11, q12

	vtbl.8      d4, {d16, d17, d18, d19}, d2
	vtbl.8      d5, {d20, d21, d22, d23}, d2
	vtbl.8      d6, {d24, d25, d26, d27}, d2
	vst4.8      {d4, d5, d6, d7}, [r0]!

	vtbl.8      d4, {d16, d17, d18, d19}, d3
	vtbl.8      d5, {d20, d21, d22, d23}, d3
	vtbl.8      d6, {d24, d25, d26, d27}, d3
	vst4.8      {d4, d5, d6, d7}, [r0]!

	vtbl.8      d4, {d16, d17, d18, d19}, d0
	vtbl.8      d5, {d20, d21, d22, d23}, d0
	vtbl.8      d6, {d24, d25, d26, d27}, d0
	vst4.8      {d4, d5, d6, d7}, [r0]!

	vtbl.8      d4, {d16, d17, d18, d19}, d1
	vtbl.8      d5, {d20, d21, d22, d23}, d1
	vtbl.8      d6, {d24, d25, d26, d27}, d1
	vst4.8      {d4, d5, d6, d7}, [r0]!

	bx          lr

    ALIGN 8
permute_table 
	DCD         0x08090100
	DCD         0x0A0B0302
	DCD         0x1A1B1312
	DCD         0x18191110
	DCD         0x0C0D0504
	DCD         0x0E0F0706
	DCD         0x1E1F1716
	DCD         0x1C1D1514
}
#endif  /* (MALI_PLATFORM_ARM_NEON) */
