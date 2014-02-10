/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

 /*
; =============================================================================
; Function calculates 4bytes per pixel format average
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_downsample8888(
;        const u32*      src,
;        u32 *dst,
;        int      sample_mask);
;   
;  sample mask can be 1111 - 4 pixels average,
                                  1001 - 2 right boarding pixels average,
                                  0011 - 2 bottom boarding pixels average,
                                  0001 - this is a corner pixel in src
# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
#     r0  = src
#     r1  = dst
#     r2  = sample_mask
#
# =============================================================================

*/

__asm void _mali_osu_downsample8888(void)
{

      cmp r2, #15
      bne asm_2_pixels_0011

     ;Proceed with the all 4 pixels
      vld1.8     {d0, d1},  [r0]
      vhadd.u8 d2, d0, d1 
      vrev64.32 d3, d2
      vhadd.u8 d4, d2, d3
      vst1.32   d4[0],  [r1]

      b finish
 
asm_2_pixels_0011:

     ;mask is 0011
      cmp r2, #3
      bne asm_2_pixels_1001

     ;Proceed with the first 2 pixels
      vld1.32     {d0[0]},  [r0]
      vrev64.32 d1, d0
      vhadd.u8 d2, d1, d0
      vst1.32    d2[0], [r1]!

     b finish

asm_2_pixels_1001:

      ;mask is 1001
      cmp r2, #9
      bne asm_2_pixels_0001
      
      vld1.8     {d0,d1},  [r0]
      vrev64.32 d2, d1
      vhadd.u8 d3, d0, d2
      vst1.32    d3[0], [r1]!

     b finish

asm_2_pixels_0001:

      vld1.32     d0[0],  [r0]
      vst1.32    d0[0], [r1]!

finish:

       bx          lr
}

/*
; =============================================================================
; Function calculates 3bytes per pixel format average
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_downsample888(
;        const u32*      src,
;        u32 *dst,
;        int      sample_mask);
;   
;  sample mask can be 1111 - 4 pixels average,
                                  1001 - 2 right boarding pixels average,
                                  0011 - 2 bottom boarding pixels average,
                                  0001 - this is a corner pixel in src
# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
#     r0  = src
#     r1  = dst
#     r2  = sample_mask
#
# =============================================================================
*/
__asm void _mali_osu_downsample888(void)
{
      cmp r2, #15
      bne ds888_asm_2_pixels_0011

     ;Proceed with the all 4 pixels
     ; reading in this way is safe for block interleaved layout
      vld3.8     {d0, d1, d2},  [r0]

     ;r summarize 4 first red pixels and put result/4 into u32
      vpaddl.u8 d3, d0
      vpaddl.u16 d0, d3
      vshr.u32 d0, d0, #2

     ;g summarize 4 first green pixels and put result/4 into u32
      vpaddl.u8 d3, d1
      vpaddl.u16 d1, d3
      vshr.u32 d1, d1, #2

     ;b summarize 4 first blue pixels and put result/4 into u32
      vpaddl.u8 d3, d2
      vpaddl.u16 d2, d3
      vshr.u32 d2, d2, #2

      vst3.8   {d0[0], d1[0], d2[0]},   [r1]!

      b ds888_finish
 
ds888_asm_2_pixels_0011:

      cmp r2, #3
      bne ds888_asm_2_pixels_1001

     ;mask is 0011
     ;Proceed with the first 2 pixels
      vld3.8     {d0[0], d1[0], d2[0]},  [r0]!
      vld3.8     {d0[1], d1[1], d2[1]},  [r0]!
      vld3.8     {d0[2], d1[2], d2[2]},  [r0]!
      vld3.8     {d0[3], d1[3], d2[3]},  [r0]

     ;r
      vpaddl.u8 d3, d0
      vshr.u16 d0, d3, #1
      vand.u32 d0, #0xFF00FFFF
      
     ;g
      vpaddl.u8 d3, d1
      vshr.u16 d1, d3, #1
      vand.u32 d1, #0xFF00FFFF

     ;b
      vpaddl.u8 d3, d2
      vshr.u16 d2, d3, #1
      vand.u32 d2, #0xFF00FFFF
      
      vst3.8   {d0[0], d1[0], d2[0]},   [r1]!

     b ds888_finish

ds888_asm_2_pixels_1001:

      ;mask is 1001
      cmp r2, #9
      bne ds888_asm_2_pixels_0001
      
      vld1.8     {d0, d1},  [r0]

      vshr.u64  d3, d1, #8
      vhadd.u8 d4, d3, d0

      vst1.32   d4[0],  [r1] #here we should have a memory to store 4 bytes

     b ds888_finish

ds888_asm_2_pixels_0001:

     ldr r4, [r0]!
     ldrb r5, [r0]

     str r4, [r1]!
     strb r5, [r1]


ds888_finish:

       bx          lr
}

/*
; =============================================================================
; Function calculates 2bytes per pixel format average
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_downsample88(
;        const u16*      src,
;        u16 *dst,
;        int      sample_mask);
;   
;  sample mask can be 1111 - 4 pixels average,
                                  1001 - 2 right boarding pixels average,
                                  0011 - 2 bottom boarding pixels average,
                                  0001 - this is a corner pixel in src
# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
#     r0  = src
#     r1  = dst
#     r2  = sample_mask
#
# =============================================================================
*/
__asm void _mali_osu_downsample88(void)
{

      cmp r2, #15
      bne ds88_asm_2_pixels_0011

     #Proceed with the all 4 pixels
      vld1.8     {d0},  [r0]

     #1th
     vrev32.16 d1, d0
     vhadd.u8 d2, d0, d1
     vrev64.32 d3, d2
     vhadd.u8 d4, d3, d2
            
     vst1.16   d4[0],  [r1]

     b ds88_finish
 
ds88_asm_2_pixels_0011:

      cmp r2, #3
      bne ds8_asm_2_pixels_1001

     #mask is 0011
     #Proceed with the first 2 pixels
      vld1.8     {d0},  [r0]

     vrev32.16 d1, d0
     vhadd.u8 d2, d0, d1
     
     vst1.16    d2[0], [r1]!

     b ds88_finish

ds88_asm_2_pixels_1001:

      #mask is 1001
      cmp r2, #9
      bne ds88_asm_2_pixels_0001
      
      vld1.8     {d0},  [r0]

      vrev32.16 d1, d0
      vrev64.32 d2, d1

      vhadd.u8 d3, d0, d2
      
      vst1.16    d3[0], [r1]!

     b ds88_finish

ds88_asm_2_pixels_0001:

      vld1.16     {d0[0]},  [r0]
      vst1.16    d0[0], [r1]!

ds88_finish:
       bx          lr
}

/*
; =============================================================================
; Function calculates 1byte per pixel format average
; -----------------------------------------------------------------------------
; Prototype:
;
;    void _mali_osu_downsample8(
;        const u8*      src,
;        u8 *dst,
;        int      sample_mask);
;   
;  sample mask can be 1111 - 4 pixels average,
                                  1001 - 2 right boarding pixels average,
                                  0011 - 2 bottom boarding pixels average,
                                  0001 - this is a corner pixel in src
# -----------------------------------------------------------------------------
# Register usage:
# -----------------------------------------------------------------------------
#     r0  = src
#     r1  = dst
#     r2  = sample_mask
#
# =============================================================================
*/
__asm void _mali_osu_downsample8(void)
{
      cmp r2, #15
      bne ds8_asm_2_pixels_0011

     #Proceed with the all 4 pixels
      vld1.32     {d0[0]},  [r0]

     #r
      vpaddl.u8 d1, d0
      vpaddl.u16 d2, d1
      vshr.u32 d3, d2, #2
            
      vst1.16   d3[0],  [r1]

      b ds8_finish
 
ds8_asm_2_pixels_0011:

      cmp r2, #3
      bne ds8_asm_2_pixels_1001

     #mask is 0011
     #Proceed with the first 2 pixels
      vld1.32     {d0[0]},  [r0]

      vpaddl.u8 d1, d0
      vshr.u16 d2, d1, #1
     
     vst1.16    d2[0], [r1]!

     b ds8_finish

ds8_asm_2_pixels_1001:

      #mask is 1001
      cmp r2, #9
      bne ds8_asm_2_pixels_0001
      
      vld1.32     {d0[0]},  [r0]

      vrev32.16 d1, d0
      vhadd.u8 d2, d0, d1
      
      vst1.16    d2[0], [r1]!

     b ds8_finish

ds8_asm_2_pixels_0001:

     ldrb r4, [r0]
     strb r4, [r1]


ds8_finish:
       bx          lr
}

