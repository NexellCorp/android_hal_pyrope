;/*
;* This confidential and proprietary software may be used only as
;* authorised by a licensing agreement from ARM Limited
;* (C) COPYRIGHT 2009-2011 ARM Limited
;* ALL RIGHTS RESERVED
;* The entire notice above must be reproduced on all authorised
;* copies and copies may only be made to the extent permitted
;* by a licensing agreement from ARM Limited.
;*/

	AREA    |.text|, CODE, READONLY
       ARM                             ; Subsequent instructions are ARM

	EXPORT _mali_osu_atomic_inc_and_return
	EXPORT _mali_osu_atomic_dec_and_return
	EXPORT _mali_osu_atomic_read
	EXPORT _mali_osu_atomic_write
	EXPORT _mali_osu_atomic_init

	MACRO
	MALI_DataMemoryBarrier
#if MALI_SMP==1
#if MALI_PLATFORM_ARM_ARCH==6
	mcr	p15, 0, r0, c7, c10, 5	; dmb
#elif MALI_PLATFORM_ARM_ARCH==7
	dmb
#endif	
#endif
	MEND

	MACRO
	MALI_DataSynchBarrier
#if MALI_SMP==1
#if MALI_PLATFORM_ARM_ARCH==6
	mcr	p15, 0, r0, c7, c10, 5	; dsb
#elif MALI_PLATFORM_ARM_ARCH==7
	dsb
#endif	
#endif
	MEND
	
;u32 _mali_osu_atomic_inc_and_return(volatile u32 * val)

_mali_osu_atomic_inc_and_return

#if MALI_PLATFORM_ARM_ARCH>=6 && !MALI_ARM_ERRATA_351422
L0001
	MALI_DataMemoryBarrier
	ldrex 	r1, [r0]
	add 		r1, r1, #1
	strex 	r2, r1, [r0]
	cmp 	r2, #0
	bne 		L0001
	mov 	r0, r1
	MALI_DataMemoryBarrier
#else
	mov 	r1, r0			; move address so we can access it while writing return value
	mov 	r2, #-100		; setup lock-tag
L0001
	swp 		r3, r2, [r1]
	cmp 	r3, #-100		; did we acquire the lock?
	beq 		L0001
	; modify
	add 		r0, r3, #1

	; release lock
	swp 		r2, r0, [r1]		; r2 is ignored, we just need something there
#endif
	bx 		lr

;u32 _mali_osu_atomic_dec_and_return(volatile u32 * val)

_mali_osu_atomic_dec_and_return

#if MALI_PLATFORM_ARM_ARCH>=6 && !MALI_ARM_ERRATA_351422
L0002
	MALI_DataMemoryBarrier
	ldrex 	r1, [r0]
	sub 		r1, r1, #1
	strex 	r2, r1, [r0]
	cmp 	r2, #0
	bne 		L0002
	mov 	r0, r1
	MALI_DataMemoryBarrier
#else
	mov 	r1, r0   			; move address so we can access it while writing return value
	mov 	r2, #-100 		; setup lock-tag
L0002
	swp 		r3, r2, [r1]
	cmp 	r3, #-100 		;did we acquire the lock?
	beq		L0002

	; modify
	sub 		r0, r3, #1

	; release lock
	swp 		r2, r0, [r1] 		; r2 is ignored, we just need something there
#endif
	bx 		lr

; u32 _mali_osu_atomic_read(mali_atomic_int * atomic)

_mali_osu_atomic_read

#if MALI_PLATFORM_ARM_ARCH>=6 && !MALI_ARM_ERRATA_351422
	MALI_DataMemoryBarrier
	ldr 		r0, [r0]
#else
	mov 	r1, r0   			; move address so we can access it while writing return value
	MALI_DataMemoryBarrier
L0003
	ldr 		r0, [r1]
	cmp 	r0, #-100 		;did we get a non -100 value?
	beq 		L0003
#endif
	bx 		lr

;void _mali_osu_atomic_write(mali_atomic_int * atomic, u32 value)

_mali_osu_atomic_write

#if MALI_PLATFORM_ARM_ARCH>=6 && !MALI_ARM_ERRATA_351422
L0004
	MALI_DataMemoryBarrier
	ldrex 	r2, [r0]
	strex 	r2, r1, [r0]
	cmp 	r2, #0
	bne 		L0004
	MALI_DataMemoryBarrier
#else
	mov 	r2, #-100 		; setup lock-tag
L0004
	swp 		r3, r2, [r0]
	cmp 	r3, #-100 		;did we acquire the lock?
	beq 		L0004

	; swap new value into place
	swp 		r2, r1, [r0]
#endif
	bx 		lr

;_mali_osk_errcode_t _mali_osu_atomic_init(mali_atomic_int * atomic, u32 value)

_mali_osu_atomic_init

	str 		r1, [r0]			; simple store
	mov     	r0, #0 				; Hardwired success code
	bx 			lr

	END

