/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <util/mali_float_matrix4x4.h>

void __mali_float_matrix4x4_make_identity(mali_float_matrix4x4 dst)
{
	dst[0][0] = 1.f; dst[0][1] = 0.f; dst[0][2] = 0.f; dst[0][3] = 0.f;
	dst[1][0] = 0.f; dst[1][1] = 1.f; dst[1][2] = 0.f; dst[1][3] = 0.f;
	dst[2][0] = 0.f; dst[2][1] = 0.f; dst[2][2] = 1.f; dst[2][3] = 0.f;
	dst[3][0] = 0.f; dst[3][1] = 0.f; dst[3][2] = 0.f; dst[3][3] = 1.f;
}

void __mali_float_matrix4x4_make_frustum(mali_float_matrix4x4 dst, float left, float right, float bottom, float top, float near, float far)
{
	float wr = 1.f / (right - left); /* reciprocal width */
	float hr = 1.f / (top - bottom); /* reciprocal height */
	float lr = 1.f / (far - near);   /* reciprocal length */

	dst[0][0] = 2.f * near * wr;     dst[0][1] = 0.f;                 dst[0][2] = 0.f;                      dst[0][3] =  0.f;
	dst[1][0] = 0.f;                 dst[1][1] = 2.f * near * hr;     dst[1][2] = 0.f;                      dst[1][3] =  0.f;
	dst[2][0] = (right + left) * wr; dst[2][1] = (top + bottom) * hr; dst[2][2] = -(far + near) * lr;       dst[2][3] = -1.f;
	dst[3][0] = 0.f;                 dst[3][1] = 0.f;                 dst[3][2] = -(2.f * far * near) * lr; dst[3][3] =  0.f;
}

void __mali_float_matrix4x4_make_ortho(mali_float_matrix4x4 dst, float left, float right, float bottom, float top, float near, float far)
{
	float wr = 1.f / (right - left); /* reciprocal width */
	float hr = 1.f / (top - bottom); /* reciprocal height */
	float lr = 1.f / (far - near);   /* reciprocal length */

	dst[0][0] = 2.f * wr;             dst[0][1] = 0.f;                  dst[0][2] = 0.f;                dst[0][3] =  0.f;
	dst[1][0] = 0.f;                  dst[1][1] = 2.f * hr;             dst[1][2] = 0.f;                dst[1][3] =  0.f;
	dst[2][0] = 0.f;                  dst[2][1] = 0.f;                  dst[2][2] = -2.f * lr;          dst[2][3] =  0.f;
	dst[3][0] = -(right + left) * wr; dst[3][1] = -(top + bottom) * hr; dst[3][2] = -(far + near) * lr; dst[3][3] =  1.f;
}

void __mali_float_matrix4x4_multiply(mali_float_matrix4x4 dst, mali_float_matrix4x4 src1, const mali_float_matrix4x4 src2)
{
	u32 i;

	/* dst and src1 should be aliasable, but not src1 and src2 or dst and src2 */
	MALI_DEBUG_ASSERT((void*)src1 != (void*)src2, ("same pointer (%p) given for src1 and scr2!", (void*)src1));
	MALI_DEBUG_ASSERT((void*)dst != (void*)src2, ("same pointer (%p) given for dst and scr2!", (void*)dst));

	for (i = 0; i < 4; ++i)
	{
		mali_float s0 = src1[0][i],
		           s1 = src1[1][i],
		           s2 = src1[2][i],
		           s3 = src1[3][i];

		/* floating point matrix multiply */
		dst[0][i] = s0 * src2[0][0] + s1 * src2[0][1] + s2 * src2[0][2] + s3 * src2[0][3];
		dst[1][i] = s0 * src2[1][0] + s1 * src2[1][1] + s2 * src2[1][2] + s3 * src2[1][3];
		dst[2][i] = s0 * src2[2][0] + s1 * src2[2][1] + s2 * src2[2][2] + s3 * src2[2][3];
		dst[3][i] = s0 * src2[3][0] + s1 * src2[3][1] + s2 * src2[3][2] + s3 * src2[3][3];
	}
}

void __mali_float_matrix4x4_copy(mali_float_matrix4x4 dst, mali_float_matrix4x4 src)
{
	_mali_sys_memcpy_32((void *)dst, (const void*)src, sizeof(mali_float_matrix4x4));
}

void __mali_float_matrix4x4_swap_rows(mali_float_matrix4x4 dst, int r1, int r2)
{
	int i;

	MALI_DEBUG_ASSERT_RANGE(r1, 0, 3);
	MALI_DEBUG_ASSERT_RANGE(r2, 0, 3);

	for (i = 0; i < 4; ++i)
	{
		float temp = dst[r1][i];
		dst[r1][i] = dst[r2][i];
		dst[r2][i] = temp;
	}
}

mali_err_code __mali_float_matrix4x4_invert_gauss(mali_float_matrix4x4 dst)
{
	int i, j, k;

	int maxrow;
	float maxval;
	float rcp_maxval;
	float val;

	mali_float_matrix4x4 tmp;
	__mali_float_matrix4x4_make_identity(tmp);

	maxval = dst[0][0];
	maxrow = 0;

	for (i = 0; i < 4; ++i)
	{
		maxval = dst[i][i];
		maxrow = i;

		/* find pivot */
		for (j = i + 1; j < 4; ++j)
		{
			val = dst[j][i];
			if (_mali_sys_fabs(val) > _mali_sys_fabs(maxval))
			{
				maxval = val;
				maxrow = j;
			}
		}

		if (maxrow != i)
		{
			__mali_float_matrix4x4_swap_rows(dst, i, maxrow);
			__mali_float_matrix4x4_swap_rows(tmp, i, maxrow);
		}

		/* singular? */
		MALI_CHECK(_mali_sys_fabs(dst[i][i]) >= 1e-15, MALI_ERR_FUNCTION_FAILED);
		MALI_DEBUG_ASSERT(_mali_sys_fabs(maxval) > 1e-15, ("maxval out of range, singular matrix?"));

		/* eliminate row */
		rcp_maxval = 1.0f / maxval;

		for (k = 0; k < 4; ++k)
		{
			tmp[i][k] *= rcp_maxval;
			dst[i][k] *= rcp_maxval;
		}

		for (j = i + 1; j < 4; ++j)
		{
			val = dst[j][i];
			for (k = 0; k < 4; ++k)
			{
				dst[j][k] -= dst[i][k] * val;
				tmp[j][k] -= tmp[i][k] * val;
			}
		}
	}

	/* back substitute */
	for (i = 3; i >= 0; --i)
	{
		for(j = i - 1; j >= 0; --j)
		{
			val = dst[j][i];

			for (k = 0; k < 4; ++k)
			{
				dst[j][k] -= dst[i][k] * val;
				tmp[j][k] -= tmp[i][k] * val;
			}
		}
	}

	__mali_float_matrix4x4_copy(dst, tmp);
	MALI_SUCCESS;
}


/* this is faster than Gauss-Jordan algorithm */
mali_err_code __mali_float_matrix4x4_invert_partitioning(mali_float_matrix4x4 dst)
{
     /* P - (dst[0][0] dst[0][1] ; dst[1][0] dst[1][1] )  */
     const mali_float detP = dst[0][0]*dst[1][1] - dst[0][1]*dst[1][0];

     /* degenerate case */
     if (detP == 0)
     {
       return __mali_float_matrix4x4_invert_gauss(dst);
     }

     /* main algorithm*/
     {
        /* P matrix*/
        const mali_float Pa = dst[0][0];
        const mali_float Pb = dst[0][1];
        const mali_float Pc = dst[1][0];
        const mali_float Pd = dst[1][1];

        /* Q matrix */
        const mali_float Qa = dst[0][2];
        const mali_float Qb = dst[0][3];
        const mali_float Qc = dst[1][2];
        const mali_float Qd = dst[1][3];

        /* R matrix */
        const mali_float Ra = dst[2][0];
        const mali_float Rb = dst[2][1];
        const mali_float Rc = dst[3][0];
        const mali_float Rd = dst[3][1];

        /* S matrix */
        const mali_float Sa = dst[2][2];
        const mali_float Sb = dst[2][3];
        const mali_float Sc = dst[3][2];
        const mali_float Sd = dst[3][3];

        /* inv(P) */
        const mali_float Pia = Pd / detP;
        const mali_float Pib = -Pb / detP;
        const mali_float Pic = -Pc / detP;
        const mali_float Pid =  Pa / detP;

        /* R*inv(P) */
        const mali_float RPia = Ra*Pia + Rb*Pic;
        const mali_float RPib = Ra*Pib + Rb*Pid;
        const mali_float RPic = Rc*Pia + Rd*Pic;
        const mali_float RPid = Rc*Pib + Rd*Pid;

        /* inv(P)*Q */
        const mali_float PiQa = Pia*Qa + Pib*Qc;
        const mali_float PiQb = Pia*Qb + Pib*Qd;
        const mali_float PiQc = Pic*Qa + Pid*Qc;
        const mali_float PiQd = Pic*Qb + Pid*Qd;


        /* R*inv(P)*Q */
        const mali_float RPiQa = Ra*PiQa + Rb*PiQc;
        const mali_float RPiQb = Ra*PiQb + Rb*PiQd;
        const mali_float RPiQc = Rc*PiQa + Rd*PiQc;
        const mali_float RPiQd = Rc*PiQb + Rd*PiQd;

        /* S - R*inv(P)*Q */
        const mali_float SSa = Sa - RPiQa;
        const mali_float SSb = Sb - RPiQb;
        const mali_float SSc = Sc - RPiQc;
        const mali_float SSd = Sd - RPiQd;

        const mali_float detSS = SSa*SSd - SSb*SSc;

        if (detSS == 0)
        {
           return __mali_float_matrix4x4_invert_gauss(dst);
        }

       {
          /* inv(S_) */
          const mali_float invSa = SSd / detSS; 
          const mali_float invSb = -SSb / detSS; 
          const mali_float invSc = -SSc / detSS; 
          const mali_float invSd = SSa / detSS; 

          /* -inv(S_)*R*inv(P) */
          const mali_float invRa = -(invSa*RPia + invSb*RPic); 
          const mali_float invRb = -(invSa*RPib + invSb*RPid); 
          const mali_float invRc = -(invSc*RPia + invSd*RPic); 
          const mali_float invRd = -(invSc*RPib + invSd*RPid); 

          /* -inv(P)*Q*inv(S_) */
          const mali_float invQa = -(PiQa*invSa + PiQb*invSc); 
          const mali_float invQb = -(PiQa*invSb + PiQb*invSd); 
          const mali_float invQc = -(PiQc*invSa + PiQd*invSc); 
          const mali_float invQd = -(PiQc*invSb + PiQd*invSd); 

          /* -inv(P) - inv(P)*Q*invR */
          const mali_float invPa = Pia - (PiQa*invRa + PiQb*invRc); 
          const mali_float invPb = Pib - (PiQa*invRb + PiQb*invRd); 
          const mali_float invPc = Pic - (PiQc*invRa + PiQd*invRc); 
          const mali_float invPd = Pid - (PiQc*invRb + PiQd*invRd); 

         /* Store inv(P) */
          dst[0][0] = invPa; dst[0][1] = invPb;         
          dst[1][0] = invPc; dst[1][1] = invPd;         

         /* Store inv(Q) */
          dst[0][2] = invQa; dst[0][3] = invQb;         
          dst[1][2] = invQc; dst[1][3] = invQd;         

         /* Store inv(R) */
          dst[2][0] = invRa; dst[2][1] = invRb;         
          dst[3][0] = invRc; dst[3][1] = invRd;         

         /* Store inv(S) */
          dst[2][2] = invSa; dst[2][3] = invSb;         
          dst[3][2] = invSc; dst[3][3] = invSd;         

       }
        
     }
       
     MALI_SUCCESS;
}


mali_err_code __mali_float_matrix4x4_invert(mali_float_matrix4x4 dst, mali_float_matrix4x4 src)
{
	if (&dst[0][0] != &src[0][0])
	{
		__mali_float_matrix4x4_copy(dst, src);
		return __mali_float_matrix4x4_invert_partitioning(dst);
	}
	else
	{
		return __mali_float_matrix4x4_invert_partitioning(dst);
	}
}

void __mali_float_matrix4x4_transpose(mali_float_matrix4x4 dst, mali_float_matrix4x4 src)
{
	u32 i, j;

	if (&dst[0][0] == &src[0][0])
	{
		/* swap elements under the diagonal with those above */
		for (i = 0; i < 4; ++i)
		{
			for (j = 0; j < i; ++j)
			{
				mali_float temp = dst[i][j];
				dst[i][j] = dst[j][i];
				dst[j][i] = temp;
			}
		}
	}
	else
	{
		/* copy the matrix and flip in the same pass */
		for (i = 0; i < 4; ++i)
		{
			for (j = 0; j < 4; ++j)
			{
				dst[i][j] = src[j][i];
			}
		}
	}
}

