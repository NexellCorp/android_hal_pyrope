/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * ( C ) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_vtable.h
 * @brief Contains a function that retrieves the vtable of containing pointers to
 *        the second layer functions of OpenGL ES 2.x
 */

#ifndef _GLES2_VTABLE_H_
#define _GLES2_VTABLE_H_

struct gles_vtable;

/**
 * @brief Retrieves a vtable containing pointers to the second layer functions of
 *        OpenGL ES 2.x to be called directly from the OpenGL ES entrypoints.
 *
 * @note This vtable is statically allocated and should _not_ be freed by the caller.
 * @return a vtable containing pointers to the second layer of OpenGL ES 2.x
 */
const struct gles_vtable *_gles2_get_vtable( void );

#endif /* _GLES2_VTABLE_H_ */
