/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "egl_test_parameters.h"
#include "egl_helpers.h"

void egl_test_set_window_pixel_format( suite* test_suite, EGLint red, EGLint green, EGLint blue, EGLint alpha )
{
    egl_test_parameters* tp = (egl_test_parameters*)test_suite->test_spec->api_parameters;
    tp->window_pixel_format.red_size = red;
    tp->window_pixel_format.green_size = green;
    tp->window_pixel_format.blue_size = blue;
    tp->window_pixel_format.alpha_size = alpha;
}

void egl_test_get_requested_window_pixel_format( suite* test_suite, EGLint* red, EGLint* green, EGLint* blue, EGLint* alpha )
{
	egl_test_parameters* tp = (egl_test_parameters*)test_suite->test_spec->api_parameters;
	if ( NULL != red ) *red = tp->window_pixel_format.red_size;
	if ( NULL != green ) *green = tp->window_pixel_format.green_size;
	if ( NULL != blue ) *blue = tp->window_pixel_format.blue_size;
	if ( NULL != alpha ) *alpha = tp->window_pixel_format.alpha_size;
}

EGLBoolean egl_test_verify_config_matches_requested_window_pixel_format( suite* test_suite, EGLDisplay display, EGLConfig config )
{
	EGLint req[4];
	static const EGLint attributes[] = {EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_ALPHA_SIZE};
	int i;

	egl_test_get_requested_window_pixel_format( test_suite, &req[0], &req[1], &req[2], &req[3] );
	
	for ( i=0; i<4; ++i)
	{
		EGLBoolean status;
		EGLint value;
		status = eglGetConfigAttrib( display, config, attributes[i], &value );
		if ( EGL_TRUE != status )
		{
			ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglGetConfigAttrib failed" );
			return EGL_FALSE;
		}
		if ( (EGL_DONT_CARE != req[i]) && (value != req[i]) ) return EGL_FALSE;
	}
	return EGL_TRUE;
}

static unsigned int count_attributes( EGLint const* attributes )
{
        unsigned int num_att = 0;
        if ( NULL == attributes ) return 0;
        for (; EGL_NONE != *attributes; attributes += 2, ++num_att );
        return num_att;
}

static EGLint* find_attribute( const EGLint attribute, EGLint* attribute_list )
{
	if ( NULL == attribute_list ) return NULL;

	for (; *attribute_list != EGL_NONE; attribute_list += 2 )
	{
		if ( attribute == *attribute_list ) return attribute_list;
	}

	return NULL; /* not found */
}

static __inline unsigned int add_attribute( const EGLint* attribute )
{
	return attribute == NULL ? 1 : 0;
}

static __inline void add_attribute_if_missing( const EGLint attribute, EGLint** att_pos, EGLint** next_pos )
{
	if ( NULL == *att_pos )
	{
		/* Update attribute pointer */
		*att_pos = *next_pos;
		/* Store attribute */
		**att_pos = attribute;
		/* Initialize attribute value to 0 */
		*((*att_pos) + 1) = 0;
		/* Update next_pos to next available slot */
		*next_pos += 2;
	}
}

EGLBoolean egl_test_get_window_config_with_attributes( suite* test_suite, EGLDisplay display, EGLint* attributes, EGLConfig* out_config )
{
	/* pointers to the attributes in the attribute list*/
	EGLint* surface_type = find_attribute( EGL_SURFACE_TYPE, attributes );
	EGLint* red_size = find_attribute( EGL_RED_SIZE, attributes );
	EGLint* green_size = find_attribute( EGL_GREEN_SIZE, attributes );
	EGLint* blue_size = find_attribute( EGL_BLUE_SIZE, attributes );
	EGLint* alpha_size = find_attribute( EGL_ALPHA_SIZE, attributes );
	const unsigned int num_given_attributes = count_attributes( attributes );
	const unsigned int additional_attributes =
		add_attribute( surface_type ) +
		add_attribute( red_size ) +
		add_attribute( green_size ) +
		add_attribute( blue_size ) +
		add_attribute( alpha_size );

	EGLint* choose_config_attributes;
	EGLBoolean retval = EGL_FALSE;
	/* points to the attribute following the attributes specified by the caller. Used for adding additional attributes later */
	EGLint* next_ptr;

	EGLint red, green, blue, alpha;

	EGLConfig* configs = NULL;
	EGLint num_configs;
	EGLint i;
	EGLBoolean status;

	choose_config_attributes = (EGLint*)CALLOC( ((num_given_attributes + additional_attributes) * 2 + 1), sizeof(EGLint) );
	ASSERT_POINTER_NOT_NULL( choose_config_attributes );
	if ( NULL == choose_config_attributes ) goto exit;

	/* copy given attributes to our newly allocated list */
	if ( NULL != attributes )
	{
		/* We will also copy attribute termination hint EGL_NONE */
		memcpy( choose_config_attributes, attributes, sizeof(EGLint)*2*num_given_attributes + 1 );
	}
	/* add sentinel */
	choose_config_attributes[(num_given_attributes + additional_attributes)*2] = EGL_NONE;

	surface_type = find_attribute( EGL_SURFACE_TYPE, choose_config_attributes );
	red_size = find_attribute( EGL_RED_SIZE, choose_config_attributes );
	green_size = find_attribute( EGL_GREEN_SIZE, choose_config_attributes );
	blue_size = find_attribute( EGL_BLUE_SIZE, choose_config_attributes );
	alpha_size = find_attribute( EGL_ALPHA_SIZE, choose_config_attributes );

	/* next_ptr points to the end of the provided attribute */
	next_ptr = choose_config_attributes + num_given_attributes * 2;
	add_attribute_if_missing( EGL_SURFACE_TYPE, &surface_type, &next_ptr );
	add_attribute_if_missing( EGL_RED_SIZE, &red_size, &next_ptr );
	add_attribute_if_missing( EGL_GREEN_SIZE, &green_size, &next_ptr );
	add_attribute_if_missing( EGL_BLUE_SIZE, &blue_size, &next_ptr );
	add_attribute_if_missing( EGL_ALPHA_SIZE, &alpha_size, &next_ptr );

	egl_test_get_requested_window_pixel_format( test_suite, &red, &green, &blue, &alpha );
	/* Patch up attribute list to contain the values we want */
	*(surface_type + 1) |= EGL_WINDOW_BIT;
	*(red_size + 1) = red;
	*(green_size + 1) = green;
	*(blue_size + 1) = blue;
	*(alpha_size + 1) = alpha;

	status = eglChooseConfig( display, choose_config_attributes, NULL, 0, &num_configs );
	if ( EGL_FALSE == status )
	{
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglChooseConfig failed" );
		goto exit;
	}
	if ( 0 == num_configs )
	{
		goto exit;
	}

	configs = CALLOC( num_configs, sizeof(EGLConfig) );
	if ( NULL == configs )
	{
		ASSERT_EGL_EQUAL(0, 1, "Out of memory");
		goto exit;
	}
	status = eglChooseConfig( display, choose_config_attributes, configs, num_configs, &num_configs );
	if ( (EGL_FALSE == status) || (0 == num_configs) )
	{
		ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglChooseConfig failed" );
		goto exit;
	}
	for (i=0; i<num_configs; ++i)
	{
		status = egl_test_verify_config_matches_requested_window_pixel_format( test_suite, display, configs[i] );
		if ( EGL_TRUE == status )
		{
			retval = EGL_TRUE;
			*out_config = configs[i];
			goto exit;
		}
	}

	/* No config found */
exit:
	if ( NULL != choose_config_attributes )
	{
		FREE( choose_config_attributes );
	}
	if ( NULL != configs )
	{
		FREE( configs );
	}
	return retval;
}

EGLBoolean egl_test_get_window_config( suite* test_suite, EGLDisplay dpy, EGLint renderable_type, EGLConfig* out_config )
{
	EGLConfig config;
	egl_test_parameters* tp = (egl_test_parameters*)test_suite->test_spec->api_parameters;

	config = egl_helper_get_config_exact(
		test_suite,
		dpy,
		renderable_type,
		EGL_WINDOW_BIT,
		tp->window_pixel_format.red_size,
		tp->window_pixel_format.green_size,
		tp->window_pixel_format.blue_size,
		tp->window_pixel_format.alpha_size,
		NULL /* no pixmap matching */
	);

	if ( (EGLConfig)NULL == config )
		return EGL_FALSE;

	*out_config = config;
	return EGL_TRUE;
}

