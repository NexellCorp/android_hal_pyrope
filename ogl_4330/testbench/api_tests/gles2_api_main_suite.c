/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * Public definition of test harness executable.
 */

#include "main_suite.h"
#include "mem.h"
#include <stdio.h>
#include "opengles2/gl2_framework.h"
#include "opengles2/gl2_framework_parameters.h"
#include "../unit_framework/suite.h"

/*
 *	External declarations of suite factory functions. This is more compact than
 *	a separate .h file for each suite.
 */
extern suite* create_suite_glViewPort(mempool* pool, result_set* results);
extern suite* create_suite_glDepthRange(mempool* pool, result_set* results);
extern suite* create_suite_glScissor(mempool* pool, result_set* results);
extern suite* create_suite_glHint(mempool* pool, result_set* results);
extern suite* create_suite_glDepthFunc(mempool* pool, result_set* results);
extern suite* create_suite_glClearColor(mempool* pool, result_set* results);
extern suite* create_suite_glClearDepthf(mempool* pool, result_set* results);
extern suite* create_suite_glColorMask(mempool* pool, result_set* results);
extern suite* create_suite_glDepthMask(mempool* pool, result_set* results);
extern suite* create_suite_glEnable_glDisable_glIsEnabled(mempool* pool, result_set* results);
extern suite* create_suite_glFlush_glFinish(mempool* pool, result_set* results);
extern suite* create_suite_glCullFace(mempool* pool, result_set* results);
extern suite* create_suite_glFrontFace(mempool* pool, result_set* results);
extern suite* create_suite_glDrawElements(mempool* pool, result_set* results);
extern suite* create_suite_glDrawArrays(mempool* pool, result_set* results);
extern suite* create_suite_glReadPixels(mempool* pool, result_set* results);
extern suite* create_suite_glPolygonOffset(mempool* pool, result_set* results);
extern suite* create_suite_glClear(mempool* pool, result_set* results);
extern suite* create_suite_glLineWidth(mempool* pool, result_set* results);
extern suite* create_suite_glPixelStorei(mempool* pool, result_set* results);
extern suite* create_suite_glSampleCoverage(mempool* pool, result_set* results);

extern suite* create_suite_glIsBuffer(mempool* pool, result_set* results);
extern suite* create_suite_glGenBuffers(mempool* pool, result_set* results);
extern suite* create_suite_glDeleteBuffers(mempool* pool, result_set* results);
extern suite* create_suite_glBindBuffer(mempool* pool, result_set* results);
extern suite* create_suite_glBufferData(mempool* pool, result_set* results);
extern suite* create_suite_glBufferSubData(mempool* pool, result_set* results);
extern suite* create_suite_glGetBufferParameteriv(mempool* pool, result_set* results);

extern suite* create_suite_glBlendFunc(mempool* pool, result_set* results);
extern suite* create_suite_glBlendColor(mempool* pool, result_set* results);
extern suite* create_suite_glBlendEquation(mempool* pool, result_set* results);
extern suite* create_suite_glBlendEquation_minmax(mempool* pool, result_set* results);

extern suite* create_suite_glError(mempool* pool, result_set* results);
extern suite* create_suite_glGet(mempool* pool, result_set* results);
extern suite* create_suite_glGetString(mempool* pool, result_set* results);

extern suite* create_suite_PointCullClip(mempool* pool, result_set* results);
extern suite* create_suite_LineCullClip(mempool* pool, result_set* results);
extern suite* create_suite_PointSprites(mempool* pool, result_set* results);

extern suite* create_suite_glCreateProgram_glDeleteProgram_glUseProgram(mempool* pool, result_set* results);
extern suite* create_suite_glCreateShader_glDeleteShader(mempool* pool, result_set* results);
extern suite* create_suite_glIsProgram(mempool* pool, result_set* results);
extern suite* create_suite_glIsShader(mempool* pool, result_set* results);
extern suite* create_suite_glShaderSource_glGetShaderSource(mempool* pool, result_set* results);
extern suite* create_suite_glCompileShader(mempool* pool, result_set* results);
extern suite* create_suite_glGetShaderiv(mempool* pool, result_set* results);
extern suite* create_suite_glGetProgramiv(mempool* pool, result_set* results);
extern suite* create_suite_glAttach_DetachShader(mempool* pool, result_set* results);
extern suite* create_suite_glGetAttachedShaders(mempool* pool, result_set* results);
extern suite* create_suite_glValidateProgram(mempool* pool, result_set* results);
extern suite* create_suite_glLinkProgram(mempool* pool, result_set* results);
extern suite* create_suite_glGetProgramInfoLog(mempool* pool, result_set* results);
extern suite* create_suite_glGetShaderInfoLog(mempool* pool, result_set* results);
extern suite* create_suite_glUseProgram_relink(mempool* pool, result_set* results);
extern suite* create_suite_glCompileShader_recompile(mempool* pool, result_set* results);
extern suite* create_suite_glReleaseShaderCompiler(mempool* pool, result_set* results);
extern suite* create_suite_glGetShaderPrecisionFormat(mempool* pool, result_set* results);
#if !defined(__SYMBIAN32__)
extern suite* create_suite_glProgramBinaryOES_glGetProgramBinaryOES(mempool* pool, result_set* results);
#endif
extern suite* create_suite_glClearStencil(mempool* pool, result_set* results);
extern suite* create_suite_glStencilMask(mempool* pool, result_set* results);
extern suite* create_suite_glStencilFunc(mempool* pool, result_set* results);
extern suite* create_suite_glStencilOp(mempool* pool, result_set* results);

extern suite* create_suite_glGetVertexAttrib(mempool* pool, result_set* results);
extern suite* create_suite_glGetActiveAttrib(mempool* pool, result_set* results);
extern suite* create_suite_glUniform(mempool* pool, result_set* results);

extern suite* create_suite_glEnableVertexAttribArray_glDisableVertexAttribArray(mempool* pool, result_set* results);
extern suite* create_suite_glGetAttribLocation_glBindAttribLocation(mempool* pool, result_set* results);
extern suite* create_suite_glVertexAttrib(mempool* pool, result_set* results);
extern suite* create_suite_glGetVertexAttrib(mempool* pool, result_set* results);
extern suite* create_suite_glVertexAttribPointer(mempool* pool, result_set* results);
extern suite* create_suite_glGetVertexAttribPointerv(mempool* pool, result_set* results);
extern suite* create_suite_glGetActiveAttrib(mempool* pool, result_set* results);
extern suite* create_suite_glGetActiveUniform(mempool* pool, result_set* results);
extern suite* create_suite_glGetUniformLocation(mempool* pool, result_set* results);
extern suite* create_suite_glGetUniformfiv(mempool* pool, result_set* results);
extern suite* create_suite_glUniform(mempool* pool, result_set* results);

extern suite* create_suite_glIsRenderbuffer(mempool* pool, result_set* results);
extern suite* create_suite_glBindRenderbuffer(mempool* pool, result_set* results);
extern suite* create_suite_glDeleteRenderbuffers(mempool* pool, result_set* results);
extern suite* create_suite_glGenRenderbuffers(mempool* pool, result_set* results);
extern suite* create_suite_glGetRenderbufferParameteriv(mempool* pool, result_set* results);
extern suite* create_suite_glRenderbufferStorage(mempool* pool, result_set* results);
extern suite* create_suite_glIsFramebuffer(mempool* pool, result_set* results);
extern suite* create_suite_glBindFramebuffer(mempool* pool, result_set* results);
extern suite* create_suite_glDeleteFramebuffers(mempool* pool, result_set* results);
extern suite* create_suite_glGenFramebuffers(mempool* pool, result_set* results);
extern suite* create_suite_glCheckFramebufferStatus(mempool* pool, result_set* results);
extern suite* create_suite_glFramebufferRenderbuffer(mempool* pool, result_set* results);
extern suite* create_suite_gl_drawing_with_renderbuffers(mempool* pool, result_set* results);
extern suite* create_suite_glGetFramebufferAttachmentParameteriv(mempool* pool, result_set* results);
extern suite* create_suite_glFramebufferTexture2D(mempool* pool, result_set* results);
extern suite* create_suite_gl_render_to_texture_stability(mempool* pool, result_set* results);
extern suite* create_suite_gl_reads_from_incomplete_rendertarget(mempool* pool, result_set* results);
extern suite* create_suite_check_framebuffer_validity(mempool* pool, result_set* results);
extern suite* create_suite_gl_multicontext_stress_tests(mempool* pool, result_set* results);
extern suite* create_suite_renderable_formats(mempool* pool, result_set* results);
extern suite* create_suite_OES_depth_texture(mempool* pool, result_set* results);
extern suite* create_suite_OES_depth_texture_test_as_FBO_attachment_input(mempool* pool, result_set* results);
extern suite* create_suite_OES_packed_depth_stencil(mempool* pool, result_set* results);
extern suite* create_suite_EXT_multisample_FBO(mempool* pool, result_set* results);

extern suite* create_suite_glIsTexture(mempool* pool, result_set* results);
extern suite* create_suite_glGetTexParameteriv(mempool* pool, result_set* results);
extern suite* create_suite_glTexParameter(mempool* pool, result_set* results);
extern suite* create_suite_glGetTexParameter(mempool* pool, result_set* results);
extern suite* create_suite_glTexParameteri(mempool* pool, result_set* results);
extern suite* create_suite_glTexParameteri2(mempool* pool, result_set* results);
extern suite* create_suite_glBindTexture(mempool* pool, result_set* results);
extern suite* create_suite_glGenTextures(mempool* pool, result_set* results);
extern suite* create_suite_glDeleteTextures(mempool* pool, result_set* results);
extern suite* create_suite_glTexImage2D(mempool* pool, result_set* results);
extern suite* create_suite_glTexImage2D2(mempool* pool, result_set* results);
extern suite* create_suite_glTexImage2D3(mempool* pool, result_set* results);
extern suite* create_suite_glTexSubImage2D(mempool* pool, result_set* results);
extern suite* create_suite_glTexImage2D4(mempool* pool, result_set* results);
extern suite* create_suite_glGenerateMipmaps(mempool* pool, result_set* results);
extern suite* create_suite_glCopyTexImage2D(mempool* pool, result_set* results);
extern suite* create_suite_glCopyTexSubImage2D(mempool* pool, result_set* results);
extern suite* create_suite_glCompressedTexImage2D(mempool* pool, result_set* results);
extern suite* create_suite_glCompressedTexSubImage2D(mempool* pool, result_set* results);
extern suite* create_suite_ETC(mempool* pool, result_set* results);
extern suite* create_suite_NPOT(mempool* pool, result_set* results);
extern suite* create_suite_EXT_shader_texture_LOD(mempool* pool, result_set* results);
extern suite* create_suite_EXT_shader_texture_LOD_border_case(mempool* pool, result_set* results);
extern suite* create_suite_glTexImage_formats(mempool* pool, result_set* results);
extern suite* create_suite_bgra(mempool* pool, result_set* results);
extern suite* create_suite_int16(mempool* pool, result_set* results);
extern suite* create_suite_int16_fullsize(mempool* pool, result_set* results);
extern suite* create_suite_fp16(mempool* pool, result_set* results);
extern suite* create_suite_cubemap_border_cases(mempool* pool, result_set* results);

#ifndef TESTBENCH_DESKTOP_GL
extern suite* create_suite_EGLimage_glGetString(mempool* pool, result_set* results);
extern suite* create_suite_EGLimage_eglGetProcAddress(mempool* pool, result_set* results);
extern suite* create_suite_EGLimage_share_textures(mempool* pool, result_set* results);
extern suite* create_suite_EGLimage_native_pixmap_type(mempool* pool, result_set* results);
extern suite* create_suite_EGLimage_renderbuffer_functionality(mempool* pool, result_set* results);
extern suite* create_suite_EGLimage_mipmap_chain(mempool* pool, result_set* results);
extern suite* create_suite_EGLimage_cubemaps(mempool* pool, result_set* results);
extern suite* create_suite_EGLimage_additional_pixmap_formats(mempool* pool, result_set* results);

extern suite* create_suite_YUV01_image_external(mempool* pool, result_set* results);
extern suite* create_suite_YUV02_basic_yuv_texturing(mempool* pool, result_set* results);
extern suite* create_suite_YUV03_multiple_yuv_textures(mempool* pool, result_set* results);
extern suite* create_suite_YUV04_multiple_yuv_formats(mempool* pool, result_set* results);
#endif

/**
 *	Function which will create, run, and remove a single test suite.
 *
 *	\param results	A result set in which to store the results of the test.
 *	\param factory	A pointer to a factory function that creates the suite to run.
 *
 *	\return			Zero if the suite executed successfully (even if it had test failures),
 *					or non-zero if the suite terminated doe to a fatal error.
 */
static result_status run_suite(result_set* results,
                               test_specifier* test_spec,
                               suite* (*factory)(mempool* pool, result_set* results) )
{
	memerr err;
	mempool pool;
	suite* new_suite;
	result_status status = result_status_pass;

	err = _test_mempool_init(&pool, 1024);
	results->suites_considered++;
	if(err != MEM_OK) return result_status_fatal; 
	

	new_suite = factory(&pool,results);
	if(new_suite == NULL) return result_status_fatal; 
	switch(test_spec->mode)
	{
	case MODE_EXECUTE:
		status = execute(new_suite, test_spec);
		break;

	case MODE_LIST_SUITES:
	case MODE_LIST_TESTS:
	case MODE_LIST_FIXTURES:
		list(new_suite,test_spec);
		break;
	}

	if(status == result_status_pass) results->suites_passed++;

	_test_mempool_destroy(&pool);

	return status;
}

/*
 *	Main test runner function.
 *	Full documentation in header.
 */

int run_main_suite(int mask, test_specifier* test_spec)
{
	int worst_error = 0;
	mempool pool;
	result_set* results = NULL;

	memerr err;
	err = _test_mempool_init(&pool, 1024);
	if(err != MEM_OK) return 1; /* critical error */

	results = create_result_set(&pool);
	if(results == NULL)
	{
		_test_mempool_destroy(&pool);
		return 1; /* critical error */
	}

	/* Instead of adding another #if 0 here... I know you're just about to do that... */
	/* Use the filter parameter instead: ./main_suite --suite=suitename  */

#ifndef ARM_INTERNAL_BUILD /* subset of API tests for customer release */

	run_suite(results, test_spec, create_suite_glDepthFunc);
	run_suite(results, test_spec, create_suite_glDrawElements);
	run_suite(results, test_spec, create_suite_glLineWidth);
	run_suite(results, test_spec, create_suite_glClear);
	run_suite(results, test_spec, create_suite_glBlendFunc);
	run_suite(results, test_spec, create_suite_glBufferData);
	run_suite(results, test_spec, create_suite_glLinkProgram);
	run_suite(results, test_spec, create_suite_glStencilOp);
	run_suite(results, test_spec, create_suite_glEnableVertexAttribArray_glDisableVertexAttribArray);
	run_suite(results, test_spec, create_suite_glTexImage2D3);
	run_suite(results, test_spec, create_suite_glTexImage2D4);
	run_suite(results, test_spec, create_suite_EGLimage_share_textures);

#else /* ARM_INTERNAL_BUILD is defined in arm_internal.mak */

	/* AGE_-_API_and_General_Entrypoints */
	run_suite(results, test_spec, create_suite_glHint);
	run_suite(results, test_spec, create_suite_glViewPort);
	run_suite(results, test_spec, create_suite_glDepthRange);
	run_suite(results, test_spec, create_suite_glScissor);
	run_suite(results, test_spec, create_suite_glDepthFunc);
	run_suite(results, test_spec, create_suite_glClearColor);
	run_suite(results, test_spec, create_suite_glClearDepthf);
	run_suite(results, test_spec, create_suite_glColorMask);
	run_suite(results, test_spec, create_suite_glDepthMask);
	run_suite(results, test_spec, create_suite_glDrawElements);
	run_suite(results, test_spec, create_suite_glDrawArrays);
	run_suite(results, test_spec, create_suite_glEnable_glDisable_glIsEnabled);
	run_suite(results, test_spec, create_suite_glFlush_glFinish);
	run_suite(results, test_spec, create_suite_glReadPixels);
	run_suite(results, test_spec, create_suite_glFrontFace);
	run_suite(results, test_spec, create_suite_glCullFace);
	run_suite(results, test_spec, create_suite_glPolygonOffset);
	run_suite(results, test_spec, create_suite_glLineWidth);
	run_suite(results, test_spec, create_suite_glClear);
	run_suite(results, test_spec, create_suite_glPixelStorei);
	run_suite(results, test_spec, create_suite_glSampleCoverage);
	run_suite(results, test_spec, create_suite_PointCullClip);
	run_suite(results, test_spec, create_suite_LineCullClip);
	run_suite(results, test_spec, create_suite_PointSprites);

	/* AGG_-_API_General_Getters */
	run_suite(results, test_spec, create_suite_glError);
	run_suite(results, test_spec, create_suite_glGet);
	run_suite(results, test_spec, create_suite_glGetString);

	/* ABO_-_API_Buffer_Objects */
	run_suite(results, test_spec, create_suite_glIsBuffer);
	run_suite(results, test_spec, create_suite_glGenBuffers);
	run_suite(results, test_spec, create_suite_glDeleteBuffers);
	run_suite(results, test_spec, create_suite_glBindBuffer);
	run_suite(results, test_spec, create_suite_glBufferData);
	run_suite(results, test_spec, create_suite_glBufferSubData);
	run_suite(results, test_spec, create_suite_glGetBufferParameteriv);


	/* ABL_-_API_Blending */
	run_suite(results, test_spec, create_suite_glBlendFunc);
	run_suite(results, test_spec, create_suite_glBlendColor);
	run_suite(results, test_spec, create_suite_glBlendEquation);
	run_suite(results, test_spec, create_suite_glBlendEquation_minmax);

	/* ASP_-_API_Shaders_and_Programs */
	run_suite(results, test_spec, create_suite_glCreateProgram_glDeleteProgram_glUseProgram);
	run_suite(results, test_spec, create_suite_glCreateShader_glDeleteShader);
	run_suite(results, test_spec, create_suite_glIsProgram);
	run_suite(results, test_spec, create_suite_glIsShader);
	run_suite(results, test_spec, create_suite_glShaderSource_glGetShaderSource);
	run_suite(results, test_spec, create_suite_glCompileShader);
	run_suite(results, test_spec, create_suite_glGetShaderiv);
	run_suite(results, test_spec, create_suite_glGetProgramiv);
	run_suite(results, test_spec, create_suite_glAttach_DetachShader);
	run_suite(results, test_spec, create_suite_glGetAttachedShaders);
	run_suite(results, test_spec, create_suite_glValidateProgram);
	run_suite(results, test_spec, create_suite_glLinkProgram);
	run_suite(results, test_spec, create_suite_glGetProgramInfoLog);
	run_suite(results, test_spec, create_suite_glGetShaderInfoLog);
	run_suite(results, test_spec, create_suite_glUseProgram_relink);
	run_suite(results, test_spec, create_suite_glCompileShader_recompile);
	run_suite(results, test_spec, create_suite_glReleaseShaderCompiler);
	run_suite(results, test_spec, create_suite_glGetShaderPrecisionFormat);
#if !defined(__SYMBIAN32__)
	run_suite(results, test_spec, create_suite_glProgramBinaryOES_glGetProgramBinaryOES);
#endif

	/* AST_-_API_Stencil */
	run_suite(results, test_spec, create_suite_glClearStencil);
	run_suite(results, test_spec, create_suite_glStencilMask);
	run_suite(results, test_spec, create_suite_glStencilOp);
	run_suite(results, test_spec, create_suite_glStencilFunc);

	/* AUA_-_API_Uniforms_and_Attributes */
	run_suite(results, test_spec, create_suite_glEnableVertexAttribArray_glDisableVertexAttribArray);
	run_suite(results, test_spec, create_suite_glGetAttribLocation_glBindAttribLocation);
	run_suite(results, test_spec, create_suite_glVertexAttrib);
	run_suite(results, test_spec, create_suite_glGetVertexAttrib);
	run_suite(results, test_spec, create_suite_glVertexAttribPointer);
	run_suite(results, test_spec, create_suite_glGetVertexAttribPointerv);
	run_suite(results, test_spec, create_suite_glGetActiveAttrib);
	run_suite(results, test_spec, create_suite_glGetActiveUniform);
	run_suite(results, test_spec, create_suite_glGetUniformLocation);
	run_suite(results, test_spec, create_suite_glGetUniformfiv);
	run_suite(results, test_spec, create_suite_glUniform);

	/* AFB_-_API_Framebuffer_Objects */
	run_suite(results, test_spec, create_suite_glIsRenderbuffer);
	run_suite(results, test_spec, create_suite_glBindRenderbuffer);
	run_suite(results, test_spec, create_suite_glDeleteRenderbuffers);
	run_suite(results, test_spec, create_suite_glGenRenderbuffers);
	run_suite(results, test_spec, create_suite_glRenderbufferStorage);
	run_suite(results, test_spec, create_suite_glGetRenderbufferParameteriv);
	run_suite(results, test_spec, create_suite_glIsFramebuffer);
	run_suite(results, test_spec, create_suite_glBindFramebuffer);
	run_suite(results, test_spec, create_suite_glDeleteFramebuffers);
	run_suite(results, test_spec, create_suite_glGenFramebuffers);
	run_suite(results, test_spec, create_suite_glCheckFramebufferStatus);
	run_suite(results, test_spec, create_suite_glFramebufferTexture2D);
	run_suite(results, test_spec, create_suite_glFramebufferRenderbuffer);
	run_suite(results, test_spec, create_suite_gl_drawing_with_renderbuffers);
	run_suite(results, test_spec, create_suite_glGetFramebufferAttachmentParameteriv);
	run_suite(results, test_spec, create_suite_gl_render_to_texture_stability);
	run_suite(results, test_spec, create_suite_gl_reads_from_incomplete_rendertarget);
	run_suite(results, test_spec, create_suite_check_framebuffer_validity);
	run_suite(results, test_spec, create_suite_gl_multicontext_stress_tests); 
	run_suite(results, test_spec, create_suite_renderable_formats);
	run_suite(results, test_spec, create_suite_OES_depth_texture);
	run_suite(results, test_spec, create_suite_OES_depth_texture_test_as_FBO_attachment_input);
	run_suite(results, test_spec, create_suite_OES_packed_depth_stencil);
	run_suite(results, test_spec, create_suite_EXT_multisample_FBO);

	/* ATX_-_API_Texture */
	run_suite(results, test_spec, create_suite_glIsTexture);
	run_suite(results, test_spec, create_suite_glGetTexParameteriv);
	run_suite(results, test_spec, create_suite_glTexParameter);
	run_suite(results, test_spec, create_suite_glGetTexParameter);
	run_suite(results, test_spec, create_suite_glTexParameteri);
	run_suite(results, test_spec, create_suite_glTexParameteri2);
	run_suite(results, test_spec, create_suite_glBindTexture);
	run_suite(results, test_spec, create_suite_glGenTextures);
	run_suite(results, test_spec, create_suite_glDeleteTextures);
	run_suite(results, test_spec, create_suite_glTexImage2D);
	run_suite(results, test_spec, create_suite_glTexImage2D2);
	run_suite(results, test_spec, create_suite_glTexImage2D3);
	run_suite(results, test_spec, create_suite_glTexSubImage2D);
	run_suite(results, test_spec, create_suite_glCopyTexImage2D);
	run_suite(results, test_spec, create_suite_glCopyTexSubImage2D);
	run_suite(results, test_spec, create_suite_glCompressedTexImage2D);
	run_suite(results, test_spec, create_suite_glTexImage2D4);
	run_suite(results, test_spec, create_suite_glGenerateMipmaps);
	run_suite(results, test_spec, create_suite_glCompressedTexSubImage2D);
	run_suite(results, test_spec, create_suite_ETC);
	run_suite(results, test_spec, create_suite_NPOT);
	run_suite(results, test_spec, create_suite_EXT_shader_texture_LOD);
	run_suite(results, test_spec, create_suite_EXT_shader_texture_LOD_border_case);
	run_suite(results, test_spec, create_suite_glTexImage_formats);
	run_suite(results, test_spec, create_suite_bgra);
	run_suite(results, test_spec, create_suite_int16);
	run_suite(results, test_spec, create_suite_int16_fullsize);
	run_suite(results, test_spec, create_suite_fp16);
	run_suite(results, test_spec, create_suite_cubemap_border_cases);

#ifndef TESTBENCH_DESKTOP_GL
	/* AEI_-_API_EGLImage */
	run_suite(results, test_spec, create_suite_EGLimage_glGetString);

#ifndef ANDROID
	run_suite(results, test_spec, create_suite_EGLimage_eglGetProcAddress); 
#endif

	run_suite(results, test_spec, create_suite_EGLimage_share_textures); 

#ifndef ANDROID
	run_suite(results, test_spec, create_suite_EGLimage_native_pixmap_type);
#endif	
	
	run_suite(results, test_spec, create_suite_EGLimage_renderbuffer_functionality);

#ifndef ANDROID
	run_suite(results, test_spec, create_suite_EGLimage_mipmap_chain);
#endif	
	run_suite(results, test_spec, create_suite_EGLimage_cubemaps);
    if ((egl_helper_nativepixmap_supported( DISPLAY_FORMAT_L8   ) == EGL_TRUE) ||
        (egl_helper_nativepixmap_supported( DISPLAY_FORMAT_AL88 ) == EGL_TRUE))
    {
	    run_suite(results, test_spec, create_suite_EGLimage_additional_pixmap_formats);
    }
#endif

#if !(defined __SYMBIAN32__) 
/* Symbian doesn't need this */
#ifndef TESTBENCH_DESKTOP_GL
    /* YUV_-_API_YUV_support */
    run_suite(results, test_spec, create_suite_YUV01_image_external);
    run_suite(results, test_spec, create_suite_YUV02_basic_yuv_texturing);
    run_suite(results, test_spec, create_suite_YUV03_multiple_yuv_textures);
    run_suite(results, test_spec, create_suite_YUV04_multiple_yuv_formats);
#endif
#endif

#endif /* ARM_INTERNAL_BUILD */

	worst_error = 0;
	if (test_spec->mode == MODE_EXECUTE)
	{
		/* Print results, clean up, and exit */
		worst_error = print_results(results, mask);
	}

	remove_result_set( results );
	_test_mempool_destroy(&pool);

	return worst_error;
}

#include "../unit_framework/framework_main.h"
int main(int argc, char **argv)
{
	return test_main(run_main_suite, gles2_parse_args, argc, argv);
}
