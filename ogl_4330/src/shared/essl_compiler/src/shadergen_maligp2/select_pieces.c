/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "shadergen_state.h"
#include "shadergen_maligp2/shader_pieces.h"
#include "shadergen_maligp2/select_pieces.h"


static const instruction instruction_three_cycle_wait[] ={
	{ { 0x42042108, 0x03800108, 0x0007ffe0, 0x00042100 } },
	{ { 0x42042108, 0x03800108, 0x0007ffe0, 0x00042100 } },
	{ { 0x42042108, 0x03800108, 0x0007ffe0, 0x00042100 } }
};
static const piece piece_three_cycle_wait = 
{	
	1338, 
	3, 
	instruction_three_cycle_wait 
};

essl_bool _vertex_shadergen_select_pieces(const struct vertex_shadergen_state* state, piece_select *out, unsigned *n_pieces) {
	unsigned features;
	int i, j;
	int out_index = 0;

#define MALI_OUT(_mmask, _piece) \
  do { \
    const piece *p = _piece; \
    if (p == 0) return 0; \
    if (p->len != 0) { \
      assert(out_index < MAX_SELECTED_PIECES); \
      out[out_index].merge_fields = _mmask; \
      out[out_index].piece = p; \
      out_index++; \
    } \
  } while (0)
#define WAIT3() MALI_OUT(INSTRUCTION_MASK_NONE, &piece_three_cycle_wait)

#define EYESPACEPOSBIT (_VERTEX_FEATURE_TRANSFORM_EYESPACEPOS_MASK << _VERTEX_FEATURE_TRANSFORM_EYESPACEPOS_OFFSET)
#define EYESPACENORMALBIT (_VERTEX_FEATURE_TRANSFORM_EYESPACENORMAL_MASK << _VERTEX_FEATURE_TRANSFORM_EYESPACENORMAL_OFFSET)

	/* Transformation phase.
	 * Eyespace pos and normal are derived from the other bits rather than set by the GLES driver.
	 */
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

	for( i = 0; i < MAX_TEXTURE_STAGES ; i++) 
	{
		if( vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_TEXGEN_ENABLE(i)))
		{
			features |= EYESPACEPOSBIT | EYESPACENORMALBIT;
			break;
		}
	}

	MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_piece(VERTEX_PHASE_TRANSFORM, features));
	if(features & (EYESPACEPOSBIT | EYESPACENORMALBIT))
	{
		WAIT3();
	}

	/* Texture coordinate transformation */

	j = 0;
	for (i = 0 ; i < MAX_TEXTURE_STAGES ; i++) {
		int texgen_enable;
		if(vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_ENABLE(i)) == 0) continue;
		texgen_enable = vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_TEXGEN_ENABLE(i));
		if(texgen_enable)
		{
			/* if we have mipgen, texcoords are generated. No need to load them */
			switch( vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_TEXGEN_MODE(i)) )
			{
				case TEXGEN_REFLECTIONMAP:
					MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_indexed_piece(VERTEX_PHASE_TEXGENREFLECTION, 0, i));
					break;
				case TEXGEN_NORMALMAP:
					MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_indexed_piece(VERTEX_PHASE_TEXGENNORMAL, 0, i));
					break;
			}
			WAIT3();
		}
		
		{
			switch (vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_TRANSFORM(i))) {
				case TEX_IDENTITY:
					/* Simple copy */
					if(!texgen_enable)
					{
						MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_indexed_piece(VERTEX_PHASE_TEXCOORDCOPY, 0, i));
					} else {
						MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_indexed_piece(VERTEX_PHASE_TEXCOORDWRITE, 0, i));
					}
				break;
				case TEX_TRANSFORM:
					/* Copy into array */
					if(!texgen_enable)
					{
						MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_indexed_piece(VERTEX_PHASE_TEXCOORDREAD, 0, i));
						MALI_OUT(INSTRUCTION_MASK_INPUT_REG, _piecegen_get_indexed_piece(VERTEX_PHASE_TEXCOORDREAD, 0, i));
					}
					j++;

				break;
				default:
					return 0;
			}
	
		}
	}

	if (j > 0) {
		/* Transformation is enabled */
		WAIT3();
		MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_piece(VERTEX_PHASE_TEXCOORDTRANSFORM, 0));
		WAIT3();

		j = 0;
		for (i = 0 ; i < MAX_TEXTURE_STAGES ; i++) {
			texcoord_mode transform;
			if(vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_ENABLE(i)) == 0) continue;
			transform = vertex_shadergen_decode(state, VERTEX_SHADERGEN_STAGE_TRANSFORM(i));
			if (TEX_TRANSFORM == transform) {
				/* Copy out of array */
				MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_indexed_piece(VERTEX_PHASE_TEXCOORDWRITE, 0, i));
				MALI_OUT(INSTRUCTION_MASK_CONSTANT_REG, _piecegen_get_indexed_piece(VERTEX_PHASE_TEXCOORDWRITE, 0, i));
			} else {
				/* In TEX_IDENTITY case, it is copied above */
				assert(TEX_IDENTITY == transform);
			}
		}
	}



	/* Lighting */
	features = vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_LIGHTING);
	MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_piece(VERTEX_PHASE_LIGHTING, features));

	/* Point size */
	features = vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_POINTSIZE);
	MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_piece(VERTEX_PHASE_POINTSIZE, features));

	/* Fog */
	features = vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_FOG);
	MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_piece(VERTEX_PHASE_FOG, features));

	/* Clip plane */
	features = vertex_shadergen_decode(state, VERTEX_SHADERGEN_FEATURES_CLIPPLANE);
	MALI_OUT(INSTRUCTION_MASK_NONE, _piecegen_get_piece(VERTEX_PHASE_CLIPPLANE, features));

	assert(out_index <= MAX_SELECTED_PIECES);
	*n_pieces = out_index;
	return 1;
}

