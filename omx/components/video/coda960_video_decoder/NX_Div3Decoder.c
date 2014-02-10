#define	LOG_TAG				"NX_DIV3DEC"

#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "DecodeFrame.h"


static int MakeDiv3DecodeSpecificInfo( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_U8 *pIn, OMX_S32 inSize, OMX_U8 *pOut )
{
	unsigned char *pbHeader = pOut;
	int nMetaData = inSize;
	unsigned char *pbMetaData = pIn;
	int retSize = 0;
	if( !nMetaData )
	{
		PUT_LE32(pbHeader, MKTAG('C', 'N', 'M', 'V'));	//signature 'CNMV'
		PUT_LE16(pbHeader, 0x00);						//version
		PUT_LE16(pbHeader, 0x20);						//length of header in bytes
		PUT_LE32(pbHeader, MKTAG('D', 'I', 'V', '3'));	//codec FourCC
		PUT_LE16(pbHeader, pDecComp->width);			//width
		PUT_LE16(pbHeader, pDecComp->height);			//height
		PUT_LE32(pbHeader, 0);							//frame rate
		PUT_LE32(pbHeader, 0);							//time scale(?)
		PUT_LE32(pbHeader, 1);							//number of frames in file
		PUT_LE32(pbHeader, 0);							//unused
		retSize += 32;
	}
	else
	{
		PUT_BE32(pbHeader, nMetaData);
		retSize += 4;
		memcpy(pbHeader, pbMetaData, nMetaData);
		retSize += nMetaData;
	}
	return retSize;
}

static int MakeDiv3Stream( OMX_U8 *pIn, OMX_S32 inSize, OMX_U8 *pOut )
{
 	PUT_LE32(pOut,inSize);
 	PUT_LE32(pOut,0);
 	PUT_LE32(pOut,0);
	memcpy( pOut, pIn, inSize );
	return (inSize + 12);
}

int NX_DecodeDiv3Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	OMX_BUFFERHEADERTYPE* pInBuf = NULL, *pOutBuf = NULL;
	int inSize = 0;
	OMX_BYTE inData;
	NX_VID_DEC_OUT decOut;

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
		DbgMsg("=========================> Receive End of Stream Message \n");
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

	//	Push Input Time Stamp
	PushVideoTimeStamp(pDecComp, pInBuf->nTimeStamp, pInBuf->nFlags );


	//	Step 2. Find First Key Frame & Do Initialize VPU
	if( OMX_FALSE == pDecComp->bInitialized )
	{
		OMX_S32 size;
		NX_VID_RET ret;
		size = MakeDiv3DecodeSpecificInfo( pDecComp, pDecComp->pExtraData, pDecComp->nExtraDataSize, pDecComp->tmpInputBuffer );
		size += MakeDiv3Stream( inData, inSize, pDecComp->tmpInputBuffer + size );
		//	Initialize VPU
		ret = InitialzieCodaVpu(pDecComp, pDecComp->tmpInputBuffer, size );
		if( 0 != ret )
		{
			ErrMsg("VPU initialized Failed!!!!\n");
		}
		pDecComp->bNeedKey = OMX_FALSE;
		pDecComp->bInitialized = OMX_TRUE;
		NX_VidDecDecodeFrame( pDecComp->hVpuCodec, pDecComp->tmpInputBuffer, 0, &decOut );
	}
	else
	{
		inSize = MakeDiv3Stream( inData, inSize, pDecComp->tmpInputBuffer );
		NX_VidDecDecodeFrame( pDecComp->hVpuCodec, pDecComp->tmpInputBuffer, inSize, &decOut );
	}

	if( decOut.outImgIdx >= 0 && ( decOut.outImgIdx < NX_OMX_MAX_BUF ) )
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

	return 0;
}
