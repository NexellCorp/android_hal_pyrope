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
 * Based on a Single UNIX Specification, Version 3
 */


#include <stdarg.h>

#ifdef HAVE_ANDROID_OS
#define LOG_TAG "MaliBase"
#include <utils/Log.h>
#endif
#include <base/mali_types.h>
#include <base/mali_runtime.h>

#include "mali_osu.h"

#if defined(MALI_TEST_API) || MALI_HOSTLIB_INDIRECT == 1

/**** Memory Functions ****/

void *_mali_base_arch_sys_malloc(u32 size)
{
	return _mali_osu_malloc(size);
}

void _mali_base_arch_sys_free(void * pointer)
{
	_mali_osu_free( pointer );
}

void *_mali_base_arch_sys_calloc(u32 nelements, u32 bytes)
{
	return _mali_osu_calloc(nelements, bytes);
}

void *_mali_base_arch_sys_realloc(void * ptr, u32 bytes)
{
	return _mali_osu_realloc(ptr, bytes);
}

char * _mali_base_arch_sys_strdup(const char * str)
{
	return _mali_osu_strdup(str);
}
#endif

#if MALI_HOSTLIB_INDIRECT == 1
void * _mali_base_arch_sys_memcpy(void * destination, const void * source, u32 num)
{
	return _mali_osu_memcpy(destination, source, num);
}

void * _mali_base_arch_sys_memset(void * ptr, u32 value, u32 num)
{
	return _mali_osu_memset(ptr, value, num);
}


s32 _mali_base_arch_sys_memcmp(const void *s1, const void *s2, u32 n)
{
	return _mali_osu_memcmp(s1, s2, n);
}

/**** Generic functions ****/

int _mali_base_arch_sys_snprintf(char * dest, u32 size, const char * format, va_list args)
{
	return _mali_osu_vsnprintf(dest, size, format, args);
}

void _mali_base_arch_sys_qsort(void * base, u32 num, u32 size, int (*compare)(const void *, const void *))
{
	_mali_osu_qsort(base, num, size, compare);
}

_mali_file_t * _mali_base_arch_sys_fopen(const char *path, const char *mode)
{
    struct {
        const char *mode_lower;
        const char *mode_upper; /* uppercase version of mode_lower: no _mali_osu_stricmp() available */
        _mali_file_mode_t osu_file_mode;
    } *mapping, file_mode_mapping[] = {
        { "r", "R", _MALI_FILE_MODE_R },
        { "w", "W", _MALI_FILE_MODE_W },
        { "a", "A", _MALI_FILE_MODE_A },
        { "rb", "RB", _MALI_FILE_MODE_RB },
        { "wb", "WB", _MALI_FILE_MODE_WB },
        { "ab", "AB", _MALI_FILE_MODE_AB },
        { NULL, NULL, (_mali_file_mode_t)0 },
    };

    mapping = file_mode_mapping;
    while (mapping->mode_lower)
    {
        if (0 == _mali_osu_strcmp(mode, mapping->mode_lower) || 0 == _mali_osu_strcmp(mode, mapping->mode_upper))
        	return _mali_osu_fopen(path, mapping->osu_file_mode);
        mapping++;
    }

    MALI_DEBUG_PRINT(0, ("unsupported mode parameter 0x%x for _mali_base_arch_sys_fopen()", mode));
    return NULL;
}

int  _mali_base_arch_sys_fprintf(_mali_file_t *file, const char *format, va_list args)
{
	int retval;
    void *buffer;
    int num_chars;

    /* Get count of characters that will get printed (excl. end of string) */
    num_chars = _mali_osu_vsnprintf(NULL, 0, format, args);
    if (num_chars < 0) return -1;

    /* Allocate temporary buffer (incl. end of string) */
    buffer = _mali_osu_malloc(num_chars + 1);
    if (NULL == buffer) return -1;

    /* Print to temporary buffer */
	if (num_chars != _mali_osu_vsnprintf(buffer, num_chars + 1, format, args))
    {
        _mali_osu_free(buffer);
        return -1;
    }

    /* Write temporary buffer to file */
#ifdef HAVE_ANDROID_OS
	if (file == MALI_STDOUT)
	{
		__android_log_print(ANDROID_LOG_INFO, "", "%s", (char*)buffer);
		retval = 0;
	}
	else
	{
		retval = _mali_osu_fwrite(buffer, sizeof(char), num_chars, file);
	}
#else
	retval = _mali_osu_fwrite(buffer, sizeof(char), num_chars, file);
#endif
    _mali_osu_free(buffer);

    /* Return number of characters written (excl. end of string) */
    return retval;
}

int _mali_base_arch_sys_printf(const char *format, va_list args)
{
    return _mali_base_arch_sys_fprintf(MALI_STDOUT, format, args);
}

u32 _mali_base_arch_sys_fwrite(const void * data, u32 element_size, u32 element_number, _mali_file_t *file)
{
	u32 retval;
	retval = _mali_osu_fwrite(data, element_size, element_number, file);
	return retval;
}

void  _mali_base_arch_sys_fclose(_mali_file_t *file)
{
	_mali_osu_fclose(file);
}

int _mali_base_arch_sys_remove(const char *filename)
{
	return _mali_osu_remove(filename);
}

mali_err_code _mali_base_arch_sys_yield(void)
{
	_mali_osu_yield();

	/** @note error code is dropped */

	MALI_SUCCESS;
}
void _mali_base_arch_sys_usleep(u64 usec)
{
	u32 csec;
	if (usec >= ((u64)MALI_U32_MAX)*((u64)10000))
	{
		csec = MALI_U32_MAX;
	}
	else
	{
		csec = (u32)(usec/((u64)10000));
		if (0 == csec)
		{
			csec = 1;
		}
	}

	_mali_osu_cssleep( csec );
}

u64 _mali_base_arch_sys_get_time_usec(void)
{
    return _mali_osu_get_time_usec();
}

u64 _mali_base_arch_sys_strtoull(const char *nptr, char **endptr, u32 base)
{
	return (u64)_mali_osu_strtoul(nptr, endptr, base);
}

s64 _mali_base_arch_sys_strtoll(const char *nptr, char **endptr, u32 base)
{
	return (s64)_mali_osu_strtol(nptr, endptr, base);
}

double _mali_base_arch_sys_strtod(const char *nptr, char **endptr)
{
	return _mali_osu_strtod(nptr, endptr);
}

u32 _mali_base_arch_sys_strlen(const char *str)
{
	return _mali_osu_strlen(str);
}

u32 _mali_base_arch_sys_strcmp(const char *str1, const char *str2)
{
	return _mali_osu_strcmp(str1,str2);
}

u32 _mali_base_arch_sys_strncmp(const char *str1, const char *str2, u32 count)
{
	return _mali_osu_strncmp(str1, str2, count);
}

char *_mali_base_arch_sys_strncpy(char *dest, const char *src, u32 n)
{
    return _mali_osu_strncpy(dest,src,n);
}

char *_mali_base_arch_sys_strncat(char *dest, const char *src, u32 n)
{
    return _mali_osu_strncat(dest,src,n);
}

u32 _mali_base_arch_sys_atoi(const char *str)
{
	return _mali_osu_atoi(str);
}

/*
 *	Math functions
 */

float _mali_base_arch_sys_sqrt(float value)
{
	return _mali_osu_sqrt(value);
}

float _mali_base_arch_sys_sin(float value)
{
	return _mali_osu_sin(value);
}

float _mali_base_arch_sys_cos(float value)
{
	return _mali_osu_cos(value);
}

float _mali_base_arch_sys_atan2(float y, float x)
{
	return _mali_osu_atan2(y, x);
}

float _mali_base_arch_sys_ceil(float value)
{
	return _mali_osu_ceil(value);
}

float _mali_base_arch_sys_floor(float value)
{
	return _mali_osu_floor(value);
}

float _mali_base_arch_sys_fabs(float value)
{
	return _mali_osu_fabs(value);
}

s32 _mali_base_arch_sys_abs(s32 value)
{
	return _mali_osu_abs(value);
}

s64 _mali_base_arch_sys_abs64(s64 value)
{
	return _mali_osu_abs64(value);
}

float _mali_base_arch_sys_exp(float value)
{
	return _mali_osu_exp(value);
}

float _mali_base_arch_sys_pow(float base, float exponent)
{
	return _mali_osu_pow(base,exponent);
}

float _mali_base_arch_sys_log(float value)
{
	return _mali_osu_log(value);
}

int _mali_base_arch_sys_isnan(float value)
{
    return _mali_osu_isnan(value);
}

float _mali_base_arch_sys_fmodf(float n, float m)
{
	return _mali_osu_fmodf(n, m);
}

u32 _mali_base_arch_sys_rand(void)
{
    return _mali_osu_rand();
}

PidType _mali_base_arch_sys_get_pid(void)
{
	return _mali_osu_get_pid();
}

PidType _mali_base_arch_sys_get_tid(void)
{
	return _mali_osu_get_tid();
}

/*
 *      System configuration/interaction
 */


const char * _mali_base_arch_sys_config_string_get(const char * name)
{
	return _mali_osu_config_string_get(name);
}

mali_bool _mali_base_arch_sys_config_string_set(const char * name, const char *value)
{
	if (_MALI_OSK_ERR_OK == _mali_osu_config_string_set(name, value)) return MALI_TRUE;
    return MALI_FALSE;
}

void _mali_base_arch_sys_config_string_release(const char * env_var)
{
	_mali_osu_config_string_release( env_var );
}

#endif /* MALI_HOSTLIB_INDIRECT = 1 */

void _mali_base_arch_sys_load_config_strings(void)
{
	_mali_osu_load_config_strings();
}

void _mali_base_arch_sys_break(void)
{
	_mali_osu_break();
}

void _mali_base_arch_sys_abort(void)
{
	_mali_osu_abort();
}

mali_library * _mali_base_arch_library_load(const char * name)
{
	return _mali_osu_library_load(name);
}

void _mali_base_arch_library_unload(mali_library * handle)
{
	_mali_osu_library_unload(handle);
}

mali_bool _mali_base_arch_library_init(mali_library * handle, void* param, u32* retval)
{
	return _mali_osu_library_init(handle, param, retval) == _MALI_OSU_ERR_OK ? MALI_TRUE : MALI_FALSE;
}
