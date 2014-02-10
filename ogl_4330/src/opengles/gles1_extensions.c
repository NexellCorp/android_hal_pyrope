/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles1_extensions.h"
#include "gles_extensions.h"
#include "gles_config_extension.h"
#include "gles_base.h"


GL_API void MALI_GLES_NAME_WRAP(glDrawTexfOES)(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height);
GL_API void MALI_GLES_NAME_WRAP(glDrawTexfvOES)(const GLfloat * coords);
GL_API void MALI_GLES_NAME_WRAP(glDrawTexiOES)(GLint x, GLint y, GLint z, GLint width, GLint height);
GL_API void MALI_GLES_NAME_WRAP(glDrawTexivOES)(const GLint * coords);
GL_API void MALI_GLES_NAME_WRAP(glDrawTexsOES)(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height);
GL_API void MALI_GLES_NAME_WRAP(glDrawTexsvOES)(const GLshort* coords);
GL_API void MALI_GLES_NAME_WRAP(glDrawTexxOES)(const GLfixed x, const GLfixed y, const GLfixed z, const GLfixed width, const GLfixed height);
GL_API void MALI_GLES_NAME_WRAP(glDrawTexxvOES)(const GLfixed * coords);
GL_API void MALI_GLES_NAME_WRAP(glCurrentPaletteMatrixOES)(GLuint index);
GL_API void MALI_GLES_NAME_WRAP(glLoadPaletteFromModelViewMatrixOES)(void);
GL_API void MALI_GLES_NAME_WRAP(glMatrixIndexPointerOES)(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
GL_API void MALI_GLES_NAME_WRAP(glWeightPointerOES)(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
GL_API void MALI_GLES_NAME_WRAP(glPointSizePointerOES)(GLenum type, GLsizei stride, const GLvoid* pointer);
GL_API GLbitfield MALI_GLES_NAME_WRAP(glQueryMatrixxOES)(GLfixed* mantissa, GLint * exponent);
GL_API void MALI_GLES_NAME_WRAP(glGenFramebuffersOES)(GLsizei n, GLuint * framebuffers);
GL_API GLboolean MALI_GLES_NAME_WRAP(glIsRenderbufferOES)(GLuint renderbuffer);
GL_API void MALI_GLES_NAME_WRAP(glBindRenderbufferOES)(GLenum target, GLuint renderbuffer);
GL_API void MALI_GLES_NAME_WRAP(glDeleteRenderbuffersOES)(GLsizei n, const GLuint * renderbuffers);
GL_API void MALI_GLES_NAME_WRAP(glGenRenderbuffersOES)(GLsizei n, GLuint * renderbuffers);
GL_API void MALI_GLES_NAME_WRAP(glRenderbufferStorageOES)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
GL_API void MALI_GLES_NAME_WRAP(glGetRenderbufferParameterivOES)(GLenum target, GLenum pname, GLint * params);
GL_API GLboolean MALI_GLES_NAME_WRAP(glIsFramebufferOES)(GLuint framebuffer);
GL_API void MALI_GLES_NAME_WRAP(glBindFramebufferOES)(GLenum target, GLuint framebuffer);
GL_API void MALI_GLES_NAME_WRAP(glDeleteFramebuffersOES)(GLsizei n, const GLuint * framebuffers);
GL_API GLenum MALI_GLES_NAME_WRAP(glCheckFramebufferStatusOES)(GLenum target);
GL_API void MALI_GLES_NAME_WRAP(glFramebufferTexture2DOES)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GL_API void MALI_GLES_NAME_WRAP(glFramebufferRenderbufferOES)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
GL_API void MALI_GLES_NAME_WRAP(glGetFramebufferAttachmentParameterivOES)(GLenum target, GLenum attachment, GLenum pname, GLint * params);
GL_API void MALI_GLES_NAME_WRAP(glGenerateMipmapOES)(GLenum target);

GL_API void MALI_GLES_NAME_WRAP(glFramebufferTexture2DMultisampleEXT)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
GL_API void MALI_GLES_NAME_WRAP(glRenderbufferStorageMultisampleEXT)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei  width, GLsizei height);

GL_API void MALI_GLES_NAME_WRAP(glTexGenfOES)(GLenum coord, GLenum pname, GLfloat param);
GL_API void MALI_GLES_NAME_WRAP(glTexGenfvOES)(GLenum coord, GLenum pname, const GLfloat *params);
GL_API void MALI_GLES_NAME_WRAP(glTexGeniOES)(GLenum coord, GLenum pname, GLint param);
GL_API void MALI_GLES_NAME_WRAP(glTexGenivOES)(GLenum coord, GLenum pname, const GLint *params);
GL_API void MALI_GLES_NAME_WRAP(glTexGenxOES)(GLenum coord, GLenum pname, GLfixed param);
GL_API void MALI_GLES_NAME_WRAP(glTexGenxvOES)(GLenum coord, GLenum pname, const GLfixed *params);
GL_API void MALI_GLES_NAME_WRAP(glGetTexGenfvOES)(GLenum coord, GLenum pname, GLfloat *params);
GL_API void MALI_GLES_NAME_WRAP(glGetTexGenivOES)(GLenum coord, GLenum pname, GLint *params);
GL_API void MALI_GLES_NAME_WRAP(glGetTexGenxvOES)(GLenum coord, GLenum pname, GLfixed *params);

#if EXTENSION_DISCARD_FRAMEBUFFER
GL_API void MALI_GLES_NAME_WRAP(glDiscardFramebufferEXT)(GLenum, GLsizei, const GLenum*);
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
GL_API GLenum MALI_GLES_NAME_WRAP(glGetGraphicsResetStatusEXT)(void);
GL_API void MALI_GLES_NAME_WRAP(glReadnPixelsEXT)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLvoid *pixels);
#endif
static const gles_extension _gles1_extensions[] = {

	/* dummy-entry, in case of empty list (empty initializers not allowed in ANSI C) */
	{NULL, NULL},

	/*** begin extension-list ***/

#if EXTENSION_QUERY_MATRIX_OES_ENABLE
	GLES_EXTENSION(glQueryMatrixxOES),
#endif

#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
	GLES_EXTENSION(glCurrentPaletteMatrixOES),
	GLES_EXTENSION(glWeightPointerOES),
	GLES_EXTENSION(glMatrixIndexPointerOES),
	GLES_EXTENSION(glLoadPaletteFromModelViewMatrixOES),
#endif

#if 0 /* org */
#if EXTENSION_POINT_SIZE_POINTER_OES_ENABLE
	GLES_EXTENSION(glPointSizePointerOES),
#endif
#else /* nexell add */
#if EXTENSION_POINT_SIZE_ARRAY_OES_ENABLE
	GLES_EXTENSION(glPointSizePointerOES),
#endif

#endif

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	GLES_EXTENSION(glIsRenderbufferOES),
	GLES_EXTENSION(glBindRenderbufferOES),
	GLES_EXTENSION(glDeleteRenderbuffersOES),
	GLES_EXTENSION(glGenRenderbuffersOES),
	GLES_EXTENSION(glRenderbufferStorageOES),
	GLES_EXTENSION(glGetRenderbufferParameterivOES),
	GLES_EXTENSION(glIsFramebufferOES),
	GLES_EXTENSION(glBindFramebufferOES),
	GLES_EXTENSION(glDeleteFramebuffersOES),
	GLES_EXTENSION(glGenFramebuffersOES),
	GLES_EXTENSION(glCheckFramebufferStatusOES),
	GLES_EXTENSION(glFramebufferTexture2DOES),
	GLES_EXTENSION(glFramebufferRenderbufferOES),
	GLES_EXTENSION(glGetFramebufferAttachmentParameterivOES),
	GLES_EXTENSION(glGenerateMipmapOES),
#if EXTENSION_DISCARD_FRAMEBUFFER
	GLES_EXTENSION(glDiscardFramebufferEXT),
#endif
#endif

#if EXTENSION_DRAW_TEX_OES_ENABLE
	GLES_EXTENSION(glDrawTexfOES ),
	GLES_EXTENSION(glDrawTexfvOES ),
	GLES_EXTENSION(glDrawTexiOES ),
	GLES_EXTENSION(glDrawTexivOES ),
	GLES_EXTENSION(glDrawTexsOES ),
	GLES_EXTENSION(glDrawTexsvOES ),
	GLES_EXTENSION(glDrawTexxOES ),
	GLES_EXTENSION(glDrawTexxvOES ),
#endif

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	GLES_EXTENSION(glFramebufferTexture2DMultisampleEXT),
	GLES_EXTENSION(glRenderbufferStorageMultisampleEXT),
#endif

#if EXTENSION_TEXTURE_CUBE_MAP_ENABLE
	GLES_EXTENSION(glTexGenfOES),
	GLES_EXTENSION(glTexGenfvOES),
	GLES_EXTENSION(glTexGeniOES),
	GLES_EXTENSION(glTexGenivOES),
	GLES_EXTENSION(glTexGenxOES),
	GLES_EXTENSION(glTexGenxvOES),
	GLES_EXTENSION(glGetTexGenfvOES),
	GLES_EXTENSION(glGetTexGenivOES),
	GLES_EXTENSION(glGetTexGenxvOES),
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
	GLES_EXTENSION(glGetGraphicsResetStatusEXT),	
	GLES_EXTENSION(glReadnPixelsEXT)
#endif
	/*** end extension-list ***/
};

MALI_INTER_MODULE_API void (* _gles1_get_proc_address(const char *procname))(void)
{
	return _gles_get_proc_address(procname, _gles1_extensions, MALI_ARRAY_SIZE(_gles1_extensions));
}

