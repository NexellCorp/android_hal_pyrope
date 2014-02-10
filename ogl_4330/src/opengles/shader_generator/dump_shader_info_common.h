/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _DUMP_SHADER_INFO_COMMON_H_
#define _DUMP_SHADER_INFO_COMMON_H_

#include <stdio.h>

#ifndef MALI_BUILD_ANDROID_MONOLITHIC
#define MALI_GLES_NAME_WRAP(a) a
#else
#define MALI_GLES_NAME_WRAP(a) shim_##a
#endif

void print_vecn(mali_file* fp, const char* name, const float* v, int n);
void print_vec3(mali_file* fp, const char* name, const float* v);
void print_vec4(mali_file* fp, const char* name, const float* v);
void print_matrix4x4(mali_file* fp, float* matrix);

#endif
