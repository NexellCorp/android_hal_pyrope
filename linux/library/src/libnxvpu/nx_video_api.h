//
//	Nexel Video En/Decoder API
//


#ifndef __NX_VIDEO_API_H__
#define	__NX_VIDEO_API_H__

#include <nx_alloc_mem.h>
#include <vpu_types.h>

typedef int		NX_VID_RET;
typedef struct	NX_VIDEO_ENC_INFO	*NX_VID_ENC_HANDLE;
typedef struct	NX_VIDEO_DEC_INFO	*NX_VID_DEC_HANDLE;

//	Video Codec Type ( API Level )
enum {
	//	Decoders
	NX_AVC_DEC		= 0x00,			//	H.264( AVC )
	NX_VC1_DEC		= 0x01,			//	WMV9
	NX_MP2_DEC		= 0x02,			//	Mpeg2 Video
	NX_MP4_DEC		= 0x03,			//	Mpeg4 Video
	NX_H263_DEC		= 0x04,			//	H.263
	NX_DIV3_DEC		= 0x05,			//	Divx 311
	NX_RV_DEC		= 0x06,			//	Real Video
	NX_VPX_THEORA	= 0x07,			//	Theora
	NX_VPX_VP8		= 0x08,			//	VP8

	//	Encoders
	NX_AVC_ENC		= 0x10,
	NX_MP4_ENC		= 0x12,
	NX_JPEG_ENC		= 0x20,			//	JPEG Encoder
};


enum{
	VID_ERR_NONE		= 0,
	VID_NEED_MORE_BUF	= 1
};


#define	MAX_DEC_FRAME_BUFFERS		22
#define	ENC_BITSTREAM_BUFFER		(4*1024*1024)

//
//	Encoder Specific APIs
//

typedef struct tNX_VID_ENC_OUT{
	unsigned char *outBuf;	//	output buffer's pointer
	int bufSize;			//	outBuf's size(input) and filled size(output)
	int isKey;				//	frame is i frame
	int width;				//	encoded image width
	int height;				//	encoded image height
}NX_VID_ENC_OUT;

typedef struct tNX_VID_DEC_OUT{
	int width;
	int height;
	int isKey;
	NX_VID_MEMORY_INFO outImg;
	int outImgIdx;
}NX_VID_DEC_OUT;

typedef struct tNX_VID_SEQ_IN{
	int width;
	int height;
	unsigned char *seqInfo;
	int seqSize;

	//	for External Buffer ( Optional )
	NX_VID_MEMORY_HANDLE *pMemHandle;
	int numBuffers;
	int enableUserData;		//	MPEG2 Decoder Only
}NX_VID_SEQ_IN;

typedef struct tNX_VID_SEQ_OUT{
	int nimBuffers;
	int numBuffers;
	int width;
	int height;

	//	for User Data( MPEG2 Decoder Only )
	int userDataNum;
	int userDataSize;
	int userDataBufFull;
}NX_VID_SEQ_OUT;

typedef struct tNX_VID_ENC_JPEG_PARAM{
	int configFlag;
	unsigned char huffVal[4][162];
	unsigned char huffBits[4][256];
	unsigned char qMatTab[4][64];
	unsigned char cInfoTab[4][6];
}NX_VID_ENC_JPEG_PARAM;

typedef struct tNX_VID_ENC_INIT_PARAM{
	int width;
	int height;
	int gopSize;
	int bitrate;
	int fpsNum;
	int fpsDen;

	//	Rate Control Parameters
	int enableRC;		//	En/Disable Rate Control
	int enableSkip;		//	Enable Skip Frame Mode ( in RC Mode )
	int maxQScale;		//	Max Quantization Scale  ( Maximum Quantization Scale in RC Mode )
	int userQScale;		//	User Quantization Scale ( in disable RC )

	//	Input Buffer Format
	int chromaInterleave;	//	0(disable) or 1(enable)

	//	for JPEG Specific Parameter
	int jpgQuality;		//	1~100
	int rotAngle;
	int mirDirection;
}NX_VID_ENC_INIT_PARAM;

#ifdef __cplusplus
extern "C" {
#endif
//
//	Encoder
//
NX_VID_ENC_HANDLE NX_VidEncOpen( int codecType );
NX_VID_RET NX_VidEncClose( NX_VID_ENC_HANDLE hEnc );
NX_VID_RET NX_VidEncInit( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_INIT_PARAM *pParam );
NX_VID_RET NX_VidEncGetSeqInfo( NX_VID_ENC_HANDLE hEnc, unsigned char* seqBuf, int *seqBufSize );
NX_VID_RET NX_VidEncEncodeFrame( NX_VID_ENC_HANDLE handle, NX_VID_MEMORY_HANDLE hInImage, NX_VID_ENC_OUT *pEncOut );

//
//	Decoder
//
NX_VID_DEC_HANDLE NX_VidDecOpen( int codecType, unsigned int mp4Class );
NX_VID_RET NX_VidDecClose( NX_VID_DEC_HANDLE hDec );
NX_VID_RET NX_VidDecInit(NX_VID_DEC_HANDLE hDec, unsigned char *seqInfo, int seqSize, int width, int height, NX_VID_SEQ_OUT *seqOut);
NX_VID_RET NX_VidDecInit2(NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *seqIn, NX_VID_SEQ_OUT *seqOut);
NX_VID_RET NX_VidDecInitWidthBuffer(NX_VID_DEC_HANDLE hDec, unsigned char *seqInfo, int seqSize, int width, int height, NX_VID_MEMORY_HANDLE *pMemHandle, int numBuffers, NX_VID_SEQ_OUT *seqOut );
NX_VID_RET NX_VidDecDecodeFrame( NX_VID_DEC_HANDLE hDec, unsigned char *strmBuf, int strmSize, NX_VID_DEC_OUT *pDecOut );
NX_VID_RET NX_VidDecClrDspFlag( NX_VID_DEC_HANDLE hDec, NX_VID_MEMORY_HANDLE hFrameBuf, int frameIdx );
NX_VID_RET NX_VidDecFlush( NX_VID_DEC_HANDLE hDec );

//
//	Jpeg Encoder APIs
//	Usage : NX_VidEncOpen() --> NX_VidEncInit() NX_VidEncJpegRunFrame() --> NX_VidEncClose()
//

NX_VID_RET NX_VidEncJpegGetHeader( NX_VID_ENC_HANDLE hEnc, unsigned char *jpgHeader, int *headerSize );
NX_VID_RET NX_VidEncJpegRunFrame( NX_VID_ENC_HANDLE hEnc, NX_VID_MEMORY_HANDLE hInImage, NX_VID_ENC_OUT *pEncOut );

#ifdef __cplusplus
}
#endif

#endif	//	__NX_VPU_API_H__
