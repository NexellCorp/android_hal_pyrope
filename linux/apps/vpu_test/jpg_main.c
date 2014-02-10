#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <nx_fourcc.h>

#include <nx_vip.h>			//	VIP
#include <nx_dsp.h>		//	Display
#include "nx_video_api.h"	//	Video En/Decoder
#include "queue.h"

#define	MAX_JPEG_HEADER_SIZE	(4*1024)

#define	MAX_ENC_BUFFER			8


/* These are the sample quantization tables given in JPEG spec section K.1.
 * The spec says that the values given produce "good" quality, and
 * when divided by 2, "very good" quality.
 */
static const char std_luminance_quant_tbl[64] = {
	16,  11,  10,  16,  24,  40,  51,  61,
	12,  12,  14,  19,  26,  58,  60,  55,
	14,  13,  16,  24,  40,  57,  69,  56,
	14,  17,  22,  29,  51,  87,  80,  62,
	18,  22,  37,  56,  68, 109, 103,  77,
	24,  35,  55,  64,  81, 104, 113,  92,
	49,  64,  78,  87, 103, 121, 120, 101,
	72,  92,  95,  98, 112, 100, 103,  99
};
static const char std_chrominance_quant_tbl[64] = {
	17,  18,  24,  47,  99,  99,  99,  99,
	18,  21,  26,  66,  99,  99,  99,  99,
	24,  26,  56,  99,  99,  99,  99,  99,
	47,  66,  99,  99,  99,  99,  99,  99,
	99,  99,  99,  99,  99,  99,  99,  99,
	99,  99,  99,  99,  99,  99,  99,  99,
	99,  99,  99,  99,  99,  99,  99,  99,
	99,  99,  99,  99,  99,  99,  99,  99
};


static void dumpdata( void *data, int len, const char *msg )
{
	int i=0;
	unsigned char *byte = (unsigned char *)data;
	printf("Dump Data : %s", msg);
	for( i=0 ; i<len ; i ++ )
	{
		if( i%16 == 0 )	printf("\n\t");
		printf("%.2x", byte[i] );
		if( i%4 == 3 ) printf(" ");
	}
	printf("\n");
}


static usage( const char *appName )
{
	printf(	"\nusage: %s [options]\n", appName );
	printf(	"    -o [out file] : output file name\n" );
	printf(	"    -a [angle]    : 0 / 90 / 180 / 270\n" );
	printf(	"    -q [value]    : 10 ~ 100 (Low(60), Middle(90), High(100))\n" );
}

int main( int argc, char *argv[] )
{
	int i;
	int inWidth, inHeight;				//	Sensor Input Image Width & Height
	int cropX, cropY, cropW, cropH;		//	Clipper Output Information
	int capture;
	int frameCnt = 0;
	FILE *fdOut = NULL;

	//	VIP
	VIP_HANDLE hVip;
	VIP_INFO vipInfo;
	//	Memory
	NX_VID_MEMORY_HANDLE hMem[MAX_ENC_BUFFER];
	//	Display
	DISPLAY_HANDLE hDsp;
	NX_QUEUE memQueue;
	DISPLAY_INFO dspInfo;
	//	Previous Displayed Memory
	NX_VID_MEMORY_INFO *pPrevDsp = NULL;
	//	Current Vip Buffer
	NX_VID_MEMORY_INFO *pCurCapturedBuf = NULL;
	NX_VID_MEMORY_INFO *pTmpMem = NULL;
	char *outFileName = NULL;

	//	Encoder Parameters
	NX_VID_ENC_INIT_PARAM encInitParam;
	int seqSize = MAX_JPEG_HEADER_SIZE;
	int captureFrameNum = 10;
	unsigned char *jpgHeader = (unsigned char *)malloc( MAX_JPEG_HEADER_SIZE );
	NX_VID_ENC_HANDLE hEnc;
	NX_VID_ENC_OUT encOut;
	int rotAngle = 0;
	int quality = 90;
	int opt;

	while (-1 != (opt = getopt(argc, argv, "ha:q:o:")))
	{
		switch (opt) {
		case 'h':	usage(argv[0]);				return 0;
		case 'o':	outFileName=strdup(optarg);	break;
		case 'q':	quality = atoi(optarg);		break;
		case 'a':	rotAngle = atoi(optarg);	break;
		default:
			break;
		}
	}

	if( !outFileName )
	{
		usage( argv[0] );
		return -1;
	}

	if( rotAngle!=0 && rotAngle!=90 && rotAngle!=180 && rotAngle!=270 )
	{
		rotAngle = 0;
	}

	//	Set Image & Clipper Information
	inWidth = 1024;
	inHeight = 768;
	cropX = 0;
	cropY = 0;
	cropW = 1024;
	cropH = 768;


	//	Initialze Memory Queue
	NX_InitQueue( &memQueue, MAX_ENC_BUFFER );
	//	Allocate Memory
	for( i=0; i<MAX_ENC_BUFFER ; i++ )
	{
		hMem[i] = NX_VideoAllocateMemory( 4096, cropW, cropH, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
		NX_PushQueue( &memQueue, hMem[i] );
	}

	memset( &vipInfo, 0, sizeof(vipInfo) );

	vipInfo.port = 0;
	vipInfo.mode = VIP_MODE_CLIPPER;

	//	Sensor Input Size
	vipInfo.width = inWidth;
	vipInfo.height = inHeight;

	//	Clipper Setting
	vipInfo.cropX = 0;
	vipInfo.cropY = 0;
	vipInfo.cropWidth  = cropW;
	vipInfo.cropHeight = cropH;

	//	Fps
	vipInfo.fpsNum = 30;
	vipInfo.fpsDen = 1;


	fdOut = fopen( outFileName, "wb" );

	//	Initailize VIP & Display
	dspInfo.port = 0;
	dspInfo.module = 0;
	dspInfo.width = cropW;
	dspInfo.height = cropH;
	dspInfo.left = 0;
	dspInfo.top = 0;
	dspInfo.right = cropW;
	dspInfo.bottom = cropH;
	hDsp = NX_DspInit( &dspInfo );
	hVip = NX_VipInit( &vipInfo );

	//	Initialize Encoder
	hEnc = NX_VidEncOpen( NX_JPEG_ENC );

	memset( &encInitParam, 0, sizeof(encInitParam) );
	encInitParam.width = cropW;
	encInitParam.height = cropH;
	encInitParam.rotAngle = rotAngle;
	encInitParam.mirDirection = 0;
	encInitParam.jpgQuality = quality;

	if( NX_VidEncInit( hEnc, &encInitParam ) != 0 )
	{
		printf("NX_VidEncInit(failed)!!!");
		exit(-1);
	}

	if( fdOut )
	{
		int size;
		//	Write Sequence Data
		NX_VidEncJpegGetHeader( hEnc, jpgHeader, &size );
		if( size > 0 )
		{
			fwrite( jpgHeader, 1, size, fdOut );
		}
	}

	//	PopQueue
	NX_PopQueue( &memQueue, (void**)&pTmpMem );
	NX_VipQueueBuffer( hVip, pTmpMem );

	while(1)
	{
		NX_PopQueue( &memQueue, (void**)&pTmpMem );
		NX_VipQueueBuffer( hVip, pTmpMem );

		NX_VipDequeueBuffer( hVip, &pCurCapturedBuf );

		NX_DspQueueBuffer( hDsp, pCurCapturedBuf );

		if( pPrevDsp )
		{
			NX_DspDequeueBuffer( hDsp );

			if( frameCnt == captureFrameNum )
			{
				printf("NX_VidEncJpegRunFrame ++\n");
				NX_VidEncJpegRunFrame( hEnc, pPrevDsp, &encOut );
				printf("NX_VidEncJpegRunFrame --\n");
				if( fdOut && encOut.bufSize>0 )
				{
					printf("encOut.bufSize = %d\n", encOut.bufSize);
					//	Write Sequence Data
					fwrite( encOut.outBuf, 1, encOut.bufSize, fdOut );
				}
				break;
			}

			NX_PushQueue( &memQueue, pPrevDsp );
		}
		pPrevDsp = pCurCapturedBuf;
		frameCnt ++;
	}

	if( fdOut )
	{
		printf("Close File Done\n");
		fclose( fdOut );
	}

	NX_DspClose( hDsp );
	NX_VipClose( hVip );

	return 0;
}

