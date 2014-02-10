/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __gl_ext_h_
#define __gl_ext_h_

/* current khronos distributed glext.h, must be on include path */
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES/glplatform.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__SYMBIAN32__)
/**
 * Defined in 3rdparty\include\khronos\GLES\glext.h
 * We are using \epoc32\include\GLES\glext.h
 */

/* GL_OES_element_index_uint */
#ifndef GL_UNSIGNED_INT
#define GL_UNSIGNED_INT                   0x1405
#endif

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT                       0x80E1
#endif

#ifndef GL_DEPTH_STENCIL_OES
#define GL_DEPTH_STENCIL_OES              0x84F9
#endif

#ifndef GL_UNSIGNED_INT_24_8_OES
#define GL_UNSIGNED_INT_24_8_OES          0x84FA
#endif

#ifndef GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_OES           0x88F0
#endif

#endif /* #if defined(__SYMBIAN32__) */

#ifndef GL_MALI_PROGRAM_BINARY_ARM
#define GL_MALI_PROGRAM_BINARY_ARM        0x8F61
#endif

#ifndef GL_EXT_MULTISAMPLED_RENDER_TO_TEXTURE
#define GL_EXT_MULTISAMPLED_RENDER_TO_TEXTURE

/**
 * Note: The specification for GL_EXT_MULTISAMPLED_RENDER_TO_TEXTURE was released with incorrect
 * ENUM values for its four #defines. The range 0x91xx is reserved for vendor-specific defines, 
 * and this is an EXT extension. The spec was changed by Khronos, but not before a mali driver 
 * was shipped supporting the 0x91xx values. 
 *
 * To make matters worse, the changed #defines had not only different values, but also different names. 
 *
 * We basically need to support both the new and old values and names. The names are defined by the 
 * programmer's GL header version, and doesn't really matter - but the values are important. 
 * To ensure this, they are both wrapped up in MALI_* enums here, and all driver code must handle both!
 *
 * The old set
 * #define GL_RENDERBUFFER_SAMPLES_EXT                             0x9133
 * #define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT               0x9134
 * #define GL_MAX_SAMPLES_EXT                                      0x9135
 * #define GL_TEXTURE_SAMPLES_EXT                                  0x9136
 *
 * The new set 
 * #define GL_RENDERBUFFER_SAMPLES_EXT                             0x8CAB
 * #define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT               0x8D56
 * #define GL_MAX_SAMPLES_EXT                                      0x8D57
 * #define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT           0x8D6C
 */

#define MALI_GL_RENDERBUFFER_SAMPLES_EXT_V1                        0x9133
#define MALI_GL_RENDERBUFFER_SAMPLES_EXT_V2                        0x8CAB
#define MALI_GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT_V1          0x9134
#define MALI_GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT_V2          0x8D56
#define MALI_GL_MAX_SAMPLES_EXT_V1                                 0x9135
#define MALI_GL_MAX_SAMPLES_EXT_V2                                 0x8D57
#define MALI_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT_V1      0x9136
#define MALI_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT_V2      0x8D6C

GL_API void GL_APIENTRY glRenderbufferStorageMultisampleEXT( GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
GL_API void GL_APIENTRY glFramebufferTexture2DMultisampleEXT( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);

#endif

/* GL_EXT_discard_framebuffer */
#ifndef GL_EXT_DISCARD_FRAMEBUFFER
#define GL_EXT_DISCARD_FRAMEBUFFER

#define GL_COLOR_EXT                               0x1800
#define GL_DEPTH_EXT                               0x1801
#define GL_STENCIL_EXT                             0x1802

GL_API void GL_APIENTRY glDiscardFramebufferEXT( GLenum target, GLsizei numAttachments, const GLenum *attachments );

#endif


/* GL_EXT_robustness */
#ifndef GL_EXT_robustness
#define GL_EXT_robustness 1
/* reuse GL_NO_ERROR */
#define GL_GUILTY_CONTEXT_RESET_EXT                             0x8253
#define GL_INNOCENT_CONTEXT_RESET_EXT                           0x8254
#define GL_UNKNOWN_CONTEXT_RESET_EXT                            0x8255
#define GL_CONTEXT_ROBUST_ACCESS_EXT                            0x90F3
#define GL_RESET_NOTIFICATION_STRATEGY_EXT                      0x8256
#define GL_LOSE_CONTEXT_ON_RESET_EXT                            0x8252
#define GL_NO_RESET_NOTIFICATION_EXT                            0x8261

#ifdef GL_GLEXT_PROTOTYPES
GL_API GLenum GL_APIENTRY glGetGraphicsResetStatusEXT (void);
GL_API void GL_APIENTRY glReadnPixelsEXT (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data);
GL_API void GL_APIENTRY glGetnUniformfvEXT (GLuint program, GLint location, GLsizei bufSize, float *params);
GL_API void GL_APIENTRY glGetnUniformivEXT (GLuint program, GLint location, GLsizei bufSize, GLint *params);
#endif
typedef GLenum (GL_APIENTRYP PFNGLGETGRAPHICSRESETSTATUSEXTPROC) (void);
typedef void (GL_APIENTRYP PFNGLREADNPIXELSEXTPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMFVEXTPROC) (GLuint program, GLint location, GLsizei bufSize, float *params);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMIVEXTPROC) (GLuint program, GLint location, GLsizei bufSize, GLint *params);
#endif




#ifdef __cplusplus
}
#endif

#endif /* __gl_ext_h_ */
