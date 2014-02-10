/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifdef __cplusplus
extern "C"
{
#endif

#if MALI_ANDROID_API >= 15
#include <ui/ANativeObjectBase.h>
#else
 #include <ui/egl/android_natives.h>
#endif
#include <tpi/mali_tpi.h>

ANativeWindow *mali_test_new_native_window(int width, int height, int format);

mali_tpi_bool mali_test_resize_native_window(ANativeWindow *win, int width, int height, int format);

mali_tpi_bool mali_test_delete_native_window(ANativeWindow *win);

#ifdef __cplusplus
}
#endif
