/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * ( C ) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_vtable.h
 * @brief Defines the virtual table of OpenGL 1.1/2.0 function pointers for the second layer's functions.
 * @note This interface shall only included by gles_entrypioints.c and glesX_vtable.c
 */
#ifndef _GLES_VTABLE_H_
#define _GLES_VTABLE_H_

#include <base/mali_context.h>
#include "gles_base.h"
#include "gles_object.h"
#include "gles_util.h"

struct gles_state;
struct gles_common_state;
struct gles_context;
struct gles_framebuffer_control;
struct gles_texture_environment;
struct gles_rasterization;
struct gles_viewport;
struct gles_pixel;
struct gles_multisampling;
struct gles_scissor;
struct gles_stencil_test;
struct gles_program_rendering_state;

#if MALI_USE_GLES_1
struct gles1_alpha_test;
struct gles1_current;
struct gles1_coloring;
struct gles1_lighting;
struct gles1_logic_op;
struct gles1_transform;
struct gles1_texture_environment;
struct gles1_rasterization;
#endif

#if MALI_USE_GLES_2
struct gles2_program_environment;
#endif

typedef struct gles_vtable
{
	/* Common OpenGL ES 1.1 and 2.0 APIs */
	GLenum ( * fp_glActiveTexture )( struct gles_context *, GLenum ) MALI_CHECK_RESULT;
	GLenum ( * fp_glBindBuffer )( struct gles_context *, GLenum, GLuint);
	GLenum ( * fp_glBindTexture )( struct gles_context *, GLenum, GLuint );
	GLenum ( * fp_glBlendFunc )( struct gles_context *, GLenum, GLenum, GLenum, GLenum );
	GLenum ( * fp_glBufferData )( mali_base_ctx_handle, struct gles_vertex_array *, enum gles_api_version, GLenum, GLsizeiptr, const GLvoid *, GLenum );
	GLenum ( * fp_glBufferSubData )( mali_base_ctx_handle, struct gles_vertex_array *, GLenum, GLintptr, GLsizeiptr, const GLvoid * );
	GLenum ( * fp_glClear )( struct gles_context *, GLbitfield );
	void ( * fp_glClearColor )( struct gles_framebuffer_control *, GLftype, GLftype, GLftype, GLftype );
	void ( * fp_glClearDepthf )( struct gles_framebuffer_control *, GLftype );
	void ( * fp_glClearStencil )( struct gles_framebuffer_control *, GLint );
	void ( * fp_glColorMask )( struct gles_context *ctx, GLboolean, GLboolean, GLboolean, GLboolean );
	GLenum ( * fp_glCompressedTexImage2D )( struct gles_context *, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid * );
	GLenum ( * fp_glCompressedTexSubImage2D )( struct gles_context *, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid * );
	GLenum ( * fp_glCopyTexImage2D )( struct gles_context *, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint );
	GLenum ( * fp_glCopyTexSubImage2D )( struct gles_context *, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei );
	GLenum ( * fp_glCullFace )( struct gles_rasterization *, GLenum );
	GLenum ( * fp_glDeleteBuffers )( mali_named_list *, struct gles_vertex_array *, GLsizei, const GLuint * );
	GLenum ( * fp_glDeleteTextures )( struct gles_context *, GLsizei, const GLuint * );
	GLenum ( * fp_glDepthFunc )( struct gles_context *, GLenum );
	void ( * fp_glDepthMask )( struct gles_context *, GLboolean );
	void ( * fp_glDepthRangef )( struct gles_context *, GLftype, GLftype );
	GLenum ( * fp_glDisable )( struct gles_context *, GLenum, GLboolean );
	GLenum ( * fp_glDrawArrays )( struct gles_context *, GLenum, GLint, GLint );
	GLenum ( * fp_glDrawElements )( struct gles_context *, GLenum, GLint, GLenum, const void * );
	GLenum ( * fp_glEnable )( struct gles_context *, GLenum, GLboolean );
	GLenum ( * fp_glFinish )( struct gles_context * );
	GLenum ( * fp_glFlush )( struct gles_context *, mali_bool );
	GLenum ( * fp_glFrontFace )( struct gles_rasterization *, GLenum );
	GLenum ( * fp_glGenBuffers )( struct mali_named_list *, GLsizei, GLuint *, enum gles_wrappertype );
	GLenum ( * fp_glGenTextures )( struct mali_named_list *, GLsizei, GLuint *, enum gles_wrappertype );
	GLenum ( * fp_glGetBooleanv )( struct gles_context *, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetBufferParameteriv )( struct gles_common_state *, GLenum, GLenum, GLint * );
	GLenum ( * fp_glGetError )( struct gles_context * );
	GLenum ( * fp_glGetFloatv )( struct gles_context*, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetIntegerv )( struct gles_context *, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetString )( struct gles_context *, GLenum, const GLubyte ** );
	GLenum ( * fp_glGetTexParameterfv )( struct gles_common_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetTexParameteriv )( struct gles_common_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glHint )( struct gles_state *, GLenum, GLenum );
	GLboolean ( * fp_glIsBuffer )( mali_named_list *, GLuint );
	GLenum ( * fp_glIsEnabled )( struct gles_context *, GLenum, GLboolean * );
	GLboolean ( * fp_glIsTexture )( mali_named_list *, GLuint );
	GLenum ( * fp_glLineWidth )( struct gles_context *, GLftype );
	GLenum ( * fp_glPixelStorei )( struct gles_pixel *, GLenum, GLint );
	GLenum ( * fp_glPolygonOffset )( struct gles_context *, GLftype, GLftype );
	GLenum ( * fp_glReadPixels )( struct gles_context *, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid * );
	void ( * fp_glSampleCoverage )( struct gles_context *, GLfloat, GLboolean );
	GLenum ( * fp_glScissor )( struct gles_context *, GLint, GLint, GLsizei, GLsizei );
	GLenum ( * fp_glStencilFunc )( struct gles_context *, GLenum, GLenum, GLint, GLuint );
	GLenum ( * fp_glStencilMask )( struct gles_context *, GLenum, GLuint );
	GLenum ( * fp_glStencilOp )( struct gles_context *, GLenum, GLenum, GLenum, GLenum );
	GLenum ( * fp_glTexImage2D )( struct gles_context *, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *, GLint );
	GLenum ( * fp_glTexParameterf )( struct gles_texture_environment *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexParameterfv )( struct gles_texture_environment *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexParameteri )( struct gles_texture_environment *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexParameteriv )( struct gles_texture_environment *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexSubImage2D )( struct gles_context *, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid *, GLint );
	GLenum ( * fp_glViewport )( struct gles_state *, GLint, GLint, GLint, GLint );

	/* Function pointers related to the Framebuffer Object Extension (Which is a part of core in
	 * OpenGL ES 2.x and therefore always defined for that API) */
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	GLboolean ( * fp_glIsRenderbuffer )( mali_named_list *, GLuint );
	GLenum ( * fp_glBindRenderbuffer )( struct mali_named_list *, struct gles_renderbuffer_state *, GLenum, GLuint );
	GLenum ( * fp_glDeleteRenderbuffers )( struct mali_named_list *, struct gles_renderbuffer_state *, struct gles_framebuffer_state *, GLsizei, const GLuint * );
	GLenum ( * fp_glGenRenderbuffers )( struct mali_named_list *, GLsizei, GLuint *, enum gles_wrappertype );
	GLenum ( * fp_glRenderbufferStorage )( mali_base_ctx_handle, mali_named_list *,	gles_renderbuffer_state *, GLenum, GLenum, GLsizei, GLsizei );
	GLenum ( * fp_glGetRenderbufferParameteriv )( gles_renderbuffer_state *, GLenum, GLenum, GLint * );
	GLboolean ( * fp_glIsFramebuffer )( mali_named_list *, GLuint );
	GLenum ( * fp_glBindFramebuffer )( struct gles_context *, mali_named_list *, struct gles_framebuffer_state *, GLenum, GLuint );
	GLenum ( * fp_glDeleteFramebuffers )( struct gles_context *ctx, GLsizei n, const GLuint *framebuffers );
	GLenum ( * fp_glGenFramebuffers )( struct mali_named_list *, GLsizei, GLuint *, enum gles_wrappertype );
	GLenum ( * fp_glCheckFramebufferStatus )( struct gles_framebuffer_state *, GLenum , GLenum * );
	GLenum ( * fp_glFramebufferTexture2D )( struct gles_context *ctx, struct gles_framebuffer_state *, mali_named_list *, mali_named_list *, mali_named_list *, GLenum, GLenum, GLenum, GLuint, GLint );
	GLenum ( * fp_glFramebufferRenderbuffer )( struct gles_framebuffer_state *, mali_named_list *, mali_named_list *, mali_named_list *, GLenum, GLenum, GLenum, GLuint);
	GLenum ( * fp_glGetFramebufferAttachmentParameteriv )( struct gles_framebuffer_state *, mali_named_list *, GLenum, GLenum, GLenum, GLint * );
	GLenum ( * fp_glGenerateMipmap )( struct gles_context *, gles_texture_environment *, mali_named_list *, GLenum );

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	GLenum ( * fp_glRenderbufferStorageMultisampleEXT )( mali_base_ctx_handle, mali_named_list *,	gles_renderbuffer_state *, GLenum, GLsizei, GLenum, GLsizei, GLsizei );
	GLenum ( * fp_glFramebufferTexture2DMultisampleEXT )( struct gles_context *ctx, struct gles_framebuffer_state *, mali_named_list *, mali_named_list *, mali_named_list *, GLenum, GLenum, GLenum, GLuint, GLint, GLsizei );
#endif

#if EXTENSION_DISCARD_FRAMEBUFFER
	GLenum ( * fp_glDiscardFramebufferEXT )( struct gles_context *, struct gles_framebuffer_state *, GLenum, GLsizei, const GLenum * );
#endif
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
	GLenum ( * fp_glGetGraphicsResetStatusEXT )(struct gles_context*);
	GLenum ( * fp_glReadnPixelsEXT )( struct gles_context *, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLsizei, GLvoid * );
#endif

	/* Function pointers related to the EGL Image extension */
#if EXTENSION_EGL_IMAGE_OES_ENABLE
	GLenum ( * fp_glEGLImageTargetTexture2DOES )(struct gles_context *ctx, GLenum target, GLeglImageOES image);
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	GLenum ( * fp_glEGLImageTargetRenderbufferStorageOES )(struct gles_context *ctx, GLenum target, GLeglImageOES image);
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */
#endif /* EXTENSION_EGL_IMAGE_OES_ENABLE */

#if MALI_USE_GLES_1

	/* OpenGL ES 1.1-specific APIs */
	GLenum ( * fp_glAlphaFunc )( struct gles_context *ctx, GLenum, GLftype );
	GLenum ( * fp_glAlphaFuncx )( struct gles_context *ctx, GLenum, GLftype );
	void ( * fp_glClearColorx)( struct gles_framebuffer_control *, GLftype, GLftype, GLftype, GLftype );
	void ( * fp_glClearDepthx)( struct gles_framebuffer_control *, GLftype );
	GLenum ( * fp_glClientActiveTexture )( struct gles_vertex_array *, GLenum );
	GLenum ( * fp_glClipPlanef )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glClipPlanex )(  struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glColor4f )( struct gles_context *ctx, GLftype, GLftype, GLftype, GLftype );
	GLenum ( * fp_glColor4ub )( struct gles_context *ctx, GLftype, GLftype, GLftype, GLftype );
	GLenum ( * fp_glColor4x )( struct gles_context *ctx, GLftype, GLftype, GLftype, GLftype );
	GLenum ( * fp_glColorPointer )( struct gles_context *, GLint, GLenum, GLsizei, const GLvoid * );
	void ( * fp_glDepthRangex )( struct gles_context *, GLftype, GLftype );
	GLenum ( * fp_glDisableClientState )( struct gles_context *, GLenum, GLboolean );
#if EXTENSION_DRAW_TEX_OES_ENABLE
	GLenum ( * fp_glDrawTexfOES )( struct gles_context *, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat );
	GLenum ( * fp_glDrawTexfvOES )( struct gles_context *, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat );
	GLenum ( * fp_glDrawTexiOES )( struct gles_context *, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat );
	GLenum ( * fp_glDrawTexivOES )( struct gles_context *, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat );
	GLenum ( * fp_glDrawTexsOES )( struct gles_context *, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat );
	GLenum ( * fp_glDrawTexsvOES )( struct gles_context *, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat );
	GLenum ( * fp_glDrawTexxOES )( struct gles_context *, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat );
	GLenum ( * fp_glDrawTexxvOES )( struct gles_context *, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat );
#endif
	GLenum ( * fp_glEnableClientState )( struct gles_context *, GLenum, GLboolean );
	GLenum ( * fp_glFogf )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glFogfv )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glFogx )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glFogxv )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glFrustumf )( struct gles_context *, GLftype, GLftype, GLftype, GLftype, GLftype, GLftype );
	GLenum ( * fp_glFrustumx )( struct gles_context *, GLftype, GLftype, GLftype, GLftype, GLftype, GLftype );
	GLenum ( * fp_glGetClipPlanef )( struct gles_state *, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetClipPlanex )( struct gles_state *, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetFixedv )( struct gles_context *, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetLightfv )( struct gles_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetLightxv )( struct gles_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetMaterialfv )( struct gles_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetMaterialxv )( struct gles_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetPointerv )( struct gles_state *, GLenum, GLvoid ** );
	GLenum ( * fp_glGetTexEnvfv )( struct gles_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetTexEnviv )( struct gles_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetTexEnvxv )( struct gles_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glGetTexParameterxv )( struct gles_common_state *, GLenum, GLenum, GLvoid *, gles_datatype );
	GLenum ( * fp_glLightf )( struct gles_context *, GLenum, GLenum, const GLvoid *, const gles_datatype );
	GLenum ( * fp_glLightfv )( struct gles_context *, GLenum, const GLenum, const GLvoid *, const gles_datatype );
	GLenum ( * fp_glLightModelf )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glLightModelfv )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glLightModelx )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glLightModelxv )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glLightx )( struct gles_context *, GLenum, GLenum, const GLvoid *, const gles_datatype );
	GLenum ( * fp_glLightxv )( struct gles_context *, GLenum, const GLenum, const GLvoid *, const gles_datatype );
	GLenum ( * fp_glLineWidthx )( struct gles_context *, GLftype );
	void ( * fp_glLoadIdentity )( struct gles_context * );
	void ( * fp_glLoadMatrixf )( struct gles_context *, const GLfloat * );
	void ( * fp_glLoadMatrixx )( struct gles_context *, const GLfixed * );
	GLenum ( * fp_glLogicOp )( struct gles_context *, GLenum );
	GLenum ( * fp_glMaterialf )( struct gles_context *, GLenum, GLenum, const GLvoid *, const gles_datatype );
	GLenum ( * fp_glMaterialfv )( struct gles_context *, GLenum, GLenum, const GLvoid *, const gles_datatype );
	GLenum ( * fp_glMaterialx )( struct gles_context *, GLenum, GLenum, const GLvoid *, const gles_datatype );
	GLenum ( * fp_glMaterialxv )( struct gles_context *, GLenum, GLenum, const GLvoid *, const gles_datatype );
	GLenum ( * fp_glMatrixMode )( struct gles_state*, GLenum );
	GLenum ( * fp_glMultiTexCoord4b )( struct gles1_current *, GLenum, GLftype, GLftype, GLftype, GLftype );
	GLenum ( * fp_glMultiTexCoord4f )( struct gles1_current *, GLenum, GLftype, GLftype, GLftype, GLftype );
	GLenum ( * fp_glMultiTexCoord4x )( struct gles1_current *, GLenum, GLftype, GLftype, GLftype, GLftype );
	void ( * fp_glMultMatrixf )( struct gles_context *, const GLfloat * );
	void ( * fp_glMultMatrixx )( struct gles_context *, const GLfixed * );
	GLenum ( * fp_glNormal3f )( struct gles1_current *, GLftype, GLftype, GLftype );
	GLenum ( * fp_glNormal3x )( struct gles1_current *, GLftype, GLftype, GLftype );
	GLenum ( * fp_glNormalPointer )( struct gles_context*, GLenum, GLsizei, const GLvoid * );
	GLenum ( * fp_glOrthof )( struct gles_context *, GLftype, GLftype, GLftype, GLftype, GLftype, GLftype );
	GLenum ( * fp_glOrthox )( struct gles_context *, GLftype, GLftype, GLftype, GLftype, GLftype, GLftype );
	GLenum ( * fp_glPointParameterf )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glPointParameterfv )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glPointParameterx )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glPointParameterxv )( struct gles_context *, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glPointSize )( struct gles_rasterization *, GLftype );
	GLenum ( * fp_glPointSizex )( struct gles_rasterization *, GLftype );
	GLenum ( * fp_glPolygonOffsetx )( struct gles_context *, GLftype, GLftype );
	GLenum ( * fp_glPopMatrix )( struct gles_context * );
	GLenum ( * fp_glPushMatrix )( struct gles_context * );
	void ( * fp_glRotatef )( struct gles_context *, GLftype, GLftype, GLftype, GLftype );
	void ( * fp_glRotatex )( struct gles_context *, GLftype, GLftype, GLftype, GLftype );
	void ( * fp_glSampleCoveragex )( struct gles_context *, GLftype, GLboolean );
	void ( * fp_glScalef )( struct gles_context *, GLftype, GLftype, GLftype );
	void ( * fp_glScalex )( struct gles_context *, GLftype, GLftype, GLftype );
	GLenum ( * fp_glShadeModel )( struct gles_context *, GLenum );
	GLenum ( * fp_glTexCoordPointer )( struct gles_context *, GLint, GLenum, GLsizei, const GLvoid * );
	GLenum ( * fp_glTexEnvf )( struct gles_context *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexEnvfv )( struct gles_context *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexEnvi )( struct gles_context *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexEnviv )( struct gles_context *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexEnvx )( struct gles_context *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexEnvxv )( struct gles_context *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexParameterx )( struct gles_texture_environment *, GLenum, GLenum, const GLvoid *, gles_datatype );
	GLenum ( * fp_glTexParameterxv )( struct gles_texture_environment *, GLenum, GLenum, const GLvoid *, gles_datatype );
	void ( * fp_glTranslatef )( struct gles_context *, GLftype, GLftype, GLftype );
	void ( * fp_glTranslatex )( struct gles_context *, GLftype, GLftype, GLftype);
	GLenum ( * fp_glVertexPointer )( struct gles_context *, GLint, GLenum, GLsizei, const GLvoid * );

	/* OpenGL ES 1.x extensions */

#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
	GLenum ( * fp_glCurrentPaletteMatrixOES )( struct gles1_transform *, GLint );
	void ( * fp_glLoadPaletteFromModelViewMatrixOES )( struct gles_context *, struct gles1_transform *transform );
	GLenum ( * fp_glMatrixIndexPointerOES )( struct gles_context *, GLint, GLenum, GLsizei, const GLvoid * );
	GLenum ( * fp_glWeightPointerOES )( struct gles_context *, GLint, GLenum, GLsizei, const GLvoid * );
#endif

#if EXTENSION_POINT_SIZE_ARRAY_OES_ENABLE
	GLenum ( * fp_glPointSizePointerOES )( struct gles_context *, GLenum, GLsizei, const GLvoid * );
#endif

#if EXTENSION_QUERY_MATRIX_OES_ENABLE
	GLbitfield ( * fp_glQueryMatrixxOES )( struct gles_context *, GLfixed *, GLint * );
#endif

#if EXTENSION_TEXTURE_CUBE_MAP_ENABLE
	GLenum ( * fp_glTexGenfOES )( struct gles_context *, GLenum, GLenum, GLfloat );
	GLenum ( * fp_glTexGenfvOES )( struct gles_context *, GLenum, GLenum, const GLfloat * );
	GLenum ( * fp_glTexGeniOES )( struct gles_context *, GLenum, GLenum, GLint );
	GLenum ( * fp_glTexGenivOES )( struct gles_context *, GLenum, GLenum, const GLint * );
	GLenum ( * fp_glTexGenxOES )( struct gles_context *, GLenum, GLenum, GLfixed param );
	GLenum ( * fp_glTexGenxvOES )( struct gles_context *, GLenum, GLenum, const GLfixed * );
	GLenum ( * fp_glGetTexGenfvOES )( struct gles_context *, GLenum, GLenum, GLfloat * );
	GLenum ( * fp_glGetTexGenivOES )( struct gles_context *, GLenum, GLenum, GLint * );
	GLenum ( * fp_glGetTexGenxvOES )( struct gles_context *, GLenum, GLenum, GLfixed * );
#endif


#endif /* MALI_USE_GLES_1 */


#if MALI_USE_GLES_2

	/* OpenGL ES 2.0-specific APIs */
	GLenum ( * fp_glAttachShader )( mali_named_list *, GLuint, GLuint );
	GLenum ( * fp_glBindAttribLocation )( mali_named_list *, GLuint, GLuint, const char* );
	GLenum ( * fp_glBlendColor )( struct gles_context *, GLclampf, GLclampf, GLclampf, GLclampf );
	GLenum ( * fp_glBlendEquation )( struct gles_context *, GLenum, GLenum );
	GLenum ( * fp_glBlendEquationSeparate )( struct gles_context *, GLenum, GLenum );
	GLenum ( * fp_glBlendFuncSeparate )( struct gles_context *, GLenum, GLenum, GLenum, GLenum );
	GLenum ( * fp_glCompileShader )( mali_named_list *, GLuint );
	GLenum ( * fp_glCreateProgram )( mali_named_list *, GLuint * );
	GLenum ( * fp_glCreateShader )( mali_named_list *, GLenum, GLuint * );
	GLenum ( * fp_glDeleteProgram )( mali_named_list *, GLuint );
	GLenum ( * fp_glDeleteShader )( mali_named_list *, GLuint );
	GLenum ( * fp_glDetachShader )( mali_named_list *, struct gles2_program_environment *, GLuint, GLuint );
	GLenum ( * fp_glDisableVertexAttribArray )( struct gles_context*, GLuint );
	GLenum ( * fp_glEnableVertexAttribArray )( struct gles_context*, GLuint );
	GLenum ( * fp_glGetActiveAttrib )( mali_named_list *, GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, char * );
	GLenum ( * fp_glGetActiveUniform )( mali_named_list *, GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, char * );
	GLenum ( * fp_glGetAttachedShaders )( mali_named_list *, GLuint, GLsizei, GLsizei *, GLuint * );
	GLenum ( * fp_glGetAttribLocation )( mali_named_list *, GLuint, const char *, GLint * );
	GLenum ( * fp_glGetProgramInfoLog )( mali_named_list *, GLuint, GLsizei, GLsizei *, char * );
	GLenum ( * fp_glGetProgramiv )( mali_named_list *, GLuint, GLenum, GLint * );
	GLenum ( * fp_glGetShaderInfoLog )( mali_named_list *, GLuint, GLsizei, GLsizei *, char * );
	GLenum ( * fp_glGetShaderiv )( mali_named_list *, GLuint, GLenum, GLint * );
	GLenum ( * fp_glGetShaderPrecisionFormat )( GLenum, GLenum, GLint *, GLint * );
	GLenum ( * fp_glGetShaderSource )( mali_named_list *, GLuint, GLsizei, GLsizei *, char * );
	GLenum ( * fp_glGetUniformfv )( mali_named_list *, GLuint, GLint, void *, GLenum );
	GLenum ( * fp_glGetUniformiv )( mali_named_list *, GLuint, GLint, void *, GLenum );
	GLenum ( * fp_glGetUniformLocation )( mali_named_list *, GLuint, const char *, GLint * );
	GLenum ( * fp_glGetVertexAttribfv )( struct gles_vertex_array *, GLuint, GLenum, gles_datatype, void * );
	GLenum ( * fp_glGetVertexAttribiv )( struct gles_vertex_array *, GLuint, GLenum, gles_datatype, void * );
	GLenum ( * fp_glGetVertexAttribPointerv )( struct gles_vertex_array *, GLuint, GLenum, void ** );
	GLboolean ( * fp_glIsProgram )( mali_named_list *, GLuint );
	GLboolean ( * fp_glIsShader )( mali_named_list *, GLuint );
	GLenum ( * fp_glLinkProgram )( struct gles_context*, mali_named_list *, GLuint );
	GLenum ( * fp_glReleaseShaderCompiler )( void );
	GLenum ( * fp_glShaderBinary )( mali_named_list *, GLint, const GLuint *, GLenum, const void *, GLint );
	GLenum ( * fp_glShaderSource )( mali_named_list *, GLuint, GLsizei, const char **, const GLint * );
	GLenum ( * fp_glStencilFuncSeparate )( struct gles_context *, GLenum, GLenum, GLint, GLuint );
	GLenum ( * fp_glStencilMaskSeparate )( struct gles_context *, GLenum, GLuint );
	GLenum ( * fp_glStencilOpSeparate )( struct gles_context *, GLenum, GLenum, GLenum, GLenum );
	GLenum ( * fp_glUniform1f )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform1fv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform1i )( struct gles_context *, GLint, GLint );
	GLenum ( * fp_glUniform1iv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform2f )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform2fv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform2i )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform2iv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform3f )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform3fv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform3i )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform3iv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform4f )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform4fv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform4i )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniform4iv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniformMatrix2fv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniformMatrix3fv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUniformMatrix4fv )( struct gles_context *, gles_datatype, GLuint, GLuint, GLint, GLint, const void * );
	GLenum ( * fp_glUseProgram )( struct gles_state*, mali_named_list *, GLuint );
	GLenum ( * fp_glValidateProgram )( mali_named_list *, GLuint );
	GLenum ( * fp_glVertexAttrib1f )( struct gles_vertex_array *, GLuint, GLuint, const float * );
	GLenum ( * fp_glVertexAttrib1fv )( struct gles_vertex_array *, GLuint, GLuint, const float * );
	GLenum ( * fp_glVertexAttrib2f )( struct gles_vertex_array *, GLuint, GLuint, const float * );
	GLenum ( * fp_glVertexAttrib2fv )( struct gles_vertex_array *, GLuint, GLuint, const float * );
	GLenum ( * fp_glVertexAttrib3f )( struct gles_vertex_array *, GLuint, GLuint, const float * );
	GLenum ( * fp_glVertexAttrib3fv )( struct gles_vertex_array *, GLuint, GLuint, const float * );
	GLenum ( * fp_glVertexAttrib4f )( struct gles_vertex_array *, GLuint, GLuint, const float * );
	GLenum ( * fp_glVertexAttrib4fv )( struct gles_vertex_array *, GLuint, GLuint, const float * );
	GLenum ( * fp_glVertexAttribPointer )( struct gles_context *, GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid * );
#endif /* MALI_USE_GLES_2 */

        /* OpenGL ES 2.x extensions */
#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
	GLenum ( * fp_glGetProgramBinaryOES )( mali_named_list *, GLuint, GLsizei, GLsizei *, GLenum *, GLvoid * );
	GLenum ( * fp_glProgramBinaryOES )( struct gles_context *ctx, mali_named_list *, GLuint, GLenum, const GLvoid *, GLint );
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE && MALI_USE_GLES_2
	GLenum ( * fp_glGetnUniformfvEXT )( struct gles_context *, mali_named_list *, GLuint, GLint, GLsizei, void *, GLenum );
	GLenum ( * fp_glGetnUniformivEXT )( struct gles_context *, mali_named_list *, GLuint, GLint, GLsizei, void *, GLenum );
#endif

	/* Function pointer for the set_error internal functions, which are implemented differently in GLES 1.1 and 2.0 */
	void ( * fp_set_error )( struct gles_context *ctx, GLenum errorcode );

} gles_vtable;

#endif /* _GLES_VTABLE_H_ */
