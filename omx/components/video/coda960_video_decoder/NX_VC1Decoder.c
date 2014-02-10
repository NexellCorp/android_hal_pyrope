#define	LOG_TAG				"NX_VC1DEC"
#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "DecodeFrame.h"

static int MakeVC1DecodeSpecificInfo( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp )
{
	int retSize=0;
	OMX_U8 *pbHeader = pDecComp->codecSpecificData;
	OMX_U8 *pbMetaData = pDecComp->pExtraData;
	OMX_S32 nMetaData = pDecComp->nExtraDataSize;
	if( pDecComp->codecType.wmvType.eFormat == OMX_VIDEO_WMVFormat8 || pDecComp->codecType.wmvType.eFormat == OMX_VIDEO_WMVFormat9 )
	{
		DbgMsg("MakeVC1DecodeSpecificInfo() WMV Mode.(%ldx%ld)\n", pDecComp->width, pDecComp->height);
        PUT_LE32(pbHeader, ((0xC5 << 24)|0));
        retSize += 4; //version
        PUT_LE32(pbHeader, nMetaData);
        retSize += 4;

        memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
        retSize += nMetaData;

        PUT_LE32(pbHeader, pDecComp->height);
        retSize += 4;
        PUT_LE32(pbHeader, pDecComp->width);
        retSize += 4;
        PUT_LE32(pbHeader, 12);
        retSize += 4;
        PUT_LE32(pbHeader, 2 << 29 | 1 << 28 | 0x80 << 24 | 1 << 0);
        retSize += 4; // STRUCT_B_FRIST (LEVEL:3|CBR:1:RESERVE:4:HRD_BUFFER|24)
        PUT_LE32(pbHeader, 0);
        retSize += 4; // hrd_rate
		PUT_LE32(pbHeader, 0);            
        retSize += 4; // frameRate
		pDecComp->codecSpecificDataSize = retSize;
	}
	else	//	VC1
	{
		DbgMsg("MakeVC1DecodeSpecificInfo() VC1 Mode.\n");
		memcpy(pDecComp->codecSpecificData, pbMetaData, nMetaData); //OpaqueDatata
		pDecComp->codecSpecificDataSize = nMetaData;
	}

	return pDecComp->codecSpecificDataSize;
}

static int MakeVC1PacketData( OMX_S32 format, OMX_U8 *pIn, OMX_S32 inSize, OMX_U8 *pOut, OMX_S32 key, OMX_S32 time )
{
	OMX_S32 size=0;
	OMX_U8 *p = pIn;
	UNUSED_PARAM(time);
	if( format == OMX_VIDEO_WMVFormat8 || format == OMX_VIDEO_WMVFormat9 )
	{
		PUT_LE32( pOut, (inSize | ((key)?0x80000000:0)) );
		size += 4;
		PUT_LE32(pOut, 0);
		size += 4;
		memcpy(pOut, pIn, inSize);
		size += inSize;
	}
	else	//	VC1
	{
		if (p[0] != 0 || p[1] != 0 || p[2] != 1) // check start code as prefix (0x00, 0x00, 0x01)
		{
			*pOut++ = 0x00;
			*pOut++ = 0x00;
			*pOut++ = 0x01;
			*pOut++ = 0x0D;
			size = 4;
			memcpy(pOut, pIn, inSize);
			size += inSize;
		}
		else
		{
			memcpy(pOut, pIn, inSize);
			size = inSize; // no extra header size, there is start code in input stream.
		}
	}
	return size;
}


int NX_DecodeVC1Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	OMX_BUFFERHEADERTYPE* pInBuf = NULL, *pOutBuf = NULL;
	OMX_S32 inSize = 0, rcSize, key;
	OMX_BYTE inData, p;
	NX_VID_DEC_OUT decOut;
	NX_VID_RET decRet;

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

	p = inData = pInBuf->pBuffer;
	inSize = pInBuf->nFilledLen;
	key = pInBuf->nFlags & OMX_BUFFERFLAG_SYNCFRAME;

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

	//	Push Input Time Stamp
	PushVideoTimeStamp(pDecComp, pInBuf->nTimeStamp, pInBuf->nFlags );

	if( OMX_FALSE == pDecComp->bInitialized )
	{
		int ret, size;
		size = MakeVC1DecodeSpecificInfo( pDecComp );
		memcpy( pDecComp->tmpInputBuffer, pDecComp->codecSpecificData, size );
		size += MakeVC1PacketData( pDecComp->codecType.wmvType.eFormat, inData, inSize, pDecComp->tmpInputBuffer+size, key, (int)(pInBuf->nTimeStamp/1000ll) );

		//	Initialize VPU
		{
			OMX_U8 *buf = pDecComp->tmpInputBuffer;
			DbgMsg("DumpData (%6d) : 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x\n",
				size,
				buf[ 0],buf[ 1],buf[ 2],buf[ 3],buf[ 4],buf[ 5],buf[ 6],buf[ 7],
				buf[ 8],buf[ 9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],
				buf[16],buf[17],buf[18],buf[19],buf[20],buf[21],buf[22],buf[23] );
		}
		ret = InitialzieCodaVpu(pDecComp, pDecComp->tmpInputBuffer, size );
		if( 0 != ret )
		{
			ErrMsg("VPU initialized Failed!!!!\n");
		}
		pDecComp->bNeedKey = OMX_FALSE;
		pDecComp->bInitialized = OMX_TRUE;
		decRet = NX_VidDecDecodeFrame( pDecComp->hVpuCodec, pDecComp->tmpInputBuffer, 0, &decOut );
	}
	else
	{
		rcSize = MakeVC1PacketData( pDecComp->codecType.wmvType.eFormat, inData, inSize, pDecComp->tmpInputBuffer, key, (int)(pInBuf->nTimeStamp/1000ll) );
		decRet = NX_VidDecDecodeFrame( pDecComp->hVpuCodec, pDecComp->tmpInputBuffer, rcSize, &decOut );
	}

	//DbgMsg("decRet = %d, decOut.outImgIdx = %d\n", decRet, decOut.outImgIdx );

	if( decRet==0 && decOut.outImgIdx >= 0 && ( decOut.outImgIdx < NX_OMX_MAX_BUF ) )
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
			TRACE("%s nTimeStamp = %lld\n", __func__, pOutBuf->nTimeStamp/1000);
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		}
	}

Exit:
	pInBuf->nFilledLen = 0;
	pDecComp->pCallbacks->EmptyBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pInBuf);

	return decRet
	;
}
