/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_extensions.h
 * @brief Retrieval of Client API and EGL extensions
 */

#ifndef _EGL_EXTENSIONS_H_
#define _EGL_EXTENSIONS_H_

/**
 * @addtogroup eglcore EGL core API
 *
 * @{
 */

/**
 * @defgroup egl_extension EGL Extensions
 *
 * @note EGL Extensions
 *
 * @{
 */

#include <base/mali_macros.h>

/**
 * @brief Retrieves the function address for the extension
 * @param procname string representing the name of function
 * @return pointer to the function on success, NULL else
 */
void (* _egl_get_proc_address_internal( const char *procname ))( void ) MALI_CHECK_RESULT;

/** @} */ /* end group eglcore */

/** @} */ /* end group egl_extension */

#endif /* _EGL_EXTENSIONS_H_ */


