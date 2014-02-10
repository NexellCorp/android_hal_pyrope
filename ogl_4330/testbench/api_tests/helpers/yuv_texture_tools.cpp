/*
 * $Copyright:
 * ----------------------------------------------------------------
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 * ----------------------------------------------------------------
 * $
 */
#if defined(__SYMBIAN32__) 
#include "egl_helpers.h"
#include "test_helpers.h"
#endif

#ifndef ANDROID
#include <shared/mali_image.h>
#else
/* TODO: remove this, taken from mali_image.h*/
/* EGL_IMAGE_FORMAT_KHR values */
/* YUV formats */
#define EGL_YUV420P_KHR    0x30F1
#define EGL_YUV420SP_KHR   0x30F2
#define EGL_YVU420SP_KHR   0x30F3
#define EGL_YUV422I_KHR    0x30F4
#define EGL_YUV422P_KHR    0x30F5
#define EGL_YUV444P_KHR    0x30F6
#define EGL_YUV444I_KHR    0x30F7
#define EGL_YV12_KHR       0x30F8
/* EGL_COLOR_RANGE_KHR values */
#define EGL_REDUCED_RANGE_KHR 0x30F9
#define EGL_FULL_RANGE_KHR    0x30FA
/* EGL_COLORSPACE additional values */
#define EGL_COLORSPACE_BT_601  0x30EC
#define EGL_COLORSPACE_BT_709  0x30ED
#endif

#include "yuv_texture_tools.h"
#include "egl_image_tools.h"
#include "suite.h"

#ifdef ANDROID
#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#endif
#include <ui/GraphicBuffer.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#if !defined(__SYMBIAN32__)
#include <pthread.h>
#endif
#include "../egl/egl_helpers_pixmap.h"

void get_yuv_values( yuv_values *values, helper_yuv_format *info )
{

    if ( EGL_FULL_RANGE_KHR == info->range )
    {
        values->l_max = 255;
        values->l_min = 0;
        values->c_max = 255;
        values->c_min = 0;
    }
    else
    {
        values->l_max = 235;
        values->l_min = 16;
        values->c_max = 239;
        values->c_min = 16;
    }

    /* calculate max/min/mid-points/quarter-points */
    values->l_mid     = values->l_min + (values->l_max-values->l_min)/2;
    values->l_quarter = values->l_min + (values->l_max-values->l_min)/4;
    values->c_mid     = values->c_min + (values->c_max-values->c_min)/2;
}

void scale_pattern_to_target(
    yuv_image *image,
    int width, int height,
    u8 *pattern_l, u8 *pattern_cr, u8 *pattern_cb,
    int pattern_width, int pattern_height )
{
    int x,y;
    int xdiv, ydiv;
    xdiv = width/pattern_width;
    ydiv = height/pattern_height;
    for ( y = 0; y < height; y++ )
    {
        for ( x = 0; x < width; x++ )
        {
            image->plane[0][y*width+x] = pattern_l[  (y/ydiv)*pattern_width + (x/xdiv) ];
            image->plane[1][y*width+x] = pattern_cr[ (y/ydiv)*pattern_width + (x/xdiv) ];
            image->plane[2][y*width+x] = pattern_cb[ (y/ydiv)*pattern_width + (x/xdiv) ];
        }
    }
}

static void downsample_yuv444_to_yuv420_planar( yuv_image *dst, yuv_image *src, int width, int height )
{
    int x,y;
    int halfwidth = width/2;

    /* keep luma as is */
    mali_tpi_memcpy( dst->plane[0], src->plane[0], width*height );

    /* downsample chroma in both directions */
    for ( y = 0; y < height; y+=2 )
    {
        for ( x = 0; x < width; x+=2 )
        {
            dst->plane[1][(y/2)*halfwidth+x/2] = src->plane[1][y*width+x];
            dst->plane[2][(y/2)*halfwidth+x/2] = src->plane[2][y*width+x];
        }
    }
}

static void downsample_yuv444_to_yuv420_semiplanar( yuv_image *dst, yuv_image *src, EGLint width, EGLint height )
{
    int x,y;
    int halfwidth = width/2;

    /* keep luma as is */
    mali_tpi_memcpy( dst->plane[0], src->plane[0], width*height );

    /* downsample chroma in both directions, and interleave the two planes */
    for ( y = 0; y < height; y+=2 )
    {
        for ( x = 0; x < width; x+=2 )
        {
            dst->plane[1][ ((y/2)*halfwidth+x/2)*2 + 0 ] = src->plane[1][y*width+x];
            dst->plane[1][ ((y/2)*halfwidth+x/2)*2 + 1 ] = src->plane[2][y*width+x];
        }
    }
}

static void downsample_yuv444_to_yuv422_interleaved( yuv_image *dst, yuv_image *src, EGLint width, EGLint height )
{
    int x,y = 0;
    int ofs = 0;

    /* downsample chroma in the horizonta direction, and interleave all three planes */
    for ( y = 0; y < height; y++ )
    {
        for ( x = 0; x < width; x+=2 )
        {
            u8 luma1 = src->plane[0][y*width+x];
            u8 luma2 = src->plane[0][y*width+x+1];
            u8 cb = src->plane[1][y*width+x];
            u8 cr = src->plane[2][y*width+x+1];

            dst->plane[0][ ofs + 0 ] = luma1;
            dst->plane[0][ ofs + 1 ] = cb;
            dst->plane[0][ ofs + 2 ] = luma2;
            dst->plane[0][ ofs + 3 ] = cr;
            ofs +=4;
        }
    }
}

void downsample_yuv444( yuv_image *dst, yuv_image *src, EGLint width, EGLint height, helper_yuv_format *info )
{
    switch (info->format)
    {
        case EGL_YUV420P_KHR:
            downsample_yuv444_to_yuv420_planar( dst, src, width, height );
            break;

        case EGL_YUV420SP_KHR:
        case EGL_YVU420SP_KHR:
            downsample_yuv444_to_yuv420_semiplanar( dst, src, width, height );
            break;

        case EGL_YUV422I_KHR:
            downsample_yuv444_to_yuv422_interleaved( dst, src, width, height );
            break;
    }
}

void convert_yuv444_to_rgb_subset( float *rgb, yuv_image *yuv, int width, int height, int pitch, int xoffset, int yoffset, helper_yuv_format *info )
{
    int x,y;
    float conversion_matrix_601_full_range[] = {
        1.000000000f,  1.401999950f,  0.000000000f, -0.700999975f,
        1.000000000f, -0.714136243f, -0.344136268f,  0.529136240f,
        1.000000000f,  0.000000000f,  1.771999955f, -0.885999978f,
        0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f
    };
    float conversion_matrix_601_reduced_range[] = {
        1.164383531f,  1.596026659f,  0.000000000f, -0.874202192f,
        1.164383531f, -0.812967539f, -0.391762257f,  0.531667769f,
        1.164383531f,  0.000000000f,  2.017231941f, -1.085630655f,
        0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f
    };


    float conversion_matrix_709_full_range[] =
    {
        1.000000000f,  1.574800014f,  0.000000000f, -0.787400007f,
        1.000000000f, -0.468124270f, -0.187324256f,  0.327724278f,
        1.000000000f,  0.000000000f,  1.855599999f, -0.927800000f,
        0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f
    };
    float conversion_matrix_709_reduced_range[] =
    {
        1.164383531f,  1.792741060f,  0.000000000f, -0.972945154f,
        1.164383531f, -0.532909274f, -0.213248581f,  0.301482677f,
        1.164383531f,  0.000000000f,  2.112401724f, -1.133402228f,
        0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f
    };

    float *matrix;

    if ( EGL_COLORSPACE_BT_601 == info->colorspace )
    {
        if ( EGL_FULL_RANGE_KHR == info->range ) matrix = conversion_matrix_601_full_range;
        else matrix = conversion_matrix_601_reduced_range;
    }
    else
    {
        if ( EGL_FULL_RANGE_KHR == info->range ) matrix = conversion_matrix_709_full_range;
        else matrix = conversion_matrix_709_reduced_range;
    }

    for ( y = yoffset; y < yoffset + height; y++ )
    {
        for ( x = xoffset; x < xoffset + width; x++ )
        {
            float luma = (yuv->plane[0][y*pitch+x] / 255.0f);
            float cr = (yuv->plane[1][y*pitch+x] / 255.0f);
            float cb = (yuv->plane[2][y*pitch+x] / 255.0f);
            float r = 0, g = 0, b = 0;

            r = luma*matrix[0] + cr*matrix[2] + cb*matrix[1] + matrix[3];
            g = luma*matrix[4] + cr*matrix[6] + cb*matrix[5] + matrix[7];
            b = luma*matrix[8] + cr*matrix[10] + cb*matrix[9] + matrix[11];

            /* clamp the values */
            if ( r < 0 ) r = 0;
            if ( r > 1.0f ) r = 1.0f;

            if ( g < 0 ) g = 0;
            if ( g > 1.0f ) g = 1.0f;

            if ( b < 0 ) b = 0;
            if ( b > 1.0f ) b = 1.0f;

            rgb[(y*pitch+x)*3+0] = r;
            rgb[(y*pitch+x)*3+1] = g;
            rgb[(y*pitch+x)*3+2] = b;
        }
    }
}

void convert_yuv444_to_rgb( float *rgb, yuv_image *yuv, int width, int height, helper_yuv_format *info )
{
    int x,y;
    float conversion_matrix_601_full_range[] = {
        1.000000000f,  1.401999950f,  0.000000000f, -0.700999975f,
        1.000000000f, -0.714136243f, -0.344136268f,  0.529136240f,
        1.000000000f,  0.000000000f,  1.771999955f, -0.885999978f,
        0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f
    };
    float conversion_matrix_601_reduced_range[] = {
        1.164383531f,  1.596026659f,  0.000000000f, -0.874202192f,
        1.164383531f, -0.812967539f, -0.391762257f,  0.531667769f,
        1.164383531f,  0.000000000f,  2.017231941f, -1.085630655f,
        0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f
    };


    float conversion_matrix_709_full_range[] =
    {
        1.000000000f,  1.574800014f,  0.000000000f, -0.787400007f,
        1.000000000f, -0.468124270f, -0.187324256f,  0.327724278f,
        1.000000000f,  0.000000000f,  1.855599999f, -0.927800000f,
        0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f
    };
    float conversion_matrix_709_reduced_range[] =
    {
        1.164383531f,  1.792741060f,  0.000000000f, -0.972945154f,
        1.164383531f, -0.532909274f, -0.213248581f,  0.301482677f,
        1.164383531f,  0.000000000f,  2.112401724f, -1.133402228f,
        0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f
    };

    float *matrix;

    if ( EGL_COLORSPACE_BT_601 == info->colorspace )
    {
        if ( EGL_FULL_RANGE_KHR == info->range ) matrix = conversion_matrix_601_full_range;
        else matrix = conversion_matrix_601_reduced_range;
    }
    else
    {
        if ( EGL_FULL_RANGE_KHR == info->range ) matrix = conversion_matrix_709_full_range;
        else matrix = conversion_matrix_709_reduced_range;
    }

    for ( y = 0; y < height; y++ )
    {
        for ( x = 0; x < width; x++ )
        {
            float luma = (yuv->plane[0][y*width+x] / 255.0f);
            float cr = (yuv->plane[1][y*width+x] / 255.0f);
            float cb = (yuv->plane[2][y*width+x] / 255.0f);
            float r = 0, g = 0, b = 0;

            r = luma*matrix[0] + cr*matrix[2] + cb*matrix[1] + matrix[3];
            g = luma*matrix[4] + cr*matrix[6] + cb*matrix[5] + matrix[7];
            b = luma*matrix[8] + cr*matrix[10] + cb*matrix[9] + matrix[11];

            /* clamp the values */
            if ( r < 0 ) r = 0;
            if ( r > 1.0f ) r = 1.0f;

            if ( g < 0 ) g = 0;
            if ( g > 1.0f ) g = 1.0f;

            if ( b < 0 ) b = 0;
            if ( b > 1.0f ) b = 1.0f;

            rgb[(y*width+x)*3+0] = r;
            rgb[(y*width+x)*3+1] = g;
            rgb[(y*width+x)*3+2] = b;
        }
    }
}

#ifdef ANDROID
using namespace android;

#define C_R 0.0
#define C_G 0.0
#define C_B 1.0


EGLImageKHR create_egl_image_yuv( suite *test_suite, yuv_image *yuv_source_image, EGLDisplay dpy, EGLint width, EGLint height, helper_yuv_format *info ){
	const int yuvTexFormat = HAL_PIXEL_FORMAT_YV12;
	const int yuvTexUsage = GraphicBuffer::USAGE_HW_TEXTURE | GraphicBuffer::USAGE_SW_WRITE_RARELY;
	sp<GraphicBuffer> yuvTexBuffer = new GraphicBuffer( width, height, yuvTexFormat, yuvTexUsage );
	const int yuvTexOffsetY = 0;
	int yuvTexStrideY = yuvTexBuffer->getStride();
	int yuvTexOffsetV = yuvTexStrideY * height;
	int yuvTexStrideV = (yuvTexStrideY/2 + 0xf) & ~0xf;
	int yuvTexOffsetU = yuvTexOffsetV + yuvTexStrideV * height/2;
	int yuvTexStrideU = yuvTexStrideV;
	char* buf = NULL;
	status_t err = yuvTexBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)(&buf));

	if(!buf){
		return EGL_NO_IMAGE_KHR;
	}

	unsigned char *val_y = yuv_source_image->plane[0];
	unsigned char *val_u = yuv_source_image->plane[1];
	unsigned char *val_v = yuv_source_image->plane[2];	

#if 0
	double c_y = 0.0 + 0.299*C_R + 0.587*C_G + 0.1440*C_B;
	double c_u = 0.5 - 0.169*C_R - 0.331*C_G + 0.4990*C_B;
	double c_v = 0.5 + 0.499*C_R - 0.418*C_G - 0.0813*C_B;

	unsigned char val_y = (unsigned char) (c_y * 255.0);
	unsigned char val_u = (unsigned char) (c_u * 255.0);
	unsigned char val_v = (unsigned char) (c_v * 255.0);
#endif

	int temp = 0;
	unsigned char *temp_ptr = NULL;
	switch( info->format ){
		case EGL_YUV420P_KHR:
			/* swap u and v offsets and source planes*/
			temp = yuvTexOffsetV;
			yuvTexOffsetV = yuvTexOffsetU;
			yuvTexOffsetU = temp;
			
			temp_ptr = val_u;
			val_u = val_v;
			val_v = temp_ptr;
			break;
		case EGL_YV12_KHR:
			/* all good */
			break;
		default:
			/* Unsupported format */
			return EGL_NO_IMAGE_KHR;
	}

	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			buf[yuvTexOffsetY + (y * yuvTexStrideY) + x] = val_y[ x + y*width];
			if (x < width/2 && y < height/2) {
				buf[yuvTexOffsetU + (y * yuvTexStrideU) + x] = val_u[ x + y*width/2];
				buf[yuvTexOffsetV + (y * yuvTexStrideV) + x] = val_v[ x + y*width/2];
			}
		}
	}

	err = yuvTexBuffer->unlock();
	if( err < 0 ){
		return EGL_NO_IMAGE_KHR;
	}

	EGLClientBuffer clientBuffer = (EGLClientBuffer)yuvTexBuffer->getNativeBuffer();
	EGLImageKHR img = eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, 0);
	
	return img;
}

#else

EGLImageKHR create_egl_image_yuv( suite *test_suite, EGLDisplay dpy, EGLint width, EGLint height, helper_yuv_format *info )
{
    EGLImageKHR egl_image = (EGLImageKHR)NULL;
    EGLint range = ( EGL_REDUCED_RANGE_KHR == info->range )? MALI_EGL_IMAGE_RANGE_REDUCED : MALI_EGL_IMAGE_RANGE_FULL;
    EGLint attribs[11];

    attribs[0]= MALI_EGL_IMAGE_WIDTH;
    attribs[1]= width;
    attribs[2]= MALI_EGL_IMAGE_HEIGHT;
    attribs[3]= height;
    attribs[4]= MALI_EGL_IMAGE_FORMAT;
    attribs[5]= info->format;
    attribs[6]= MALI_EGL_IMAGE_RANGE;
    attribs[7]= range;
    attribs[8]= MALI_EGL_IMAGE_COLORSPACE;
    attribs[9]= info->colorspace;
    attribs[10]= EGL_NONE;

    egl_image = mali_egl_image_create( dpy, attribs);

    return egl_image;
}

EGLBoolean destroy_egl_image_yuv( EGLDisplay dpy, EGLImageKHR image, EGLNativePixmapType pixmap )
{
	EGLBoolean status;

	egl_helper_free_nativepixmap( pixmap );

	status = _eglDestroyImageKHR( dpy, image );
	return status;
}

EGLBoolean write_yuv_data_to_image( EGLImageKHR egl_image, yuv_image *yuv_source_image, EGLint width, EGLint height, helper_yuv_format *info )
{
    mali_egl_image *img = mali_egl_image_lock_ptr( egl_image );

    if(NULL==img) return EGL_FALSE;

    switch ( info->format )
    {
        case EGL_YUV420P_KHR:
        {
            EGLint attribs_y[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_Y,
                EGL_NONE
            };
            EGLint attribs_u[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_U,
                EGL_NONE
            };
            EGLint attribs_v[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_V,
                EGL_NONE
            };
            void *luma, *cr, *cb;

            luma = mali_egl_image_map_buffer( img, attribs_y );
            if ( luma != NULL )
            {
                mali_tpi_memcpy( luma, yuv_source_image->plane[0], width*height );
            }
            mali_egl_image_unmap_buffer( img, attribs_y );

            cr = mali_egl_image_map_buffer( img, attribs_u );
            if ( cr != NULL )
            {
                mali_tpi_memcpy( cr, yuv_source_image->plane[1], width/2*height/2 );
            }
            mali_egl_image_unmap_buffer( img, attribs_u );

            cb = mali_egl_image_map_buffer( img, attribs_v );
            if ( cb != NULL )
            {
                mali_tpi_memcpy( cb, yuv_source_image->plane[2], width/2*height/2 );
            }
            mali_egl_image_unmap_buffer( img, attribs_v );
        }
        break;
        case EGL_YV12_KHR:
        {
            EGLint attribs_y[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_Y,
                EGL_NONE
            };
            EGLint attribs_u[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_U,
                EGL_NONE
            };
            EGLint attribs_v[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_V,
                EGL_NONE
            };
            void *luma, *cr, *cb;

            luma = mali_egl_image_map_buffer( img, attribs_y );
            if ( luma != NULL )
            {
                mali_tpi_memcpy( luma, yuv_source_image->plane[0], width*height );
            }
            mali_egl_image_unmap_buffer( img, attribs_y );

            cr = mali_egl_image_map_buffer( img, attribs_u );
            if ( cr != NULL )
            {
                mali_tpi_memcpy( cr, yuv_source_image->plane[2], width/2*height/2 );
            }
            mali_egl_image_unmap_buffer( img, attribs_u );

            cb = mali_egl_image_map_buffer( img, attribs_v );
            if ( cb != NULL )
            {
                mali_tpi_memcpy( cb, yuv_source_image->plane[1], width/2*height/2 );
            }
            mali_egl_image_unmap_buffer( img, attribs_v );
        }
        break;
        case EGL_YUV422P_KHR:
        {
            EGLint attribs_y[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_Y,
                EGL_NONE
            };
            EGLint attribs_u[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_U,
                EGL_NONE
            };
            EGLint attribs_v[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_V,
                EGL_NONE
            };
            void *luma, *cr, *cb;

            luma = mali_egl_image_map_buffer( img, attribs_y );
            if ( luma != NULL )
            {
                mali_tpi_memcpy( luma, yuv_source_image->plane[0], width*height );
            }
            mali_egl_image_unmap_buffer( img, attribs_y );

            cr = mali_egl_image_map_buffer( img, attribs_u );
            if ( cr != NULL )
            {
                mali_tpi_memcpy( cr, yuv_source_image->plane[1], height*width/2 );
            }
            mali_egl_image_unmap_buffer( img, attribs_u );

            cb = mali_egl_image_map_buffer( img, attribs_v );
            if ( cb != NULL )
            {
                mali_tpi_memcpy( cb, yuv_source_image->plane[2], height*width/2 );
            }
            mali_egl_image_unmap_buffer( img, attribs_v );
        }
        break;

        case EGL_YUV444P_KHR:
        {
            EGLint attribs_y[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_Y,
                EGL_NONE
            };
            EGLint attribs_u[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_U,
                EGL_NONE
            };
            EGLint attribs_v[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_V,
                EGL_NONE
            };
            void *luma, *cr, *cb;

            luma = mali_egl_image_map_buffer( img, attribs_y );
            if ( luma != NULL )
            {
                mali_tpi_memcpy( luma, yuv_source_image->plane[0], width*height );
            }
            mali_egl_image_unmap_buffer( img, attribs_y );

            cr = mali_egl_image_map_buffer( img, attribs_u );
            if ( cr != NULL )
            {
                mali_tpi_memcpy( cr, yuv_source_image->plane[1], height*width );
            }
            mali_egl_image_unmap_buffer( img, attribs_u );

            cb = mali_egl_image_map_buffer( img, attribs_v );
            if ( cb != NULL )
            {
                mali_tpi_memcpy( cb, yuv_source_image->plane[2], height*width );
            }
            mali_egl_image_unmap_buffer( img, attribs_v );
        }
        break;


        case EGL_YVU420SP_KHR:
        case EGL_YUV420SP_KHR:
        {
            EGLint attribs_y[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_Y,
                EGL_NONE
            };
            EGLint attribs_uv[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_UV,
                EGL_NONE
            };
            void *luma, *chroma;

            luma = mali_egl_image_map_buffer( img, attribs_y );
            if ( luma != NULL )
            {
                mali_tpi_memcpy( luma, yuv_source_image->plane[0], width*height );
            }
            mali_egl_image_unmap_buffer( img, attribs_y );

            chroma = mali_egl_image_map_buffer( img, attribs_uv );
            if ( chroma != NULL )
            {
                mali_tpi_memcpy( chroma, yuv_source_image->plane[1], width*height/2 );
            }
            mali_egl_image_unmap_buffer( img, attribs_uv );
        }
        break;

        case EGL_YUV422I_KHR:
        {
            EGLint attribs_yuv[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_YUV,
                EGL_NONE
            };
            void *yuv = mali_egl_image_map_buffer( img, attribs_yuv );

            if ( yuv != NULL )
            {
                mali_tpi_memcpy( yuv, yuv_source_image->plane[0], width*height*2 );
            }

            mali_egl_image_unmap_buffer( img, attribs_yuv );
        }
        break;

        case EGL_YUV444I_KHR:
        {
            EGLint attribs_yuv[] =
            {
                MALI_EGL_IMAGE_PLANE, MALI_EGL_IMAGE_PLANE_YUV,
                EGL_NONE
            };
            void *yuv = mali_egl_image_map_buffer( img, attribs_yuv );

            if ( yuv != NULL )
            {
                mali_tpi_memcpy( yuv, yuv_source_image->plane[0], width*height*3 );
            }

            mali_egl_image_unmap_buffer( img, attribs_yuv );
        }
        break;
    }

    mali_egl_image_unlock_ptr( egl_image );
    img = NULL;

    return EGL_TRUE;
}

#endif /* ... */ 

#if EGL_MALI_GLES && MALI_EGL_GLES_MAJOR_VERSION == 2

unsigned int create_texture_from_image( EGLImageKHR egl_image )
{
    GLuint tex_id;
    glGenTextures( 1, &tex_id );
    glBindTexture( GL_TEXTURE_EXTERNAL_OES, tex_id );
#if defined( __SYMBIAN32__ ) || defined( ANDROID )
    glEGLImageTargetTexture2DOES( GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)egl_image );
#else
    glEGLImageTargetTexture2DOES( GL_TEXTURE_EXTERNAL_OES, egl_image );
#endif
    return tex_id;
}

unsigned int create_2d_texture_from_image( EGLImageKHR egl_image )
{
    GLuint tex_id;
    glGenTextures( 1, &tex_id );
    glBindTexture( GL_TEXTURE_2D, tex_id );
#ifdef __SYMBIAN32__
    glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)egl_image );
#else
    glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, egl_image );
#endif
    return tex_id;
}

void render_yuv_texture( GLuint *tex, int num_textures, int width, int height )
{
    int i;
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    for ( i = 0; i < num_textures; i++ )
    {
        glActiveTexture( GL_TEXTURE0 + i );
        glBindTexture( GL_TEXTURE_EXTERNAL_OES, tex[i] );
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    load_program("pass_two_attribs.vert", "texture_yuv.frag");
    draw_normalized_textured_square(0, 0, (float)width, (float)height);
}

void render_yuv_texture_multiple_samplers( GLuint *tex, int num_textures, int width, int height )
{
    int i;
    GLint viewport[4];
    GLfloat vertices[4 * 4];
    GLint proggy;
    GLint pos;
    float x = 0;
    float y = 0;
    float w = width;
    float h = height;
    float s_offset = 0.0f;
    float t_offset = 0.0f;

    GLushort indices[] =
    {
        0, 1, 2,
        0, 2, 3
    };

#if defined(__ARMCC__)
    GLfloat texcoords[8];
    texcoords[0] = 0 + s_offset;
    texcoords[1] = 0 + t_offset;
    texcoords[2] = 0 + s_offset;
    texcoords[3] = 1 + t_offset;
    texcoords[4] = 1 + s_offset;
    texcoords[5] = 1 + t_offset;
    texcoords[6] = 1 + s_offset;
    texcoords[7] = 0 + t_offset;
#else
    GLfloat texcoords[] =
    {
        0 + s_offset, 0 + t_offset,
        0 + s_offset, 1 + t_offset,
        1 + s_offset, 1 + t_offset,
        1 + s_offset, 0 + t_offset
    };
#endif

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    load_program("pass_two_attribs.vert", "texture_yuv_multi.frag");

    glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
    glGetIntegerv(GL_VIEWPORT, viewport);

    /* bottom-left vertex */
    vertices[0*3+0] = (x + 0) / (viewport[2] / 2.0) - 1.0f;
    vertices[0*3+1] = (y + 0) / (viewport[3] / 2.0) - 1.0f;
    vertices[0*3+2] = 0.5;

    /* top-left vertex */
    vertices[1*3+0] = (x + 0) / (viewport[2] / 2.0) - 1.0f;
    vertices[1*3+1] = (y + h) / (viewport[3] / 2.0) - 1.0f;
    vertices[1*3+2] = 0.5;

    /* top-right vertex */
    vertices[2*3+0] = (x + w) / (viewport[2] / 2.0) - 1.0f;
    vertices[2*3+1] = (y + h) / (viewport[3] / 2.0) - 1.0f;
    vertices[2*3+2] = 0.5;

    /* bottom-right vertex */
    vertices[3*3+0] = (x + w) / (viewport[2] / 2.0) - 1.0f;
    vertices[3*3+1] = (y + 0) / (viewport[3] / 2.0) - 1.0f;
    vertices[3*3+2] = 0.5;

    for ( i=0; i<num_textures; i++ )
    {
        glActiveTexture( GL_TEXTURE0 + i );
        glBindTexture( GL_TEXTURE_EXTERNAL_OES, tex[i] );
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    pos = glGetAttribLocation(proggy, "in_tex0");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

    for ( i=0; i<num_textures; i++ )
    {
        char sampler[256];

        memset( sampler, 0, 256 );
        sprintf( sampler, "diffuse%i", i );
        pos = glGetUniformLocation( proggy, sampler );
        glUniform1i(pos, i);
    }

    pos = glGetAttribLocation(proggy, "position");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void render_yuv_texture_custom_conversion( int width, int height, int xofs, int yofs )
{
	GLint viewport[4];
	GLfloat vertices[4 * 4];
	GLint proggy;
	GLint pos;
	float x = xofs;
	float y = yofs;
	float w = width;
	float h = height;
	const float s_offset = 0.0f;
	const float	t_offset = 0.0f;

	GLushort indices[] =
	{
		0, 1, 2,
		0, 2, 3
	};

#if defined(__ARMCC__)
    GLfloat texcoords[8];
    texcoords[0] = 0 + s_offset;
    texcoords[1] = 0 + t_offset;
    texcoords[2] = 0 + s_offset;
    texcoords[3] = 1 + t_offset;
    texcoords[4] = 1 + s_offset;
    texcoords[5] = 1 + t_offset;
    texcoords[6] = 1 + s_offset;
    texcoords[7] = 0 + t_offset;
#else
	GLfloat texcoords[] =
	{
		0 + s_offset, 0 + t_offset,
		0 + s_offset, 1 + t_offset,
		1 + s_offset, 1 + t_offset,
		1 + s_offset, 0 + t_offset
	};
#endif

	glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
	glGetIntegerv(GL_VIEWPORT, viewport);

	/* bottom-left vertex */
	vertices[0*3+0] = (x + 0) / (viewport[2] / 2.0) - 1.0f;
	vertices[0*3+1] = (y + 0) / (viewport[3] / 2.0) - 1.0f;
	vertices[0*3+2] = 0.5;

	/* top-left vertex */
	vertices[1*3+0] = (x + 0) / (viewport[2] / 2.0) - 1.0f;
	vertices[1*3+1] = (y + h) / (viewport[3] / 2.0) - 1.0f;
	vertices[1*3+2] = 0.5;

	/* top-right vertex */
	vertices[2*3+0] = (x + w) / (viewport[2] / 2.0) - 1.0f;
	vertices[2*3+1] = (y + h) / (viewport[3] / 2.0) - 1.0f;
	vertices[2*3+2] = 0.5;

	/* bottom-right vertex */
	vertices[3*3+0] = (x + w) / (viewport[2] / 2.0) - 1.0f;
	vertices[3*3+1] = (y + 0) / (viewport[3] / 2.0) - 1.0f;
	vertices[3*3+2] = 0.5;

	pos = glGetAttribLocation(proggy, "in_tex0");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

	pos = glGetAttribLocation(proggy, "position");
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void render_yuv_texture_multiple_samplers_mixed( GLuint *tex, int num_textures_yuv, int num_textures_rgb, int width, int height )
{
    int i;
    GLint viewport[4];
    GLfloat vertices[4 * 4];
    GLint proggy;
    GLint pos;
    float x = 0;
    float y = 0;
    float w = width;
    float h = height;
    const float s_offset = 0.0f;
    const float t_offset = 0.0f;

    GLushort indices[] =
    {
        0, 1, 2,
        0, 2, 3
    };

#if defined(__ARMCC__)
    GLfloat texcoords[8];
    texcoords[0] = 0 + s_offset;
    texcoords[1] = 0 + t_offset;
    texcoords[2] = 0 + s_offset;
    texcoords[3] = 1 + t_offset;
    texcoords[4] = 1 + s_offset;
    texcoords[5] = 1 + t_offset;
    texcoords[6] = 1 + s_offset;
    texcoords[7] = 0 + t_offset;
#else
    GLfloat texcoords[] =
    {
        0 + s_offset, 0 + t_offset,
        0 + s_offset, 1 + t_offset,
        1 + s_offset, 1 + t_offset,
        1 + s_offset, 0 + t_offset
    };
#endif

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    load_program("pass_two_attribs.vert", "texture_yuv_multi_mixed.frag");

    glGetIntegerv(GL_CURRENT_PROGRAM, &proggy);
    glGetIntegerv(GL_VIEWPORT, viewport);

    /* bottom-left vertex */
    vertices[0*3+0] = (x + 0) / (viewport[2] / 2.0) - 1.0f;
    vertices[0*3+1] = (y + 0) / (viewport[3] / 2.0) - 1.0f;
    vertices[0*3+2] = 0.5;

    /* top-left vertex */
    vertices[1*3+0] = (x + 0) / (viewport[2] / 2.0) - 1.0f;
    vertices[1*3+1] = (y + h) / (viewport[3] / 2.0) - 1.0f;
    vertices[1*3+2] = 0.5;

    /* top-right vertex */
    vertices[2*3+0] = (x + w) / (viewport[2] / 2.0) - 1.0f;
    vertices[2*3+1] = (y + h) / (viewport[3] / 2.0) - 1.0f;
    vertices[2*3+2] = 0.5;

    /* bottom-right vertex */
    vertices[3*3+0] = (x + w) / (viewport[2] / 2.0) - 1.0f;
    vertices[3*3+1] = (y + 0) / (viewport[3] / 2.0) - 1.0f;
    vertices[3*3+2] = 0.5;

    for ( i=0; i<num_textures_yuv; i++ )
    {
        glActiveTexture( GL_TEXTURE0 + i );
        glBindTexture( GL_TEXTURE_EXTERNAL_OES, tex[i] );
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    for ( ; i<num_textures_yuv+num_textures_rgb; i++ )
    {
        glActiveTexture( GL_TEXTURE0 + i );
        glBindTexture( GL_TEXTURE_2D, tex[i] );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    pos = glGetAttribLocation(proggy, "in_tex0");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

    for ( i=0; i<num_textures_yuv + num_textures_rgb; i++ )
    {
        char sampler[256];

        memset( sampler, 0, 256 );
        sprintf( sampler, "diffuse%i", i );
        pos = glGetUniformLocation( proggy, sampler );
        glUniform1i(pos, i );
    }

    pos = glGetAttribLocation(proggy, "position");
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 0, vertices);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}


int compare_to_reference( BackbufferImage *backbuffer, float *rgb, int width, int height, float epsilon )
{
	int x,y;
	int diff = 0;

	for ( y = 0; y < height; y++ )
	{
		for ( x = 0; x < width; x++ )
		{
			struct gles_color expected = gles_color_make_3f( rgb[(y*width+x)*3+0],
			                                                 rgb[(y*width+x)*3+1],
															 rgb[(y*width+x)*3+2] );
			if ( 1 != gles_fb_color_equal(gles_fb_get_pixel_color(backbuffer, x, y), expected, epsilon) )
			{
				diff++;
			}
		}
	}

	return diff;
}

void additive_blend_rgb_images( float *dst, float *src, int width, int height )
{
	int x,y;
	for ( y = 0; y < height; y++ )
	{
		for ( x = 0; x < width; x++ )
		{
			int c;
			for ( c = 0; c < 3; c++ )
			{
				dst[(y*width+x)*3+c] = MAX( dst[(y*width+x)*3+c]+src[(y*width+x)*3+c], 1.0f);
			}
		}
	}
}
#endif /* EGL_MALI_GLES && MALI_EGL_GLES_MAJOR_VERSION == 2 */

void generate_yuv444_image_black( yuv_image *image, int width, int height )
{
	MEMSET( image->plane[0], 0x0, width*height );
	MEMSET( image->plane[1], 0x80, width*height );
	MEMSET( image->plane[2], 0x80, width*height );
}

/**
 * Process and threading abstraction
 */

#if !defined(__SYMBIAN32__)
/**
 * Parameter wrapper for threadproc wrapp
 */
typedef struct yuv_theadproc_param_wrapper_T
{
	yuv_helper_threadproc *actual_threadproc;
	void *actual_param;
} yuv_threadproc_param_wrapper;

/**
 * Pthread Static wrapper for platform independant egl_helper_threadproc functions
 * A pointer to this function is of type void *(*)(void*).
 * Purpose is an example of how you would wrap it, and to ensure the type is all correct;
 */
static void* yuv_helper_threadproc_wrapper( void *threadproc_params )
{
	yuv_threadproc_param_wrapper *params;
	void *retcode;
	TPI_ASSERT_POINTER( threadproc_params );
	params = (yuv_threadproc_param_wrapper *)threadproc_params;

	retcode = (void*)params->actual_threadproc(params->actual_param);

	/* Would free the threadproc_params here, but suite mempool does this for us */

	return retcode;
}
#endif

u32 yuv_helper_create_thread( suite *test_suite, yuv_helper_threadproc *threadproc, void *start_param )
{
#if defined(__SYMBIAN32__)
    return egl_helper_create_thread( test_suite, threadproc, start_param );
#else
	yuv_threadproc_param_wrapper *param_wrapper;
	pthread_t thread_handle; /* Type unsigned long int, so can use as handles without backing with memory */
	int ret;
	TPI_ASSERT_POINTER( test_suite );
	TPI_ASSERT_POINTER( threadproc );

	/* If not using mempools, would free this inside the wrapper threadproc */
	param_wrapper = (yuv_threadproc_param_wrapper*)_test_mempool_alloc( &test_suite->fixture_pool, sizeof( yuv_threadproc_param_wrapper ) );

	param_wrapper->actual_threadproc = threadproc;
	param_wrapper->actual_param = start_param;

	ret = pthread_create( &thread_handle, NULL, yuv_helper_threadproc_wrapper, param_wrapper );

	if ( 0 != ret )
	{
		/* Failure - would free the param_wrapper here if not using mempools */
		return 0;
	}

	return (u32)thread_handle;
#endif
}

u32 yuv_helper_wait_for_thread( suite *test_suite, u32 thread_handle )
{
#if defined(__SYMBIAN32__)
    return egl_helper_wait_for_thread( test_suite, thread_handle );
#else
	pthread_t pthread_handle; /* Type unsigned long int, so can use as handles without backing with memory */
	void *exitcode = NULL;

	TPI_ASSERT_POINTER( test_suite );

	pthread_handle = (pthread_t)thread_handle;

	if ( 0 != pthread_join( pthread_handle, &exitcode ) )
	{
		/* Failure */
		return (u32)-1;
	}

	return (u32)exitcode;
#endif
}


