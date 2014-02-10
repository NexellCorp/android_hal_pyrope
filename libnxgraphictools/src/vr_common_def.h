#ifndef __VR_COMMON_DEF__
#define __VR_COMMON_DEF__

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdlib.h>
#include <string.h>

#define VR_TRUE 	1
#define VR_FALSE	0

#define MALLOC_FUNC malloc
#define CALLOC_FUNC calloc
#define REALLOC_FUNC realloc
#define FREE_FUNC	free
#define MEMSET_FUNC memset
#define MEMCPY_FUNC memcpy

#define MALLOC(a) MALLOC_FUNC((a))
#define CALLOC(a,b) CALLOC_FUNC((a), (b))
#define REALLOC(a,b) REALLOC_FUNC((a), (b))
#define FREE(a) FREE_FUNC((a))
#define MEMSET(dst,val,size) MEMSET_FUNC((dst),(val),(size))
#define MEMCPY(dst,src,size) MEMCPY_FUNC((dst),(src),(size))

#define VR_FEATURE_INPUT_EGLIMAGE_USE
#define VR_FEATURE_ION_ALLOC_USE
//#define VR_FEATURE_SHADER_FILE_USE

#ifdef VR_FEATURE_SHADER_FILE_USE
#define VERTEX_SHADER_SOURCE_DEINTERLACE		"res/deinterlace.vert"
#define FRAGMENT_SHADER_SOURCE_DEINTERLACE		"res/deinterlace.frag"
#define VERTEX_SHADER_SOURCE_SCALE				"res/scale.vert"
#define FRAGMENT_SHADER_SOURCE_SCALE			"res/scale.frag"
#define VERTEX_SHADER_SOURCE_CVT2Y				"res/cvt2y.vert"
#define FRAGMENT_SHADER_SOURCE_CVT2Y			"res/cvt2y.frag"
#define VERTEX_SHADER_SOURCE_CVT2UV				"res/cvt2uv.vert"
#define FRAGMENT_SHADER_SOURCE_CVT2UV			"res/cvt2uv.frag"
#define VERTEX_SHADER_SOURCE_CVT2RGBA			"res/cvt2rgba.vert"
#define FRAGMENT_SHADER_SOURCE_CVT2RGBA			"res/cvt2rgba.frag"
#else
#define VERTEX_SHADER_SOURCE_DEINTERLACE		deinterace_vertex_shader
#define FRAGMENT_SHADER_SOURCE_DEINTERLACE		deinterace_frag_shader
#define VERTEX_SHADER_SOURCE_SCALE				scaler_vertex_shader
#define FRAGMENT_SHADER_SOURCE_SCALE			scaler_frag_shader
#define VERTEX_SHADER_SOURCE_CVT2Y				cvt2y_vertex_shader
#define FRAGMENT_SHADER_SOURCE_CVT2Y			cvt2y_frag_shader
#define VERTEX_SHADER_SOURCE_CVT2UV				cvt2uv_vertex_shader
#define FRAGMENT_SHADER_SOURCE_CVT2UV			cvt2uv_frag_shader
#define VERTEX_SHADER_SOURCE_CVT2RGBA			cvt2rgba_vertex_shader
#define FRAGMENT_SHADER_SOURCE_CVT2RGBA			cvt2rgba_frag_shader
#endif

#endif /* __VR_COMMON_DEF__ */

