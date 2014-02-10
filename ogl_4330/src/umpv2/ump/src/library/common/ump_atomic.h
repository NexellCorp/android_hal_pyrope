/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_ump_atomic.h
 * Platform-specific header files for the UMP module
 */
#ifndef _UMP_ATOMIC_H_
#define _UMP_ATOMIC_H_


#ifdef __GNUC__

/* GCC specific atomic ops */

#if defined(__x86_64__) || defined(__i486__) || defined(__i386__)
static INLINE u32 ump_atomic_get(const volatile u32 *val)
{
	return *val;
}

static INLINE void ump_atomic_set(volatile u32 *var, u32 val)
{
	*var = val;
}

static INLINE u32 ump_atomic_add(volatile u32 *var, u32 val)
{
	u32 ret;

#if 1==MALI_VALGRIND
	static pthread_mutex_t alock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&alock);
#endif

	ret = __sync_add_and_fetch( var, val );

#if 1==MALI_VALGRIND
	pthread_mutex_unlock(&alock);
#endif

	return ret;
}

static INLINE u32 ump_atomic_compare_and_swap(volatile u32 *var, u32 old_value,
					      u32 new_value)
{
	u32 ret;

#if 1==MALI_VALGRIND
	static pthread_mutex_t alock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&alock);
#endif

	ret = __sync_val_compare_and_swap( var, old_value, new_value );

#if 1==MALI_VALGRIND
	pthread_mutex_unlock(&alock);
#endif

	return ret;
}
#else

#if defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6__)
#define dmb()	__asm__ volatile("mcr p15, 0, %0, c7, c10, 5" \
				 : : "r" (0) : "memory")
#endif

#if defined(__ARM_ARCH_7A__)
#define dmb()	__asm__ volatile("dmb" : : : "memory")
#endif

static INLINE u32 ump_atomic_get(const volatile u32 *var)
{
	return *var;
}

static INLINE void ump_atomic_set(volatile u32 *var, u32 val)
{
	u32 tmp;

	dmb();
	__asm__ volatile("1:	ldrex	%[tmp], [%[addr]]\n"
			 "	strex	%[tmp], %[val], [%[addr]]\n"
			 "	cmp	%[tmp], #0\n"
			 "	bne	1b"
			 : [tmp] "=&r" (tmp), "+Qo" (*var)
			 : [val] "r" (val), [addr] "r" (var)
			 : "cc");
	dmb();
}

static INLINE u32 ump_atomic_add(volatile u32 *var, u32 val)
{
	u32 res;
	u32 tmp;

	dmb();
	__asm__ volatile("1:	ldrex	%[res], [%[addr]]\n"
			 "	add	%[res], %[res], %[val]\n"
			 "	strex	%[tmp], %[res], [%[addr]]\n"
			 "	cmp	%[tmp], #0\n"
			 "	bne	1b"
			 : [tmp] "=&r" (tmp), [res] "=&r" (res), "+Qo" (*var)
			 : [addr] "r" (var), [val] "r" (val)
			 : "cc");
	dmb();
	return res;
}

static INLINE u32 ump_atomic_compare_and_swap(volatile u32 *var, u32 old_value,
					      u32 new_value)
{
	u32 res;
	u32 tmp;

	dmb();
	__asm__ volatile("1:	ldrex	%[res], [%[addr]]\n"
			 "	mov     %[tmp], #0\n"
			 "	cmp	%[res], %[old_value]\n"
			 "      it	eq\n"
			 "	strexeq	%[tmp], %[new_value], [%[addr]]\n"
			 "	cmp	%[tmp], #0\n"
			 "	bne	1b\n"
			 : [tmp] "=&r" (tmp), [res] "=&r" (res), "+Qo" (*var)
			 : [addr] "r" (var), [new_value] "r" (new_value), [old_value] "r" (old_value)
			 : "cc");
	dmb();
	return res;
}

#endif /* x86 platform */
#endif /* __GNUC__ */
#endif /* _UMP_PLATFORM_H_ */
