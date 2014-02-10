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
 * @file gles2_entrypoints.c
 * @brief Contains the definitions of the entrypoint functions that are only present
 *        in OpenGL ES 2.x.
 */

 
#ifdef __SYMBIAN32__
/* define GL_APICALL as __declspec(dllexport) in GLES2\gl2platform.h */
#define SYMBIAN_OGLES_DLL_EXPORTS
#endif /*  __SYMBIAN32__ */

#include "gles_base.h"
#include "gles_entrypoints.h"
#include "base/mali_runtime.h"

#include "gles_context.h"
#include "gles2_state/gles2_state.h"
#include "gles_vtable.h"
#include "gles_share_lists.h"
#include "gles_object.h"
#include "gles2_api_trace.h"

#if defined(__SYMBIAN32__) 
#include "symbian_gles_trace.h"
#endif

#define GLES2_API_ENTER(ctx) do {\
		MALI_DEBUG_ASSERT_POINTER(ctx->state.api.gles2); \
		GLES_API_ENTER(ctx); \
	} while (0);

MALI_GL_APICALL void MALI_GL_APIENTRY( glAttachShader )(GLuint program, GLuint shader)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glAttachShader);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_SHADERID(shader);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glAttachShader( ctx->share_lists->program_object_list, program, shader );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLATTACHSHADER(program, shader, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glAttachShader);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBindAttribLocation )(GLuint program, GLuint index, const char *name)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;
	
	MALI_PROFILING_ENTER_FUNC(glBindAttribLocation);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLUINT(index);
	MALI_API_TRACE_PARAM_CHAR_ARRAY(MALI_API_TRACE_IN, name, NULL != name ? _mali_sys_strlen(name) + 1 : 0, 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glBindAttribLocation( ctx->share_lists->program_object_list, program, index, name );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBindAttribLocation);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glBlendColor )( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glBlendColor);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLCLAMPF(red);
	MALI_API_TRACE_PARAM_GLCLAMPF(green);
	MALI_API_TRACE_PARAM_GLCLAMPF(blue);
	MALI_API_TRACE_PARAM_GLCLAMPF(alpha);
	
	/* No locking needed */
	err = ctx->vtable->fp_glBlendColor( ctx, red, green, blue, alpha );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBlendColor);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBlendEquation )( GLenum mode )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glBlendEquation);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(mode);

	/* Locking needed */
	err = ctx->vtable->fp_glBlendEquation( ctx, mode, mode );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBlendEquation);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBlendEquationSeparate )( GLenum modeRGB, GLenum modeAlpha )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glBlendEquationSeparate);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(modeRGB);
	MALI_API_TRACE_PARAM_GLENUM(modeAlpha);
	
	/* No locking needed */
	err = ctx->vtable->fp_glBlendEquationSeparate( ctx, modeRGB, modeAlpha );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBlendEquationSeparate);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBlendFuncSeparate )( GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (NULL == ctx) return;

	MALI_PROFILING_ENTER_FUNC(glBlendFuncSeparate);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(srcRGB);
	MALI_API_TRACE_PARAM_GLENUM(dstRGB);
	MALI_API_TRACE_PARAM_GLENUM(srcAlpha);
	MALI_API_TRACE_PARAM_GLENUM(dstAlpha);

	/* No locking needed */
	err = ctx->vtable->fp_glBlendFuncSeparate( ctx, srcRGB, dstRGB, srcAlpha, dstAlpha );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBlendFuncSeparate);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glCompileShader )(GLuint shader)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glCompileShader);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_SHADERID(shader);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glCompileShader( ctx->share_lists->program_object_list, shader);
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLCOMPILESHADER(shader, ctx, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCompileShader);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLuint MALI_GL_APIENTRY( glCreateProgram )(void)
{
	GLenum err;
	GLuint program_id = 0;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return 0;

	MALI_PROFILING_ENTER_FUNC(glCreateProgram);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glCreateProgram( ctx->share_lists->program_object_list, &program_id );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLCREATEPROGRAM(program_id);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_PROGRAMID(program_id);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCreateProgram);
	GLES_API_LEAVE(ctx);

	return program_id;
}

MALI_GL_APICALL GLuint MALI_GL_APIENTRY( glCreateShader )(GLenum type)
{
	GLenum err;
	GLuint program_id = 0;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return 0;

	MALI_PROFILING_ENTER_FUNC(glCreateShader);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(type);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glCreateShader( ctx->share_lists->program_object_list, type, &program_id );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
    SYMBIAN_GPU_TRACE_GLCREATESHADER(type, program_id);
	#endif

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_SHADERID(program_id);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glCreateShader);
	GLES_API_LEAVE(ctx);

	return program_id;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDeleteProgram )(GLuint program)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDeleteProgram);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDeleteProgram( ctx->share_lists->program_object_list, program );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLDELETEPROGRAM(program);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDeleteProgram);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDeleteShader )(GLuint shader)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDeleteShader);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_SHADERID(shader);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDeleteShader( ctx->share_lists->program_object_list, shader );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLDELETESHADER(shader, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDeleteShader);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDetachShader )(GLuint program, GLuint shader)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDetachShader);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_SHADERID(shader);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDetachShader( ctx->share_lists->program_object_list, &(ctx->state.api.gles2->program_env), program, shader );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLDETACHSHADER(program, shader, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDetachShader);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDisableVertexAttribArray )(GLuint index)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDisableVertexAttribArray);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(index);

	/* No locking needed */
	err = ctx->vtable->fp_glDisableVertexAttribArray( ctx, index );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDisableVertexAttribArray);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glEnableVertexAttribArray )(GLuint index)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glEnableVertexAttribArray);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(index);

	/* No locking needed */
	err = ctx->vtable->fp_glEnableVertexAttribArray( ctx, index );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glEnableVertexAttribArray);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetActiveAttrib )(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, char *name)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetActiveAttrib);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLUINT(index);
	MALI_API_TRACE_PARAM_GLSIZEI(bufsize);
	MALI_API_TRACE_PARAM_GLSIZEI_ARRAY(MALI_API_TRACE_OUT, length, 1);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, size, 1);
	MALI_API_TRACE_PARAM_GLENUM_ARRAY(MALI_API_TRACE_OUT, type, 1);
	MALI_API_TRACE_PARAM_CHAR_ARRAY(MALI_API_TRACE_OUT, name, bufsize > 0 ? bufsize : 0, 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetActiveAttrib( ctx->share_lists->program_object_list, program, index, bufsize, length, size, type, name );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLSIZEI_ARRAY(4, (GL_NO_ERROR == err) ? length : NULL, 1);
	MALI_API_TRACE_RETURN_GLINT_ARRAY(5, (GL_NO_ERROR == err) ? size : NULL, 1);
	MALI_API_TRACE_RETURN_GLENUM_ARRAY(6, (GL_NO_ERROR == err) ? type : NULL, 1);
	MALI_API_TRACE_RETURN_CHAR_ARRAY(7, (GL_NO_ERROR == err) ? name : NULL, (GL_NO_ERROR == err && bufsize > 0 && NULL != name) ? _mali_sys_strlen(name) + 1 : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetActiveAttrib);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetActiveUniform )(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, char *name)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetActiveUniform);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLUINT(index);
	MALI_API_TRACE_PARAM_GLSIZEI(bufsize);
	MALI_API_TRACE_PARAM_GLSIZEI_ARRAY(MALI_API_TRACE_OUT, length, 1);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, size, 1);
	MALI_API_TRACE_PARAM_GLENUM_ARRAY(MALI_API_TRACE_OUT, type, 1);
	MALI_API_TRACE_PARAM_CHAR_ARRAY(MALI_API_TRACE_OUT, name, bufsize > 0 ? bufsize : 0, 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetActiveUniform( ctx->share_lists->program_object_list, program, index, bufsize, length, size, type, name );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLSIZEI_ARRAY(4, (GL_NO_ERROR == err) ? length : NULL, 1);
	MALI_API_TRACE_RETURN_GLINT_ARRAY(5, (GL_NO_ERROR == err) ? size : NULL, 1);
	MALI_API_TRACE_RETURN_GLENUM_ARRAY(6, (GL_NO_ERROR == err) ? type : NULL, 1);
	MALI_API_TRACE_RETURN_CHAR_ARRAY(7, (GL_NO_ERROR == err) ? name : NULL, (GL_NO_ERROR == err && bufsize > 0 && NULL != name) ? _mali_sys_strlen(name) + 1 : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetActiveUniform);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetAttachedShaders )(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetAttachedShaders);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLSIZEI(maxcount);
	MALI_API_TRACE_PARAM_GLSIZEI_ARRAY(MALI_API_TRACE_OUT, count, 1);
	MALI_API_TRACE_PARAM_SHADERID_ARRAY(MALI_API_TRACE_OUT, shaders, maxcount > 0 ? maxcount : 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetAttachedShaders( ctx->share_lists->program_object_list, program, maxcount, count, shaders );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLSIZEI_ARRAY(3, (GL_NO_ERROR == err) ? count : NULL, 1);
	MALI_API_TRACE_RETURN_SHADERID_ARRAY(4, (GL_NO_ERROR == err) ? shaders : NULL, ((GL_NO_ERROR == err) && (NULL != count) && ((*count) > 0)) ? (u32)(*count) : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetAttachedShaders);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLint MALI_GL_APIENTRY( glGetAttribLocation )(GLuint program, const char *name)
{
	GLenum err;
	GLint retval = -1;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return 0;

	MALI_PROFILING_ENTER_FUNC(glGetAttribLocation);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_CHAR_ARRAY(MALI_API_TRACE_IN, name, NULL != name ? _mali_sys_strlen(name) + 1 : 0, 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetAttribLocation( ctx->share_lists->program_object_list, program, name, &retval );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetAttribLocation);
	GLES_API_LEAVE(ctx);

	return retval;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetProgramInfoLog )(GLuint program, GLsizei bufsize, GLsizei *length, char *infolog)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetProgramInfoLog);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLSIZEI(bufsize);
	MALI_API_TRACE_PARAM_GLSIZEI_ARRAY(MALI_API_TRACE_OUT, length, 1);
	MALI_API_TRACE_PARAM_CHAR_ARRAY(MALI_API_TRACE_OUT, infolog, bufsize > 0 ? bufsize : 0, 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetProgramInfoLog( ctx->share_lists->program_object_list, program, bufsize, length, infolog );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLSIZEI_ARRAY(3, (GL_NO_ERROR == err) ? length : NULL, 1);
	MALI_API_TRACE_RETURN_CHAR_ARRAY(4, (GL_NO_ERROR == err) ? infolog : NULL, (GL_NO_ERROR == err && bufsize > 0 && NULL != infolog) ? _mali_sys_strlen(infolog) + 1 : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetProgramInfoLog);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetProgramiv )(GLuint program, GLenum pname, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetProgramiv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, 1);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetProgramiv( ctx->share_lists->program_object_list, program, pname, params );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetProgramiv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetShaderInfoLog )(GLuint shader, GLsizei bufsize, GLsizei *length, char *infolog)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetShaderInfoLog);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_SHADERID(shader);
	MALI_API_TRACE_PARAM_GLSIZEI(bufsize);
	MALI_API_TRACE_PARAM_GLSIZEI_ARRAY(MALI_API_TRACE_OUT, length, 1);
	MALI_API_TRACE_PARAM_CHAR_ARRAY(MALI_API_TRACE_OUT, infolog, bufsize > 0 ? bufsize : 0, 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetShaderInfoLog( ctx->share_lists->program_object_list, shader, bufsize, length, infolog);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLSIZEI_ARRAY(3, (GL_NO_ERROR == err) ? length : NULL, 1);
	MALI_API_TRACE_RETURN_CHAR_ARRAY(4, (GL_NO_ERROR == err) ? infolog : NULL, (GL_NO_ERROR == err && bufsize > 0 && NULL != infolog) ? _mali_sys_strlen(infolog) + 1 : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetShaderInfoLog);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetShaderiv )(GLuint shader, GLenum pname, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetShaderiv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_SHADERID(shader);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, 1);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetShaderiv( ctx->share_lists->program_object_list, shader, pname, params);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetShaderiv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetShaderPrecisionFormat )(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetShaderPrecisionFormat);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(shadertype);
	MALI_API_TRACE_PARAM_GLENUM(precisiontype);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, range, 2);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, precision, 1);

	/* No locking needed */
	err = ctx->vtable->fp_glGetShaderPrecisionFormat( shadertype, precisiontype, range, precision);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? range : NULL, 2);
	MALI_API_TRACE_RETURN_GLINT_ARRAY(4, (GL_NO_ERROR == err) ? precision : NULL, 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetShaderPrecisionFormat);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetShaderSource )(GLuint shader, GLsizei bufsize, GLsizei *length, char *source)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetShaderSource);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_SHADERID(shader);
	MALI_API_TRACE_PARAM_GLSIZEI(bufsize);
	MALI_API_TRACE_PARAM_GLSIZEI_ARRAY(MALI_API_TRACE_OUT, length, 1);
	MALI_API_TRACE_PARAM_CHAR_ARRAY(MALI_API_TRACE_OUT, source, bufsize, 0);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetShaderSource( ctx->share_lists->program_object_list, shader, bufsize, length, source);
	_gles_share_lists_unlock( ctx->share_lists );
	
	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLSHADERSOURCE(shader, ctx, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLSIZEI_ARRAY(3, (GL_NO_ERROR == err) ? length : NULL, 1);
	MALI_API_TRACE_RETURN_CHAR_ARRAY(4, (GL_NO_ERROR == err) ? source : NULL, (GL_NO_ERROR == err && bufsize > 0 && NULL != source) ? _mali_sys_strlen(source) + 1 : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetShaderSource);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetUniformfv )(GLuint program, GLint location, GLfloat *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;
	
	MALI_PROFILING_ENTER_FUNC(glGetUniformfv);
	GLES2_API_ENTER(ctx);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );

	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_get_uniform_size(ctx->share_lists->program_object_list, program, location));

	err = ctx->vtable->fp_glGetUniformfv( ctx->share_lists->program_object_list, program, location, params, GLES_FLOAT );

	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_get_uniform_size(ctx->share_lists->program_object_list, program, location));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetUniformfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetUniformiv )(GLuint program, GLint location, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;
	MALI_PROFILING_ENTER_FUNC(glGetUniformiv);
	GLES2_API_ENTER(ctx);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );

	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_get_uniform_size(ctx->share_lists->program_object_list, program, location));

	err = ctx->vtable->fp_glGetUniformiv( ctx->share_lists->program_object_list, program, location, params, GLES_INT );

	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_get_uniform_size(ctx->share_lists->program_object_list, program, location));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetUniformiv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLint MALI_GL_APIENTRY( glGetUniformLocation )(GLuint program, const char *name)
{
	GLenum err;
	GLint retval = -1;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return 0;

	MALI_PROFILING_ENTER_FUNC(glGetUniformLocation);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_CHAR_ARRAY(MALI_API_TRACE_IN, name, NULL != name ? _mali_sys_strlen(name) + 1 : 0, 0);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetUniformLocation( ctx->share_lists->program_object_list, program, name, &retval );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetUniformLocation);
	GLES_API_LEAVE(ctx);

	return retval;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetVertexAttribfv )(GLuint index, GLenum pname, GLfloat *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetVertexAttribfv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(index);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_OUT, params, (pname == GL_CURRENT_VERTEX_ATTRIB) ? 4 : 1);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetVertexAttribfv( &ctx->state.common.vertex_array, index, pname, GLES_FLOAT, params );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, (pname == GL_CURRENT_VERTEX_ATTRIB) ? 4 : 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetVertexAttribfv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetVertexAttribiv )(GLuint index, GLenum pname, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetVertexAttribiv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(index);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, (pname == GL_CURRENT_VERTEX_ATTRIB) ? 4 : 1);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetVertexAttribiv( &ctx->state.common.vertex_array, index, pname, GLES_INT, params );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, (pname == GL_CURRENT_VERTEX_ATTRIB) ? 4 : 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetVertexAttribiv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetVertexAttribPointerv )(GLuint index, GLenum pname, void **pointer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetVertexAttribPointerv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(index);
	MALI_API_TRACE_PARAM_GLENUM(pname);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER_ARRAY(MALI_API_TRACE_OUT, pointer, 1);

	/* No locking needed */
	err = ctx->vtable->fp_glGetVertexAttribPointerv( &ctx->state.common.vertex_array, index, pname, pointer );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_VERTEXATTRIBARRAYPOINTER_ARRAY(3, (GL_NO_ERROR == err) ? pointer : NULL, 1);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetVertexAttribPointerv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLboolean MALI_GL_APIENTRY( glIsProgram )(GLuint program)
{
	GLboolean err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return GL_FALSE;

	MALI_PROFILING_ENTER_FUNC(glIsProgram);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glIsProgram( ctx->share_lists->program_object_list, program );
	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_RETURN_GLBOOLEAN(err);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glIsProgram);
	GLES_API_LEAVE(ctx);

	return err;
}

MALI_GL_APICALL GLboolean MALI_GL_APIENTRY( glIsShader )(GLuint shader)
{
	GLboolean err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return GL_FALSE;

	MALI_PROFILING_ENTER_FUNC(glIsShader);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_SHADERID(shader);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glIsShader( ctx->share_lists->program_object_list, shader );
	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_RETURN_GLBOOLEAN(err);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glIsShader);
	GLES_API_LEAVE(ctx);

	return err;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glLinkProgram )(GLuint program)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glLinkProgram);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glLinkProgram( ctx, ctx->share_lists->program_object_list, program );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLLINKPROGRAM(program, ctx, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glLinkProgram);
	GLES_API_LEAVE(ctx);
}

#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY( glGetProgramBinaryOES )(GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, GLvoid *binary)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetProgramBinaryOES);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLSIZEI(bufSize);
	MALI_API_TRACE_PARAM_GLSIZEI_ARRAY(MALI_API_TRACE_OUT, length, 1);
	MALI_API_TRACE_PARAM_GLENUM_ARRAY(MALI_API_TRACE_OUT, binaryFormat, 1);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_OUT, (u8*)binary, bufSize > 0 ? bufSize : 0, 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGetProgramBinaryOES( ctx->share_lists->program_object_list, program, bufSize, length, binaryFormat, binary );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetProgramBinaryOES);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glProgramBinaryOES )(GLuint program, GLenum binaryFormat, const GLvoid *binary, GLint length)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glProgramBinaryOES);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLENUM(binaryFormat);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_IN, (u8*)binary, length > 0 ? length : 0, 0);
	MALI_API_TRACE_PARAM_GLINT(length);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glProgramBinaryOES( ctx, ctx->share_lists->program_object_list, program, binaryFormat, binary, length );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glProgramBinaryOES);
	GLES_API_LEAVE(ctx);
}
#endif

MALI_GL_APICALL void MALI_GL_APIENTRY( glReleaseShaderCompiler )(void)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glReleaseShaderCompiler);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();

	/* This function has no effect for our driver, so fp_glReleaseShaderCompiler
	   will do nothing (except trigger an instrumented error if called while an
	   OpenGL ES 1.x context is current). */

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glReleaseShaderCompiler();
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glReleaseShaderCompiler);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glShaderBinary )(GLint n, const GLuint *shaders, GLenum binaryformat, const void *binary, GLint length)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glShaderBinary);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(n);
	MALI_API_TRACE_PARAM_SHADERID_ARRAY(MALI_API_TRACE_IN, shaders, n > 0 ? n : 0);
	MALI_API_TRACE_PARAM_GLENUM(binaryformat);
	MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(MALI_API_TRACE_IN, (u8*)binary, length > 0 ? length : 0, 0);
	MALI_API_TRACE_PARAM_GLINT(length);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glShaderBinary(ctx->share_lists->program_object_list, n, shaders, binaryformat, binary, length);
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLSHADERBINARY(n, shaders, ctx);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glShaderBinary);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glShaderSource )(GLuint shader, GLsizei count, const char **string, const GLint *length)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glShaderSource);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_SHADERID(shader);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_CSTR_ARRAY(string, count > 0 ? count : 0);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_IN, length, count > 0 ? count : 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glShaderSource( ctx->share_lists->program_object_list, shader, count, string, length);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glShaderSource);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glStencilFuncSeparate )( GLenum face, GLenum func, GLint ref, GLuint mask )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glStencilFuncSeparate);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(face);
	MALI_API_TRACE_PARAM_GLENUM(func);
	MALI_API_TRACE_PARAM_GLINT(ref);
	MALI_API_TRACE_PARAM_GLUINT(mask);

	/* No locking needed */
	err = ctx->vtable->fp_glStencilFuncSeparate( ctx, face, func, ref, mask );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glStencilFuncSeparate);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glStencilMaskSeparate )(GLenum face, GLuint mask)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;

	MALI_PROFILING_ENTER_FUNC(glStencilMaskSeparate);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(face);
	MALI_API_TRACE_PARAM_GLUINT(mask);

	/* No locking needed */
	err = ctx->vtable->fp_glStencilMaskSeparate( ctx, face, mask );
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glStencilMaskSeparate);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glStencilOpSeparate )( GLenum face, GLenum fail, GLenum zfail, GLenum zpass )
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL ) return;
	
	MALI_PROFILING_ENTER_FUNC(glStencilOpSeparate);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(face);
	MALI_API_TRACE_PARAM_GLENUM(fail);
	MALI_API_TRACE_PARAM_GLENUM(zfail);
	MALI_API_TRACE_PARAM_GLENUM(zpass);

	/* No locking needed */
	err = ctx->vtable->fp_glStencilOpSeparate( ctx, face, fail, zfail, zpass);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glStencilOpSeparate);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glUniform1f )(GLint location, GLfloat x)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform1f);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform1f( ctx, GLES_FLOAT, 1, 1, 1, location, &x);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform1f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform1fv )(GLint location, GLsizei count, const GLfloat *v)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform1fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, v, 1 * (count > 0 ? count : 0));
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform1fv( ctx, GLES_FLOAT, 1, 1, count, location, v );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform1fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform1i )(GLint location, GLint x)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform1i);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLINT(x);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform1i(ctx, location, x);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform1i);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform1iv )(GLint location, GLsizei count, const GLint *v)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform1iv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_IN, v, 1 * (count > 0 ? count : 0));
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform1iv( ctx, GLES_INT, 1, 1, count, location, v );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform1iv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glUniform2f )(GLint location, GLfloat x, GLfloat y)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	GLfloat tab[2]; tab[0] = x; tab[1] = y;
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform2f);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform2f( ctx, GLES_FLOAT, 2, 1, 1, location, tab );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform2f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform2fv )(GLint location, GLsizei count, const GLfloat *v)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform2fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, v, 2 * (count > 0 ? count : 0));

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform2fv( ctx, GLES_FLOAT, 2, 1, count, location, v );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform2fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform2i )(GLint location, GLint x, GLint y)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	GLint tab[2]; tab[0] = x; tab[1] = y;
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform2i);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform2i( ctx, GLES_INT, 2, 1, 1, location, tab );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform2i);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform2iv )(GLint location, GLsizei count, const GLint *v)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform2iv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_IN, v, 2 * (count > 0 ? count : 0));
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform2iv( ctx, GLES_INT, 2, 1, count, location, v );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform2iv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glUniform3f )(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	GLfloat tab[3]; tab[0] = x; tab[1] = y; tab[2] = z;
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform3f);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);
	MALI_API_TRACE_PARAM_GLFLOAT(z);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform3f( ctx, GLES_FLOAT, 3, 1, 1, location, tab );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform3f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform3fv )(GLint location, GLsizei count, const GLfloat *v)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform3fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, v, 3 * (count > 0 ? count : 0));

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform3fv( ctx, GLES_FLOAT, 3, 1, count, location, v );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform3fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform3i )(GLint location, GLint x, GLint y, GLint z)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	GLint tab[3]; tab[0] = x; tab[1] = y; tab[2] = z;
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform3i);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	MALI_API_TRACE_PARAM_GLINT(z);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform3i( ctx, GLES_INT, 3, 1, 1, location, tab);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform3i);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform3iv )(GLint location, GLsizei count, const GLint *v)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform3iv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_IN, v, 3 * (count > 0 ? count : 0));
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform3iv( ctx, GLES_INT, 3, 1, count, location, v );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform3iv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glUniform4f )(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	GLfloat tab[4]; tab[0] = x; tab[1] = y; tab[2] = z; tab[3] = w;
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform4f);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);
	MALI_API_TRACE_PARAM_GLFLOAT(z);
	MALI_API_TRACE_PARAM_GLFLOAT(w);
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform4f( ctx, GLES_FLOAT, 4, 1, 1, location, tab );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform4f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform4fv )(GLint location, GLsizei count, const GLfloat *v)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform4fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, v, 4 * (count > 0 ? count : 0));

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform4fv( ctx, GLES_FLOAT, 4, 1, count, location, v );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform4fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform4i )(GLint location, GLint x, GLint y, GLint z, GLint w)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	GLint tab[4]; tab[0] = x; tab[1] = y; tab[2] = z; tab[3] = w;
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform4i);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLINT(x);
	MALI_API_TRACE_PARAM_GLINT(y);
	MALI_API_TRACE_PARAM_GLINT(z);
	MALI_API_TRACE_PARAM_GLINT(w);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform4i( ctx, GLES_INT, 4, 1, 1, location, tab );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform4i);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniform4iv )(GLint location, GLsizei count, const GLint *v)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniform4iv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_IN, v, 4 * (count > 0 ? count : 0));
	
	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUniform4iv( ctx, GLES_INT, 4, 1, count, location, v );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniform4iv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniformMatrix2fv )(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	GLenum err = GL_INVALID_VALUE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniformMatrix2fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLBOOLEAN(transpose);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, value, 4 * (count > 0 ? count : 0));
	
	if(transpose == GL_FALSE)
	{
		_gles_share_lists_lock( ctx->share_lists );
		err = ctx->vtable->fp_glUniformMatrix2fv( ctx, GLES_FLOAT, 2, 2, count, location, value );
		_gles_share_lists_unlock( ctx->share_lists );
	}
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniformMatrix2fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniformMatrix3fv )(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	GLenum err = GL_INVALID_VALUE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniformMatrix3fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLBOOLEAN(transpose);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, value, 9 * (count > 0 ? count : 0));
	
	if(transpose == GL_FALSE)
	{
		_gles_share_lists_lock( ctx->share_lists );
		err = ctx->vtable->fp_glUniformMatrix3fv( ctx, GLES_FLOAT, 3, 3, count, location, value );
		_gles_share_lists_unlock( ctx->share_lists );
	}
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniformMatrix3fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUniformMatrix4fv )(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	GLenum err = GL_INVALID_VALUE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUniformMatrix4fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(count);
	MALI_API_TRACE_PARAM_GLBOOLEAN(transpose);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, value, 16 * (count > 0 ? count : 0));
	
	if(transpose == GL_FALSE)
	{
		_gles_share_lists_lock( ctx->share_lists );
		err = ctx->vtable->fp_glUniformMatrix4fv( ctx, GLES_FLOAT, 4, 4, count, location, value );
		_gles_share_lists_unlock( ctx->share_lists );
	}
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUniformMatrix4fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glUseProgram )(GLuint program)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glUseProgram);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glUseProgram(&(ctx->state), ctx->share_lists->program_object_list, program);
	_gles_share_lists_unlock( ctx->share_lists );
	
	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLUSEPROGRAM(program, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glUseProgram);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glValidateProgram )(GLuint program)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glValidateProgram);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glValidateProgram( ctx->share_lists->program_object_list, program);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glValidateProgram);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glVertexAttrib1f )(GLuint indx, GLfloat x)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glVertexAttrib1f);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(indx);
	MALI_API_TRACE_PARAM_GLFLOAT(x);

	/* No locking needed */
	err = ctx->vtable->fp_glVertexAttrib1f( &ctx->state.common.vertex_array, indx, 1, &x);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexAttrib1f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glVertexAttrib1fv )(GLuint indx, const GLfloat *values)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glVertexAttrib1fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(indx);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, values, 1);
	
	/* No locking needed */
	err = ctx->vtable->fp_glVertexAttrib1fv( &ctx->state.common.vertex_array, indx, 1, values);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexAttrib1fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glVertexAttrib2f )(GLuint indx, GLfloat x, GLfloat y)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	GLfloat tab[2]; tab[0] = x; tab[1] = y;
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glVertexAttrib2f);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(indx);
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);
	
	/* No locking needed */
	err = ctx->vtable->fp_glVertexAttrib2f( &ctx->state.common.vertex_array, indx, 2, tab);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexAttrib2f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glVertexAttrib2fv )(GLuint indx, const GLfloat *values)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glVertexAttrib2fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(indx);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, values, 2);
	
	/* No locking needed */
	err = ctx->vtable->fp_glVertexAttrib2fv( &ctx->state.common.vertex_array, indx, 2, values);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexAttrib2fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glVertexAttrib3f )(GLuint indx, GLfloat x, GLfloat y, GLfloat z)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	GLfloat tab[3]; tab[0] = x; tab[1] = y; tab[2] = z;
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glVertexAttrib3f);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(indx);
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);
	MALI_API_TRACE_PARAM_GLFLOAT(z);
	
	/* No locking needed */
	err = ctx->vtable->fp_glVertexAttrib3f( &ctx->state.common.vertex_array, indx, 3, tab);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexAttrib3f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glVertexAttrib3fv )(GLuint indx, const GLfloat *values)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glVertexAttrib3fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(indx);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, values, 3);
	
	/* No locking needed */
	err = ctx->vtable->fp_glVertexAttrib3fv( &ctx->state.common.vertex_array, indx, 3, values);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexAttrib3fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY( glVertexAttrib4f )(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	GLfloat tab[4]; tab[0] = x; tab[1] = y; tab[2] = z; tab[3] = w;
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glVertexAttrib4f);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(indx);
	MALI_API_TRACE_PARAM_GLFLOAT(x);
	MALI_API_TRACE_PARAM_GLFLOAT(y);
	MALI_API_TRACE_PARAM_GLFLOAT(z);
	MALI_API_TRACE_PARAM_GLFLOAT(w);
	
	/* No locking needed */
	err = ctx->vtable->fp_glVertexAttrib4f( &ctx->state.common.vertex_array, indx, 4, tab);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexAttrib4f);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glVertexAttrib4fv )(GLuint indx, const GLfloat *values)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glVertexAttrib4fv);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(indx);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_IN, values, 4);
	
	/* No locking needed */
	err = ctx->vtable->fp_glVertexAttrib4fv( &ctx->state.common.vertex_array, indx, 4, values);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexAttrib4fv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glVertexAttribPointer )(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *ptr)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glVertexAttribPointer);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(index);
	MALI_API_TRACE_PARAM_GLINT(size);
	MALI_API_TRACE_PARAM_GLENUM(type);
	MALI_API_TRACE_PARAM_GLBOOLEAN(normalized);
	MALI_API_TRACE_PARAM_GLSIZEI(stride);
	MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(ptr);

	/* No locking needed */
	err = ctx->vtable->fp_glVertexAttribPointer(ctx, index, size, type, normalized == 0 ? MALI_FALSE : MALI_TRUE, stride, ptr);
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glVertexAttribPointer);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLboolean MALI_GL_APIENTRY( glIsRenderbuffer )(GLuint renderbuffer)
{
	GLboolean retval = GL_FALSE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return retval;

	MALI_PROFILING_ENTER_FUNC(glIsRenderbuffer);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(renderbuffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	retval = ctx->vtable->fp_glIsRenderbuffer( ctx->share_lists->renderbuffer_object_list, renderbuffer );
	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_RETURN_GLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glIsRenderbuffer);
	GLES_API_LEAVE(ctx);

	return retval;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBindRenderbuffer )(GLenum target, GLuint renderbuffer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glBindRenderbuffer);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLUINT(renderbuffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glBindRenderbuffer( ctx->share_lists->renderbuffer_object_list, &ctx->state.common.renderbuffer, target, renderbuffer );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBindRenderbuffer);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDeleteRenderbuffers )(GLsizei n, const GLuint *renderbuffers)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDeleteRenderbuffers);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_IN, renderbuffers, n > 0 ? n : 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDeleteRenderbuffers( ctx->share_lists->renderbuffer_object_list,
			&ctx->state.common.renderbuffer, &ctx->state.common.framebuffer, n, renderbuffers );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLDELETERENDERBUFFERS(n, renderbuffers);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDeleteRenderbuffers);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGenRenderbuffers )(GLsizei n, GLuint *renderbuffers)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGenRenderbuffers);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_OUT, renderbuffers, n > 0 ? n : 0);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGenRenderbuffers( ctx->share_lists->renderbuffer_object_list, n, renderbuffers, WRAPPER_RENDERBUFFER);
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLGENRENDERBUFFERS(n, renderbuffers, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLUINT_ARRAY(2, (GL_NO_ERROR == err) ? renderbuffers : NULL, n > 0 ? n : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGenRenderbuffers);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glRenderbufferStorage )(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glRenderbufferStorage);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLENUM(internalformat);
	MALI_API_TRACE_PARAM_GLSIZEI(width);
	MALI_API_TRACE_PARAM_GLSIZEI(height);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glRenderbufferStorage( ctx->base_ctx, ctx->share_lists->renderbuffer_object_list, &ctx->state.common.renderbuffer, target, internalformat, width, height );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLRENDERBUFFERSTORAGE(ctx, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glRenderbufferStorage);
	GLES_API_LEAVE(ctx);
}


MALI_GL_APICALL void MALI_GL_APIENTRY( glGetRenderbufferParameteriv )(GLenum target, GLenum pname, GLint* params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetRenderbufferParameteriv);
	GLES2_API_ENTER(ctx);
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
	MALI_PROFILING_LEAVE_FUNC(glGetRenderbufferParameteriv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLboolean MALI_GL_APIENTRY( glIsFramebuffer )(GLuint framebuffer)
{
	GLboolean retval = GL_FALSE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return retval;

	MALI_PROFILING_ENTER_FUNC(glIsFramebuffer);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLUINT(framebuffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	retval = ctx->vtable->fp_glIsFramebuffer( ctx->share_lists->framebuffer_object_list, framebuffer );
	_gles_share_lists_unlock( ctx->share_lists );

	MALI_API_TRACE_RETURN_GLBOOLEAN(retval);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glIsFramebuffer);
	GLES_API_LEAVE(ctx);

	return retval;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glBindFramebuffer )(GLenum target, GLuint framebuffer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glBindFramebuffer);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);
	MALI_API_TRACE_PARAM_GLUINT(framebuffer);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glBindFramebuffer( ctx, ctx->share_lists->framebuffer_object_list, &ctx->state.common.framebuffer, target, framebuffer );
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glBindFramebuffer);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glDeleteFramebuffers )(GLsizei n, const GLuint *framebuffers)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glDeleteFramebuffers);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_IN, framebuffers, n > 0 ? n : 0);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glDeleteFramebuffers( ctx, n, framebuffers );
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLDELETEFRAMEBUFFERS(n, framebuffers);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glDeleteFramebuffers);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGenFramebuffers )(GLsizei n, GLuint *framebuffers)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;
	
	MALI_PROFILING_ENTER_FUNC(glGenFramebuffers);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLSIZEI(n);
	MALI_API_TRACE_PARAM_GLUINT_ARRAY(MALI_API_TRACE_OUT, framebuffers, n > 0 ? n : 0);
	
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGenFramebuffers( ctx->share_lists->framebuffer_object_list, n, framebuffers, WRAPPER_FRAMEBUFFER);
	_gles_share_lists_unlock( ctx->share_lists );

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLGENFRAMEBUFFERS(n, framebuffers, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLUINT_ARRAY(2, (GL_NO_ERROR == err) ? framebuffers: NULL, n > 0 ? n : 0);
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGenFramebuffers);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL GLenum MALI_GL_APIENTRY( glCheckFramebufferStatus )(GLenum target)
{
	GLenum err;
	GLenum retval = GL_INVALID_VALUE;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return GL_INVALID_OPERATION;

	MALI_PROFILING_ENTER_FUNC(glCheckFramebufferStatus);
	GLES2_API_ENTER(ctx);
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
	MALI_PROFILING_LEAVE_FUNC(glCheckFramebufferStatus);
	GLES_API_LEAVE(ctx);

	return retval;
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFramebufferTexture2D )(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glFramebufferTexture2D);
	GLES2_API_ENTER(ctx);
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

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLFRAMEBUFFERTEXTURE2D(texture, ctx, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFramebufferTexture2D);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glFramebufferRenderbuffer )(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glFramebufferRenderbuffer);
	GLES2_API_ENTER(ctx);
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

	#if defined(__SYMBIAN32__) 
	SYMBIAN_GPU_TRACE_GLFRAMEBUFFERRENDERBUFFER(renderbuffer, ctx, err);
	#endif
	
	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glFramebufferRenderbuffer);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetFramebufferAttachmentParameteriv )(GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGetFramebufferAttachmentParameteriv);
	GLES2_API_ENTER(ctx);
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
	MALI_PROFILING_LEAVE_FUNC(glGetFramebufferAttachmentParameteriv);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGenerateMipmap )(GLenum target)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;

	MALI_PROFILING_ENTER_FUNC(glGenerateMipmap);
	GLES2_API_ENTER(ctx);
	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_GLENUM(target);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );
	err = ctx->vtable->fp_glGenerateMipmap( ctx, &ctx->state.common.texture_env, ctx->share_lists->texture_object_list, target);
	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGenerateMipmap);
	GLES_API_LEAVE(ctx);
}

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY( glGetnUniformfvEXT )(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;
	
	MALI_PROFILING_ENTER_FUNC(glGetnUniformfvEXT);
	GLES2_API_ENTER(ctx);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );

	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(bufSize);
	MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_get_uniform_size(ctx->share_lists->program_object_list, program, location));

	err = ctx->vtable->fp_glGetnUniformfvEXT( ctx, ctx->share_lists->program_object_list, program, location, bufSize, params, GLES_FLOAT );

	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_get_uniform_size(ctx->share_lists->program_object_list, program, location));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetnUniformfvEXT);
	GLES_API_LEAVE(ctx);
}

MALI_GL_APICALL void MALI_GL_APIENTRY( glGetnUniformivEXT )(GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
	GLenum err;
	gles_context *ctx = GLES_GET_CONTEXT();
	if (ctx == NULL) return;
	MALI_PROFILING_ENTER_FUNC(glGetnUniformivEXT);
	GLES2_API_ENTER(ctx);

	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	_gles_share_lists_lock( ctx->share_lists );

	MALI_API_TRACE_FUNC_BEGIN();
	MALI_API_TRACE_PARAM_PROGRAMID(program);
	MALI_API_TRACE_PARAM_GLINT(location);
	MALI_API_TRACE_PARAM_GLSIZEI(bufSize);
	MALI_API_TRACE_PARAM_GLINT_ARRAY(MALI_API_TRACE_OUT, params, mali_api_trace_get_uniform_size(ctx->share_lists->program_object_list, program, location));

	err = ctx->vtable->fp_glGetnUniformfvEXT( ctx, ctx->share_lists->program_object_list, program, location, bufSize, params, GLES_INT );

	_gles_share_lists_unlock( ctx->share_lists );

	if (err != GL_NO_ERROR) ctx->vtable->fp_set_error( ctx, err );

	MALI_API_TRACE_RETURN_GLINT_ARRAY(3, (GL_NO_ERROR == err) ? params : NULL, mali_api_trace_get_uniform_size(ctx->share_lists->program_object_list, program, location));
	MALI_API_TRACE_FUNC_END();
	MALI_PROFILING_LEAVE_FUNC(glGetnUniformivEXT);
	GLES_API_LEAVE(ctx);
}
#endif
