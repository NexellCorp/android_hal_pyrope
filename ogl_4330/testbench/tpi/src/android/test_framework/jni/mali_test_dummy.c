/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#if MALI_ANDROID_API >= 15
#include <ui/ANativeObjectBase.h>
#else
 #include <ui/android_native_buffer.h>
#endif
#include <android/log.h>
#include <hardware/hardware.h>

#include "../mali_tpi_android_framework.h"

int main(int argc, char **argv)
{
	/* Call our Java code to make sure everything is working */
	ANativeWindow *win = mali_test_new_native_window( 20, 40, 0 );
	mali_test_resize_native_window(win, 30, 50, 0);
	mali_test_delete_native_window(win);
	return 0;
}
