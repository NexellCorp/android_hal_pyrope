/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifdef __SYMBIAN32__
#ifdef MALI_USE_GLES_1
/* define GL_API as __declspec(dllexport) in GLES\glplatform.h */
#define __GL_EXPORTS
#endif /* MALI_USE_GLES_1 */


#ifdef MALI_USE_GLES_2
/* define MALI_GL_APICALL as __declspec(dllexport) in GLES2\gl2platform.h */
#define SYMBIAN_OGLES_DLL_EXPORTS
#endif /* MALI_USE_GLES_2 */

#if !defined(MALI_USE_GLES_1) && !defined(MALI_USE_GLES_2)
#error "Neither MALI_USING_GLES_1 nor MALI_USING_GLES_2 defined"
#endif

#endif  /* __SYMBIAN32__ */

#include "gles_base.h"
#include "gles_entrypoints.h"
#include "gles_debug_stats.h" /* can (and often will) override GLES_ENTER_FUNC and GLES_LEAVE_FUNC */
#include "base/mali_runtime.h"

#include "gles_context.h"
#include "gles_vtable.h"
#include "gles_share_lists.h"
#include "gles_object.h"
#include "mali_instrumented_context.h"
#include "gles_context.h"
#include "gles_share_lists.h"
#include "gles_object.h"
#include "gles_state.h"
#include "gles_config_extension.h"
#include "gles_util.h"
#include "gles_api_trace.h"

#ifdef __SYMBIAN32__
#include "symbian_gles_trace.h"
#endif

#ifndef MALI_GL_APIENTRY
#define MALI_GL_APIENTRY(a) GL_APIENTRY a
#endif

#if !defined( NDEBUG )
/* nexell add */
#if defined(HAVE_ANDROID_OS)
#include <android/log.h>
#include "../egl/egl_image_android.h"
#endif
void _gles_debug_enter_func(const char *func_name)
{
#if defined(HAVE_ANDROID_OS)
	ADBG(1, "[GL] %s enter\n", func_name);
#else
	/*MALI_DEBUG_PRINT(2, ("[GL] %s enter\n", func_name));*/
#endif
}
void _gles_debug_leave_func(const char *func_name)
{
#if defined(HAVE_ANDROID_OS)
	ADBG(1, "[GL] %s leave\n", func_name);
#else
	/*MALI_DEBUG_PRINT(2, ("[GL] %s leave\n", func_name));*/
#endif
}
#endif

MALI_GL_APICALL void MALI_GL_APIENTRY( glActiveTexture )( GLenum texture )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glActiveTexture);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(texture);

	err = ctx->vtable->fp_glActiveTexture( ctx, texture );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glActiveTexture);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBindBuffer )( GLenum target, GLuint buffer )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glBindBuffer);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLUINT(buffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glBindBuffer( ctx, target, buffer);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR)
	{
		ctx->vtable->fp_set_error( ctx, err );
	}

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBindBuffer);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBindTexture )(GLenum target, GLuint texture)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glBindTexture);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLUINT(texture);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glBindTexture( ctx, target, texture );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBindTexture);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBlendFunc )( GLenum src, GLenum dst )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glBlendFunc);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(src);
	MALI_API_TRACE_PARAM_GLENUM(dst);

	/* No locking needed */
	err = ctx->vtable->fp_glBlendFunc( ctx, src, dst, src, dst );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBlendFunc);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBufferData )(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glBufferData);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLSIZEIPTR(size);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_IN, data, size > 0 ? size : 0, 0);
	MALI_API_TRACE_PARAM_GLENUM(usage);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glBufferData(ctx->base_ctx, &ctx->state.common.vertex_array, ctx->api_version, target, size, data, usage);
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLBUFFERDATA(target, size, ctx, err);
	#endif

	if (err != GL_NO_ERROR)
	{
		ctx->vtable->fp_set_error( ctx, err );
	}

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBufferData);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBufferSubData )(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glBufferSubData);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLINTPTR(offset);
	MALI_API_TRACE_PARAM_GLSIZEIPTR(size);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_IN, data, size > 0 ? size : 0, 0);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glBufferSubData(ctx->base_ctx, &ctx->state.common.vertex_array, target, offset, size, data);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBufferSubData);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glClear )(GLbitfield mask)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glClear);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLBITFIELD(mask);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glClear(ctx, mask);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glClear);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glClearColor )( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glClearColor);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLCLAMPF(red);
	MALI_API_TRACE_PARAM_GLCLAMPF(green);
	MALI_API_TRACE_PARAM_GLCLAMPF(blue);
	MALI_API_TRACE_PARAM_GLCLAMPF(alpha);

	/* No locking needed */
	ctx->vtable->fp_glClearColor(&ctx->state.common.framebuffer_control, FLOAT_TO_FTYPE( red ), FLOAT_TO_FTYPE( green ), FLOAT_TO_FTYPE( blue ), FLOAT_TO_FTYPE( alpha ) );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glClearColor);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glClearDepthf )( GLclampf depth )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glClearDepthf);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLCLAMPF(depth);

	/* No locking needed */
	ctx->vtable->fp_glClearDepthf(&(ctx->state.common.framebuffer_control), FLOAT_TO_FTYPE( depth ) );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glClearDepthf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glClearStencil )( GLint s )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glClearStencil);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(s);

	/* No locking needed */
	ctx->vtable->fp_glClearStencil(&ctx->state.common.framebuffer_control, s );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glClearStencil);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glColorMask )( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glColorMask);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLBOOLEAN(red);
	MALI_API_TRACE_PARAM_GLBOOLEAN(green);
	MALI_API_TRACE_PARAM_GLBOOLEAN(blue);
	MALI_API_TRACE_PARAM_GLBOOLEAN(alpha);

	/* No locking needed */
	ctx->vtable->fp_glColorMask( ctx, red, green, blue, alpha );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glColorMask);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glCompressedTexImage2D )( GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height,
                                                     GLint border, GLsizei imageSize, const GLvoid *data)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glCompressedTexImage2D);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLINT(level);
	MALI_API_TRACE_PARAM_GLENUM(internalformat);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);
	MALI_API_TRACE_PARAM_GLINT(border);
	MALI_API_TRACE_PARAM_GLSIZEI(imageSize);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_IN, data, imageSize > 0 ? imageSize : 0, 0);	

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glCompressedTexImage2D( ctx, target, level, internalformat, width, height, border, imageSize, data );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLCOMPRESSEDTEXIMAGE2D(width, height ,imageSize, data, err);
	#endif

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCompressedTexImage2D);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glCompressedTexSubImage2D )( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                                        GLenum format, GLsizei imageSize, const GLvoid *data)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glCompressedTexSubImage2D);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLINT(level);
	MALI_API_TRACE_PARAM_GLINT(xoffset);
	MALI_API_TRACE_PARAM_GLINT(yoffset);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLENUM(format);
	MALI_API_TRACE_PARAM_GLSIZEI(height);
	MALI_API_TRACE_PARAM_GLSIZEI(imageSize);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_IN, data, imageSize > 0 ? imageSize : 0, 0);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glCompressedTexSubImage2D( ctx, target, level, xoffset, yoffset, width, height, format, imageSize, data );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCompressedTexSubImage2D);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glCopyTexImage2D )( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                                               GLsizei width, GLsizei height, GLint border)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glCopyTexImage2D);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLINT(level);
	MALI_API_TRACE_PARAM_GLENUM(internalformat);
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);
	MALI_API_TRACE_PARAM_GLINT(border);


	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glCopyTexImage2D( ctx, target, level, internalformat, x, y, width, height, border );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCopyTexImage2D);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glCopyTexSubImage2D )( GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                  GLint x, GLint y, GLsizei width, GLsizei height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glCopyTexSubImage2D);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLINT(level);
	MALI_API_TRACE_PARAM_GLINT(xoffset);
	MALI_API_TRACE_PARAM_GLINT(yoffset);
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glCopyTexSubImage2D( ctx, target, level, xoffset, yoffset, x, y, width, height );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLCOPYTEXIMAGE2D(target, level, width, height, ctx, err); 
	#endif


	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCopyTexSubImage2D);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glCullFace )( GLenum mode )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glCullFace);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(mode);

	/* No locking needed */
	err = ctx->vtable->fp_glCullFace( &ctx->state.common.rasterization, mode );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCullFace);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDeleteBuffers )(GLsizei n, const GLuint * buffers)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDeleteBuffers);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_IN, buffers, n > 0 ? n : 0);	

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDeleteBuffers( ctx->share_lists->vertex_buffer_object_list, &ctx->state.common.vertex_array, n, buffers );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLDELETEBUFFERS(n, buffers, err);
	#endif

	if (err != GL_NO_ERROR)	ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDeleteBuffers);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDeleteTextures )(GLsizei n, const GLuint *textures )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDeleteTextures);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_IN, textures, n > 0 ? n : 0);

	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDeleteTextures( ctx, n, textures );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLDELETETEXTURES(n, textures, err);
	#endif

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDeleteTextures);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDepthFunc )( GLenum func )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glDepthFunc);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(func);

	/* No locking needed */
	err = ctx->vtable->fp_glDepthFunc( ctx, func );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDepthFunc);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDepthMask )( GLboolean flag )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glDepthMask);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLBOOLEAN(flag);

	/* No locking needed */
	ctx->vtable->fp_glDepthMask( ctx, flag );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDepthMask);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glDepthRangef )(GLclampf n, GLclampf f)
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDepthRangef);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLCLAMPF(n);
	MALI_API_TRACE_PARAM_GLCLAMPF(f);

	/* No locking needed */
	ctx->vtable->fp_glDepthRangef( ctx, FLOAT_TO_FTYPE(n), FLOAT_TO_FTYPE(f));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDepthRangef);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDisable )( GLenum cap )
{
	GLenum err = GL_NO_ERROR;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDisable);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(cap);

	/* No locking needed */
	err = ctx->vtable->fp_glDisable( ctx, cap, (GLboolean)GL_FALSE );
	if ( GL_NO_ERROR != err ) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDisable);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDrawArrays )( GLenum mode, GLint first, GLsizei count )
{
	GLenum err = GL_NO_ERROR;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glDrawArrays);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(mode);
	MALI_API_TRACE_PARAM_GLINT(first);
	MALI_API_TRACE_PARAM_GLSIZEI(count);

	_gles_share_lists_lock( ctx->share_lists );

	MALI_API_TRACE_GL_DRAW_ARRAYS(ctx, (first >= 0 && count >= 0) ? first + count : 0); /* This will copy attrib data, if not placed inside VBO */

	err = ctx->vtable->fp_glDrawArrays( ctx, mode, first, count );
	if ( GL_NO_ERROR != err ) ctx->vtable->fp_set_error( ctx, err );

	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawArrays);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDrawElements )( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	GLenum err = GL_NO_ERROR;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glDrawElements);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(mode);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_INDEXPOINTER(indices);

	_gles_share_lists_lock( ctx->share_lists );

	MALI_API_TRACE_GL_DRAW_ELEMENTS(ctx, mode, count, type, indices); /* This will copy attrib data, if not placed inside VBO */

	err = ctx->vtable->fp_glDrawElements( ctx, mode, count, type, indices );
	if ( GL_NO_ERROR != err ) ctx->vtable->fp_set_error( ctx, err );

	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawElements);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glEnable )( GLenum cap )
{
	GLenum err = GL_NO_ERROR;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glEnable);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(cap);

	/* No locking needed */
	err = ctx->vtable->fp_glEnable( ctx, cap, (GLboolean)GL_TRUE );
	if ( GL_NO_ERROR != err ) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glEnable);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFinish )( void )
{
	GLenum err = GL_NO_ERROR;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glFinish);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();


	
	/* ctx->share_lists is locked in _gles_finish (needed by egl) */
	err = ctx->vtable->fp_glFinish( ctx );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLFINISH(err);
	#endif

	if ( GL_NO_ERROR != err ) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFinish);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFlush )( void )
{
	GLenum err = GL_NO_ERROR;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glFlush);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	
	/* ctx->share_lists is locked in _gles_flush (needed by egl) */
	err = ctx->vtable->fp_glFlush( ctx, MALI_FALSE);

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLFLUSH(err);
#endif

	if ( GL_NO_ERROR != err ) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFlush);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFrontFace )( GLenum mode )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glFrontFace);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(mode);

	/* No locking needed */
	err = ctx->vtable->fp_glFrontFace( &ctx->state.common.rasterization, mode );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFrontFace);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGenBuffers )( GLsizei n, GLuint *buffers )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGenBuffers);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_OUT, buffers, n > 0 ? n : 0);


	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGenBuffers( ctx->share_lists->vertex_buffer_object_list, n, buffers, WRAPPER_VERTEXBUFFER );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLGENBUFFERS(n, buffers, err);
	#endif

	if (err != GL_NO_ERROR)
	{
		ctx->vtable->fp_set_error( ctx, err );
	}

	MALI_API_TRACE_RETURN_GLUINT_ARRAY(2, (GL_NO_ERROR == err) ? buffers : NULL, n > 0 ? n : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGenBuffers);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGenTextures )(GLsizei n, GLuint *textures )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGenTextures);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_OUT, textures, n > 0 ? n : 0);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGenTextures( ctx->share_lists->texture_object_list, n, textures, WRAPPER_TEXTURE );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLGENTEXTURES(n, textures, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLUINT_ARRAY(2, (GL_NO_ERROR == err) ? textures : NULL, n > 0 ? n : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGenTextures);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetBooleanv )(GLenum pname, GLboolean *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetBooleanv);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);

	_gles_share_lists_lock( ctx->share_lists );

	MALI_API_TRACE_PARAM_GLBOOLEAN_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_get_getter_out_count(ctx, pname), 0);

	err = ctx->vtable->fp_glGetBooleanv(ctx, pname, params, GLES_BOOLEAN );

	MALI_API_TRACE_RETURN_GLBOOLEAN_ARRAY(2, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_get_getter_out_count(ctx, pname));

	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetBooleanv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetBufferParameteriv )( GLenum target, GLenum pname, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetBufferParameteriv);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, 1);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetBufferParameteriv( &ctx->state.common, target, pname, params );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetBufferParameteriv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLenum MALI_GL_APIENTRY( glGetError )(void)
{
	GLenum errorcode;
	gles_context *ctx = GLES_GET_CONTEXT();

	/* The OpenGL ES 2.0 spec., chapter 2.5 (GL Errors), paragraph 4 states that:
	 * "[...] In other cases, the command generating the error is ignored so that
	 * it has no effect on GL state or framebuffer contents. If the generating
	 * command returns a value, it returns zero."
	 *
	 * We interpret this to mean that glGetError is a generating command and
	 * therefore should return the value 0.
	 */
	if (NULL == ctx) return 0;

	MALI_PROFILING_ENTER_FUNC(glGetError);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();

	/* No locking needed */
	errorcode = ctx->vtable->fp_glGetError( ctx );

	MALI_API_TRACE_RETURN_GLENUM(errorcode);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetError);
	GLES_API_LEAVE(ctx);

	return errorcode;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetFloatv )(GLenum pname, GLfloat *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetFloatv);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);

	_gles_share_lists_lock( ctx->share_lists );

	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_get_getter_out_count(ctx, pname));

	err = ctx->vtable->fp_glGetFloatv(ctx, pname, params, GLES_FLOAT );

	MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(2, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_get_getter_out_count(ctx, pname));

	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetFloatv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetIntegerv )(GLenum pname, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetIntegerv);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);

	_gles_share_lists_lock( ctx->share_lists );

	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_get_getter_out_count(ctx, pname));

	err = ctx->vtable->fp_glGetIntegerv(ctx, pname, params, GLES_INT );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(2, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_get_getter_out_count(ctx, pname));

	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetIntegerv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL const GLubyte * MALI_GL_APIENTRY( glGetString )(GLenum name)
{
	GLenum err;
	const GLubyte *return_string;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return NULL;

	MALI_PROFILING_ENTER_FUNC(glGetString);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(name);

	/* No locking needed */
	err = ctx->vtable->fp_glGetString( ctx, name, &return_string);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLUBYTE_ARRAY(0, (GL_NO_ERROR == err) ? return_string : NULL, NULL != return_string ? _mali_sys_strlen((char*)return_string) + 1 : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetString);
	GLES_API_LEAVE(ctx);

	return (const GLubyte *) return_string;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetTexParameterfv )(GLenum target, GLenum pname, GLfloat *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetTexParameterfv);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_texparameter_count(pname));

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetTexParameterfv(&ctx->state.common, target, pname, params, GLES_FLOAT );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_texparameter_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetTexParameterfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetTexParameteriv )(GLenum target, GLenum pname, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetTexParameteriv);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_texparameter_count(pname));

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetTexParameteriv(&ctx->state.common, target, pname, params, GLES_INT );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_texparameter_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetTexParameteriv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glHint )( GLenum target, GLenum mode )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glHint);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(mode);

	/* No locking needed */
	err = ctx->vtable->fp_glHint(&ctx->state, target, mode );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glHint);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLboolean MALI_GL_APIENTRY( glIsBuffer )(GLuint buffer)
{
	GLenum value;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return (GLboolean) GL_FALSE;

	MALI_PROFILING_ENTER_FUNC(glIsBuffer);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(buffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	value = ctx->vtable->fp_glIsBuffer(ctx->share_lists->vertex_buffer_object_list, buffer);
	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_RETURN_GLBOOLEAN((GLboolean)value);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glIsBuffer);
	GLES_API_LEAVE(ctx);

	return (GLboolean) value;
}

MALI_GL_APICALL GLboolean MALI_GL_APIENTRY( glIsEnabled )(GLenum cap)
{
	GLenum err;
	GLboolean enabled;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return GL_FALSE;

	MALI_PROFILING_ENTER_FUNC(glIsEnabled);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(cap);

	/* No locking needed */
	err = ctx->vtable->fp_glIsEnabled( ctx, cap, & enabled );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLBOOLEAN(enabled);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glIsEnabled);
	GLES_API_LEAVE(ctx);

	return enabled;
}

MALI_GL_APICALL GLboolean MALI_GL_APIENTRY( glIsTexture )(GLuint texture)
{
	GLboolean is_texture = GL_FALSE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return (GLboolean) GL_FALSE;

	MALI_PROFILING_ENTER_FUNC(glIsTexture);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(texture);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	is_texture = ctx->vtable->fp_glIsTexture(ctx->share_lists->texture_object_list, texture);
	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_RETURN_GLBOOLEAN(is_texture);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glIsTexture);
	GLES_API_LEAVE(ctx);

	return is_texture;
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glLineWidth )( GLfloat width )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glLineWidth);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(width);

	/* No locking needed */
	err = ctx->vtable->fp_glLineWidth( ctx, FLOAT_TO_FTYPE( width ) );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLineWidth);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glPixelStorei )( GLenum pname, GLint param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPixelStorei);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT(param);

	/* No locking needed */
	err = ctx->vtable->fp_glPixelStorei(&(ctx->state.common.pixel), pname, param);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPixelStorei);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glPolygonOffset )( GLfloat factor, GLfloat units )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glPolygonOffset);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(factor);
	MALI_API_TRACE_PARAM_GLFLOAT(units);

	/* No locking needed */
	err = ctx->vtable->fp_glPolygonOffset( ctx, FLOAT_TO_FTYPE( factor ), FLOAT_TO_FTYPE( units ) );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPolygonOffset);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glReadPixels )( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glReadPixels);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);
	MALI_API_TRACE_PARAM_GLENUM(format);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_TEST_IN|MALI_API_TRACE_OUT, (u8*)pixels, mali_api_trace_gl_read_pixels_size(ctx, width, height, format, type), 0);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glReadPixels( ctx, x, y, width, height, format, type, pixels );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLUBYTE_ARRAY(7, (GL_NO_ERROR == err) ? (u8*)pixels : NULL, mali_api_trace_gl_read_pixels_size(ctx, width, height, format, type));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glReadPixels);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glSampleCoverage )( GLclampf value, GLboolean invert )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glSampleCoverage);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLCLAMPF(value);
	MALI_API_TRACE_PARAM_GLBOOLEAN(invert);

	/* No locking needed */
	ctx->vtable->fp_glSampleCoverage( ctx, value, invert );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glSampleCoverage);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glScissor )(GLint x, GLint y, GLint width, GLint height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glScissor);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	MALI_API_TRACE_PARAM_GLINT(width);
	MALI_API_TRACE_PARAM_GLINT(height);

	/* No locking needed */
	err = ctx->vtable->fp_glScissor( ctx, x, y, width, height );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glScissor);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glStencilFunc )( GLenum func, GLint ref, GLuint mask )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glStencilFunc);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(func);
	MALI_API_TRACE_PARAM_GLINT(ref);
	MALI_API_TRACE_PARAM_GLUINT(mask);

	/* No locking needed */
	err = ctx->vtable->fp_glStencilFunc( ctx, GL_FRONT_AND_BACK, func, ref, mask );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glStencilFunc);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glStencilMask )( GLuint mask )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glStencilMask);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(mask);

	/* No locking needed */
	err = ctx->vtable->fp_glStencilMask( ctx, GL_FRONT_AND_BACK, mask );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glStencilMask);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glStencilOp )( GLenum sfail, GLenum zfail, GLenum zpass )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glStencilOp);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(sfail);
	MALI_API_TRACE_PARAM_GLENUM(zfail);
	MALI_API_TRACE_PARAM_GLENUM(zpass);

	/* No locking needed */
	err = ctx->vtable->fp_glStencilOp( ctx, GL_FRONT_AND_BACK, sfail, zfail, zpass);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glStencilOp);
	GLES_API_LEAVE(ctx);
}

/* If OpenGL ES 1.x is included in the build then we always use the entrypoint where internalformat is
   type GLint instead of GLenum */
#if defined(__SYMBIAN32__)
#if MALI_USE_GLES_1
MALI_GL_APICALL void MALI_GL_APIENTRY( glTexImage2D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                                         GLint border, GLenum format, GLenum type, const GLvoid *pixels)
#elif MALI_USE_GLES_2
	MALI_GL_APICALL void MALI_GL_APIENTRY( glTexImage2D )(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height,
	                                          GLint border, GLenum format, GLenum type, const void *pixels)
#endif  /* MALI_USE_GLES_2 */
#else
	MALI_GL_APICALL void MALI_GL_APIENTRY( glTexImage2D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
	                                         GLint border, GLenum format, GLenum type, const GLvoid *pixels)
#endif  /* __SYMBIAN32__ */
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexImage2D);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLINT(level);
#if defined(__SYMBIAN32__)
#if MALI_USE_GLES_1
	MALI_API_TRACE_PARAM_GLINT(internalformat);
#elif MALI_USE_GLES_2
	MALI_API_TRACE_PARAM_GLENUM(internalformat);
#endif  /* MALI_USE_GLES_2 */
#else
	MALI_API_TRACE_PARAM_GLINT(internalformat);
#endif
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);
	MALI_API_TRACE_PARAM_GLINT(border);
	MALI_API_TRACE_PARAM_GLENUM(format);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_IN, (u8*)pixels, mali_api_trace_gl_teximage2d_size(ctx, width, height, format, type), 0);


	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glTexImage2D( ctx, target, level, internalformat,
	                                    width, height, border, format, type, pixels, ctx->state.common.pixel.unpack_alignment );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLTEXIMAGE2D(width, height, format, type, pixels, ctx->state.common.pixel.unpack_alignment, err );//assume 8 bits for size
	#endif

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexImage2D);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glTexParameterf )(GLenum target, GLenum pname, GLfloat param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexParameterf);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT(param);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glTexParameterf(&ctx->state.common.texture_env, target, pname, (GLvoid *)&param, GLES_FLOAT );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexParameterf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexParameterfv )(GLenum target, GLenum pname, const GLfloat *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexParameterfv);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_texparameter_count(pname));

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glTexParameterfv(&ctx->state.common.texture_env, target, pname, (GLvoid *)params, GLES_FLOAT );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexParameterfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexParameteri )(GLenum target, GLenum pname, GLint param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexParameteri);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT(param);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glTexParameteri(&ctx->state.common.texture_env, target, pname, (GLvoid *)&param, GLES_INT );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexParameteri);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexParameteriv )(GLenum target, GLenum pname, const GLint *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexParameteriv);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_texparameter_count(pname));

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glTexParameteriv(&ctx->state.common.texture_env, target, pname, (GLvoid *)params, GLES_INT);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexParameteriv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexSubImage2D )( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                              GLenum format, GLenum type, const GLvoid *pixels)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexSubImage2D);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLINT(level);
	MALI_API_TRACE_PARAM_GLINT(xoffset);
	MALI_API_TRACE_PARAM_GLINT(yoffset);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);
	MALI_API_TRACE_PARAM_GLENUM(format);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_IN, (u8*)pixels, mali_api_trace_gl_teximage2d_size(ctx, width, height, format, type), 0);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glTexSubImage2D( ctx, target, level, xoffset, yoffset,
	                                       width, height, format, type, pixels, ctx->state.common.pixel.unpack_alignment );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexSubImage2D);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glViewport )(GLint x, GLint y, GLint width, GLint height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glViewport);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	MALI_API_TRACE_PARAM_GLINT(width);
	MALI_API_TRACE_PARAM_GLINT(height);

	/* No locking needed */
	err = ctx->vtable->fp_glViewport(&ctx->state, x, y, width, height);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glViewport);
	GLES_API_LEAVE(ctx);
}

/* API functions that are present in various extensions that are relevant to in both
   OpenGL ES 1.x and OpenGL ES 2.x */

/* API functions that are present if the GL_OES_egl_image extension is supported */
#if EXTENSION_EGL_IMAGE_OES_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY( glEGLImageTargetTexture2DOES )( GLenum target, GLeglImageOES image )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glEGLImageTargetTexture2DOES);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLEGLIMAGEOES(image);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glEGLImageTargetTexture2DOES( ctx, target, image );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	#if defined(__SYMBIAN32__)
    SYMBIAN_GPU_TRACE_GLEGLIMAGETARGETTEXTURE2DOES(ctx->state.common.texture_env.active_texture, image, ctx, err);//image sizeunknown
	#endif

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glEGLImageTargetTexture2DOES);
	GLES_API_LEAVE(ctx);
}

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
MALI_GL_APICALL void MALI_GL_APIENTRY( glRenderbufferStorageMultisampleEXT )(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glRenderbufferStorageMultisampleEXT);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLSIZEI(samples);
	MALI_API_TRACE_PARAM_GLENUM(internalformat);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glRenderbufferStorageMultisampleEXT( ctx->base_ctx, ctx->share_lists->renderbuffer_object_list, &ctx->state.common.renderbuffer, target, samples, internalformat, width, height );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glRenderbufferStorageMultisampleEXT);
	GLES_API_LEAVE(ctx);
}
#endif

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
MALI_GL_APICALL void MALI_GL_APIENTRY( glFramebufferTexture2DMultisampleEXT )(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glFramebufferTexture2DMultisampleEXT);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(attachment);
	MALI_API_TRACE_PARAM_GLENUM(textarget);
	MALI_API_TRACE_PARAM_GLUINT(texture);
	MALI_API_TRACE_PARAM_GLINT(level);
	MALI_API_TRACE_PARAM_GLSIZEI(samples);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glFramebufferTexture2DMultisampleEXT(
		ctx,
		&ctx->state.common.framebuffer,
		ctx->share_lists->framebuffer_object_list,
		ctx->share_lists->renderbuffer_object_list,
		ctx->share_lists->texture_object_list,
		target,
		attachment,
		textarget,
		texture,
		level,
		samples
	);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFramebufferTexture2DMultisampleEXT);
	GLES_API_LEAVE(ctx);
}
#endif

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY( glEGLImageTargetRenderbufferStorageOES )( GLenum target, GLeglImageOES image )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glEGLImageTargetRenderbufferStorageOES);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLEGLIMAGEOES(image);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glEGLImageTargetRenderbufferStorageOES(ctx, target, image );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glEGLImageTargetRenderbufferStorageOES);
	GLES_API_LEAVE(ctx);
}
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */
#endif /* EXTENSION_EGL_IMAGE_OES_ENABLE */

#if EXTENSION_DISCARD_FRAMEBUFFER
MALI_GL_APICALL void MALI_GL_APIENTRY( glDiscardFramebufferEXT )( GLenum target, GLsizei numAttachments, const GLenum *attachments )
{
	GLenum err = GL_NO_ERROR;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDiscardFramebufferEXT);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLSIZEI( numAttachments );
	MALI_API_TRACE_PARAM_GLENUM_ARRAY( MALI_API_TRACE_IN, attachments, numAttachments );

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDiscardFramebufferEXT(ctx, 
	                                              &ctx->state.common.framebuffer,
	                                              target, 
	                                              numAttachments, 
	                                              attachments);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDiscardFramebufferEXT);
	GLES_API_LEAVE(ctx);
}
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
MALI_GL_APICALL GLenum MALI_GL_APIENTRY( glGetGraphicsResetStatusEXT )(void)
{
	GLenum err = GL_NO_ERROR;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return 0;

	MALI_PROFILING_ENTER_FUNC( glGetGraphicsResetStatusEXT );
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetGraphicsResetStatusEXT(ctx);
	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetGraphicsResetStatusEXT);
	GLES_API_LEAVE(ctx);

	return err;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glReadnPixelsEXT )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLvoid *pixels)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glReadnPixelsEXT);
	GLES_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);
	MALI_API_TRACE_PARAM_GLENUM(format);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLSIZEI(bufSize);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_TEST_IN|MALI_API_TRACE_OUT, (u8*)pixels, mali_api_trace_gl_read_pixels_size(ctx, width, height, format, type), 0);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glReadnPixelsEXT( ctx, x, y, width, height, format, type, bufSize, pixels );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLUBYTE_ARRAY(7, (GL_NO_ERROR == err) ? (u8*)pixels : NULL, mali_api_trace_gl_read_pixels_size(ctx, width, height, format, type));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glReadnPixelsEXT);
	GLES_API_LEAVE(ctx);
}

#endif /* EXTENSION_ROBUSTNESS_EXT_ENABLE */
