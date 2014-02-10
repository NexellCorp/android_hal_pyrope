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

precision mediump float;
//precision highp float;

uniform float u_fTexHeight;
uniform sampler2D diffuse;
uniform sampler2D ref_tex;
uniform sampler2D filter_frac_tex;

varying vec2 v_tex0;
varying vec2 v_tex1;
varying vec2 v_tex2;
varying vec2 v_tex3;
varying vec2 v_tex4;
varying vec2 v_tex5;
varying vec2 v_offset_y;

void main()
{
	vec4 tval0 = vec4(0.0, 0.0, 0.0, 0.0), tval1 = vec4(0.0, 0.0, 0.0, 0.0);

	//	 deinterface without scaling	
	vec4 y0_frac = texture2D(ref_tex, v_offset_y);
	if( y0_frac.x == 1.0 )
	{
		tval0 += texture2D(diffuse, v_tex0) * (-1.0/8.0);
		tval0 += texture2D(diffuse, v_tex1) * ( 4.0/8.0);
		tval0 += texture2D(diffuse, v_tex2) * ( 2.0/8.0);
		tval0 += texture2D(diffuse, v_tex3) * ( 4.0/8.0);
		tval0 += texture2D(diffuse, v_tex4) * (-1.0/8.0);
	}
	else //even
	{
		tval0 = texture2D(diffuse, v_tex2);
	}

	gl_FragColor = tval0;

	//for debugging
	//gl_FragColor = texture2D(diffuse, v_v2TexCoord.xy);
}














/*

void main()
{
	vec4 tval0 = vec4(0.0, 0.0, 0.0, 0.0), tval1 = vec4(0.0, 0.0, 0.0, 0.0);
	vec2 tcoord;
	float tex_pos_y, fract_temp;
	
	tcoord.x = v_v2TexCoord.x;
	tex_pos_y = v_v2TexCoord.y * u_fTexHeight;
	
	if(gl_FragCoord.y==1.0)
    {
    	tcoord.y = v_v2TexCoord.y - 1.0; 
        tval0 += texture2D(diffuse, tcoord.xy) * -1.0;
        tval0 += texture2D(diffuse, tcoord.xy) * 4.0;
        tcoord.y = v_v2TexCoord.y; 
        tval0 += texture2D(diffuse, tcoord.xy) * 2.0;
        tcoord.y = v_v2TexCoord.y + 1.0; 
        tval0 += texture2D(diffuse, tcoord.xy) * 4.0;
        tcoord.y = v_v2TexCoord.y + 2.0; 
        tval0 += texture2D(diffuse, tcoord.xy) * -1.0;
        tval0 = tval0 / 8.0;		
    }
    else
    {
	    if (gl_FragCoord.y==(u_fDestHeight - 2.0))
	    {
	    	tcoord.y = v_v2TexCoord.y - 2.0; 
	        tval0 += texture2D(diffuse, tcoord.xy) * -1.0;
	        tcoord.y = v_v2TexCoord.y - 1.0; 
	        tval0 += texture2D(diffuse, tcoord.xy) * 4.0;
	        tcoord.y = v_v2TexCoord.y; 
	        tval0 += texture2D(diffuse, tcoord.xy) * 2.0;
	        tcoord.y = v_v2TexCoord.y + 1.0; 
	        tval0 += texture2D(diffuse, tcoord.xy) * 4.0;
	        tval0 += texture2D(diffuse, tcoord.xy) * -1.0;
	        tval0 = tval0 / 8.0;			
		}
	    else 
	    {
	        if(gl_FragCoord.y == 0.0 || gl_FragCoord.y== (u_fDestHeight - 1.0)) //start, end line
	        {
		        tcoord.y = v_v2TexCoord.y; 
	            tval0= texture2D(diffuse, tcoord.xy);
	        }
			else if(gl_FragCoord.y== (u_fDestHeight - 3.0))
			{
				tcoord.y = v_v2TexCoord.y - 2.0; 
				tval0 += texture2D(diffuse, tcoord.xy) * -1.0;
				tcoord.y = v_v2TexCoord.y - 1.0; 
				tval0 += texture2D(diffuse, tcoord.xy) * 4.0;
				tcoord.y = v_v2TexCoord.y; 
				tval0 += texture2D(diffuse, tcoord.xy) * 2.0;
				tcoord.y = v_v2TexCoord.y + 1.0; 
				tval0 += texture2D(diffuse, tcoord.xy) * 4.0;
				tcoord.y = v_v2TexCoord.y + 2.0; 
				tval0 += texture2D(diffuse, tcoord.xy) * -1.0;
				tval0 = tval0 / 8.0;
			}				
	        else //bilinear filter
	        {	        	
				//if(0.0 != floor((mod(floor(gl_FragCoord.y), 2.0)))) //odd
				vec4 temp_texval = texture2D(ref_tex, v_v2TexCoord2);
				if( temp_texval.x == 1.0 )
				{
		        	tcoord.y = v_v2TexCoord.y - 2.0; 
		            tval0 += texture2D(diffuse, tcoord.xy) * -1.0;
		            tcoord.y = v_v2TexCoord.y - 1.0; 
		            tval0 += texture2D(diffuse, tcoord.xy) * 4.0;
		            tcoord.y = v_v2TexCoord.y; 
		            tval0 += texture2D(diffuse, tcoord.xy) * 2.0;
		            tcoord.y = v_v2TexCoord.y + 1.0; 
		            tval0 += texture2D(diffuse, tcoord.xy) * 4.0;
		            tcoord.y = v_v2TexCoord.y + 2.0; 
		            tval0 += texture2D(diffuse, tcoord.xy) * -1.0;
		            tval0 = tval0 / 8.0;		
					//tval0.x = 0.0;
					//tval0.y = 0.0;
					//tval0.z = 0.0;
					//tval0.w = 0.0;
				}
				else //even
				{
					tcoord.y = v_v2TexCoord.y; 
					tval0 = texture2D(diffuse, tcoord.xy);
				}
				//tcoord.y = v_v2TexCoord.y - 1.0; 
				//tval1 += texture2D(diffuse, tcoord.xy) * -1.0;
				//tcoord.y = v_v2TexCoord.y; 
				//tval1 += texture2D(diffuse, tcoord.xy) * 4.0;
				//tcoord.y = v_v2TexCoord.y + 1.0; 
				//tval1 += texture2D(diffuse, tcoord.xy) * 2.0;
				//tcoord.y = v_v2TexCoord.y + 2.0; 
				//tval1 += texture2D(diffuse, tcoord.xy) * 4.0;
				//tcoord.y = v_v2TexCoord.y + 3.0; 
				//tval1 += texture2D(diffuse, tcoord.xy) * -1.0;
				//tval1 = tval1 / 8.0;
				//
				////ex. floor(7.1) = 7
				////ex. fract(7.1) = 7.1 - 7 = 0.1
				//fract_temp = fract(tex_pos_y);
				//tval0 = (tval0 * (1.0 - fract_temp)) + (tval1 * fract_temp);				
				} 
	    }
    }
    //if(tval0.x > 1.0) tval0.x = 1.0;
    //else if(tval0.x < 0.0) tval0.x = 0.0;    
    //if(tval0.y > 1.0) tval0.y = 1.0;
    //else if(tval0.y < 0.0) tval0.y = 0.0;
    //if(tval0.z > 1.0) tval0.z = 1.0;
    //else if(tval0.z < 0.0) tval0.z = 0.0;    
    //if(tval0.w > 1.0 ) tval0.w = 1.0;
    //else if(tval0.w < 0.0) tval0.w = 0.0;	

	gl_FragColor = tval0;

	//for debugging
	//gl_FragColor = texture2D(diffuse, v_v2TexCoord.xy);
}

*/

