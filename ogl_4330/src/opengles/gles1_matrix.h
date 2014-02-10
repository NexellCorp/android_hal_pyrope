/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_matrix.h
 * @brief Matrix routines for use internally in the gles-driver.
 */

#ifndef _GLES_MATRIX_H_
#define _GLES_MATRIX_H_

#include <mali_system.h>
#include "gles_ftype.h"

struct gles_state;
struct gles_context;

/**
 * @brief Loads a given matrix into the current matrix
 * @param ctx GLES context pointer
 * @param m the new matrix
 * @note This is a wrapper around the GLES-entrypoint glLoadMatrixf()
 */
void _gles1_load_matrixf(struct gles_context *ctx, const GLfloat *m);

/**
 * @brief Multiplies the current matrix with a given matrix
 * @param ctx GLES context pointer
 * @param m the matrix to multiply the current matrix with
 * @note This is a wrapper around the GLES-entrypoint glMultMatrixf()
 */
void _gles1_mult_matrixf(struct gles_context *ctx, const GLfloat *m);

/**
 * @brief Loads a given matrix into the current matrix
 * @param ctx GLES context pointer
 * @param m the new matrix
 * @note This is a wrapper around the GLES-entrypoint glLoadMatrixf()
 */
void _gles1_load_matrixx(struct gles_context *ctx, const GLfixed *m);

/**
 * @brief Multiplies the current matrix with a given matrix
 * @param ctx GLES context pointer
 * @param m the matrix to multiply the current matrix with
 * @note This is a wrapper around the GLES-entrypoint glMultMatrixf()
 */
void _gles1_mult_matrixx(struct gles_context *ctx, const GLfixed *m);

/**
 * @brief Query the matrix
 * @param ctx GLES context pointer
 * @param mantissa The array to return the mantissas in
 * @param exponent The array to return the exponents in
 * @return A value indicating if we support overflows etc. (which we don't ;))
 * @note This is a wrapper around the GLES-entrypoint glQueryMatrixOES()
 */
GLbitfield _gles1_query_matrixx(struct gles_context *ctx, GLfixed *mantissa, GLint *exponent) MALI_CHECK_RESULT;

/**
 * @brief Push the current matrix-stack
 * @param ctx GLES context pointer
 * @return An errorcode.
 * @note This is a wrapper around the GLES-entrypoint glPushMatrix()
*/
GLenum _gles1_push_matrix(struct gles_context *ctx) MALI_CHECK_RESULT;

/**
 * @brief Pops the current matrix-stack
 * @param ctx GLES context pointer
 * @return An errorcode.
 * @note This is a wrapper around the GLES-entrypoint glPopMatrix()
*/
GLenum _gles1_pop_matrix(struct gles_context *ctx) MALI_CHECK_RESULT;

/**
 * @brief Loads an identity-matrix into the current matrix
 * @param ctx GLES context pointer
 * @note This is a wrapper around the GLES-entrypoint glLoadIdentity()
 */
void _gles1_load_identity(struct gles_context *ctx);

/**
 * @brief Multiplies the current matrix with a rotation matrix
 * @param ctx GLES context pointer
 * @param angle the angle (in degrees) to rotate
 * @param x the x-component of the axis to rotate about
 * @param y the y-component of the axis to rotate about
 * @param z the z-component of the axis to rotate about
 * @note This is a wrapper around the GLES-entrypoint glRotatef/x().
 */
void _gles1_rotate(struct gles_context *ctx, GLftype angle, GLftype x, GLftype y, GLftype z);


/**
 * @brief Multiplies the current matrix with a translation matrix
 * @param ctx GLES context pointer
 * @param x the translation in the x-axis
 * @param y the translation in the y-axis
 * @param z the translation in the z-axis
 * @note This is a wrapper around the GLES-entrypoint glTranslatef/x().
 */
void _gles1_translate(struct gles_context *ctx, GLftype x, GLftype y, GLftype z);

/**
 * @brief Multiplies the current matrix with a scale matrix
 * @param ctx GLES context pointer
 * @param x the scale of the x-axis
 * @param y the scale of the y-axis
 * @param z the scale of the z-axis
 * @note This is a wrapper around the GLES-entrypoint glScalef/x().
 */
void _gles1_scale(struct gles_context *ctx, GLftype x, GLftype y, GLftype z);

/**
 * @brief Multiplies the current matrix with a frustum matrix
 * @param ctx GLES context pointer
 * @param left the distance to the left plane
 * @param right the distance to the right plane
 * @param bottom the distance to the bottom plane
 * @param top the distance to the top plane
 * @param near the distance to the near plane
 * @param far the distance to the far plane
 * @note This is a wrapper around the GLES-entrypoint glFrustumf/x()
 */
GLenum _gles1_frustum(struct gles_context *ctx, GLftype l, GLftype r, GLftype b, GLftype t, GLftype n, GLftype f) MALI_CHECK_RESULT;

/**
 * @brief Multiplies the current matrix with an ortho matrix
 * @param ctx GLES context pointer
 * @param left the distance to the left plane
 * @param right the distance to the right plane
 * @param bottom the distance to the bottom plane
 * @param top the distance to the top plane
 * @param near the distance to the near plane
 * @param far the distance to the far plane
 * @note This is a wrapper around the GLES-entrypoint glOrthof/x()
 */
GLenum  _gles1_ortho(struct gles_context *ctx, GLftype l, GLftype r, GLftype b, GLftype t, GLftype n, GLftype f) MALI_CHECK_RESULT;

#endif /* _GLES_MATRIX_H_ */
