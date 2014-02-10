/*
 * This confidential and proprietary software may be used only as
 * authorized by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_image_system_android.cpp
 * @brief Android backend for image_system extension
 */

#if EGL_KHR_image_system_ENABLE
void __egl_remove_client_name_handle( egl_display *display )
{
	MALI_IGNORE( display );
}

EGLClientNameKHR __egl_get_client_name( egl_display *display, PidType pid )
{
	MALI_IGNORE( display );
	MALI_IGNORE( pid );
	return EGL_NO_CLIENT_NAME_KHR;
}

egl_client_handles* __egl_get_add_client_name_handles( egl_display *display )
{
	MALI_IGNORE( display );
	return NULL;
}

EGLClientNameKHR __egl_add_client_name( egl_display *display, PidType pid )
{
	MALI_IGNORE( display );
	MALI_IGNORE( pid );
	return EGL_NO_CLIENT_NAME_KHR;
}

EGLBoolean __egl_verify_client_name_valid( egl_display *display, EGLClientNameKHR target_client )
{
	MALI_IGNORE( display );
	MALI_IGNORE( target_client );
	return EGL_FALSE;
}

egl_image_name_handles* __egl_get_add_image_name_handles( egl_display *display )
{
	MALI_IGNORE( display );
	return NULL;
}

EGLImageNameKHR __egl_get_image_name( egl_display *display, egl_image *image, EGLClientNameKHR target_client )
{
	MALI_IGNORE( display );
	MALI_IGNORE( image );
	MALI_IGNORE( target_client );
	return EGL_NO_IMAGE_NAME_KHR;
}

EGLImageNameKHR __egl_add_image_name( egl_display *display, egl_image *image, EGLClientNameKHR target_client )
{
	MALI_IGNORE( display );
	MALI_IGNORE( image );
	MALI_IGNORE( target_client );
	return EGL_NO_IMAGE_NAME_KHR;
}

void __egl_remove_image_name_handle( struct egl_display *display )
{
	MALI_IGNORE( display );
}

EGLClientBuffer __egl_verify_convert_image_name_to_client_buffer( struct egl_display *display, EGLImageNameKHR buffer, EGLenum *target_type, EGLint *err )
{
	MALI_IGNORE( display );
	MALI_IGNORE( buffer );
	MALI_IGNORE( target_type );
	MALI_IGNORE( err );
	return (EGLClientBuffer)0;
}
#endif /* EGL_KHR_image_system_ENABLE */
