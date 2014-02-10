/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * ( C ) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_base.h"
#include "gles_context.h"
#include "gles_share_lists.h"
#include "gles_util.h"
#include "gles_flush.h"
#include "gles_clear.h"
#include "gles2_draw.h"
#include "gles_object.h"
#include "gles_read_pixels.h"
#include "gles_buffer_object.h"
#include "gles_mipmap.h"
#include "gles_framebuffer_object.h"
#include "gles2_texture_object.h"

#include "gles_common_state/gles_rasterization.h"
#include "gles_common_state/gles_framebuffer_control.h"
#include "gles_common_state/gles_viewport.h"
#include "gles2_state/gles2_get.h"
#include "gles2_state/gles2_enable.h"
#include "gles2_state/gles2_tex_state.h"
#include "gles2_state/gles2_pixel.h"
#include "gles2_state/gles2_pixel_operations.h"
#include "gles2_state/gles2_rasterization.h"
#include "gles2_state/gles2_viewport.h"
#include "gles2_state/gles2_error.h"
#include "gles2_state/gles2_hint.h"
#include "gles2_state/gles2_program_object.h"
#include "gles2_state/gles2_shader_object.h"
#include "gles2_state/gles2_vertex_array.h"

#include "gles_vtable.h"

#if MALI_USE_GLES_1
	#include "gles1_stubs.h"
#endif

static const gles_vtable gles20_vtable =
{
	/* Common OpenGL ES 1.1 and 2.0 APIs */
	_gles_active_texture,                   /* glActiveTexture */
	_gles_bind_buffer,                      /* glBindBuffer */
	_gles_bind_texture,                     /* glBindTexture */
	_gles2_blend_func,                      /* glBlendFunc */
	_gles_buffer_data,                      /* glBufferData */
	_gles_buffer_sub_data,                  /* glBufferSubData */
	_gles_clear,                            /* glClear */
	_gles_clear_color,                      /* glClearColor */
	_gles_clear_depth,                      /* glClearDepthf */
	_gles_clear_stencil,                    /* glClearStencil */
	_gles_color_mask,                       /* glColorMask */
	_gles2_compressed_texture_image_2d,     /* glCompressedTexImage2D */
	_gles2_compressed_texture_sub_image_2d, /* glCompressedTexSubImage2D */
	_gles2_copy_texture_image_2d,           /* glCopyTexImage2D */
	_gles2_copy_texture_sub_image_2d,       /* glCopyTexSubImage2D */
	_gles2_cull_face,                       /* glCullFace */
	_gles_delete_buffers,                   /* glDeleteBuffers */
	_gles2_delete_textures,                 /* glDeleteTextures */
	_gles_depth_func,                       /* glDepthFunc */
	_gles_depth_mask,                       /* glDepthMask */
	_gles_depth_range,                      /* glDepthRangef */
	_gles2_enable,                          /* glDisable */
	_gles2_draw_arrays,                     /* glDrawArrays */
	_gles2_draw_elements,                   /* glDrawElements */
	_gles2_enable,                          /* glEnable */
	_gles_finish,                           /* glFinish */
	_gles_flush,                            /* glFlush */
	_gles2_front_face,                      /* glFrontFace */
	_gles_gen_objects,                      /* glGenBuffers */
	_gles_gen_objects,                      /* glGenTextures */
	_gles2_getv,                            /* glGetBooleanv */
	_gles2_get_buffer_parameter,            /* glGetBufferParameteriv */
	_gles2_get_error,                       /* glGetError */
	_gles2_getv,                            /* glGetFloatv */
	_gles2_getv,                            /* glGetIntegerv */
	_gles2_get_string,                      /* glGetString */
	_gles2_get_tex_param,                   /* glGetTexParameterfv */
	_gles2_get_tex_param,                   /* glGetTexParameteriv */
	_gles2_hint,                            /* glHint */
	_gles_is_object,                        /* glIsBuffer */
	_gles2_is_enabled,                      /* glIsEnabled */
	_gles_is_object,                        /* glIsTexture */
	_gles2_line_width,                      /* glLineWidth */
	_gles2_pixel_storei,                    /* glPixelStorei */
	_gles_polygon_offset,                   /* glPolygonOffset */
	_gles_read_pixels,                      /* glReadPixels */
	_gles_sample_coverage,                  /* glSampleCoverage */
	_gles_scissor,                          /* glScissor */
	_gles_stencil_func,                     /* glStencilFunc */
	_gles_stencil_mask,                     /* glStencilMask */
	_gles2_stencil_op,                      /* glStencilOp */
	_gles2_tex_image_2d,                    /* glTexImage2D */
	_gles2_tex_parameter,                   /* glTexParameterf */
	_gles2_tex_parameter,                   /* glTexParameterfv */
	_gles2_tex_parameter,                   /* glTexParameteri */
	_gles2_tex_parameter,                   /* glTexParameteriv */
	_gles2_tex_sub_image_2d,                /* glTexSubImage2D */
	_gles2_viewport,                        /* glViewport */

	/* Framebuffer objects are a part of core in OpenGL ES 2.0 so they
	   will always be included */
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	_gles_is_object,                             /* glIsRenderbuffer */
	_gles_bind_renderbuffer,                     /* glBindRenderbuffer */
	_gles_delete_renderbuffers,                  /* glDeleteRenderbuffers */
	_gles_gen_objects,                           /* glGenRenderbuffers */
	_gles_renderbuffer_storage,                  /* glRenderbufferStorage */
	_gles_get_renderbuffer_parameter,            /* glGetRenderbufferParameteriv */
	_gles_is_object,                             /* glIsFramebuffer */
	_gles_bind_framebuffer,                      /* glBindFramebuffer */
	_gles_delete_framebuffers,                   /* glDeleteFramebuffers */
	_gles_gen_objects,                           /* glGenFramebuffers */
	_gles_check_framebuffer_status,              /* glCheckFramebufferStatus */
	_gles_framebuffer_texture2d,                 /* glFramebufferTexture2D */
	_gles_framebuffer_renderbuffer,              /* glFramebufferRenderbuffer */
	_gles_get_framebuffer_attachment_parameter,  /* glGetFramebufferAttachmentParameteriv */
	_gles_generate_mipmap,                       /* glGenerateMipmap */
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	_gles_renderbuffer_storage_multisample,      /* glRenderbufferStorageMultisampleEXT */
	_gles_framebuffer_texture2d_multisample,     /* glFramebufferTexture2DMultisampleEXT */
#endif

#if EXTENSION_DISCARD_FRAMEBUFFER
	_gles_discard_framebuffer,					/* glDiscardFramebufferEXT */
#endif
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
	_gles_get_graphics_reset_status_ext,       /* glGetGraphicsResetStatusEXT */
	_gles_read_n_pixels_ext,                   /* glReadnPixelsEXT */
#endif

#if EXTENSION_EGL_IMAGE_OES_ENABLE
	_gles2_egl_image_target_texture_2d,           /* glEGLImageTargetTexture2DOES */
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	_gles_egl_image_target_renderbuffer_storage, /* glEGLImageTargetRenderbufferStorageOES */
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */
#endif /* EXTENSION_EGL_IMAGE_OES_ENABLE */

#if MALI_USE_GLES_1

	/* OpenGL ES 1.x-specific APIs. For OpenGL ES 2.x context these API functions will
	   be routed to the stub functions listed below. These functions will report an
	   instrumented error, but will otherwise return without any OpenGL ES errors. They
	   will not have any side effects such as state setting, etc. This avoids weird
	   behavior if the user by some mistake calls OpenGL ES 1.x functions while having
	   a 2.x context bound. */

	_gles1_alpha_func_stub,                 /* glAlphaFunc */
	_gles1_alpha_funcx_stub,                /* glAlphaFuncx */
	_gles1_clear_colorx_stub,               /* glClearColorx */
	_gles1_clear_depthx_stub,               /* glClearDepthx */
	_gles1_client_active_texture_stub,      /* glClientActiveTexture */
	_gles1_clip_planef_stub,                /* glClipPlanef */
	_gles1_clip_planex_stub,                /* glClipPlanex */
	_gles1_color4f_stub,                    /* glColor4f */
	_gles1_color4ub_stub,                   /* glColor4ub */
	_gles1_color4x_stub,                    /* glColor4x */
	_gles1_color_pointer_stub,              /* glColorPointer */
	_gles1_depth_rangex_stub,               /* glDepthRangex */
	_gles1_disable_client_state_stub,       /* glDisableClientState */
#if EXTENSION_DRAW_TEX_OES_ENABLE
	_gles1_draw_tex_oes_stub,               /* glDrawTexfOES */
	_gles1_draw_tex_oes_stub,               /* glDrawTexfvOES */
	_gles1_draw_tex_oes_stub,               /* glDrawTexiOES */
	_gles1_draw_tex_oes_stub,               /* glDrawTexivOES */
	_gles1_draw_tex_oes_stub,               /* glDrawTexsOES */
	_gles1_draw_tex_oes_stub,               /* glDrawTexsvOES */
	_gles1_draw_tex_oes_stub,               /* glDrawTexxOES */
	_gles1_draw_tex_oes_stub,               /* glDrawTexxvOES */
#endif
	_gles1_enable_client_state_stub,        /* glEnableClientState */
	_gles1_fogf_stub,                       /* glFogf */
	_gles1_fogfv_stub,                      /* glFogfv */
	_gles1_fogx_stub,                       /* glFogx */
	_gles1_fogxv_stub,                      /* glFogxv */
	_gles1_frustumf_stub,                   /* glFrustumf */
	_gles1_frustumx_stub,                   /* glFrustumx */
	_gles1_get_clip_planef_stub,            /* glGetClipPlanef */
	_gles1_get_clip_planex_stub,            /* glGetClipPlanex */
	_gles1_get_fixedv_stub,                 /* glGetFixedv */
	_gles1_get_lightfv_stub,                /* glGetLightfv */
	_gles1_get_lightxv_stub,                /* glGetLightxv */
	_gles1_get_materialfv_stub,             /* glGetMaterialfv */
	_gles1_get_materialxv_stub,             /* glGetMaterialxv */
	_gles1_get_pointerv_stub,               /* glGetPointerv */
	_gles1_get_tex_envfv_stub,              /* glGetTexEnvfv */
	_gles1_get_tex_enviv_stub,              /* glGetTexEnviv */
	_gles1_get_tex_envxv_stub,              /* glGetTexEnvxv */
	_gles1_get_tex_parameterxv_stub,        /* glGetTexParameterxv */
	_gles1_lightf_stub,                     /* glLightf */
	_gles1_lightfv_stub,                    /* glLightfv */
	_gles1_light_modelf_stub,               /* glLightModelf */
	_gles1_light_modelfv_stub,              /* glLightModelfv */
	_gles1_light_modelx_stub,               /* glLightModelx */
	_gles1_light_modelxv_stub,              /* glLightModelxv */
	_gles1_lightx_stub,                     /* glLightx */
	_gles1_lightxv_stub,                    /* glLightxv */
	_gles1_line_widthx_stub,                /* glLineWidthx */
	_gles1_load_identity_stub,              /* glLoadIdentity */
	_gles1_load_matrixf_stub,               /* glLoadMatrixf */
	_gles1_load_matrixx_stub,               /* glLoadMatrixx */
	_gles1_logic_op_stub,                   /* glLogicOp */
	_gles1_materialf_stub,                  /* glMaterialf */
	_gles1_materialfv_stub,                 /* glMaterialfv */
	_gles1_materialx_stub,                  /* glMaterialx */
	_gles1_materialxv_stub,                 /* glMaterialxv */
	_gles1_matrix_mode_stub,                /* glMatrixMode */
	_gles1_multi_tex_coord4b_stub,          /* glMultiTexCoord4b */
	_gles1_multi_tex_coord4f_stub,          /* glMultiTexCoord4f */
	_gles1_multi_tex_coord4x_stub,          /* glMultiTexCoord4x */
	_gles1_mult_matrixf_stub,               /* glMultMatrixf */
	_gles1_mult_matrixx_stub,               /* glMultMatrixx */
	_gles1_normal3f_stub,                   /* glNormal3f */
	_gles1_normal3x_stub,                   /* glNormal3x */
	_gles1_normal_pointer_stub,             /* glNormalPointer */
	_gles1_orthof_stub,                     /* glOrthof */
	_gles1_orthox_stub,                     /* glOrthox */
	_gles1_point_parameterf_stub,           /* glPointParameterf */
	_gles1_point_parameterfv_stub,          /* glPointParameterfv */
	_gles1_point_parameterx_stub,           /* glPointParameterx */
	_gles1_point_parameterxv_stub,          /* glPointParameterxv */
	_gles1_point_size_stub,                 /* glPointSize */
	_gles1_point_sizex_stub,                /* glPointSizex */
	_gles1_polygon_offsetx_stub,            /* glPolygonOffsetx */
	_gles1_pop_matrix_stub,                 /* glPopMatrix */
	_gles1_push_matrix_stub,                /* glPushMatrix */
	_gles1_rotatef_stub,                    /* glRotatef */
	_gles1_rotatex_stub,                    /* glRotatex */
	_gles1_sample_coveragex_stub,           /* glSampleCoveragex */
	_gles1_scalef_stub,                     /* glScalef */
	_gles1_scalex_stub,                     /* glScalex */
	_gles1_shade_model_stub,                /* glShadeModel */
	_gles1_tex_coord_pointer_stub,          /* glTexCoordPointer */
	_gles1_tex_envf_stub,                   /* glTexEnvf */
	_gles1_tex_envfv_stub,                  /* glTexEnvfv */
	_gles1_tex_envi_stub,                   /* glTexEnvi */
	_gles1_tex_enviv_stub,                  /* glTexEnviv */
	_gles1_tex_envx_stub,                   /* glTexEnvx */
	_gles1_tex_envxv_stub,                  /* glTexEnvxv */
	_gles1_tex_parameterx_stub,             /* glTexParameterx */
	_gles1_tex_parameterxv_stub,            /* glTexParameterxv */
	_gles1_translatef_stub,                 /* glTranslatef */
	_gles1_translatex_stub,                 /* glTranslatex */
	_gles1_vertex_pointer_stub,             /* glVertexPointer */

	/* OpenGL ES 1.x extensions */

#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
	_gles1_current_palette_matrix_oes_stub,              /* glCurrentPaletteMatrixOES */
	_gles1_load_palette_from_model_view_matrix_oes_stub, /* glLoadPaletteFromModelViewMatrixOES */
	_gles1_matrix_index_pointer_oes_stub,                /* glMatrixIndexPointerOES */
	_gles1_weight_pointer_oes_stub,                      /* glWeightPointerOES */
#endif

#if EXTENSION_POINT_SIZE_ARRAY_OES_ENABLE
	_gles1_point_size_pointer_oes_stub,                  /* glPointSizePointerOES */
#endif

#if EXTENSION_QUERY_MATRIX_OES_ENABLE
	_gles1_query_matrixx_oes_stub,                       /* glQueryMatrixxOES */
#endif

#if EXTENSION_TEXTURE_CUBE_MAP_ENABLE
	_gles1_tex_genf_oes_stub,                            /* glTexGenfOES */
	_gles1_tex_genfv_oes_stub,                           /* glTexGenfvOES */
	_gles1_tex_geni_oes_stub,                            /* glTexGeniOES */
	_gles1_tex_geniv_oes_stub,                           /* glTexGenivOES */
	_gles1_tex_genx_oes_stub,                            /* glTexGenxOES */
	_gles1_tex_genxv_oes_stub,                           /* glTexGenxvOES */
	_gles1_get_tex_genfv_oes_stub,                       /* glGetTexGenfvOES */
	_gles1_get_tex_geniv_oes_stub,                       /* glGetTexGenivOES */
	_gles1_get_tex_genxv_oes_stub,                       /* glGetTexGenxvOES */
#endif


#endif /* MALI_USE_GLES_1 */

	/* OpenGL ES 2.0-specific APIs */
	_gles2_attach_shader,                   /* glAttachShader */
	_gles2_bind_attrib_location,            /* glBindAttribLocation */
	_gles2_blend_color,                     /* glBlendColor */
	_gles2_blend_equation,                  /* glBlendEquation */
	_gles2_blend_equation,                  /* glBlendEquationSeparate */
	_gles2_blend_func,                      /* glBlendFuncSeparate */
	_gles2_compile_shader,                  /* glCompileShader */
	_gles2_create_program,                  /* glCreateProgram */
	_gles2_create_shader,                   /* glCreateShader */
	_gles2_delete_program,                  /* glDeleteProgram */
	_gles2_delete_shader,                   /* glDeleteShader*/
	_gles2_detach_shader,                   /* glDetachShader */
	_gles2_disable_vertex_attrib_array,     /* glDisableVertexAttribArray */
	_gles2_enable_vertex_attrib_array,      /* glEnableVertexAttribArray */
	_gles2_get_active_attrib,               /* glGetActiveAttrib */
	_gles2_get_active_uniform,              /* glGetActiveUniform */
	_gles2_get_attached_shaders,            /* glGetAttachedShaders */
	_gles2_get_attrib_location,             /* glGetAttribLocation */
	_gles2_get_program_info_log,            /* glGetProgramInfoLog */
	_gles2_get_programiv,                   /* glGetProgramiv */
	_gles2_get_shader_info_log,             /* glGetShaderInfoLog */
	_gles2_get_shader,                      /* glGetShaderiv */
	_gles2_get_shader_precision_format,     /* glGetShaderPrecisionFormat */
	_gles2_get_shader_source,               /* glGetShaderSource */
	_gles2_get_uniform,                     /* glGetUniformfv */
	_gles2_get_uniform,                     /* glGetUniformiv */
	_gles2_get_uniform_location,            /* glGetUniformLocation */
	_gles2_get_vertex_attrib,               /* glGetVertexAttribfv */
	_gles2_get_vertex_attrib,               /* glGetVertexAttribiv */
	_gles2_get_vertex_attrib_pointer,       /* glGetVertexAttribPointerv */
	_gles2_is_program,                      /* glIsProgram */
	_gles2_is_shader,                       /* glIsShader */
	_gles2_link_program,                    /* glLinkProgram */
	_gles2_release_shader_compiler,         /* glReleaseShaderCompiler */
	_gles2_shader_binary,                   /* glShaderBinary */
	_gles2_shader_source,                   /* glShaderSource */
	_gles_stencil_func,                     /* glStencilFuncSeparate */
	_gles_stencil_mask,                     /* glStencilMaskSeparate */
	_gles2_stencil_op,                      /* glStencilOpSeparate */
	_gles2_uniform,                         /* glUniform1f */
	_gles2_uniform,                         /* glUniform1fv */
	_gles2_uniform1i,                       /* glUniform1i */
	_gles2_uniform,                         /* glUniform1iv */
	_gles2_uniform,                         /* glUniform2f */
	_gles2_uniform,                         /* glUniform2fv */
	_gles2_uniform,                         /* glUniform2i */
	_gles2_uniform,                         /* glUniform2iv */
	_gles2_uniform,                         /* glUniform3f */
	_gles2_uniform,                         /* glUniform3fv */
	_gles2_uniform,                         /* glUniform3i */
	_gles2_uniform,                         /* glUniform3iv */
	_gles2_uniform,                         /* glUniform4f */
	_gles2_uniform,                         /* glUniform4fv */
	_gles2_uniform,                         /* glUniform4i */
	_gles2_uniform,                         /* glUniform4iv */
	_gles2_uniform,                         /* glUniformMatrix2fv */
	_gles2_uniform,                         /* glUniformMatrix3fv */
	_gles2_uniform,                         /* glUniformMatrix4fv */
	_gles2_use_program,                     /* glUseProgram */
	_gles2_validate_program,                /* glValidateProgram */
	_gles2_vertex_attrib,                   /* glVertexAttrib1f */
	_gles2_vertex_attrib,                   /* glVertexAttrib1fv */
	_gles2_vertex_attrib,                   /* glVertexAttrib2f */
	_gles2_vertex_attrib,                   /* glVertexAttrib2fv */
	_gles2_vertex_attrib,                   /* glVertexAttrib3f */
	_gles2_vertex_attrib,                   /* glVertexAttrib3fv */
	_gles2_vertex_attrib,                   /* glVertexAttrib4f */
	_gles2_vertex_attrib,                   /* glVertexAttrib4fv */
	_gles2_vertex_attrib_pointer,           /* glVertexAttribPointer */

        /* OpenGL ES 2.x extensions */
#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
	_gles2_get_program_binary,              /* glGetProgramBinaryOES */
	_gles2_program_binary,                  /* glProgramBinaryOES */
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
	_gles2_get_n_uniform_ext,                  /* glGetnUniformfvEXT */
	_gles2_get_n_uniform_ext,                  /* glGetnUniformivEXT */
#endif

	_gles2_set_error                        /* Internal function */
};

const struct gles_vtable *_gles2_get_vtable( void )
{
	return &gles20_vtable;
}
