/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_runtime_stdarg.h>
#include <base/arch/base_arch_runtime.h>

#if MALI_HOSTLIB_INDIRECT == 1
MALI_EXPORT int _mali_sys_vsnprintf(char* str, u32 size, const char * format, va_list ap )
{
	return _mali_base_arch_sys_snprintf(str, size, format, ap);
}

MALI_EXPORT int _mali_sys_vfprintf(mali_file *file, const char *format, va_list ap)
{
	return _mali_base_arch_sys_fprintf(file, format, ap);
}
#endif

