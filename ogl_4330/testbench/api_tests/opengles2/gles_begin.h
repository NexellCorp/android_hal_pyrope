/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef GLES_BEGIN_H
#define GLES_BEGIN_H

#ifdef __cplusplus
extern "C" {
#endif

void glesBegin(GLenum mode);
void glesEnd(void);

void glesVertex2f(GLfloat x, GLfloat y);
void glesVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glesVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);

void glesColor3f(GLfloat r, GLfloat g, GLfloat b);
void glesColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

#ifdef __cplusplus
}
#endif

#endif /* GLES_BEGIN_H */
