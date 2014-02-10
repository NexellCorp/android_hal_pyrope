/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
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
.global _mali_osu_downsample8888
.type   _mali_osu_downsample8888, %function

# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
#     r0  = src
#     r1  = dst (first 2 bytes)
#     r2  = sample_mask
#
# =============================================================================
_mali_osu_downsample8888:

      cmp r2, #15
      bne asm_2_pixels_0011

      #Proceed with the 8 pixels (2 dst at a time)
      vld1.32     {d0 - d3},  [r0]
      vrhadd.u8 d4, d0, d1 
      vrev64.32 d5, d4
      vrhadd.u8 d4, d4, d5

      vrhadd.u8 d5, d2, d3 
      vrev64.32 d6, d5
      vrhadd.u8 d7, d6, d5

      vtrn.32 d4, d7

      vst1.32   d4,  [r1]

      b finish
 
asm_2_pixels_0011:

     #mask is 0011
      cmp r2, #3
      bne asm_2_pixels_1001

     #Proceed with the first 2 pixels
      vld1.32     {d0},  [r0]
      vrev64.32 d1, d0
      vrhadd.u8 d2, d1, d0
      vst1.32    d2[0], [r1]!

     b finish

asm_2_pixels_1001:

      #mask is 1001
      cmp r2, #9
      bne asm_2_pixels_0001
      
      vld1.32     {d0,d1},  [r0]
      vrev64.32 d2, d1
      vrhadd.u8 d3, d0, d2
      vst1.32    d3[0], [r1]!

     b finish

asm_2_pixels_0001:

      vld1.32     d0[0],  [r0]
      vst1.32    d0[0], [r1]!

finish:

       bx          lr

# =============================================================================
# Preamble
# =============================================================================
.arch armv7a
.text
.align 4
.global _mali_osu_downsample888
.type   _mali_osu_downsample888, %function

# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
#     r0  = src
#     r1  = dst ( first 2 bytes )
#     r2  = sample_mask
#
# =============================================================================
_mali_osu_downsample888:

      cmp r2, #15
      bne ds888_asm_2_pixels_0011

     #Proceed with the all 8 pixels (2 dst at a time)
     # reading in this way is safe for block interleaved layout
      vld3.8     {d0, d1, d2},  [r0]!

     #r summarize 8 red pixels and put result/4 into u32
      vpaddl.u8 d3, d0
      vpaddl.u16 d0, d3
      vshr.u32 d0, d0, #2

     #g summarize 8 green pixels and put result/4 into u32
      vpaddl.u8 d3, d1
      vpaddl.u16 d1, d3
      vshr.u32 d1, d1, #2

     #b summarize 8 blue pixels and put result/4 into u32
      vpaddl.u8 d3, d2
      vpaddl.u16 d2, d3
      vshr.u32 d2, d2, #2

      vst3.8   {d0[0], d1[0], d2[0]},   [r1]!
      vst3.8   {d0[4], d1[4], d2[4]},   [r1]

      b ds888_finish
 
ds888_asm_2_pixels_0011:

      cmp r2, #3
      bne ds888_asm_2_pixels_1001

     #mask is 0011
     #Proceed with the first 2 pixels
      vld3.8     {d0[0], d1[0], d2[0]},  [r0]!
      vld3.8     {d0[1], d1[1], d2[1]},  [r0]

     #r
      vpaddl.u8 d3, d0
      vshr.u16 d0, d3, #1
      
     #g
      vpaddl.u8 d3, d1
      vshr.u16 d1, d3, #1

     #b
      vpaddl.u8 d3, d2
      vshr.u16 d2, d3, #1
      
      vst3.8   {d0[0], d1[0], d2[0]},   [r1]

     b ds888_finish

ds888_asm_2_pixels_1001:

      #mask is 1001
      cmp r2, #9
      bne ds888_asm_2_pixels_0001
      
      vld1.8     {d0, d1},  [r0]

      vshr.u64  d3, d1, #8
      vrhadd.u8 d4, d3, d0

      vst1.32   d4[0],  [r1] @ here we should have a memory to store 4 bytes

     b ds888_finish

ds888_asm_2_pixels_0001:

     push        {r4, lr} @stack all registers and the return address

     ldr r4, [r0]!
     str r4, [r1]!

     ldrb r4, [r0]
     strb r4, [r1]
     
     pop        {r4, pc} @load all the registers  and return automatically

     
ds888_finish:

       bx          lr


# =============================================================================
# Preamble
# =============================================================================
.arch armv7a
.text
.align 4
.global _mali_osu_downsample88
.type   _mali_osu_downsample88, %function

# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
#     r0  = src
#     r1  = dst
#     r2  = sample_mask
#
# =============================================================================
_mali_osu_downsample88:

      cmp r2, #15
      bne ds88_asm_2_pixels_0011

     #Proceed with the all 8 pixels (2 dst at a time)
      vld1.32     {d0,d1},  [r0]

     #1th
     vrev32.16 d2, d0
     vrhadd.u8 d3, d0, d2
     vrev64.32 d4, d3
     vrhadd.u8 d5, d3, d4

     #2th
     vrev32.16 d2, d1
     vrhadd.u8 d3, d1, d2
     vrev64.32 d4, d3
     vrhadd.u8 d6, d3, d4

     vtrn.16 d5, d6

     vst1.32   d6[0],  [r1]

     b ds88_finish
 
ds88_asm_2_pixels_0011:

      cmp r2, #3
      bne ds8_asm_2_pixels_1001

     #mask is 0011
     #Proceed with the first 2 pixels
      vld1.32     {d0},  [r0]

     vrev32.16 d1, d0
     vrhadd.u8 d2, d0, d1
     
     vst1.16    d2[0], [r1]!

     b ds88_finish

ds88_asm_2_pixels_1001:

      #mask is 1001
      cmp r2, #9
      bne ds88_asm_2_pixels_0001
      
      vld1.8     {d0},  [r0]

      vrev32.16 d1, d0
      vrev64.32 d2, d1

      vrhadd.u8 d3, d0, d2
      
      vst1.16    d3[0], [r1]!

     b ds88_finish

ds88_asm_2_pixels_0001:

      vld1.16     {d0[0]},  [r0]
      vst1.16    d0[0], [r1]!

ds88_finish:
       bx          lr

# =============================================================================
# Preamble
# =============================================================================
.arch armv7a
.text
.align 4
.global _mali_osu_downsample8
.type   _mali_osu_downsample8, %function

# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
#     r0  = src
#     r1  = dst
#     r2  = sample_mask
#
# =============================================================================
_mali_osu_downsample8:

      cmp r2, #15
      bne ds8_asm_2_pixels_0011
 
      #Proceed with the all 8 pixels (2 dst at a time)
      vld1.32     {d0},  [r0]

     #r
      vpaddl.u8 d1, d0
      vpaddl.u16 d2, d1
      vshr.u32 d3, d2, #2

      vtrn.32 d3, d4
            
      vst2.8   {d3[0], d4[0]},  [r1]

      b ds8_finish
 
ds8_asm_2_pixels_0011:

      cmp r2, #3
      bne ds8_asm_2_pixels_1001

     #mask is 0011
     #Proceed with the first 2 pixels
      vld1.32     {d0[0]},  [r0]

      vpaddl.u8 d1, d0
      vshr.u16 d2, d1, #1

      vst1.16    d2[0], [r1]

     b ds8_finish

ds8_asm_2_pixels_1001:

      #mask is 1001
      cmp r2, #9
      bne ds8_asm_2_pixels_0001
      
      vld1.32     {d0[0]},  [r0]

      vrev32.16 d1, d0
      vrhadd.u8 d2, d0, d1
      
      vst1.16    d2[0], [r1]!

     b ds8_finish

ds8_asm_2_pixels_0001:

     push        {r4, lr} @stack all registers and the return address

     ldrb r4, [r0]
     strb r4, [r1]

     pop        {r4, pc} @load all the registers  and return automatically


ds8_finish:
       bx          lr

