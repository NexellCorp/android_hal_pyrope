/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_entrypoints_shim.h
 * @brief Contains helper macros for the gles*_entrypoints_shim.c files
 *  The macros are used to produce files with thin shims for export  
 */

#ifndef _GLES_ENTRYPOINTS_SHIM_H_
#define _GLES_ENTRYPOINTS_SHIM_H_

/* this wraps the whole file so nothing is included without this define */
#ifdef MALI_BUILD_ANDROID_MONOLITHIC

#include "gles_entrypoints.h"
#include "gles_shims_common.h"

MALI_DECLARE_SHIM_GL_APIENTRY_v1( glActiveTexture , GLenum, texture )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glBindBuffer , GLenum, target, GLuint, buffer )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glBindTexture ,GLenum, target, GLuint, texture)
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glBlendFunc , GLenum, src, GLenum, dst )
MALI_DECLARE_SHIM_GL_APIENTRY_v4( glBufferData , GLenum, target, GLsizeiptr, size, const GLvoid*, data, GLenum, usage )
MALI_DECLARE_SHIM_GL_APIENTRY_v4( glBufferSubData, GLenum, target, GLintptr, offset, GLsizeiptr, size, const GLvoid *, data)
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glClear , GLbitfield, mask)
MALI_DECLARE_SHIM_GL_APIENTRY_v4( glClearColor , GLclampf, red, GLclampf, green, GLclampf, blue, GLclampf, alpha )
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glClearDepthf , GLclampf, depth )
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glClearStencil , GLint, s )
MALI_DECLARE_SHIM_GL_APIENTRY_v4( glColorMask , GLboolean, red, GLboolean, green, GLboolean, blue, GLboolean, alpha )
MALI_DECLARE_SHIM_GL_APIENTRY_v8( glCompressedTexImage2D , GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLsizei, imageSize, maliGLConstVoidPtr, data)
MALI_DECLARE_SHIM_GL_APIENTRY_v9( glCompressedTexSubImage2D , GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLsizei, imageSize, maliGLConstVoidPtr, data)
MALI_DECLARE_SHIM_GL_APIENTRY_v8( glCopyTexImage2D , GLenum, target, GLint, level, GLenum, internalformat, GLint, x, GLint, y, GLsizei, width, GLsizei, height, GLint, border)
MALI_DECLARE_SHIM_GL_APIENTRY_v8( glCopyTexSubImage2D , GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLint, x, GLint, y, GLsizei, width, GLsizei, height)
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glCullFace , GLenum, mode )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glDeleteBuffers ,GLsizei, n, maliGLConstUintPtr, buffers)
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glDeleteTextures ,GLsizei, n, maliGLConstUintPtr, textures )
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glDepthFunc , GLenum, func )
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glDepthMask , GLboolean, flag )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glDepthRangef ,GLclampf, n, GLclampf, f)
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glDisable , GLenum, cap )
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glDrawArrays , GLenum, mode, GLint, first, GLsizei, count )
MALI_DECLARE_SHIM_GL_APIENTRY_v4( glDrawElements , GLenum, mode, GLsizei, count, GLenum, type, maliGLConstVoidPtr, indices)
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glEnable , GLenum, cap )
MALI_DECLARE_SHIM_GL_APIENTRY_v0( glFinish , void )
MALI_DECLARE_SHIM_GL_APIENTRY_v0( glFlush , void )
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glFrontFace , GLenum, mode )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glGenBuffers , GLsizei, n, maliGLUintPtr, buffers )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glGenTextures ,GLsizei, n, maliGLUintPtr, textures )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glGetBooleanv ,GLenum, pname, maliGLBooleanPtr, params)
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glGetBufferParameteriv , GLenum, target, GLenum, pname, maliGLIntPtr, params)
MALI_DECLARE_SHIM_GL_APIENTRY_e0( glGetError , void )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glGetFloatv ,GLenum, pname, maliGLFloatPtr, params)
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glGetIntegerv ,GLenum, pname, maliGLIntPtr, params)
MALI_DECLARE_SHIM_GL_APIENTRY_ubp1( glGetString ,GLenum, name)
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glGetTexParameterfv ,GLenum, target, GLenum, pname, maliGLFloatPtr, params)
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glGetTexParameteriv ,GLenum, target, GLenum, pname, maliGLIntPtr, params)
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glHint , GLenum, target, GLenum, mode )
MALI_DECLARE_SHIM_GL_APIENTRY_b1( glIsBuffer ,GLuint, buffer)
MALI_DECLARE_SHIM_GL_APIENTRY_b1( glIsEnabled ,GLenum, cap)
MALI_DECLARE_SHIM_GL_APIENTRY_b1( glIsTexture ,GLuint, texture)
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glLineWidth , GLfloat, width )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glPixelStorei , GLenum, pname, GLint, param )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glPolygonOffset , GLfloat, factor, GLfloat, units )
MALI_DECLARE_SHIM_GL_APIENTRY_v7( glReadPixels , GLint, x, GLint, y, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, maliGLVoidPtr, pixels )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glSampleCoverage , GLclampf, value, GLboolean, invert )
MALI_DECLARE_SHIM_GL_APIENTRY_v4( glScissor ,GLint, x, GLint, y, GLint, width, GLint, height)
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glStencilFunc , GLenum, func, GLint, ref, GLuint, mask )
MALI_DECLARE_SHIM_GL_APIENTRY_v1( glStencilMask , GLuint, mask )
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glStencilOp , GLenum, sfail, GLenum, zfail, GLenum, zpass )
#if defined(__SYMBIAN32__) && !MALI_USE_GLES_1 && MALI_USE_GLES_2
MALI_DECLARE_SHIM_GL_APIENTRY_v9( glTexImage2D ,GLenum, target, GLint, level, GLenum, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLenum, format, GLenum, type, maliConstVoidPtr, pixels)
#else
MALI_DECLARE_SHIM_GL_APIENTRY_v9( glTexImage2D ,GLenum, target, GLint, level, GLint, internalformat, GLsizei, width, GLsizei, height, GLint, border, GLenum, format, GLenum, type, maliGLConstVoidPtr, pixels)
#endif
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glTexParameterf ,GLenum, target, GLenum, pname, GLfloat, param )
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glTexParameterfv ,GLenum, target, GLenum, pname, maliGLConstFloatPtr, params )
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glTexParameteri ,GLenum, target, GLenum, pname, GLint, param )
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glTexParameteriv ,GLenum, target, GLenum, pname, maliGLConstIntPtr, params )
MALI_DECLARE_SHIM_GL_APIENTRY_v9( glTexSubImage2D , GLenum, target, GLint, level, GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, maliGLConstVoidPtr, pixels)
MALI_DECLARE_SHIM_GL_APIENTRY_v4( glViewport ,GLint, x, GLint, y, GLint, width, GLint, height)
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glEGLImageTargetTexture2DOES , GLenum, target, GLeglImageOES, image )
MALI_DECLARE_SHIM_GL_APIENTRY_v2( glEGLImageTargetRenderbufferStorageOES , GLenum, target, GLeglImageOES, image )

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
MALI_DECLARE_SHIM_GL_APIENTRY_v6( glFramebufferTexture2DMultisampleEXT, GLenum, target, GLenum, attachment, GLenum, textarget, GLuint, texture, GLint, level, GLsizei, samples)
MALI_DECLARE_SHIM_GL_APIENTRY_v5( glRenderbufferStorageMultisampleEXT, GLenum, target, GLsizei, samples, GLenum, internalformat, GLsizei, width, GLsizei, height)
#endif

#if EXTENSION_DISCARD_FRAMEBUFFER
MALI_DECLARE_SHIM_GL_APIENTRY_v3( glDiscardFramebufferEXT, GLenum, target, GLsizei, count, const GLenum*, attachments )
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
MALI_DECLARE_SHIM_GL_APIENTRY_v0( glGetGraphicsResetStatusEXT, void );
MALI_DECLARE_SHIM_GL_APIENTRY_v8( glReadnPixelsEXT, GLint, x, GLint, y, GLsizei, width, GLsizei, height, GLenum, format, GLenum, type, GLsizei, bufSize, maliGLVoidPtr, pixels)
#endif

#endif /* MALI_BUILD_ANDROID_MONOLITHIC*/

#endif /* _GLES_ENTRYPOINTS_SHIM_H */
