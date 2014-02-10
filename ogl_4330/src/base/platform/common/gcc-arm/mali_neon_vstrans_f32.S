/*
* This confidential and proprietary software may be used only as
* authorised by a licensing agreement from ARM Limited
* (C) COPYRIGHT 2010-2012 ARM Limited
* ALL RIGHTS RESERVED
* The entire notice above must be reproduced on all authorised
* copies and copies may only be made to the extent permitted
* by a licensing agreement from ARM Limited.
*/


 .macro mul_col_f32 res_q, col0_d, col1_d, col2_d, col3_d
    vmul.f32    \res_q, q8, \col0_d      @ multiply col element 0 by matrix col 0
    vmla.f32    \res_q, q9, \col1_d      @ multiply-acc col element 1 by matrix col 1
    vmla.f32    \res_q, q10, \col2_d    @ multiply-acc col element 2 by matrix col 2
    vmla.f32    \res_q, q11, \col3_d    @ multiply-acc col element 3 by matrix col 3
 .endm

 .macro mul_col3_f32 res_q, col0_d, col1_d, col2_d
    vmul.f32    \res_q, q8, \col0_d      @ multiply col element 0 by matrix col 0
    vmla.f32    \res_q, q9, \col1_d      @ multiply-acc col element 1 by matrix col 1
    vmla.f32    \res_q, q10, \col2_d    @ multiply-acc col element 2 by matrix col 2
 .endm

.macro clip_by_w d_x, d_y, d_z, d_w

   vmov.f32 d0, #-1.0
   vmul.f32    d1, \d_w, d0      @ w = (w1,w2), store -w into separate register  
   vmov.u32 d3, #0

   vclt.f32    d2, \d_x, d1  	@ x clipping left
   vshr.u32 d2,  d2, #31
   vorr.u32   d3, d3, d2 

   vcgt.f32    d2, \d_x, \d_w  	@ x clipping right
   vshr.u32 d2,  d2, #31
   vshl.u32  d2, d2, #1
   vorr.u32   d3, d3, d2 

   vcgt.f32    d2, \d_y, \d_w  	@ y clipping top
   vshr.u32 d2,  d2, #31
   vshl.u32  d2, d2, #2
   vorr.u32   d3, d3, d2 

   vclt.f32    d2, \d_y, d1  	@ y clipping bottom
   vshr.u32 d2,  d2, #31
   vshl.u32  d2, d2, #3
   vorr.u32   d3, d3, d2 

   
   vclt.f32    d2, \d_z, d1  	@ z clipping near
   vshr.u32 d2,  d2, #31
   vshl.u32  d2, d2, #4
   vorr.u32   d3, d3, d2 

   vcgt.f32    d2, \d_z, \d_w  	@ z clipping far
   vshr.u32 d2,  d2, #31
   vshl.u32  d2, d2, #5
   vorr.u32   d3, d3, d2 

   vand.u32 d12, d12, d3
   vorr.u32 d13, d13, d3

   vrev64.32 d3, d3
   vand.u32 d12, d12, d3
   vorr.u32 d13, d13, d3

.endm

# =============================================================================
# Preamble
# Macro converts clip space x,y,z,w vector into viewport. reciprocal value is 
# calculated via vrecpe instruction + 2 Newton Raphson 's steps (vrecps).
# NOTE: we transform 2 vectors into viewport at a time 
# input: x1x2 ,  y1y2,  z1z2, w1w2
# output: x1y1 ,  z1w1,  x2y2, z2w2
# =============================================================================
 .macro convert_to_viewport d_x, d_y, d_z, d_w
    vrecpe.f32   d0, \d_w          @ 1/w, store into d0
    vrecps.f32   d2, d0, \d_w      @Few Newton-Raphson's steps 
    vmul.f32     d0, d2, d0
    vrecps.f32   d2, d0, \d_w
    vmul.f32     \d_w, d0, d2

    vmul.f32    d1, \d_x, \d_w  	@ multiply x1, x2 on 1/w1, 1/w2 
    vmul.f32    d2, \d_y, \d_w  	@ multiply y1, y2 on 1/w1, 1/w2 
    vzip.f32     d1, d2           	@  x1x2y1y2 -> x1y1x2y2 

    vmul.f32    d3, d1, d12        	@  x1,y1 times  half_w/half_h 
    vmul.f32    d1, d2, d12        	@  x2,y2 times  half_w/half_h
    
    vadd.f32    \d_x, d3,  d13      @ x1, y1 + offset_x/ offset_y

    vmul.f32    d2, \d_z, \d_w    	@ multiply z1, z2 on 1/w1, 1/w2 
    vmul.f32    d3, d2, d15        	@  z1, z2 times  half_z 
    vadd.f32    \d_y, d3, d14      	@  + offset_z

    vadd.f32    \d_z, d1,  d13      @ x2, y2 + offset_x/ offset_y

    vzip.f32     \d_y, \d_w  		@ z1z2, w1,w2 -> z1w1, z2w2
   
 .endm

# =============================================================================
# Preamble
# Macro converts clip space x,y,z,w vector into viewport. reciprocal value is 
# calculated via vrecpe instruction + 2 Newton Raphson 's steps (vrecps).
# NOTE: we transform 1 vector into viewport at a time 
# =============================================================================
 .macro convert_to_viewport_1vec d_0, d_1
    vrecpe.f32   d0, \d_1          @ 1/w, store into d0[1]
    vrecps.f32   d2, d0, \d_1      @Few Newton-Raphson's steps 
    vmul.f32     d0, d2, d0
    vrecps.f32   d2, d0, \d_1
    vmul.f32     d0, d0, d2

    vmul.f32    d1, \d_0, d0[1]  	@ multiply x,y on 1/w 
    vmul.f32    d2, d1, d12        	@  x,y time 1/w * half_w/half_h 
    vadd.f32    \d_0, d2,  d13      @  1/w*half_w/half_h + offset_x/ offset_y

    vmul.f32    d1, \d_1, d0[1]  	@ multiply z on 1/w  and store into d1[0] 
    vmul.f32    d1, d1, d15[0]      @  z times 1/w * half_z , store into d1[0]

    vadd.f32    \d_1, d1, d14      	@  1/w*half_z + offset_z, store into d25[0]
    vext.32      \d_1, d0, \d_1, #1
    vrev64.32  \d_1, \d_1

 .endm


# =============================================================================
# Preamble
# Function performs a bunch of multiplications like  A*vec4(pos,a), where A - 4x4 f32 matrix, a - f32 constant, pos - f32 3 elements vector
# input: r0 - (4 bytes aligned array)
#         0 - input vectors stream
#         1 - matrix M
#         2 - memory to store the result
#         3 - constant value 
#         4 - alignment
#         5 - input stream stride
#         6 - number of vertexes to handle
# =============================================================================
.arch armv7a
.text
.align 4
.global _mali_neon_matrix4x4_vector3_multiply_f32
.type  _mali_neon_matrix4x4_vector3_multiply_f32, %function

_mali_neon_matrix4x4_vector3_multiply_f32:

    push {r4,r5}
    vpush {d8-d11}
 
    ldr r3, [r0], #4		@ stream_start, pointer to the first element in pos vectors array
    ldr r2, [r0], #4		@ A matrix
    ldr r1, [r0], #4		@ result address

    vld1.f32     {d16-d19}, [r2]!		@ load first eight elements of matrix A
    vld1.f32     {d20-d23}, [r2]		@ load last eight elements of matrix A

    vld1.u32     {d8}, [r0]!		@ load constant values

    ldr r2, [r0], #4		@ stride
    
    ldr     r4, [r0]		@count

    asr r0, r4, #2
    mov r5, r0
    
    vmul.f32    q5,  q11, d8[0]		@ we know that the last element is a constant, so we can premultiply it

    cmp r0, #0
    beq diff

loop:
 
    vld1.f32     {d0-d1}, [r3], r2		@load another 2
    vld1.f32     {d2-d3}, [r3], r2
        
    vld1.f32     {d4-d5}, [r3], r2		@load another 2
    vld1.f32     {d6-d7}, [r3], r2

    mul_col3_f32 q12, d0[0], d0[1], d1[0]             
    mul_col3_f32 q13, d2[0], d2[1], d3[0]             
    
    vadd.f32 q12, q12,q5
    vadd.f32 q13, q13,q5
   
    vst1.f32     {d24-d25}, [r1]!		@ store result
    vst1.f32     {d26-d27}, [r1]!		@ store result

    mul_col3_f32 q12, d4[0], d4[1], d5[0]             
    mul_col3_f32 q13, d6[0], d6[1], d7[0]             
    
    vadd.f32 q12, q12,q5
    vadd.f32 q13, q13,q5

    vst1.f32     {d24-d25}, [r1]!		@ store result
    vst1.f32     {d26-d27}, [r1]!		@ store result

    
    sub r0,r0,#1
    
    cmp   r0, #0
    bgt   loop

diff:

    lsl     r5, r5, #2
    sub   r0, r4, r5

    cmp  r0, #0
    beq  end

    cmp     r0, #1
    beq     last

loop2:
 
    vld1.f32     {d0-d1}, [r3], r2		@load vector
    
    mul_col3_f32 q12, d0[0], d0[1], d1[0]        
 
    vadd.f32 q12, q12,q5

    vst1.f32     {d24-d25}, [r1]!		@ store result

    sub r0,r0,#1
    
    cmp  r0, #1		@ to avoid out of bounds offset last element will be proceeded separetely
    bgt  loop2

last:

    vld1.f32     {d0}, [r3]!		@load vector
    vld1.f32     {d1[0]}, [r3]		@load vector
    
    mul_col3_f32 q12, d0[0], d0[1], d1[0]

    vadd.f32 q12, q12,q5
 
    vst1.f32     {d24-d25}, [r1]!		@ store result

end:

    vpop {d8-d11}
    pop {r4,r5}
    
    bx          lr



# =============================================================================
# Preamble
# Function does pretty much same thing as _mali_neon_matrix4x4_vector3_multiply_f32,
# but in additional performs transformation of vertex into viewport coordinates
# thus we have additional input parameters
#  7:  viewport half width;
#  8:  viewport half height;
#  9:  viewport x offset;
#  10: viewport y offset;
#  11: half depth size;
#  12: viewport z offset;
# =============================================================================
.arch armv7a
.text
.align 4
.global _mali_neon_transform_and_produce_clip_bits
.type  _mali_neon_transform_and_produce_clip_bits, %function

_mali_neon_transform_and_produce_clip_bits:

    vpush {d8-d13}
    push {r4,r5}

    ldr r3, [r0], #4		@ stream_start, pointer to the first element in pos vectors array
    ldr r2, [r0], #4		@ A matrix
    ldr r1, [r0], #4		@ result address

    vld1.f32     {d16-d19}, [r2]!		@ load first eight elements of matrix A
    vld1.f32     {d20-d23}, [r2]		@ load last eight elements of matrix A

    vld1.u32     {d8}, [r0]!		@ load constant values

    add r0, r0, #0x8		@ stride, count
    mov r2, #0x14			@ put 20 into r2 so we can use it to point r3 to zmin
    
    ldr     r4, [r0], #4		@ load clip bits AND output addresses
    ldr     r5, [r0], #4		@ load clip bits OR output addresses


    vmul.f32    q5,  q11, d8[0]		@ we know that the last element is a constant, so we can premultiply it
 
    vld1.f32     {d0-d1}, [r3], r2		@ vector of xmax, ymax, zmax and junk, increment r3 to point to zmin
    vmov 	  q1	, q0
    vld1.f32 	 {d3[0]}	, [r3]
    sub      r3, r3, #4					@ point r3 to ymin

    vmov.u32 d12, #0x3F
    vmov.u32 d13, #0x0
 
    mul_col3_f32 q12, d0[0], d0[1], d1[0]             
    mul_col3_f32 q13, d2[0], d2[1], d3[0]             
    
    vadd.f32 q12, q12,q5
    vadd.f32 q13, q13,q5

    vmov 	  q2	, q0					@ copy the bottom two into the top two
    vmov 	  q3	, q1
    vld1.f32 	 {d4[1]}	, [r3]			@ load ymax to the last two vectors
    vld1.f32 	 {d6[1]}	, [r3]
     
    sub      r3, r3, #4                    @ move r3 to point to xmin

    vzip.f32 q12, q13 					@ x1y1z1w1 x2y2z2w2 -> x1x2y1y2 z1z2w1w2

    clip_by_w d24, d25, d26, d27


    mul_col3_f32 q12, d4[0], d4[1], d5[0]             
    mul_col3_f32 q13, d6[0], d6[1], d7[0]             
    
    vadd.f32 q12, q12,q5
    vadd.f32 q13, q13,q5

    vzip.f32 q12, q13 					@ x1y1z1w1x2y2z2w2 -> x1x2y1y2z1z2w1w2


    clip_by_w  d24, d25, d26, d27

    vmov 	  q0	, q2				@ reload the bottom two vectors becasue they've been stomped on
    vmov 	  q1	, q3

    
    vld1.f32 	 {d0[0]}	, [r3]		@ copy xmin into first term of four vectors
    vld1.f32 	 {d2[0]}	, [r3]
    vld1.f32 	 {d4[0]}	, [r3]
    vld1.f32 	 {d6[0]}	, [r3]
    sub r3, r3, #8
    vld1.f32 	 {d0[1]}	, [r3]			@ load ymax back into the first two vectors
    vld1.f32 	 {d2[1]}	, [r3]

    mul_col3_f32 q12, d0[0], d0[1], d1[0]             
    mul_col3_f32 q13, d2[0], d2[1], d3[0]             
    
    vadd.f32 q12, q12,q5
    vadd.f32 q13, q13,q5

    vzip.f32 q12, q13 					@ x1y1z1w1 x2y2z2w2 -> x1x2y1y2 z1z2w1w2

    clip_by_w  d24, d25, d26, d27

    

    mul_col3_f32 q12, d4[0], d4[1], d5[0]             
    mul_col3_f32 q13, d6[0], d6[1], d7[0]             
    
    vadd.f32 q12, q12,q5
    vadd.f32 q13, q13,q5

    vzip.f32 q12, q13 					@ x1y1z1w1x2y2z2w2 -> x1x2y1y2z1z2w1w2


    clip_by_w  d24, d25, d26, d27

    vst1.u32     {d12[0]}, [r4]!		@ store clip bits
    vst1.u32     {d13[0]}, [r5]!		@

    pop {r4,r5}
    vpop {d8-d13}

    bx          lr


# =============================================================================
# Preamble
# Function searches for min, max by x,y,z in a 4 components stream
# input: r0 - (4 bytes aligned array)
#         0 - input vector stream
#         1 - stride
#         2 - verticies count 
#         3 - output 6 floats buffer 
# =============================================================================
.arch armv7a
.text
.align 4
.global _mali_neon_minmax_4f32
.type  _mali_neon_minmax_4f32, %function

_mali_neon_minmax_4f32:

   vld1.f32     {d0-d1}, [r0], r1		@load 1th vector
   vmov.f32     q3, q0		              @ min
   vmov.f32     q10, q0		              @max

   sub r2,r2,#1
   cmp   r2, #0
   bls minmax_end


minmax_loop:
 
    vld1.f32     {d2-d3}, [r0], r1		@load vector

    vmax.f32  q10, q10, q1
    vmin.f32  q3, q3, q1
        
    sub r2,r2,#1
    
    cmp   r2, #1
    bgt   minmax_loop

    vld1.f32     {d2}, [r0]!		@load the last vector
    vld1.f32     {d3[0]}, [r0]		

    vmax.f32  q10, q10, q1
    vmin.f32  q3, q3, q1

minmax_end:

    vst1.f32     {d6}, [r3]!		@ store min
    vst1.f32     {d7[0]}, [r3]!		

    vst1.f32     {d20}, [r3]!		@ store max
    vst1.f32     {d21[0]}, [r3]!		

    bx lr
    
    
# =============================================================================
# Preamble
# Function does pretty much same thing as _mali_neon_matrix4x4_vector3_multiply_f32,
# but in additional performs transformation of vertex into viewport coordinates
# thus we have additional input parameters
#  7:  viewport half width;
#  8:  viewport half height;
#  9:  viewport x offset;
#  10: viewport y offset;
#  11: half depth size;
#  12: viewport z offset;
# =============================================================================
.arch armv7a
.text
.align 4
.global _mali_neon_matrix4x4_vector3_multiply_and_vp_convert

.type  _mali_neon_matrix4x4_vector3_multiply_and_vp_convert, %function

_mali_neon_matrix4x4_vector3_multiply_and_vp_convert:
    push {r4,r5}
    vpush {d8-d15}

    ldr r3, [r0], #4		@ stream_start, pointer to the first element in pos vectors array
    ldr r2, [r0], #4		@ A matrix
    ldr r1, [r0], #4		@ result address

    vld1.f32     {d16-d19}, [r2]!		@ load first eight elements of matrix A
    vld1.f32     {d20-d23}, [r2]		@ load last eight elements of matrix A

    vld1.u32     {d8}, [r0]!		@ load constant values

    ldr r2, [r0], #4		@ stride
    
    ldr     r4, [r0], #4		@count
    
    vld1.f32     {d12-d14}, [r0]!		@ load viewport transformation matrix

    vdup.f32 d15, d14[0]                      @ half_z, half_z into d10
    vdup.f32 d14, d14[1]                      @ offset_z, offset_z into d14

    asr r0, r4, #2
    mov r5, r0
    
    vmul.f32    q5,  q11, d8[0]		@ we know that the last element is a constant, so we can premultiply it

    cmp r0, #0
    beq vp_convert_diff

vp_convert_loop:
 
    vld1.f32     {d0-d1}, [r3], r2		@load two vectors
    vld1.f32     {d2-d3}, [r3], r2
        
    vld1.f32     {d4-d5}, [r3], r2		@load another 2
    vld1.f32     {d6-d7}, [r3], r2

    mul_col3_f32 q12, d0[0], d0[1], d1[0]             
    mul_col3_f32 q13, d2[0], d2[1], d3[0]             
    
    vadd.f32 q12, q12,q5
    vadd.f32 q13, q13,q5

    vzip.f32 q12, q13 					@ x1y1z1w1 x2y2z2w2 -> x1x2y1y2 z1z2w1w2

    convert_to_viewport d24, d25, d26, d27

    vst1.f32     {d24-d27}, [r1]!		@ store result
    

    mul_col3_f32 q12, d4[0], d4[1], d5[0]             
    mul_col3_f32 q13, d6[0], d6[1], d7[0]             
    
    vadd.f32 q12, q12,q5
    vadd.f32 q13, q13,q5

    vzip.f32 q12, q13 					@ x1y1z1w1x2y2z2w2 -> x1x2y1y2z1z2w1w2


    convert_to_viewport d24, d25, d26, d27
    vst1.f32     {d24-d27}, [r1]!		@ store result
    
    sub r0,r0,#1
    
    cmp   r0, #0
    bgt   vp_convert_loop

vp_convert_diff:

    lsl     r5, r5, #2
    sub   r0, r4, r5

    cmp  r0, #0
    beq  vp_convert_end

    cmp     r0, #1
    beq     vp_convert_last

vp_convert_loop2:
 
    vld1.f32     {d0-d1}, [r3], r2		@load vector
    
    mul_col3_f32 q12, d0[0], d0[1], d1[0]        
 
    vadd.f32 q12, q12,q5

    convert_to_viewport_1vec d24, d25

    vst1.f32     {d24-d25}, [r1]!		@ store result

    sub r0,r0,#1
    
    cmp  r0, #1		@ to avoid out of bounds offset last element will be proceeded separetely
    bgt  vp_convert_loop2

vp_convert_last:

    vld1.f32     {d0}, [r3]!		@load vector
    vld1.f32     {d1[0]}, [r3]		@load vector
    
    mul_col3_f32 q12, d0[0], d0[1], d1[0]

    vadd.f32 q12, q12,q5

    convert_to_viewport_1vec d24, d25

    vst1.f32     {d24-d25}, [r1]!		@ store result

vp_convert_end:

    vpop {d8-d15}
    pop {r4,r5}

    bx          lr




