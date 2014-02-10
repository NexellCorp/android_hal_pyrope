/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifdef MALI_BUILD_ANDROID_MONOLITHIC

#include "gles_entrypoints.h"

MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glActiveTexture , GLenum, texture )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glBindBuffer , GLenum, target, GLuint, buffer )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glBindTexture ,GLenum, target, GLuint, texture)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glBlendFunc , GLenum, src, GLenum, dst )
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glBufferData , GLenum, target, GLsizeiptr, size, const GLvoid*, data, GLenum, usage )
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glBufferSubData, GLenum, target, GLintptr, offset, GLsizeiptr, size, const GLvoid *, data)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glClear , GLbitfield, mask)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v4( glClearColor , GLclampf, red, GLclampf, green, GLclampf, blue, GLclampf, alpha )
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v1( glClearDepthf , GLclampf, depth )
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glClearStencil , GLint, s )
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glColorMask , GLboolean, red, GLboolean, green, GLboolean, blue, GLboolean, alpha )
MALI_GL_APICALL void MALI_GL_APIENTRY_v8( glCompressedTexImage2D , GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLsizei, imageSize, maliGLConstVoidPtr, data)
MALI_GL_APICALL void MALI_GL_APIENTRY_v9( glCompressedTexSubImage2D , GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLsizei, imageSize, maliGLConstVoidPtr, data)
MALI_GL_APICALL void MALI_GL_APIENTRY_v8( glCopyTexImage2D , GLenum, target, GLint, level, GLenum, internalformat, GLint, x, GLint, y, GLsizei, width, GLsizei, height, GLint, border)
MALI_GL_APICALL void MALI_GL_APIENTRY_v8( glCopyTexSubImage2D , GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLint, x, GLint, y, GLsizei, width, GLsizei, height)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glCullFace , GLenum, mode )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glDeleteBuffers ,GLsizei, n, maliGLConstUintPtr, buffers)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glDeleteTextures ,GLsizei, n, maliGLConstUintPtr, textures )
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glDepthFunc , GLenum, func )
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glDepthMask , GLboolean, flag )
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v2( glDepthRangef ,GLclampf, n, GLclampf, f)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glDisable , GLenum, cap )
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glDrawArrays , GLenum, mode, GLint, first, GLsizei, count )
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glDrawElements , GLenum, mode, GLsizei, count, GLenum, type, maliGLConstVoidPtr, indices)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glEnable , GLenum, cap )
MALI_GL_APICALL void MALI_GL_APIENTRY_v0( glFinish , void )
MALI_GL_APICALL void MALI_GL_APIENTRY_v0( glFlush , void )
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glFrontFace , GLenum, mode )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glGenBuffers , GLsizei, n, maliGLUintPtr, buffers )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glGenTextures ,GLsizei, n, maliGLUintPtr, textures )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glGetBooleanv ,GLenum, pname, maliGLBooleanPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetBufferParameteriv , GLenum, target, GLenum, pname, maliGLIntPtr, params)
MALI_GL_APICALL GLenum MALI_GL_APIENTRY_e0( glGetError , void )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glGetFloatv ,GLenum, pname, maliGLFloatPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glGetIntegerv ,GLenum, pname, maliGLIntPtr, params)
MALI_GL_APICALL const GLubyte * MALI_GL_APIENTRY_ubp1( glGetString ,GLenum, name)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetTexParameterfv ,GLenum, target, GLenum, pname, maliGLFloatPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetTexParameteriv ,GLenum, target, GLenum, pname, maliGLIntPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glHint , GLenum, target, GLenum, mode )
MALI_GL_APICALL GLboolean MALI_GL_APIENTRY_b1( glIsBuffer ,GLuint, buffer)
MALI_GL_APICALL GLboolean MALI_GL_APIENTRY_b1( glIsEnabled ,GLenum, cap)
MALI_GL_APICALL GLboolean MALI_GL_APIENTRY_b1( glIsTexture ,GLuint, texture)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v1( glLineWidth , GLfloat, width )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glPixelStorei , GLenum, pname, GLint, param )
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v2( glPolygonOffset , GLfloat, factor, GLfloat, units )
MALI_GL_APICALL void MALI_GL_APIENTRY_v7( glReadPixels , GLint, x, GLint, y, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, maliGLVoidPtr, pixels )
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v2( glSampleCoverage , GLclampf, value, GLboolean, invert )
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glScissor ,GLint, x, GLint, y, GLint, width, GLint, height)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glStencilFunc , GLenum, func, GLint, ref, GLuint, mask )
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glStencilMask , GLuint, mask )
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glStencilOp , GLenum, sfail, GLenum, zfail, GLenum, zpass )
#if defined(__SYMBIAN32__) && !MALI_USE_GLES_1 && MALI_USE_GLES_2
MALI_GL_APICALL void MALI_GL_APIENTRY_v9( glTexImage2D ,GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLenum, format, GLenum, type, maliConstVoidPtr, pixels)
#else
MALI_GL_APICALL void MALI_GL_APIENTRY_v9( glTexImage2D ,GLenum, target, GLint, level, GLint, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLenum, format, GLenum, type, maliGLConstVoidPtr, pixels)
#endif
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v3( glTexParameterf ,GLenum, target, GLenum, pname, GLfloat, param )
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glTexParameterfv ,GLenum, target, GLenum, pname, maliGLConstFloatPtr, params )
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glTexParameteri ,GLenum, target, GLenum, pname, GLint, param )
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glTexParameteriv ,GLenum, target, GLenum, pname, maliGLConstIntPtr, params )
MALI_GL_APICALL void MALI_GL_APIENTRY_v9( glTexSubImage2D , GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, maliGLConstVoidPtr, pixels)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glViewport ,GLint, x, GLint, y, GLint, width, GLint, height)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glEGLImageTargetTexture2DOES , GLenum, target, GLeglImageOES, image )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glEGLImageTargetRenderbufferStorageOES , GLenum, target, GLeglImageOES, image )

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
MALI_GL_APICALL void MALI_GL_APIENTRY_v6( glFramebufferTexture2DMultisampleEXT, GLenum, target, GLenum, attachment, GLenum, textarget, GLuint, texture, GLint, level, GLsizei, samples)
MALI_GL_APICALL void MALI_GL_APIENTRY_v5( glRenderbufferStorageMultisampleEXT, GLenum, target, GLsizei, samples, GLenum, internalformat, GLsizei, width, GLsizei, height)
#endif

#if EXTENSION_DISCARD_FRAMEBUFFER
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glDiscardFramebufferEXT, GLenum, target, GLsizei, count, const GLenum*, attachments)
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
MALI_GL_APICALL GLenum MALI_GL_APIENTRY_e0( glGetGraphicsResetStatusEXT, void );
MALI_GL_APICALL void MALI_GL_APIENTRY_v8( glReadnPixelsEXT, GLint, x, GLint, y, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, GLsizei, bufSize, maliGLVoidPtr, pixels)
#endif

#endif /* MALI_BUILD_ANDROID_MONOLITHIC */
