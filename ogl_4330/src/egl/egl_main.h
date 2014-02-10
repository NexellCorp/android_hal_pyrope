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
 * @file egl_main.h
 * @brief The internal context of an EGL process
 */

#ifndef _EGL_MAIN_H_
#define _EGL_MAIN_H_

/**
 * @addtogroup eglcore EGL core API
 *
 * @{
 *
 * \dot
 *  digraph example {
 *      node [shape=record, fontname=Helvetica, fontsize=10];
 *      a [ label="EGL Main" URL="\ref EGLMain"];
 *      b [ label="EGL Display" URL="\ref EGLDisplay"];
 *      c [ label="EGL Surface" URL="\ref EGLSurface"];
 *      d [ label="EGL Context" URL="\ref EGLContext"];
 *      e [ label="EGL Config" URL="\ref EGLConfig"];
 *      a -> b [ arrowhead="open", style="dashed" ];
 *      b -> c [ arrowhead="open", style="dashed" ];
 *      b -> d [ arrowhead="open", style="dashed" ];
 *      b -> e [ arrowhead="open", style="dashed" ];
 *  }
 *  \enddot
 */

#include <EGL/mali_egl.h>
#include "egl_config_extension.h"
#ifdef EGL_HAS_EXTENSIONS
#include <EGL/mali_eglext.h>
#endif
#include <shared/mali_named_list.h>
#include <shared/mali_egl_api_globals.h>
#include <base/mali_types.h>
#include <base/mali_context.h>
#include <base/mali_macros.h>

#include "api_interface_egl.h"

#ifdef EGL_MALI_GLES
#include "api_interface_gles.h"
#endif

#ifdef EGL_MALI_VG
#include "api_interface_vg.h"
#endif

#include "egl_linker.h"

#ifdef EGL_WORKER_THREAD_ENABLED
#include <base/mali_worker.h>
#endif

extern egl_api_shared_function_ptrs egl_funcptrs;

/** types of operations to perform on the main mutex */
typedef enum
{
	EGL_MAIN_MUTEX_LOCK,
	EGL_MAIN_MUTEX_UNLOCK,
	EGL_MAIN_MUTEX_ALL_LOCK,
	EGL_MAIN_MUTEX_ALL_UNLOCK,
#if EGL_KHR_reusable_sync_ENABLE
	EGL_MAIN_MUTEX_SYNC_LOCK,
	EGL_MAIN_MUTEX_SYNC_UNLOCK,
#endif
	EGL_MAIN_MUTEX_IMAGE_LOCK,
	EGL_MAIN_MUTEX_IMAGE_UNLOCK,
	EGL_MAIN_MUTEX_NOP
} __main_mutex_action;

typedef enum
{
	EGL_POWER_EVENT_SUSPEND = 0,      /**< Power Management Event when suspending / loosing power */
	EGL_POWER_EVENT_RESUME = (1<<0)   /**< Power Management Event when resuming after suspend / power loss */
} __egl_power_event;

typedef enum
{
	EGL_STATE_UNITITALIZED = 0,       /**< Nothing initialized yet            */
	EGL_STATE_MEMOPEN      = (1<<0),  /**< Base memory opened                 */
	EGL_STATE_PPOPEN       = (1<<1),  /**< MaliPP open                        */
	EGL_STATE_GPOPEN       = (1<<2),  /**< MaliGP open                        */
	EGL_STATE_POPEN        = (1<<3),  /**< Native platform initialized        */
	EGL_STATE_IOPEN        = (1<<4),  /**< Instrumented initialized           */
	EGL_STATE_INITIALIZED  = (1<<5)   /**< EGL main context initialized       */
} __egl_state;

typedef struct __egl_main_context
{
	mali_named_list      *display;          /** list of all displays */
	u32                  displays_open;     /** number of displays on which mali has been opened */
	mali_named_list      *thread;           /** list of all threads */
	u32                  vsync_ticker;      /** ticker events from vsync */
	mali_bool            vsync_busy;        /** MALI_TRUE if vsync_ticker is being updated */
	mali_lock_handle     main_lock;         /** cross thread main mutex for keeping internal egl state sane*/
#if EGL_KHR_reusable_sync_ENABLE
	mali_lock_handle     sync_lock;         /** cross thread sync mutex for non-blocking swap buffers */
#endif
	mali_lock_handle     image_lock;        /** EGLImageKHR API entrypoint lock */
	mali_mutex_handle    mutex_vsync;       /** mutex for updating the vsync variable */
	mali_base_ctx_handle base_ctx;          /** reference to the base driver context */
	EGLBoolean           back_buffer_bound; /** EGL_TRUE if the back buffer is bound i.e when using render to image */
	EGLBoolean           never_blit;        /** if envvar is set never perform blit (used for benchmarking) */
	EGLBoolean           flip_pixmap;       /** if envvar is set (MALI_FLIP_PIXMAP), pixmap is flipped to upper-left coordinate system */
	u32                  state;             /** bitfield to remember state of base */
	egl_linker           *linker;           /** linker instance */
#ifdef EGL_MALI_VG
	void                 *vg_global_context;/** OpenVG global context */
	egl_vg_global_data   vg_global_data;   /** OpenVG global data */
#endif
#ifdef EGL_MALI_GLES
	egl_gles_global_data gles_global_data;  /**< OpenGLES global data */
#endif

#if EGL_KHR_image_ENABLE
	mali_named_list      *egl_images;
#endif

#ifdef EGL_WORKER_THREAD_ENABLED
	mali_base_worker_handle worker; /**< Interface to worker thread that performs slow tasks that shouldn't be performed in the mali callback thread */
#endif

} __egl_main_context;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief callback for any power event
 * @param event the power event identifier
 */
void __egl_main_power_event( u32 event );

/**
 * @brief Checks if EGL main context has been initialized
 * @return EGL_TRUE if it has, EGL_FALSE else
 */
EGLBoolean __egl_main_initialized( void ) MALI_CHECK_RESULT;

/**
 * @brief Retrieves the egl main context - and creates one if it has not been done yet
 * @return a pointer to the egl main context
 * @note allocates space for the thread and display list
 * @note is supposed to set up the callback for vsync
 * @note initializes the base driver, and opens pp/gp
 */
__egl_main_context* __egl_get_main_context( void ) MALI_CHECK_RESULT;

/**
 * @brief Performs a lock on the main mutex lock
 * @note will automatically try to initialize the main
 * context if it has not been initialized previously
 */
void __egl_main_mutex_lock( void );

/**
 * @brief Performs an unlock on the main mutex lock
 * @note will automatically try to initialize the main
 * context if it has not been initialized previously
 */
void __egl_main_mutex_unlock( void );

/**
 * @brief Performs a lock on both the sync and the main mutex
 * @note will automatically try to initialize the main
 * context if it has not been initialized previously
 */
void __egl_all_mutexes_lock( void );

/**
 * @brief Performs an unlock on both the sync and main mutex
 * @note will automatically try to initialize the main
 * context if it has not been initialized previously
 */
void __egl_all_mutexes_unlock( void );

/**
 * @brief Performs a lock on the sync mutex
 * @note will automatically try to initialize the main
 * context if it has not been initialized previously
 */
void __egl_sync_mutex_lock( void );

/**
 * @brief Performs an unlock on the sync mutex
 * @note will automatically try to initialize the main
 * context if it has not been initialized previously
 */
void __egl_sync_mutex_unlock( void );

/**
 * @brief Performs a lock on the image mutex
 * @note will automatically try to initialize the main
 * context if it has not been initialized previously
 */
void __egl_image_mutex_lock( void );

/**
 * @brief Performs an unlock on the image mutex
 * @note will automatically try to initialize the main
 * context if it has not been initialized previously
 */
void __egl_image_mutex_unlock( void );

/**
 * @brief Performs a mutex action on the main mutex lock
 * Available actions are
 * EGL_MAIN_MUTEX_LOCK   locks the mutex
 * EGL_MAIN_MUTEX_UNLOCK unlocks the mutex
 * EGL_MAIN_MUTEX_NOP    performs no operation
 * @param mutex_action the action to take on the mutex lock
 * @note this is used internally
 */
void __egl_main_mutex_action( __main_mutex_action mutex_action );

/**
 * @brief Open all mali resources in an atomic fashion
 *
 * @note If __egl_main does not previously exist, then it'll be created
 *       This is reasonable, as you want resources to be opened
 *
 * @note Do not call this if any resource is already open.
 *
 * @return EGL_TRUE on success, EGL_FALSE otherwise
 * @note If opening fails, then resources are closed, and so there is no need
 *       to call __egl_main_close_mali().
 */
EGLBoolean __egl_main_open_mali( void );

/**
 * @brief Close all mali resources
 *
 * This can be used to close mali resources in an error state, as it handles
 * incomplete resource opening.
 *
 * @note Do not call this while the EGL main context is not initialized
 * @note When the EGL main context is initialized, itt is acceptable to call
 *       this when no resources are open.
 */
void __egl_main_close_mali( void );

/**
 * @brief Destroys the egl main context
 */
void __egl_destroy_main_context( void );

/**
 * @brief Destroys main context if no threads are initialized
 * 
 * Used for complete cleanup of all resources allocated by EGL. Assumes
 * that all threads have been released.
 *
 * @note EGL mutexes will be freed so any mutexes held should not be unlocked
 *       afterwards.
 * @return If main context was destroyed
 */
EGLBoolean __egl_destroy_main_context_if_threads_released( void );

#ifdef __cplusplus
}
#endif

/** @} */ /* end group eglcore */

#endif /* _EGL_MAIN_H_ */


