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

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#include <nx_alloc_mem.h>
#include <nxp-v4l2-media.h>

#include "nx_dsp.h"

typedef struct DISPLAY_HANDLE_INFO	DISPLAY_HANDLE_INFO;

struct DISPLAY_HANDLE_INFO {
	V4L2_PRIVATE_HANDLE	hPrivate;

	//  Setting values
	DISPLAY_INFO		displayInfo;
	int32_t				mlcId;				//	Internal V4L2 MLC's ID

	//  Buffer Control Informations
	NX_VID_MEMORY_INFO	*videoBuf[DISPLAY_MAX_BUF_SIZE];
	int32_t				numPlane;			// video memory plane
	int32_t				numberV4L2ReqBuf;	// vidoe buffer size
	int32_t				lastQueueIdx;

	int32_t				streamOnFlag;		// on/off flag
	pthread_mutex_t		hMutex;
};

DISPLAY_HANDLE NX_DspInit( DISPLAY_INFO *pDspInfo )
{
	DISPLAY_HANDLE hDisplay = NULL;
	V4L2_PRIVATE_HANDLE hPrivate = NULL;
	
	int32_t result;
	int32_t mlcPin, mlcId, format;
	int32_t width, height;

	struct V4l2UsageScheme s;
	memset(&s, 0, sizeof(s));

	if( pDspInfo->port == DISPLAY_PORT_LCD ) {
		s.useHdmi		= false;
	} else {
		s.useHdmi		= true;
	}

	if( pDspInfo->module == DISPLAY_MODULE_MLC0 ) {
		s.useMlc0Video	= true;
		s.useMlc1Video	= false;
		
		mlcId	= nxp_v4l2_mlc0_video;
		mlcPin	= nxp_v4l2_mlc0;
	}
	else if( pDspInfo->module == DISPLAY_MODULE_MLC1 ) {
		s.useMlc0Video	= false;
		s.useMlc1Video	= true;

		mlcId	= nxp_v4l2_mlc1_video;
		mlcPin	= nxp_v4l2_mlc1;
	}
	else {
		printf("Unknown Mode..\n");
		return NULL;
	}


	if( NULL == (hPrivate = v4l2_init(&s)) ) 
	{
		printf("v4l2_init() failed!!!\n");
		return NULL;
	}

	if( pDspInfo->port != DISPLAY_PORT_LCD ) {
		result = v4l2_link(hPrivate, mlcPin, nxp_v4l2_hdmi);
		if( result < 0 ) {
			printf("v4l2_link() failed!!\n");
			return NULL;
		}
		result = v4l2_set_preset(hPrivate, nxp_v4l2_hdmi, V4L2_DV_1080P60);
		if( result < 0 ) {
			printf("v4l2_set_preset() failed!\n");
		}
	}

	result = v4l2_set_format(hPrivate, mlcId, pDspInfo->width, pDspInfo->height, PIXFORMAT_YUV420_PLANAR);
	if( result < 0 ) 
	{
		printf("v4l2_set_format() failed!!!\n");
		return NULL;
	}

	result = v4l2_set_crop(hPrivate, mlcId, pDspInfo->left, pDspInfo->top, pDspInfo->right-pDspInfo->left, pDspInfo->bottom-pDspInfo->top);
	if( result < 0 )
	{
		printf("v4l2_set_crop() failed!!!\n");
		return NULL;
	}

	if( pDspInfo->port == DISPLAY_PORT_LCD ) {
		result = v4l2_set_ctrl(hPrivate, mlcId, V4L2_CID_MLC_VID_PRIORITY, 0);
		if( result < 0 )
		{
			printf("v4l2_set_ctrl() failed!!!\n");
			goto ErrorExit;
		}
	}

	result = v4l2_reqbuf(hPrivate, mlcId, DISPLAY_MAX_BUF_SIZE);
	if( result < 0 )
	{
		printf("v4l2_reqbuf() failed!!!\n");
		goto ErrorExit;
	}

	hDisplay = (DISPLAY_HANDLE)malloc( sizeof(DISPLAY_HANDLE_INFO) );
	memset( hDisplay, 0, sizeof(DISPLAY_HANDLE_INFO) );
	memcpy( &hDisplay->displayInfo, pDspInfo, sizeof(hDisplay->displayInfo) );

	hDisplay->hPrivate			= hPrivate;
	hDisplay->numPlane			= 3;
	hDisplay->streamOnFlag		= 0;
	hDisplay->numberV4L2ReqBuf	= DISPLAY_MAX_BUF_SIZE;
	hDisplay->mlcId				= mlcId;

	return hDisplay;

ErrorExit:
	v4l2_exit(hDisplay->hPrivate);
	return NULL;
}

void NX_DspClose(DISPLAY_HANDLE hDisplay)
{
	if( hDisplay )
	{
		if( hDisplay->streamOnFlag )
		{
			v4l2_streamoff( hDisplay->hPrivate, hDisplay->mlcId );
			hDisplay->streamOnFlag = 0;
		}
		v4l2_exit( hDisplay->hPrivate );
		
		if(hDisplay)
			free(hDisplay);
	}
}

int32_t NX_DspQueueBuffer(DISPLAY_HANDLE hDisplay, NX_VID_MEMORY_INFO *pVidBuf)
{
	int32_t i;
	struct nxp_vid_buffer buf;
	NX_MEMORY_INFO *vidMem;

	buf.plane_num = hDisplay->numPlane;

	for( i = 0; i < hDisplay->numPlane; i++ )
	{
		vidMem			= (NX_MEMORY_INFO *)pVidBuf->privateDesc[i];
		buf.fds[i]		= (int)vidMem->privateDesc;
		buf.virt[i]		= (char*)vidMem->virAddr;
		buf.phys[i]		= (unsigned long)vidMem->phyAddr;
		buf.sizes[i]	= vidMem->size;
	}
	if( v4l2_qbuf(hDisplay->hPrivate, hDisplay->mlcId, hDisplay->numPlane, hDisplay->lastQueueIdx, &buf, -1, NULL) < 0 )
	{
		printf("DisplayQueueBuffer() failed\n");
		return -1;
	}
	hDisplay->lastQueueIdx = (hDisplay->lastQueueIdx + 1) % DISPLAY_MAX_BUF_SIZE;
	if( !hDisplay->streamOnFlag )
	{
		v4l2_streamon(hDisplay->hPrivate, hDisplay->mlcId);
		hDisplay->streamOnFlag = true;
	}
	return 0;
}

int32_t NX_DspDequeueBuffer(DISPLAY_HANDLE hDisplay)
{
	int32_t idx;
	if( v4l2_dqbuf(hDisplay->hPrivate, hDisplay->mlcId, hDisplay->numPlane, &idx, NULL) < 0 )
	{
		printf("DisplayDequeueBuffer() failed\n");
		return -1;
	}
}

int32_t NX_DspStreamControl(DISPLAY_HANDLE hDisplay, int32_t enable)
{
	printf("[%s] Not Implemetation..\n", __func__);
	return 0;

	if( hDisplay )
	{
		if( true == enable ) {
			if( false == hDisplay->streamOnFlag ) {
				hDisplay->streamOnFlag = true;
				v4l2_streamon(hDisplay->hPrivate, hDisplay->mlcId);
			} else {
				printf("[%s] Already Stream On..\n", __func__);
			}
		} else {
			if( true == hDisplay->streamOnFlag ) {
				hDisplay->streamOnFlag = false;
				v4l2_streamoff(hDisplay->hPrivate, hDisplay->mlcId);
			} else {
				printf("[%s] Already Stream Off..\n", __func__);
			}
		}
	} else {
		printf("[%s] DisplayStreamControl() failed\n", __func__);
	}

	return 0;
}


int32_t NX_DspVideoPosition ( DISPLAY_HANDLE hDisplay, int32_t left, int32_t top, int32_t right, int32_t bottom )
{
	DISPLAY_INFO *pDspInfo;
	if( !hDisplay || hDisplay->hPrivate )
	if( (1 > (right-left)) || (1 > (bottom-top)) )
	{
		return -1;
	}

	pDspInfo = &hDisplay->displayInfo;

	if( (pDspInfo->left   != left  ) ||
		(pDspInfo->top    != top   ) ||
		(pDspInfo->right  != right ) ||
		(pDspInfo->bottom != bottom) )
	{
		int32_t result = v4l2_set_crop(hDisplay->hPrivate, hDisplay->mlcId, left, top, right-left, bottom-top);
		if( result < 0 )
		{
			printf("%s():Line(%d) Error : v4l2_set_crop failed(%p,%d,%d,%d,%d,%d)!!!\n",
				__func__, __LINE__, hDisplay->hPrivate, hDisplay->mlcId, left, top, right-left, bottom-top );
			return -1;
		}

		//	Update Display Information
		pDspInfo->left   = left  ;
		pDspInfo->top    = top   ;
		pDspInfo->right  = right ;
		pDspInfo->bottom = bottom;
		return 0;
	}
	return 0;
}
