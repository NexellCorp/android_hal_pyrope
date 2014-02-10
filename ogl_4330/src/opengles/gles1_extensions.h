/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_extensions.h
 * @brief Defines an interface to get extension function pointers from OpenGL ES 1.x.
 */

#ifndef _GLES1_EXTENSIONS_H_
#define _GLES1_EXTENSIONS_H_

/**
 * Returns a pointer to the function with the name procname from the OpenGL ES 1.x
 * extension list.
 *
 * This function is only declared/defined if OpenGL ES 1.x is a part of the build.
 *
 * @param procname The name of the function to retrieve a pointer to.
 * @return A pointer to the function with the name procname.
 */
void (* _gles1_get_proc_address(const char *procname))(void);

#endif /* _GLES1_EXTENSIONS_H_ */
