/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_entrypoints.h"

#ifdef MALI_BUILD_ANDROID_MONOLITHIC

MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glAttachShader , GLuint,  program, GLuint,  shader)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glBindAttribLocation , GLuint,  program, GLuint,  index, maliConstCharPtr, name)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v4( glBlendColor ,  GLclampf, red, GLclampf, green, GLclampf, blue, GLclampf, alpha )
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glBlendEquation ,  GLenum, mode )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glBlendEquationSeparate ,  GLenum, modeRGB, GLenum, modeAlpha )
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glBlendFuncSeparate ,  GLenum, srcRGB, GLenum, dstRGB, GLenum, srcAlpha, GLenum, dstAlpha )
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glCompileShader , GLuint,  shader)
MALI_GL_APICALL GLuint MALI_GL_APIENTRY_ui0( glCreateProgram , void)
MALI_GL_APICALL GLuint MALI_GL_APIENTRY_ui1( glCreateShader , GLenum, type)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glDeleteProgram , GLuint,  program)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glDeleteShader , GLuint,  shader)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glDetachShader , GLuint,  program, GLuint,  shader)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glDisableVertexAttribArray , GLuint,  index)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glEnableVertexAttribArray , GLuint,  index)
MALI_GL_APICALL void MALI_GL_APIENTRY_v7( glGetActiveAttrib , GLuint,  program, GLuint,  index, GLsizei,  bufsize, maliGLSizeiPtr, length, maliGLIntPtr, size, maliGLEnumPtr, type, maliCharPtr, name)
MALI_GL_APICALL void MALI_GL_APIENTRY_v7( glGetActiveUniform , GLuint,  program, GLuint,  index, GLsizei,  bufsize, maliGLSizeiPtr, length, maliGLIntPtr, size, maliGLEnumPtr, type, maliCharPtr, name)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glGetAttachedShaders , GLuint,  program, GLsizei,  maxcount, maliGLSizeiPtr, count, maliGLUintPtr, shaders)
MALI_GL_APICALL GLint  MALI_GL_APIENTRY_i2( glGetAttribLocation , GLuint,  program, maliConstCharPtr, name)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glGetProgramInfoLog , GLuint,  program, GLsizei,  bufsize, maliGLSizeiPtr, length, maliCharPtr, infolog)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetProgramiv , GLuint,  program, GLenum, pname, maliGLIntPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glGetShaderInfoLog , GLuint,  shader, GLsizei,  bufsize, maliGLSizeiPtr, length, maliCharPtr, infolog)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetShaderiv , GLuint,  shader, GLenum, pname, maliGLIntPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glGetShaderPrecisionFormat , GLenum, shadertype, GLenum, precisiontype, maliGLIntPtr, range, maliGLIntPtr, precision)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glGetShaderSource , GLuint,  shader, GLsizei,  bufsize, maliGLSizeiPtr, length, maliCharPtr, source)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetUniformfv , GLuint,  program, GLint,  location, maliGLFloatPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetUniformiv , GLuint,  program, GLint,  location, maliGLIntPtr, params)
MALI_GL_APICALL GLint  MALI_GL_APIENTRY_i2( glGetUniformLocation , GLuint,  program, maliConstCharPtr, name)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetVertexAttribfv , GLuint,  index, GLenum, pname, maliGLFloatPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetVertexAttribiv , GLuint,  index, GLenum, pname, maliGLIntPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetVertexAttribPointerv , GLuint,  index, GLenum, pname, maliVoidPtrPtr, pointer)
MALI_GL_APICALL GLboolean MALI_GL_APIENTRY_b1( glIsProgram , GLuint,  program)
MALI_GL_APICALL GLboolean MALI_GL_APIENTRY_b1( glIsShader , GLuint,  shader)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glLinkProgram , GLuint,  program)
MALI_GL_APICALL void MALI_GL_APIENTRY_v0( glReleaseShaderCompiler , void)
MALI_GL_APICALL void MALI_GL_APIENTRY_v5( glShaderBinary , GLint,  n, maliGLConstUintPtr,  shaders, GLenum, binaryformat, maliConstVoidPtr,  binary, GLint,  length)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glShaderSource , GLuint,  shader, GLsizei,  count, maliConstCharPtrPtr, string, maliGLConstIntPtr, length)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glStencilFuncSeparate ,  GLenum, face, GLenum, func, GLint,  ref, GLuint,  mask )
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glStencilMaskSeparate , GLenum, face, GLuint,  mask)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glStencilOpSeparate ,  GLenum, face, GLenum, fail, GLenum, zfail, GLenum, zpass )
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v2( glUniform1f , GLint,  location, GLfloat,  x)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glUniform1fv , GLint,  location, GLsizei,  count, maliGLConstFloatPtr, v)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glUniform1i , GLint,  location, GLint,  x)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glUniform1iv , GLint,  location, GLsizei,  count, maliGLConstIntPtr, v)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v3( glUniform2f , GLint,  location, GLfloat,  x, GLfloat,  y)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glUniform2fv , GLint,  location, GLsizei,  count, maliGLConstFloatPtr, v)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glUniform2i , GLint,  location, GLint,  x, GLint,  y)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glUniform2iv , GLint,  location, GLsizei,  count, maliGLConstIntPtr, v)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v4( glUniform3f , GLint,  location, GLfloat,  x, GLfloat,  y, GLfloat,  z)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glUniform3fv , GLint,  location, GLsizei,  count, maliGLConstFloatPtr, v)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glUniform3i , GLint,  location, GLint,  x, GLint,  y, GLint,  z)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glUniform3iv , GLint,  location, GLsizei,  count, maliGLConstIntPtr, v)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v5( glUniform4f , GLint,  location, GLfloat,  x, GLfloat,  y, GLfloat,  z, GLfloat,  w)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glUniform4fv , GLint,  location, GLsizei,  count, maliGLConstFloatPtr, v)
MALI_GL_APICALL void MALI_GL_APIENTRY_v5( glUniform4i , GLint,  location, GLint,  x, GLint,  y, GLint,  z, GLint,  w)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glUniform4iv , GLint,  location, GLsizei,  count, maliGLConstIntPtr, v)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glUniformMatrix2fv , GLint,  location, GLsizei,  count, GLboolean, transpose, maliGLConstFloatPtr, value)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glUniformMatrix3fv , GLint,  location, GLsizei,  count, GLboolean, transpose, maliGLConstFloatPtr, value)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glUniformMatrix4fv , GLint,  location, GLsizei,  count, GLboolean, transpose, maliGLConstFloatPtr, value)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glUseProgram , GLuint,  program)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glValidateProgram , GLuint,  program)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v2( glVertexAttrib1f , GLuint,  indx, GLfloat,  x)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glVertexAttrib1fv , GLuint,  indx, maliGLConstFloatPtr, values)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v3( glVertexAttrib2f , GLuint,  indx, GLfloat,  x, GLfloat,  y)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glVertexAttrib2fv , GLuint,  indx, maliGLConstFloatPtr, values)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v4( glVertexAttrib3f , GLuint,  indx, GLfloat,  x, GLfloat,  y, GLfloat,  z)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glVertexAttrib3fv , GLuint,  indx, maliGLConstFloatPtr, values)
MALI_GL_APICALL MALI_SOFTFP void MALI_GL_APIENTRY_v5( glVertexAttrib4f , GLuint,  indx, GLfloat,  x, GLfloat,  y, GLfloat,  z, GLfloat,  w)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glVertexAttrib4fv , GLuint,  indx, maliGLConstFloatPtr, values)
MALI_GL_APICALL void MALI_GL_APIENTRY_v6( glVertexAttribPointer , GLuint,  index, GLint,  size, GLenum, type, GLboolean, normalized, GLsizei,  stride, maliGLConstVoidPtr,  ptr)
MALI_GL_APICALL GLboolean MALI_GL_APIENTRY_b1( glIsRenderbuffer , GLuint,  renderbuffer)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glBindRenderbuffer , GLenum, target, GLuint,  renderbuffer)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glDeleteRenderbuffers , GLsizei,  n, maliGLConstUintPtr, renderbuffers)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glGenRenderbuffers , GLsizei,  n, maliGLUintPtr, renderbuffers)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glRenderbufferStorage , GLenum, target, GLenum, internalformat, GLsizei,  width, GLsizei,  height)
MALI_GL_APICALL void MALI_GL_APIENTRY_v3( glGetRenderbufferParameteriv , GLenum, target, GLenum, pname, maliGLIntPtr, params)
MALI_GL_APICALL GLboolean MALI_GL_APIENTRY_b1( glIsFramebuffer , GLuint,  framebuffer)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glBindFramebuffer , GLenum, target, GLuint,  framebuffer)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glDeleteFramebuffers , GLsizei,  n, maliGLConstUintPtr, framebuffers)
MALI_GL_APICALL void MALI_GL_APIENTRY_v2( glGenFramebuffers , GLsizei,  n, maliGLUintPtr, framebuffers)
MALI_GL_APICALL GLenum MALI_GL_APIENTRY_e1( glCheckFramebufferStatus , GLenum, target)
MALI_GL_APICALL void MALI_GL_APIENTRY_v5( glFramebufferTexture2D , GLenum, target, GLenum, attachment, GLenum, textarget, GLuint,  texture, GLint,  level)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glFramebufferRenderbuffer , GLenum, target, GLenum, attachment, GLenum, renderbuffertarget, GLuint,  renderbuffer)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glGetFramebufferAttachmentParameteriv , GLenum, target, GLenum, attachment, GLenum, pname, maliGLIntPtr, params)
MALI_GL_APICALL void MALI_GL_APIENTRY_v1( glGenerateMipmap , GLenum, target)
#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY_v5( glGetProgramBinaryOES, GLuint, program, GLsizei, bufSize, maliGLSizeiPtr, length, maliGLEnumPtr, binaryFormat, maliVoidPtr, binary)
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glProgramBinaryOES, GLuint, program, GLenum, binaryFormat, maliConstVoidPtr, binary, GLint, length)
#endif
#if EXTENSION_ROBUSTNESS_EXT_ENABLE
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glGetnUniformfvEXT, GLuint, program, GLint, location, GLsizei, bufSize, maliGLFloatPtr, params )
MALI_GL_APICALL void MALI_GL_APIENTRY_v4( glGetnUniformivEXT, GLuint, program, GLint, location, GLsizei, bufSize, maliGLIntPtr, params  )
#endif

#endif /* MALI_BUILD_ANDROID_MONOLITHIC */

