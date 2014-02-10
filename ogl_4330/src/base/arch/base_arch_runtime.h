/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_arch_runtime.h
 */

#ifndef _BASE_ARCH_RUNTIME_H_
#define _BASE_ARCH_RUNTIME_H_

#include <stdarg.h>

#include <base/mali_types.h>
#include <base/mali_macros.h>
#include <base/mali_debug.h>
#include <base/common/base_common_context.h>


#ifdef __cplusplus
extern "C" {
#endif

/*** Memory functions ***/

/**
 * Allocate memory. A buffer of minimum ''size'' bytes is allocated.
 * Remember to free memory using @see _mali_base_arch_sys_free.
 * @param size Number of bytes to allocate
 * @return The buffer allocated
 */
void *_mali_base_arch_sys_malloc(u32 size);

/**
 * Free memory. Returns the buffer pointed to by ''pointer'' to the system.
 * Remember to free all allocated memory, and to only free it once.
 * @param pointer Pointer to buffer to free
 */
void _mali_base_arch_sys_free(void *pointer);

/**
 * Allocate zero-initialized memory. Returns a buffer capable of containing ''nelements'' of ''bytes'' size each.
 * @param nelements Number of elements to allocate
 * @param bytes Size of each element
 * @return The zero-initialized buffer allocated
 */
void *_mali_base_arch_sys_calloc(u32 nelements, u32 bytes);

/**
 * Reallocate memory. Tries to shrink or extend the memory buffer pointed to by ''ptr'' to ''bytes'' bytes.
 * If there is no room for an inline expansion the buffer will be relocated.
 * @param ptr Pointer to buffer to try to shrink/expand
 * @param bytes New size of buffer after resize
 * @return Pointer to the possibly moved memory buffer.
 */
void *_mali_base_arch_sys_realloc(void *ptr, u32 bytes);

/**
 * Generate a random number
 * Should return a 32-bit pseudo random number.
 * @return Random 32-bit number
 */
u32 _mali_base_arch_sys_rand(void);

/**
 * Get process indentification (pid)
 * @return 32-bit number with the process' identification
 */
PidType _mali_base_arch_sys_get_pid(void);

/**
 * Get thread indentification (tid)
 * @return 32-bit number with the thread's identification
 */
PidType _mali_base_arch_sys_get_tid(void);

/**
 * Copies the values of num bytes from the location pointed by source directly to
 * the memory block pointed by destination
 * @param ctx
 * @param destination Pointer to the destination array where the content is to be copied.
 * @param source Pointer to the source of data to be copied.
 * @param num Number of bytes to copy.
 */
void * _mali_base_arch_sys_memcpy(void * destination, const void * source, u32 num);

/**
 * Sets the first num bytes of the block of memory pointed to by ptr to the specified value
 * @param ctx
 * @param ptr Pointer to the block of memory to fill.
 * @param value Value to be set, passed as u32 big block is filled with unsigned char conversion.
 * @param num Number of bytes to be set to the value.
 */
void * _mali_base_arch_sys_memset(void * ptr, u32 value, u32 num);

/**
 * Compares n bytes of two regions of memory, treating each byte as an unsigned character.
 * It returns an integer less than, equal to or greater than zero, according to whether s1
 * is lexicographically less than, equal to or greater than s2.
 * @param ctx
 * @param s1 Points to the first buffer to compare
 * @param s2 Points to second buffer
 * @param n Number of bytes to compare
 */

s32 _mali_base_arch_sys_memcmp(const void *s1, const void *s2, u32 n);

/*** Spinlock functions ***/

mali_spinlock_handle _mali_base_arch_sys_spinlock_create(void);
void _mali_base_arch_sys_spinlock_lock(mali_spinlock_handle spinlock);
mali_err_code _mali_base_arch_sys_spinlock_try_lock(mali_spinlock_handle spinlock);
void _mali_base_arch_sys_spinlock_unlock(mali_spinlock_handle spinlock);
void _mali_base_arch_sys_spinlock_destroy(mali_spinlock_handle spinlock);

/*** Mutex functions ***/

/**
 * Auto initialize a mutex on first use.
 * Used to initialize a mutex by the first thread referencing it.
 * This function will fail if the mutex could not be created or the given pointer is NULL.
 * This function should only be used when you don't know if a mutex has been created or not,
 * use @see _mali_sys_mutex_create for normal initialization functions.
 * @param pHandle A pointer to the mutex
 * @return MALI_ERR_NO_ERROR if mutex is ready for use, or MALI_ERR_FUNCTION_FAILED on failure
 */
mali_err_code _mali_base_arch_sys_mutex_auto_init(volatile mali_mutex_handle * pHandle) MALI_CHECK_RESULT;

/**
 * Obtains access to a statically initialized mutex. Initially these
 * mutexes are in an unlocked state.
 * @param id Identifier of a statically initialized mutex
 * @return A handle to a statically initialized mutex. NULL on failure.
 */
mali_mutex_handle _mali_base_arch_sys_mutex_static(mali_static_mutex id);

/**
 * Create a mutex.
 * Creates a mutex, initializes it to an unlocked state and returns a handle to it.
 * When the mutex is no longer needed it has to destroyed, see @see _mali_base_arch_mutex_destroy.
 * @return A handle to a new mutex
 */
mali_mutex_handle _mali_base_arch_sys_mutex_create(void);

/**
 * Destroy a mutex.
 * Destroys a mutex previously allocated by @see _mali_base_arch_mutex_create.
 * Returns the resources used by this mutex to the system.
 * @param mutex The mutex to destroy.
 */
void _mali_base_arch_sys_mutex_destroy(mali_mutex_handle mutex);

/**
 * Lock a mutex.
 * Locks the mutex, blocks until a lock can be taken if the mutex is already locked.
 * When finished with a mutex, remember to unlock it with @see _mali_base_arch_mutex_unlock.
 * @param mutex The mutex to lock
 */
void _mali_base_arch_sys_mutex_lock(mali_mutex_handle mutex);

/**
 * Try to lock a mutex, but don't block if it's already locked.
 * This function tries to lock a mutex, but returns with an error if the mutex can't be locked right away.
 * When finished with a mutex, remember to unlock it with @see _mali_base_arch_mutex_unlock.
 * @param mutex The mutex to try to lock
 * @return If the mutex was successfully locked: MALI_ERR_NO_ERROR,
 * 			on error or if the mutex was locked: MALI_ERR_FUNCTION_FAILED
 */
mali_err_code _mali_base_arch_sys_mutex_try_lock(mali_mutex_handle mutex) MALI_CHECK_RESULT;

/**
 * Unlock a locked mutex.
 * This function unlocks a previously locked mutex.
 * @param mutex The mutex to unlock
 */
void _mali_base_arch_sys_mutex_unlock(mali_mutex_handle mutex);

/**
 * Create a lock object.
 * Lock objects extends mutexes by being unlockable from a different thread than the one doing the locking.
 * Locks are initially in an unlocked state.
 * @return Handle of a new lock or MALI_NO_HANDLE on error
 */
mali_lock_handle _mali_base_arch_sys_lock_create(void);

/**
 * Auto initialize a lock object on first use.
 * Used to initialize a lock object by the first thread referencing it.
 * This function will fail if the lock object could not be created or the given pointer is NULL.
 * This function should only be used when you don't know if a lock object has been created or not,
 * use @see _mali_sys_lock_create for normal initialization functions.
 * @param pHandle A pointer to the lock handle
 * @return MALI_ERR_NO_ERROR if the lock object is ready for use, or MALI_ERR_FUNCTION_FAILED on failure
 */
mali_err_code _mali_base_arch_sys_lock_auto_init(volatile mali_lock_handle * pHandle) MALI_CHECK_RESULT;

/**
 * Destroy a lock object.
 * Destroy a lock previously created by @see _mali_base_arch_sys_lock_create
 * Locks must be in the unlocked state when being destroyed
 * @param lock Handle to the lock object to destroy
 */
void _mali_base_arch_sys_lock_destroy(mali_lock_handle lock);

/**
 * Lock a lock object.
 * When returning the lock object will be in a locked state.
 * If the lock object is already in this state the function
 * will wait until it comes into the unlocked state before taking the lock.
 * @param lock Handle to the lock object to lock
 */
void _mali_base_arch_sys_lock_lock(mali_lock_handle lock);

/**
 * Lock a lock object with timeout.
 * When returning the lock object will be in a locked state unless the timeout expired.
 * If the lock object is already in this state the function
 * will wait until it comes into the unlocked state before taking the lock.
 * If the supplied timeout expires then the function will return without locking.
 * @param lock Handle to the lock object to lock
 * @param timeout Relative time in microseconds of timeout
 * @return MALI_ERR_NO_ERROR if the lock was obtained or MALI_ERR_TIMEOUT on timeout
 */
mali_err_code _mali_base_arch_sys_lock_timed_lock(mali_lock_handle lock, u64 timeout);

/**
 * Try to lock a lock object, but don't block if it's already locked.
 * This function tries to lock a lock object, but returns with an error if the lock object can't be locked right away.
 * @param mutex Handle to the lock object to try to lock
 * @return If the mutex was successfully locked: MALI_ERR_NO_ERROR,
 * 			on error or if the mutex was locked: MALI_ERR_FUNCTION_FAILED
 */
mali_err_code _mali_base_arch_sys_lock_try_lock(mali_lock_handle lock) MALI_CHECK_RESULT;

/**
 * Unlock a lock object.
 * Will unlock the lock object, notifying any other threads waiting for the lock before returning.
 * Calling unlock on a lock object in the unlocked state is invalid and will result in undefined behavior.
 * @param lock Handle to the lock object to unlock
 */
void _mali_base_arch_sys_lock_unlock(mali_lock_handle lock);

/*** Thread Local Storage ***/

/**
 * Set data stored in the TLS slot referenced by ''key''.
 * @param key The TLS key
 * @param value The value to store in the slot
 */
mali_err_code _mali_base_arch_sys_thread_key_set_data(mali_thread_keys key, void* value) MALI_CHECK_RESULT;

/**
 * Get data stored in the TLS slot referenced by ''key''.
 * @param key The TLS key
 * @return The value stored in the slot
 */
void* _mali_base_arch_sys_thread_key_get_data(mali_thread_keys key);

/**
 * Get current thread identifier
 * @return 32-bit thread identifier
 */
u32 _mali_base_arch_sys_thread_get_current(void);

/**
 * Yield the processor from the currently executing thread to another ready to run,
 * active thread of equal or higher priority
 * Function will return if there are no such threads, and continue to run
 * @return MALI_ERR_NO_ERROR if successfull, or MALI_ERR_FUNCTION_FAILED on failure
 */
mali_err_code _mali_base_arch_sys_yield(void);

/*** Generic functions ***/
/**
 * printf wrapper.
 * Follows the usual behavior of printf, so you can see its documentaton for its uage.
 */
int _mali_base_arch_sys_printf(const char *format, va_list args);


/**
 * snprintf wrapper.
 * A failsafe snprintf wrapper. The function is like a printf that prints
 * its contents to a text string with a given maximum length.
 * We guarantee that the last byte written will be a Zero byte, that
 * terminates the text string, and that the total number of bytes written
 * will never exceed size bytes. This guarantee also implies that the
 * maximum letters written to the string is (size-1), before the
 * Zero-termination.
 * @param str The string that this function writes to.
 * @param size Number of bytes in the buffer.
 * @param format The input text string that formats the output string.
 * @param ... Input parameters for the input text string.
 */
int _mali_base_arch_sys_snprintf(char* str, u32 size, const char * format, va_list args);

/**
 * Sort elements of an array.
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
void _mali_base_arch_sys_qsort(void * base, u32 num, u32 size, int (*compare)(const void *, const void *));

/** A wrapper for fopen.
 * This works like the standard fopen, except for the return value.  This value is the
 * descriptor used by the underlying operating system, but reinterpreted as a  mali_file *.
 * \param path the path to and name of the file to be opened, in the notation of the
 *		underlying operating system
 * \param mode string specifying the type of access to be done, as for the standard fopen.
 * \return a file descriptor, in the form of a mali_file (data type).
 */
mali_file * _mali_base_arch_sys_fopen(const char * path, const char * mode);


/** A wrapper for fprintf, or actually vfprintf.
 * This works like the standard vfprintf, except for the type of the file/stream parameter.
 * The value of this parameter is the descriptor used by the underlying operating system,
 * but reinterpreted as a  mali_file *.
 * \param file The formatted print text will go to the strean identified by this.
 * \param format A format string, as in standard printf.
 * \param args The list of values to be formatted and output.
 * \return The number of characters output, not counting the terminating '\0'; as for standard printf.
 */
int  _mali_base_arch_sys_fprintf(mali_file * file, const char * format, va_list args);

/** A wrapper for fwrite
 * This works like the standard fwrite, except for the type of the file/stream parameter.
 * The value of this parameter is the descriptor used by the underlying operating system,
 * but reinterpreted as a  mali_file *.
 * \param data Pointer to the memory we want to write from
 * \param element_size Size of the element we want to print
 * \param element_number Number of elements of element_size that should be written. Usually 1
 * \param mali_file The file function writes to. Opened with _mali_sys_fopen
 * \return Number of elements written. If this is unequal to \a element_number there was an error.
 */
u32 _mali_base_arch_sys_fwrite(const void * data, u32 element_size, u32 element_number, mali_file *file);


/** A wrapper for fclose.
 * This works like the standard fclose, except for the type of the parameter.
 * The value of this parameter is the descriptor used by the underlying operating system,
 * but reinterpreted as a  mali_file *.
 * \param file The formatted print text will go to the strean identified by this.
 */
void  _mali_base_arch_sys_fclose(mali_file * file);

/**
 * A wrapper for remove.
 * This works like the standard remove.
 * \filename the path to and name of the file to be opened, in the notation of the
 *      underlying operating system
 * \return Upon successful completion, 0 shall be returned. Otherwise, -1 shall be returned.
 */
int _mali_base_arch_sys_remove(const char *filename);

/**
 * A sleep function.
 * Takes the number of microseconds to sleep.
 * The function will sleep AT LEAST usec microseconds.
 * The sleep duration might be longer due to scheduling on the system.
 * @param usec Number of microseconds to sleep
 */
void _mali_base_arch_sys_usleep(u64 usec);

/**
 * Get a timer with at most, microseconds precision.
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
u64 _mali_base_arch_sys_get_time_usec(void);

/**
 * Program will exit if this function is called. Function is called from
 * assert-macro in mali_debug.h.
 */
void _mali_base_arch_sys_abort(void);

/**
 * Sets breakpoint at point where function is called.
 */
void _mali_base_arch_sys_break(void);

/**
 * Converts the initial part of the string in nptr to an unsigned 64 bit integer value
 * according to the given base, which must be between 2 and 36 inclusive or be
 * the special value 0.
 * @param nptr Character string to convert
 * @param endptr Pointer to the character that ended the scan or a null value
 * @param base Specifies the base to use for the conversion.
 */
u64 _mali_base_arch_sys_strtoull(const char *nptr, char **endptr, u32 base);

/**
 * Converts the initial part of the string in nptr to a signed 64 bit integer value
 * according to the given base, which must be between 2 and 36 inclusive or be
 * the special value 0.
 * @param nptr Character string to convert
 * @param endptr Pointer to the character that ended the scan or a null value
 * @param base Specifies the base to use for the conversion.
 */
s64 _mali_base_arch_sys_strtoll(const char *nptr, char **endptr, u32 base);

/**
 * Converts the initial portion of the string pointed to by nptr to a double representation
 * @pararm ctx
 * @param nptr Character string to convert
 * @param endptr Pointer to the character that ended the scan or a null value
 */
double _mali_base_arch_sys_strtod(const char *nptr, char **endptr);

/**
 * Returns the length of str
 * @param ctx
 * @param str C string
 */
u32 _mali_base_arch_sys_strlen(const char *str);

/**
 * Creates a duplicate of the string argument.
 * Creates a copy of the string argument. The copy will
 * contain the same bytes as the original up to and including
 * the first zero terminator. A zero-length string will result
 * in another zero-length string.
 * @param str The string to duplicate
 * @return The duplicated string, or NULL if the string duplication failed
 */
char * _mali_base_arch_sys_strdup(const char * str);

/**
 * Performs a lexical comparison of str1 and str2
 * @param str1 C string
 * @param str2 C string
 * @return If str1==str2, returns 0. If str1<str2, returns value<0, if str1>str2, returns >0.
 */
u32 _mali_base_arch_sys_strcmp(const char *str1, const char *str2);

/**
 * Performs a lexical comparison of the first count characters of str1 and str2
 * @param str1 C string
 * @param str2 C string
 * @return If str1(0:n-1)==str2(0:n-1), returns 0. If str1(0:n-1)<str2(0:n-1), returns value<0, if str1(0:n-1)>str2(0:n-1), returns >0.
 */
u32 _mali_base_arch_sys_strncmp(const char *str1, const char *str2, u32 count);

/**
 * Copy string, limited to n bytes
 * @param dest Where to copy the string to
 * @param src String to copy
 * @param n Maximum number of bytes to copy
 * @return The destination string
 */
char *_mali_base_arch_sys_strncpy(char *dest, const char *src, u32 n);

/**
 * String concatenation, limited to n bytes
 * Appends the src string to the destination string, limited to maximum n bytes
 * @param dest The string to append to
 * @param src The string to append
 * @param n The maximum bytes to add to dest
 * @return The destination string
 */
char *_mali_base_arch_sys_strncat(char *dest, const char *src, u32 n);

/**
 * Converts a string into integer
 * @param str The string to be converted
 * @return The integer value of the string
 */
u32 _mali_base_arch_sys_atoi(const char *str);

/*
 *	Math functions
 */

/**
 *	Square root
 *	@param value	Value to take the square root of
 *	@return			The square root
 */
float _mali_base_arch_sys_sqrt(float value);

/**
 *	Sine
 *	@param value	Value to take the sine of
 *	@return			The sine
 */
float _mali_base_arch_sys_sin(float value);

/**
 *	Cosine
 *	@param value	Value to take the cosine of
 *	@return			The cosine
 */
float _mali_base_arch_sys_cos(float value);


/**
 *	Arc tangent
 *	@param y y-coordinate
 *	@param x x-coordinate
 *	@return  Principal arc tangent of y/x in the range [-pi,pi] (radians)
 */
float _mali_base_arch_sys_atan2(float y, float x);


/**
 *	Ceil - Round up
 *	@param value	Value to take the ceil of
 *	@return			The ceiled value
 */
float _mali_base_arch_sys_ceil(float value);

/**
 *	Floor - Round down
 *	@param value	Value to round down
 *	@return			The floored value
 */
float _mali_base_arch_sys_floor(float value);

/**
 *	Absolute value
 *	@param value	Value to take the absolute value of
 *	@return			The absolute value
 */
float _mali_base_arch_sys_fabs(float value);

/**
 * Absolute value
 * @param value Value to take the absolute value of
 * @return The absolute value
 */
s32 _mali_base_arch_sys_abs(s32 value);

/**
 * Absolute value
 * @param value Value to take the absolute value of
 * @return The absolute value
 */
s64 _mali_base_arch_sys_abs64(s64 value);

/**
 *	Exponent
 *	@param value	Value to take the exponent of
 *	@return			The exponent
 */
float _mali_base_arch_sys_exp(float value);

/**
 *	Power function
 *	@param base		Base value to raise to a power
 *	@param exponent	Exponent to which to raise the base
 *	@return			The value of base to the power of exponent
 */
float _mali_base_arch_sys_pow(float base, float exponent);

/**
 *	Natural Logarithm function
 *	@param value	Value to find logarithm of
 *	@return			Log to base e of value
 */
float _mali_base_arch_sys_log(float value);

/**
 *  Judge whether the input is a NaN
 *  @param value    Value to be judged
 *  @return         If value has NaN, return non-zero value 
 */
int _mali_base_arch_sys_isnan(float value);

/**
 * 	Floating point modulus function
 * 	@param n The value to take the modulus of
 *	@param m The modulus
 *	@return n - x*2 for some x so that the restult is smaller than m
 */
float _mali_base_arch_sys_fmodf(float n, float m);

/**
 * Creates a mali_base_wait_handle
 */
mali_base_wait_handle _mali_base_arch_sys_wait_handle_create(void);

/**
 * Wait for a wait handle to trigger
 * Will block this thread until a wait handle has been triggered.
 * @param mali_base_handle Handle to a wait object
 */
void _mali_base_arch_sys_wait_handle_wait(mali_base_wait_handle handle);

/**
 * Wait for a wait handle to trigger.
 * Will block this thread until a wait handle has been triggered.
 * If the supplied timeout expires then the function even if the handle hasn't been triggered.
 * @param handle Handle to a wait object
 * @param timeout Relative time in microseconds of timeout
 * @return MALI_ERR_NO_ERROR if the handle was triggered  or MALI_ERR_TIMEOUT on timeout
 */
mali_err_code _mali_base_arch_sys_wait_handle_timed_wait(mali_base_wait_handle handle, u64 timeout);

/**
 * Abandon a wait handle.
 * Allows the waiting thread to signal that it has abandoned the object and will never finish the wait
 * @param handle The handle to abandon
 */
void _mali_base_arch_sys_wait_handle_abandon(mali_base_wait_handle handle);

/**
 * Triggers a wait_handle.
 * Any thread running @see _mali_base_arch_sys_wait_handle_wait will no longer be blocked
 * @note If the handle has been abandoned this will finish the cleanup
 * @param mali_base_handle Handle to a wait object
 */
void _mali_base_arch_sys_wait_handle_trigger(mali_base_wait_handle handle);


/*
 * Atomic integer operations
 */

/**
 * Atomic integer increment
 * @param atomic Pointer to mali_atomic_int to atomically increment
 */
void _mali_base_arch_sys_atomic_inc(mali_atomic_int * atomic);

/**
 * Atomic integer increment and return post-increment value
 * @param atomic Pointer to mali_atomic_int to atomically increment
 * @return Value of val after increment
 */
u32 _mali_base_arch_sys_atomic_inc_and_return(mali_atomic_int * atomic);

/**
 * Atomic integer decrement
 * @param atomic Pointer to mali_atomic_int to atomically decrement
 */
void _mali_base_arch_sys_atomic_dec(mali_atomic_int * atomic);

/**
 * Atomic integer decrement and return post-decrement value
 * @param atomic Pointer to mali_atomic_int to atomically decrement
 * @return Value of val after decrement
 */
u32 _mali_base_arch_sys_atomic_dec_and_return(mali_atomic_int * atomic);

/**
 * Atomic integer get. Read value stored in the atomic integer.
 * @param atomic Pointer to mali_atomic_int to read contents of
 * @return Value currently in mali_atomic_int
 * @note If an increment/decrement/set is currently in progress you'll get either
 * the pre or post value.
 */
u32 _mali_base_arch_sys_atomic_get(mali_atomic_int * atomic);

/**
 * Atomic integer set. Used to explicitly set the contents of an atomic
 * integer to a specific value.
 * @param atomic Pointer to mali_atomic_int to set
 * @param value Value to write to the atomic int
 * @note If increment or decrement is currently in progress the mali_atomic_int will contain either the value set, or the value after increment/decrement.
 * @note If another set is currently in progress it's uncertain which will
 * persist when the function returns.
 */
void _mali_base_arch_sys_atomic_set(mali_atomic_int * atomic, u32 value);

/**
 * Initialize an atomic integer. Used to initialize an atomic integer to
 * a sane value when created.
 * @param atomic Pointer to mali_atomic_int to set
 * @param value Value to initialize the atomic integer to
 * @note This function has to be called when a mali_atomic_int is
 * created/before first use
 */
void _mali_base_arch_sys_atomic_init(mali_atomic_int * atomic, u32 value);

/*
 *      System configuration/interaction
 */

/**
 * Load (extra) configuration variables.
 * Some platforms do not have a built-in method for specifying config variables
 * for a specific application. This function will make sure the application specific
 * configuration variables are loaded. All variables loaded can afterwards be
 * accessed through _mali_sys_config_string_get() and related functions.
 *      @return                 void
 */
void _mali_base_arch_sys_load_config_strings(void);

/**
 * Get text string from the OS.
 * The intentional use is to be able to read enviroment variables set in the OS.
 * This will make it easier to configure the programs and drivers without doing it
 * through configuration files or recompilation.
 *      @note You must free the string you get by a call to \a _mali_sys_config_string_release()
 *      @param name             The name of the variable we want to read
 *      @return                 The string with text if the name exixted, or NULL.
 */
const char * _mali_base_arch_sys_config_string_get(const char * name);

/**
 * Set a text string in the OS.
 * The intentional use is to be able to set OS enviroment variables.
 * The variable's new value will only be guaranteed to be available to the
 * current process, and new children of the current process.
 * This will make it easier to configure the programs and drivers without doing it
 * through configuration files or recompilation.
 *      @param name             The name of the variable we want to set
 *      @param value            The value to assign to the variable
 *      @return                 MALI_TRUE on success, MALI_FALSE otherwise.
 */
mali_bool _mali_base_arch_sys_config_string_set(const char * name, const char *value);

/**
 * Free allocated text string from the OS.
 * The function frees a text string allocated by \a _mali_sys_config_string_get()
 * It is legal to input a NULL-pointer.
 *      @param env_var  The env_var returned from \a _mali_sys_config_string_get()
 */
void _mali_base_arch_sys_config_string_release(const char * env_var);

/**
 * Dynamically load a shared library.
 * @param name Name of library file to load.
 * @return A handle to the loaded library, or NULL for failure.
 */
MALI_IMPORT mali_library * _mali_base_arch_library_load(const char * name) MALI_CHECK_RESULT;

/**
 * Unload a previously dynamically loaded shared library.
 * @param handle Handle to the library to unload (as returned from _mali_base_arch_library_load).
 */
MALI_IMPORT void _mali_base_arch_library_unload(mali_library * handle);

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
 * @return MALI_TRUE if init function was called, otherwise MALI_FALSE.
 */
MALI_IMPORT mali_bool _mali_base_arch_library_init(mali_library * handle, void* param, u32* retval) MALI_CHECK_RESULT;

/**
 * Get a setting value from the settings DB
 *
 * @param setting the ID of the setting to retrieve
 * @return The requested value
 */
MALI_IMPORT u32 _mali_base_arch_get_setting(_mali_setting_t setting);

#ifdef __cplusplus
}
#endif

#endif /* _BASE_ARCH_RUNTIME_H_ */
