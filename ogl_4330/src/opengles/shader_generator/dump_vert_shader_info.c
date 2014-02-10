/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_base.h>

#include "gles_context.h"
#include "gles_share_lists.h"

#if MALI_BUILD_ANDROID_MONOLITHIC
#include "gles_entrypoints_shim.h"
#include "gles1_entrypoints_shim.h"
#endif

#include "../gles1_state/gles1_state.h"

#include "common/essl_common.h"

#include "shadergen_maligp2/shader_pieces.h"
#include "shadergen_maligp2/select_pieces.h"

#include "dump_shader_info_common.h"

#include "shadergen_maligp2/piece_names.h"

const char* get_piece_name(const piece* p)
{
    unsigned i;
    for(i = 0; pieceIdAndStringArray[i].id != p->id && pieceIdAndStringArray[i].id != 0; i++)
    {
    }
    return pieceIdAndStringArray[i].name;
}

void print_transform_state(mali_file* fp)
{
    GLfloat modelviewMatrix[4*4], projectionMatrix[4*4];
    MALI_GLES_NAME_WRAP(glGetFloatv)(GL_MODELVIEW_MATRIX, modelviewMatrix);
    MALI_GLES_NAME_WRAP(glGetFloatv)(GL_PROJECTION_MATRIX, projectionMatrix);
    _mali_sys_fprintf(fp, "    MODELVIEW_MATRIX:\n");
    print_matrix4x4(fp, modelviewMatrix);
    _mali_sys_fprintf(fp, "\n");
    _mali_sys_fprintf(fp, "    PROJECTION_MATRIX:\n");
    print_matrix4x4(fp, projectionMatrix);
    _mali_sys_fprintf(fp, "\n");
    _mali_sys_fprintf(   fp, "    NORMALIZE: %s\n\n", MALI_GLES_NAME_WRAP(glIsEnabled)(GL_NORMALIZE)?"ON":"OFF");
}


void print_skinning_matrices(mali_file* fp)
{
    int i;
    gles1_state       *state_gles1;
    gles_context *ctx = _gles_get_context();
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->state.api.gles1);

    state_gles1  = ctx->state.api.gles1;

    for (i = 0 ; i < GLES1_MAX_PALETTE_MATRICES_OES; i++)
    {
        if(!state_gles1->transform.matrix_palette_identity[i])
        {
            mali_matrix4x4* skinningMatrix = &state_gles1->transform.matrix_palette[i];
            _mali_sys_fprintf(fp, "      PALETTE_MATRIX%d:\n", i);
            print_matrix4x4(fp, (float*)skinningMatrix);
        }
    }
}

void print_skinning_state(mali_file* fp)
{
#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
    int matrixIndexArraySize = 0, weightArraySize = 0;
    GLboolean matrixPaletteEnabled = MALI_GLES_NAME_WRAP(glIsEnabled)(GL_MATRIX_PALETTE_OES);
    /* MATRIX_PALETTE_OES */
    _mali_sys_fprintf(   fp, "    MATRIX_PALETTE_OES: %s\n", (matrixPaletteEnabled==GL_TRUE)?"ON":"OFF");
    if(matrixPaletteEnabled == GL_TRUE)
    {
        MALI_GLES_NAME_WRAP(glGetIntegerv)(GL_MATRIX_INDEX_ARRAY_SIZE_OES, &matrixIndexArraySize);
        MALI_GLES_NAME_WRAP(glGetIntegerv)(GL_WEIGHT_ARRAY_SIZE_OES, &weightArraySize);
        _mali_sys_fprintf(   fp, "      MATRIX_INDEX_ARRAY_SIZE_OES: %d\n", matrixIndexArraySize);
        _mali_sys_fprintf(   fp, "      WEIGHT_ARRAY_SIZE_OES:       %d\n", weightArraySize);
        print_skinning_matrices(fp);
    }
    _mali_sys_fprintf(fp, "\n");
#endif
}

void print_texcoord_state(mali_file* fp)
{
    int i;
    GLint maxTexUnits = 0, activeTexUnit;

    MALI_GLES_NAME_WRAP(glGetIntegerv)(GL_ACTIVE_TEXTURE, &activeTexUnit);
    MALI_GLES_NAME_WRAP(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &maxTexUnits);
    for( i = 0; i < maxTexUnits; i++ )
    {
        MALI_GLES_NAME_WRAP(glActiveTexture)(GL_TEXTURE0 + i);
        if(MALI_GLES_NAME_WRAP(glIsEnabled)(GL_TEXTURE_2D))
        {
            GLfloat matrix[4*4];
            MALI_GLES_NAME_WRAP(glGetFloatv)(GL_TEXTURE_MATRIX, matrix);
            _mali_sys_fprintf(fp, "    TEXTURE%d matrix:\n", i);
            print_matrix4x4(fp, matrix);
        }
    }
    MALI_GLES_NAME_WRAP(glActiveTexture)(activeTexUnit);
    _mali_sys_fprintf(fp, "\n");
}



void print_lighting_state(mali_file* fp)
{
    GLboolean lightingEnabled = GL_FALSE;
    GLint maxLights = 0;

    lightingEnabled = MALI_GLES_NAME_WRAP(glIsEnabled)(GL_LIGHTING);
    if(lightingEnabled == GL_TRUE)
        _mali_sys_fprintf(fp, "    Lighting enabled.\n");
    {
        GLfloat ambient[4];
        GLfloat diffuse[4];
        GLfloat specular[4];
        GLfloat emission[4];
        GLfloat shininess;
		int i,j;

        _mali_sys_fprintf(fp, " GLES 1.1 Lighting state:\n");
        _mali_sys_fprintf(fp, "    MATERIAL_FRONT:\n");
        MALI_GLES_NAME_WRAP(glGetMaterialfv)(GL_FRONT, GL_AMBIENT, ambient);
        MALI_GLES_NAME_WRAP(glGetMaterialfv)(GL_FRONT, GL_DIFFUSE, diffuse);
        MALI_GLES_NAME_WRAP(glGetMaterialfv)(GL_FRONT, GL_SPECULAR, specular);
        MALI_GLES_NAME_WRAP(glGetMaterialfv)(GL_FRONT, GL_EMISSION, emission);
        MALI_GLES_NAME_WRAP(glGetMaterialfv)(GL_FRONT, GL_SHININESS, &shininess);
        print_vec4(fp, "      AMBIENT:        ", ambient);
        print_vec4(fp, "      DIFFUSE:        ", diffuse);
        print_vec4(fp, "      SPECULAR:       ", specular);
        print_vec4(fp, "      EMISSION:       ", emission);
        _mali_sys_fprintf(fp,    "      SHININESS:      %f\n\n", shininess);

		j = 0;
        MALI_GLES_NAME_WRAP(glGetIntegerv)(GL_MAX_LIGHTS, &maxLights);
		for( i = 0; i < maxLights; i++ )
		{
            GLboolean lightEnabled = GL_FALSE;
            lightEnabled = MALI_GLES_NAME_WRAP(glIsEnabled)(GL_LIGHT0+i);

            if(lightEnabled == GL_TRUE)
			{
			    GLfloat ambient[4];
				GLfloat diffuse[4];
				GLfloat specular[4];
				GLfloat spotDir[3];
                GLfloat position[4];
                GLfloat spotExp, spotCutOff, attC, attL, attQ;
                _mali_sys_fprintf(fp, "    LIGHT%d:\n", i);
                /*MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0, GL_AMBIENT, ambient);*/
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_AMBIENT, ambient);
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_DIFFUSE, diffuse);
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_SPECULAR, specular);
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_SPOT_DIRECTION, spotDir);
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_POSITION, position);
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_SPOT_EXPONENT, &spotExp);
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_SPOT_CUTOFF, &spotCutOff);
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_CONSTANT_ATTENUATION, &attC);
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_LINEAR_ATTENUATION, &attL);
                MALI_GLES_NAME_WRAP(glGetLightfv)(GL_LIGHT0+i, GL_QUADRATIC_ATTENUATION, &attQ);
                print_vec4(fp, "      POSITION:       ", position);
                print_vec4(fp, "      AMBIENT:        ", ambient);
                print_vec4(fp, "      DIFFUSE:        ", diffuse);
                print_vec4(fp, "      SPECULAR:       ", specular);
                print_vec3(fp, "      SPOT_DIRECTION: ", spotDir);
                _mali_sys_fprintf(fp,    "      SPOT_EXPONENT:  %f\n", spotExp);
                _mali_sys_fprintf(fp,    "      SPT_CUTOFF:     %f\n", spotCutOff);
                _mali_sys_fprintf(fp,    "      ATTENUATION:    %f, %f, %f\n\n", attC, attL, attQ);
				j++;
			}
		}
	}
}



void print_pointsize_state(mali_file* fp)
{
    GLfloat attenuation[3];
    GLboolean arrayEnabled;

    MALI_GLES_NAME_WRAP(glGetFloatv)(GL_POINT_DISTANCE_ATTENUATION, attenuation);
    arrayEnabled = MALI_GLES_NAME_WRAP(glIsEnabled)(GL_POINT_SIZE_ARRAY_OES);
    print_vec3(fp, "  POINT_DISTANCE_ATTENUATION: ", attenuation);
    _mali_sys_fprintf(   fp, "  GL_POINT_SIZE_ARRAY_OES: %s\n", (arrayEnabled == GL_TRUE)?"ON":"OFF");
}



void print_fog_state(mali_file* fp)
{
    GLboolean fogEnabled;
    fogEnabled = MALI_GLES_NAME_WRAP(glIsEnabled)(GL_FOG);

    if(fogEnabled == GL_TRUE) {
        GLint fogMode;
        GLint fogHint;
        _mali_sys_fprintf(fp, "    Fog enabled.\n");
        MALI_GLES_NAME_WRAP(glGetIntegerv)(GL_FOG_MODE, &fogMode);
        if(fogMode == GL_LINEAR) {
            GLfloat start, end;
            MALI_GLES_NAME_WRAP(glGetFloatv)(GL_FOG_START, &start);
            MALI_GLES_NAME_WRAP(glGetFloatv)(GL_FOG_END, &end);
            _mali_sys_fprintf(   fp, "      Mode: LINEAR, start = %f, end = %f\n", start, end);
        }
        else {
            GLfloat density;
            MALI_GLES_NAME_WRAP(glGetFloatv)(GL_FOG_DENSITY, &density);
            _mali_sys_fprintf(   fp, "      Mode: %s, density = %f\n", (fogMode == GL_EXP2)?"EXP2":"EXP", density);
        }
        MALI_GLES_NAME_WRAP(glGetIntegerv)(GL_FOG_HINT, &fogHint);
        _mali_sys_fprintf(fp, "      Hint: %s\n", (fogHint == GL_DONT_CARE )?"DONT_CARE": ((fogHint == GL_NICEST )?"NICEST":"FASTEST") );
    }
}



void print_clipplane_state(mali_file* fp)
{
    int i, maxClipPlanes = 0;
    MALI_GLES_NAME_WRAP(glGetIntegerv)(GL_MAX_CLIP_PLANES, &maxClipPlanes);
    for( i = 0; i < maxClipPlanes; i++ )
    {
        GLboolean enabled = MALI_GLES_NAME_WRAP(glIsEnabled)(GL_CLIP_PLANE0 + i);
        if(enabled == GL_TRUE)
        {
            GLfloat clipPlane[4];
            MALI_GLES_NAME_WRAP(glGetClipPlanef)(GL_CLIP_PLANE0 + i, clipPlane);
            _mali_sys_fprintf(fp, "    CLIP_PLANE%d: ", i);
            print_vec4(fp, "", clipPlane);
        }
    }
}



void print_gles1_state(mali_file* fp)
{
    gles_context *ctx = _gles_get_context();
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->state.api.gles1);
    _gles_share_lists_unlock( ctx->share_lists );

    print_transform_state(fp);
    print_skinning_state(fp);
    print_texcoord_state(fp);
    print_lighting_state(fp);
    print_pointsize_state(fp);
    print_fog_state(fp);
    print_clipplane_state(fp);

	_gles_share_lists_lock( ctx->share_lists );
}

#define EYESPACEPOSBIT (_VERTEX_FEATURE_TRANSFORM_EYESPACEPOS_MASK << _VERTEX_FEATURE_TRANSFORM_EYESPACEPOS_OFFSET)
#define EYESPACENORMALBIT (_VERTEX_FEATURE_TRANSFORM_EYESPACENORMAL_MASK << _VERTEX_FEATURE_TRANSFORM_EYESPACENORMAL_OFFSET)

void print_transform_phase(mali_file* fp, vertex_shadergen_state* state)
{
    const piece* p = 0;
    unsigned features;
	features = vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_TRANSFORM);
	if (vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS) == NUMLIGHTS_ONE ||
		vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS) == NUMLIGHTS_MANY ||
		vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_FOG_DIST) ||
		vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_POINTSIZE_ATTENUATION) ||
		vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_CLIPPLANE_PLANE0))
	{
		features |= EYESPACEPOSBIT;
	}
	if (vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS) == NUMLIGHTS_ONE ||
		vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS) == NUMLIGHTS_MANY)
	{
		features |= EYESPACENORMALBIT;
	}
    _mali_sys_fprintf(fp, " Transform phase:\n");
    _mali_sys_fprintf(fp, "    Normalize:       %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_NORMALIZE)?"ON":"OFF");
    _mali_sys_fprintf(fp, "    Eyespace normal: %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_EYESPACENORMAL)?"ON":"OFF");
    _mali_sys_fprintf(fp, "    Eyespace pos:    %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_EYESPACEPOS)?"ON":"OFF");
    _mali_sys_fprintf(fp, "    Skinning:        %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_TRANSFORM_SKINNING)?"ON":"OFF");
    p = _piecegen_get_piece(VERTEX_PHASE_TRANSFORM, vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_TRANSFORM));
    if(p && p->len != 0)
    {
        if(features & (EYESPACEPOSBIT | EYESPACENORMALBIT))
            _mali_sys_fprintf(fp, " => Piece: %s\n    (%d+3 instructions)\n", get_piece_name(p), p->len);
        else
            _mali_sys_fprintf(fp, " => Piece: %s\n    (%d+3 instructions)\n", get_piece_name(p), p->len);
    }
    _mali_sys_fprintf(fp, "\n");
}



void print_texcoord_transform_phase(mali_file* fp, vertex_shadergen_state*  state)
{
    int i, nonIdentity = 0;
    unsigned lengthRead = 0, lengthTransform = 0, lengthWrite = 0;
    _mali_sys_fprintf(fp, " Texcoord transform phase:\n");
    for (i = 0 ; i < MAX_TEXTURE_STAGES ; i++) {
        unsigned transform = vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_TRANSFORM(i));
        if( vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_ENABLE(i))) {
            _mali_sys_fprintf(fp, "    TexCoord%d:       %s\n", i, (transform==TEX_IDENTITY)?"IDENTITY":"TRANSFORM");
            lengthRead += 1;
            if(transform == TEX_TRANSFORM)
                nonIdentity = 1;
        }
    }        
    if(nonIdentity) {
        const piece* p = _piecegen_get_piece(VERTEX_PHASE_TEXCOORDTRANSFORM, 0);
        lengthTransform = p->len;
        for (i = 0 ; i < MAX_TEXTURE_STAGES ; i++) {
        	unsigned transform = vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_TRANSFORM(i));
            if(transform == TEX_TRANSFORM)
                lengthWrite += 1;
         }
    }
    if(nonIdentity)
        _mali_sys_fprintf(fp, " => (%d+3+%d+3+%d instructions)\n", lengthRead, lengthTransform, lengthWrite);
    else
        _mali_sys_fprintf(fp, " => (%d instructions)\n", lengthRead);
    _mali_sys_fprintf(fp, "\n");

/*
  Example:
    Texcoord0: TEX_IDENTITY
    Texcoord1: TEX_DISABLED
    Texcoord2: TEX_TRANSFORM

=>  var_TexCoord0 = attr_TexCoord0;

    TransformedTexCoords[0] = attr_TexCoord0;
    replace attr_TexCoord* with attr_TexCoord2 in previous piece, yielding TransformedTexCoords[0] = attr_TexCoord2
    WAIT3
    tex_coord_transform
    WAIT3
    var_TexCoord2 = TransformedTexCoords[2];
    replace TransformedTexCoords[*] with TransformedTexCoords[0] in previous piece, yielding var_TexCoord2 = TransformedTexCoords[0]


  Example:
    Texcoord0: TEX_TRANSFORM
    Texcoord1: TEX_DISABLED
    Texcoord2: TEX_IDENTITY

=>  TransformedTexCoords[0] = attr_TexCoord0;
    TransformedTexCoords[0] = attr_TexCoord0;
    var_TexCoord2 = attr_TexCoord2;
       
    WAIT3
    tex_coord_transform
    WAIT3
    var_TexCoord0 = TransformedTexCoords[0];
    var_TexCoord0 = TransformedTexCoords[0];


    
*/
}


void print_lighting_phase(mali_file* fp, vertex_shadergen_state* state)
{
    const piece* p = 0;
    const char* strNumLights[] = { "OFF", "ZERO", "ONE", "MANY" };
    /*PRINT_ONOFF("Color material: ", VERTEX_GET_FEATURE_LIGHTING_COLORMATERIAL(*/
    _mali_sys_fprintf(fp, " Lighting phase:\n");
    _mali_sys_fprintf(fp, "    Color material:  %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_COLORMATERIAL)?"ON":"OFF");
    _mali_sys_fprintf(fp, "    Attenuation:     %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_ATTENUATION)?"ON":"OFF");
    _mali_sys_fprintf(fp, "    Specular:        %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_SPECULAR)?"ON":"OFF");
    _mali_sys_fprintf(fp, "    Spotlight:       %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_SPOTLIGHT)?"ON":"OFF");
    _mali_sys_fprintf(fp, "    Twosided:        %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_TWOSIDED)?"ON":"OFF");
    _mali_sys_fprintf(fp, "    Numlights:       %s\n", strNumLights[(vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_LIGHTING_NUMLIGHTS) & 0x3)]);
    p = _piecegen_get_piece(VERTEX_PHASE_LIGHTING, vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_LIGHTING));
    if(p && p->len != 0)
    {
        _mali_sys_fprintf(fp, " => Piece: %s\n    (%d instructions)\n", get_piece_name(p), p->len);
    }
    _mali_sys_fprintf(fp, "\n");
}



void print_pointsize_phase(mali_file* fp, vertex_shadergen_state* state)
{
    const piece* p = 0;
    _mali_sys_fprintf(fp, " Pointsize phase:\n");
    _mali_sys_fprintf(fp, "    Attenuation:     %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_POINTSIZE_ATTENUATION)?"ON":"OFF");
    _mali_sys_fprintf(fp, "    Copy:            %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_POINTSIZE_COPY)?"ON":"OFF");
    p = _piecegen_get_piece(VERTEX_PHASE_POINTSIZE, vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_POINTSIZE));
    if(p && p->len != 0)
    {
        _mali_sys_fprintf(fp, " => Piece: %s\n    (%d instructions)\n", get_piece_name(p), p->len);
    }
    _mali_sys_fprintf(fp, "\n");
}



void print_fog_phase(mali_file* fp, vertex_shadergen_state* state)
{
    const piece* p = 0;
    _mali_sys_fprintf(fp, " Fog phase:\n");
    _mali_sys_fprintf(fp, "    Dist:            %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_FOG_DIST)?"ON":"OFF");
    p = _piecegen_get_piece(VERTEX_PHASE_FOG, vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_FOG));
    if(p && p->len != 0)
    {
        _mali_sys_fprintf(fp, " => Piece: %s\n    (%d instructions)\n", get_piece_name(p), p->len); 
    }
    _mali_sys_fprintf(fp, "\n");
}



void print_clipplane_phase(mali_file* fp, vertex_shadergen_state* state)
{
    const piece* p = 0;
    _mali_sys_fprintf(fp, " Clip plane phase:\n");
    _mali_sys_fprintf(fp, "    Plane0:          %s\n", vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURE_CLIPPLANE_PLANE0)?"ON":"OFF");
    p = _piecegen_get_piece(VERTEX_PHASE_CLIPPLANE, vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_CLIPPLANE));
    if(p && p->len != 0)
    {
        _mali_sys_fprintf(fp, " => Piece: %s\n    (%d instructions)\n", get_piece_name(p), p->len);
    }
    _mali_sys_fprintf(fp, "\n");
}



void shader_dump_print_vertex_state(mali_file* fp, vertex_shadergen_state* state)
{
    _mali_sys_fprintf(fp, "GLES 1.1 states:\n");
    print_gles1_state(fp);

    _mali_sys_fprintf(fp, "\n\n\nSimplified vertex state:\n");

    print_transform_phase(fp, state);
    print_texcoord_transform_phase(fp, state);
    print_lighting_phase(fp, state);
    print_pointsize_phase(fp, state);
    print_fog_phase(fp, state);
    print_clipplane_phase(fp, state);
}



void print_instruction(mali_file* fp, const instruction* instr)
{
    unsigned i;
    for(i = 0; i < 4; i++)
    {
        _mali_sys_fprintf(fp, "%08X ", instr->data[i]);
    }
    _mali_sys_fprintf(fp, "\n");
}

void print_piece(mali_file* fp, const piece* p)
{
    unsigned i;
    if(!p)
        return;
    _mali_sys_fprintf(fp, "    (%d instructions)\n", p->len);

    for(i = 0; i < p->len; i++)
    {
        _mali_sys_fprintf(fp, "    ");
        print_instruction(fp, &p->ptr[i]);
    }
}



void shader_dump_print_vertex_shader_pieces(mali_file* fp, const piece_select* pieces, unsigned n_pieces)
{
    unsigned i;
    for(i = 0; i < n_pieces; i++)
    {
        _mali_sys_fprintf(fp, "  Piece %d:\n", i);
        print_piece(fp, pieces[i].piece);
        _mali_sys_fprintf(fp, "\n");
    }
}



void dump_vert_shader_info_internal(vertex_shadergen_state* state)
{
    mali_file* fp;
    char filename[64];

    sprintf(filename, "shadergen_vs_%08X%08X.txt", state->bits[0], state->bits[1]);
    fp = _mali_sys_fopen(filename, "w");
    if(fp)
    {
        shader_dump_print_vertex_state(fp, state);
        _mali_sys_fclose(fp);
    }
    else
        printf("ERROR: Could not open file \"%s\" for writing.\n", filename);
}



void dump_vert_shader_info(vertex_shadergen_state* state)
{
    piece_select pieces[MAX_SELECTED_PIECES];
    unsigned n_pieces;
    essl_bool res;
    
    res = _vertex_shadergen_select_pieces(state, pieces, &n_pieces);
    if(res)
    {
        /*unsigned size;
        unsigned *data = NULL;
        data = _vertex_shadergen_glue_pieces(pieces, n_pieces, &size, alloc, free);*/
        /*
        if(data != 0)
        {
            _essl_buffer_native_to_le_byteswap(data, size/sizeof(u32));
        }
        */
        dump_vert_shader_info_internal(state);
    }
}

