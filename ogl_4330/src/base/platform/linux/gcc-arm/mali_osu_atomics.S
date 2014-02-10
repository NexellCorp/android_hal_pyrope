/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

.text
.arm

	.macro MALI_DataMemoryBarrier
#if MALI_SMP==1
#if MALI_PLATFORM_ARM_ARCH==6
	mcr	p15, 0, r0, c7, c10, 5	@ dmb
#elif MALI_PLATFORM_ARM_ARCH==7
	dmb
#endif	
#endif
	.endm

	.macro MALI_DataSynchBarrier
#if MALI_SMP==1
#if MALI_PLATFORM_ARM_ARCH==6
	mcr	p15, 0, r0, c7, c10, 4	@ dsb
#elif MALI_PLATFORM_ARM_ARCH==7
	dsb
#endif	
#endif
	.endm

@u32 _mali_osu_atomic_inc_and_return( _mali_osu_atomic_t *atom )
.global _mali_osu_atomic_inc_and_return
.type   _mali_osu_atomic_inc_and_return, %function

_mali_osu_atomic_inc_and_return:

#if MALI_PLATFORM_ARM_ARCH>=6 && !MALI_ARM_ERRATA_351422
1:
	MALI_DataMemoryBarrier
	ldrex 	r1, [r0]
	add 		r1, r1, $1
	strex 	r2, r1, [r0]
	cmp 	r2, $0
	bne 		1b
	mov 	r0, r1
	MALI_DataMemoryBarrier
#else
	mov 	r1, r0   @ move address so we can access it while writing return value
	mov 	r2, $-100 @ setup lock-tag
1:
	swp 		r3, r2, [r1]
	cmp 	r3, $-100 @did we acquire the lock?
	beq 		1b
	@ modify
	add 		r0, r3, $1

	@ release lock
	swp 		r2, r0, [r1] @ r2 is ignored, we just need something there ;)	 
#endif
	bx 		lr

@u32 _mali_osu_atomic_dec_and_return( _mali_osu_atomic_t *atom )
.global _mali_osu_atomic_dec_and_return
.type   _mali_osu_atomic_dec_and_return, %function
 
_mali_osu_atomic_dec_and_return:

#if MALI_PLATFORM_ARM_ARCH>=6 && !MALI_ARM_ERRATA_351422
1:
	MALI_DataMemoryBarrier
	ldrex 	r1, [r0]
	sub 		r1, r1, $1
	strex 	r2, r1, [r0]
	cmp 	r2, $0
	bne 		1b
	mov 	r0, r1
	MALI_DataMemoryBarrier
#else
	mov 	r1, r0   @ move address so we can access it while writing return value
	mov 	r2, $-100 @ setup lock-tag
1:
	swp 		r3, r2, [r1]
	cmp 	r3, $-100 @did we acquire the lock?
	beq 		1b

	@ modify
	sub 		r0, r3, $1

	@ release lock
	swp 		r2, r0, [r1] @ r2 is ignored, we just need something there ;)
#endif
	bx 		lr

@u32 _mali_osu_atomic_read( _mali_osu_atomic_t *atom )
.global _mali_osu_atomic_read
.type   _mali_osu_atomic_read, %function

_mali_osu_atomic_read:
#if MALI_PLATFORM_ARM_ARCH>=6 && !MALI_ARM_ERRATA_351422
	MALI_DataMemoryBarrier
	ldr 		r0, [r0]
#else
	mov 	r1, r0   @ move address so we can access it while writing return value
	MALI_DataMemoryBarrier
1:
	ldr 		r0, [r1]
	cmp 	r0, $-100 @did we get a non -100 value?
	beq 		1b
#endif
	bx 		lr

@void _mali_osu_atomic_write( _mali_osu_atomic_t *atom, u32 val )
.global _mali_osu_atomic_write
.type   _mali_osu_atomic_write, %function

_mali_osu_atomic_write:
#if MALI_PLATFORM_ARM_ARCH>=6 && !MALI_ARM_ERRATA_351422
1:
	MALI_DataMemoryBarrier
	ldrex 	r2, [r0]
	strex 	r2, r1, [r0]
	cmp 	r2, $0
	bne 		1b
	MALI_DataMemoryBarrier
#else
	mov 	r2, $-100 @ setup lock-tag
1:
	swp 		r3, r2, [r0]
	cmp 	r3, $-100 @did we accuire the lock?
	beq 		1b

	@ swap new value into place
	swp 		r2, r1, [r0]
#endif
	bx 		lr

@_mali_osu_errcode_t _mali_osu_atomic_init( _mali_osu_atomic_t *atom, u32 val )
.global _mali_osu_atomic_init
.type   _mali_osu_atomic_init, %function

_mali_osu_atomic_init:
@ simple store
	str 		r1, [r0]
	mov 	r0, $0 @ Hardwired success code
	bx 		lr
