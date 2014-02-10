//------------------------------------------------------------------------------
//
//  Copyright (C) 2013 Nexell Co. All Rights Reserved
//  Nexell Co. Proprietary & Confidential
//
//  NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//  Module      :
//  File        :
//  Description :
//  Author      : 
//  Export      :
//  History     :
//
//------------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "NX_Queue.h"
#include "NX_Semaphore.h"
#include "NX_GTService.h"

#include <nx_fourcc.h>

#include "vr_deinterlace.h"
#include "nx_graphictools.h"

#include "dbgmsg.h"

#define MAX_SERVICE_NUM		6
#define MAX_CMD_SIZE		MAX_SERVICE_NUM + 1

////////////////////////////////////////////////////////////////////////////////
//
// Debugging tools
//
//#define DEBUG

#ifdef DEBUG
#include <sys/time.h>

static uint64_t gNxStartTime   = 0;
static uint64_t gNxStopTime    = 0;

uint64_t NX_GetTickCount( void )
{
	uint64_t ret;
	struct timeval  tv;
	struct timezone zv;
	gettimeofday( &tv, &zv );
	ret = ((uint64_t)tv.tv_sec)*1000 + tv.tv_usec/1000;
	return ret;
}

#define NxDbgCheckTimeStart()       gNxStartTime = NX_GetTickCount()
#define NxDbgCheckTimeStop(A)       do {												\
										gNxStopTime = NX_GetTickCount();				\
										ErrMsg("%s Process Time : %lld msec\n",			\
										A, (uint64_t)(gNxStopTime - gNxStartTime));		\
									} while(0)
#else
#define NxDbgCheckTimeStart()		do {} while(0)
#define NxDbgCheckTimeStop(A)       do {} while(0)
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Graphic tools engine
//



////////////////////////////////////////////////////////////////////////////////
//
//	Deinterlace Service Routine
//
struct NX_GT_DEINT_INFO {
	int32_t isInit;
	int32_t srcWidth;
	int32_t srcHeight;
	int32_t dstWidth;
	int32_t dstHeight;

	int32_t maxInOutBufSize;
	int32_t isScaleMode;

	NX_VID_MEMORY_INFO inputImage[MAX_GRAPHIC_BUF_SIZE];		//	Input Image
	HDEINTERLACESOURCE hInSurface[MAX_GRAPHIC_BUF_SIZE][3];		//	Input Image Surface

	NX_VID_MEMORY_INFO outputImage[MAX_GRAPHIC_BUF_SIZE];		//	Output Image
	HDEINTERLACETARGET hOutSurface[MAX_GRAPHIC_BUF_SIZE][3];	//	Output Image Surface
};

static void *GTServiceDeintOpen( NX_GT_PARAM_OPEN *pParam )
{
	NX_GT_DEINT_HANDLE handle = (NX_GT_DEINT_HANDLE)malloc(sizeof(struct NX_GT_DEINT_INFO));

	if( handle )
	{
		memset( handle, 0, sizeof(struct NX_GT_DEINT_INFO) );
		handle->isInit = 1;
		handle->srcWidth	= pParam->srcWidth;
		handle->srcHeight	= pParam->srcHeight;
		handle->dstWidth	= pParam->srcWidth;
		handle->dstHeight	= pParam->srcHeight;
		handle->maxInOutBufSize = pParam->maxInOutBufSize;
		pParam->handle      = handle;
	}
	return (void*)handle;
}

static int32_t GTServiceDeintDoDeinterlace(NX_GT_PARAM_DO *pParam)
{
	int32_t i, ret = ERR_NONE;
	HDEINTERLACESOURCE hInSurface[3] = {NULL, };
	HDEINTERLACETARGET hOutSurface[3] = {NULL, };
	NX_GT_DEINT_HANDLE handle = (NX_GT_DEINT_HANDLE)pParam->handle;
	NX_VID_MEMORY_INFO *pInBuf = (NX_VID_MEMORY_INFO *)pParam->pInBuf;
	NX_VID_MEMORY_INFO *pOutBuf = (NX_VID_MEMORY_INFO *)pParam->pOutBuf;

	if( !handle || !handle->isInit )
	{
		return ERR_NOT_INIT;
	}

	//	Check Input Buffer in Cache
	for( i=0 ; i<handle->maxInOutBufSize ; i++ )
	{
		if( handle->inputImage[i].luPhyAddr == pInBuf->luPhyAddr )
		{
			hInSurface[0] = handle->hInSurface[i][0];
			hInSurface[1] = handle->hInSurface[i][1];
			hInSurface[2] = handle->hInSurface[i][2];
			break;
		}
	}
	if( hInSurface[0]==NULL )
	{
		for( i=0 ; i<handle->maxInOutBufSize ; i++ )
		{
			if( 0 == handle->inputImage[i].luPhyAddr )
			{
				hInSurface[0] = handle->hInSurface[i][0] = vrCreateDeinterlaceSource( handle->srcWidth,   handle->srcHeight,   (NX_MEMORY_HANDLE)pInBuf->privateDesc[0] );
				hInSurface[1] = handle->hInSurface[i][1] = vrCreateDeinterlaceSource( handle->srcWidth/2, handle->srcHeight/2, (NX_MEMORY_HANDLE)pInBuf->privateDesc[1] );
				hInSurface[2] = handle->hInSurface[i][2] = vrCreateDeinterlaceSource( handle->srcWidth/2, handle->srcHeight/2, (NX_MEMORY_HANDLE)pInBuf->privateDesc[2] );
				if( hInSurface[0] == NULL || hInSurface[1] == NULL || hInSurface[2]==NULL )
				{
					if( hInSurface[0] )		vrDestroyDeinterlaceSource( hInSurface[0] );
					if( hInSurface[1] )		vrDestroyDeinterlaceSource( hInSurface[1] );
					if( hInSurface[2] )		vrDestroyDeinterlaceSource( hInSurface[2] );
					ret = ERR_SURFACE_CREATE;
					goto ErrorExit;
				}
				handle->inputImage[i] = *pInBuf;
				break;
			}
		}
	}

	//	Check Output Buffer in Cache
	for( i=0 ; i<handle->maxInOutBufSize ; i++ )
	{
		if( handle->outputImage[i].luPhyAddr == pOutBuf->luPhyAddr )
		{
			hOutSurface[0] = handle->hOutSurface[i][0];
			hOutSurface[1] = handle->hOutSurface[i][1];
			hOutSurface[2] = handle->hOutSurface[i][2];
			break;
		}
	}
	if( hOutSurface[0]==NULL )
	{
		for( i=0 ; i<handle->maxInOutBufSize ; i++ )
		{
			if( 0 == handle->outputImage[i].luPhyAddr )
			{
				hOutSurface[0] = handle->hOutSurface[i][0] = vrCreateDeinterlaceTarget( handle->srcWidth,   handle->srcHeight,   (NX_MEMORY_HANDLE)pOutBuf->privateDesc[0] );
				hOutSurface[1] = handle->hOutSurface[i][1] = vrCreateDeinterlaceTarget( handle->srcWidth/2, handle->srcHeight/2, (NX_MEMORY_HANDLE)pOutBuf->privateDesc[1] );
				hOutSurface[2] = handle->hOutSurface[i][2] = vrCreateDeinterlaceTarget( handle->srcWidth/2, handle->srcHeight/2, (NX_MEMORY_HANDLE)pOutBuf->privateDesc[2] );
				if( hOutSurface[0] == NULL || hOutSurface[1] == NULL || hOutSurface[2]==NULL )
				{
					if( hOutSurface[0] )	vrDestroyDeinterlaceTarget( hOutSurface[0] );
					if( hOutSurface[1] )	vrDestroyDeinterlaceTarget( hOutSurface[1] );
					if( hOutSurface[2] )	vrDestroyDeinterlaceTarget( hOutSurface[2] );
					ret = ERR_SURFACE_CREATE;
					goto ErrorExit;
				}
				handle->outputImage[i] = *pOutBuf;
				break;
			}
		}
	}

	if( NULL==hOutSurface[0] || NULL==hInSurface[0] )
	{
		ret = ERR_BUF_SIZE;
		goto ErrorExit;
	}

	//	Deinterlace
	vrRunDeinterlace(hOutSurface[0], hInSurface[0]);	vrWaitDeinterlaceDone();
	vrRunDeinterlace(hOutSurface[1], hInSurface[1]);	vrWaitDeinterlaceDone();
	vrRunDeinterlace(hOutSurface[2], hInSurface[2]);	vrWaitDeinterlaceDone();

	return ret;

ErrorExit:
	return ret;
}

static void GTServiceDeintClose(NX_GT_PARAM_CLOSE *pParam)
{
	int i;
	NX_GT_DEINT_HANDLE handle = (NX_GT_DEINT_HANDLE)pParam->handle;
	if( handle )
	{
		if( handle->isInit )
		{
			for( i = 0 ; i < handle->maxInOutBufSize ; i++)
			{
				if( handle->inputImage[i].luPhyAddr )
				{
					vrDestroyDeinterlaceSource( handle->hInSurface[i][0] );
					vrDestroyDeinterlaceSource( handle->hInSurface[i][1] );
					vrDestroyDeinterlaceSource( handle->hInSurface[i][2] );
				}
				if( handle->outputImage[i].luPhyAddr )
				{
					vrDestroyDeinterlaceTarget( handle->hOutSurface[i][0] );
					vrDestroyDeinterlaceTarget( handle->hOutSurface[i][1] );
					vrDestroyDeinterlaceTarget( handle->hOutSurface[i][2] );
				}
			}
		}
		free( handle );
	}
}
//
//	End of Deinterlace Service
//
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
//
//	Scaler Service
//
struct NX_GT_SCALER_INFO {
	int32_t isInit;
	int32_t srcWidth;
	int32_t srcHeight;
	int32_t dstWidth;
	int32_t dstHeight;
	int32_t maxInOutBufSize;

	NX_VID_MEMORY_INFO inputImage[MAX_GRAPHIC_BUF_SIZE];		//	Input Image
	HDEINTERLACESOURCE hInSurface[MAX_GRAPHIC_BUF_SIZE][3];		//	Input Image Surface

	NX_VID_MEMORY_INFO outputImage[MAX_GRAPHIC_BUF_SIZE];		//	Target Output Buffer
	HDEINTERLACETARGET hOutSurface[MAX_GRAPHIC_BUF_SIZE][3];	//	Target Output Surface
};

static void *GTServiceSclOpen( NX_GT_PARAM_OPEN *pParam )
{
	NX_GT_SCALER_HANDLE handle = (NX_GT_SCALER_HANDLE)malloc(sizeof(struct NX_GT_SCALER_INFO));
	
	if( handle )
	{
		memset( handle, 0, sizeof(struct NX_GT_SCALER_INFO) );
		handle->isInit		= 1;
		handle->srcWidth	= pParam->srcWidth;
		handle->srcHeight	= pParam->srcHeight;
		handle->dstWidth	= pParam->dstWidth;
		handle->dstHeight	= pParam->dstHeight;
		handle->maxInOutBufSize = pParam->maxInOutBufSize;
		pParam->handle 		= handle;
	}

	return (void*)handle;
}

static int32_t GTServiceSclDoScale(NX_GT_PARAM_DO *pParam )
{
	int32_t i;
	HDEINTERLACESOURCE hInSurface[3] = {NULL, };
	HDEINTERLACETARGET hOutSurface[3] = {NULL, };
	NX_GT_SCALER_HANDLE handle = (NX_GT_SCALER_HANDLE)pParam->handle;

	NX_VID_MEMORY_INFO *pInBuf = (NX_VID_MEMORY_INFO *)pParam->pInBuf;
	NX_VID_MEMORY_INFO *pOutBuf = (NX_VID_MEMORY_INFO *)pParam->pOutBuf;

	if( !handle || !handle->isInit )
	{
		return -1;
	}

	//	Check surface
	for( i=0 ; i<handle->maxInOutBufSize ; i++ )
	{
		if( handle->inputImage[i].luPhyAddr == pInBuf->luPhyAddr )
		{
			hInSurface[0] = handle->hInSurface[i][0];
			hInSurface[1] = handle->hInSurface[i][1];
			hInSurface[2] = handle->hInSurface[i][2];
			break;
		}
	}
	//	Create Input(Source) Surface
	if( hInSurface[0]==NULL )
	{
		for( i=0 ; i<handle->maxInOutBufSize ; i++ )
		{
			if( 0 == handle->inputImage[i].luPhyAddr )
			{
				hInSurface[0] = handle->hInSurface[i][0] = vrCreateScaleSource( handle->srcWidth,   handle->srcHeight,   (NX_MEMORY_HANDLE)pInBuf->privateDesc[0] );
				hInSurface[1] = handle->hInSurface[i][1] = vrCreateScaleSource( handle->srcWidth/2, handle->srcHeight/2, (NX_MEMORY_HANDLE)pInBuf->privateDesc[1] );
				hInSurface[2] = handle->hInSurface[i][2] = vrCreateScaleSource( handle->srcWidth/2, handle->srcHeight/2, (NX_MEMORY_HANDLE)pInBuf->privateDesc[2] );
				handle->inputImage[i] = *pInBuf;
				break;
			}
		}
	}

	//	Check output surface
	for( i=0 ; i<handle->maxInOutBufSize ; i++ )
	{
		if( handle->outputImage[i].luPhyAddr == pOutBuf->luPhyAddr )
		{
			hOutSurface[0] = handle->hOutSurface[i][0];
			hOutSurface[1] = handle->hOutSurface[i][1];
			hOutSurface[2] = handle->hOutSurface[i][2];
			break;
		}
	}
	//	Create Output Surface
	if( hOutSurface[0]==NULL )
	{
		for( i=0 ; i<handle->maxInOutBufSize ; i++ )
		{
			if( 0 == handle->outputImage[i].luPhyAddr )
			{
				hOutSurface[0] = handle->hOutSurface[i][0] = vrCreateScaleTarget( handle->dstWidth,   handle->dstHeight,   (NX_MEMORY_HANDLE)pOutBuf->privateDesc[0] );
				hOutSurface[1] = handle->hOutSurface[i][1] = vrCreateScaleTarget( handle->dstWidth/2, handle->dstHeight/2, (NX_MEMORY_HANDLE)pOutBuf->privateDesc[1] );
				hOutSurface[2] = handle->hOutSurface[i][2] = vrCreateScaleTarget( handle->dstWidth/2, handle->dstHeight/2, (NX_MEMORY_HANDLE)pOutBuf->privateDesc[2] );
				handle->outputImage[i] = *pOutBuf;
				break;
			}
		}
	}

	//	Scale Image
	vrRunScale(hOutSurface[0], hInSurface[0]);	vrWaitDeinterlaceDone();
	vrRunScale(hOutSurface[1], hInSurface[1]);	vrWaitDeinterlaceDone();
	vrRunScale(hOutSurface[2], hInSurface[2]);	vrWaitDeinterlaceDone();

	return 0;
}

static void GTServiceSclClose(NX_GT_PARAM_CLOSE *pParam)
{
	int i;
	NX_GT_SCALER_HANDLE handle = (NX_GT_SCALER_HANDLE)pParam->handle;

	if( handle )
	{
		if( handle->isInit )
		{
			for( i = 0 ; i < handle->maxInOutBufSize ; i++)
			{
				if( handle->inputImage[i].luPhyAddr )
				{
					vrDestroyScaleSource( handle->hInSurface[i][0] );
					vrDestroyScaleSource( handle->hInSurface[i][1] );
					vrDestroyScaleSource( handle->hInSurface[i][2] );
				}
				if( handle->outputImage[i].luPhyAddr )
				{
					vrDestroyScaleTarget( handle->hOutSurface[i][0] );
					vrDestroyScaleTarget( handle->hOutSurface[i][1] );
					vrDestroyScaleTarget( handle->hOutSurface[i][2] );
				}
			}
		}
		free( handle );
	}
}
//
//			End of Scaler Service
//
////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
//
//	RGB to YUV Service
//	Input  : RGB Surface's Ion Handle
//	OUtput : NX_VID_MEMORY_INFO
//
struct NX_GT_RGB2YUV_INFO {
	int32_t isInit;
	int32_t srcWidth;
	int32_t srcHeight;
	int32_t dstWidth;
	int32_t dstHeight;
	int32_t maxInOutBufSize;

	int					inputFd[MAX_GRAPHIC_BUF_SIZE];			//	Input Image
	HDEINTERLACESOURCE	hInSurface[MAX_GRAPHIC_BUF_SIZE][2];		//	RGB Source Surface, 0(Y Source) 1(UV Source)

	NX_VID_MEMORY_INFO	outputImage[MAX_GRAPHIC_BUF_SIZE];		//	Target Output Buffer
	HDEINTERLACETARGET	hOutSurface[MAX_GRAPHIC_BUF_SIZE][2];	//	Target YUV Surface, 0(Y Target) 1(UV Target)
};

static void *GTServiceRgb2YuvOpen( NX_GT_PARAM_OPEN *pParam )
{
	NX_GT_RGB2YUV_HANDLE handle = (NX_GT_RGB2YUV_HANDLE)malloc(sizeof(struct NX_GT_RGB2YUV_INFO));
	
	if( handle )
	{
		memset( handle, 0, sizeof(struct NX_GT_RGB2YUV_INFO) );
		handle->isInit		= 1;
		handle->srcWidth	= pParam->srcWidth;
		handle->srcHeight	= pParam->srcHeight;
		handle->dstWidth	= pParam->dstWidth;
		handle->dstHeight	= pParam->dstHeight;
		handle->maxInOutBufSize = pParam->maxInOutBufSize;
		pParam->handle 		= handle;
	}

	return (void*)handle;
}

static int32_t GTServiceRgb2YuvDoConvert(NX_GT_PARAM_DO *pParam )
{
	int32_t i;
	HDEINTERLACESOURCE hInSurface[2] = {NULL, };
	HDEINTERLACETARGET hOutSurface[2] = {NULL, };
	NX_GT_RGB2YUV_HANDLE handle = (NX_GT_RGB2YUV_HANDLE)pParam->handle;

	int inputFd  = (int)pParam->pInBuf;
	NX_VID_MEMORY_INFO *pOutBuf = (NX_VID_MEMORY_INFO *)pParam->pOutBuf;

	if( !handle || !handle->isInit )
	{
		return -1;
	}

	//	Check Input ION File Descriptor
	for( i=0 ; i<handle->maxInOutBufSize ; i++ )
	{
		if( handle->inputFd[i] == inputFd )
		{
			hInSurface[0] = handle->hInSurface[i][0];
			hInSurface[1] = handle->hInSurface[i][1];
			break;
		}
	}

	//	If is not exist in cache, craete input surface.
	if( hInSurface[0]==NULL )
	{
		for( i=0 ; i<handle->maxInOutBufSize ; i++ )
		{
			if( 0 == handle->inputFd[i] )
			{
				handle->hInSurface[i][0] = vrCreateCvt2YSource ( handle->srcWidth, handle->srcHeight, (NX_MEMORY_INFO*)inputFd );
				handle->hInSurface[i][1] = vrCreateCvt2UVSource( handle->srcWidth, handle->srcHeight, (NX_MEMORY_INFO*)inputFd );
				hInSurface[0] = handle->hInSurface[i][0];
				hInSurface[1] = handle->hInSurface[i][1];
				handle->inputFd[i] = inputFd;
				break;
			}
		}
	}

	//	Check output surface
	for( i=0 ; i<handle->maxInOutBufSize ; i++ )
	{
		if( handle->outputImage[i].luPhyAddr == pOutBuf->luPhyAddr )
		{
			hOutSurface[0] = handle->hOutSurface[i][0];
			hOutSurface[1] = handle->hOutSurface[i][1];
			break;
		}
	}
	//	Create Output Surface
	if( hOutSurface[0]==NULL )
	{
		for( i=0 ; i<handle->maxInOutBufSize ; i++ )
		{
			if( 0 == handle->outputImage[i].luPhyAddr )
			{
				handle->hOutSurface[i][0] = vrCreateCvt2YTarget ( handle->dstWidth, handle->dstHeight, (NX_MEMORY_HANDLE)pOutBuf->privateDesc[0] );
				handle->hOutSurface[i][1] = vrCreateCvt2UVTarget( handle->dstWidth, handle->dstHeight, (NX_MEMORY_HANDLE)pOutBuf->privateDesc[1] );
				hOutSurface[0] = handle->hOutSurface[i][0];
				hOutSurface[1] = handle->hOutSurface[i][1];
				handle->outputImage[i] = *pOutBuf;
				break;
			}
		}
	}

	//	Run RGB 2 YUV 
	vrRunCvt2Y (hOutSurface[0], hInSurface[0]);	vrWaitCvt2YDone();
	vrRunCvt2UV(hOutSurface[1], hInSurface[1]);	vrWaitCvt2UVDone();

	return 0;
}

static void GTServiceRgb2YuvClose(NX_GT_PARAM_CLOSE *pParam)
{
	int i;
	NX_GT_RGB2YUV_HANDLE handle = (NX_GT_RGB2YUV_HANDLE)pParam->handle;

	if( handle )
	{
		if( handle->isInit )
		{
			for( i = 0 ; i < handle->maxInOutBufSize ; i++)
			{
				if( handle->inputFd[i] )
				{
					vrDestroyCvt2YSource( handle->hInSurface[i][0] );
					vrDestroyCvt2UVSource( handle->hInSurface[i][1] );
				}
				if( handle->outputImage[i].luPhyAddr )
				{
					vrDestroyCvt2YTarget( handle->hOutSurface[i][0] );
					vrDestroyCvt2UVTarget( handle->hOutSurface[i][1] );
				}
			}
		}
		free( handle );
	}
}
//
//			End of RGB to YUV(NV12) Service
//
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
//
//	YUV to RGB Service
//	Input  : NX_VID_MEMORY_INFO
//	OUtput : RGB Surface's Ion Handle
//
struct NX_GT_YUV2RGB_INFO {
	int32_t isInit;
	int32_t srcWidth;
	int32_t srcHeight;
	int32_t dstWidth;
	int32_t dstHeight;
	int32_t maxInOutBufSize;

	NX_VID_MEMORY_INFO	inputImage[MAX_GRAPHIC_BUF_SIZE];		//	Input Image
	HDEINTERLACESOURCE	hInSurface[MAX_GRAPHIC_BUF_SIZE];		//	Input Image's Surface

	int					outputFd[MAX_GRAPHIC_BUF_SIZE];			//	Target Buffer's Ion File Descriptor
	HDEINTERLACETARGET	hOutSurface[MAX_GRAPHIC_BUF_SIZE];		//	Target RGB Surface
};

static void *GTServiceYuv2RgbOpen( NX_GT_PARAM_OPEN *pParam )
{
	NX_GT_YUV2RGB_HANDLE handle = (NX_GT_YUV2RGB_HANDLE)malloc(sizeof(struct NX_GT_YUV2RGB_INFO));
	
	if( handle )
	{
		memset( handle, 0, sizeof(struct NX_GT_YUV2RGB_INFO) );
		handle->isInit		= 1;
		handle->srcWidth	= pParam->srcWidth;
		handle->srcHeight	= pParam->srcHeight;
		handle->dstWidth	= pParam->dstWidth;
		handle->dstHeight	= pParam->dstHeight;
		handle->maxInOutBufSize = pParam->maxInOutBufSize;
		pParam->handle 		= handle;
	}

	return (void*)handle;
}

static int32_t GTServiceYuv2RgbDoConvert(NX_GT_PARAM_DO *pParam )
{
	int32_t i;
	HDEINTERLACESOURCE hInSurface  = NULL;
	HDEINTERLACETARGET hOutSurface = NULL;
	NX_GT_YUV2RGB_HANDLE handle = (NX_GT_YUV2RGB_HANDLE)pParam->handle;

	NX_VID_MEMORY_INFO *pInBuf = (NX_VID_MEMORY_INFO *)pParam->pInBuf;
	int outFd  = (int)pParam->pOutBuf;

	if( !handle || !handle->isInit )
	{
		return -1;
	}

	//	Check Input ION File Descriptor
	for( i=0 ; i<handle->maxInOutBufSize ; i++ )
	{
		if( handle->inputImage[i].luPhyAddr == pInBuf->luPhyAddr )
		{
			hInSurface = handle->hInSurface[i];
			break;
		}
	}

	//	If is not exist in cache, craete input surface.
	if( hInSurface==NULL )
	{
		for( i=0 ; i<handle->maxInOutBufSize ; i++ )
		{
			if( 0 == handle->inputImage[i].luPhyAddr )
			{
				hInSurface =
				handle->hInSurface[i] = vrCreateCvt2RgbaSource ( handle->srcWidth, handle->srcHeight,
											(NX_MEMORY_HANDLE)pInBuf->privateDesc[0],
											(NX_MEMORY_HANDLE)pInBuf->privateDesc[1],
											(NX_MEMORY_HANDLE)pInBuf->privateDesc[2] );
				handle->inputImage[i] = *pInBuf;
				break;
			}
		}
	}

	//	Check output surface
	for( i=0 ; i<handle->maxInOutBufSize ; i++ )
	{
		if( handle->outputFd[i] == outFd )
		{
			hOutSurface = handle->hOutSurface[i];
			break;
		}
	}
	//	Create Output Surface
	if( hOutSurface==NULL )
	{
		for( i=0 ; i<handle->maxInOutBufSize ; i++ )
		{
			if( 0 == handle->outputFd[i] )
			{
				handle->hOutSurface[i] = vrCreateCvt2YTarget ( handle->dstWidth, handle->dstHeight, (NX_MEMORY_HANDLE)outFd );
				hOutSurface = handle->hOutSurface[i];
				handle->outputFd[i] = outFd;
				break;
			}
		}
	}

	//	Run RGB 2 YUV 
	vrRunCvt2Rgba(hOutSurface, hInSurface); vrWaitCvt2RgbaDone();

	return 0;
}

static void GTServiceYuv2RgbClose(NX_GT_PARAM_CLOSE *pParam)
{
	int i;
	NX_GT_YUV2RGB_HANDLE handle = (NX_GT_YUV2RGB_HANDLE)pParam->handle;

	if( handle )
	{
		if( handle->isInit )
		{
			for( i = 0 ; i < handle->maxInOutBufSize ; i++)
			{
				if( handle->inputImage[i].luPhyAddr )
				{
					vrDestroyCvt2RgbaSource( handle->hInSurface[i] );
				}
				if( handle->outputFd[i] )
				{
					vrDestroyCvt2RgbaTarget( handle->hOutSurface[i] );
				}
			}
		}
		free( handle );
	}
}
//
//			End of YUV to RGB Service
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
// Graphic tools Service
//
typedef struct tag_NX_GT_SERVICE {
	void*		(*GTOpen)	( NX_GT_PARAM_OPEN *param );
	int32_t		(*GTDo)		( NX_GT_PARAM_DO *param );
	void		(*GTClose)	( NX_GT_PARAM_CLOSE *param );
} NX_GT_SERVICE;

typedef struct tag_NX_GT_SERVICE_INFO {
	pthread_t			hThread;
	int32_t				bThreadRun;
	pthread_mutex_t		hLock;
	NX_QUEUE_HANDLE		hCmd;
	NX_SEM_HANDLE		hSemCmd;
	NX_SEM_HANDLE		hSemResult;
} NX_GT_SERVICE_INFO;

typedef struct tag_NX_GT_SERVICE_CMD {
	//	Input
	void		*pHandle;
	int32_t		id;
	int32_t		cmd;
	void		*data;
	//	Output
	int32_t		retCode;
} NX_GT_SERVICE_CMD;

void *GTServiceThread( void *arg )
{
	int32_t retCode = 0;
	NX_GT_SERVICE_INFO	*pService	= (NX_GT_SERVICE_INFO*)arg;
	NX_GT_SERVICE_CMD	*pCmd		= NULL;

	NX_GT_PARAM_OPEN	*pParamOpen = NULL;
	NX_GT_PARAM_DO		*pParamDo	= NULL;
	NX_GT_PARAM_CLOSE	*pParamClose= NULL;

	// Graphic Tool Engine Registration 
	NX_GT_SERVICE serviceList[MAX_SERVICE_NUM];
	
	serviceList[0].GTOpen	= &GTServiceDeintOpen;
	serviceList[0].GTDo		= &GTServiceDeintDoDeinterlace;
	serviceList[0].GTClose	= &GTServiceDeintClose;

	serviceList[1].GTOpen	= &GTServiceSclOpen;
	serviceList[1].GTDo		= &GTServiceSclDoScale;
	serviceList[1].GTClose	= &GTServiceSclClose;

	serviceList[2].GTOpen	= &GTServiceRgb2YuvOpen;
	serviceList[2].GTDo		= &GTServiceRgb2YuvDoConvert;
	serviceList[2].GTClose	= &GTServiceRgb2YuvClose;

	serviceList[3].GTOpen	= &GTServiceYuv2RgbOpen;
	serviceList[3].GTDo		= &GTServiceYuv2RgbDoConvert;
	serviceList[3].GTClose	= &GTServiceYuv2RgbClose;

	// GL Surface Initialize
	vrInitializeGLSurface();
	
	while( pService->bThreadRun )
	{
		pCmd = NULL;

		if( !NX_QueueGetCount( pService->hCmd ) )
			NX_SemaporePend( pService->hSemCmd );
		
		NX_QueuePop( pService->hCmd, (void**)&pCmd );
		
		// Command Parser..
		if( !pCmd ) continue;
		
		switch( pCmd->cmd )
		{
		case GT_SERVICE_CMD_OPEN :
			pParamOpen	= (NX_GT_PARAM_OPEN*)pCmd->data;
			retCode = (int32_t)serviceList[pCmd->id].GTOpen( pParamOpen );
			pCmd->retCode = retCode;
			break;
		case GT_SERVICE_CMD_DO :
			pParamDo	= (NX_GT_PARAM_DO*)pCmd->data;
			retCode = serviceList[pCmd->id].GTDo( pParamDo );
			pCmd->retCode = retCode;
			break;
		case GT_SERVICE_CMD_CLOSE :
			pParamClose	= (NX_GT_PARAM_CLOSE*)pCmd->data;
			serviceList[pCmd->id].GTClose( pParamClose );
			pCmd->retCode = 0;
			break;
		default :
			break;
		}
		NX_SemaporePost( pService->hSemResult );
	}

	// GL Surface Terminate
	vrTerminateGLSurface();

	return (void*)0xDEADDEAD;
}

NX_GT_SERVICE_HANDLE NX_GTServiceInit( void )
{
	NX_GT_SERVICE_INFO *pService = (NX_GT_SERVICE_INFO*)malloc( sizeof( NX_GT_SERVICE_INFO) );
	
	if( !pService ) {
		ErrMsg("%s(): GTService Create failed.\n", __func__);
		return NULL;
	}

	memset( pService, 0x00, sizeof( NX_GT_SERVICE_INFO) );

	pService->hThread		= 0;
	pService->bThreadRun	= false;
	pService->hCmd			= NX_QueueInit( MAX_CMD_SIZE );
	pService->hSemCmd		= NX_SemaporeInit( MAX_CMD_SIZE, 0 );
	pService->hSemResult	= NX_SemaporeInit( MAX_CMD_SIZE, 0 );
	pthread_mutex_init( &pService->hLock, NULL);

	return (NX_GT_SERVICE_HANDLE)pService;
}

void NX_GTServiceDeinit( NX_GT_SERVICE_HANDLE hService )
{
	NX_GT_SERVICE_INFO *pService = (NX_GT_SERVICE_INFO*)hService;

	if( !pService ) {
		ErrMsg("%s(): GTService Handle is NULL.\n", __func__);
		return;
	}
	
	if( pService->bThreadRun ) {
		NX_GTServiceStop( pService );
	}

	NX_QueueDeinit( pService->hCmd );
	NX_SemaporeDeinit( pService->hSemCmd );
	NX_SemaporeDeinit( pService->hSemResult );
	pthread_mutex_destroy( &pService->hLock );

	free( pService );
}

int32_t NX_GTServiceStart( NX_GT_SERVICE_HANDLE hService )
{
	NX_GT_SERVICE_INFO *pService = (NX_GT_SERVICE_INFO*)hService;

	if( pService->bThreadRun ) {
		ErrMsg("%s(): Fail, Already running.\n", __func__);
		return -1;
	}

	pService->bThreadRun = true;
	if( 0 > pthread_create( &pService->hThread, NULL, &GTServiceThread, (void*)hService ) ) {
		ErrMsg("%s(): Fail, Create thread.\n", __func__);
		return -1;
	}
	return 0;
}

int32_t NX_GTServiceStop( NX_GT_SERVICE_HANDLE hService )
{
	NX_GT_SERVICE_INFO *pService = (NX_GT_SERVICE_INFO*)hService;

	if( !pService->bThreadRun ) {
		ErrMsg("%s(): Fail, Already stopping.\n", __func__);
		return -1;
	}

	pService->bThreadRun = false;
	NX_SemaporePost( pService->hSemCmd );	// send dummy
	pthread_join( pService->hThread, NULL );

	return 0;
}

int32_t NX_GTServiceCommand( NX_GT_SERVICE_HANDLE hService, int32_t serviceID, int32_t serviceCmd, void *serviceData )
{
	NX_GT_SERVICE_CMD	cmd; 
	NxDbgCheckTimeStart();
	
	pthread_mutex_lock( &hService->hLock );

	if( !hService->bThreadRun )
	{
		ErrMsg("%s(): Service is not running.\n", __func__);
		pthread_mutex_unlock( &hService->hLock );
		return -1;
	}
	
	cmd.id		= serviceID;
	cmd.cmd		= serviceCmd;
	cmd.data	= serviceData;
	cmd.retCode = 0;

	NX_QueuePush( hService->hCmd, &cmd );
	NX_SemaporePost( hService->hSemCmd );
	// wait result..
	NX_SemaporePend( hService->hSemResult );
	pthread_mutex_unlock( &hService->hLock );

	NxDbgCheckTimeStop("Command");
	return cmd.retCode;
}

