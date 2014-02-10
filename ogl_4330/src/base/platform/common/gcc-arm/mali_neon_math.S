/*
* This confidential and proprietary software may be used only as
* authorised by a licensing agreement from ARM Limited
* (C) COPYRIGHT 2010-2012 ARM Limited
* ALL RIGHTS RESERVED
* The entire notice above must be reproduced on all authorised
* copies and copies may only be made to the extent permitted
* by a licensing agreement from ARM Limited.
*/

 .macro mul_col_f32 res_q, col0_d, col1_d
    vmul.f32    \res_q, q8, \col0_d[0]      @ multiply col element 0 by matrix col 0
    vmla.f32    \res_q, q9, \col0_d[1]      @ multiply-acc col element 1 by matrix col 1
    vmla.f32    \res_q, q10, \col1_d[0] 	@ multiply-acc col element 2 by matrix col 2
    vmla.f32    \res_q, q11, \col1_d[1] 	@ multiply-acc col element 3 by matrix col 3
 .endm

# =============================================================================
# Preamble
# =============================================================================
	.arch armv7a
	.text
	.align 4
	.global _mali_osu_matrix4x4_multiply
	.type   _mali_osu_matrix4x4_multiply, %function
	

_mali_osu_matrix4x4_multiply:
 
    vld1.32     {d16-d19}, [r1]!            @ load first eight elements of matrix 0
    vld1.32     {d20-d23}, [r1]            @ load second eight elements of matrix 0
    vld1.32     {d0-d3}, [r2]!              @load first eight elements of matrix 1
    vld1.32     {d4-d7}, [r2]!              @ load second eight elements of matrix 1

    mul_col_f32 q12, d0, d1         		@ matrix 0 * matrix 1 col 0
    mul_col_f32 q13, d2, d3         		@ matrix 0 * matrix 1 col 1
    mul_col_f32 q14, d4, d5         		@ matrix 0 * matrix 1 col 2
    mul_col_f32 q15, d6, d7         		@ matrix 0 * matrix 1 col 3

    vst1.32     {d24-d27}, [r0]!            @ store first eight elements of result
    vst1.32     {d28-d31}, [r0]            @ store second eight elements of result
    
    bx          lr


# =============================================================================
# Preamble
# =============================================================================
	.arch armv7a
	.text
	.align 4
	.global _mali_osu_matrix4x4_copy
	.type   _mali_osu_matrix4x4_copy, %function
	

_mali_osu_matrix4x4_copy:

    vld1.32     {d16-d19}, [r1]!            @ load first eight elements of matrix 0
    vld1.32     {d20-d23}, [r1]            @ load second eight elements of matrix 0

    vst1.32     {d16-d19}, [r0]!            @ store first eight elements of result
    vst1.32     {d20-d23}, [r0]            @ store second eight elements of result

	bx          lr

# =============================================================================
# Preamble
# =============================================================================
	.arch armv7a
	.text
	.align 4
	.global _mali_osu_matrix4x4_transpose
	.type   _mali_osu_matrix4x4_transpose, %function
	

_mali_osu_matrix4x4_transpose:

    vld1.32     {d16-d19}, [r1]!            @ load first eight elements of matrix 0
    vld1.32     {d20-d23}, [r1]             @ load second eight elements of matrix 0

    vst4.32     {d16,d18,d20,d22}, [r0]!            @ store second eight elements of result
    vst4.32     {d17,d19,d21,d23}, [r0]            @ store first eight elements of result

    
	bx          lr


# =============================================================================
# Preamble
# =============================================================================
	.arch armv7a
	.text
	.align 4
	.global _mali_osu_matrix4x4_translate
	.type   _mali_osu_matrix4x4_translate, %function
#   r0 - src matrix
#   r1 - translation by x
#   r2 - translation by y
#   r3 - translation by z
#
#
#(*mat)[3][0] = (*mat)[0][0] * x + (*mat)[1][0] * y + (*mat)[2][0] * z + (*mat)[3][0];
#(*mat)[3][1] = (*mat)[0][1] * x + (*mat)[1][1] * y + (*mat)[2][1] * z + (*mat)[3][1];
#(*mat)[3][2] = (*mat)[0][2] * x + (*mat)[1][2] * y + (*mat)[2][2] * z + (*mat)[3][2];
#(*mat)[3][3] = (*mat)[0][3] * x + (*mat)[1][3] * y + (*mat)[2][3] * z + (*mat)[3][3];
#
#
_mali_osu_matrix4x4_translate:

    vld1.32     {d16-d19}, [r0]!            @ load first eight elements of matrix
    vld1.32     {d20-d21}, [r0]!            @ load another 4 elements of matrix
    vld1.32     {d22-d23}, [r0]            @ load last  4 elements of matrix and don't shift r0, since the result will be stored right here

    vld1.32     {d0[0]}, [r1]            @ load x translation
    vld1.32     {d1[0]}, [r2]            @ load y translation
    vld1.32     {d2[0]}, [r3]            @ load z translation

    vmla.f32 q11, q8, d0[0]
    vmla.f32 q11, q9, d1[0]
    vmla.f32 q11, q10, d2[0]

    vst1.32     {q11}, [r0]

    
	bx          lr


# =============================================================================
# Preamble
# =============================================================================
	.arch armv7a
	.text
	.align 4
	.global _mali_osu_matrix4x4_scale
	.type   _mali_osu_matrix4x4_scale, %function
#   r0 - src matrix
#   r1 - scale by x
#   r2 - scale by y
#   r3 - scale by z
#
_mali_osu_matrix4x4_scale:


    vld1.32     {d16-d19}, [r0]!            @ load matrix
    vld1.32     {d20-d23}, [r0]
                

    vld1.32     {d0[0]}, [r1]            @ load x translation
    vld1.32     {d1[0]}, [r2]            @ load y translation
    vld1.32     {d2[0]}, [r3]            @ load z translation

    vmul.f32 q8, q8, d0[0]
    vmul.f32 q9, q9, d1[0]
    vmul.f32 q10, q10, d2[0]

    add   r0, #-32 
    
    vst1.32     {d16-d19}, [r0]!
    vst1.32     {d20- d21}, [r0]

    
	bx          lr

