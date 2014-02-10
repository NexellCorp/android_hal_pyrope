/*
* This confidential and proprietary software may be used only as
* authorised by a licensing agreement from ARM Limited
* (C) COPYRIGHT 2010-2011 ARM Limited
* ALL RIGHTS RESERVED
* The entire notice above must be reproduced on all authorised
* copies and copies may only be made to the extent permitted
* by a licensing agreement from ARM Limited.
*/

# =============================================================================
# Preamble
# =============================================================================
	.arch armv7a
	.text
	.align 4
	.global _mali_osu_tex16_l_to_tex16_b_full_block
	.type   _mali_osu_tex16_l_to_tex16_b_full_block, %function
# =============================================================================

# =============================================================================
# 16-bit full block conversion
# -----------------------------------------------------------------------------
# Prototype:
#
#    void _mali_osu_tex16_l_to_tex16_b_full_block(
#        u8*      dst,
#        const u8 *src,
#        u32      src_pitch
#
# -----------------------------------------------------------------------------
# Implementation notes
# -----------------------------------------------------------------------------
# The algorithm for 16-bit texels operates on four 8x8 blocks, although 
# internally is it essentially a 8x4 block algorithm with a slight change in
# the write order for the second 8x4 block (write out the 4x4 sub-blocks in
# reverse). We assume co-ordinate 0,0 is the top left of the texture, although
# is has no effect on algorithm (just helps with comments).
#
# Texel data from four source rows is loaded into four quad words (8 texels
# linear per quad word), and these are permuted in place to form the block 
# repacked data.
#
# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
# ARM (ARM ABI: must save r4-r11, r13-r14):
#     r0  = dst
#     r1  = src
#     r2  = src_pitch
#     r3  = src_working
#
# NEON (NEON ABI: must save: d8-d15 / q4-q7):
#     d16-d23 = block1 data
#     d24-d31 = block2 data
#
# =============================================================================
_mali_osu_tex16_l_to_tex16_b_full_block:

	push        {r4, lr}

	# Execute on top left 8x8 block
	mov         r3, r1
	bl          asm_block_8x8

	# Execute on top right 8x8 block
	add         r3, r1, #16
	bl          asm_block_8x8

	# Execute on bottom left 8x8 block
	add         r3, r1, #16
	add         r3, r3, r2, LSL #3
	bl          asm_block_8x8

	# Execute on bottom right 8x8 block
	add         r3, r1, r2, LSL #3
	bl          asm_block_8x8

	pop         {r4, pc}

# -----------------------------------------------------------------------------
# Internal non-ABI conformant pseudo-function for 8x8 block permute.
# -----------------------------------------------------------------------------
asm_block_8x8:
	# Load upper 8x4 block  - src has only 16-bit alignment guarantee
	vld1.16     {q8},  [r3], r2
	vld1.16     {q9},  [r3], r2
	vld1.16     {q10}, [r3], r2
	vld1.16     {q11}, [r3], r2

	# Permute upper 8x4 block
	vrev32.16   q9,  q9
	vzip.32     q8,  q9
	vrev32.16   q11, q11
	vzip.32     q10, q11

	# Load lower 8x4 block, interleaved with top block permute
	veor        q9, q10
	vld1.16     {q12}, [r3], r2
	veor        q10, q9
	vld1.16     {q13}, [r3], r2
	veor        q9, q10
	vld1.16     {q14}, [r3], r2
	vswp        d18, d19
	vld1.16     {q15}, [r3], r2

	# Store upper 8x4 block
	vswp        d22, d23
	vstm        r0!, {q8, q9, q10, q11}

	# Permute bottom 8x4 block and store it (no scope for interleave)
	vrev32.16   q13, q13
	vrev32.16   q15, q15
	vzip.32     q12, q13
	vzip.32     q14, q15
	vswp        q13, q14
	vswp        d26, d27
	vswp        d30, d31

	# Write back reverse order 4x4 blocks
	vstm        r0!, {q14, q15}
	vstm        r0!, {q12, q13}

	bx          lr
