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
 * @file mali_osu_string.c
 * File implements the user side of the OS interface
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <mali_system.h>
#include "mali_osu.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


int _mali_osu_vsnprintf(char* str, u32 size, const char * format, va_list args)
{
	return vsnprintf(str, size, format, args);
}

u32 _mali_osu_strtoul(const char *nptr, char **endptr, u32 base)
{
	return strtoul(nptr, endptr, base);
}

s32 _mali_osu_strtol(const char *nptr, char **endptr, u32 base)
{
	return strtol(nptr, endptr, base);
}

double _mali_osu_strtod(const char *nptr, char **endptr)
{
	return strtod(nptr, endptr);
}

u32 _mali_osu_strlen(const char *str)
{
	return strlen(str);
}

char * _mali_osu_strdup(const char * str)
{
	return strdup(str);
}

u32 _mali_osu_strcmp(const char *str1, const char *str2)
{
	return strcmp(str1,str2);
}

u32 _mali_osu_strncmp(const char *str1, const char *str2, u32 count)
{
	return strncmp(str1, str2, count);
}

char *_mali_osu_strncpy(char *dest, const char *src, u32 n)
{
	return strncpy(dest,src,n);
}

char *_mali_osu_strncat(char *dest, const char *src, u32 n)
{
	return strncat(dest,src,n);
}

u32 _mali_osu_atoi(const char *str)
{
	return atoi(str);
}

