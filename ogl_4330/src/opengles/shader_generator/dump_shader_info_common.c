/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_base.h>

#include "dump_shader_info_common.h"

void print_vecn(mali_file* fp, const char* name, const float* v, int n)
{
    int i;
    _mali_sys_fprintf(fp, "%s{ ", name);
    for(i=0; i<n; i++) {
        if(i == n-1)
            _mali_sys_fprintf(fp, "% 8.4f }", v[i]);
        else
            _mali_sys_fprintf(fp, "% 8.4f, ", v[i]);
    }
    _mali_sys_fprintf(fp, "\n");
}

void print_vec3(mali_file* fp, const char* name, const float* v)
{
    print_vecn(fp, name, v, 3);
}

void print_vec4(mali_file* fp, const char* name, const float* v)
{
    print_vecn(fp, name, v, 4);
}

void print_matrix4x4(mali_file* fp, float* matrix)
{
    int i,j;
    for(i = 0; i < 4; i++)
    {
        _mali_sys_fprintf(fp, "        [ ");
        for(j = 0; j < 4; j++)
        {
            _mali_sys_fprintf(fp, "% 8.4f ", matrix[j*4+i]);
        }
        _mali_sys_fprintf(fp, "]\n");
    }    
}

