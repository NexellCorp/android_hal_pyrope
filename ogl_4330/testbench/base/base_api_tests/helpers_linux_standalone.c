/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/* For the moment, just define EXCLUDE_EGL_SPECIFICS, and #include the original egl_helpers_fbdev.c */
#define EXCLUDE_EGL_SPECIFICS

#include "../../api_tests/egl/egl_helpers_fbdev.c"
