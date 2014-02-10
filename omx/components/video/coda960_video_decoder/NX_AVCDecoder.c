#define	LOG_TAG				"NX_AVCDEC"

#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "DecodeFrame.h"

int NX_DecodeAvcFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	OMX_BUFFERHEADERTYPE* pInBuf = NULL, *pOutBuf = NULL;
	int inSize = 0;
	OMX_BYTE inData;
	NX_VID_DEC_OUT decOut;
	int ret = 0;

	UNUSED_PARAM(pOutQueue);

	if( pDecComp->bFlush )
	{
		flushVideoCodec( pDecComp );
		pDecComp->bFlush = OMX_FALSE;
	}

	NX_PopQueue( pInQueue, (void**)&pInBuf );
	if( pInBuf == NULL ){
		return 0;
	}

	inData = pInBuf->pBuffer;
	inSize = pInBuf->nFilledLen;

	TRACE("pInBuf->nFlags = 0x%08x\n", (int)pInBuf->nFlags );

	if( pInBuf->nFlags & OMX_BUFFERFLAG_EOS )
	{
		OMX_S32 i=0;
		for( i=0 ; i<NX_OMX_MAX_BUF ; i++ )
		{
			if( pDecComp->outBufferUseFlag[i] )
			{
				pOutBuf = pDecComp->pOutputBuffers[i];
				break;
			}
		}
		pDecComp->outBufferUseFlag[i] = 0;
		pDecComp->curOutBuffers --;
		DbgMsg("=========================> Receive Endof Stream Message \n");
		pInBuf->nFilledLen = 0;
		pDecComp->pCallbacks->EmptyBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pInBuf);

		if( pOutBuf )
		{
			pOutBuf->nFilledLen = 0;
			pOutBuf->nTimeStamp = pInBuf->nTimeStamp;
			pOutBuf->nFlags     = OMX_BUFFERFLAG_EOS;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		}
		return 0;
	}

	//	Step 1. Found Sequence Information
	if( OMX_FALSE == pDecComp->bInitialized )
	{
		if( pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
		{
			DbgMsg("Copy Extra Data (%d)\n", inSize );
			if( pDecComp->codecSpecificDataSize + inSize > MAX_DEC_SPECIFIC_DATA )
			{
				NX_ErrMsg("Too Short Codec Config Buffer!!!!\n");
				goto Exit;
			}
			memcpy( pDecComp->codecSpecificData + pDecComp->codecSpecificDataSize, inData, inSize );
			pDecComp->codecSpecificDataSize += inSize;
			goto Exit;
		}
	}

//	DbgMsg( "isIdrFrame = %d\n", isIdrFrame( pInBuf->pBuffer , pInBuf->nFilledLen ) );

	//{
	//	OMX_U8 *buf = pInBuf->pBuffer;
	//	DbgMsg("pInBuf->nFlags(%7d) = 0x%08x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x\n", pInBuf->nFilledLen, pInBuf->nFlags,
	//		buf[ 0],buf[ 1],buf[ 2],buf[ 3],buf[ 4],buf[ 5],buf[ 6],buf[ 7],
	//		buf[ 8],buf[ 9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],
	//		buf[16],buf[17],buf[18],buf[19],buf[20],buf[21],buf[22],buf[23] );
	//}

	//	Push Input Time Stamp
	PushVideoTimeStamp(pDecComp, pInBuf->nTimeStamp, pInBuf->nFlags );


	//	Step 2. Find First Key Frame & Do Initialize VPU
	if( OMX_FALSE == pDecComp->bInitialized )
	{
		int initBufSize = inSize + pDecComp->codecSpecificDataSize;
		unsigned char *initBuf = (unsigned char *)malloc( initBufSize );
		memcpy( initBuf, pDecComp->codecSpecificData, pDecComp->codecSpecificDataSize );
		memcpy( initBuf + pDecComp->codecSpecificDataSize, inData, inSize );

		//	Initialize VPU
		ret = InitialzieCodaVpu(pDecComp, initBuf, initBufSize );
		free( initBuf );

		if( 0 != ret )
		{
			ErrMsg("VPU initialized Failed!!!!\n");
			goto Exit;
		}

		pDecComp->bNeedKey = OMX_FALSE;
		pDecComp->bInitialized = OMX_TRUE;
		ret = NX_VidDecDecodeFrame( pDecComp->hVpuCodec, inData, 0, &decOut );
	}
	else
	{
		ret = NX_VidDecDecodeFrame( pDecComp->hVpuCodec, inData, inSize, &decOut );
	}

	if( ret==0 && decOut.outImgIdx >= 0 && ( decOut.outImgIdx < NX_OMX_MAX_BUF ) )
	{
		if( OMX_TRUE == pDecComp->bEnableThumbNailMode )
		{
			//	Thumbnail Mode
			OMX_U8 *outData;
			OMX_U8 *srcY, *srcU, *srcV;
			OMX_S32 strideY, strideU, strideV, width, height;
			NX_PopQueue( pOutQueue, (void**)&pOutBuf );
			outData = pOutBuf->pBuffer;

			srcY = (OMX_U8*)decOut.outImg.luVirAddr;
			srcU = (OMX_U8*)decOut.outImg.cbVirAddr;
			srcV = (OMX_U8*)decOut.outImg.crVirAddr;
			strideY = decOut.outImg.luStride;
			strideU = decOut.outImg.cbStride;
			strideV = decOut.outImg.crStride;
			width = pDecComp->width;
			height = pDecComp->height;

			if( width == strideY )
			{
				memcpy( outData, srcY, width*height );
				outData += width*height;

			}
			else
			{
				OMX_S32 h;
				for( h=0 ; h<height ; h++ )
				{
					memcpy( outData, srcY, width );
					srcY += strideY;
					outData += width;
				}
			}
			width /= 2;
			height /= 2;

			if( width == strideU )
			{
				memcpy( outData, srcU, width*height );
				outData += width*height;
				memcpy( outData, srcV, width*height );
			}
			else
			{
				OMX_S32 h;
				for( h=0 ; h<height ; h++ )
				{
					memcpy( outData, srcU, width );
					srcY += strideY;
					outData += width;
				}
				for( h=0 ; h<height ; h++ )
				{
					memcpy( outData, srcV, width );
					srcY += strideY;
					outData += width;
				}
			}

			NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.outImgIdx );
			pOutBuf->nFilledLen = pDecComp->width * pDecComp->height * 3 / 2;
			if( 0 != PopVideoTimeStamp(pDecComp, &pOutBuf->nTimeStamp, &pOutBuf->nFlags )  )
			{
				pOutBuf->nTimeStamp = pInBuf->nTimeStamp;
				pOutBuf->nFlags     = pInBuf->nFlags;
			}
			DbgMsg("ThumbNail Mode : pOutBuf->nAllocLen = %ld, pOutBuf->nFilledLen = %ld\n", pOutBuf->nAllocLen, pOutBuf->nFilledLen );
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		}
		else
		{
			//	Native Window Buffer Mode
			//	Get Output Buffer Pointer From Output Buffer Pool
			pOutBuf = pDecComp->pOutputBuffers[decOut.outImgIdx];

			if( pDecComp->outBufferUseFlag[decOut.outImgIdx] == 0 )
			{
				NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.outImgIdx );
				ErrMsg("Unexpected Buffer Handling!!!! Goto Exit\n");
				goto Exit;
			}
			pDecComp->outBufferUseFlag[decOut.outImgIdx] = 0;
			pDecComp->curOutBuffers --;

			pOutBuf->nFilledLen = sizeof(struct private_handle_t);
			if( 0 != PopVideoTimeStamp(pDecComp, &pOutBuf->nTimeStamp, &pOutBuf->nFlags )  )
			{
				pOutBuf->nTimeStamp = pInBuf->nTimeStamp;
				pOutBuf->nFlags     = pInBuf->nFlags;
			}
			TRACE("nTimeStamp = %lld\n", pOutBuf->nTimeStamp/1000);
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		}
	}

Exit:
	pInBuf->nFilledLen = 0;
	pDecComp->pCallbacks->EmptyBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pInBuf);

	return ret;
}
