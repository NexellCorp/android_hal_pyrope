/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_entrypoints.h
 * @brief Contains the definitions of the entrypoint functions that are only present
 *        in OpenGL ES 1.x.
 */

#ifdef __SYMBIAN32__
/* define MALI_GL_APICALL as __declspec(dllexport) in GLES\glplatform.h */
#define __GL_EXPORTS

#ifdef MALI_USE_GLES_2
/* define GL_APICALL as __declspec(dllexport) in GLES2\gl2platform.h */
#define SYMBIAN_OGLES_DLL_EXPORTS
#endif

#endif  /* __SYMBIAN32__ */

#include "gles_base.h"
#include "gles_entrypoints.h"
#include "base/mali_runtime.h"
#include "gles_context.h"
#include "gles1_state/gles1_state.h"
#include "gles_vtable.h"
#include "gles_share_lists.h"
#include "gles_convert.h"
#include "gles1_api_trace.h"

#define GLES1_API_ENTER(ctx) do {\
		MALI_DEBUG_ASSERT_POINTER(ctx->state.api.gles1); \
		GLES_API_ENTER(ctx); \
	} while (0);

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glAlphaFunc )( GLenum func, GLclampf ref )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glAlphaFunc);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(func);
	MALI_API_TRACE_PARAM_GLCLAMPF(ref);

	/* No locking needed */
	err = ctx->vtable->fp_glAlphaFunc( ctx, func, FLOAT_TO_FTYPE( ref ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glAlphaFunc);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glAlphaFuncx )( GLenum func, GLclampx ref )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glAlphaFuncx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(func);
	MALI_API_TRACE_PARAM_GLCLAMPX(ref);

	/* No locking needed */
	err = ctx->vtable->fp_glAlphaFuncx( ctx, func, FIXED_TO_FTYPE( ref ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glAlphaFuncx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glClearColorx )( GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glClearColorx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLCLAMPX(red);
	MALI_API_TRACE_PARAM_GLCLAMPX(green);
	MALI_API_TRACE_PARAM_GLCLAMPX(blue);
	MALI_API_TRACE_PARAM_GLCLAMPX(alpha);

	/* No locking needed */
	ctx->vtable->fp_glClearColorx(&ctx->state.common.framebuffer_control,
			FIXED_TO_FTYPE( red ), FIXED_TO_FTYPE( green ),
			FIXED_TO_FTYPE( blue ), FIXED_TO_FTYPE( alpha ) );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glClearColorx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glClearDepthx )( GLclampx depth )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glClearDepthx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLCLAMPX(depth);

	/* No locking needed */
	ctx->vtable->fp_glClearDepthx(&ctx->state.common.framebuffer_control, FIXED_TO_FTYPE( depth ) );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glClearDepthx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glClientActiveTexture )( GLenum texture )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glClientActiveTexture);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(texture);

	err = ctx->vtable->fp_glClientActiveTexture( &ctx->state.common.vertex_array, texture );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glClientActiveTexture);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glClipPlanef )( GLenum plane, const GLfloat *equation )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glClipPlanef);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(plane);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, equation, 4);

	/* No locking needed */
	err = ctx->vtable->fp_glClipPlanef( ctx, plane, (GLvoid *) equation, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glClipPlanef);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glClipPlanex )( GLenum plane, const GLfixed *equation )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glClipPlanex);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(plane);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, equation, 4);

	/* No locking needed */
	err = ctx->vtable->fp_glClipPlanex( ctx, plane, (GLvoid *) equation, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glClipPlanex);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glColor4f )( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glColor4f);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(red);
	MALI_API_TRACE_PARAM_GLFLOAT(green);
	MALI_API_TRACE_PARAM_GLFLOAT(blue);
	MALI_API_TRACE_PARAM_GLFLOAT(alpha);

	/* No locking needed */
	err = ctx->vtable->fp_glColor4f( ctx,
			FLOAT_TO_FTYPE( red ), FLOAT_TO_FTYPE( green ),
			FLOAT_TO_FTYPE( blue ), FLOAT_TO_FTYPE( alpha ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glColor4f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glColor4ub )( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glColor4ub);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUBYTE(red);
	MALI_API_TRACE_PARAM_GLUBYTE(green);
	MALI_API_TRACE_PARAM_GLUBYTE(blue);
	MALI_API_TRACE_PARAM_GLUBYTE(alpha);

	/* No locking needed */
	err = ctx->vtable->fp_glColor4ub( ctx,
			FIXED_TO_FTYPE( UBYTE_TO_FIXED( red ) ),
			FIXED_TO_FTYPE( UBYTE_TO_FIXED( green ) ),
			FIXED_TO_FTYPE( UBYTE_TO_FIXED( blue ) ),
			FIXED_TO_FTYPE( UBYTE_TO_FIXED( alpha ) ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glColor4ub);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glColor4x )( GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glColor4x);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(red);
	MALI_API_TRACE_PARAM_GLFIXED(green);
	MALI_API_TRACE_PARAM_GLFIXED(blue);
	MALI_API_TRACE_PARAM_GLFIXED(alpha);

	/* No locking needed */
	err = ctx->vtable->fp_glColor4x( ctx,
			FIXED_TO_FTYPE( red ), FIXED_TO_FTYPE( green ),
			FIXED_TO_FTYPE( blue ), FIXED_TO_FTYPE( alpha ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glColor4x);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glColorPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glColorPointer);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(size);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLSIZEI(stride);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(pointer);

	/* No locking needed */
	err = ctx->vtable->fp_glColorPointer(ctx, size, type, stride, pointer);
	if (GL_NO_ERROR!= err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glColorPointer);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDepthRangex )(GLclampx z_near, GLclampx z_far)
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDepthRangex);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_PARAM_GLCLAMPX(z_near);
	MALI_API_TRACE_PARAM_GLCLAMPX(z_far);

	/* No locking needed */
	ctx->vtable->fp_glDepthRangex( ctx, FIXED_TO_FTYPE(z_near), FIXED_TO_FTYPE(z_far));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDepthRangex);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDisableClientState )( GLenum cap )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDisableClientState);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(cap);

	/* No locking needed */
	err = ctx->vtable->fp_glDisableClientState( ctx, cap, (GLboolean)GL_FALSE );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDisableClientState);
	GLES_API_LEAVE(ctx);
}

#if EXTENSION_DRAW_TEX_OES_ENABLE
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glDrawTexfOES  )(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDrawTexfOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);
	MALI_API_TRACE_PARAM_GLFLOAT(z);
	MALI_API_TRACE_PARAM_GLFLOAT(width);
	MALI_API_TRACE_PARAM_GLFLOAT(height);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDrawTexfOES( ctx, x, y, z, width, height );
	_gles_share_lists_unlock( ctx->share_lists );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawTexfOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDrawTexfvOES  )(const GLfloat *coords)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDrawTexfvOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, coords, 5); /* x, y, z, width, height */

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDrawTexfvOES( ctx, coords[ 0 ], coords[ 1 ], coords[ 2 ], coords[ 3 ], coords[ 4 ] );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawTexfvOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDrawTexiOES )(GLint x, GLint y, GLint z, GLint width, GLint height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDrawTexiOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	MALI_API_TRACE_PARAM_GLINT(z);
	MALI_API_TRACE_PARAM_GLINT(width);
	MALI_API_TRACE_PARAM_GLINT(height);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDrawTexiOES( ctx, ( GLfloat ) x, ( GLfloat ) y, ( GLfloat ) z, ( GLfloat ) width, ( GLfloat ) height );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawTexiOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDrawTexivOES )(const GLint *coords)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDrawTexivOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_IN, coords, 5); /* x, y, z, width, height */

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDrawTexivOES( ctx, ( GLfloat ) coords[ 0 ], ( GLfloat ) coords[ 1 ], ( GLfloat ) coords[ 2 ], ( GLfloat ) coords[ 3 ], ( GLfloat ) coords[ 4 ] );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawTexivOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDrawTexsOES )(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDrawTexsOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSHORT(x);
	MALI_API_TRACE_PARAM_GLSHORT(y);
	MALI_API_TRACE_PARAM_GLSHORT(z);
	MALI_API_TRACE_PARAM_GLSHORT(width);
	MALI_API_TRACE_PARAM_GLSHORT(height);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDrawTexsOES( ctx, ( GLfloat ) x, ( GLfloat ) y, ( GLfloat ) z, ( GLfloat ) width, ( GLfloat ) height );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawTexsOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDrawTexsvOES )(const GLshort *coords)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDrawTexsvOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSHORT_ARRAY(MALI_API_TRACE_IN, coords, 5); /* x, y, z, width, height */

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDrawTexsvOES( ctx, ( GLfloat ) coords[ 0 ], ( GLfloat ) coords[ 1 ], ( GLfloat ) coords[ 2 ], ( GLfloat ) coords[ 3 ], ( GLfloat ) coords[ 4 ] );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawTexsvOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDrawTexxOES )(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDrawTexxOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(x);
	MALI_API_TRACE_PARAM_GLFIXED(y);
	MALI_API_TRACE_PARAM_GLFIXED(z);
	MALI_API_TRACE_PARAM_GLFIXED(width);
	MALI_API_TRACE_PARAM_GLFIXED(height);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDrawTexsOES( ctx,
	                                     _gles_convert_element_to_ftype( ( void * ) &x, 0, GLES_FIXED ),
	                                     _gles_convert_element_to_ftype( ( void * ) &y, 0, GLES_FIXED ),
	                                     _gles_convert_element_to_ftype( ( void * ) &z, 0, GLES_FIXED ),
	                                     _gles_convert_element_to_ftype( ( void * ) &width, 0, GLES_FIXED ),
	                                     _gles_convert_element_to_ftype( ( void * ) &height, 0, GLES_FIXED ) );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawTexxOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDrawTexxvOES )(const GLfixed *coords)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glDrawTexxvOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, coords, 5); /* x, y, z, width, height */

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDrawTexxvOES( ctx,
	                                      _gles_convert_element_to_ftype( ( void * ) coords, 0, GLES_FIXED ),
	                                      _gles_convert_element_to_ftype( ( void * ) coords, 1, GLES_FIXED ),
	                                      _gles_convert_element_to_ftype( ( void * ) coords, 2, GLES_FIXED ),
	                                      _gles_convert_element_to_ftype( ( void * ) coords, 3, GLES_FIXED ),
	                                      _gles_convert_element_to_ftype( ( void * ) coords, 4, GLES_FIXED ) );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDrawTexxvOES);
	GLES_API_LEAVE(ctx);
}
#endif

MALI_GL_APICALL void MALI_GL_APIENTRY( glEnableClientState )( GLenum cap )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glEnableClientState);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(cap);

	/* No locking needed */
	err = ctx->vtable->fp_glEnableClientState( ctx, cap, (GLboolean)GL_TRUE );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glEnableClientState);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glFogf )( GLenum pname, GLfloat param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glFogf);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT(param);

	/* No locking needed */
	err = ctx->vtable->fp_glFogf( ctx, pname, ( GLvoid * ) &param, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFogf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFogfv )( GLenum pname, const GLfloat *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glFogfv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, params, pname == GL_FOG_COLOR ? 4 : 1);

	/* No locking needed */
	err = ctx->vtable->fp_glFogfv( ctx, pname, ( GLvoid * ) params, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFogfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFogx )( GLenum pname, GLfixed param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glFogx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED(param);

	/* No locking needed */
	err = ctx->vtable->fp_glFogx( ctx, pname, ( GLvoid * ) &param, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFogx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFogxv )( GLenum pname, const GLfixed *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glFogxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, params, pname == GL_FOG_COLOR ? 4 : 1);

	/* No locking needed */
	err = ctx->vtable->fp_glFogxv( ctx, pname, ( GLvoid * ) params, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFogxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glFrustumf )(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glFrustumf);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(l);
	MALI_API_TRACE_PARAM_GLFLOAT(r);
	MALI_API_TRACE_PARAM_GLFLOAT(b);
	MALI_API_TRACE_PARAM_GLFLOAT(t);
	MALI_API_TRACE_PARAM_GLFLOAT(n);
	MALI_API_TRACE_PARAM_GLFLOAT(f);

	/* No locking needed */
	err = ctx->vtable->fp_glFrustumf(ctx, FLOAT_TO_FTYPE(l), FLOAT_TO_FTYPE(r),
			FLOAT_TO_FTYPE(b), FLOAT_TO_FTYPE(t),
			FLOAT_TO_FTYPE(n), FLOAT_TO_FTYPE(f));
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFrustumf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFrustumx )(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glFrustumx);
	GLES1_API_ENTER(ctx);

	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(l);
	MALI_API_TRACE_PARAM_GLFIXED(r);
	MALI_API_TRACE_PARAM_GLFIXED(b);
	MALI_API_TRACE_PARAM_GLFIXED(t);
	MALI_API_TRACE_PARAM_GLFIXED(n);
	MALI_API_TRACE_PARAM_GLFIXED(f);

	/* No locking needed */
	err = ctx->vtable->fp_glFrustumx(ctx, FIXED_TO_FTYPE(l), FIXED_TO_FTYPE(r),
			FIXED_TO_FTYPE(b), FIXED_TO_FTYPE(t),
			FIXED_TO_FTYPE(n), FIXED_TO_FTYPE(f));
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFrustumx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetClipPlanef )(GLenum plane, GLfloat *equation)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetClipPlanef);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(plane);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_OUT, equation, 4);

	/* No locking needed */
	err = ctx->vtable->fp_glGetClipPlanef( &ctx->state, plane, equation, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(2, (GL_NO_ERROR == err) ? equation : NULL, 4);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetClipPlanef);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetClipPlanex )( GLenum plane, GLfixed *equation)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetClipPlanex);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(plane);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_OUT, equation, 4);

	/* No locking needed */
	err = ctx->vtable->fp_glGetClipPlanex( &ctx->state, plane, equation, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFIXED_ARRAY(2, (GL_NO_ERROR == err) ? equation : NULL, 4);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetClipPlanex);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetFixedv )(GLenum pname, GLfixed *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetFixedv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_get_getter_out_count(ctx, pname));

	/* No locking needed */
	err = ctx->vtable->fp_glGetFixedv(ctx, pname, params, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFIXED_ARRAY(2, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_get_getter_out_count(ctx, pname));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetFixedv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetLightfv )( GLenum light, GLenum pname, GLfloat *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetLightfv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(light);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_light_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glGetLightfv( &ctx->state, light, pname, params, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_light_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetLightfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetLightxv )( GLenum light, GLenum pname, GLfixed *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetLightxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(light);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_light_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glGetLightxv( &ctx->state, light, pname, params, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFIXED_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_light_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetLightxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetMaterialfv )( GLenum face, GLenum pname, GLfloat *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetMaterialfv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(face);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_material_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glGetMaterialfv( &ctx->state, face, pname, params, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_material_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetMaterialfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetMaterialxv )( GLenum face, GLenum pname, GLfixed *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetMaterialxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(face);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_material_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glGetMaterialxv( &ctx->state, face, pname, params, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFIXED_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_material_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetMaterialxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetPointerv )( GLenum pname, GLvoid **pointer )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetPointerv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER_ARRAY(MALI_API_TRACE_OUT, pointer, 1);

	/* No locking needed */
	err = ctx->vtable->fp_glGetPointerv( &ctx->state, pname, pointer );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_VERTEXATTRIBARRAYPOINTER_ARRAY(2, (GL_NO_ERROR == err) ? pointer : NULL, 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetPointerv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetTexEnvfv )(GLenum target, GLenum pname, GLfloat *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetTexEnvfv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_texenv_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glGetTexEnvfv(&ctx->state, target, pname, params, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_texenv_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetTexEnvfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetTexEnviv )(GLenum target, GLenum pname, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetTexEnviv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_texenv_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glGetTexEnviv(&ctx->state, target, pname, params, GLES_INT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_texenv_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetTexEnviv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetTexEnvxv )(GLenum target, GLenum pname, GLfixed *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetTexEnvxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_texenv_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glGetTexEnvxv(&ctx->state, target, pname, params, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFIXED_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_texenv_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetTexEnvxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetTexParameterxv )(GLenum target, GLenum pname, GLfixed *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glGetTexParameterxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_gl_texparameter_count(pname));

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetTexParameterxv(&ctx->state.common, target, pname, params, GLES_FIXED );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFIXED_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_gl_texparameter_count(pname));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetTexParameterxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glLightf )(GLenum light, GLenum pname, GLfloat param)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLightf);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(light);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT(param);

	/* No locking needed */
	err = ctx->vtable->fp_glLightf( ctx, light, pname, (GLvoid *) &param, GLES_FLOAT);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLightf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLightfv )(GLenum light, GLenum pname, const GLfloat *param)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLightfv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(light);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, param, mali_api_trace_gl_light_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glLightfv( ctx, light, pname, (GLvoid *)param, GLES_FLOAT);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLightfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glLightModelf )( GLenum pname, GLfloat param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLightModelf);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT(param);

	/* No locking needed */
	err = ctx->vtable->fp_glLightModelf(ctx, pname, (GLvoid *) &param, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLightModelf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLightModelfv )( GLenum pname, const GLfloat *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLightModelfv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_lightmodel_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glLightModelfv(ctx, pname, (GLvoid *) params, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLightModelfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLightModelx )( GLenum pname, GLfixed param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLightModelx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED(param);

	/* No locking needed */
	err = ctx->vtable->fp_glLightModelx( ctx, pname, (GLvoid *) &param, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLightModelx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLightModelxv )( GLenum pname, const GLfixed *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLightModelxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_lightmodel_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glLightModelxv(ctx, pname, (GLvoid *) params, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLightModelxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLightx )(GLenum light, GLenum pname, GLfixed param)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLightx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(light);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED(param);

	/* No locking needed */
	err = ctx->vtable->fp_glLightx( ctx, light, pname, (GLvoid *) &param, GLES_FIXED);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLightx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLightxv )(GLenum light, GLenum pname, const GLfixed *param)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLightxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(light);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, param, mali_api_trace_gl_light_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glLightxv( ctx, light, pname, (GLvoid *)param, GLES_FIXED);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLightxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLineWidthx )( GLfixed width )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLineWidthx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(width);

	/* No locking needed */
	err = ctx->vtable->fp_glLineWidthx( ctx, FIXED_TO_FTYPE( width ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLineWidthx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLoadIdentity )( void )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLoadIdentity);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();

	/* No locking needed */
	ctx->vtable->fp_glLoadIdentity(ctx);

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLoadIdentity);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLoadMatrixf )( const GLfloat *m )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLoadMatrixf);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, m, 16);

	/* No locking needed */
	ctx->vtable->fp_glLoadMatrixf(ctx, m);

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLoadMatrixf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLoadMatrixx )( const GLfixed *m )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLoadMatrixx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, m, 16);

	/* No locking needed */
	ctx->vtable->fp_glLoadMatrixx(ctx, m);

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLoadMatrixx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLogicOp )( GLenum opcode )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLogicOp);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(opcode);

	/* No locking needed */
	err = ctx->vtable->fp_glLogicOp( ctx, opcode );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLogicOp);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glMaterialf )( GLenum face, GLenum pname, GLfloat param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMaterialf);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(face);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT(param);

	/* No locking needed */
	err = ctx->vtable->fp_glMaterialf(ctx, face, pname, (GLvoid *) &param, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMaterialf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glMaterialfv )( GLenum face, GLenum pname, const GLfloat *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMaterialfv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(face);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_material_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glMaterialfv(ctx, face, pname, (GLvoid *) params, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMaterialfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glMaterialx )( GLenum face, GLenum pname, GLfixed param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMaterialx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(face);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED(param);

	/* No locking needed */
	err = ctx->vtable->fp_glMaterialx(ctx, face, pname, (GLvoid *) &param, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMaterialx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glMaterialxv )( GLenum face, GLenum pname, const GLfixed *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMaterialxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(face);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_material_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glMaterialxv(ctx, face, pname, (GLvoid *) params, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMaterialxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glMatrixMode )(GLenum mode)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMatrixMode);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(mode);

	/* No locking needed */
	err = ctx->vtable->fp_glMatrixMode(&ctx->state, mode);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMatrixMode);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glMultiTexCoord4b )( GLenum target, GLbyte s, GLbyte t, GLbyte r, GLbyte q )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMultiTexCoord4b);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLBYTE(s);
	MALI_API_TRACE_PARAM_GLBYTE(t);
	MALI_API_TRACE_PARAM_GLBYTE(r);
	MALI_API_TRACE_PARAM_GLBYTE(q);

	/* No locking needed */
	err = ctx->vtable->fp_glMultiTexCoord4b( &ctx->state.api.gles1->current, target,
			FIXED_TO_FTYPE( BYTE_TO_FIXED( s ) ),
			FIXED_TO_FTYPE( BYTE_TO_FIXED( t ) ),
			FIXED_TO_FTYPE( BYTE_TO_FIXED( r ) ),
			FIXED_TO_FTYPE( BYTE_TO_FIXED( q ) ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMultiTexCoord4b);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glMultiTexCoord4f )( GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMultiTexCoord4f);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLFLOAT(s);
	MALI_API_TRACE_PARAM_GLFLOAT(t);
	MALI_API_TRACE_PARAM_GLFLOAT(r);
	MALI_API_TRACE_PARAM_GLFLOAT(q);

	/* No locking needed */
	err = ctx->vtable->fp_glMultiTexCoord4f( &ctx->state.api.gles1->current, target,
			FLOAT_TO_FTYPE( s ), FLOAT_TO_FTYPE( t ),
			FLOAT_TO_FTYPE( r ), FLOAT_TO_FTYPE( q ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMultiTexCoord4f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glMultiTexCoord4x )( GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMultiTexCoord4x);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLFIXED(s);
	MALI_API_TRACE_PARAM_GLFIXED(t);
	MALI_API_TRACE_PARAM_GLFIXED(r);
	MALI_API_TRACE_PARAM_GLFIXED(q);

	/* No locking needed */
	err = ctx->vtable->fp_glMultiTexCoord4x( &ctx->state.api.gles1->current, target,
			FIXED_TO_FTYPE( s ), FIXED_TO_FTYPE( t ),
			FIXED_TO_FTYPE( r ), FIXED_TO_FTYPE( q ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMultiTexCoord4x);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glMultMatrixf )( const GLfloat *m )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMultMatrixf);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, m, 16);

	/* No locking needed */
	ctx->vtable->fp_glMultMatrixf(ctx, m);

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMultMatrixf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glMultMatrixx )( const GLfixed *m )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glMultMatrixx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, m, 16);

	/* No locking needed */
	ctx->vtable->fp_glMultMatrixx(ctx, m);

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMultMatrixx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glNormal3f )( GLfloat nx, GLfloat ny, GLfloat nz )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glNormal3f);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(nx);
	MALI_API_TRACE_PARAM_GLFLOAT(ny);
	MALI_API_TRACE_PARAM_GLFLOAT(nz);

	/* No locking needed */
	err = ctx->vtable->fp_glNormal3f( &ctx->state.api.gles1->current,
			FLOAT_TO_FTYPE( nx ), FLOAT_TO_FTYPE( ny ), FLOAT_TO_FTYPE( nz ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glNormal3f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glNormal3x )( GLfixed nx, GLfixed ny, GLfixed nz )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glNormal3x);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(nx);
	MALI_API_TRACE_PARAM_GLFIXED(ny);
	MALI_API_TRACE_PARAM_GLFIXED(nz);

	/* No locking needed */
	err = ctx->vtable->fp_glNormal3x( &ctx->state.api.gles1->current,
			FIXED_TO_FTYPE( nx ), FIXED_TO_FTYPE( ny ), FIXED_TO_FTYPE( nz ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glNormal3x);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glNormalPointer )(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glNormalPointer);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLSIZEI(stride);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(pointer);

	/* No locking needed */
	err = ctx->vtable->fp_glNormalPointer(ctx, type, stride, pointer);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glNormalPointer);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glOrthof )(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glOrthof);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(l);
	MALI_API_TRACE_PARAM_GLFLOAT(r);
	MALI_API_TRACE_PARAM_GLFLOAT(b);
	MALI_API_TRACE_PARAM_GLFLOAT(t);
	MALI_API_TRACE_PARAM_GLFLOAT(n);
	MALI_API_TRACE_PARAM_GLFLOAT(f);

	/* No locking needed */
	err = ctx->vtable->fp_glOrthof(ctx,	FLOAT_TO_FTYPE(l), FLOAT_TO_FTYPE(r),
			FLOAT_TO_FTYPE(b), FLOAT_TO_FTYPE(t),
			FLOAT_TO_FTYPE(n), FLOAT_TO_FTYPE(f));
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glOrthof);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glOrthox )(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glOrthox);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(l);
	MALI_API_TRACE_PARAM_GLFIXED(r);
	MALI_API_TRACE_PARAM_GLFIXED(b);
	MALI_API_TRACE_PARAM_GLFIXED(t);
	MALI_API_TRACE_PARAM_GLFIXED(n);
	MALI_API_TRACE_PARAM_GLFIXED(f);

	/* No locking needed */
	err = ctx->vtable->fp_glOrthox(ctx,	FIXED_TO_FTYPE(l), FIXED_TO_FTYPE(r),
			FIXED_TO_FTYPE(b), FIXED_TO_FTYPE(t),
			FIXED_TO_FTYPE(n), FIXED_TO_FTYPE(f));
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glOrthox);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glPointParameterf )( GLenum pname, GLfloat param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPointParameterf);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT(param);

	/* No locking needed */
	err = ctx->vtable->fp_glPointParameterf( ctx, pname, (GLvoid *) &param, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPointParameterf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glPointParameterfv )( GLenum pname, const GLfloat *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPointParameterfv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_pointparameter_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glPointParameterfv( ctx, pname, (GLvoid *) params, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPointParameterfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glPointParameterx )( GLenum pname, GLfixed param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPointParameterx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED(param);

	/* No locking needed */
	err = ctx->vtable->fp_glPointParameterx( ctx, pname, (GLvoid *) &param, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPointParameterx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glPointParameterxv )( GLenum pname, const GLfixed *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPointParameterxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_pointparameter_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glPointParameterxv( ctx, pname, (GLvoid *) params, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPointParameterxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glPointSize )( GLfloat size )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPointSize);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(size);

	/* No locking needed */
	err = ctx->vtable->fp_glPointSize( &ctx->state.common.rasterization, FLOAT_TO_FTYPE( size ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPointSize);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glPointSizex )( GLfixed size )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPointSizex);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(size);

	/* No locking needed */
	err = ctx->vtable->fp_glPointSizex( &ctx->state.common.rasterization, FIXED_TO_FTYPE( size ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPointSizex);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glPolygonOffsetx )( GLfixed factor, GLfixed units )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPolygonOffsetx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(factor);
	MALI_API_TRACE_PARAM_GLFIXED(units);

	/* No locking needed */
	err = ctx->vtable->fp_glPolygonOffsetx( ctx, FIXED_TO_FTYPE( factor ), FIXED_TO_FTYPE( units ) );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPolygonOffsetx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glPopMatrix )( void )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPopMatrix);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();

	/* No locking needed */
	err = ctx->vtable->fp_glPopMatrix(ctx);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPopMatrix);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glPushMatrix )( void )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPushMatrix);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();

	/* No locking needed */
	err = ctx->vtable->fp_glPushMatrix(ctx);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPushMatrix);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glRotatef )(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glRotatef);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(angle);
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);
	MALI_API_TRACE_PARAM_GLFLOAT(z);

	/* No locking needed */
	ctx->vtable->fp_glRotatef(ctx, FLOAT_TO_FTYPE(angle),
			FLOAT_TO_FTYPE(x), FLOAT_TO_FTYPE(y), FLOAT_TO_FTYPE(z));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glRotatef);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glRotatex )(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glRotatex);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(angle);
	MALI_API_TRACE_PARAM_GLFIXED(x);
	MALI_API_TRACE_PARAM_GLFIXED(y);
	MALI_API_TRACE_PARAM_GLFIXED(z);

	/* No locking needed */
	ctx->vtable->fp_glRotatex(ctx, FIXED_TO_FTYPE(angle),
			FIXED_TO_FTYPE(x), FIXED_TO_FTYPE(y), FIXED_TO_FTYPE(z));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glRotatex);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glSampleCoveragex )( GLclampx value, GLboolean invert )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glSampleCoveragex);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLCLAMPX(value);
	MALI_API_TRACE_PARAM_GLBOOLEAN(invert);

	/* No locking needed */
	ctx->vtable->fp_glSampleCoveragex( ctx, FIXED_TO_FTYPE(value), invert );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glSampleCoveragex);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glScalef )(GLfloat x, GLfloat y, GLfloat z)
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glScalef);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);
	MALI_API_TRACE_PARAM_GLFLOAT(z);

	/* No locking needed */
	ctx->vtable->fp_glScalef(ctx, FLOAT_TO_FTYPE(x), FLOAT_TO_FTYPE(y), FLOAT_TO_FTYPE(z));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glScalef);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glScalex )(GLfixed x, GLfixed y, GLfixed z)
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glScalex);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(x);
	MALI_API_TRACE_PARAM_GLFIXED(y);
	MALI_API_TRACE_PARAM_GLFIXED(z);

	/* No locking needed */
	ctx->vtable->fp_glScalex(ctx, FIXED_TO_FTYPE(x), FIXED_TO_FTYPE(y), FIXED_TO_FTYPE(z));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glScalex);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glShadeModel )(GLenum mode )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glShadeModel);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(mode);

	/* No locking needed */
	err = ctx->vtable->fp_glShadeModel( ctx, mode);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glShadeModel);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexCoordPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexCoordPointer);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(size);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLSIZEI(stride);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(pointer);

	/* No locking needed */
	err = ctx->vtable->fp_glTexCoordPointer(ctx, size, type, stride, pointer);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexCoordPointer);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glTexEnvf )( GLenum target, GLenum pname, GLfloat param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexEnvf);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT(param);

	/* No locking needed */
	err = ctx->vtable->fp_glTexEnvf( ctx, target, pname, ( GLvoid * ) &param, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexEnvf);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexEnvfv )( GLenum target, GLenum pname, const GLfloat *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexEnvfv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_texenv_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glTexEnvfv( ctx, target, pname, ( GLvoid * ) params, GLES_FLOAT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexEnvfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexEnvi )( GLenum target, GLenum pname, GLint param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexEnvi);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT(param);

	/* No locking needed */
	err = ctx->vtable->fp_glTexEnvi( ctx, target, pname, ( GLvoid * ) &param, GLES_INT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexEnvi);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexEnviv )( GLenum target, GLenum pname, const GLint *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexEnviv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_texenv_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glTexEnviv( ctx, target, pname, ( GLvoid * ) params, GLES_INT );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexEnviv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexEnvx )( GLenum target, GLenum pname, GLfixed param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexEnvx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED(param);

	/* No locking needed */
	err = ctx->vtable->fp_glTexEnvx( ctx, target, pname, ( GLvoid * ) &param, GLES_FIXED );
	if (GL_NO_ERROR != err ) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexEnvx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexEnvxv )( GLenum target, GLenum pname, const GLfixed *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexEnvxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_texenv_count(pname));

	/* No locking needed */
	err = ctx->vtable->fp_glTexEnvxv( ctx, target, pname, ( GLvoid * ) params, GLES_FIXED );
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexEnvxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexParameterx )(GLenum target, GLenum pname, GLfixed param )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexParameterx);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED(param);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glTexParameterx( &ctx->state.common.texture_env,
			target, pname, (GLvoid *) &param, GLES_FIXED );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexParameterx);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexParameterxv )(GLenum target, GLenum pname, const GLfixed *params )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTexParameterxv);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_IN, params, mali_api_trace_gl_texparameter_count(pname));

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glTexParameterxv( &ctx->state.common.texture_env,
			target, pname, (GLvoid *) params, GLES_FIXED );
	_gles_share_lists_unlock( ctx->share_lists );

	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexParameterxv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glTranslatef )(GLfloat x, GLfloat y, GLfloat z)
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTranslatef);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);
	MALI_API_TRACE_PARAM_GLFLOAT(z);

	/* No locking needed */
	ctx->vtable->fp_glTranslatef(ctx, FLOAT_TO_FTYPE(x), FLOAT_TO_FTYPE(y), FLOAT_TO_FTYPE(z));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTranslatef);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTranslatex )(GLfixed x, GLfixed y, GLfixed z)
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glTranslatex);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED(x);
	MALI_API_TRACE_PARAM_GLFIXED(y);
	MALI_API_TRACE_PARAM_GLFIXED(z);

	/* No locking needed */
	ctx->vtable->fp_glTranslatex(ctx, FIXED_TO_FTYPE(x), FIXED_TO_FTYPE(y), FIXED_TO_FTYPE(z));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTranslatex);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glVertexPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glVertexPointer);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(size);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLSIZEI(stride);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(pointer);

	/* No locking needed */
	err = ctx->vtable->fp_glVertexPointer(ctx, size, type, stride, pointer);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexPointer);
	GLES_API_LEAVE(ctx);
}

#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY( glCurrentPaletteMatrixOES )( GLuint index )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glCurrentPaletteMatrixOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(index);

	/* No locking needed */
	err = ctx->vtable->fp_glCurrentPaletteMatrixOES(&(ctx->state.api.gles1->transform), index);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCurrentPaletteMatrixOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLoadPaletteFromModelViewMatrixOES )( void )
{
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glLoadPaletteFromModelViewMatrixOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();

	/* No locking needed */
	ctx->vtable->fp_glLoadPaletteFromModelViewMatrixOES(ctx, &(ctx->state.api.gles1->transform));

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLoadPaletteFromModelViewMatrixOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glMatrixIndexPointerOES )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();

	if (NULL == ctx) return;
	
	MALI_PROFILING_ENTER_FUNC(glMatrixIndexPointerOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(size);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLSIZEI(stride);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(pointer);

	/* No locking needed */
	err = ctx->vtable->fp_glMatrixIndexPointerOES(ctx, size, type, stride, pointer);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glMatrixIndexPointerOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glWeightPointerOES )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glWeightPointerOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(size);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLSIZEI(stride);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(pointer);

	/* No locking needed */
	err = ctx->vtable->fp_glWeightPointerOES(ctx, size, type, stride, pointer);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glWeightPointerOES);
	GLES_API_LEAVE(ctx);
}

#endif /* EXTENSION_PALETTE_MATRIX_OES_ENABLE */


#if EXTENSION_POINT_SIZE_ARRAY_OES_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY( glPointSizePointerOES )(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glPointSizePointerOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLSIZEI(stride);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(pointer);

	/* No locking needed */
	err = ctx->vtable->fp_glPointSizePointerOES(ctx, type, stride, pointer);
	if (GL_NO_ERROR != err) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glPointSizePointerOES);
	GLES_API_LEAVE(ctx);
}
#endif /* EXTENSION_POINT_SIZE_ARRAY_OES_ENABLE */


#if EXTENSION_QUERY_MATRIX_OES_ENABLE
MALI_GL_APICALL GLbitfield MALI_GL_APIENTRY( glQueryMatrixxOES )(GLfixed * mantissa, GLint * exponent)
{
	GLbitfield ret;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return 0;

	MALI_PROFILING_ENTER_FUNC(glQueryMatrixxOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLFIXED_ARRAY(MALI_API_TRACE_OUT, mantissa, 16);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, exponent, 16);

	/* No locking needed */
	ret = ctx->vtable->fp_glQueryMatrixxOES(ctx, mantissa, exponent);

	MALI_API_TRACE_RETURN_GLFIXED_ARRAY(1, mantissa, 16);
	MALI_API_TRACE_RETURN_GLINT_ARRAY(2, exponent, 16);
	MALI_API_TRACE_RETURN_GLBITFIELD(ret);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glQueryMatrixxOES);
	GLES_API_LEAVE(ctx);

	return ret;
}
#endif

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY( glGenFramebuffersOES )(GLsizei n, GLuint *framebuffers)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;
	
	MALI_PROFILING_ENTER_FUNC(glGenFramebuffersOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_OUT, framebuffers, n > 0 ? n : 0);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGenFramebuffers( ctx->share_lists->framebuffer_object_list, n, framebuffers, WRAPPER_FRAMEBUFFER);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLUINT_ARRAY(2, (GL_NO_ERROR == err) ? framebuffers: NULL, n > 0 ? n : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGenFramebuffersOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLboolean MALI_GL_APIENTRY( glIsRenderbufferOES )(GLuint renderbuffer)
{
	GLboolean retval = GL_FALSE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return retval;

	MALI_PROFILING_ENTER_FUNC(glIsRenderbufferOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(renderbuffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	retval = ctx->vtable->fp_glIsRenderbuffer( ctx->share_lists->renderbuffer_object_list, renderbuffer );
	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_RETURN_GLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glIsRenderbufferOES);
	GLES_API_LEAVE(ctx);

	return retval;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBindRenderbufferOES )(GLenum target, GLuint renderbuffer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glBindRenderbufferOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLUINT(renderbuffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glBindRenderbuffer( ctx->share_lists->renderbuffer_object_list, &ctx->state.common.renderbuffer, target, renderbuffer );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBindRenderbufferOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDeleteRenderbuffersOES )(GLsizei n, const GLuint *renderbuffers)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDeleteRenderbuffersOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_IN, renderbuffers, n > 0 ? n : 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDeleteRenderbuffers( ctx->share_lists->renderbuffer_object_list,
			&ctx->state.common.renderbuffer, &ctx->state.common.framebuffer, n, renderbuffers );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDeleteRenderbuffersOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGenRenderbuffersOES )(GLsizei n, GLuint *renderbuffers)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGenRenderbuffersOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_OUT, renderbuffers, n > 0 ? n : 0);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGenRenderbuffers( ctx->share_lists->renderbuffer_object_list, n, renderbuffers, WRAPPER_RENDERBUFFER);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLUINT_ARRAY(2, (GL_NO_ERROR == err) ? renderbuffers : NULL, n > 0 ? n : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGenRenderbuffersOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glRenderbufferStorageOES )(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glRenderbufferStorageOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(internalformat);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glRenderbufferStorage( ctx->base_ctx, ctx->share_lists->renderbuffer_object_list, &ctx->state.common.renderbuffer, target, internalformat, width, height );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glRenderbufferStorageOES);
	GLES_API_LEAVE(ctx);
}


MALI_GL_APICALL void MALI_GL_APIENTRY( glGetRenderbufferParameterivOES )(GLenum target, GLenum pname, GLint* params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetRenderbufferParameterivOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, 1);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetRenderbufferParameteriv( &ctx->state.common.renderbuffer, target, pname, params );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetRenderbufferParameterivOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLboolean MALI_GL_APIENTRY( glIsFramebufferOES )(GLuint framebuffer)
{
	GLboolean retval = GL_FALSE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return retval;

	MALI_PROFILING_ENTER_FUNC(glIsFramebufferOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(framebuffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	retval = ctx->vtable->fp_glIsFramebuffer( ctx->share_lists->framebuffer_object_list, framebuffer );
	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_RETURN_GLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glIsFramebufferOES);
	GLES_API_LEAVE(ctx);

	return retval;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBindFramebufferOES )(GLenum target, GLuint framebuffer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glBindFramebufferOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLUINT(framebuffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glBindFramebuffer( ctx, ctx->share_lists->framebuffer_object_list, &ctx->state.common.framebuffer, target, framebuffer );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBindFramebufferOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDeleteFramebuffersOES )(GLsizei n, const GLuint *framebuffers)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDeleteFramebuffersOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_IN, framebuffers, n > 0 ? n : 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDeleteFramebuffers( ctx, n, framebuffers );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDeleteFramebuffersOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLenum MALI_GL_APIENTRY( glCheckFramebufferStatusOES )(GLenum target)
{
	GLenum err;
	GLenum retval = GL_INVALID_VALUE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return GL_INVALID_OPERATION;

	MALI_PROFILING_ENTER_FUNC(glCheckFramebufferStatusOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glCheckFramebufferStatus(
		&ctx->state.common.framebuffer,
		target,
		&retval
	);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLENUM(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCheckFramebufferStatusOES);
	GLES_API_LEAVE(ctx);

	return retval;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFramebufferTexture2DOES )(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glFramebufferTexture2DOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(attachment);
	MALI_API_TRACE_PARAM_GLENUM(textarget);
	MALI_API_TRACE_PARAM_GLUINT(texture);
	MALI_API_TRACE_PARAM_GLINT(level);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glFramebufferTexture2D(
		ctx,
		&ctx->state.common.framebuffer,
		ctx->share_lists->framebuffer_object_list,
		ctx->share_lists->renderbuffer_object_list,
		ctx->share_lists->texture_object_list,
		target,
		attachment,
		textarget,
		texture,
		level
	);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFramebufferTexture2DOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFramebufferRenderbufferOES )(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glFramebufferRenderbufferOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(attachment);
	MALI_API_TRACE_PARAM_GLENUM(renderbuffertarget);
	MALI_API_TRACE_PARAM_GLUINT(renderbuffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glFramebufferRenderbuffer(
		&ctx->state.common.framebuffer,
		ctx->share_lists->framebuffer_object_list,
		ctx->share_lists->renderbuffer_object_list,
		ctx->share_lists->texture_object_list,
		target,
		attachment,
		renderbuffertarget,
		renderbuffer
	);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFramebufferRenderbufferOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetFramebufferAttachmentParameterivOES )(GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetFramebufferAttachmentParameterivOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(attachment);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, 1);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetFramebufferAttachmentParameteriv(
		&ctx->state.common.framebuffer,
		ctx->share_lists->framebuffer_object_list,
		target,
		attachment,
		pname,
		params
	);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(4, (GL_NO_ERROR == err) ? params : NULL, 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetFramebufferAttachmentParameterivOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGenerateMipmapOES )(GLenum target)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGenerateMipmapOES);
	GLES1_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGenerateMipmap( ctx, &ctx->state.common.texture_env, ctx->share_lists->texture_object_list, target);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGenerateMipmapOES);
	GLES_API_LEAVE(ctx);
}

#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */

#if EXTENSION_TEXTURE_CUBE_MAP_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY( glTexGenfOES )(GLenum coord, GLenum pname, GLfloat param)
{
        GLenum err;
        gles_context *ctx = GLES_GET_CONTEXT();
        if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glTexGenfOES);
        GLES1_API_ENTER(ctx);
        MALI_API_TRACE_FUNC_BEGIN();
        MALI_API_TRACE_PARAM_GLENUM(coord);
        MALI_API_TRACE_PARAM_GLENUM(pname);
        MALI_API_TRACE_PARAM_GLFLOAT(param);

        MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

        _gles_share_lists_lock( ctx->share_lists );
        err = ctx->vtable->fp_glTexGenfOES( ctx, coord, pname, param );
        _gles_share_lists_unlock( ctx->share_lists );

        if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

        MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexGenfOES);
        GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexGenfvOES )(GLenum coord, GLenum pname, const GLfloat *params)
{
        GLenum err;
        gles_context *ctx = GLES_GET_CONTEXT();
        if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glTexGenfvOES);
        GLES1_API_ENTER(ctx);
        MALI_API_TRACE_FUNC_BEGIN();
        MALI_API_TRACE_PARAM_GLENUM(coord);
        MALI_API_TRACE_PARAM_GLENUM(pname);

        MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

        _gles_share_lists_lock( ctx->share_lists );
        err = ctx->vtable->fp_glTexGenfvOES( ctx, coord, pname, params );
        _gles_share_lists_unlock( ctx->share_lists );

        if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

        MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexGenfvOES);
        GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexGeniOES )(GLenum coord, GLenum pname, GLint param)
{
        GLenum err;
        gles_context *ctx = GLES_GET_CONTEXT();
        if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glTexGeniOES);
        GLES1_API_ENTER(ctx);
        MALI_API_TRACE_FUNC_BEGIN();
        MALI_API_TRACE_PARAM_GLENUM(coord);
        MALI_API_TRACE_PARAM_GLENUM(pname);
        MALI_API_TRACE_PARAM_GLINT(param);

        MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

        _gles_share_lists_lock( ctx->share_lists );
        err = ctx->vtable->fp_glTexGeniOES( ctx, coord, pname, param );
        _gles_share_lists_unlock( ctx->share_lists );

        if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

        MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexGeniOES);
        GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexGenivOES )(GLenum coord, GLenum pname, const GLint *params)
{
        GLenum err;
        gles_context *ctx = GLES_GET_CONTEXT();
        if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glTexGenivOES);
        GLES1_API_ENTER(ctx);
        MALI_API_TRACE_FUNC_BEGIN();
        MALI_API_TRACE_PARAM_GLENUM(coord);
        MALI_API_TRACE_PARAM_GLENUM(pname);

        MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

        _gles_share_lists_lock( ctx->share_lists );
        err = ctx->vtable->fp_glTexGenivOES( ctx, coord, pname, params );
        _gles_share_lists_unlock( ctx->share_lists );

        if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

        MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexGenivOES);
        GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexGenxOES )(GLenum coord, GLenum pname, GLfixed param)
{
        GLenum err;
        gles_context *ctx = GLES_GET_CONTEXT();
        if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glTexGenxOES);
        GLES1_API_ENTER(ctx);
        MALI_API_TRACE_FUNC_BEGIN();
        MALI_API_TRACE_PARAM_GLENUM(coord);
        MALI_API_TRACE_PARAM_GLENUM(pname);
        MALI_API_TRACE_PARAM_GLFIXED(param);

        MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

        _gles_share_lists_lock( ctx->share_lists );
        err = ctx->vtable->fp_glTexGenxOES( ctx, coord, pname, param );
        _gles_share_lists_unlock( ctx->share_lists );

        if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

        MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexGenxOES);
        GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glTexGenxvOES )(GLenum coord, GLenum pname, const GLfixed *params)
{
        GLenum err;
        gles_context *ctx = GLES_GET_CONTEXT();
        if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glTexGenxvOES);
        GLES1_API_ENTER(ctx);
        MALI_API_TRACE_FUNC_BEGIN();
        MALI_API_TRACE_PARAM_GLENUM(coord);
        MALI_API_TRACE_PARAM_GLENUM(pname);

        MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

        _gles_share_lists_lock( ctx->share_lists );
        err = ctx->vtable->fp_glTexGenxvOES( ctx, coord, pname, params );
        _gles_share_lists_unlock( ctx->share_lists );

        if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

        MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glTexGenxvOES);
        GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetTexGenfvOES )(GLenum coord, GLenum pname, GLfloat *params)
{
        GLenum err;
        gles_context *ctx = GLES_GET_CONTEXT();
        if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetTexGenfvOES);
        GLES1_API_ENTER(ctx);
        MALI_API_TRACE_FUNC_BEGIN();
        MALI_API_TRACE_PARAM_GLENUM(coord);
        MALI_API_TRACE_PARAM_GLENUM(pname);

        MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

        _gles_share_lists_lock( ctx->share_lists );
        err = ctx->vtable->fp_glGetTexGenfvOES( ctx, coord, pname, params );
        _gles_share_lists_unlock( ctx->share_lists );

        if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

        MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetTexGenfvOES);
        GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetTexGenivOES )(GLenum coord, GLenum pname, GLint *params)
{
        GLenum err;
        gles_context *ctx = GLES_GET_CONTEXT();
        if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetTexGenivOES);
        GLES1_API_ENTER(ctx);
        MALI_API_TRACE_FUNC_BEGIN();
        MALI_API_TRACE_PARAM_GLENUM(coord);
        MALI_API_TRACE_PARAM_GLENUM(pname);

        MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

        _gles_share_lists_lock( ctx->share_lists );
        err = ctx->vtable->fp_glGetTexGenivOES( ctx, coord, pname, params );
        _gles_share_lists_unlock( ctx->share_lists );

        if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

        MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetTexGenivOES);
        GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetTexGenxvOES )(GLenum coord, GLenum pname, GLfixed *params)
{
        GLenum err;
        gles_context *ctx = GLES_GET_CONTEXT();
        if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetTexGenxvOES);
        GLES1_API_ENTER(ctx);
        MALI_API_TRACE_FUNC_BEGIN();
        MALI_API_TRACE_PARAM_GLENUM(coord);
        MALI_API_TRACE_PARAM_GLENUM(pname);

        MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

        _gles_share_lists_lock( ctx->share_lists );
        err = ctx->vtable->fp_glGetTexGenxvOES( ctx, coord, pname, params );
        _gles_share_lists_unlock( ctx->share_lists );

        if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

        MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetTexGenxvOES);
        GLES_API_LEAVE(ctx);
}

#endif

