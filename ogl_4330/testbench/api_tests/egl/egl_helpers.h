/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef EGL_HELPERS_H
#define EGL_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(EXCLUDE_EGL_SPECIFICS) && !defined(EXCLUDE_UNIT_FRAMEWORK)
#include "egl_helpers_pixmap.h"
#endif /* !defined(EXCLUDE_EGL_SPECIFICS) && !defined(EXCLUDE_UNIT_FRAMEWORK) */


#ifndef EXCLUDE_UNIT_FRAMEWORK
# include "suite.h"
#else /* EXCLUDE_UNIT_FRAMEWORK */
/** Fake the 'suite' with an opaque struct */
typedef struct suite suite;
#endif /* EXCLUDE_UNIT_FRAMEWORK */

#ifndef EXCLUDE_EGL_SPECIFICS
# include "egl_framework_actual.h"
#endif /* EXCLUDE_EGL_SPECIFICS */
#include "base/mali_types.h"
#include <stdio.h>

#include "../helpers/egl_image_tools.h"

#include <mali_tpi.h>

#define EGL_HELPER_ASSERT_MSG(expr, X)	TPI_ASSERT_MSG(expr, #X)
#define EGL_HELPER_ASSERT_POINTER(expr)	TPI_ASSERT_POINTER(expr)

#define EGL_HELPER_DEBUG_PRINT(nr, X )  do { if ( nr<=MALI_DEBUG_LEVEL ) { mali_tpi_printf X ; } } while (0)

/* These are currently only supported for the unit framework */
#ifndef EXCLUDE_UNIT_FRAMEWORK
#ifndef EXCLUDE_EGL_SPECIFICS

#define ASSERT_EGL_ERROR(input, expected, str)                          \
	do { \
		EGLint error = (input); \
		assert_fail((error) == (expected), dsprintf(test_suite, "%s : %s is %s (0x%X), expected %s (0x%X) (%s:%d)", str, #input, get_eglenum_name(error), error, get_eglenum_name(expected), expected, __FILE__, __LINE__)); \
	} while(0);

#ifdef EGL_MALI_VG
#define ASSERT_VG_ERROR(input, expected, str)                           \
	do { \
		VGint error = (input); \
		assert_fail((error) == (expected), dsprintf(test_suite, "%s : %s is %s (0x%X), expected %s (0x%X) (%s:%d)", str, #input, get_vgenum_name(error), error, get_vgenum_name(expected), expected, __FILE__, __LINE__)); \
	} while(0);
#endif

#ifdef EGL_MALI_GLES
#define ASSERT_GLES_ERROR(input, expected, str)                         \
	do { \
		GLenum error = (input); \
		assert_fail((error) == (expected), dsprintf(test_suite, "%s : %s is %s (0x%X), expected %s (0x%X) (%s:%d)", str, #input, get_glesenum_name(error), error, get_glesenum_name(expected), expected, __FILE__, __LINE__)); \
	} while(0);
#endif



#define ASSERT_EGL_EQUAL(input, expected, str) assert_fail((input) == (expected), dsprintf(test_suite, "%s (%s:%d)", str, __FILE__, __LINE__));
#define ASSERT_EGL_NOT_EQUAL(input, expected, str) assert_fail((input) != (expected), dsprintf(test_suite, "%s (%s:%d)", str, __FILE__, __LINE__));

#define MALI_EGL_NULL_WINDOW (EGLNativeWindowType)NULL

/* EGL_NATIVE_VISUAL_ID missing in eglChooseConfig */
static const EGLint config_allowed_attributes[] = { EGL_BUFFER_SIZE, EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_ALPHA_SIZE, EGL_LUMINANCE_SIZE,
                                                    EGL_ALPHA_MASK_SIZE, EGL_BIND_TO_TEXTURE_RGB, EGL_BIND_TO_TEXTURE_RGBA, EGL_COLOR_BUFFER_TYPE, EGL_CONFIG_CAVEAT, EGL_CONFIG_ID,
                                                    EGL_CONFORMANT, EGL_DEPTH_SIZE, EGL_LEVEL, EGL_MAX_PBUFFER_WIDTH, EGL_MAX_PBUFFER_HEIGHT, EGL_MAX_PBUFFER_PIXELS, EGL_MAX_SWAP_INTERVAL,
                                                    EGL_MIN_SWAP_INTERVAL, EGL_NATIVE_RENDERABLE, EGL_NATIVE_VISUAL_ID, EGL_NATIVE_VISUAL_TYPE, EGL_RENDERABLE_TYPE, EGL_SAMPLE_BUFFERS,
                                                    EGL_SAMPLES, EGL_STENCIL_SIZE, EGL_SURFACE_TYPE, EGL_TRANSPARENT_TYPE, EGL_TRANSPARENT_RED_VALUE, EGL_TRANSPARENT_GREEN_VALUE,
                                                    EGL_TRANSPARENT_BLUE_VALUE };

/* Corresponding valid values to the above list (for use in eglChooseConfig testing) */
static const EGLint config_allowed_attribute_values[] = {
	1, 1, 1, 1, 1, 1,
	1, EGL_TRUE, EGL_TRUE, EGL_RGB_BUFFER, EGL_NONE, 1,
	EGL_TRUE, 1, 0, 1, 1, 1, 1,
	1, EGL_TRUE, /*EGL_NATIVE_VISUAL_ID*/0, /*EGL_NATIVE_VISUAL_TYPE*/0, EGL_OPENVG_BIT, 1,
	1, 1, EGL_WINDOW_BIT, EGL_NONE, 0, 0,
	0 };

static const EGLint surface_allowed_attributes[] = { EGL_VG_ALPHA_FORMAT, EGL_VG_COLORSPACE, EGL_CONFIG_ID, EGL_HEIGHT,
                                                     EGL_HORIZONTAL_RESOLUTION, EGL_LARGEST_PBUFFER, EGL_MIPMAP_TEXTURE,
                                                     EGL_MIPMAP_LEVEL, EGL_PIXEL_ASPECT_RATIO,
                                                     EGL_RENDER_BUFFER, EGL_SWAP_BEHAVIOR, EGL_TEXTURE_FORMAT,
                                                     EGL_TEXTURE_TARGET, EGL_VERTICAL_RESOLUTION, EGL_WIDTH };

const char *get_eglenum_name(EGLenum e);
const char *get_vgenum_name(EGLenum e);
#ifdef EGL_MALI_GLES
const char *get_glesenum_name(GLenum e);
#endif

unsigned long long egl_helper_get_ms(void);

EGLConfig egl_helper_get_config_exact( suite *test_suite, EGLDisplay dpy, EGLint renderable_type, EGLint surface_type, EGLint red_size, EGLint green_size, EGLint blue_size, EGLint alpha_size, EGLNativePixmapType* pixmap_match );
EGLConfig egl_helper_get_dualconfig( suite *test_suite, EGLDisplay dpy, EGLint red_size, EGLint green_size, EGLint blue_size, EGLint alpha_size );
EGLConfig egl_helper_get_specific_dualconfig( suite *test_suite, EGLDisplay dpy, EGLint renderable_type, EGLint red_size, EGLint green_size, EGLint blue_size, EGLint alpha_size );
void egl_helper_query_check_surface_value( suite *test_suite, EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value, EGLint wanted_value );

/* Test dumping current framebuffer to ppm */
#if defined(ANDROID) && !defined(EXCLUDE_EGL_SPECIFICS)
#define EGL_HELPER_TEST_FBDEV_DUMP 0
EGLBoolean egl_helper_fbdev_test( egl_helper_windowing_state *state );
#endif

/**
 * Helpers for general display creation.
 */

/**
 * @brief Get the windowing state from the default fixture of a gles1 test suite.
 * There is a bug 10680 to investigate why this is needed.
 * @param[in] test_suite the suite containing the fixture to get the windowing state from
 * @return a pointer to the windowing state of the default fixture of the given test suite.
 */
egl_helper_windowing_state* egl1_helper_get_windowing_state(suite *test_suite);

/**
 * @brief Get the windowing state from the default fixture of a test suite.
 * @param[in] test_suite the suite containing the fixture to get the windowing state from
 * @return a pointer to the windowing state of the default fixture of the given test suite.
 */
egl_helper_windowing_state* egl_helper_get_windowing_state(suite *test_suite);

/** Macro for obtaining the windowing state from the default fixture, assuming that test_suite is present */
#define EGL_FRAMEWORK_GET_WINDOWING_STATE() egl_helper_get_windowing_state(test_suite)

/**
 * @brief prepare a display
 *
 * OS Independant code for making boilerplate EGL and egl_helper calls to
 * prepare a display for use in further EGL calls, including error checking.
 *
 * If a non-boilerplate sequence of calls must be made to eglGetDisplay,
 * eglInitialize, eglTerminate, then use the egl_helper_get_native_display and
 * egl_helper_release_native_display abstraction to obtain and release the
 * native display.
 *
 * @param[in] test_suite the suite to make allocations/log errors on
 * @param[out] display an EGLDisplay for use in further EGL calls.
 * @param[in] initialize EGL_TRUE if the display should also be initialized,
 * EGL_FALSE otherwise.
 * @param[out] retNativeDpy pointer to storage for a native display, which must
 * be terminated correctly with a call to egl_helper_terminate_display.
 * @return EGL_TRUE on success - display can be used in further EGL calls, and
 * retNativeDpy can be passed to egl_helper_terminate_display.
 * EGL_FALSE on failure, *display is set to EGL_NO_DISPLAY and *retNativeDpy
 * will not be modified.
 */
EGLBoolean egl_helper_prepare_display( suite *test_suite, EGLDisplay *display, EGLBoolean initialize, EGLNativeDisplayType *retNativeDpy );

/**
 * @brief terminate a display
 *
 * OS Independant code for making boilerplate EGL and egl_helper calls to
 * terminate a display when it is no longer required, including error checking.
 *
 * If a non-boilerplate sequence of calls must be made to eglGetDisplay,
 * eglInitialize, eglTerminate, then use the egl_helper_get_native_display and
 * egl_helper_release_native_display abstraction to obtain and release the
 * native display.
 *
 * @param[in] test_suite the suite to make allocations/log errors on
 * @param[in] display the EGLDisplay to terminate.
 * @param[in,out] retNativeDpy pointer to a native display returned from
 * egl_helper_prepare_display. Trashed on exit.
 */
void egl_helper_terminate_display( suite *test_suite, EGLDisplay dpy, EGLNativeDisplayType *inoutNativeDpy );

#endif /* EXCLUDE_EGL_SPECIFICS */

/**
 * Process and threading abstraction
 */

typedef int pipe_t[2];

/**
 * Platform-independant thread procedure type
 */
typedef u32 (egl_helper_threadproc)(void *);

/**
 * @brief Create a new process and continues, caller needs to exit the child process
 * @return 0 to the child process, valid child pid (>0) to the parent process, negative (<0) on failure
 */
int egl_helper_create_process_simple( void );

/**
 * @brief exits from the current process
 */
void egl_helper_exit_process( void );

/**
 * Fork new processes and initialize an array of pipes that can be used for
 * communication.
 *
 * @param pipefds,  an allocated array with the length equal to world_size.
 * @param world_size,  number of processes (including the mother process).
 * @return -1 on errror, a unique rank (>=0 && <world_size) on success.
 */
int egl_helper_create_coprocesses(pipe_t *pipefds, int world_size);

/**
 * Signal mother and shutdown
 *
 * Signal the pipe specified by signal_rank with 0xff, then close all pipes and
 * exit the current process with code 0.
 *
 * @param pipefds,  an allocated array with the length equal to world_size
 *                  containing allready initialized pipes
 * @param world_size,  number of processes (including the mother process)
 * @param signal_rank, index for the pipe to write to
 */
void egl_helper_exit_coprocess(pipe_t *pipefds, int world_size, int signal_rank);

/**
 * Wait for Children to terminate
 *
 * Waits for 0xff to be signaled world_size-1 times to the pipe specified by
 * rank.  If the shoutdown signal is exedentaly signaled to pipefds[rank], this
 * function might return early.
 *
 * @param pipefds,  an allocated array with the length equal to world_size
 *                  containing allready initialized pipes
 * @param world_size,  number of processes (including the mother process)
 * @param rank,  index for the pipe to read from
 */
void egl_helper_wait_coprocesses(pipe_t *pipefds, int world_size, int rank);

/**
 * wraps a write to pipefd[1]
 *
 * @param buf, send buffer
 * @param length, bytes to send
 * @return bytes sent
 */
size_t egl_helper_pipe_send(pipe_t pipefd, void *buf, size_t length);

/**
 * wraps a read from pipefd[0]
 *
 * @param buf, read buffer
 * @param length, bytes to read
 * @return bytes read
 */
size_t egl_helper_pipe_recv(pipe_t pipefd, void *buf, size_t length);


/**
 * @brief Create a message queue ( or mailbox) to share information with other process
 * @param key , key/type for the message queue
 * @param flags, control bits for creating the message queue
 * @return message queue identifier (non negetive), -1 on failure
 */
int egl_helper_create_msgq( int key, int flags);

/**
 * @brief Send a message to the given queue
 * @param mesq_id, id/name of the message queue to which message needs be sent
 * @param from, pointer of the data to be sent as message.
 * @param size, size of the data to be sent as message.
 * @param flags, control flags
 * @return 0 on success, -1 on failure.
 */
int egl_helper_msgsnd(int msgq_id, const void *from, u32 size, int flags);

/**
 * @brief Receive a message
 * @param msgq_id, id/name of the message queue from which message needs to be received
 * @param to, pointer to which message needs to be copied
 * @param size, size of the message copied into "to" pointer
 * @param msg_id, if supported, then id of the message that needs to be received
 * @param flags, control flags
 * @return number of bytes received in the message
 */
u32 egl_helper_msgrcv(int msgq_id, void *to, u32 size, long msg_id, int flags);

/**
 * @brief Create a new process asynchronously
 * @param test_suite test_suite for making mem pool allocations
 * @param executable the executable to start
 * @param args arguments to the executable
 * @param log_basename basename of the logfile to use. Will append
 * '-stdout.txt' and '-stderr.txt' for STDOUT/STDERR respectively
 * @return process handle. Zero indicates failure
 */
u32 egl_helper_create_process( suite *test_suite, char* executable, char* args, char* log_basename );

/**
 * @brief Wait for a process to finish
 * @param test_suite test_suite for making mem pool allocations
 * @param proc_handle process handle to wait for.
 * @return exit code of the process, or a non-zero value if there was a fault.
 */
int egl_helper_wait_for_process( suite *test_suite, u32 proc_handle );

/**
 * @brief Create a new thread
 * @param test_suite test_suite for making mem pool allocations
 * @param threadproc pointer to the thread procedure to start
 * @param start_param parameter to pass to the thread procedure
 * @return thread handle. Zero indicates failure
 * @note the main() thread must call egl_helper_wait_for_thread() on each thread before it terminates.
 */
u32 egl_helper_create_thread( suite *test_suite, egl_helper_threadproc *threadproc, void *start_param );

/**
 * @brief Create a new thread
 * @param test_suite test_suite for making mem pool allocations
 * @param thread_handle thread to wait on
 * @return exitcode of the thread
 * @note the main() thread must call egl_helper_wait_for_thread() on each thread before it terminates.
 */
u32 egl_helper_wait_for_thread( suite *test_suite, u32 thread_handle );

/**
 * @brief Create a new mutex
 * @param test_suite test_suite for making mem pool allocations
 * @return pointer to the mutex
 */
u32 * egl_helper_mutex_create( suite *test_suite );

/**
 * @brief Destroys the mutex
 * @param test_suite test_suite for making mem pool allocations�
 * @param mutex to be destsroyed
 * @return void
 */
void egl_helper_mutex_destroy( suite *test_suite, u32 * mutex );

/**
 * @brief Locks the mutex
 * @param test_suite test_suite for making mem pool allocations�
 * @param mutex to be locked
 * @return void
 */
void egl_helper_mutex_lock( suite *test_suite, u32 * mutex );

/**
 * @brief Unlocks the mutex if it is locked
 * @param test_suite test_suite for making mem pool allocations�
 * @param mutex to be unlocked
 * @return void
 */
void egl_helper_mutex_unlock(  suite *test_suite, u32 * mutex );

/**
 * Initialize a barrier.  The barrier is used to synchronize the execution of multiple threads.
 * @param test_suite test_suite for making mem pool allocations�
 * @param number_of_threads that need to wait at the barrier before it releases
 * @return thread handle. Zero indicates failure
 */
u32* egl_helper_barrier_init(suite *test_suite, u32 number_of_threads );

/**
 * Destroy a barrier.  The barrier is used to synchronize the execution of multiple threads.
 * @param test_suite test_suite for making mem pool allocations
 * @param barrier that will be destroyed.  The barrier must have been created by egl_helper_barrier_init(...)
 * @return void
 */
void egl_helper_barrier_destroy( suite *test_suite, u32 * barrier );

/**
 * A thread calling this funtion will wait at a barrier until some number of
 * other threads are waiting.  The barrier is used to synchronize the execution
 * of multiple threads.
 * @param test_suite test_suite for making mem pool allocations
 * @param barrier at which the thread will wait.  The barrier must have been created by egl_helper_barrier_init(...)
 * @return void
 */
void egl_helper_barrier_wait( suite *test_suite, u32*barrier );

#endif /* EXCLUDE_UNIT_FRAMEWORK */

#ifndef EXCLUDE_EGL_SPECIFICS
/**
 * Display abstraction
 */

/**
 * Flags to use on display abstraction functions
 */
typedef enum egl_helper_dpy_flags
{
	EGL_HELPER_DPY_FLAG_INVALID_DPY = 0x1 /**< Return an invalid native display */
} egl_helper_dpy_flags;

/**
 * @brief Get a native display
 *
 * Makes all the necessary OS calls to get a native display. This function is
 * OS-specific.
 *
 * @param[in] test_suite the suite to make allocations/log errors on
 * @param[out] ret_native_display a pointer to storage for a native display.
 * @param[in] flags EGL_HELPER_DPY_FLAG_INVALID_DPY causes an invalid display to be requested
 * @return EGL_TRUE on success and *ret_native_dpy is the requested display.
 * EGL_FALSE indicates failure.
 */
EGLBoolean egl_helper_get_native_display( suite *test_suite, EGLNativeDisplayType *ret_native_dpy, egl_helper_dpy_flags flags );

/**
 * @brief Release a native display
 *
 * Makes all the necessary OS calls to release a native display. This function
 * is OS-specific.
 *
 * The native display will be modified to release it. Use of it in subsequent
 * calls to eglGetDisplay() may or may not produce a correct result - or, the
 * result that was intended.
 *
 * @param[in] test_suite the suite to make allocations/log errors on
 * @param[in,out] inout_native_display a pointer to a native display to be
 * released, which will be modified (trashed).
 */
void egl_helper_release_native_display( suite *test_suite, EGLNativeDisplayType *inout_native_dpy );



/**
 * Windowing abstraction
 */

/**
 * @brief Allocate and initialize the windowing abstraction
 * @param state pointer to windowing state pointer to allocate and initialize
 * @param test_suite suite to access for windowing calls on this state e.g.
 * mempool allocs and test results.
 * @return EGL_TRUE on success, EGL_FALSE on failure
 * @note on failure, *state is unmodified
 * @note when EXCLUDE_UNIT_FRAMEWORK is defined, pass in NULL for test_suite.
 * @note *state is guaranteed to be Non-NULL on success.
 */
EGLBoolean egl_helper_windowing_init( egl_helper_windowing_state **state, suite *test_suite );

/**
 * @brief De-Initialize and free the windowing abstraction
 * @param state pointer to windowing state pointer to deinitialize and free
 * @return EGL_TRUE on succes, EGL_FALSE on failure
 * @note Do Not call De-Initialize if the state is un-initialized (either
 * through not calling init successfully, or through a successful deinit).
 */
EGLBoolean egl_helper_windowing_deinit( egl_helper_windowing_state **state );

/**
 * @brief Set the 'quadrant' for windows
 * @param state windowing state pointer
 * @param quadant to set, between 0 and 3.
 * @return EGL_TRUE on succes, EGL_FALSE on failure
 * @note This sets which 'quadrant' of the screen that windows will be created upon
 * This must happen before the EGL Display is initialized, to be compatibile with FBDev.
 */
EGLBoolean egl_helper_windowing_set_quadrant( egl_helper_windowing_state *state, int quadrant );

/**
 * @brief Create a window, and open it if possible.
 * @param state windowing abstraction state
 * @param width width of the window
 * @param height height of the window
 * @param win pointer to native window, to return the output value.
 * @return EGL_TRUE on success, EGL_FALSE on failure
 * @note on failure, *win is set to a value suitable for calling
 * egl_helper_destroy_window on it.
 * @remarks Separate return value used here; some platforms support multiple
 * NULL windows, so NULL cannot be an error code, and each platform has a
 * different interpretation as to what an invalid Native Window is.
 */
EGLBoolean egl_helper_create_window( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType *win );
#ifdef ANDROID
EGLBoolean egl_helper_create_window_outside_framebuffer( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType *win );
#endif

/**
 * @brief Close a window and free all resources associated with it
 * @param state windowing abstraction state
 * @param win native window to close
 * @return EGL_TRUE on succes, EGL_FALSE on failure
 * @note This will show success for a value of win from a failling
 * egl_helper_destroy_window, allowing arrays of these where some failed to be
 * freed without error
 */
EGLBoolean egl_helper_destroy_window( egl_helper_windowing_state *state, EGLNativeWindowType win );

/**
 * @brief Create an invalid window
 * @param state windowing abstraction state
 * @param win pointer to native window, to return the output value.
 * @return EGL_TRUE on success, EGL_FALSE on failure
 * @note You must use egl_helper_destroy_window when finished with this
 * invalid window.
 * @note on failure, *win is set to a value suitable for calling
 * egl_helper_destroy_window on it.
 * @remarks Separate return value used here; some platforms support multiple
 * NULL windows, so NULL cannot be an error code, and each platform has a
 * different interpretation as to what an invalid Native Window is.
 */
EGLBoolean egl_helper_create_invalid_window( egl_helper_windowing_state *state, EGLNativeWindowType *win );

/**
 * @brief Invalidate a window so that it may never be used again, but
 * resources are still present
 * @param state windowing abstraction state
 * @param win native window to close
 * @return EGL_TRUE on succes, EGL_FALSE on failure
 * @note The window must still be destroyed with egl_helper_destroy_window()
 */
EGLBoolean egl_helper_invalidate_window( egl_helper_windowing_state *state, EGLNativeWindowType win );

/**
 * @brief Get a native window's width
 * @param state windowing abstraction state
 * @param win the native window
 * @return native window's width
 */
u32 egl_helper_get_window_width( egl_helper_windowing_state *state, EGLNativeWindowType win );

/**
 * @brief Get a native window's height
 * @param state windowing abstraction state
 * @param win the native window
 * @return native window's height
 */
u32 egl_helper_get_window_height( egl_helper_windowing_state *state, EGLNativeWindowType win );

/**
 * @brief Resize a window.
 * @param state windowing abstraction state
 * @param width new width of the window
 * @param height new height of the window
 * @param win native window to modify
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
EGLBoolean egl_helper_resize_window( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType win );

/**
 * @brief save the display info in to egl_helper_windowing_state.
 * @param[in] display: egl display
 * @param[out]: state: pointer to egl helper windowing state
 * @return None
 */
void egl_helper_android_set_display(egl_helper_windowing_state *state, EGLDisplay display);

#endif /* EXCLUDE_EGL_SPECIFICS */

/**
 * @brief Allocate physically contiguous memory
 * @param test_suite test_suite to make allocations/log errors on
 * @param mapped_pointer address of a pointer to the mapped phsyically contiguous memory allocated
 * @param phys_addr pointer to the physically contiguous memory allocated
 * @param size number of bytes to allocate
 * @return cookie value associated with allocation. Note that the function failed when *mapped_pointer == NULL
 */
u32 egl_helper_physical_memory_get( suite *test_suite, void **mapped_pointer, u32 size, u32 *phys_addr );

/**
 * @brief Release physically contiguous memory allocated by egl_helper_physical_memory_get
 * @param test_suite test_suite to make allocations/log errors on
 * @param mapped_pointer address of a pointer to the mapped phsyically contiguous memory allocated
 * @param size size of the allocation
 * @param cookie value associated with allocation
 */
void egl_helper_physical_memory_release( suite *test_suite, void *mapped_pointer, u32 size, u32 cookie );


/**
 * A sleep function.
 * Takes the number of microseconds to sleep.
 * The function will sleep AT LEAST usec microseconds.
 * The sleep duration might be longer due to scheduling on the system.
 * @param usec Number of microseconds to sleep
 */
void egl_helper_usleep( u64 usec );

/**
 * Yield the processor from the currently executing thread to another ready to run,
 * active thread of equal or higher priority
 * Function will return if there are no such threads, and continue to run
 * @return MALI_ERR_NO_ERROR if successful, or MALI_ERR_FUNCTION_FAILED on failure
 */
u32 egl_helper_yield( void );


/**
 * Initialize/terminate
 */

/**
 * @brief Initialize EGL helper platform specific layer. e.g.
 * create execution control environment, connect to Window Server session, etc.
 * @param None
 * @return 0 on success, error code on failure
 */
int egl_helper_initialize( void );

/**
 * @brief Terminate EGL helper platform specific layer, e.g.
 * destroy execution control environment, close Window Server session, etc.
 * @param None
 * @return None
 */
void egl_helper_terminate( void );

/**
 * File opening
 */
FILE *egl_helper_fopen(const char *filename, const char *mode);

/**
 * File reading
 */
u32 egl_helper_fread(void *buf, u32 size, u32 count, FILE *fp);

/**
 * @brief fill config with invalid value.
 * @param[out] config: to be given with an invalid value
 * @return None
 */
void egl_helper_get_invalid_config(EGLConfig *config);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EGL_HELPERS_H */
