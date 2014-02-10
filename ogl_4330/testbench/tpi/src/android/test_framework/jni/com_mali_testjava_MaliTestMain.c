/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <jni.h>
#include <errno.h>
#include <stdint.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#if MALI_ANDROID_API >= 15
#include <ui/ANativeObjectBase.h>
#else
 #include <ui/android_native_buffer.h>
#endif
#include <hardware/hardware.h>
#include "../mali_tpi_android_framework.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "mali_test:native", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "mali_test:native", __VA_ARGS__))

extern int main(int argc, char **argv);
extern void set_native_window(ANativeWindow *theWindow);

#define GET_WINDOW_SURFACE_NAME "getWindowSurface"
#define GET_WINDOW_SURFACE_SIG  "(III)Landroid/view/Surface;"

jclass     app_class;
jmethodID  getWindowSurface_id;
JNIEnv    *app_env;
jobject    app_caller;

ANativeWindow *mali_test_new_native_window(int width, int height, int format)
{
	jobject surface;
	ANativeWindow *native_win;
	ANativeWindow *retval = NULL;
	android_native_buffer_t *buffer;

	surface = (*app_env)->CallObjectMethod(app_env, app_caller, getWindowSurface_id, width, height, format);
	native_win = ANativeWindow_fromSurface(app_env, surface);

	if (NULL == native_win)
	{
		goto finish;
	}

	if (native_win->dequeueBuffer(native_win, &buffer) == 0)
	{
		native_win->cancelBuffer(native_win, buffer);
		retval = native_win;
	}
	else
	{
		ANativeWindow_release(native_win);
	}

finish:
	return retval;
}

mali_tpi_bool mali_test_resize_native_window(ANativeWindow *win, int width, int height, int format)
{
	jobject surface;
	ANativeWindow *native_win;
	mali_tpi_bool retval = MALI_TPI_FALSE;

	surface = (*app_env)->CallObjectMethod(app_env, app_caller, getWindowSurface_id, width, height, format);
	native_win = ANativeWindow_fromSurface(app_env, surface);

	if (NULL == native_win)
	{
		goto finish;
	}

	/* We already held a reference and ANativeWindow_fromSurface adds another, so release that new one */
	ANativeWindow_release(native_win);
	retval = MALI_TPI_TRUE;

finish:
	return retval;
}

mali_tpi_bool mali_test_delete_native_window(ANativeWindow *win)
{
	ANativeWindow_release(win);

	return MALI_TPI_TRUE;
}

/**
 * Entry point for native code
 */
JNIEXPORT void JNICALL Java_com_mali_testjava_MaliTestMain_malitestmain
  (JNIEnv *env, jobject caller, jint argc, jobjectArray argv_array)
{
	char *argv[argc];
	int i;
	int ret;
	/* Set up globals */
	app_env = env;
	app_caller = caller;
	app_class = (*app_env)->GetObjectClass(app_env, app_caller);
	getWindowSurface_id = (*env)->GetMethodID(env, app_class, GET_WINDOW_SURFACE_NAME, GET_WINDOW_SURFACE_SIG);
	if (getWindowSurface_id == 0)
	{
		LOGW("Could not find getWindowSurface function!");
		return;
	}


	for(i=0; i<argc; i++)
	{
		jstring element = (jstring) (*env)->GetObjectArrayElement(env, argv_array, i);
		argv[i] = (char*) (*env)->GetStringUTFChars(env, element, NULL);
	}

	ret = main(argc, argv);
	if( ret != 0 )
	{
		LOGW("Test returned error code %d", ret);
	}

	/* Clean-up for Java */
	for(i=0; i<argc; i++)
	{
		jstring element = (jstring) (*env)->GetObjectArrayElement(env, argv_array, i);
		(*env)->ReleaseStringUTFChars(env, element, argv[i]);
	}

}
