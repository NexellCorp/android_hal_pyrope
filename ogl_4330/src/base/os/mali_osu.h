/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_osu.h
 * Defines the OS abstraction layer for the base driver
 */

#ifndef __MALI_OSU_H__
#define __MALI_OSU_H__

#include <stdarg.h>
#include <mali_system.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @addtogroup uddapi Unified Device Driver (UDD) APIs
 *
 * @{
 */

/**
 * @defgroup osuapi UDD OS Abstraction for User-side (OSU) APIs
 *
 * @note The integral types used by the OSU are inherited from file mali_types.h
 *
 * @{
 */


/* The following is necessary to prevent the _mali_osk_errcode_t doxygen from
 * becoming unreadable: */
/** @cond OSU_COPY_OF__MALI_OSU_ERRCODE_T */

/**
 * @brief OSU/OSK Error codes.
 *
 * Each OS may use its own set of error codes, and may require that the
 * User/Kernel interface take certain error code. This means that the common
 * error codes need to be sufficiently rich to pass the correct error code
 * through from the OSK/OSU to U/K layer, across all OSs.
 *
 * The result is that some error codes will appear redundant on some OSs.
 * Under all OSs, the OSK/OSU layer must translate native OS error codes to
 * _mali_osk/u_errcode_t codes. Similarly, the U/K layer must translate from
 * _mali_osk/u_errcode_t codes to native OS error codes.
 *
 */
typedef enum
{
    _MALI_OSK_ERR_OK = 0,              /**< Success. */
    _MALI_OSK_ERR_FAULT = -1,          /**< General non-success */
    _MALI_OSK_ERR_INVALID_FUNC = -2,   /**< Invalid function requested through User/Kernel interface (e.g. bad IOCTL number) */
    _MALI_OSK_ERR_INVALID_ARGS = -3,   /**< Invalid arguments passed through User/Kernel interface */
    _MALI_OSK_ERR_NOMEM = -4,          /**< Insufficient memory */
    _MALI_OSK_ERR_TIMEOUT = -5,        /**< Timeout occured */
    _MALI_OSK_ERR_RESTARTSYSCALL = -6, /**< Special: On certain OSs, must report when an interruptable mutex is interrupted. Ignore otherwise. */
    _MALI_OSK_ERR_ITEM_NOT_FOUND = -7, /**< Table Lookup failed */
    _MALI_OSK_ERR_BUSY = -8,           /**< Device/operation is busy. Try again later */
	_MALI_OSK_ERR_UNSUPPORTED = -9,    /**< Optional part of the interface used, and is unsupported */
} _mali_osk_errcode_t;

/** @endcond */ /* end cond OSU_COPY_OF__MALI_OSU_ERRCODE_T */

/**
 * @brief OSU Error codes.
 *
 * OSU error codes - enum values intentionally same as OSK
 */
typedef enum
{
    _MALI_OSU_ERR_OK = 0,           /**< Success. */
    _MALI_OSU_ERR_FAULT = -1,       /**< General non-success */
    _MALI_OSU_ERR_TIMEOUT = -2,     /**< Timeout occured */
} _mali_osu_errcode_t;

/** @brief Translate OSU error code to base driver error code.
 *
 * The _MALI_OSU_TRANSLATE_ERROR macro translates an OSU error code to the
 * error codes in use by the base driver.
 */
#define _MALI_OSU_TRANSLATE_ERROR(_mali_osu_errcode) ( ( _MALI_OSU_ERR_OK == (_mali_osu_errcode) ) ? MALI_ERR_NO_ERROR : MALI_ERR_FUNCTION_FAILED)

/** @defgroup _mali_osu_atomic OSU Atomic counters
 * @{ */

/** @brief Public type of atomic counters.
 *
 * This is public for allocation on stack. On systems that support it, this is just a single 32-bit value.
 * On others, it could be encapsulating an object stored elsewhere.
 *
 * Even though the structure has space for a u32, the counters will only
 * represent 24-bit signed-integers.
 *
 * Regardless of implementation, the _mali_osu_atomic functions \b must be used
 * for all accesses to the variable's value, even if atomicity is not requried.
 * Do not access u.val or u.obj directly.
 *
 * @note the definition of this type is found in mali_types.h
 */

typedef mali_atomic_int _mali_osu_atomic_t;
/** @} */ /* end group _mali_osu_atomic */



/** @defgroup _mali_osu_lock OSU Mutual Exclusion Locks
  * @{ */

/** @brief OSU Mutual Exclusion Lock flags type.
 *
 * This is made to look like and function identically to the OSK locks (refer
 * to \ref _mali_osk_lock). However, please note the following \b important
 * differences:
 * - the OSU default lock is a Sleeping, non-interruptible mutex.
 * - the OSU adds the ANYUNLOCK type of lock which allows a thread which doesn't
 * own the lock to release the lock.
 * - the order parameter when creating a lock is currently unused
 *
 * @note Pay careful attention to the difference in default locks for OSU and
 * OSK locks; OSU locks are always non-interruptible, but OSK locks are by
 * default, interruptible. This has implications for systems that do not
 * distinguish between user and kernel mode.
 */
 typedef enum
{
	_MALI_OSU_LOCKFLAG_DEFAULT = 0, /**< Default lock type. */
	/** @enum _mali_osu_lock_flags_t
	 *
	 * Flags from 0x0--0x8000 are RESERVED for Kernel-mode
	 */
	_MALI_OSU_LOCKFLAG_ANYUNLOCK = 0x10000, /**< Mutex that guarantees that any thread can unlock it when locked. Otherwise, this will not be possible. */
	/** @enum _mali_osu_lock_flags_t
	 *
	 * Flags from 0x10000 are RESERVED for User-mode
	 */
	_MALI_OSU_LOCKFLAG_STATIC = 0x20000, /**< Statically initialized lock. This flag is only used interally in base; users use _mali_osu_lock_static to access static locks. */
	_MALI_OSU_LOCKFLAG_SPINLOCK = 0x40000, /**< Use a spin lock instead of a mutex */
 } _mali_osu_lock_flags_t;

typedef enum
{
	_MALI_OSU_LOCKMODE_UNDEF = -1,  /**< Undefined lock mode. For internal use only */
	_MALI_OSU_LOCKMODE_RW    = 0x0, /**< Default. Lock is used to protect data that is read from and written to */
	/** @enum _mali_osu_lock_mode_t
	 *
	 * Lock modes 0x1--0x3F are RESERVED for Kernel-mode */
} _mali_osu_lock_mode_t;

/** @brief Private type for Mutual Exclusion lock objects. */
typedef struct _mali_osu_lock_t_struct _mali_osu_lock_t;

/** @brief The number of static locks supported in _mali_osu_lock_static(). */
#define MALI_OSU_STATIC_LOCK_COUNT (sizeof(_mali_osu_static_locks) / sizeof(_mali_osu_lock_t))

/** @} */ /* end group _mali_osu_lock */

/** @defgroup _mali_osu_thread OSU Thread functions
 * @{ */

/**
 * @brief Thread Local Storage (TLS) keys.
 *
 * Thread Local Storage (TLS) are used for threads to store thread local context data.
 *
 * Each thread can have their own data stored without the risk of other threads corrupting it
 * or needing any access synchronization.
 *
 * TLS is divided into multiple slots, each accessed by a special key.
 */
typedef enum
{
	_MALI_OSU_THREAD_KEY_EGL_CONTEXT = 0,              /**< Current EGL context for thread */
	_MALI_OSU_THREAD_KEY_INSTRUMENTED_ACTIVE_FRAME,    /**< Current active instrumented frame (used in callback thread(s)) */
	_MALI_OSU_THREAD_KEY_INSTRUMENTED_DATA,            /**< Current instrumented data context (instrumented driver only) */
 	_MALI_OSU_THREAD_KEY_DUMP_CALLBACK,                /**< Current Dumping context for thread */
	_MALI_OSU_THREAD_KEY_GLES_CONTEXT,                 /**< Current GLES context for thread*/
	_MALI_OSU_THREAD_KEY_VG_CONTEXT,                   /**< Current VG context for thread */
	_MALI_OSU_THREAD_KEY_MALI_EGL_IMAGE,               /**< Current EGL image context for thread */

	_MALI_OSU_THREAD_KEY_MAX /**< The current number of thread keys in use */
} _mali_osu_thread_key_t;

/** @brief Private type for thread object. */
typedef struct _mali_osu_thread_t_struct _mali_osu_thread_t;

/** @brief Entry point for a thread.
 *
 * @param arg pointer to an object passed on to thread entry point
 */
typedef void *(_mali_osu_threadproc_t)(void *arg);
/** @} */ /* end group _mali_osu_thread */

/** @defgroup _mali_osu_debug_test_instrumented OSU functions to support debug, test and instrumented build
 * @{ */

/** @brief Private type for file descriptor object. */
typedef struct _mali_file_t_struct _mali_file_t;

/** @brief File descriptor for the Standard Output (stdout) stream. */
extern _mali_file_t *MALI_STDOUT;

/** @brief File descriptor for the Standard Error (stderr) stream. */
extern _mali_file_t *MALI_STDERR;

/** @brief Defines read, write or append access mode for _mali_osu_fopen(). */
typedef enum
{
    _MALI_FILE_MODE_R = 0,  /**< read only */
    _MALI_FILE_MODE_W = 1,  /**< write only */
    _MALI_FILE_MODE_A = 2,  /**< write only, append */
    _MALI_FILE_MODE_RB = 3, /**< read only, binary */
    _MALI_FILE_MODE_WB = 4, /**< write only, binary */
    _MALI_FILE_MODE_AB = 5, /**< write only, append, binary */
} _mali_file_mode_t;


/** @brief Private type for dynamic library object. */
typedef struct _mali_library_t_struct _mali_library_t;

/** @} */ /* end group  _mali_osu_debug_test_instrumented */

/** @addtogroup _mali_osu_atomic
 * @{ */

/** @brief Decrement an atomic counter and return its decremented value.
 *
 * @note It is an error to decrement the counter beyond -(1<<23)
 *
 * @param atom pointer to an atomic counter
 * @return decremented value of atomic counter
 */
u32 _mali_osu_atomic_dec_and_return( _mali_osu_atomic_t *atom );

/** @brief Increment an atomic counter and return its incremented value.
 *
 * @note It is an error to increment the counter beyond (1<<23)-1
 *
 * @param atom pointer to an atomic counter
 * @return incremented value of atomic counter
 */
u32 _mali_osu_atomic_inc_and_return( _mali_osu_atomic_t *atom );

/** @brief Initialize an atomic counter.
 *
 * The counters have storage for signed 24-bit integers. Initializing to signed
 * values requiring more than 24-bits storage will fail.
 *
 * @note the parameter required is a u32, and so signed integers should be
 * cast to u32.
 *
 * @param atom pointer to an atomic counter
 * @param val the value to initialize the atomic counter.
 * @return _MALI_OSU_ERR_OK on success, otherwise, a suitable
 * _mali_osu_errcode_t on failure.
 */
_mali_osu_errcode_t _mali_osu_atomic_init( _mali_osu_atomic_t *atom, u32 val );

/** @brief Read a value from an atomic counter.
 *
 * Although the value returned is a u32, only numbers with signed 24-bit
 * precision (sign extended to u32) are returned.
 *
 * This can only be safely used to determine the value of the counter when it
 * is guaranteed that other threads will not be modifying the counter. This
 * makes its usefullness limited.
 *
 * @param atom pointer to an atomic counter
 * @return value read from the atomic counter
 */
u32 _mali_osu_atomic_read( _mali_osu_atomic_t *atom );

/** @brief Write a value to an atomic counter.
 *
 * Although the value is a u32, only numbers with signed 24-bit
 * precision (sign extended to u32) are allowed. Providing a signed
 * value requiring more than 24-bits storage will fail.
 *
 * @note the parameter required is a u32, and so signed integers should be
 * cast to u32.
 *
 * @param atom pointer to an atomic counter
 * @param val the value to write to the atomic counter.
 */
void _mali_osu_atomic_write( _mali_osu_atomic_t *atom, u32 val );

/** @} */ /* end group _mali_osu_atomic */

/** @defgroup _mali_osu_memory OSU Memory Allocation
 * @{ */

/** @brief Allocate zero-initialized memory.
 *
 * Returns a buffer capable of containing at least \a n elements of \a size
 * bytes each. The buffer is initialized to zero.
 *
 * The buffer is suitably aligned for storage and subsequent access of every
 * type that the compiler supports. Therefore, the pointer to the start of the
 * buffer may be cast into any pointer type, and be subsequently accessed from
 * such a pointer, without loss of information.
 *
 * When the buffer is no longer in use, it must be freed with _mali_osu_free().
 * Failure to do so will cause a memory leak.
 *
 * @note Most toolchains supply memory allocation functions that meet the
 * compiler's alignment requirements.
 *
 * @param n Number of elements to allocate
 * @param size Size of each element
 * @return On success, the zero-initialized buffer allocated. NULL on failure
 */
void *_mali_osu_calloc( u32 n, u32 size );

/** @brief Allocate memory.
 *
 * Returns a buffer capable of containing at least \a size bytes. The
 * contents of the buffer are undefined.
 *
 * The buffer is suitably aligned for storage and subsequent access of every
 * type that the compiler supports. Therefore, the pointer to the start of the
 * buffer may be cast into any pointer type, and be subsequently accessed from
 * such a pointer, without loss of information.
 *
 * When the buffer is no longer in use, it must be freed with _mali_osu_free().
 * Failure to do so will cause a memory leak.
 *
 * @note Most toolchains supply memory allocation functions that meet the
 * compiler's alignment requirements.
 *
 * Remember to free memory using _mali_osu_free().
 * @param size Number of bytes to allocate
 * @return On success, the buffer allocated. NULL on failure.
 */
void *_mali_osu_malloc( u32 size );

/** @brief Reallocate memory.
 *
 * Resizes a previously allocated buffer. The contents of the buffer remain
 * unchanged up to the lesser of the old and new sizes. If the new size is
 * larger, the contents of the newly allocated portion is undefined. If a
 * larger size cannot be allocated, the original allocation remains unchanged.
 *
 * If the new size required the original allocation of the buffer to be moved,
 * then that original allocation will be freed. The new buffer returned is
 * suitably aligned for storage and subsequent access of every type that the
 * compiler supports. Therefore, the pointer to the start of the buffer may be
 * cast into any pointer type, and be subsequently accessed from such a
 * pointer, without loss of information.
 *
 * @note Most toolchains supply memory allocation functions that meet the
 * compiler's alignment requirements.
 *
 * When the buffer is no longer in use, it must be freed with _mali_osu_free().
 * Failure to do so will cause a memory leak.
 *
 * If NULL is used for the \a ptr parameter, then _mali_osu_realloc() behaves
 * identically to _mali_osu_malloc().
 *
 * If the \a size parameter is 0 and the \a ptr parameter is non-NULL, then
 * the buffer will be freed, as though it was passed to _mali_osu_free(). NULL
 * will be returned in this instance.
 *
 * @param ptr A pointer to the start of a buffer that was previously allocated
 * with _mali_osu_malloc(), _mali_osu_calloc(), or _mali_osu_realloc().
 * @param size the new size for the previously allocated memory
 * @return If the new buffer has non-zero \a size, then on sucess, a pointer to
 * the buffer, and NULL on failure. If the buffer has zero \a size, then NULL.
 */
void *_mali_osu_realloc( void *ptr, u32 size );

/** @brief Free memory.
 *
 * Reclaims the buffer pointed to by the parameter \a ptr for the system.
 * All memory returned from _mali_osu_malloc(), _mali_osu_calloc() and
 * _mali_osu_realloc() must be freed before the application exits. Otherwise,
 * a memory leak will occur.
 *
 * Memory must be freed once. It is an error to free the same non-NULL pointer
 * more than once.
 *
 * It is legal to free the NULL pointer.
 *
 * @param ptr Pointer to buffer to free
 */
void _mali_osu_free( void *ptr );

/** @brief Copies memory.
 *
 * Copies the \a len bytes from the buffer pointed by the parameter \a src
 * directly to the buffer pointed by \a dst.
 *
 * It is an error for \a src to overlap \a dst anywhere in \a len bytes.
 *
 * @param dst Pointer to the destination array where the content is to be
 * copied.
 * @param src Pointer to the source of data to be copied.
 * @param len Number of bytes to copy.
 * @return \a dst is always passed through unmodified.
 */
void *_mali_osu_memcpy( void *dst, const void *src, u32	len );

/** @brief Fills memory.
 *
 * Sets the first \a size bytes of the block of memory pointed to by \a ptr to
 * the specified value
 * @param ptr Pointer to the block of memory to fill.
 * @param chr Value to be set, passed as u32. Only the 8 Least Significant Bits (LSB)
 * are used.
 * @param size Number of bytes to be set to the value.
 * @return \a ptr is always passed through unmodified
 */
void *_mali_osu_memset( void *ptr, u32 chr, u32 size );

/** @brief Compares bytes in memory.
 *
 * @param ptr1 Pointer to memory
 * @param ptr2 Pointer to memory
 * @param size Number of bytes to be compared
 *
 * @return Returns 0 when \a size bytes in the two memory areas match. If not, the
 * sign of the comparison between the first different bytes determines the
 * result:
 * - < 0 when the different byte in \a ptr1 is smaller than the one in \a ptr2
 * - > 0 when the different byte in \a ptr1 is larger than the one in \a ptr2
 */
int _mali_osu_memcmp( const void *ptr1, const void *ptr2, u32 size );
/** @} */ /* end group _mali_osu_memory */


/** @addtogroup _mali_osu_lock
 * @{ */

/** @brief Initialize a Mutual Exclusion Lock.
 *
 * Locks are created in the signalled (unlocked) state.
 *
 * The parameter \a initial must be zero.
 *
 * At present, the parameter \a order must be zero. It remains for future
 * expansion for mutex order checking.
 *
 * @param flags flags combined with bitwise OR ('|'), or zero. There are
 * restrictions on which flags can be combined, see \ref _mali_osu_lock_flags_t.
 * @param initial For future expansion into semaphores. SBZ.
 * @param order The locking order of the mutex. SBZ.
 * @return On success, a pointer to a \ref _mali_osu_lock_t object. NULL on failure.
 */
_mali_osu_lock_t *_mali_osu_lock_init( _mali_osu_lock_flags_t flags, u32 initial, u32 order );

/** @brief Obtain a statically initialized Mutual Exclusion Lock.
 *
 * Retrieves a reference to a statically initialized lock. Up to
 * _MALI_OSU_STATIC_LOCK_COUNT statically initialized locks are
 * available. Only _mali_osu_lock_wait(), _mali_osu_lock_trywait(),
 * _mali_osu_lock_signal() can be used with statically initialized locks.
 * _MALI_OSU_LOCKMODE_RW mode should be used when waiting and signalling
 * statically initialized locks.
 *
 * For the same \a nr a pointer to the same statically initialized lock is
 * returned. That is, given the following code:
 * @code
 *	extern u32 n;
 *
 * 	_mali_osu_lock_t *locka = _mali_osu_lock_static(n);
 *	_mali_osu_lock_t *lockb = _mali_osu_lock_static(n);
 * @endcode
 * Then (locka == lockb), for all 0 <= n < MALI_OSU_STATIC_LOCK_COUNT.
 *
 * @param nr index of a statically initialized lock [0..MALI_OSU_STATIC_LOCK_COUNT-1]
 * @return On success, a pointer to a _mali_osu_lock_t object. NULL on failure.
 */
_mali_osu_lock_t *_mali_osu_lock_static( u32 nr );

/** @brief Initialize a Mutual Exclusion Lock safely across multiple threads.
 *
 * The _mali_osu_lock_auto_init() function guarantees that the given lock will
 * be initialized once and precisely once, even in a situation involving
 * multiple threads.
 *
 * This is necessary because the first call to certain Public API functions must
 * initialize the API. However, there can be a race involved to call the first
 * library function in multi-threaded applications. To resolve this race, a
 * mutex can be used. This mutex must be initialized, but initialized only once
 * by any thread that might compete for its initialization. This function
 * guarantees the initialization to happen correctly, even when there is an
 * initialization race between multiple threads.
 *
 * Otherwise, the operation is identical to the _mali_osu_lock_init() function.
 * For more details, refer to _mali_osu_lock_init().
 *
 * @param pplock pointer to storage for a _mali_osu_lock_t pointer. This
 * _mali_osu_lock_t pointer may point to a _mali_osu_lock_t that has been
 * initialized already
 * @param flags flags combined with bitwise OR ('|'), or zero. There are
 * restrictions on which flags can be combined. Refer to
 * \ref _mali_osu_lock_flags_t for more information.
 * The absence of any flags (the value 0) results in a sleeping-mutex,
 * which is non-interruptible.
 * @param initial For future expansion into semaphores. SBZ.
 * @param order The locking order of the mutex. SBZ.
 * @return On success, _MALI_OSU_ERR_OK is returned and a pointer to an
 * initialized \ref _mali_osu_lock_t object is written into \a *pplock.
 * _MALI_OSU_ERR_FAULT is returned on failure.
 */
_mali_osu_errcode_t _mali_osu_lock_auto_init( _mali_osu_lock_t **pplock, _mali_osu_lock_flags_t flags, u32 initial, u32 order );

/** @brief Wait for a lock to be signalled (obtained).
 *
 * After a thread has successfully waited on the lock, the lock is obtained by
 * the thread, and is marked as unsignalled. The thread releases the lock by
 * signalling it.
 *
 * To prevent deadlock, locks must always be obtained in the same order.
 *
 * @param lock the lock to wait upon (obtain).
 * @param mode the mode in which the lock should be obtained. Currently this
 * must be _MALI_OSU_LOCKMODE_RW.
 * @return On success, _MALI_OSU_ERR_OK, _MALI_OSU_ERR_FAULT on error.
 */
_mali_osu_errcode_t _mali_osu_lock_wait( _mali_osu_lock_t *lock, _mali_osu_lock_mode_t mode);

/** @brief Wait for a lock to be signalled (obtained) with timeout
 *
 * After a thread has successfully waited on the lock, the lock is obtained by
 * the thread, and is marked as unsignalled. The thread releases the lock by
 * signalling it.
 *
 * To prevent deadlock, locks must always be obtained in the same order.
 *
 * This version can return early if it cannot obtain the lock within the given timeout.
 *
 * @param lock the lock to wait upon (obtain).
 * @param mode the mode in which the lock should be obtained. Currently this
 * must be _MALI_OSU_LOCKMODE_RW.
 * @param timeout Relative time in microseconds for the timeout
 * @return _MALI_OSU_ERR_OK if the lock was obtained, _MALI_OSU_ERR_TIMEOUT if the timeout expired or  _MALI_OSU_ERR_FAULT on error.
 */
_mali_osu_errcode_t _mali_osu_lock_timed_wait( _mali_osu_lock_t *lock, _mali_osu_lock_mode_t mode, u64 timeout);

/** @brief Test for a lock to be signalled and obtains the lock when so.
 *
 * Obtains the lock only when it is in signalled state. The lock is then
 * marked as unsignalled. The lock is released again by signalling
 * it by _mali_osu_lock_signal().
 *
 * If the lock could not be obtained immediately (that is, another thread
 * currently holds the lock), then this function \b does \b not wait for the
 * lock to be in a signalled state. Instead, an error code is immediately
 * returned to indicate that the thread could not obtain the lock.
 *
 * To prevent deadlock, locks must always be obtained in the same order.
 *
 * @param lock the lock to wait upon (obtain).
 * @param mode the mode in which the lock should be obtained. Currently this
 * must be _MALI_OSU_LOCKMODE_RW.
 * @return When the lock was obtained, _MALI_OSU_ERR_OK. If the lock could not
 * be obtained, _MALI_OSU_ERR_FAULT.
 */
_mali_osu_errcode_t _mali_osu_lock_trywait( _mali_osu_lock_t *lock, _mali_osu_lock_mode_t mode);

/** @brief Signal (release) a lock.
 *
 * Locks may only be signalled by the thread that originally waited upon the
 * lock, unless the lock was created using the _MALI_OSU_LOCKFLAG_ANYUNLOCK flag.
 *
 * @param lock the lock to signal (release).
 * @param mode the mode in which the lock should be obtained. This must match
 * the mode in which the lock was waited upon.
 */
void _mali_osu_lock_signal( _mali_osu_lock_t *lock, _mali_osu_lock_mode_t mode );

/** @brief Terminate a lock.
 *
 * This terminates a lock and frees all associated resources.
 *
 * It is a programming error to terminate the lock when it is held (unsignalled)
 * by a thread.
 *
 * @param lock the lock to terminate.
 */
void _mali_osu_lock_term( _mali_osu_lock_t *lock );
/** @} */ /* end group _mali_osu_lock */

/** @defgroup _mali_osu_math OSU Math functions
 * @{ */

/** @brief Square root.
 *
 *	@param value	Value to take the square root of
 *	@return			The square root
 */
float _mali_osu_sqrt(float value);

/** @brief Sine.
 *
 *	@param value	Value to take the sine of
 *	@return			The sine
 */
float _mali_osu_sin(float value);

/** @brief Cosine.
 *
 *	@param value	Value to take the cosine of
 *	@return			The cosine
 */
float _mali_osu_cos(float value);


/** @brief Arc tangent.
 *
 *	@param y y-coordinate
 *	@param x x-coordinate
 *	@return  Principal arc tangent of y/x in the range [-pi,pi] (radians)
 */
float _mali_osu_atan2(float y, float x);


/** @brief Ceil - Round up.
 *
 *	@param value	Value to take the ceil of
 *	@return			The ceiled value
 */
float _mali_osu_ceil(float value);

/** @brief Floor - Round down.
 *
 *	@param value	Value to round down
 *	@return			The floored value
 */
float _mali_osu_floor(float value);

/** @brief Absolute value of floating point number.
 *
 *	@param value	Value to take the absolute value of
 *	@return			The absolute value
 */
float _mali_osu_fabs(float value);

/** @brief Absolute value of 32-bit signed integer number.
 *
 *  @param value Value to take the absolute value of
 *  @return The absolute value
 */
s32 _mali_osu_abs(s32 value);

/** @brief Absolute value of 64-bit signed integer number.
 *
 *  @param value Value to take the absolute value of
 *  @return The absolute value
 */
s64 _mali_osu_abs64(s64 value);

/** @brief Compute the exponential function.
 *
 * The exponention function on parameter \a value is defined to be the natural
 * number e (approx 2.71) raised to the power of \a value. That is, e^value.
 *
 *	@param value	Value to take the exponent of
 *	@return			The result of the exponential function on \a value, calculated as e^value.
 */
float _mali_osu_exp(float value);

/** @brief Power function.
 *
 *	@param base		Base value to raise to a power
 *	@param exponent	Exponent to which to raise the base
 *	@return			The value of base to the power of exponent
 */
float _mali_osu_pow(float base, float exponent);

/** @brief Natural Logarithm function.
 *
 *	@param value	Value to find logarithm of
 *	@return			Log to base e of value
 */
float _mali_osu_log(float value);

/**
 *  Judge whether the input is a NaN
 *  @param value    Value to be judged
 *  @return         If value has NaN, return non-zero value 
 */
int _mali_osu_isnan(float value);

/** @brief Calculate the modulus for a float
 *
 *  @param n The number to be divided.
 *  @param m The modulus.
 *  @return The value of n - x*m for some x.
 */
float _mali_osu_fmodf(float n, float m);

/** @} */ /* end group _mali_osu_math */


/** @defgroup _mali_osu_string OSU string manipulation functions
 * @{ */

/** @brief Format output of a stdarg argument list.
 *
 * This function formats the parameters passed in \a args, according to the
 * parameter \a format. The string is written to the buffer pointed to by the
 * parameter \a str.
 *
 * This function is fail-safe. That is, no more than \a size bytes will be
 * written, and if size if greater than zero, then the last byte written will
 * be a zero-byte (termination character).
 *
 * This implies that the maximum length of the written string (as determined by
 * _mali_osu_strlen()) is \a size-1.
 *
 * This function may be used to determine the size of the formatted string,
 * before a buffer is allocated to contain it. In this case, set \a size to
 * zero. In this case, and this case only, \a str may also be NULL.
 *
 * The format is guaranteed to recognise the following conversion specifiers:
 * - \%d : a signed integer parameter, formatted to a decimal number
 * - \%X : an unsigned integer parameter, formatted to a hexadecimal number.
 * - \%f : a double-precision floating-point parameter
 * - \%c : a character parameter, formatted to a single ASCII character.
 * - \%s : a char* paramater (pointer to a buffer of characters), formatted to
 * a string.
 * - \%\% : produce a single '\%' character.
 *
 * Other conversion specifiers are \em implementation \em defined.
 *
 * @param str The buffer to write the formatted string into.
 * @param size Maximum number of bytes to write into the buffer, including the
 * zero-termination character.
 * @param format The input text string that formats the output string.
 * @param args Input parameters for the input text string.
 * @return The length of the formatted string (excluding the null termination
 * character), regardless of how many bytes were actually written. Add one to
 * the return value to determine the size of the buffer required to store the
 * string. An error is indicated by a negative value.
 */
int _mali_osu_vsnprintf(char* str, u32 size, const char * format, va_list args);

/** @brief Convert a string to an unsigned 32-bit integer.
 *
 * The conversion is as described in _mali_osu_strtol(), except for when the
 * conversion sequence is signed. In this case, the conversion happens as though
 * no sign was used, and then the unsigned value is negated before being
 * returned.
 *
 * @note Negation of unsigned values is defined by ANSI-C89 compliant compilers
 * to return the highest integer that is congruent to the value mod 2^n, where
 * n is the size of the unsigned type in bits.
 *
 * @param nptr Character string to convert
 * @param endptr Pointer to the character that ended the scan or a null value
 * @param base Specifies the base to use for the conversion.
 * @return unsigned 32 bit integer value as result of conversion. On error, zero
 * will be returned. If the value represented by the string would overflow a
 *  u32, then it is saturated to the minimum and maximum range of u32.
 */
u32 _mali_osu_strtoul(const char *nptr, char **endptr, u32 base);

/** @brief Convert a string to a signed 32-bit integer.
 *
 * Converts the initial part of a string to a signed 32 bit integer value
 * according to the given base.
 *
 * The number takes the form of an optional sign character ('+' or '-'),
 * followed by a sequence of characters. The parameter \a base determines the
 * meaning of this sequence of characters:
 * - \a base == 0 indicates that \a base should be determined by the first
 * character(s) in the sequence after the sign:
 *  - a character in the range '1'..'9' indicates a decimal number.
 *  - a '0' followed by an 'x' or 'X' character indicates a hexadecimal number.
 *  - a '0' followed by any other digit character indicates an octal number.
 *  - The remaining characters are interpreted as though \a base had been set
 * appropriately:
 * - 2 to 36 indicates that the characters 'A'..'Z' (or 'a'..'z') in the string
 * be translate to the numbers 10..35 respectively.
 *
 * If a character in the sequence is found that does not translate to a number
 * that is strictly less than the \a base, then the scan conversion ends.
 *
 * @param nptr Character string to convert
 * @param endptr Pointer to the character that ended the scan or a null value
 * @param base Specifies the base to use for the conversion.
 * @return signed 32 bit integer value as result of conversion. On error, zero
 * will be returned. If the value represented by the string would overflow an
 * s32, then it is saturated to the minimum and maximum range of s32.
 */
s32 _mali_osu_strtol(const char *nptr, char **endptr, u32 base);

/** @brief Convert a string to a double precision number.
 *
 * Converts the initial part of a string to a double-precision floating point
 * number.
 *
 * The format of the character sequence in the string is similar to that of
 * floating-point constants in the ANSI C89 standard. However, the sequence
 * need not contain a period or an exponent specifier. In this case, the
 * sequence is treated as though it had an ''e0'' exponent specifier.
 *
 * The format is a sequence of:
 * - an optional sign
 * - zero or more digit characters
 * - an optional period
 * - zero or more digit characters
 * - an optional exponent character 'e' or 'E', followed by an (optionally
 * signed) integral number.
 *
 * The conversion ends when a character not matching this format is reached.
 *
 * @param nptr Character string to convert
 * @param endptr Pointer to the character that ended the scan or a null value
 * @return double precision number as result of conversion. On error, zero is
 * returned. If the value cannot be represented as a double, then +/-inf is
 * returned (where the sign matches that of the character sequence).
 */
double _mali_osu_strtod(const char *nptr, char **endptr);

/** @brief Get string length.
 *
 * @param str C string
 * @return the length of \a str, excluding the null-termination character.
 */
u32 _mali_osu_strlen(const char *str);

/** @brief Duplicate a string.
 *
 * Creates a copy of the string pointed to by the parameter \a str. The copy
 * will contain the same bytes as the original up to and including
 * the first zero terminator. A zero-length string will result
 * in another zero-length string (non-NULL on success).
 *
 * New memory is allocated for the string. Therefore, such memory must
 * be passed to _mali_osu_free() when it is no longer in use. To do otherwise
 * will cause a memory leak.
 *
 * @param str The string to duplicate
 * @return The duplicated string, or NULL if the string duplication failed
 */
char * _mali_osu_strdup(const char * str);

/** @brief Compare two strings.
 *
 * Performs a lexical comparison of \a str1 and \a str2
 * @param str1 C string
 * @param str2 C string
 * @return If str1==str2, returns 0. If str1<str2, returns value<0, if str1>str2, returns >0.
 */
u32 _mali_osu_strcmp(const char *str1, const char *str2);

/** @brief Compare count characters of two strings.
 *
 * Performs a lexical comparison of first \a count characters of \a str1 and \a str2
 * @param str1 C string
 * @param str2 C string
 * @param count integer
 * @return If str1(0:n-1)==str2(0:n-1), returns 0. If str1(0:n-1)<str2(0:n-1), returns value<0, if str1(0:n-1)>str2(0:n-1), returns >0.
 */
u32 _mali_osu_strncmp(const char *str1, const char *str2, u32 count);

/** @brief Copy and pad part of a string.
 *
 * Copy a string into a buffer, limiting and padding to n bytes.
 *
 * When the length of the string is less than the parameter \a n, then the
 * buffer pointed to by \a dest is padded with NULL characters until \a n bytes
 * have been written in total by the function.
 *
 * @note Care must be taken: if \a n bytes are copied before a NULL termination
 * character is found in \a src, then a NULL termination character is \b not
 * written to \a dest.
 *
 * It is an error for \a src and \a dest to overlap over the \a n bytes.
 *
 * @param dest Where to copy the string to
 * @param src String to copy
 * @param n Maximum number of bytes to copy
 * @return The destination string. No error conditions are indicated.
 */
char *_mali_osu_strncpy(char *dest, const char *src, u32 n);

/** @brief Concatenate a string with part of another
 *
 * Copies the string pointed to by the parameter \a src to the end of the
 * string pointed to by the parameter \a dest. The NULL termination character
 * in \a dest will be overwritten.
 *
 * At most, \a n bytes of \a src are copied before the NULL termination
 * character in \a src is reached.
 *
 * A NULL termination character is always then added to the end of the string.
 *
 * @note \a n only limits the increase in string length of \a dest. The number of
 * bytes that are copied is no more than \a n + 1, due to the NULL termination
 * character.
 *
 * @param dest The string to append to
 * @param src The string to append
 * @param n The maximum characters to add to \a dest from \a src before the NULL
 * termination charcater in \a src. The number of bytes written to the buffer \a
 * dest may be \a n + 1, if a NULL termination character was not found in the
 * first \a n characters of \a src.
 * @return The destination string.
 */
char *_mali_osu_strncat(char *dest, const char *src, u32 n);

/** @brief Converts a string to an integer
 *
 * The string pointed to by the argument \a src is converted to an integer.
 * Any initial whitespace characters are skipped (space, tab, carriage return, new line, vertical tab, or formfeed).
 * The number may consist of an optional sign and a string of digits.
 * Conversion stops when the first unrecognized character is reached.
 *
 * On success the converted number is returned.
 * If the number cannot be converted, then 0 is returned.
 *
 * @param src The string to convert
 * @return The value of the integer passed as a string.
 */
u32 _mali_osu_atoi(const char *str);
/** @} */ /* end group _mali_osu_string */

/** @addtogroup _mali_osu_thread
 * @{ */

/** @brief Set data stored in the TLS slot referenced by \a key.
 *
 * @param key The TLS key
 * @param value The value to store in the slot
 * @return _MALI_OSU_ERR_OK on success, otherwise, a suitable
 * _mali_osu_errcode_t on failure.
 */
_mali_osu_errcode_t _mali_osu_thread_key_set_data( _mali_osu_thread_key_t key, void* value ) MALI_CHECK_RESULT;

/** @brief Get data stored in the TLS slot referenced by \a key.
 *
 * @param key The TLS key
 * @return The value stored in the slot. If the slot has never been set, then
 * NULL will be returned.
 */
void* _mali_osu_thread_key_get_data( _mali_osu_thread_key_t key );

/** @brief Get current thread identifier.
 *
 * @return 32-bit thread identifier, which is unique for all threads in the
 * current process.
 */
u32 _mali_osu_thread_get_current(void);

/** @brief Create a thread.
 *
 * A new thread is created executing the function threadproc with start_param passed
 * in as sole argument. The thread finishes when the function returns.
 *
 * @param thread pointer to storage for a pointer to the created thread object
 * @param threadproc entry point of the thread
 * @param start_param pointer to object passed as parameter to threadproc
 * @return _MALI_OSU_ERR_OK on success, otherwise, a suitable
 * _mali_osu_errcode_t on failure.
 */
_mali_osu_errcode_t _mali_osu_create_thread(_mali_osu_thread_t **thread, _mali_osu_threadproc_t *threadproc, void *start_param);

/** @brief Wait for \a thread to finish.
 *
 * @param thread pointer to a thread object
 * @param exitcode pointer to storage for the 32-bit exit code of the finished thread.
 * The exit code is only valid upon successful return. This pointer may be NULL if there
 * is no interest in the exit code.
 * @return _MALI_OSU_ERR_OK on success, otherwise, a suitable
 * _mali_osu_errcode_t on failure. exitcode
 */
_mali_osu_errcode_t _mali_osu_wait_for_thread(_mali_osu_thread_t *thread, u32 *exitcode);

/** @} */ /* end group _mali_osu_thread */


/** @defgroup _mali_osu_misc OSU miscellaneous functions
 * @{ */

/** @brief Suspend execution for the specified amount of time.
 *
 * Takes the number of centi-seconds (1/100s) to sleep.
 * The function will sleep AT LEAST csec centi-seconds.
 * The sleep duration might be longer due to scheduling on the system.
 * @param csec Number of centi-seconds to sleep
 */
void _mali_osu_cssleep(u32 csec);

/** @brief Yield so that another thread can run.
 *
 * Use of this is provided purely for compatiblity and testing. Its yielding
 * cannot be guaranteed.
 *
 * @note The use of yielding is deprecated. The use of yield for busy-wait
 * loops consumes power unnecessarily, and delays other threads of the same or
 * lower priority from running. Instead, use a blocking synchronization
 * primitive, as defined in \ref _mali_osu_lock.
 */
void _mali_osu_yield(void);

/** @brief Sort elements of an array.
 *
 * Sort the \a num elements of the array given by \a base, each element being
 * \a size bytes long.
 *
 * Comparison between two elements has to be performed by the given \a compare
 * function pointer. \a compare must return a negative, zero or positive value
 * to indicate if its first argument is considered to be less, equal or greater
 * than its second argument.
 * @param base The array to sort
 * @param num Number of elements in the array
 * @param size Size of each element
 * @param compare Function pointer to compare function
 */
void _mali_osu_qsort(void * base, u32 num, u32 size, int (*compare)(const void *, const void *));

/**
 * Load (extra) configuration variables.
 * Some platforms do not have a built-in method for specifying config variables
 * for a specific application. This function will make sure the application specific
 * configuration variables are loaded. All variables loaded can afterwards be
 * accessed through _mali_sys_config_string_get() and related functions.
 *      @return                 void
 */
void _mali_osu_load_config_strings(void);

/** @brief Get text string from the OS.
 *
 * The intended use is to be able to read environment variables set in the OS.
 * This will make it easier to configure the programs and drivers without doing it
 * through configuration files or recompilation.
 * @note The string consumes a small amount of system resource. The caller must
 * ensure that when it no longer needs the string, it is freed with a call to
 * _mali_osu_config_string_release(). Otherwise, a memory leak will occur.
 * @param name The name of the variable we want to read
 * @return The string with text if the name existed, or NULL.
 */
const char * _mali_osu_config_string_get(const char * name);

/** @brief Set (or unset) text string in the OS environment.
 *
 * The intended use is to be able to set environment variables in the OS.
 * This will make it easier to configure the programs and drivers without doing it
 * through configuration files or recompilation.
 * @param name The name of the variable we want to set
 * @param value The string with text or NULL to unset the variable
 * @return _MALI_OSK_ERR_OK on success, _MALI_OSK_ERR_FAULT otherwise
 */
_mali_osk_errcode_t _mali_osu_config_string_set(const char * name, const char *value);

/** @brief Release text string from the OS.
 *
 * Free allocated text string from the OS.
 * The function frees a text string allocated by \a _mali_osu_config_string_get()
 *
 * It is legal to input a NULL-pointer.
 * @param env_var The env_var returned from \a _mali_osu_config_string_get()
 */
void _mali_osu_config_string_release(const char * env_var);

/** @brief Get a timer with at most, microseconds precision.
 *
 * Intended usage is instrumented functions.
 *
 * The function is a wrapper to get the time from the system.
 * It either has the precision of microseconds, or the precision that is
 * supported by the underlying system - whichever is lower.
 *
 * If the system does not have a timer the function returns Zero. The function
 * only returns Zero when it fails.
 *
 * The returned value will be microseconds since some platform-dependant date,
 * and so cannot be used as a platform independent way of determining the current
 * time/date. Only the difference in time between two events may be measured.
 *
 * @return A 64bit counter with microsecond precision, or zero.
 */
u64 _mali_osu_get_time_usec(void);

/** @} */ /* end group _mali_osu_misc */

/** @addtogroup _mali_osu_debug_test_instrumented
 * @{
 */

/** @brief Abnormal process abort.
 *
 * Program will exit if this function is called. Function is called from
 * assert-macro in mali_debug.h.
 */
void _mali_osu_abort(void);

/** @brief Sets breakpoint at point where function is called. */
void _mali_osu_break(void);

/** @brief Open a stream.
 *
 * Opens a stream on a file for use by _mali_osu_fread() and _mali_osu_fwrite().
 *
 * The path to a file has a system-specific interpretation.
 *
 * @param path The path to and name of the file to be opened, using the '/'
 * character as the directory separator.
 * @param mode enumeration specifying the type of access to be done.
 * Refer to \ref _mali_file_mode_t for more information
 * @return a file descriptor, in the form of a \ref _mali_file_t, or NULL for
 * failure.
 */
_mali_file_t * _mali_osu_fopen(const char * path, _mali_file_mode_t mode);

/** @brief Close a stream.
 *
 * Closes a stream on a file, and guarantees that all writes to the stream will
 * be complete by the time the function completes.
 *
 * \ref _mali_file_t descriptors must be closed after use. Otherwise, writes
 * to the file may be lost when the application exits.
 *
 * A \ref _mali_file_t descriptor must not be used after it is closed.
 *
 * @param file The \ref _mali_file_t descriptor to close.
 */
void _mali_osu_fclose(_mali_file_t * file);

/** @brief Remove a file.
 *
 * Removes from the system a file specified by the parameter \a path.
 *
 * @param filename the path to and name of the file to be removed, which is
 * interpreted in the same way as paths in _mali_osu_fopen().
 * @return Upon successful completion, 0 shall be returned. Otherwise, -1 shall
 * be returned.
 */
int _mali_osu_remove(const char *filename);

/** @brief Write data to a stream.
 *
 * Writes a buffer to a stream. The stream may either be:
 * - A stream opened by a call to _mali_osu_fopen(), as represented by an
 * object of the \ref _mali_file_t file descriptor type.
 * - The \ref MALI_STDOUT and \ref MALI_STDERR streams.
 *
 * @note If the stream was opened in a mode other than the binary modes from
 * \ref _mali_file_mode_t, then it is \em implementation \em defined how line
 * endings are treated, and how many bytes they will occupy.
 *
 * @note Code that wishes to do a formatted print to the Standard Output or
 * Standard Error streams may do so by a combination of _mali_osu_vsnprintf(),
 * and _mali_osu_fwrite().
 *
 * @param ptr The buffer to write to a file.
 * @param element_size The number of bytes for each element
 * @param num_elements The number of elements to write to the file
 * @param file A file descriptor corresponding to the stream to write to.
 * @return The number of elements successfully written.
*/
int _mali_osu_fwrite(const void *ptr, u32 element_size, u32 num_elements, _mali_file_t * file);

/** @brief Read data from a stream.
 *
 * Reads data from a stream into a buffer. The stream may only be a stream
 * opened by a call to _mali_osu_fopen(), as represented by an object of the
 * \ref _mali_file_t file descriptor type.
 *
 * The \ref MALI_STDOUT and \ref MALI_STDERR streams may not be read from.
 *
 * @note If the stream was opened in a mode other than tbe binary modes from
 * \ref _mali_file_mode_t, then it is \em implementation \em defined how line
 * endings are treated, and how many bytes they will occupy.
 *
 * @param ptr The buffer that data from the stream should be read into.
 * @param element_size The number of bytes for each element
 * @param num_elements The number of elements to read from th efile
 * @param file The file descriptor that represents the stream to read from.
 * @return The number of elements successfully read. On a read error or
 * end-of-file condition, the return value is strictly less than
 * \a num_elements. To distinguish between read errors and end-of-file
 * conditions, use _mali_osu_feof().
 */
int _mali_osu_fread(void *ptr, u32 element_size, u32 num_elements, _mali_file_t * file);

/** @brief Get stderr standard I/O stream.
 *
 * Return a static file descriptor to the stderr stream. The caller can expect
 * this stream to be always open for reading and writing. It is not necessary
 * to open/close the stream explicitely.
 *
 * Use of the returned stream is equivalent to using \ref MALI_STDERR directly,
 * but is provided for systems that cannot export variables from libraries.
 *
 * @return \ref _mali_file_t file descriptor that is equivalent to
 * \ref MALI_STDERR.
 */
_mali_file_t *_mali_osu_stderr(void);

/** @brief Test for the end-of-file condition.
 *
 * The end-of-file condition occurs when no more data can be read from a stream
 * using _mali_osu_fread(). It must be used to distinguish whether the return
 * value from _mali_osu_fread() indicated:
 * - that the end of the stream has been reached
 * - that there was an error in reading from the stream.
 *
 * @param file The file descriptor corresponding to a steam to test for the
 * end-of-file condition on.
 * @return non-zero when the end of file is reached, 0 otherwise
 */
int _mali_osu_feof(_mali_file_t *file);


/** @brief Get stdout standard I/O stream.
 *
 * Return a static file descriptor to the stdout stream. The caller can expect
 * this stream to be always open for reading and writing. It is not necessary
 * to open/close the stream explicitely.
 *
 * Use of the returned stream is equivalent to using \ref MALI_STDOUT directly,
 * but is provided for systems that cannot export variables from libraries.
 *
 * @return \ref _mali_file_t file descriptor that is equivalent to
 * \ref MALI_STDERR.
 */
_mali_file_t *_mali_osu_stdout(void);

/**
 * @brief Return a pseudo-random number.
 *
 * @note Only test code should be using this function.
 *
 * @return A pseudo-random number in the range 0..2^31-1.
 */
u32 _mali_osu_rand(void);

/**
 * @brief Return the calling process' pid
 * @return The process identification
 */
PidType _mali_osu_get_pid(void);

/**
 * @brief Return the calling thread's tid
 * @return The thread identification
 */
PidType _mali_osu_get_tid(void);

/** @} */ /* end group _mali_osu_debug_test_instrumented */


/**
 * Dynamically load a shared library.
 * @param name Name of library file to load.
 * @return A handle to the loaded library, or NULL for failure.
 */
_mali_library_t * _mali_osu_library_load(const char * name);

/**
 * Unload a previously dynamically loaded shared library.
 * @param handle Handle to the library to unload (as returned from _mali_osu_library_load).
 */
void _mali_osu_library_unload(_mali_library_t * handle);

/**
 * Calls the specific initialization function inside the specified library
 *
 * The initialization function is declared as;
 *     int mali_library_init(void* arg);
 *
 * For operating systems relying on ordinals, this function must be the first one.
 *
 * @param handle Handle to dynimic library to initialize.
 * @param param Pointer passed to initialization funcion within library.
 * @param retval value returned by the init function.
 * @return _MALI_OSU_ERR_OK if init function was called, otherwise _MALI_OSU_ERR_FAULT.
 */
_mali_osu_errcode_t _mali_osu_library_init(_mali_library_t * handle, void* param, u32* retval);

/**
 * Setup a static annotation channel to be used by _mali_osu_annotate_write.
 *
 * @return MALI_TRUE if the channel was available and opened successfully, MALI_FALSE otherwise.
 */
mali_bool _mali_osu_annotate_setup(void);

/**
 * Fully write data to the annotation channel opened by _mali_osu_annotate_setup.
 * If the channel has not been opened this function does nothing.
 *
 * @param data Pointer to the data to be written.
 * @param length Number of bytes to be written.
 */
void _mali_osu_annotate_write(const void* data, u32 length);

/**
 * Set scheduling policy to idle for current thread.
 *
 * @return _MALI_OSU_ERR_OK if policy was successfully set, _MALI_OSU_ERR_FAULT if not.
 */
_mali_osu_errcode_t _mali_osu_thread_set_idle_scheduling_policy(void);

/** @} */ /* end group osuapi */

/** @} */ /* end group uddapi */

#ifdef __cplusplus
}
#endif

#endif /* __MALI_OSU_H__ */
