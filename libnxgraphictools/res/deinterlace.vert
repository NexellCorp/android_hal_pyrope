/* vim:set sts=4 ts=4 noexpandtab cindent syntax=c: */
/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#version 100

precision highp float;

attribute vec4 a_v4Position;
attribute vec2 a_v2TexCoord;
uniform float u_fTexHeight;

varying vec2 v_tex0;
varying vec2 v_tex1;
varying vec2 v_tex2;
varying vec2 v_tex3;
varying vec2 v_tex4;
varying vec2 v_tex5;
varying vec2 v_offset_y;

//...T_T...
//varying float v_tex_x ;
//varying float v_tex_y0;
//varying float v_tex_y1;
//varying float v_tex_y2;
//varying float v_tex_y3;
//varying float v_tex_y4;
//varying float v_tex_y5;
//varying float v_offset_y;

	
void main()
{
	float size_of_texel = 1.0/u_fTexHeight;

	v_offset_y = a_v2TexCoord * vec2(1.0,(u_fTexHeight/2.0));

	v_tex0 = a_v2TexCoord + vec2(0.0,(size_of_texel*-2.0));
	v_tex1 = a_v2TexCoord + vec2(0.0,(size_of_texel*-1.0));
	v_tex2 = a_v2TexCoord;
	v_tex3 = a_v2TexCoord + vec2(0.0,(size_of_texel* 1.0));
	v_tex4 = a_v2TexCoord + vec2(0.0,(size_of_texel* 2.0));
	v_tex5 = a_v2TexCoord + vec2(0.0,(size_of_texel* 3.0));

	gl_Position = a_v4Position;
}

