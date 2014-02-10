/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2009-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

#ifndef _MALI_TPI_INTERNAL_H_
#define _MALI_TPI_INTERNAL_H_

/* ------------------------------------------------------------------------- */
/** @page tpi_page Mali Test Porting Interface
 *
 * The Mali Test Porting Interface (TPI) provides a platform independent API
 * for handling common tasks encountered during development of applications
 * running on typical embedded and desktop platforms. The API tries to provide
 * functional and portable libraries where no standard API exists or where
 * standard APIs are inconsistently implemented.
 *
 * The prioritization of portability over functionality means that this API is
 * in many ways more restrictive than most C library implementations. This is
 * so that we can cope with the underlying features of the most basic platform
 * which we have to support.
 *
 * @note This documentation is informative, not normative. Please refer to the
 * document Mali Test Porting Interface User Guide (PR441-GENC-010402) for the
 * formal specification.
 *
 * @section tpi_core_sec TPI Core Functionality
 *
 * The core functionality of TPI abstracts the parts of the C library and
 * platform libraries which are not portable or which are not consistently
 * implemented by the various library providers. In summary we support:
 *
 *   - Memory Allocators
 *   - String Formatting and I/O Streams
 *   - File I/O
 *   - Time
 *   - Threading, Locks and Semaphores
 *   - Random Number Generation
 *   - Atomic Access
 *
 * @subsection tpi_core_mem_sec Memory Allocators
 *
 * Portable drop-in implementations of @c malloc, and @c free. By replacing
 * the built-in versions we gain the ability to instrument the test memory
 * allocations independently of the product memory allocations, and as such can
 * trivially debug memory leaks in the tests themselves.
 *
 * @subsection tpi_core_stf_sec String Formatting and I/O Streams
 *
 * Portable drop-in implementations of @c snprintf, and @c vsnprintf, for
 * formatting strings into a buffer. Abstraction allows us to support
 * identical formatting and extensions on all platforms.
 *
 * Portable drop-in file streams for @c stdin, and @c stdout. This abstraction
 * allows us to retarget log streams to file or serial consoles on embedded
 * devices.
 *
 * Portable drop-in implementations of @c puts, @c printf, and @c vprintf.
 *
 * @subsection tpi_core_fio_sec File I/O
 *
 * A portable implementation of @c fopen, @c fflush, @c fprintf, @c vfprintf,
 * @c fputs, @c fread, @c fwrite, and @c fclose. This area of the TPI is
 * perhaps one of the more restrictive compared to more fully fledged file
 * APIs. This is to cope with the lack of "current working directory" in
 * Windows CE and Symbian OS. Instead the TPI supports two symbolic root
 * locations @c MALI_TPI_DIR_DATA and @c MALI_TPI_DIR_RESULTS which refer to a
 * platform specific file location. The location @c MALI_TPI_DIR_DATA is a
 * read-only location, and @c MALI_TPI_DIR_RESULTS is suitable for writing
 * result logs.
 *
 * @subsection tpi_core_tim_sec Time
 *
 * A portable implementation of some custom time functions which can be
 * portably implemented on top of the available APIs. We support a single
 * function for getting the current time since an arbitrary origin,
 * @c mali_tpi_get_time_ns, and some functions for implementing portable
 * sleep functions, @c mali_tpi_sleep_ns, and @c mali_tpi_sleep_until_ns.
 * Actual precision of these time and sleep functions is implementation
 * dependent; most systems only provide millisecond precision.
 *
 * @subsection tpi_core_trd_sec Threading Support
 *
 * A portable implementation of thread management and simple mutex and semaphores
 * locks.  This interface exposing a subset of the functionality found in pthreads
 * which we know we can portably implement on other systems. It is intentionally
 * restrictive to try and reduce error-prone complex use of threads in the tests
 * themselves.
 *
 * @subsection tpi_core_rng_sec Random Support
 *
 * A portable implementation of random number generation, for various types,
 * which is independent of the host C library. This consistency of random
 * number generation allows cross-platform testing which produces directly
 * comparable results - ideal if tests want to compare against reference data.
 *
 * @section tpi_egl_sec TPI EGL Helper Functionality
 *
 * The TPI provides an optional secondary library which provides helper
 * functions for handling the parts of the windowing system which the Khronos
 * EGL APIs skip over; creating, manipulating, and destroying windows which
 * can then in turn be bound to a rendering context.
 *
 * Function-level documentation can be found on the Modules tab.
 *
 *    - @ref tpi_core
 *    - @ref tpi_egl
 */

/* ------------------------------------------------------------------------- */
/**
 * @file mali_tpi_internal.h
 * @brief Prototypes for the interface.
 *
 * @addtogroup tpi_core TPI Core library
 * @{
 */

/* ============================================================================
	Includes
============================================================================ */
#include "malisw/mali_malisw.h"

#include <stdarg.h>
#include <stddef.h>
#include "plat/mali_tpi_atomic_helpers.h"
#include "plat/mali_tpi_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
	Type Definitions
	We don't use the coding standard types directly to allow a customer to
	port this header to use an appropriate local types which may conflict with
	the coding standard headers for their local OS and application pairing.
============================================================================ */
typedef uint8_t                 mali_tpi_uint8;
typedef int8_t                  mali_tpi_int8;
typedef uint16_t                mali_tpi_uint16;
typedef int16_t                 mali_tpi_int16;
typedef uint32_t                mali_tpi_uint32;
typedef int32_t                 mali_tpi_int32;
typedef uint64_t                mali_tpi_uint64;
typedef int64_t                 mali_tpi_int64;

typedef bool_t                  mali_tpi_bool;

#define MALI_TPI_TRUE           TRUE
#define MALI_TPI_FALSE          FALSE

#define MALI_TPI_EXPORT         CSTD_LINK_EXPORT
#define MALI_TPI_IMPORT         CSTD_LINK_IMPORT
#ifndef MALI_TPI_IMPL
 #define MALI_TPI_IMPL           CSTD_LINK_IMPL
#endif

#ifndef MALI_TPI_API
    #define MALI_TPI_API        MALI_TPI_IMPORT
#endif

#define TPI_PRIV_PLATFORM_SEM_STRUCT_PADDING 1

#define MALI_TPI_VERSION_MAJOR  1
#define MALI_TPI_VERSION_MINOR  1

/* ------------------------------------------------------------------------- */
/**
 * @defgroup tpi_core_init Initialization and shutdown
 * @{
 */

/**
 * @brief Public initialization function
 *
 * @return @c MALI_TPI_TRUE on success, @c MALI_TPI_FALSE on failure.
 *
 * @note This function is not guaranteed to be reentrant; users of the TPI
 * library should ensure that this function is only called from a single thread
 * context.
 */
MALI_TPI_API mali_tpi_bool mali_tpi_init( void );

/* ------------------------------------------------------------------------- */
/**
 * @brief Public shutdown function
 *
 * @return @c MALI_TPI_TRUE on success, @c MALI_TPI_FALSE on failure
 *
 * @note This function is not guaranteed to be reentrant; users of the TPI
 * library should ensure that this function is only called from a single thread
 * context.
 */
MALI_TPI_API mali_tpi_bool mali_tpi_shutdown( void );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @addtogroup tpi_core_internal Private functions
 * @{
 */

/**
 * @brief Internal initialization function
 *
 * @return @c MALI_TPI_TRUE on success, @c MALI_TPI_FALSE on failure.
 *
 * @note This function need not be reentrant.
 */
mali_tpi_bool _mali_tpi_init_internal( void );

/* ------------------------------------------------------------------------- */
/**
 * @brief Internal shutdown function
 *
 * @return @c MALI_TPI_TRUE on success, @c MALI_TPI_FALSE on failure.
 *
 * @note This function need not be reentrant.
 */
mali_tpi_bool _mali_tpi_shutdown_internal( void );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @defgroup tpi_core_stdio Test output functions
 * @{
 */

/**
 * @brief Return a handle to the standard output stream.
 *
 * @return A pointer to the stream, or @c NULL on failure.
 */
MALI_TPI_API mali_tpi_file* mali_tpi_stdout( void );

/* ------------------------------------------------------------------------- */
/**
 * @brief Return a handle to the standard error stream.
 *
 * @return A pointer to the stream, or @c NULL on failure.
 */
MALI_TPI_API mali_tpi_file* mali_tpi_stderr( void );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @defgroup tpi_core_format String formatting functions
 * @{
 */

/**
 * @brief Portable printf-style string formatting.
 *
 * Refer to the specification for restrictions on the format string.
 *
 * @param output   A callback function that is fed each character. It must
 *                 return a non-negative value on success, negative on failure.
 * @param arg      A generic argument passed as the second argument to
 *                 @c output.
 * @param format   The format string
 * @param ap       The variadic arguments to format.
 *
 * @return The number of characters written on success, or a negative value
 * on failure.
 */
MALI_TPI_API int mali_tpi_format_string(
	int(*output)(char, void *),
	void *arg,
	const char *format,
	va_list ap );

/* ------------------------------------------------------------------------- */
/**
 * @brief Portable equivalent to @c vsnprintf.
 *
 * This implements C99 semantics for the return value. Refer to the TPI
 * specification for restrictions on the format string.
 *
 * @param str      The string to fill in - may be @c NULL if @c size is 0
 * @param size     The size of @c str, including space for terminator
 * @param format   The format string
 * @param ap       The variadic arguments to format.
 *
 * @return The number of characters that would be written (ignoring truncation),
 * or a negative value if an illegal format string was detected.
 */
MALI_TPI_API int mali_tpi_vsnprintf(
	char *str, size_t size,
	const char *format,
	va_list ap );

/* ------------------------------------------------------------------------- */
/**
 * @brief Portable equivalent to @c snprintf.
 *
 * This implements C99 semantics for the return value. Refer to the TPI
 * specification for restrictions on the format string.
 *
 * @param str      The string to fill in - may be @c NULL if @c size is 0
 * @param size     The size of @c str, including space for terminator
 * @param format   The format string
 *
 * @return The number of characters that would be written (ignoring truncation),
 * or a negative value if an illegal format string was detected.
 */
MALI_TPI_API int mali_tpi_snprintf(
	char *str,
	size_t size,
	const char *format,
	... );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @defgroup tpi_core_fs File-system functions
 * @{
 */

/**
 * @brief Symbolic names for possible file locations.
 */
typedef enum {
	/** Input files for the test. */
	MALI_TPI_DIR_DATA,
	/** Results of the test. */
	MALI_TPI_DIR_RESULTS
} mali_tpi_dir;

/* ------------------------------------------------------------------------- */
/**
 * @brief Open a file.
 *
 * @param dir      Directory to store files.
 * @param filename Path for the file, relative to @c dir, using slash as a
 *                 directory separator.
 * @param mode     Mode to pass to C @c fopen function.
 *
 * @return The file handle on success, NULL on failure
 */
MALI_TPI_API mali_tpi_file *mali_tpi_fopen(
	mali_tpi_dir dir,
	const char *filename,
	const char *mode );

/* ------------------------------------------------------------------------- */
/**
 * @brief Write data to a stream.
 *
 * It is not guaranteed that data members are written atomically. If you
 * want to know exactly how many bytes were written, set @c size to 1.
 *
 * @param ptr      The data to write
 * @param size     The size in bytes of each data member to write
 * @param nmemb    The number of data members to write
 * @param stream   The output stream to write
 *
 * @return The number of complete data members successfully written.
 */
MALI_TPI_API size_t mali_tpi_fwrite(
	const void *ptr,
	size_t size,
	size_t nmemb,
	mali_tpi_file *stream );

/* ------------------------------------------------------------------------- */
/**
 * @brief Write formatted text to a stream
 *
 * This function uses @c mali_tpi_format_string to format the data.
 *
 * @param stream   The stream to write to
 * @param format   The format string (see @c mali_tpi_format_string)
 * @param ap       The variadic arguments
 *
 * @return The number of characters written on success, or a negative value on
 * failure.
 */
MALI_TPI_API int mali_tpi_vfprintf(
	mali_tpi_file *stream,
	const char *format,
	va_list ap );

/* ------------------------------------------------------------------------- */
/**
 * @brief Write formatted text to a stream
 *
 * This function uses @c mali_tpi_format_string to format the data.
 *
 * @param stream   The stream to write to
 * @param format   The format string (see @c mali_tpi_format_string)
 *
 * @return The number of characters written on success, or a negative value on
 * failure.
 */
MALI_TPI_API int mali_tpi_fprintf(
	mali_tpi_file *stream,
	const char *format,
	... );

/* ------------------------------------------------------------------------- */
/**
 * @brief Write a string to a stream
 *
 * The string does not have any newline appended.
 *
 * @param str      A null-terminated string to output
 * @param stream   The stream to output it to
 *
 * @return A non-negative value on success, or a negative value on failure.
 */
MALI_TPI_API int mali_tpi_fputs(
	const char *str,
	mali_tpi_file *stream );

/* ------------------------------------------------------------------------- */
/**
 * @brief Read data from a stream.
 *
 * It is not guaranteed that data members are read atomically. If you want to
 * know exactly how many bytes were read, set \a size to 1.
 *
 * There is no way to distinguish EOF from error.
 *
 * @param ptr      The data buffer to read to
 * @param size     The size in bytes of each data member to read
 * @param nmemb    The number of data members to read
 * @param stream   The input stream to read
 *
 * @return The number of complete data members successfully read.
 */
MALI_TPI_API size_t mali_tpi_fread(
	void *ptr,
	size_t size,
	size_t nmemb,
	mali_tpi_file *stream );

/* ------------------------------------------------------------------------- */
/**
 * @brief Close an open file stream
 *
 * @param stream   The stream to close
 *
 * @return zero on success, a negative value on failure
 */
MALI_TPI_API int mali_tpi_fclose(
	mali_tpi_file *stream );

/* ------------------------------------------------------------------------- */
/**
 * @brief Flush an open file stream
 *
 * This only guarantees that the data has reached the operating system (and
 * hence should not be lost if the process terminates). There is no way to
 * guarantee that the data has been committed to physical media.
 *
 * @param stream The stream to flush
 */
MALI_TPI_API int mali_tpi_fflush(
	mali_tpi_file *stream );

/* ------------------------------------------------------------------------- */
/**
 * @brief Query the size of the file beneath an open file stream.
 *
 * This function queries the size of a file. The file must have been opened
 * in binary mode, or the results are unpredictable.
 *
 * It is only guaranteed to be supported for files stored in the
 * @c MALI_TPI_DIR_DATA; support for querying the size of files in the
 * @c MALI_TPI_DIR_RESULTS directory is implementation defined, and use is
 * discouraged as the aim of TPI is for portability of test code.
 *
 * When executing this function it is guaranteed that the current position of
 * the file stream will be unchanged when the function returns, unless an
 * error occurs manipulating the file, in which case it is unpredictable.
 *
 * @param stream   The stream to query the underlying file size of.
 *
 * @return The length of the file, in bytes. On error this function will return
 * 0, meaning it is not possible to distinguish an invalid file from a zero
 * length file.
 */
MALI_TPI_API size_t mali_tpi_fsize(
	mali_tpi_file *stream );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @addtogroup tpi_core_stdio
 * @{
 */

/**
 * @brief Output a string to the platform-specific test log.
 *
 * This is equivalent to:
 * @code
 * mali_tpi_fputs( str, mali_tpi_stdout );
 * @endcode
 *
 * @param str      The string to output (must be non-NULL)
 *
 * @return A non-negative value on success, or a negative value on failure.
 */
MALI_TPI_API int mali_tpi_puts(
	const char *str );

/* ------------------------------------------------------------------------- */
/**
 * @brief Output a formatted string to the platform-specific test log.
 *
 * This is equivalent to:
 * @code
 * mali_tpi_fprintf(mali_tpi_stdout, format, ...);
 * @endcode
 *
 * @param format   The format to pass to @c mali_tpi_format_string
 *
 * @return The number of characters written on success, or a negative value
 * on failure.
 */
MALI_TPI_API int mali_tpi_printf(
	const char *format,
	... );

/* ------------------------------------------------------------------------- */
/**
 * @brief Output a formatted string to the platform-specific test log.
 *
 * This is equivalent to:
 * @code
 * mali_tpi_vfprintf(mali_tpi_stdout, format, ap);
 * @endcode
 *
 * @param format   The format to pass to @c mali_tpi_format_string
 * @param ap       The variadic parameters to format
 *
 * @return The number of characters written on success, or a negative value
 * on failure.
 */
MALI_TPI_API int mali_tpi_vprintf(
	const char *format,
	va_list ap );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @defgroup tpi_core_memory Memory allocation functions
 * @{
 */

/**
 * @brief Memory allocation.
 *
 * @param size     The number of bytes to allocate.
 *
 * @return A pointer to the allocated memory, or @c NULL on failure.
 */
MALI_TPI_API void *mali_tpi_alloc(
	size_t size );

/* ------------------------------------------------------------------------- */
/**
 * @brief Free memory allocated by @c mali_tpi_alloc
 *
 * @param ptr A pointer obtained from mali_tpi_alloc, or @c NULL.
 */
MALI_TPI_API void mali_tpi_free(
	void *ptr );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @defgroup tpi_core_time Time functions
 * @{
 */

/**
 * @brief Get timestamp.
 *
 * @return The elapsed time since some arbitrary point in time, in nanoseconds.
 *
 * @note Only millisecond precision is required.
 */
MALI_TPI_API mali_tpi_uint64 mali_tpi_get_time_ns( void );

/* ------------------------------------------------------------------------- */
/**
 * @brief Return status from sleep functions
 */
typedef enum {
	/** The sleep was successful */
	MALI_TPI_SLEEP_STATUS_OK,
	/** The sleep failed due to some OS-specific failure */
	MALI_TPI_SLEEP_STATUS_FAILED,
	/** The sleep was interrupted by delivery of an event */
	MALI_TPI_SLEEP_STATUS_INTERRUPTED
} mali_tpi_sleep_status;

/* ------------------------------------------------------------------------- */
/**
 * @brief Sleep for time interval.
 *
 * @param interval      Time interval, in nanoseconds
 * @param interruptible If @c MALI_TPI_FALSE, restart automatically after
 *                      interrupt
 *
 * @return The status of the sleep.
 *
 * @note Actual precision of the underlying sleep is implementation-dependent;
 * many systems only provide millisecond precision.
 */
MALI_TPI_API mali_tpi_sleep_status mali_tpi_sleep_ns(
	mali_tpi_uint64 interval,
	mali_tpi_bool interruptible );

/* ------------------------------------------------------------------------- */
/**
 * @brief Sleep until an absolute time.
 *
 * @param timestamp     The target time to sleep until.
 * @param interruptible If @c MALI_TPI_FALSE, restart automatically after
 *                      interrupt
 *
 * @return The status of the sleep.
 *
 * @note Actual precision of the underlying sleep is implementation-dependent;
 * many systems only provide millisecond precision.
 */
MALI_TPI_API mali_tpi_sleep_status mali_tpi_sleep_until_ns(
	mali_tpi_uint64 timestamp,
	mali_tpi_bool interruptible );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @defgroup tpi_core_rand Random number functions
 * @{
 */

/** The length of the Mersenne Twister state vector, in words. */
#define MALI_TPI_RNG_STATE_LEN          (624)

/* ------------------------------------------------------------------------- */
/**
 * @brief Random number generator state structure.
 */
typedef struct mali_tpi_rand_state {
	/** The index in to the state array for the next random word. */
	unsigned int index;
	/** The seed used when initializing this random data. */
	mali_tpi_uint32 seed;
	/** The number of words generated since state initialized. */
	mali_tpi_uint32 generated;
	/** The state vector. */
	mali_tpi_uint32 state[MALI_TPI_RNG_STATE_LEN];
} mali_tpi_rand_state;

/* ------------------------------------------------------------------------- */
/**
 * @brief Initialize a random number generator state for use.
 *
 * @param state         A new empty state structure to initialize.
 * @param seed          The seed value.
 * @param generated     The generated value from a previous state structure.
 *                      allowing this implementation to "skip forwards" to
 *                      the same point in time. This may take some time if
 *                      generated is high. Generated should be set to zero
 *                      if not needed.
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API void mali_tpi_rand_init(
	mali_tpi_rand_state *state,
	mali_tpi_uint32 seed,
	mali_tpi_uint32 generated );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a buffer of random data.
 *
 * @param state         The state to generate the data from.
 * @param buf           The destination buffer.
 * @param len           The length in bytes of the destination buffer.
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API void mali_tpi_rand_gen_buffer(
	mali_tpi_rand_state *state,
	void* buf,
	size_t len );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random integer between the two given values.
 *
 * @param state         The state to generate the data from.
 * @param min           The minimum value (inclusive).
 * @param max           The maximum value (inclusive).
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API mali_tpi_int8 mali_tpi_rand_gen_int8(
	mali_tpi_rand_state *state,
	mali_tpi_int8 min,
	mali_tpi_int8 max );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random integer between the two given values.
 *
 * @param state         The state to generate the data from.
 * @param min           The minimum value (inclusive).
 * @param max           The maximum value (inclusive).
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API mali_tpi_int16 mali_tpi_rand_gen_int16(
	mali_tpi_rand_state *state,
	mali_tpi_int16 min,
	mali_tpi_int16 max );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random integer between the two given values.
 *
 * @param state         The state to generate the data from.
 * @param min           The minimum value (inclusive).
 * @param max           The maximum value (inclusive).
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API mali_tpi_int32 mali_tpi_rand_gen_int32(
	mali_tpi_rand_state *state,
	mali_tpi_int32 min,
	mali_tpi_int32 max );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random integer between the two given values.
 *
 * @param state         The state to generate the data from.
 * @param min           The minimum value (inclusive).
 * @param max           The maximum value (inclusive).
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API mali_tpi_int64 mali_tpi_rand_gen_int64(
	mali_tpi_rand_state *state,
	mali_tpi_int64 min,
	mali_tpi_int64 max );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random unsigned integer between the two given values.
 *
 * @param state         The state to generate the data from.
 * @param min           The minimum value (inclusive).
 * @param max           The maximum value (inclusive).
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API mali_tpi_uint8 mali_tpi_rand_gen_uint8(
	mali_tpi_rand_state *state,
	mali_tpi_uint8 min,
	mali_tpi_uint8 max );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random unsigned integer between the two given values.
 *
 * @param state         The state to generate the data from.
 * @param min           The minimum value (inclusive).
 * @param max           The maximum value (inclusive).
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API mali_tpi_uint16 mali_tpi_rand_gen_uint16(
	mali_tpi_rand_state *state,
	mali_tpi_uint16 min,
	mali_tpi_uint16 max );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random unsigned integer between the two given values.
 *
 * @param state         The state to generate the data from.
 * @param min           The minimum value (inclusive).
 * @param max           The maximum value (inclusive).
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API mali_tpi_uint32 mali_tpi_rand_gen_uint32(
	mali_tpi_rand_state *state,
	mali_tpi_uint32 min,
	mali_tpi_uint32 max );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random unsigned integer between the two given values.
 *
 * @param state         The state to generate the data from.
 * @param min           The minimum value (inclusive).
 * @param max           The maximum value (inclusive).
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API mali_tpi_uint64 mali_tpi_rand_gen_uint64(
	mali_tpi_rand_state *state,
	mali_tpi_uint64 min,
	mali_tpi_uint64 max );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random single-precision float between 0 and 1.
 *
 * @param state         The state to generate the data from.
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API float mali_tpi_rand_gen_ufloat(
	mali_tpi_rand_state *state );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random single-precision float between -1 and 1.
 *
 * @param state         The state to generate the data from.
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API float mali_tpi_rand_gen_sfloat(
	mali_tpi_rand_state *state );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random double-precision float between 0 and 1.
 *
 * @param state         The state to generate the data from.
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API double mali_tpi_rand_gen_udouble(
	mali_tpi_rand_state *state );

/* ------------------------------------------------------------------------- */
/**
 * @brief Generate a random double-precision float between -1 and 1.
 *
 * @param state         The state to generate the data from.
 *
 * @note This function is guaranteed to be reentrant for differing @c state
 * structure, but not if the the same state structure is used concurrently.
 */
MALI_TPI_API double mali_tpi_rand_gen_sdouble(
	mali_tpi_rand_state *state );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @defgroup tpi_core_thread Threading functions
 * @{
 */

/* ------------------------------------------------------------------------- */
/**
 * @typedef mali_tpi_thread
 * This type represents a thread of excecution in the system.
 */

/* ------------------------------------------------------------------------- */
/**
 * @typedef mali_tpi_mutex
 * This type represents a mutex in the system.
 */

/**
 * @brief Entry point function signature for a thread.
 *
 * @param arg           Pointer to an object passed to thread entry point
 */
typedef void* (*mali_tpi_threadproc)(void *arg);

/* ------------------------------------------------------------------------- */
/**
 * @brief Create a thread.
 *
 * A new thread is created executing the function \a threadproc with \a
 * start_param passed in as the sole argument. The thread finishes when \a
 * threadproc returns.
 *
 * Every thread created by mali_tpi_thread_create() must be waited upon exactly
 * once, to fully release the resources associated with it. To do otherwise
 * may cause a leak, even if you can determine by other means that the thread
 * has finished. Refer to mali_tpi_thread_wait() for more information.
 *
 * @param thread pointer To storage for the created thread object
 * @param threadproc     Entry point of the thread
 * @param start_param    Pointer to object passed as parameter to \a threadproc
 *
 * It is a programming error to pass an invalid pointer (including \c NULL)
 * through the \a thread or \a threadproc parameters.
 *
 * @return MALI_TPI_TRUE if the thread was created successfully
 * @return MALI_TPI_FALSE if the thread could not be created
 */
MALI_TPI_API mali_tpi_bool mali_tpi_thread_create(
	mali_tpi_thread *thread,
	mali_tpi_threadproc threadproc,
	void *start_param );

/* ------------------------------------------------------------------------- */
/**
 * @brief Wait for a thread to finish.
 *
 * The calling thread is blocked waiting on \a thread to finish. If \a thread
 * refers to a thread that has already finished, the function returns
 * immediately.
 *
 * Every thread created by mali_tpi_thread_create() must be waited upon exactly
 * once, to fully release the resources associated with it. To do otherwise
 * will cause a leak, even if you can determine by other means that the thread
 * has finished. For example, calling mali_tpi_thread_create() in a loop whilst
 * waiting for each thread to exit by sleeping will cause a memory leak,
 * because mali_tpi_thread_wait() has not been used to notify the OS that we no
 * longer require any resources associated with it.
 *
 * It is a programming error for a thread to wait upon itself.
 *
 * It is a programming error to pass an invalid pointer (including \c NULL)
 * through the \a thread parameter.
 *
 * Note that it is illegal for more than one thread to call
 * mali_tpi_thread_wait() on the same @ref mali_tpi_thread value. That is, each
 * mali_tpi_thread must be waited upon by exactly one other thread (the main
 * thread cannot be waited upon).
 *
 * @param thread        pointer to a thread object
 * @param exitcode      pointer to storage for the exit code of the thread.
 *                      This pointer may be NULL if there is no interest in the
 *                      exit code.
 */
MALI_TPI_API void mali_tpi_thread_wait(
	mali_tpi_thread *thread,
	void **exitcode );

/* ------------------------------------------------------------------------- */
/**
 * @brief Initialize a mutex.
 *
 * Initialize a mutex structure. If the function returns successfully, the
 * mutex is in the unlocked state.
 *
 * If the OS-specific  mutex referenced from the structure cannot be
 * initialized, an error is returned.
 *
 * The mutex must be terminated when no longer required, by using
 * mali_tpi_mutex_term(). Otherwise, a resource leak may result in the OS.
 *
 * It is a programming error to pass an invalid pointer (including @c NULL)
 * through the @a mutex parameter.
 *
 * It is a programming error to attempt to initialize a mutex that is
 * currently initialized.
 *
 * @param mutex         pointer to the mutex structure

 * @return MALI_TPI_TRUE if the mutex was initialized successfully
 * @return MALI_TPI_FALSE if the mutex could not be initialized
 */
MALI_TPI_API mali_tpi_bool mali_tpi_mutex_init(
	mali_tpi_mutex *mutex );

/* ------------------------------------------------------------------------- */
/**
 * @brief Terminate a mutex.
 *
 * Terminate the mutex pointed to by @a mutex, which must be
 * a pointer to a valid unlocked mutex. When the mutex is terminated, the
 * OS-specific mutex is freed.
 *
 * It is a programming error to pass an invalid pointer (including @c NULL)
 * through the @a mutex parameter.
 *
 * It is a programming error to attempt to terminate a mutex that is currently
 * terminated, or a mutex that is currently locked.
 *
 * @param mutex         pointer to the mutex structure
 */
MALI_TPI_API void mali_tpi_mutex_term(
	mali_tpi_mutex *mutex );

/* ------------------------------------------------------------------------- */
/**
 * @brief Lock a mutex.
 *
 * Lock the mutex pointed to by @a mutex. If the mutex is currently unlocked,
 * the calling thread returns with the mutex locked. If a second thread
 * attempts to lock the same mutex, it blocks until the first thread
 * unlocks the mutex. If two or more threads are blocked waiting on the first
 * thread to unlock the mutex, it is undefined as to which thread is unblocked
 * when the first thread unlocks the mutex.
 *
 * It is a programming error to pass an invalid pointer (including @c NULL)
 * through the @a mutex parameter.
 *
 * It is a programming error to exit a thread while it has a locked mutex.
 * It is a programming error call lock on a mutex that is currently
 * locked by the caller thread; this will deadlock.
 *
 * @param mutex pointer to the mutex
 */
MALI_TPI_API void mali_tpi_mutex_lock(
	mali_tpi_mutex *mutex );

/* ------------------------------------------------------------------------- */
/**
 * @brief Unlock a mutex.
 *
 * Unlock the mutex pointed to by @a mutex. The calling thread must be the
 * same thread that locked the mutex. If no other threads are waiting on the
 * mutex to be unlocked, the function returns immediately, with the mutex
 * unlocked. If one or more threads are waiting on the mutex to be unlocked,
 * then this function returns, and a thread waiting on the mutex can be
 * unblocked. It is undefined as to which thread is unblocked.
 *
 * @note It is not defined @em when a waiting thread is unblocked. For example,
 * a thread calling mali_tpi_mutex_unlock() followed by osu_mutex_lock()
 * may (or may not) obtain the lock again, preventing other threads from being
 * released.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the @a mutex parameter.
 *
 * It is a programming error to call osu_mutex_unlock() on a mutex which the
 * calling thread has not locked. That is, it is illegal for any thread other
 * than the 'owner' of the mutex to unlock it. In addition, you must not unlock
 * an already unlocked mutex.
 *
 * @param mutex pointer to the mutex
 */
MALI_TPI_API void mali_tpi_mutex_unlock(
	mali_tpi_mutex *mutex );

/* ------------------------------------------------------------------------- */
/**
 * @brief Initializes a barrier.
 *
 * The barrier is used to synchronize the execution of multiple threads.
 *
 * @param barrier 			The point to the barrier storage.
 * @param number_of_threads Number of threads that need to wait at the barrier before it releases.
 *
 * @return MALI_TPI_TRUE if the barrier was initialized successfully
 * @return MALI_TPI_FALSE if the barrier could not be initialized
 */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_init(
		mali_tpi_barrier *barrier,
		u32 number_of_threads);

/* ------------------------------------------------------------------------- */
/**
 * @brief Destroys a barrier.
 *
 * The barrier is used to synchronize the execution of multiple threads.
 *
 * @param barrier that will be destroyed.  The barrier must have been created by mali_tpi_barrier_init(...)
 *
 * @return MALI_TPI_TRUE if the barrier was destroyed successfully
 * @return MALI_TPI_FALSE if the barrier could not be destroyed
 */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_destroy( mali_tpi_barrier *barrier );

/* ------------------------------------------------------------------------- */
/**
 * @brief Barrier sync.
 *
 * A thread calling this function will wait at a barrier until some number of
 * other threads are waiting. The barrier is used to synchronize the execution
 * of multiple threads.
 *
 * @param barrier at which the thread will wait. The barrier must have been created by mali_tpi_barrier_init(...)
 *
 * @return MALI_TPI_TRUE Upon successful completion of wait call
 * @return MALI_TPI_FALSE Upon unsuccessful completion of wait call
 */
MALI_TPI_API mali_tpi_bool mali_tpi_barrier_wait( mali_tpi_barrier *barrier );

/* ------------------------------------------------------------------------- */
/**
 * @}
 * @defgroup tpi_core_atomic Atomic Access
 *
 * @anchor tpiatomic_important
 * @par Important Information on Atomic variables
 *
 * Atomic variables are objects that can be modified by only one thread at a time.
 * For use in SMP systems, strongly ordered access is enforced using memory
 * barriers.
 *
 * An atomic variable implements an unsigned integer counter which is exactly
 * 32 bits long. Arithmetic on it is the same as on u32 values, which is the
 * arithmetic of integers modulo 2^32. For example, incrementing past
 * 0xFFFFFFFF rolls over to 0, decrementing past 0 rolls over to
 * 0xFFFFFFFF. That is, overflow is a well defined condition (unlike signed
 * integer arithmetic in C).
 *
 * @par Thread Safety
 *
 * All User-side TPI Atomic Access functions are thread safe:
 * - Any Atomic Access function may be called from one thread whilst any
 * Atomic Access function (regardless of if it's the same or different one)
 * is called from any other thread without causing an error. The result of the
 * function will be the same as a single thread calling the function with the
 * same parameters.
 * @{
 */

/** @brief Subtract a value from an atomic variable and return the new value.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a atom parameter.
 *
 * @note Please refer to @ref tpiatomic_important Important Information on Atomic
 * variables.
 *
 * @param atom pointer to an atomic variable
 * @param value value to subtract from \a atom
 * @return value of atomic variable after \a value has been subtracted from it.
 */
static INLINE u32 mali_tpi_atomic_sub( mali_tpi_atomic *atom, u32 value );

/** @brief Add a value to an atomic variable and return the new value.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a atom parameter.
 *
 * @note Please refer to @ref tpiatomic_important Important Information on Atomic
 * variables.
 *
 * @param atom pointer to an atomic variable
 * @param value value to add to \a atom
 * @return value of atomic variable after \a value has been added to it.
 */
static INLINE u32 mali_tpi_atomic_add( mali_tpi_atomic *atom, u32 value );

/** @brief Decrement an atomic variable and return its decremented value.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a atom parameter.
 *
 * @note This function is provided as a convenience wrapper of
 * tpi_atomic_sub(). It does not need to be ported.
 *
 * @note Please refer to @ref tpiatomic_important Important Information on Atomic
 * variables.
 *
 * @param atom pointer to an atomic variable
 * @return decremented value of atomic variable
 */
static INLINE u32 mali_tpi_atomic_dec( mali_tpi_atomic *atom )
{
	return mali_tpi_atomic_sub( atom, 1 );
}

/** @brief Increment an atomic variable and return its incremented value.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a atom parameter.
 *
 * @note This function is provided as a convenience wrapper of
 * tpi_atomic_add(). It does not need to be ported.
 *
 * @note Please refer to @ref tpiatomic_important Important Information on Atomic
 * variables.
 *
 * @param atom pointer to an atomic variable
 * @return incremented value of atomic variable
 */
static INLINE u32 mali_tpi_atomic_inc( mali_tpi_atomic *atom )
{
	return mali_tpi_atomic_add( atom, 1 );
}

/** @brief Initialize the value of an atomic variable.
 *
 * This function allows an atomic variable to be initialized. After
 * initialization, you should not use this when multiple threads could access
 * the atomic variable. Instead, use tpi_atomic_compare_and_swap().
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a atom parameter.
 *
 * @note Please refer to @ref tpiatomic_important Important Information on Atomic
 * variables.
 *
 * @param atom pointer to an atomic variable
 * @param val the value to set
 */
static INLINE void mali_tpi_atomic_set( mali_tpi_atomic *atom, u32 val );

/** @brief Return the value of an atomic variable.
 *
 * This function is intended to be used in conjunction with
 * tpi_atomic_compare_and_swap() to implement read-modify-write of atomic
 * variables.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a atom parameter.
 *
 * @note Please refer to @ref tpiatomic_important Important Information on Atomic
 * variables.
 *
 * @param atom pointer to an atomic variable
 * @return value of the atomic variable
 */
static INLINE u32 mali_tpi_atomic_get( const mali_tpi_atomic *atom );

/** @brief Compare the value of an atomic variable, and atomically exchange it
 * if the comparison succeeds.
 *
 * This function implements the Atomic Compare-And-Swap operation (CAS). It
 * atomically does the following: compare \a atom with \a old_value and sets \a
 * atom to \a new_value if the comparison was true.
 *
 * Regardless of the outcome of the comparison, the initial value of \a atom is
 * returned - hence the reason for this being a 'swap' operation. If the value
 * returned is equal to \a old_value, then the atomic operation succeeded. Any
 * other value shows that the atomic operation failed, and should be repeated
 * based upon the returned value.
 *
 * This function is intended to be used in conjunction with
 * tpi_atomic_get() to implement read-modify-write of atomic
 * variables.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a atom parameter.
 *
 * @note Please refer to @ref tpiatomic_important Important Information on Atomic
 * variables.
 *
 * @param atom pointer to an atomic variable
 * @param old_value The value to make the comparison with \a atom
 * @param new_value The value to atomically write to atom, depending on whether
 * the comparison succeeded.
 * @return The \em initial value of \a atom, before the operation commenced.
 */
static INLINE u32 mali_tpi_atomic_compare_and_swap( mali_tpi_atomic *atom, u32 old_value, u32 new_value );

/** @} end group tpi_core_atomic */
/* ------------------------------------------------------------------------- */

/**
 * @defgroup tpi_core_sem Semaphores API
 * @{
 */

/**
 * @brief Type for semaphore structure.
 * This structure has public visibility to allow TPI clients to embed it in
 * their own data structures. It is padded so that it is the same size
 * regardless of the target OS. This allows clients to layout structures that
 * contain TPI primitives in such a way that they are D-Cacheline efficient, on
 * all target OSs.
 *
 * You must not attempt to make your own copies of this structure, nor should
 * you attempt to move its location.
 */
typedef union mali_tpi_sem
{
	tpi_priv_sem sem;
	char padding[TPI_PRIV_PLATFORM_SEM_STRUCT_PADDING];
} mali_tpi_sem;

/** @brief Initialize a semaphore.
 *
 * Initialize a semaphore structure with an initial value of \a value.
 * The client must allocate the memory for the @ref tpi_core_sem
 * structure, which is then populated within this function. If the OS-specific
 * semaphore referenced from the structure cannot be initialized, an error is
 * returned.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a sem parameter.
 *
 * @illegal It is illegal to provide an initial \a value to mali_tpi_sem_init() that
 * exceeds S32_MAX.
 *
 * @param sem pointer to a valid semaphore structure
 * @param value Initial value of the semaphore
 * @return MALI_TPI_TRUE on success
 * @return a value not equal to MALI_TPI_TRUE if the OS-specific semaphore cannot
 * be initialized
 */
MALI_TPI_API mali_tpi_bool mali_tpi_sem_init(mali_tpi_sem *sem, u32 value);

/**
 * @brief Terminate a semaphore.
 *
 * Terminate the semaphore pointed to by \a sem. The OS-specific semaphore
 * referenced from the tpi_core_sem structure is terminated.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a sem parameter.
 *
 * @illegal It is illegal to call mali_tpi_sem_term() on a semaphore that has
 * threads blocked on it.
 *
 * @param sem pointer to the semaphore
 */
MALI_TPI_API void mali_tpi_sem_term(mali_tpi_sem *sem);

/**
 * @brief Block if a semaphore is not signalled.
 *
 * Any thread, and any number of threads can wait on a semaphore to be
 * signalled.
 *
 * If the value of the semaphore pointed to by \a sem is zero, the
 * calling thread blocks if \a timeout_nsec is zero. If the value of the semaphore
 * is non-zero, it is decremented by one and the function returns immediately.
 *
 * When \a timeout_nsec is non-zero and the semaphore is currently non-signalled,
 * then a thread attempting to wait on the semaphore will wait for \a timeout_nsec
 * nanoseconds for the semaphore to become signalled. If the semaphore is
 * signalled before the timeout is exceeded, then the function returns
 * immediately (without waiting for the timeout to be exceeded). If the
 * semaphore was not signalled within the timeout period, then at some point
 * after this time interval, the function will return, whether the semaphore
 * was signalled or not. If the semaphore was not in the signalled state when
 * the function returned, then this is indicated in the error code return
 * value.
 *
 * @note Since not all systems support nanosecond granularity timeouts, the
 * timeout may be internally rounded-up to the system's
 * synchronization-primitive timeout precision. Therefore, this function may
 * wait for longer than expected, but it is guarenteed to wait for a finite
 * time.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a sem parameter.
 *
 * @param sem pointer to the semaphore
 * @param timeout_nsec timeout in nanoseconds to wait for
 * @return if \a timeout_nsec == 0, then MALI_TPI_TRUE is always returned.
 * @return if \a timeout_nsec != 0, then MALI_TPI_TRUE indicates that the semaphore
 * was signalled within the timeout period. Otherwise, a value not equal to
 * MALI_TPI_TRUE indicates that the timeout expired, and the semaphore was not
 * signalled.
 */
MALI_TPI_API mali_tpi_bool mali_tpi_sem_wait(mali_tpi_sem *sem, u64 timeout_nsec);

/**
 * @brief Signal a semaphore
 *
 * If one or more threads are blocked waiting on the semaphore object, one
 * thread is unblocked and allowed to return from its call to
 * mali_tpi_sem_wait(). It is not defined which thread is unblocked
 * when more than one thread is waiting.
 *
 * @note It is not defined \em when a waiting thread is unblocked. For example,
 * a thread calling mali_tpi_sem_post() followed by mali_tpi_sem_wait() may (or may not)
 * consume the signal, preventing other threads from being released. Neither
 * the 'immediately releasing', nor the 'delayed releasing' behavior of
 * mali_tpi_sem_post() can be relied upon. If such behavior is required, then you
 * must implement it yourself, such as by using a second synchronization
 * primitve.
 *
 * If no threads are waiting, the value of the semaphore
 * object is incremented by one and the function returns immediately.
 *
 * Any thread in the current process can signal a semaphore.
 *
 * It is a programming error to pass an invalid pointer (including NULL)
 * through the \a sem parameter.
 *
 * @illegal It is illegal to call mali_tpi_sem_post() on a semaphore in such a way
 * that would cause its value to exceed S32_MAX.
 *
 * @param sem pointer to the semaphore object
 */
MALI_TPI_API void mali_tpi_sem_post(mali_tpi_sem *sem);

/** @} end group tpi_core_sem */

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* End (_MALI_TPI_INTERNAL_H_) */
