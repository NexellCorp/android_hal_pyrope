/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_spinlock.h
 * Implementation of a simple spinlock using GCC intrinsics. A fallback using
 * pthread's spin lock is used if a new version of GCC isn't used.
 * Android's Bionic does not support pthread's spin lock, so this must be used
 * instead.
 * See
 * http://blogs.arm.com/software-enablement/169-locks-swps-and-two-smoking-barriers/
 * for an explanation of why this code is better than using SWP instructions.
 */

#ifndef MALI_SPINLOCK_H_INCLUDED
#define MALI_SPINLOCK_H_INCLUDED

#if CSTD_TOOLCHAIN_RVCT_GCC_MODE || __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ > 4) || ( __GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ > 3)

typedef struct { int flag; } mali_spinlock_t;
#elif defined(ANDROID)
typedef void* mali_spinlock_t;
#else
#include <pthread.h>
typedef pthread_spinlock_t mali_spinlock_t;
#endif

/* check for GCC >= 4.4.3 (need __sync_lock_test_and_set support) Android 4.4.3 is missing support */
#if CSTD_TOOLCHAIN_RVCT_GCC_MODE || __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ > 4) || ( __GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ > 3)

MALI_STATIC_INLINE int mali_spinlock_init( mali_spinlock_t *lock)
{
    lock->flag = 0;
    return 0;
}

MALI_STATIC_INLINE int mali_spinlock_wait(mali_spinlock_t *lock)
{
    while (__sync_lock_test_and_set(&lock->flag, 1));
    return 0;
}

MALI_STATIC_INLINE void mali_spinlock_signal(mali_spinlock_t *lock)
{
    __sync_lock_release(&lock->flag);
}

MALI_STATIC_INLINE void mali_spinlock_destroy(mali_spinlock_t *lock)
{
    /* empty */
}


#elif defined(ANDROID)
MALI_STATIC_INLINE int mali_spinlock_init( mali_spinlock_t *lock)
{
    MALI_DEBUG_ASSERT(MALI_FALSE, ("Spinlock shouldn't be used on gingerbread"));
    return 0;
}

MALI_STATIC_INLINE int mali_spinlock_wait(mali_spinlock_t *lock)
{
    MALI_DEBUG_ASSERT(MALI_FALSE, ("Spinlock shouldn't be used on gingerbread"));
    return 0;
}

MALI_STATIC_INLINE void mali_spinlock_signal(mali_spinlock_t *lock)
{
    MALI_DEBUG_ASSERT(MALI_FALSE, ("Spinlock shouldn't be used on gingerbread"));
    return;
}

MALI_STATIC_INLINE void mali_spinlock_destroy(mali_spinlock_t *lock)
{
    MALI_DEBUG_ASSERT(MALI_FALSE, ("Spinlock shouldn't be used on gingerbread"));
    return;
}

#else
MALI_STATIC_INLINE int mali_spinlock_init( mali_spinlock_t *lock)
{
    return pthread_spin_init(lock, PTHREAD_PROCESS_SHARED);
}

MALI_STATIC_INLINE int mali_spinlock_wait(mali_spinlock_t *lock)
{
    return pthread_spin_lock(lock);
}

MALI_STATIC_INLINE void mali_spinlock_signal(mali_spinlock_t *lock)
{
    pthread_spin_unlock(lock);
}

MALI_STATIC_INLINE void mali_spinlock_destroy(mali_spinlock_t *lock)
{
    pthread_spin_destroy(lock);
}

#endif

#endif
