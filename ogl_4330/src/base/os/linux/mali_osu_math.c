/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * Define X/Open macro to indicate Single Unix Specification v3
 * This does NOT mean open source, but source based on X/Open.
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <mali_system.h>
#include "mali_osu.h"

#include <math.h>

#include <stdlib.h>


/**
 * @file mali_osu_math.c
 * File implements the user side of the OS interface
 */

float _mali_osu_sqrt(float value)
{
	return sqrtf(value);
}

float _mali_osu_sin(float value)
{
	return sinf(value);
}

float _mali_osu_cos(float value)
{
	return cosf(value);
}

float _mali_osu_atan2(float y, float x)
{
	return MALI_STATIC_CAST(float)atan2(y, x);
}

float _mali_osu_ceil(float value)
{
	return ceilf(value);
}

float _mali_osu_floor(float value)
{
	return floorf(value);
}

float _mali_osu_fabs(float value)
{
	return fabsf(value);
}

s32 _mali_osu_abs(s32 value)
{
	return labs(value);
}

s64 _mali_osu_abs64(s64 value)
{
	return llabs(value);
}

float _mali_osu_exp(float value)
{
	return expf(value);
}

float _mali_osu_pow(float base, float exponent)
{
	return MALI_STATIC_CAST(float) pow(base,exponent);
}

float _mali_osu_log(float value)
{
	return logf(value);
}

int _mali_osu_isnan(float value)
{
    return isnan(value);
}

float _mali_osu_fmodf(float n, float m)
{
	return MALI_STATIC_CAST(float) fmod(n, m);
}
