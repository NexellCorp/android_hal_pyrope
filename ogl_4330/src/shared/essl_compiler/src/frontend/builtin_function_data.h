/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
   Macro table of all built-in functions.
   Assumes that the includer has defined a PROCESS macro as follows:
   PROCESS(Function enum, return type, function name, arg1 type, arg2 type, arg3 type, arg4 type, include in vertex shader, include in fragment shader, extension required)
*/

/* angle and trig functions */
PROCESS(EXPR_OP_FUN_RADIANS, genType, "radians", genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_DEGREES, genType, "degrees", genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_SIN,     genType, "sin",     genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_COS,     genType, "cos",     genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TAN,     genType, "tan",     genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_ASIN,    genType, "asin",    genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_ACOS,    genType, "acos",    genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_ATAN,    genType, "atan",    genType, genType, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_ATAN,    genType, "atan",    genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)

/* exponential functions */
PROCESS(EXPR_OP_FUN_POW,         genType, "pow",         genType, genType, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_EXP,         genType, "exp",         genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_LOG,         genType, "log",         genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_EXP2,        genType, "exp2",        genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_LOG2,        genType, "log2",        genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_SQRT,        genType, "sqrt",        genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_INVERSESQRT, genType, "inversesqrt", genType, 0,       0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)

/* common functions */
PROCESS(EXPR_OP_FUN_ABS,        genType, "abs",        genType,    0,          0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_SIGN,       genType, "sign",       genType,    0,          0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_FLOOR,      genType, "floor",      genType,    0,          0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_CEIL,       genType, "ceil",       genType,    0,          0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_FRACT,      genType, "fract",      genType,    0,          0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_MOD,        genType, "mod",        genType,    TYPE_FLOAT, 0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_MOD,        genType, "mod",        genType,    genType,    0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_MIN,        genType, "min",        genType,    genType,    0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_MIN,        genType, "min",        genType,    TYPE_FLOAT, 0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_MAX,        genType, "max",        genType,    genType,    0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_MAX,        genType, "max",        genType,    TYPE_FLOAT, 0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_CLAMP,      genType, "clamp",      genType,    genType,    genType,    0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_CLAMP,      genType, "clamp",      genType,    TYPE_FLOAT, TYPE_FLOAT, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_MIX,        genType, "mix",        genType,    genType,    genType,    0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_MIX,        genType, "mix",        genType,    genType,    TYPE_FLOAT, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_STEP,       genType, "step",       genType,    genType,    0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_STEP,       genType, "step",       TYPE_FLOAT, genType,    0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_SMOOTHSTEP, genType, "smoothstep", genType,    genType,    genType,    0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_SMOOTHSTEP, genType, "smoothstep", TYPE_FLOAT, TYPE_FLOAT, genType,    0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)

/* geometric functions */
PROCESS(EXPR_OP_FUN_LENGTH,      TYPE_FLOAT, "length",      genType, 0,       0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_DISTANCE,    TYPE_FLOAT, "distance",    genType, genType, 0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_DOT,         TYPE_FLOAT, "dot",         genType, genType, 0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_CROSS,       vec3,       "cross",       vec3,    vec3,    0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_NORMALIZE,   genType,    "normalize",   genType, 0,       0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_FACEFORWARD, genType,    "faceforward", genType, genType, genType,    0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_REFLECT,     genType,    "reflect",     genType, genType, 0,          0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_REFRACT,     genType,    "refract",     genType, genType, TYPE_FLOAT, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)

/* matrix functions */
PROCESS(EXPR_OP_FUN_MATRIXCOMPMULT, mat, "matrixCompMult", mat, mat, 0,0,  ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)

/* vector relational functions */
PROCESS(EXPR_OP_FUN_LESSTHAN,         bvec,      "lessThan",         vec,  vec,  0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_LESSTHAN,         bvec,      "lessThan",         ivec, ivec, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_LESSTHANEQUAL,    bvec,      "lessThanEqual",    vec,  vec,  0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_LESSTHANEQUAL,    bvec,      "lessThanEqual",    ivec, ivec, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_GREATERTHAN,      bvec,      "greaterThan",      vec,  vec,  0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_GREATERTHAN,      bvec,      "greaterThan",      ivec, ivec, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_GREATERTHANEQUAL, bvec,      "greaterThanEqual", vec,  vec,  0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_GREATERTHANEQUAL, bvec,      "greaterThanEqual", ivec, ivec, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_EQUAL,            bvec,      "equal",            vec,  vec,  0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_EQUAL,            bvec,      "equal",            ivec, ivec, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_EQUAL,            bvec,      "equal",            bvec, bvec, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_NOTEQUAL,         bvec,      "notEqual",         vec,  vec,  0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_NOTEQUAL,         bvec,      "notEqual",         ivec, ivec, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_NOTEQUAL,         bvec,      "notEqual",         bvec, bvec, 0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_ANY,              TYPE_BOOL, "any",              bvec, 0,    0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_ALL,              TYPE_BOOL, "all",              bvec, 0,    0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_NOT,              bvec,      "not",              bvec, 0,    0, 0, ESSL_TRUE, ESSL_TRUE, EXTENSION_NONE)

/* texture lookup functions (includes definitions for GL_OES_EGL_image_external) */
PROCESS(EXPR_OP_FUN_TEXTURE2D,        vec4, "texture2D",        TYPE_SAMPLER_2D,   vec2, 0,          0, ESSL_TRUE,  ESSL_TRUE,  EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTURE2D,        vec4, "texture2D",        TYPE_SAMPLER_2D,   vec2, TYPE_FLOAT, 0, ESSL_FALSE, ESSL_TRUE,  EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTUREEXTERNAL,       vec4, "texture2D",   TYPE_SAMPLER_EXTERNAL,  vec2, 0, 0,  ESSL_TRUE,  ESSL_TRUE,  EXTENSION_GL_OES_TEXTURE_EXTERNAL)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJ,    vec4, "texture2DProj",    TYPE_SAMPLER_2D,   vec3, 0,          0, ESSL_TRUE,  ESSL_TRUE,  EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJ,    vec4, "texture2DProj",    TYPE_SAMPLER_2D,   vec3, TYPE_FLOAT, 0, ESSL_FALSE, ESSL_TRUE,  EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJ_W,  vec4, "texture2DProj",    TYPE_SAMPLER_2D,   vec4, 0,          0, ESSL_TRUE,  ESSL_TRUE,  EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJ_W,  vec4, "texture2DProj",    TYPE_SAMPLER_2D,   vec4, TYPE_FLOAT, 0, ESSL_FALSE, ESSL_TRUE,  EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTUREEXTERNALPROJ,   vec4, "texture2DProj",    TYPE_SAMPLER_EXTERNAL,  vec3, 0, 0, ESSL_TRUE,  ESSL_TRUE,  EXTENSION_GL_OES_TEXTURE_EXTERNAL)
PROCESS(EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W, vec4, "texture2DProj",    TYPE_SAMPLER_EXTERNAL,  vec4, 0, 0, ESSL_TRUE,  ESSL_TRUE,  EXTENSION_GL_OES_TEXTURE_EXTERNAL)
PROCESS(EXPR_OP_FUN_TEXTURE2DLOD,     vec4, "texture2DLod",     TYPE_SAMPLER_2D,   vec2, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_FALSE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJLOD, vec4, "texture2DProjLod", TYPE_SAMPLER_2D,   vec3, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_FALSE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJLOD_W,vec4,"texture2DProjLod", TYPE_SAMPLER_2D,   vec4, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_FALSE, EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTURECUBE,      vec4, "textureCube",      TYPE_SAMPLER_CUBE, vec3, 0,          0, ESSL_TRUE,  ESSL_TRUE,  EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTURECUBE,      vec4, "textureCube",      TYPE_SAMPLER_CUBE, vec3, TYPE_FLOAT, 0, ESSL_FALSE, ESSL_TRUE,  EXTENSION_NONE)
PROCESS(EXPR_OP_FUN_TEXTURECUBELOD,   vec4, "textureCubeLod",   TYPE_SAMPLER_CUBE, vec3, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_FALSE, EXTENSION_NONE)

PROCESS(EXPR_OP_FUN_TEXTURE2DLOD_EXT,		vec4, "texture2DLodEXT",	TYPE_SAMPLER_2D,   vec2, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_TRUE, EXTENSION_GL_EXT_SHADER_TEXTURE_LOD)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT,	vec4, "texture2DProjLodEXT",TYPE_SAMPLER_2D,   vec3, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_TRUE, EXTENSION_GL_EXT_SHADER_TEXTURE_LOD)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT,	vec4,"texture2DProjLodEXT",	TYPE_SAMPLER_2D,   vec4, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_TRUE, EXTENSION_GL_EXT_SHADER_TEXTURE_LOD)
PROCESS(EXPR_OP_FUN_TEXTURECUBELOD_EXT,		vec4, "textureCubeLodEXT",	TYPE_SAMPLER_CUBE, vec3, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_TRUE, EXTENSION_GL_EXT_SHADER_TEXTURE_LOD)

PROCESS(EXPR_OP_FUN_TEXTURE2DGRAD_EXT,		vec4, "texture2DGradEXT",		TYPE_SAMPLER_2D,   vec2, vec2, vec2, ESSL_TRUE,  ESSL_TRUE, EXTENSION_GL_EXT_SHADER_TEXTURE_LOD)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT,	vec4, "texture2DProjGradEXT",	TYPE_SAMPLER_2D,   vec3, vec2, vec2, ESSL_TRUE,  ESSL_TRUE, EXTENSION_GL_EXT_SHADER_TEXTURE_LOD)
PROCESS(EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT,vec4, "texture2DProjGradEXT",	TYPE_SAMPLER_2D,   vec4, vec2, vec2, ESSL_TRUE,  ESSL_TRUE, EXTENSION_GL_EXT_SHADER_TEXTURE_LOD)
PROCESS(EXPR_OP_FUN_TEXTURECUBEGRAD_EXT,	vec4, "textureCubeGradEXT",		TYPE_SAMPLER_CUBE, vec3, vec3, vec3, ESSL_TRUE,  ESSL_TRUE, EXTENSION_GL_EXT_SHADER_TEXTURE_LOD)

/* texture 3d functions (extension required: GL_OES_texture_3D) */

PROCESS(EXPR_OP_FUN_TEXTURE3D,        vec4, "texture3D",        TYPE_SAMPLER_3D,   vec3, 0,          0, ESSL_TRUE,  ESSL_TRUE,  EXTENSION_GL_OES_TEXTURE_3D)
PROCESS(EXPR_OP_FUN_TEXTURE3D,        vec4, "texture3D",        TYPE_SAMPLER_3D,   vec3, TYPE_FLOAT, 0, ESSL_FALSE, ESSL_TRUE,  EXTENSION_GL_OES_TEXTURE_3D)
PROCESS(EXPR_OP_FUN_TEXTURE3DPROJ,    vec4, "texture3DProj",    TYPE_SAMPLER_3D,   vec4, 0,          0, ESSL_TRUE,  ESSL_TRUE,  EXTENSION_GL_OES_TEXTURE_3D)
PROCESS(EXPR_OP_FUN_TEXTURE3DPROJ,    vec4, "texture3DProj",    TYPE_SAMPLER_3D,   vec4, TYPE_FLOAT, 0, ESSL_FALSE,  ESSL_TRUE, EXTENSION_GL_OES_TEXTURE_3D)
PROCESS(EXPR_OP_FUN_TEXTURE3DLOD,     vec4, "texture3DLod",     TYPE_SAMPLER_3D,   vec3, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_FALSE, EXTENSION_GL_OES_TEXTURE_3D)
PROCESS(EXPR_OP_FUN_TEXTURE3DPROJLOD, vec4, "texture3DProjLod", TYPE_SAMPLER_3D,   vec4, TYPE_FLOAT, 0, ESSL_TRUE,  ESSL_FALSE, EXTENSION_GL_OES_TEXTURE_3D)

/* shadow functions (extension required: GL_OES_shadow) */

PROCESS(EXPR_OP_FUN_SHADOW2D,         vec4, "shadow2DEXT",        TYPE_SAMPLER_2D_SHADOW, vec3, 0,          0, ESSL_TRUE, ESSL_TRUE,  EXTENSION_GL_EXT_SHADOW_SAMPLERS)
PROCESS(EXPR_OP_FUN_SHADOW2DPROJ,     vec4, "shadow2DProjEXT",    TYPE_SAMPLER_2D_SHADOW, vec4, 0,          0, ESSL_TRUE, ESSL_TRUE,  EXTENSION_GL_EXT_SHADOW_SAMPLERS)

/* fragment processing functions (extension required: GL_OES_standard_derivatives */
PROCESS(EXPR_OP_FUN_DFDX,   genType, "dFdx",   genType, 0, 0, 0, ESSL_FALSE, ESSL_TRUE, EXTENSION_GL_OES_STANDARD_DERIVATIVES)
PROCESS(EXPR_OP_FUN_DFDY,   genType, "dFdy",   genType, 0, 0, 0, ESSL_FALSE, ESSL_TRUE, EXTENSION_GL_OES_STANDARD_DERIVATIVES)
PROCESS(EXPR_OP_FUN_FWIDTH, genType, "fwidth", genType, 0, 0, 0, ESSL_FALSE, ESSL_TRUE, EXTENSION_GL_OES_STANDARD_DERIVATIVES)
