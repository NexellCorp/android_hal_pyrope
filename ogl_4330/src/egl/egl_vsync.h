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
 * @file egl_vsync.h
 * @brief Implementation of the vsync handling.
 * @deprecated
 * Includes a simulator for generating vsync events given a hertz.
 */

#ifndef _EGL_VSYNC_H_
#define _EGL_VSYNC_H_
#include <mali_system.h>
#include <base/mali_macros.h>

#ifdef EGL_SIMULATE_VSYNC
/**
 * @brief Starts a simulation of vsync events
 * @param hertz the hertz of the display to simulate
 */
void __egl_vsync_simulation_start( u32 hertz );

/**
 * @brief Stops the simulation of vsync events
 */
void __egl_vsync_simulation_stop( void );
#endif

/**
 * @brief Callback that is called each time a vsync event occurs
 * This callback will be called once for every vsync
 * recorded by the base driver.
 * We'll have to make an assumption that we will
 * have this functionality. Do as little as possible here,
 * we do not want to block the callback too much.
 * @note locks the egl main context vsync mutex while updating the vsync ticker
 */
void __egl_vsync_callback( void );

/**
 * @brief returns the current vsync ticker
 * @return vsync ticker
 */
u32 __egl_vsync_get( void ) MALI_CHECK_RESULT;

/**
 * @brief Resets the vsync counter
 */
void __egl_vsync_reset( void );

#endif /* _EGL_VSYNC_H_ */

