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
#include "gles_util.h"
#include "gles1_matrix.h"
#include "gles_read_pixels.h"
#include "gles_state.h"
#include "gles1_texture_object.h"
#include "gles_texture_object.h"
#include "gles_buffer_object.h"
#include "gles_clear.h"
#include "gles_flush.h"
#include "gles1_draw.h"
#include "gles_config_extension.h"
#include "gles_framebuffer_object.h"
#include "mali_instrumented_context.h"
#include "gles_context.h"
#include "gles_share_lists.h"
#include "gles_object.h"
#include "gles_mipmap.h"

#include "gles_common_state/gles_multisampling.h"
#include "gles_common_state/gles_rasterization.h"
#include "gles_common_state/gles_framebuffer_control.h"
#include "gles_common_state/gles_viewport.h"
#include "gles1_state/gles1_lighting.h"
#include "gles1_state/gles1_coloring.h"
#include "gles1_state/gles1_error.h"
#include "gles1_state/gles1_enable.h"
#include "gles1_state/gles1_get.h"
#include "gles1_state/gles1_hint.h"
#include "gles1_state/gles1_rasterization.h"
#include "gles1_state/gles1_pixel_operations.h"
#include "gles1_state/gles1_vertex_array.h"
#include "gles1_state/gles1_tex_state.h"
#include "gles1_state/gles1_pixel.h"
#include "gles1_state/gles1_transform.h"
#include "gles1_state/gles1_viewport.h"
#include "gles1_state/gles1_current.h"
#include "gles_vtable.h"

#if MALI_USE_GLES_2
	#include "gles2_stubs.h"
#endif

static const gles_vtable gles11_vtable=
{
	/* Common OpenGL ES 1.1 and 2.0 APIs */
	_gles1_active_texture,                  /* glActiveTexture */
	_gles_bind_buffer,                      /* glBindBuffer */
	_gles_bind_texture,                     /* glBindTexture */
	_gles1_blend_func,                      /* glBlendFunc */
	_gles_buffer_data,                      /* glBufferData */
	_gles_buffer_sub_data,                  /* glBufferSubData */
	_gles_clear,                            /* glClear */
	_gles_clear_color,                      /* glClearColor */
	_gles_clear_depth,                      /* glClearDepthf */
	_gles_clear_stencil,                    /* glClearStencil */
	_gles_color_mask,                       /* glColorMask */
	_gles1_compressed_texture_image_2d,     /* glCompressedTexImage2D */
	_gles1_compressed_texture_sub_image_2d, /* glCompressedTexSubImage2D */
	_gles1_copy_texture_image_2d,           /* glCopyTexImage2D */
	_gles1_copy_texture_sub_image_2d,       /* glCopyTexSubImage2D */
	_gles1_cull_face,                       /* glCullFace */
	_gles_delete_buffers,                   /* glDeleteBuffers */
	_gles1_delete_textures,                 /* glDeleteTextures */
	_gles_depth_func,                       /* glDepthFunc */
	_gles_depth_mask,                       /* glDepthMask */
	_gles_depth_range,                      /* glDepthRangef */
	_gles1_enable,                          /* glDisable */
	_gles1_draw_arrays,                     /* glDrawArrays */
	_gles1_draw_elements,                   /* glDrawElements */
	_gles1_enable,                          /* glEnable */
	_gles_finish,                           /* glFinish */
	_gles_flush,                            /* glFlush */
	_gles1_front_face,                      /* glFrontFace */
	_gles_gen_objects,                      /* glGenBuffers */
	_gles_gen_objects,                      /* glGenTextures */
	_gles1_getv,                            /* glGetBooleanv */
	_gles1_get_buffer_parameter,            /* glGetBufferParameteriv */
	_gles1_get_error,                       /* glGetError */
	_gles1_getv,                            /* glGetFloatv */
	_gles1_getv,                            /* glGetIntegerv */
	_gles1_get_string,                      /* glGetString */
	_gles1_get_tex_param,                   /* glGetTexParameterfv */
	_gles1_get_tex_param,                   /* glGetTexParameteriv */
	_gles1_hint,                            /* glHint */
	_gles_is_object,                        /* glIsBuffer */
	_gles1_is_enabled,                      /* glIsEnabled */
	_gles_is_object,                        /* glIsTexture */
	_gles1_line_width,                      /* glLineWidth */
	_gles1_pixel_storei,                    /* glPixelStorei */
	_gles_polygon_offset,                   /* glPolygonOffset */
	_gles_read_pixels,                      /* glReadPixels */
	_gles_sample_coverage,                  /* glSampleCoverage */
	_gles_scissor,                          /* glScissor */
	_gles_stencil_func,                     /* glStencilFunc */
	_gles_stencil_mask,                     /* glStencilMask */
	_gles1_stencil_op,                      /* glStencilOp */
	_gles1_tex_image_2d,                    /* glTexImage2D */
	_gles1_tex_parameter,                   /* glTexParameterf */
	_gles1_tex_parameter_v,                 /* glTexParameterfv */
	_gles1_tex_parameter,                   /* glTexParameteri */
	_gles1_tex_parameter_v,                 /* glTexParameteriv */
	_gles1_tex_sub_image_2d,                /* glTexSubImage2D */
	_gles1_viewport,                        /* glViewport */

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
	_gles_get_graphics_reset_status_ext,            /* glGetGraphicsResetStatusEXT */
	_gles_read_n_pixels_ext,                        /* glReadnPixelsEXT */
#endif

#if EXTENSION_EGL_IMAGE_OES_ENABLE
	_gles1_egl_image_target_texture_2d,                    /* glEGLImageTargetTexture2DOES */
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	_gles_egl_image_target_renderbuffer_storage,  /* glEGLImageTargetRenderbufferStorageOES */
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */
#endif /* EXTENSION_EGL_IMAGE_OES_ENABLE */


	/* OpenGL ES 1.1-specific APIs */
	_gles1_alpha_func,                        /* glAlphaFunc */
	_gles1_alpha_func,                        /* glAlphaFunc */
	_gles_clear_color,                        /* glClearColorx */
	_gles_clear_depth,                        /* glClearDepthx */
	_gles1_client_active_texture,             /* glClientActiveTexture */
	_gles1_clip_plane,                        /* glClipPlanef */
	_gles1_clip_plane,                        /* glClipPlanex */
	_gles1_color4,                            /* glColor4f */
	_gles1_color4,                            /* glColor4ub */
	_gles1_color4,                            /* glColor4x */
	_gles1_color_pointer,                     /* glColorPointer */
	_gles_depth_range,                        /* glDepthRangex */
	_gles1_client_state,                      /* glDisableClientState */
#if EXTENSION_DRAW_TEX_OES_ENABLE
	_gles1_draw_tex_oes,                      /* glDrawTexfOES */
	_gles1_draw_tex_oes,                      /* glDrawTexfvOES */
	_gles1_draw_tex_oes,                      /* glDrawTexiOES */
	_gles1_draw_tex_oes,                      /* glDrawTexivOES */
	_gles1_draw_tex_oes,                      /* glDrawTexsOES */
	_gles1_draw_tex_oes,                      /* glDrawTexsvOES */
	_gles1_draw_tex_oes,                      /* glDrawTexxOES */
	_gles1_draw_tex_oes,                      /* glDrawTexxvOES */
#endif
	_gles1_client_state,                      /* glEnableClientState */
	_gles1_fog,                               /* glFogf */
	_gles1_fogv,                              /* glFogfv */
	_gles1_fog,                               /* glFogx */
	_gles1_fogv,                              /* glFogxv */
	_gles1_frustum,                           /* glFrustumf */
	_gles1_frustum,                           /* glFrustumx */
	_gles1_get_clip_plane,                    /* glGetClipPlanef */
	_gles1_get_clip_plane,                    /* glGetClipPlanex */
	_gles1_getv,                              /* glGetFixedv */
	_gles1_get_light,                         /* glGetLightfv */
	_gles1_get_light,                         /* glGetLightxv */
	_gles1_get_material,                      /* glGetMaterialfv */
	_gles1_get_material,                      /* glGetMaterialxv */
	_gles1_get_pointer,                       /* glGetPointerv */
	_gles1_get_tex_env,                       /* glGetTexEnvfv */
	_gles1_get_tex_env,                       /* glGetTexEnviv */
	_gles1_get_tex_env,                       /* glGetTexEnvxv */
	_gles1_get_tex_param,                     /* glGetTexParameterxv */
	_gles1_light,                             /* glLightf */
	_gles1_lightv,                            /* glLightfv */
	_gles1_light_model,                       /* glLightModelf */
	_gles1_light_modelv,                      /* glLightModelfv */
	_gles1_light_model,                       /* glLightModelx */
	_gles1_light_modelv,                      /* glLightModelxv */
	_gles1_light,                             /* glLightx */
	_gles1_lightv,                            /* glLightxv */
	_gles1_line_width,                        /* glLineWidthx */
	_gles1_load_identity,                     /* glLoadIdentity */
	_gles1_load_matrixf,                      /* glLoadMatrixf */
	_gles1_load_matrixx,                      /* glLoadMatrixx */
	_gles1_logic_op,                          /* glLogicOp */
	_gles1_material,                          /* glMaterialf */
	_gles1_materialv,                         /* glMaterialfv */
	_gles1_material,                          /* glMaterialx */
	_gles1_materialv,                         /* glMaterialxv */
	_gles1_matrix_mode,                       /* glMatrixMode */
	_gles1_multi_tex_coord4,                  /* glMultiTexCoord4b */
	_gles1_multi_tex_coord4,                  /* glMultiTexCoord4f */
	_gles1_multi_tex_coord4,                  /* glMultiTexCoord4x */
	_gles1_mult_matrixf,                      /* glMultMatrixf */
	_gles1_mult_matrixx,                      /* glMultMatrixx */
	_gles1_normal3,                           /* glNormal3f */
	_gles1_normal3,                           /* glNormal3x */
	_gles1_normal_pointer,                    /* glNormalPointer */
	_gles1_ortho,                             /* glOrthof */
	_gles1_ortho,                             /* glOrthox */
	_gles1_point_parameter,                   /* glPointParameterf */
	_gles1_point_parameterv,                  /* glPointParameterfv */
	_gles1_point_parameter,                   /* glPointParameterx */
	_gles1_point_parameterv,                  /* glPointParameterxv */
	_gles1_point_size,                        /* glPointSize */
	_gles1_point_size,                        /* glPointSizex */
	_gles_polygon_offset,                     /* glPolygonOffsetx */
	_gles1_pop_matrix,                        /* glPopMatrix */
	_gles1_push_matrix,                       /* glPushMatrix */
	_gles1_rotate,                            /* glRotatef */
	_gles1_rotate,                            /* glRotatex */
	_gles_sample_coverage,                    /* glSampleCoveragex */
	_gles1_scale,                             /* glScalef */
	_gles1_scale,                             /* glScalex */
	_gles1_shade_model,                       /* glShadeModel */
	_gles1_tex_coord_pointer,                 /* glTexCoordPointer */
	_gles1_tex_env,                           /* glTexEnvf */
	_gles1_tex_envv,                          /* glTexEnvfv */
	_gles1_tex_env,                           /* glTexEnvi */
	_gles1_tex_envv,                          /* glTexEnviv */
	_gles1_tex_env,                           /* glTexEnvx */
	_gles1_tex_envv,                          /* glTexEnvxv */
	_gles1_tex_parameter,                     /* glTexParameterx */
	_gles1_tex_parameter_v,                   /* glTexParameterxv */
	_gles1_translate,                         /* glTranslatef */
	_gles1_translate,                         /* glTranslatex */
	_gles1_vertex_pointer,                    /* glVertexPointer */

	/* OpenGL ES 1.x extensions */

#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
	_gles1_set_current_palette_matrix_oes,          /* glCurrentPaletteMatrixOES */
	_gles1_load_palette_from_model_view_matrix_oes, /* glLoadPaletteFromModelViewMatrixOES */
	_gles1_matrix_index_pointer_oes,                /* glMatrixIndexPointerOES */
	_gles1_weight_pointer_oes,                      /* glWeightPointerOES */
#endif

#if EXTENSION_POINT_SIZE_ARRAY_OES_ENABLE
	_gles1_point_size_pointer,                      /* glPointSizePointerOES */
#endif

#if EXTENSION_QUERY_MATRIX_OES_ENABLE
	_gles1_query_matrixx,                           /* glQueryMatrixxOES */
#endif

#if EXTENSION_TEXTURE_CUBE_MAP_ENABLE
	_gles1_tex_genf_oes,                            /* glTexGenfOES */
	_gles1_tex_genfv_oes,                           /* glTexGenfvOES */
	_gles1_tex_geni_oes,                            /* glTexGeniOES */
	_gles1_tex_geniv_oes,                           /* glTexGenivOES */
	_gles1_tex_genx_oes,                            /* glTexGenxOES */
	_gles1_tex_genxv_oes,                           /* glTexGenxvOES */
	_gles1_get_tex_genfv_oes,                       /* glGetTexGenfvOES */
	_gles1_get_tex_geniv_oes,                       /* glGetTexGenivOES */
	_gles1_get_tex_genxv_oes,                       /* glGetTexGenxvOES */
#endif


#if MALI_USE_GLES_2

	/* OpenGL ES 2.x-specific APIs. For OpenGL ES 1.x context these API functions will
	   be routed to the stub functions listed below. These functions will report an
	   instrumented error, but will otherwise return without any OpenGL ES errors. They
	   will not have any side effects such as state setting, etc. This avoids weird
	   behavior if the user by some mistake calls OpenGL ES 2.x functions while having
	   a 1.x context bound. */

	_gles2_attach_shader_stub,                /* glAttachShader */
	_gles2_bind_attrib_location_stub,         /* glBindAttribLocation */
	_gles2_blend_color_stub,                  /* glBlendColor */
	_gles2_blend_equation_stub,               /* glBlendEquation */
	_gles2_blend_equation_separate_stub,      /* glBlendEquationSeparate */
	_gles2_blend_func_separate_stub,          /* glBlendFuncSeparate */
	_gles2_compile_shader_stub,               /* glCompileShader */
	_gles2_create_program_stub,               /* glCreateProgram */
	_gles2_create_shader_stub,                /* glCreateShader */
	_gles2_delete_program_stub,               /* glDeleteProgram */
	_gles2_delete_shader_stub,                /* glDeleteShader*/
	_gles2_detach_shader_stub,                /* glDetachShader */
	_gles2_disable_vertex_attrib_array_stub,  /* glDisableVertexAttribArray */
	_gles2_enable_vertex_attrib_array_stub,   /* glEnableVertexAttribArray */
	_gles2_get_active_attrib_stub,            /* glGetActiveAttrib */
	_gles2_get_active_uniform_stub,           /* glGetActiveUniform */
	_gles2_get_attached_shaders_stub,         /* glGetAttachedShaders */
	_gles2_get_attrib_location_stub,          /* glGetAttribLocation */
	_gles2_get_program_info_log_stub,         /* glGetProgramInfoLog */
	_gles2_get_programiv_stub,                /* glGetProgramiv */
	_gles2_get_shader_info_log_stub,          /* glGetShaderInfoLog */
	_gles2_get_shaderiv_stub,                 /* glGetShaderiv */
	_gles2_get_shader_precision_format_stub,  /* glGetShaderPrecisionFormat */
	_gles2_get_shader_source_stub,            /* glGetShaderSource */
	_gles2_get_uniformfv_stub,                /* glGetUniformfv */
	_gles2_get_uniformiv_stub,                /* glGetUniformiv */
	_gles2_get_uniform_location_stub,         /* glGetUniformLocation */
	_gles2_get_vertex_attribfv_stub,          /* glGetVertexAttribfv */
	_gles2_get_vertex_attribiv_stub,          /* glGetVertexAttribiv */
	_gles2_get_vertex_attrib_pointerv_stub,   /* glGetVertexAttribPointerv */
	_gles2_is_program_stub,                   /* glIsProgram */
	_gles2_is_shader_stub,                    /* glIsShader */
	_gles2_link_program_stub,                 /* glLinkProgram */
	_gles2_release_shader_compiler_stub,      /* glReleaseShaderCompiler */
	_gles2_shader_binary_stub,                /* glShaderBinary */
	_gles2_shader_source_stub,                /* glShaderSource */
	_gles2_stencil_func_separate_stub,        /* glStencilFuncSeparate */
	_gles2_stencil_mask_separate_stub,        /* glStencilMaskSeparate */
	_gles2_stencil_op_separate_stub,          /* glStencilOpSeparate */
	_gles2_uniform1f_stub,                    /* glUniform1f */
	_gles2_uniform1fv_stub,                   /* glUniform1fv */
	_gles2_uniform1i_stub,                    /* glUniform1i */
	_gles2_uniform1iv_stub,                   /* glUniform1iv */
	_gles2_uniform2f_stub,                    /* glUniform2f */
	_gles2_uniform2fv_stub,                   /* glUniform2fv */
	_gles2_uniform2i_stub,                    /* glUniform2i */
	_gles2_uniform2iv_stub,                   /* glUniform2iv */
	_gles2_uniform3f_stub,                    /* glUniform3f */
	_gles2_uniform3fv_stub,                   /* glUniform3fv */
	_gles2_uniform3i_stub,                    /* glUniform3i */
	_gles2_uniform3iv_stub,                   /* glUniform3iv */
	_gles2_uniform4f_stub,                    /* glUniform4f */
	_gles2_uniform4fv_stub,                   /* glUniform4fv */
	_gles2_uniform4i_stub,                    /* glUniform4i */
	_gles2_uniform4iv_stub,                   /* glUniform4iv */
	_gles2_uniform_matrix2fv_stub,            /* glUniformMatrix2fv */
	_gles2_uniform_matrix3fv_stub,            /* glUniformMatrix3fv */
	_gles2_uniform_matrix4fv_stub,            /* glUniformMatrix4fv */
	_gles2_use_program_stub,                  /* glUseProgram */
	_gles2_validate_program_stub,             /* glValidateProgram */
	_gles2_vertex_attrib1f_stub,              /* glVertexAttrib1f */
	_gles2_vertex_attrib1fv_stub,             /* glVertexAttrib1fv */
	_gles2_vertex_attrib2f_stub,              /* glVertexAttrib2f */
	_gles2_vertex_attrib2fv_stub,             /* glVertexAttrib2fv */
	_gles2_vertex_attrib3f_stub,              /* glVertexAttrib3f */
	_gles2_vertex_attrib3fv_stub,             /* glVertexAttrib3fv */
	_gles2_vertex_attrib4f_stub,              /* glVertexAttrib4f */
	_gles2_vertex_attrib4fv_stub,             /* glVertexAttrib4fv */
	_gles2_vertex_attrib_pointer_stub,        /* glVertexAttribPointer */

#endif /* MALI_USE_GLES_2 */

        /* OpenGL ES 2.x extensions */
#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
        _gles2_get_program_binary_stub,           /* glGetProgramBinaryOES */
        _gles2_program_binary_stub,               /* glProgramBinaryOES */
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE && MALI_USE_GLES_2
	_gles2_get_n_uniformfv_ext_stub,          /* glGetnUniformfvEXT */
	_gles2_get_n_uniformiv_ext_stub,          /* glGetnUniformivEXT */
#endif

	_gles1_set_error                          /* Internal function */

};

const struct gles_vtable *_gles1_get_vtable( void )
{
	return &gles11_vtable;
}
