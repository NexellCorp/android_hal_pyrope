/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _MALI_GL2EXT_H_
#define _MALI_GL2EXT_H_


/*
 * This is the Mali gl wrapper, for use in driver development only.  All
 * applications should be built with the stock gl2.h and gl2ext.h
 */

/* current khronos distributed gl.h, must be on include path */
#include <GLES2/gl2ext.h>

/* driver specific contents can be defined here */

/** ETC */
#define GL_OES_compressed_ETC1_RGB8_texture 1
#define GL_ETC1_RGB8_OES           0x8D64

/** video controls extensions */
#ifdef EXTENSION_VIDEO_CONTROLS_ARM_ENABLE
#define GL_TEXTURE_BRIGHTNESS_ARM           0x6001
#define GL_TEXTURE_CONTRAST_ARM             0x6002
#define GL_TEXTURE_SATURATION_ARM           0x6003
#endif

/* GL_OES_standard_derivatives */
#ifndef GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES                  0x8B8B
#endif

/* GL_OES_packed_depth_stencil */
#ifndef GL_OES_packed_depth_stencil
#define GL_DEPTH_STENCIL_OES                                    0x84F9
#define GL_UNSIGNED_INT_24_8_OES                                0x84FA
#define GL_DEPTH24_STENCIL8_OES                                 0x88F0
#endif

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT                                             0x80E1
#endif

/* GL_EXT_blend_minmax */
#ifndef GL_MIN_EXT
#define GL_MIN_EXT                                              0x8007
#endif
#ifndef GL_MAX_EXT
#define GL_MAX_EXT                                              0x8008
#endif

/* GL_ARM_mali_shader_binary */
#ifndef GL_ARM_mali_shader_binary
#define GL_MALI_SHADER_BINARY_ARM                               0x8F60
#endif

#ifndef GL_MALI_PROGRAM_BINARY_ARM
#define GL_MALI_PROGRAM_BINARY_ARM                              0x8F61
#endif


/* GL_OES_EGL_image_external */
#ifndef GL_OES_EGL_image_external
/* GLeglImageOES defined in GL_OES_EGL_image already. */
#define GL_TEXTURE_EXTERNAL_OES                                 0x8D65
#define GL_SAMPLER_EXTERNAL_OES                                 0x8D66
#define GL_TEXTURE_BINDING_EXTERNAL_OES                         0x8D67
#define GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES                     0x8D68
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

GL_APICALL void GL_APIENTRY glRenderbufferStorageMultisampleEXT( GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
GL_APICALL void GL_APIENTRY glFramebufferTexture2DMultisampleEXT( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);

#endif

/* GL_EXT_discard_framebuffer */
#ifndef GL_EXT_DISCARD_FRAMEBUFFER
#define GL_EXT_DISCARD_FRAMEBUFFER

#define GL_COLOR_EXT                               0x1800
#define GL_DEPTH_EXT                               0x1801
#define GL_STENCIL_EXT                             0x1802

GL_APICALL void GL_APIENTRY glDiscardFramebufferEXT( GLenum target, GLsizei numAttachments, const GLenum *attachments );

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
GL_APICALL GLenum GL_APIENTRY glGetGraphicsResetStatusEXT (void);
GL_APICALL void GL_APIENTRY glReadnPixelsEXT (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data);
GL_APICALL void GL_APIENTRY glGetnUniformfvEXT (GLuint program, GLint location, GLsizei bufSize, float *params);
GL_APICALL void GL_APIENTRY glGetnUniformivEXT (GLuint program, GLint location, GLsizei bufSize, GLint *params);
#endif
typedef GLenum (GL_APIENTRYP PFNGLGETGRAPHICSRESETSTATUSEXTPROC) (void);
typedef void (GL_APIENTRYP PFNGLREADNPIXELSEXTPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMFVEXTPROC) (GLuint program, GLint location, GLsizei bufSize, float *params);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMIVEXTPROC) (GLuint program, GLint location, GLsizei bufSize, GLint *params);
#endif

#endif /* _MALI_GL2EXT_H_ */
