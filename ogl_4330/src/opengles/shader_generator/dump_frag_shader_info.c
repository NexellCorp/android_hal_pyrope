/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/* For PRO, fragment shader reduces from 2 to 1 cycle in GL_REPLACE case when changing texture format from RGB to RGBA */

#include <gles_base.h>

#include "dump_frag_shader_info.h"

#include <shared/shadergen_interface.h>
#include <shared/binary_shader/bs_loader.h>

#include "gles_context.h"
#include "gles_share_lists.h"

#if MALI_BUILD_ANDROID_MONOLITHIC 
#include "gles_entrypoints_shim.h"
#include "gles1_entrypoints_shim.h"
#endif

#include "gles_shader_generator_context.h"

#include "dump_shader_info_common.h"

struct valueToStrStruct
{
    GLint value;
    const char* str;
};

const char* vts(GLint value, const struct valueToStrStruct* vtsArray)
{
    int i;
    for(i = 0; vtsArray[i].value != 0 && vtsArray[i].value != value; i++)
    {
    }
    return vtsArray[i].str;
}

void print_texenv_state_single(mali_file* fp)
{
    GLfloat envColor[4];
    GLint envMode, coordReplace = GL_FALSE;
    struct valueToStrStruct vtsEnvMode[] = 
    { 
        { GL_ADD,       "ADD" },
        { GL_MODULATE,  "MODULATE" },
        { GL_DECAL,     "DECAL" },
        { GL_BLEND,     "BLEND" },
        { GL_REPLACE,   "REPLACE" },
        { GL_COMBINE,   "COMBINE" },
        { 0, "" },
    };
    MALI_GLES_NAME_WRAP(glGetTexEnvfv)(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envColor);
    MALI_GLES_NAME_WRAP(glGetTexEnviv)(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &envMode);
    MALI_GLES_NAME_WRAP(glGetTexEnviv)(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, &coordReplace);
    print_vec4(fp, "    TEXTURE_ENV_COLOR: ", envColor);
    _mali_sys_fprintf(fp,    "    TEXTURE_ENV_MODE:  %s\n", vts(envMode, vtsEnvMode));
    _mali_sys_fprintf(fp,    "    COORD_REPLACE_OES: %s\n", (coordReplace == GL_TRUE)?"ON":"OFF");
}

void print_texenv_state(mali_file* fp)
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
            _mali_sys_fprintf(fp, "  TEXTURE%d:\n", i);
            print_texenv_state_single(fp);
        }
    }
    MALI_GLES_NAME_WRAP(glActiveTexture)(activeTexUnit);
    _mali_sys_fprintf(fp, "\n");
}

void print_gles1_state_frag(mali_file* fp)
{
    gles_context *ctx = _gles_get_context();
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->state.api.gles1);
    _gles_share_lists_unlock( ctx->share_lists );

    /*print_transform_state(fp);*/
    print_texenv_state(fp);
    /*print_lighting_state(fp);
    print_pointsize_state(fp);
    print_fog_state(fp);
    print_clipplane_state(fp);*/
    _mali_sys_fprintf(fp, "\n");    

	_gles_share_lists_lock( ctx->share_lists );
}

void print_texture_format(mali_file* fp, unsigned unitIndex)
{
    struct valueToStrStruct vtsFormat[] = 
    { 
        { GL_ALPHA,                 "ALPHA" },
		{ GL_LUMINANCE,             "LUMINANCE" },
        { GL_DEPTH_COMPONENT,       "DEPTH_COMPONENT" },
        { GL_DEPTH_STENCIL_OES,     "DEPTH_STENCIL_OES" },
        { GL_RGB,                   "RGB" },
        { GL_ETC1_RGB8_OES,         "ETC1_RGB8_OES" },
        { GL_PALETTE4_RGB8_OES,     "PALETTE4_RGB8_OES" },
        { GL_PALETTE4_R5_G6_B5_OES, "PALETTE4_R5_G6_B5_OES" },
        { GL_PALETTE8_RGB8_OES,     "PALETTE8_RGB8_OES" },
        { GL_PALETTE8_R5_G6_B5_OES, "PALETTE8_R5_G6_B5_OES" },
        { GL_LUMINANCE_ALPHA,       "LUMINANCE_ALPHA" },
        { GL_PALETTE4_RGBA8_OES,    "PALETTE4_RGBA8_OES" },
        { GL_PALETTE4_RGBA4_OES,    "PALETTE4_RGBA4_OES" },
        { GL_PALETTE4_RGB5_A1_OES,  "PALETTE4_RGB5_A1_OES" },
        { GL_PALETTE8_RGBA8_OES,    "PALETTE8_RGBA8_OES" },
        { GL_PALETTE8_RGBA4_OES,    "PALETTE8_RGBA4_OES" },
        { GL_PALETTE8_RGB5_A1_OES,  "PALETTE8_RGB5_A1_OES" },
        { GL_RGBA,                  "RGBA" },
        { 0,                        "UNKNOWN" },
    };
    gles_context *ctx = _gles_get_context();
	gles_state *glstate;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->sg_ctx);

	glstate = &ctx->state;

    {
        const gles_texture_unit  *unit   = &glstate->common.texture_env.unit[unitIndex];
        GLenum format = unit->current_texture_object[GLES_TEXTURE_TARGET_2D]->mipchains[0]->levels[0]->format;
        _mali_sys_fprintf(fp,    "  INTERNAL_FORMAT:   %s\n", vts(format, vtsFormat));
    }
}

void print_instructions_as_hex(mali_file* fp, unsigned* ptr, int size)
{
    unsigned int offs = 0;
    while(size >= 4)
    {
        /* print one 32-bit word */
        unsigned int dataword = ptr[offs++];
        _mali_sys_fprintf(fp, "%08X", dataword);
        if(size == 4)
            _mali_sys_fprintf(fp, "\n");
        else if((offs & 3) == 0)
            _mali_sys_fprintf(fp, "\n    ");
        else
            _mali_sys_fprintf(fp, " ");
        size -= 4;
    }
}

void print_texture_stage_component(mali_file* fp, unsigned* operands, unsigned* sources, unsigned combiner, unsigned scale)
{
    const char* strArg[] = 
    {
        "DISABLE",
        "CONSTANT_0",
        "CONSTANT_1",
        "CONSTANT_2",
        "CONSTANT_3",
        "CONSTANT_4",
        "CONSTANT_5",
        "CONSTANT_6",
        "CONSTANT_7",
        "CONSTANT_COLOR",
        "PRIMARY_COLOR",
        "SPECULAR_COLOR",
        "TEXTURE_0",
        "TEXTURE_1",
        "TEXTURE_2",
        "TEXTURE_3",
        "TEXTURE_4",
        "TEXTURE_5",
        "TEXTURE_6",
        "TEXTURE_7",
        "INPUT_COLOR",
        "STAGE_RESULT_0",
        "STAGE_RESULT_1",
        "STAGE_RESULT_2",
        "STAGE_RESULT_3",
        "STAGE_RESULT_4",
        "STAGE_RESULT_5",
        "STAGE_RESULT_6",
        "STAGE_RESULT_7",
        "PREVIOUS_STAGE_RESULT",
        "PREVIOUS_STAGE_RESULT",
        "DIFFUSE_COLOR = PRIMARY_COLOR",
        "TFACTOR = CONSTANT_0"
    };

    const char* strOperand[] =  { "SRC_COLOR", "ONE_MINUS_SRC_COLOR", "SRC_ALPHA", "ONE_MINUS_SRC_ALPHA", "SRC", "ONE_MINUS_SRC" };
    const char* strCombiner[] = { "REPLACE", "MODULATE", "ADD", "ADD_SIGNED", "INTERPOLATE", "SUBTRACT", "DOT3_RGB", "DOT3_RGBA" };
    const char* strScale[] =    { "ONE", "TWO", "FOUR" };
    unsigned  i;

    for(i = 0; i < 3; i++)
    {
        _mali_sys_fprintf(fp, "    SOURCE%d (ARG,OPERAND): (%s,%s)\n", i, strArg[sources[i]], strOperand[operands[i]]);
    }
    _mali_sys_fprintf(fp,     "    COMBINER:               %s\n", strCombiner[combiner]);
    _mali_sys_fprintf(fp,     "    SCALE:                  %s\n", strScale[scale]);
}

int stage_components_equal(const fragment_shadergen_state *state, unsigned stage)
{
    unsigned j;

    for(j = 0; j < 3; j++)
    {
        if( fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_OPERAND(stage, j)) != 
            fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_OPERAND(stage, j)))
            return 0;

        if( fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stage, j)) !=
            fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stage, j)))
            return 0;
    }
    if( fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_COMBINER(stage)) !=
        fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_COMBINER(stage)))
        return 0;
    if( fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_SCALE(stage)) !=
        fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SCALE(stage)))
        return 0;
    return 1;
}

void print_texture_stage(mali_file* fp, const fragment_shadergen_state *state, unsigned stage)
{
    const char* strTexturing[] = { "TEXTURE_2D", "TEXTURE_2D_PROJ_W", "TEXTURE_EXTERNAL", "TEXTURE_EXTERNAL_PROJ_W", "TEXTURE_CUBE" };
    unsigned operandsRGB[3], sourcesRGB[3], combinerRGB, scaleRGB, operandsA[3], sourcesA[3], combinerA, scaleA, enabledRGB, enabledA;
    unsigned j;
    enabledRGB = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_ENABLE(stage));
    enabledA = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_ENABLE(stage));
    _mali_sys_fprintf(fp, "TEXTURE STAGE%d:\n", stage);
    print_texture_format(fp, stage);
    _mali_sys_fprintf(fp, "  DIMENSIONALITY:    %s\n", strTexturing[fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_TEXTURE_DIMENSIONALITY(stage))]);
    _mali_sys_fprintf(fp, "  POINTCOORD:        %s\n", fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_TEXTURE_POINT_COORDINATE(stage))?"ON":"OFF");
    for(j = 0; j < 3; j++)
    {
        sourcesRGB[j] = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_SOURCE(stage, j));
        operandsRGB[j] = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_OPERAND(stage, j));
        sourcesA[j] = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SOURCE(stage, j));
        operandsA[j] = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_OPERAND(stage, j));
    }
    combinerRGB = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_COMBINER(stage));
    scaleRGB = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_SCALE(stage));
    combinerA = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_COMBINER(stage));
    scaleA = fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_SCALE(stage));

    if( enabledRGB && enabledA && stage_components_equal(state, stage))
    {
        _mali_sys_fprintf(fp, "  RGBA stage:\n");
        print_texture_stage_component(fp, operandsRGB, sourcesRGB, combinerRGB, scaleRGB);
    }
    else if(enabledRGB)
    {
        _mali_sys_fprintf(fp, "  RGB stage:\n");
        print_texture_stage_component(fp, operandsRGB, sourcesRGB, combinerRGB, scaleRGB);
    }
    else if(enabledA)
    {
        _mali_sys_fprintf(fp, "  ALPHA stage:\n");
        print_texture_stage_component(fp, operandsA, sourcesA, combinerA, scaleA);
    }

    _mali_sys_fprintf(fp, "\n");
}

void print_texture_stages(mali_file* fp, const fragment_shadergen_state *state)
{
    const char* strArg[] = 
    {
        "DISABLE",
        "CONSTANT_0",
        "CONSTANT_1",
        "CONSTANT_2",
        "CONSTANT_3",
        "CONSTANT_4",
        "CONSTANT_5",
        "CONSTANT_6",
        "CONSTANT_7",
        "CONSTANT_COLOR",
        "PRIMARY_COLOR",
        "SPECULAR_COLOR",
        "TEXTURE_0",
        "TEXTURE_1",
        "TEXTURE_2",
        "TEXTURE_3",
        "TEXTURE_4",
        "TEXTURE_5",
        "TEXTURE_6",
        "TEXTURE_7",
        "INPUT_COLOR",
        "STAGE_RESULT_0",
        "STAGE_RESULT_1",
        "STAGE_RESULT_2",
        "STAGE_RESULT_3",
        "STAGE_RESULT_4",
        "STAGE_RESULT_5",
        "STAGE_RESULT_6",
        "STAGE_RESULT_7",
        "PREVIOUS_STAGE_RESULT",
        "PREVIOUS_STAGE_RESULT",
        "DIFFUSE_COLOR = PRIMARY_COLOR",
        "TFACTOR = CONSTANT_0"
    };
    const char* strFog[] = { "NONE", "LINEAR", "EXP", "EXP2" };
    unsigned i;
    _mali_sys_fprintf(fp, "FLATSHADING:         %s\n", fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_FLATSHADED_COLORS)?"ON":"OFF");
    _mali_sys_fprintf(fp, "TWOSIDED LIGHTING:   %s\n", fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_TWOSIDED_LIGHTING)?"ON":"OFF");
    _mali_sys_fprintf(fp, "CLIP_PLANE0:         %s\n", fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_CLIP_PLANE_ENABLE(0))?"ON":"OFF");
    _mali_sys_fprintf(fp, "FOG:                 %s\n", strFog[fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_FOG_MODE)]);
    _mali_sys_fprintf(fp, "INPUT COLOR SOURCE:  %s\n", strArg[fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_INPUT_COLOR_SOURCE)]);
    _mali_sys_fprintf(fp, "RESULT COLOR SOURCE: %s\n", strArg[fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_RESULT_COLOR_SOURCE)]);
    for(i = 0; i < MAX_TEXTURE_STAGES; i++)
    {
        if( fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_ENABLE(i)) || 
            fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_ENABLE(i)))
        {
            print_texture_stage(fp, state, i);
        }
    }
    _mali_sys_fprintf(fp, "\n");
}

void dump_frag_shader_info(const fragment_shadergen_state *state, struct bs_shader* shader)
{
    mali_file* fp;
    char filename[256];
    int i;

    sprintf(filename, "shadergen_fs_%08X__", state->bits[0]);
    for(i = 0; i < MAX_TEXTURE_STAGES; i++)
    {
        char str[32];
        unsigned int stagebitsLo = state->bits[1+2*i];
        unsigned int stagebitsHi = state->bits[1+2*i+1];

        if( fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_RGB_ENABLE(i)) || 
            fragment_shadergen_decode(state, FRAGMENT_SHADERGEN_STAGE_ALPHA_ENABLE(i)))
        {
            sprintf(str, "_%08X%08X", stagebitsHi, stagebitsLo);
            strcat(filename, str);
        }
    }
    strcat(filename, ".txt");

    fp = _mali_sys_fopen(filename, "w");
    if(fp)
    {
        unsigned int numInstructions = 0;
        unsigned int offs = 0;
        unsigned int* ptr = (unsigned int*)shader->shader_block;
        for(offs = 0; offs*4 < shader->shader_size; numInstructions++)
        {
            offs += ptr[offs] & 0x1F;
        }

        print_gles1_state_frag(fp);
        print_texture_stages(fp, state);

        _mali_sys_fprintf(fp, "  Shader data as hex: (%u instructions)\n  BEGIN\n    ", numInstructions);
        print_instructions_as_hex(fp, (unsigned*)shader->shader_block, shader->shader_size);
        _mali_sys_fprintf(fp, "  END\n");
        _mali_sys_fclose(fp);
    }
    else
        _mali_sys_printf("ERROR: Could not open file \"%s\" for writing.\n", filename);
}

