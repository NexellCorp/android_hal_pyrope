/*
 * (C) Copyright 2010
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

/* DEBUG MACRO */
#define	DBG_ERR				(1)
#define	DBG_LIB				(0)
#define	DBG_FUNC			(0)
#define	CLEAR_MMAP_ZONE		(0)

/*	PLATFORM MACRO */
#ifdef UNDER_CE
#include <stdio.h>

#define DBGOUT				RETAILMSG
#define ERROUT				ERRORMSG

#else 	/* UNDER_CE */

/* Linux Platform */
#define	  _T(exp)			exp
#define	TEXT(exp)			exp
#define DBGOUT(cond, exp)	{ if(cond) printf exp; }
#define ERROUT(cond, exp)	{ if(cond) printf("ERROR: %s line %d: \n", __FILE__, __LINE__), printf exp; }

#endif	/* UNDER_CE */
#endif 	/* __DEBUG_H__ */

