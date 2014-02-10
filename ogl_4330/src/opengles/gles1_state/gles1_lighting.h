/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_lighting.h
 * @brief GLES Light-state related operations
 */
#ifndef GLES1_LIGHTING_H
#define GLES1_LIGHTING_H

#include <gles_base.h>
#include "gles1_transform.h"
#include <gles_state.h>

/* forward declaration */
struct gles_context;

/* Local definitions */
typedef enum
{
	GL_CONSTANT_ATTENUATION_INDEX=0,
	GL_LINEAR_ATTENUATION_INDEX,
	GL_QUADRATIC_ATTENUATION_INDEX,
	GL_NUM_ATTENUATION_INDICES
} gles_attenuation_indices;

/** Holds the parameters for one light */
typedef struct gles_light
{
	GLftype ambient[4];                                 /**< The ambient intensity of the light */
	GLftype diffuse[4];                                 /**< The diffuse intensity of the light */
	GLftype specular[4];                                /**< The specular intensity of the light */
	GLftype position[4];                                /**< The position of the light */
	GLftype attenuation[GL_NUM_ATTENUATION_INDICES];    /**< The attenuation of the light */
	GLftype spot_direction[3];                          /**< The direction of the spot-light */
	GLftype spot_exponent;                              /**< The shininess factor of the spot-light */
	GLftype cos_spot_cutoff;			    /**< The cosine( cutoff_angle ) of the spot-light */
} gles_light;

/** Holds all the information about the lighting-state, including all the lights */
typedef struct gles1_lighting
{
	GLboolean enabled;                          /** Is GL_LIGHTING enabled? */

	/** Material properties */
	GLftype ambient[4];                         /** The ambient coefficients for the material */
	GLftype diffuse[4];                         /** The diffuse coefficients for the material */
	GLftype specular[4];                        /** The specular coefficients for the material */
	GLftype emission[4];                        /** The emission coefficients for the material */
	GLftype shininess;                          /** The shininess factor for the material */

	/** Global ambient */
	GLftype   light_model_ambient[4];           /** The global ambient light intensity */
	gles_light light[GLES1_MAX_LIGHTS];         /** The different lighst(GL_LIGHT0, etc) */

	u8 spot_enabled_field;                      /** Bitfield saying if spot is enabled, spot_enabled & 0x2 == LIGHT1_SPOT_ENABLED */
	u8 specular_field;                          /** Bitfield saying if specular is present in shader(iow are the values useful in shader) */
	u8 n_lights;                                /** The number of lights active */
	u8 attenuation_field;                       /** Bitfield saying if attenuation is present in shader(iow are the values useful in shader) */
	u8 enabled_field;                           /** Bitfield saying which lights are enabled */

	/* Note:  these values are separate from the per-light structure
	 * to avoid polluting the D-cache with data we do not (ordinarily)
	 * need.
	 */
	GLftype spotlight_cutoff[GLES1_MAX_LIGHTS]; /**< Spotlight cutoff angles for glGet accuracy */

} gles1_lighting;


/**
 * @brief Initializes the lighting-state
 * @param state A pointer to the GLES state
 */
void _gles1_lighting_init( struct gles_state * const state );

/**
 * @brief Sets the different parameters for the material
 * @param ctx The pointer to the GLES context
 * @param face Which face we should alter the parameter for
 * @param pname Which parameter we should alter
 * @param param The new parameter value
 * @param type The data-type for the new parameter value
 * @return An errorcode.
 * @note This implements the GLES entrypoint glMaterial().
 */
GLenum _gles1_material(struct gles_context *ctx, GLenum face, GLenum pname, const GLvoid *param, const gles_datatype type) MALI_CHECK_RESULT;

/**
 * @brief Sets the different parameters for the material
 * @param ctx The pointer to the GLES context
 * @param face Which face we should alter the parameter for
 * @param pname Which parameter we should alter
 * @param params The array of new parameter values
 * @param type The data-type for the new parameter values
 * @return An errorcode.
 * @note This implements the GLES entrypoint glMaterialv().
 */
GLenum _gles1_materialv(struct gles_context *ctx, GLenum face, GLenum pname, const GLvoid *params, const gles_datatype type) MALI_CHECK_RESULT;

/**
 * @brief Sets the different parameters for the lights
 * @param ctx The pointer to the GLES context
 * @param light The identifier for which light that is to be altered
 * @param pname The identifier for which parameter that should be altered
 * @param param The new parameter
 * @param type The data-type of the parameter
 * @return An errorcode
 * @note This implements the GLES entrypoint glLight().
 */
GLenum _gles1_light(struct gles_context * ctx, GLenum light, GLenum pname, const GLvoid *param, const gles_datatype type) MALI_CHECK_RESULT;

/**
 * @brief Sets the different parameters for the lights
 * @param ctx The pointer to the GLES context
 * @param transform A pointer to the current transform structure, which is used to transform the position
 * @param light The identifier for which light that is to be altered
 * @param pname The identifier for which parameter that should be altered
 * @param params The array with the new parameter-values
 * @param type The data-type of the elements in the array
 * @return An errorcode.
 * @note This is a wrapper function of the GLES entrypoint glLightv().
 */
GLenum _gles1_lightv(struct gles_context * ctx, GLenum light, const GLenum pname, const GLvoid *params, const gles_datatype type) MALI_CHECK_RESULT;

/**
 * @brief Sets the different parameters for the light-model
 * @param ctx The pointer to the GLES context
 * @param pname identifier for which parameter is to be altered
 * @param param The new value for the parameter
 * @param type The type of the new value
 * @return An errorcode.
 * @note This implements the GLES entrypoint glLightModel().
 */
GLenum _gles1_light_model( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief Sets the different parameters for the light-model
 * @param ctx The pointer to the GLES context
 * @param pname Identifies which parameter is to be altered
 * @param params The array of new values
 * @param type The data-type of the elements in the array
 * @return An errorcode.
 * @note This implements the GLES entrypoint glLightModelv().
 */
GLenum _gles1_light_modelv( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type ) MALI_CHECK_RESULT;

/**
 * Update the twosided lighting state in the backends. This function must be called after
 *  - dis/enabling lighting
 *  - dis/enabling any light
 *  - dis/enabling two-sided lighting mode
 * @param ctx A pointer to the gles context.
 */
void _gles1_push_twosided_lighting_state(struct gles_context* ctx);

#endif /* GLES1_LIGHTING_H */
