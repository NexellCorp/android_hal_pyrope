/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifdef __SYMBIAN32__
#include <symbian_base.h>
#endif

#include <stdarg.h>
#include <base/mali_runtime.h>
#include <base/mali_byteorder.h>
#include <base/arch/base_arch_runtime.h>

#ifdef MALI_TEST_API
	#include <base/test/mali_mem_test.h>
#endif

#ifdef __SYMBIAN32__
#include <mali_osu.h>
#include <symbian_base.h>
#endif

#if defined(MALI_TEST_API) || MALI_HOSTLIB_INDIRECT == 1
MALI_EXPORT void *_mali_sys_malloc(u32 size)
{
#ifdef MALI_TEST_API
	void *ptr =_mali_test_sys_malloc(size);
	return ptr;
#elif defined(__SYMBIAN32__) && defined(MEM_FAIL_PRINT)
	u32 lr = __return_address();
	void* ptr = _mali_base_arch_sys_malloc(size);
	if ( NULL == ptr )
	{
		__MEM_FAIL_PRINT("*** Error: %s(%d) lr=0x%X ***", __FUNCTION__, size, lr);
	}
	return ptr;
#else
	return _mali_base_arch_sys_malloc(size);
#endif
}

MALI_EXPORT void _mali_sys_free(void *pointer)
{
#ifdef MALI_TEST_API
    _mali_test_sys_free(pointer);
#else
	_mali_base_arch_sys_free(pointer);
#endif
}

MALI_EXPORT void *_mali_sys_calloc(u32 nelements, u32 bytes)
{
#ifdef MALI_TEST_API
	return _mali_test_sys_calloc(nelements, bytes);
#elif defined(__SYMBIAN32__) && defined(MEM_FAIL_PRINT)
    u32 lr = __return_address();
    void* ptr = _mali_base_arch_sys_calloc(nelements, bytes);
    if ( NULL == ptr )
    {
        __MEM_FAIL_PRINT("*** Error: %s(%d %d) lr=0x%X ***", __FUNCTION__, nelements, bytes, lr);    
    }
    return ptr;
#else
	return _mali_base_arch_sys_calloc(nelements, bytes);
#endif
}

MALI_EXPORT void *_mali_sys_realloc(void *ptr, u32 bytes)
{
#ifdef MALI_TEST_API
	return _mali_test_sys_realloc(ptr, bytes);
#elif defined(__SYMBIAN32__) && defined(MEM_FAIL_PRINT)
    u32 lr = __return_address();
    void* new_ptr = _mali_base_arch_sys_realloc(ptr, bytes);
    if ( NULL == new_ptr )
    {
        __MEM_FAIL_PRINT("*** Error: %s(0x%X %d) lr=0x%X ***", __FUNCTION__, ptr, bytes, lr);
    }
    return new_ptr;
#else
	return _mali_base_arch_sys_realloc(ptr, bytes);
#endif
}

MALI_EXPORT char * _mali_sys_strdup(const char * str)
{
#ifdef MALI_TEST_API
	return _mali_test_sys_strdup(str);
#else
	return _mali_base_arch_sys_strdup(str);
#endif
}

#endif /* MALI_TEST_API || MALI_HOSTLIB_INDIRECT == 1 */

#if MALI_HOSTLIB_INDIRECT == 1

MALI_EXPORT void * _mali_sys_memcpy(void * destination, const void * source, u32 num)
{
	return _mali_base_arch_sys_memcpy(destination, source, num);
}

MALI_EXPORT void * _mali_sys_memcpy_cpu_to_mali(void * destination, const void * source, u32 num, u32 element_size)
{
	MALI_IGNORE(element_size);
	return _mali_byteorder_copy_cpu_to_mali(destination, source, num, element_size);
}

MALI_EXPORT void * _mali_sys_memcpy_mali_to_mali(void * destination, const void * source, u32 num, u32 element_size)
{
	MALI_IGNORE(element_size);
	return _mali_byteorder_copy_mali_to_mali(destination, source, num, element_size);
}

MALI_EXPORT void * _mali_sys_memset(void * ptr, u32 value, u32 num)
{
	return _mali_base_arch_sys_memset(ptr, value, num);
}

MALI_EXPORT void * _mali_sys_memset_endian_safe(void * ptr, u32 value, u32 num)
{
	return _mali_byteorder_memset(ptr, value, num);
}

MALI_EXPORT s32 _mali_sys_memcmp(const void *s1, const void *s2, u32 n)
{
	return _mali_base_arch_sys_memcmp(s1, s2, n);
}

MALI_EXPORT int _mali_sys_printf(const char *format, ...)
{
	int res;
	va_list args;

	va_start(args, format);
	res = _mali_base_arch_sys_printf(format, args);
	va_end(args);

	return res;
}

MALI_EXPORT int _mali_sys_snprintf(char* str, u32 size, const char * format, ... )
{
	int res;
	va_list args;

	va_start(args, format);
	res = _mali_base_arch_sys_snprintf(str, size, format, args);
	va_end(args);

	return res;
}

MALI_EXPORT void _mali_sys_qsort(void * base, u32 num, u32 size, int (*compare)(const void *, const void *))
{
	_mali_base_arch_sys_qsort(base, num, size, compare);
}

MALI_EXPORT u64 _mali_sys_strtoull(const char *nptr, char **endptr, u32 base)
{
	return _mali_base_arch_sys_strtoull(nptr, endptr, base);
}

MALI_EXPORT s64 _mali_sys_strtoll(const char *nptr, char **endptr, u32 base)
{
	return _mali_base_arch_sys_strtoll(nptr, endptr, base);
}

MALI_EXPORT double _mali_sys_strtod(const char *nptr, char **endptr)
{
	return _mali_base_arch_sys_strtod(nptr, endptr);
}

MALI_EXPORT u32 _mali_sys_strlen(const char *str)
{
	return _mali_base_arch_sys_strlen(str);
}

MALI_EXPORT u32 _mali_sys_strcmp(const char *str1, const char *str2)
{
	return _mali_base_arch_sys_strcmp(str1,str2);
}

MALI_EXPORT u32 _mali_sys_strncmp(const char *str1, const char *str2, u32 count)
{
	return _mali_base_arch_sys_strncmp(str1, str2, count);
}

MALI_EXPORT char *_mali_sys_strncat(char *dest, const char *src, u32 n)
{
	return _mali_base_arch_sys_strncat(dest,src,n);
}

MALI_EXPORT char *_mali_sys_strncpy(char *dest, const char *src, u32 n)
{
	return _mali_base_arch_sys_strncpy(dest,src,n);
}

MALI_EXPORT u32 _mali_sys_atoi(const char *str)
{
	return _mali_base_arch_sys_atoi(str);
}

/*
 *	Math functions
 */

MALI_EXPORT float _mali_sys_sqrt(float value)
{
	return _mali_base_arch_sys_sqrt(value);
}

MALI_EXPORT float _mali_sys_sin(float value)
{
	return _mali_base_arch_sys_sin(value);
}

MALI_EXPORT float _mali_sys_cos(float value)
{
	return _mali_base_arch_sys_cos(value);
}

MALI_EXPORT float _mali_sys_atan2(float y, float x)
{
	return _mali_base_arch_sys_atan2(y, x);
}

MALI_EXPORT float _mali_sys_ceil(float value)
{
	return _mali_base_arch_sys_ceil(value);
}

MALI_EXPORT float _mali_sys_floor(float value)
{
	return _mali_base_arch_sys_floor(value);
}

MALI_EXPORT float _mali_sys_fabs(float value)
{
	return _mali_base_arch_sys_fabs(value);
}

MALI_EXPORT s32 _mali_sys_abs(s32 value)
{
	return _mali_base_arch_sys_abs(value);
}

MALI_EXPORT s64 _mali_sys_abs64(s64 value)
{
	return _mali_base_arch_sys_abs64(value);
}

MALI_EXPORT float _mali_sys_exp(float value)
{
	return _mali_base_arch_sys_exp(value);
}

MALI_EXPORT float _mali_sys_pow(float base, float exponent)
{
	return _mali_base_arch_sys_pow(base,exponent);
}

MALI_EXPORT float _mali_sys_log(float value)
{
	return _mali_base_arch_sys_log(value);
}

MALI_EXPORT float _mali_sys_log2(float value)
{
	return _mali_base_arch_sys_log(value)/_mali_base_arch_sys_log(2);
}

MALI_EXPORT int _mali_sys_isnan(float value)
{
    return _mali_base_arch_sys_isnan(value);
}

MALI_EXPORT float _mali_sys_fmodf(float n, float m)
{
	return _mali_base_arch_sys_fmodf(n, m);
}

MALI_EXPORT u32 _mali_sys_rand(void)
{
	return _mali_base_arch_sys_rand();
}

MALI_EXPORT PidType _mali_sys_get_pid(void)
{
	return _mali_base_arch_sys_get_pid();
}

MALI_EXPORT PidType _mali_sys_get_tid(void)
{
	return _mali_base_arch_sys_get_tid();
}

MALI_EXPORT mali_file * _mali_sys_fopen(const char *path, const char *mode)
{
	return _mali_base_arch_sys_fopen(path, mode);
}

MALI_EXPORT int  _mali_sys_fprintf(mali_file *file, const char *format, ...)
{
	int res;
	va_list args;

	va_start(args, format);
	res = _mali_base_arch_sys_fprintf(file, format, args);
	va_end(args);

	return res;
}

MALI_EXPORT void  _mali_sys_fclose(mali_file *file)
{
	_mali_base_arch_sys_fclose(file);
}

MALI_EXPORT u32 _mali_sys_fwrite(const void * data, u32 element_size, u32 element_number, mali_file *file)
{
	u32 retval;
	retval = _mali_base_arch_sys_fwrite(data, element_size, element_number, file);
	return retval;
}

MALI_EXPORT int _mali_sys_remove(const char *filename)
{
	return _mali_base_arch_sys_remove(filename);
}

/*
 *      Time/sleep functions
 */
MALI_EXPORT void _mali_sys_usleep(u64 usec)
{
	_mali_base_arch_sys_usleep(usec);
}

MALI_EXPORT u64 _mali_sys_get_time_usec(void)
{
	return _mali_base_arch_sys_get_time_usec();
}

/*
 *      System configuration/interaction
 */
MALI_EXPORT const char * _mali_sys_config_string_get(const char * name)
{
	return _mali_base_arch_sys_config_string_get(name);
}

MALI_EXPORT mali_bool _mali_sys_config_string_set(const char * name, const char *value)
{
    return _mali_base_arch_sys_config_string_set(name, value);
}

MALI_EXPORT void _mali_sys_config_string_release(const char * env_var)
{
	_mali_base_arch_sys_config_string_release(env_var);
}

MALI_EXPORT s64 _mali_sys_config_string_get_s64(const char * name, s64 default_val, s64 min_val, s64 max_val)
{
	const char * sys_string;
	s64 retval = default_val;
	sys_string = _mali_sys_config_string_get(name);
	if ( NULL!=sys_string )
	{
		retval = _mali_sys_strtoll(sys_string, NULL, 0);
		_mali_sys_config_string_release(sys_string);
	}
	retval = MAX(retval, min_val);
	retval = MIN(retval, max_val);
	return retval;
}

MALI_EXPORT mali_bool _mali_sys_config_string_get_bool(const char * name, mali_bool default_val)
{
	const char * sys_string;
	mali_bool retval = default_val;
	sys_string = _mali_sys_config_string_get(name);
	if ( NULL!=sys_string )
	{
		if(0==_mali_sys_strcmp(sys_string, "TRUE")) retval = MALI_TRUE;
		else if(0==_mali_sys_strcmp(sys_string, "1")) retval = MALI_TRUE;

		else if(0==_mali_sys_strcmp(sys_string, "FALSE")) retval = MALI_FALSE;
		else if(0==_mali_sys_strcmp(sys_string, "0")) retval = MALI_FALSE;

		_mali_sys_config_string_release(sys_string);
	}

	return retval;
}

MALI_EXPORT mali_err_code _mali_sys_yield(void)
{
	return _mali_base_arch_sys_yield();
}

#endif /* MALI_HOSTLIB_INDIRECT = 1 */

MALI_EXPORT void _mali_sys_load_config_strings(void)
{
	_mali_base_arch_sys_load_config_strings();
}

MALI_EXPORT mali_spinlock_handle _mali_sys_spinlock_create(void)
{
	return _mali_base_arch_sys_spinlock_create();
}

MALI_EXPORT void _mali_sys_spinlock_lock(mali_spinlock_handle spinlock)
{
	_mali_base_arch_sys_spinlock_lock(spinlock);
}

MALI_EXPORT mali_err_code _mali_sys_spinlock_try_lock(mali_spinlock_handle spinlock)
{
	return _mali_base_arch_sys_spinlock_try_lock(spinlock);
}

MALI_EXPORT void _mali_sys_spinlock_unlock(mali_spinlock_handle spinlock)
{
	_mali_base_arch_sys_spinlock_unlock(spinlock);
}

MALI_EXPORT void _mali_sys_spinlock_destroy(mali_spinlock_handle spinlock)
{
	_mali_base_arch_sys_spinlock_destroy(spinlock);
}

MALI_EXPORT mali_err_code _mali_sys_mutex_auto_init(volatile mali_mutex_handle * pHandle)
{
	return _mali_base_arch_sys_mutex_auto_init(pHandle);
}

MALI_EXPORT mali_mutex_handle _mali_sys_mutex_static(mali_static_mutex id)
{
	return _mali_base_arch_sys_mutex_static(id);
}

MALI_EXPORT mali_mutex_handle _mali_sys_mutex_create(void)
{
	return _mali_base_arch_sys_mutex_create();
}

MALI_EXPORT void _mali_sys_mutex_destroy(mali_mutex_handle mutex)
{
	_mali_base_arch_sys_mutex_destroy(mutex);
}

MALI_EXPORT void _mali_sys_mutex_lock(mali_mutex_handle mutex)
{
	_mali_base_arch_sys_mutex_lock(mutex);
}

MALI_EXPORT mali_err_code _mali_sys_mutex_try_lock(mali_mutex_handle mutex)
{
	return _mali_base_arch_sys_mutex_try_lock(mutex);
}

MALI_EXPORT void _mali_sys_mutex_unlock(mali_mutex_handle mutex)
{
	_mali_base_arch_sys_mutex_unlock(mutex);
}

MALI_EXPORT mali_lock_handle _mali_sys_lock_create(void)
{
	return _mali_base_arch_sys_lock_create();
}

MALI_EXPORT mali_err_code _mali_sys_lock_auto_init(volatile mali_lock_handle * pHandle)
{
	return _mali_base_arch_sys_lock_auto_init(pHandle);
}

MALI_EXPORT void _mali_sys_lock_destroy(mali_lock_handle lock)
{
	_mali_base_arch_sys_lock_destroy(lock);
}

MALI_EXPORT void _mali_sys_lock_lock(mali_lock_handle lock)
{
	_mali_base_arch_sys_lock_lock(lock);
}

MALI_EXPORT mali_err_code _mali_sys_lock_timed_lock(mali_lock_handle lock, u64 timeout)
{
	return _mali_base_arch_sys_lock_timed_lock(lock, timeout);
}

MALI_EXPORT mali_err_code _mali_sys_lock_try_lock(mali_lock_handle lock)
{
	return _mali_base_arch_sys_lock_try_lock(lock);
}

MALI_EXPORT void _mali_sys_lock_unlock(mali_lock_handle lock)
{
	_mali_base_arch_sys_lock_unlock(lock);
}

MALI_EXPORT mali_err_code  _mali_sys_thread_key_set_data(mali_thread_keys key, void* value)
{
	return _mali_base_arch_sys_thread_key_set_data(key, value);
}

MALI_EXPORT void* _mali_sys_thread_key_get_data(mali_thread_keys key)
{
	return _mali_base_arch_sys_thread_key_get_data(key);
}

MALI_EXPORT u32 _mali_sys_thread_get_current(void)
{
	return _mali_base_arch_sys_thread_get_current();
}


MALI_EXPORT void _mali_sys_abort(void)
{
	_mali_base_arch_sys_abort();
}

MALI_EXPORT void _mali_sys_break(void)
{
	_mali_base_arch_sys_break();
}

#ifndef MALI_HAVE_INLINED_ATOMICS

MALI_EXPORT void _mali_sys_atomic_inc(mali_atomic_int * val)
{
	_mali_base_arch_sys_atomic_inc(val);
}

MALI_EXPORT u32 _mali_sys_atomic_inc_and_return(mali_atomic_int * val)
{
	return _mali_base_arch_sys_atomic_inc_and_return(val);
}

MALI_EXPORT void _mali_sys_atomic_dec(mali_atomic_int * val)
{
	_mali_base_arch_sys_atomic_dec(val);
}

MALI_EXPORT u32 _mali_sys_atomic_dec_and_return(mali_atomic_int * val)
{
	return _mali_base_arch_sys_atomic_dec_and_return(val);
}

MALI_EXPORT u32 _mali_sys_atomic_get(mali_atomic_int * atomic)
{
	return _mali_base_arch_sys_atomic_get(atomic);
}

MALI_EXPORT void _mali_sys_atomic_set(mali_atomic_int * atomic, u32 value)
{
	_mali_base_arch_sys_atomic_set(atomic, value);
}

MALI_EXPORT void _mali_sys_atomic_initialize(mali_atomic_int * atomic, u32 value)
{
	_mali_base_arch_sys_atomic_init(atomic, value);
}

#endif /* MALI_HAVE_INLINED_ATOMICS */

MALI_EXPORT mali_library * _mali_sys_library_load(const char * name)
{
	return _mali_base_arch_library_load(name);
}

MALI_EXPORT void _mali_sys_library_unload(mali_library * handle)
{
	_mali_base_arch_library_unload(handle);
}

MALI_EXPORT mali_bool _mali_sys_library_init(mali_library * handle, void* param, u32* retval)
{
	return _mali_base_arch_library_init(handle, param, retval);
}
