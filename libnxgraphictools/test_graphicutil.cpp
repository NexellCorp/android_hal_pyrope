#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>

#include <linux/fb.h>	//	for FB
#include <sys/types.h>	//	for open
#include <sys/stat.h>	//	for open
#include <fcntl.h>		//	for open
#include <sys/mman.h>	//	for mmap
#include <sys/ioctl.h>	//	for _IOWR

#include <nx_fourcc.h>
#include <nx_alloc_mem.h>
#include <nx_graphictools.h>

#include "src/dbgmsg.h"

#define	ALIGNED16(X)	(((X+15)>>4)<<4)

void print_usage(const char *appName)
{
	printf("\nUsage : %s [options]\n", appName);
	printf("  -h                  : help\n");
	printf("  -f [file name]      : Input file name(mandatory)\n");
	printf("  -o [file name]      : Output file name(optional)\n");
	printf("  -s [width,height]   : Source file image size(mandatory)\n");
	printf("  -d [width,height]   : Destination file image size(optional)\n");
	printf("  -m [1 or 2 or 3]    : 1(deinterlace), 2(scale), 3(both) (mandatory)\n");
	printf("  -t                  : Thread test mode\n");
	printf("-----------------------------------------------------------------------\n");
	printf(" example) \n");
	printf(" 1. deinterlace test\n");
	printf("  #> %s -m 1 -f input.yuv -s 1920,1080\n", appName);
	printf(" 2. scale test\n");
	printf("  #> %s -m 2 -f input.yuv -s 1920,1080 -d 1280,720\n", appName);
	printf(" 3. deinterlace & scale test\n");
	printf("  #> %s -m 3 -f input.yuv -s 1920,1080 -d 1280,720 -o deintscale_out.yuv\n", appName);
	printf(" 4. framebuffer capture\n");
	printf("  #> %s -m 4 -d 1280,720 -o framebuffer.yuv\n", appName);
}

const char *gstModeStr[] = 
{
	"Deinterlace Mode",
	"Scaling Mode",
	"Deinterlace & Scaling Mode",
	"FrameBuffer Capture Mode"
};


#define	NUM_FB_BUFFER	3
#define	FB_DEV_NAME	"/dev/graphics/fb0"
#define NXPFB_GET_FB_FD		_IOWR('N', 101, __u32)
#define NXPFB_GET_ACTIVE	_IOR('N', 103, __u32)
#define	ENABLE_FB_INFO

#define VSYNC_ON        "1"
#define VSYNC_OFF       "0"
#define VSYNC_CTL_FILE  "/sys/devices/platform/display/active.0"
#define VSYNC_MON_FILE  "/sys/devices/platform/display/vsync.0"

#if 1
#include <nx_video_api.h>

//
//	Application Sequence
//	Step
//		1. Initialize & Open Devices
//		2. Enable Vertical Sync Event
//		3. Wait Vertical Sync Event
//		4. Get Currrent FrameBuffer Index
//		5. Convert FrameBuffer(RGB32) to Encodable Memory(NV12)
//		6. Encoding Image
//		7. Write Capture Image
//		8. Loop 3 ~ 7
//
int FrameBufferCapture( const char *outFileName, int outWidth, int outHeight )
{
	int err = 0;
	static char uevent_desc[4096];
	memset(uevent_desc, 0, sizeof(uevent_desc));
	unsigned char seqBuf[1024];
	int seqBufSize=1024;
	NX_VID_ENC_OUT encOut;
	NX_VID_ENC_HANDLE hEncoder = NULL;

	//	Frame Buffer Related Variables
	int vsync_fd = -1;						//	Vertical Sync
	int vsync_ctl_fd = -1;					//	Vertical Sync En/Disalbe Descriptor
	int fbFd = -1;							//	Frame Buffer File Descriptor
	int ionFd[NUM_FB_BUFFER] = {-1,};		//	Frame Buffer's ION Memory File Descriptor
	struct fb_var_screeninfo info;			//	for Frame Buffer Screen Information

	//	Frame Buffer Capture Related
	NX_GT_RGB2YUV_HANDLE hRgb2Yuv = NULL;
	NX_VID_MEMORY_HANDLE encBuffer = NULL;

	//	Output File Descriptor
	FILE *outFd = NULL;

	//
	//	Step 1. Initialize & Open Drivers
	//

	//	Open Vertical Sync
	vsync_fd = open(VSYNC_MON_FILE, O_RDONLY);		//	Open Vertical Sync Monitor File
	vsync_ctl_fd = open(VSYNC_CTL_FILE, O_RDWR);	//	Open Vertical Sync Control File

	//	Open FrameBuffer & Frame Buffer Information
	fbFd = open(FB_DEV_NAME, O_RDWR, 0);			//	Open FrameBuffer Device Driver
	if( fbFd<0 )
	{
		printf("Cannot create FB Driver(%s)\n", FB_DEV_NAME);
		goto ErrorExit;
	}
	if (ioctl(fbFd, FBIOGET_VSCREENINFO, &info) == -1)
	{
		printf("FBIOGET_VSCREENINFO ioctl failed\n");
		goto ErrorExit;
	}
#ifdef ENABLE_FB_INFO
	printf("\n===========================================\n");
	printf(" Frame Buffer Information :\n");
	printf("===========================================\n");
	printf(" xres           : %d\n", info.xres);
	printf(" yres           : %d\n", info.yres);
	printf(" bits_per_pixel : %d\n", info.bits_per_pixel);
	printf(" width          : %d\n", info.width);
	printf(" height         : %d\n", info.height);
	printf("===========================================\n");
#endif
	//	Get Frame Buffer's ION File Descriptor
	for( int i=0 ; i<NUM_FB_BUFFER ; i++ )
	{
		ionFd[i] = i;
		int ret = ioctl(fbFd, NXPFB_GET_FB_FD, &ionFd[i]);
		if (ret < 0)
		{
			printf("%s: failed to NXPFB_GET_FB_FD!!!(%d)\n", __func__, ret);
			goto ErrorExit;
		}
	}

	//	Set to Default SIze
	if( outWidth==0 || outHeight==0 )
	{
		outWidth = info.xres;
		outHeight = info.yres;
	}

	printf("Encoding Size = %dx%d\n", outWidth, outHeight);
	hEncoder = NX_VidEncOpen( NX_AVC_ENC );
	if( hEncoder )
	{
		NX_VID_ENC_INIT_PARAM initParam;
		memset( &initParam, 0, sizeof(initParam) );

		initParam.width   = outWidth;
		initParam.height  = outHeight;
		initParam.gopSize = 60;
		if( outWidth == 720 )
			initParam.bitrate = 3000000;
		else
			initParam.bitrate = 5000000;
		initParam.fpsNum  = 60;
		initParam.fpsDen  = 1;

		initParam.enableRC  = 1;
		initParam.enableSkip= 0;
		initParam.maxQScale = 51;
		initParam.userQScale= 23;

		initParam.chromaInterleave = 1;

		NX_VidEncInit( hEncoder, &initParam );
		NX_VidEncGetSeqInfo(hEncoder, seqBuf, &seqBufSize);
	}
	else
	{
		printf("Encoder Open Failed!!!\n");
		goto ErrorExit;
	}

	//	Open RGB to YUV Engine
	hRgb2Yuv = NX_GTRgb2YuvOpen( info.xres, info.yres, outWidth, outHeight, 4 );
	if( hRgb2Yuv == NULL )
	{
		printf("NX_GTRgb2YuvOpen Failed!!\n");
		goto ErrorExit;
	}
	encBuffer = NX_VideoAllocateMemory( 4096, outWidth, outHeight, NX_MEM_MAP_LINEAR, FOURCC_NV12 );
	if( NULL == encBuffer )
	{
		printf("NX_VideoAllocateMemory failed!!(Encoder input buffer)\n");
		goto ErrorExit;
	}

	//
	//	Step 2. Enable Vertical Sync
	//
	{
		int err = write(vsync_ctl_fd, VSYNC_ON, sizeof(VSYNC_ON));
		if( err < 0 )
		{
			printf("Virtical sync eanble failed!!!\n");
			goto ErrorExit;
		}
	}

	struct pollfd fds[1];
	fds[0].fd = vsync_fd;
	fds[0].events = POLLPRI;

	if( outFileName == NULL )
	{
		outFd = fopen("./FrameBufferCap.264", "wb");
	}
	else
	{
		outFd = fopen(outFileName, "wb");
	}

	if( outFd && seqBufSize>0 )
	{
		fwrite(seqBuf, 1, seqBufSize, outFd);
	}

	while(true)
	{
		//
		//	Step 3. Wait Vertical Sync Event
		//
		err = poll(fds, 1, -1);
		if (err > 0)
		{
			if( fds[0].revents & POLLPRI )
			{
				int encRet;
				int activeFbIdx;
				//
				//	Step 4. Get Currrent FrameBuffer Index
				//
				if( ioctl(fbFd, NXPFB_GET_ACTIVE, &activeFbIdx) < 0 )
				{
					printf("Cannot Get Active Framebuffer Index\n");
					goto ErrorExit;
				}

				if( activeFbIdx >= NUM_FB_BUFFER || activeFbIdx < 0 )
				{
					printf("Invalid active FbIndex (%d)\n", activeFbIdx);
					goto ErrorExit;
				}

				//
				//	Step 5. Convert FrameBuffer to Encoding Memory
				//
				NX_GTRgb2YuvDoConvert( hRgb2Yuv, ionFd[activeFbIdx], encBuffer );

				//
				//	Step 6. Encoding Image
				//
				encRet = NX_VidEncEncodeFrame( hEncoder, encBuffer, &encOut );

				//
				//	Step 7. Write Encoding
				//
				if( encRet == 0 )
				{
					//printf("FbIdx(%d), isKey = %d, size = %d\n", activeFbIdx, encOut.isKey, encOut.bufSize);
					if( outFd )
					{
						fwrite(encOut.outBuf, 1, encOut.bufSize, outFd);
					}
				}
			}
		}
		else if (err == -1)
		{
			if (errno == EINTR)
				break;
			printf("error in vsync thread: %s", strerror(errno));
		}
	}

ErrorExit:
	if( hEncoder )
	{
		NX_VidEncClose( hEncoder );
	}
	if( vsync_fd > 0 )
	{
		close(vsync_fd);
	}
	if( vsync_ctl_fd )
	{
		close( vsync_ctl_fd );
	}
	if( fbFd > 0 )
	{
		close( fbFd );
	}
	for( int i=0 ; i<3 ; i++ )
	{
		if( ionFd[i] > 0 )
		{
			close( ionFd[i] );
		}
	}
	if( hRgb2Yuv )
	{
		NX_GTRgb2YuvClose( hRgb2Yuv );
	}
	if( encBuffer )
	{
		NX_FreeVideoMemory( encBuffer );
	}
	if( outFd )
	{
		fclose( outFd );
	}
	return 0;
}
#else	//	Low Level Internal API
#include <vr_deinterlace_private.h>
int FrameBufferCapture( const char *outFileName, int outWidth, int outHeight )
{
	//	Get Frame Buffer Information
	int ionFd[NUM_FB_BUFFER] = {-1,};
	int fbFd = open(FB_DEV_NAME, O_RDWR, 0);
	if( fbFd<0 )
	{
		printf("Cannot create FB Driver(%s)\n", FB_DEV_NAME);
		return -1;
	}

	struct fb_var_screeninfo info;
	if (ioctl(fbFd, FBIOGET_VSCREENINFO, &info) == -1)
	{
		return -1;
	}

	printf("\n===========================================\n");
	printf(" Frame Buffer Information :\n");
	printf("===========================================\n");
	printf(" xres           : %d\n", info.xres);
	printf(" yres           : %d\n", info.yres);
	printf(" bits_per_pixel : %d\n", info.bits_per_pixel);
	printf(" width          : %d\n", info.width);
	printf(" height         : %d\n", info.height);
	printf("===========================================\n");

	//	Get Frame Buffer's ION File Descriptor
	for( int i=0 ; i<NUM_FB_BUFFER ; i++ )
	{
		ionFd[i] = i;
		int ret = ioctl(fbFd, NXPFB_GET_FB_FD, &ionFd[i]);
		if (ret < 0)
		{
			printf("%s: failed to NXPFB_GET_FB_FD!!!", __func__);
			return -1;
		}
	}

	vrInitializeGLSurface();

	{
		NX_VID_MEMORY_HANDLE outBuffer = NX_VideoAllocateMemory( 4096, info.xres, info.yres, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
		HDEINTERLACESOURCE source = vrCreateCvt2YSource (info.xres, info.yres, (NX_MEMORY_HANDLE)ionFd[0]);
		HDEINTERLACETARGET target = vrCreateCvt2YTarget(info.xres, info.yres, (NX_MEMORY_HANDLE)outBuffer->privateDesc[0]);
		
		vrRunCvt2Y(target, source);
		vrWaitCvt2YDone();

		vrDestroyCvt2YTarget(target);
		vrDestroyCvt2YSource(source);
		int size = info.xres * info.yres;
		unsigned char *virAddr = (unsigned char *)outBuffer->luVirAddr;
		printf("dst surface(0x%x)\n", (int)virAddr);

	    FILE *outFd;
	    if( outFileName )
	    {
	    	outFd = fopen(outFileName, "wb");
	    }
	    else
	    {
	    	outFd = fopen("framebuffer.rgb.y", "wb");
	    }
	    if( outFd == NULL )
	    {
	    	printf("Cannot open output file\n");
	    	return -1;
	    }
	    fwrite(virAddr, 1, size, outFd);
	    fclose( outFd );
		if( fbFd )
		{
			close( fbFd );
		}
		if( outBuffer )
			NX_FreeVideoMemory(outBuffer);	
	}
	{
		NX_VID_MEMORY_HANDLE outBuffer = NX_VideoAllocateMemory( 4096, info.xres, info.yres/2, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
		HDEINTERLACESOURCE source = vrCreateCvt2UVSource (info.xres, info.yres, (NX_MEMORY_HANDLE)ionFd[0]);
		HDEINTERLACETARGET target = vrCreateCvt2UVTarget(info.xres, info.yres, (NX_MEMORY_HANDLE)outBuffer->privateDesc[0]);
		
		vrRunCvt2UV(target, source);
		vrWaitCvt2UVDone();

		vrDestroyCvt2UVTarget(target);
		vrDestroyCvt2UVSource(source);
		int size = (info.xres * info.yres) /2;
		unsigned char *virAddr = (unsigned char *)outBuffer->luVirAddr;
		printf("dst surface(0x%x)\n", (int)virAddr);

	    FILE *outFd;
	    if( outFileName )
	    {
	    	outFd = fopen(outFileName, "wb");
	    }
	    else
	    {
	    	outFd = fopen("framebuffer.rgb.uv", "wb");
	    }
	    if( outFd == NULL )
	    {
	    	printf("Cannot open output file\n");
	    	return -1;
	    }
	    fwrite(virAddr, 1, size, outFd);
	    fclose( outFd );
		if( fbFd )
		{
			close( fbFd );
		}
		if( outBuffer )
			NX_FreeVideoMemory(outBuffer);	
	}
	vrTerminateGLSurface();
	return 0;
}
#endif


// HDEINTERLACESOURCE cvt_input = vrCreateCvt2RgbaSource( VR_CVT_WIDTH, VR_CVT_HEIGHT, pInCvtDataY, pInCvtDataU, pInCvtDataV );
// HDEINTERLACETARGET cvt_output = vrCreateCvt2RgbaTarget( VR_CVT_WIDTH, VR_CVT_HEIGHT, pOutCvtData );
// vrRunCvt2Rgba(cvt_output, cvt_input);		
// vrWaitCvt2RgbaDone();

int main( int argc, char *argv[] )
{
	int opt;
	char *inFileName = NULL;
	char *outFileName = NULL;
	int srcWidth = 0, srcHeight = 0;
	int dstWidth = 0, dstHeight = 0;
	int mode = 0;

	while( -1 != (opt=getopt(argc, argv, "hf:o:s:d:m:")))
	{
		switch( opt ){
			case 'h':
				print_usage( argv[0] );
				return 0;
			case 'f':
				inFileName = strdup(optarg);
				break;
			case 'o':
				outFileName = strdup(optarg);
				break;
			case 's':
				sscanf( optarg, "%d,%d", &srcWidth, &srcHeight );
				break;
			case 'd':
				sscanf( optarg, "%d,%d", &dstWidth, &dstHeight );
				break;
			case 'm':
				mode = atoi(optarg);
				break;
			default:
				break;
		}
	}

	//	Frame Buffer Caputer Mode
	if( mode == 4 ){
		printf("dstWidth = %d, dstHeight=%d\n", dstWidth, dstHeight);
		return FrameBufferCapture(outFileName, dstWidth, dstHeight);
	}

	//	Check Argument Check
	if( srcWidth==0 || srcHeight==0 || inFileName==NULL || mode==0 )
	{
		printf("\nError Argumuent Error!!\n");
		printf("option : -f -s -m is mandatory!!!\n");
		print_usage( argv[0] );
		exit(-1);
	}

	if( dstWidth==0 && dstHeight==0 )
	{
		dstWidth = srcWidth;
		dstHeight = srcHeight;
	}

	//	Destination width & height
	if( mode == 1 )
	{
		printf("Deinterlace Mode.\n");
		if( (srcWidth!=dstWidth) || (srcHeight!=dstHeight) )
		{
			printf(" Error Invalid Argument\n");
			printf(" Source Size(%dx%d) != Output Size(%dx%d) !!!\n", srcWidth, srcHeight, dstWidth, dstHeight);
			exit(-1);
		}
	}
	else if( mode == 2 )
	{
		printf("Scaling Mode.\n");
		if( (srcWidth == dstWidth) && (srcHeight == dstHeight) )
		{
			printf(" Warning!! Source Size(%dx%d) == Output Size(%dx%d) !!!\n", srcWidth, srcHeight, dstWidth, dstHeight);
		}
	}
	else if( mode == 3 )
	{
		printf("Deinterlace & Scaling Mode.\n");
		if( (srcWidth == dstWidth) && (srcHeight == dstHeight) )
		{
			printf(" Warning!! Source Size(%dx%d) == Output Size(%dx%d) !!!\n", srcWidth, srcHeight, dstWidth, dstHeight);
			printf(" Mode change : Deinterlace & Scale Mode  to Deinterlace only mode.!!!\n");
			mode = 1;
		}
	}
	else if( mode == 4 )
	{
		printf("Frame Buffer Capture Mode.\n");
	}
	else
	{
		printf("Invalid Mode\n");
		print_usage( argv[0] );
		exit(-1);
	}

	if( outFileName == NULL )
	{
		outFileName = (char*)malloc( strlen(inFileName) + 5 );
		strcpy( outFileName, inFileName );
		strcat( outFileName, ".out" );
		outFileName[ strlen(outFileName)+1 ] = 0;
	}

	//	Print In/Out Informations
	printf("\n====================================================\n");
	printf("  Mode = %s(%d)\n", gstModeStr[mode-1], mode);
	printf("  Source filename   : %s\n", inFileName);
	printf("  Source Image Size : %d, %d\n", srcWidth, srcHeight);
	printf("  Output filename   : %s\n", outFileName);
	printf("  Output Image Size : %d, %d\n", dstWidth, dstHeight);
	printf("====================================================\n");

	//
	//	Load Input Image to Input Buffer
	//	 : Allocation Memory --> Load Image
	//
	NX_VID_MEMORY_HANDLE inBuffer = NULL; 	//	Allocate 3 plane memory for YUV
	inBuffer = NX_VideoAllocateMemory( 4096, srcWidth, srcHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );

	//	Load Image to Allocated Memory
	FILE *inFd = fopen( inFileName, "rb");
	if( inFd == NULL )
	{
		printf("Cannot open input file(%s)\n", inFileName);
		exit(-1);
	}
	else
	{
		size_t allocSize = (srcWidth * srcHeight * 3) / 2;
		size_t readSize = 0;
		unsigned char *inImgBuffer = (unsigned char *)malloc(allocSize);
		unsigned char *tmpSrc;
		unsigned char *tmpDst;
		readSize = fread( inImgBuffer, 1, allocSize, inFd );

		printf("ReadSize = %d\n", readSize);

		if( readSize != allocSize )
		{
			printf("[Warning] Input file information is not valid.!!\n");
		}
		tmpSrc = inImgBuffer;
		//	Load Lu
		tmpDst = (unsigned char *)inBuffer->luVirAddr;
		for( int i=0 ; i<srcHeight ; i ++ )
		{
			memcpy( tmpDst, tmpSrc, srcWidth );
			tmpDst += inBuffer->luStride;
			tmpSrc += srcWidth;
		}
		//	Load Cb
		tmpDst = (unsigned char *)inBuffer->cbVirAddr;
		for( int i=0 ; i<srcHeight/2 ; i ++ )
		{
			memcpy( tmpDst, tmpSrc, srcWidth/2 );
			tmpDst += inBuffer->cbStride;
			tmpSrc += srcWidth/2;
		}
		//	Load Cr
		tmpDst = (unsigned char *)inBuffer->crVirAddr;
		for( int i=0 ; i<srcHeight/2 ; i ++ )
		{
			memcpy( tmpDst, tmpSrc, srcWidth/2 );
			tmpDst += inBuffer->crStride;
			tmpSrc += srcWidth/2;
		}
		fclose( inFd );
	}

	//
	//	Deinter / Scale / Deinterlace & Scale
	//
	NX_VID_MEMORY_HANDLE outBuffer = NX_VideoAllocateMemory( 4096, dstWidth, dstHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
	NX_VID_MEMORY_HANDLE tmpBuffer = NULL;
	NX_GT_DEINT_HANDLE hDeint = NULL;
	NX_GT_SCALER_HANDLE hScale = NULL;

	switch( mode )
	{
		case 1:	//	Deinterloace Only
		{
			hDeint = NX_GTDeintOpen( srcWidth, srcHeight, 1 );
			if( NULL == hDeint )
			{
				printf("NX_DeintOpen() failed!!!\n");
				exit(1);
			}
			NX_GTDeintDoDeinterlace( hDeint, inBuffer, outBuffer );

			for( int j=0 ; j<100 ; j++ )
			{
				printf("j = %d\n", j);
				NX_GTDeintClose(hDeint);
				hDeint = NX_GTDeintOpen( srcWidth, srcHeight, 1 );
				hScale = NX_GTSclOpen( srcWidth, srcHeight, dstWidth, dstHeight, 1 );
				NX_GTDeintDoDeinterlace( hDeint, inBuffer, outBuffer );
				NX_GTSclClose(hScale);
			}
			break;
		}
		case 2:
		{
			hScale = NX_GTSclOpen( srcWidth, srcHeight, dstWidth, dstHeight, 1 );
			if( NULL == hScale )
			{
				printf("NX_SclOpen() failed!!!\n");
				exit(1);
			}
			NX_GTSclDoScale( hScale, inBuffer, outBuffer );
			break;
		}
		case 3:
		{
			tmpBuffer = NX_VideoAllocateMemory( 4096, srcWidth, srcHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );

			//	Deinterlace --> Scaling
			if( NULL == (hDeint = NX_GTDeintOpen( srcWidth, srcHeight, 1 )) )
			{
				printf("NX_DeintOpen() failed!!!\n");
				exit(1);
			}

			if( NULL == (hScale = NX_GTSclOpen( srcWidth, srcHeight, dstWidth, dstHeight, 1 )) )
			{
				printf("NX_SclOpen() failed!!!\n");
				exit(1);
			}

			NX_GTDeintDoDeinterlace( hDeint, inBuffer, tmpBuffer );
			NX_GTSclDoScale( hScale, tmpBuffer, outBuffer );

			for( int j=0 ; j<10000 ; j++ )
			{
				printf("j = %d\n", j);
				NX_GTDeintClose(hDeint);
				NX_GTSclClose(hScale);
				hDeint = NX_GTDeintOpen( srcWidth, srcHeight, 1 );
				hScale = NX_GTSclOpen( srcWidth, srcHeight, dstWidth, dstHeight, 1 );
				NX_GTDeintDoDeinterlace( hDeint, inBuffer, tmpBuffer );
				NX_GTSclDoScale( hScale, tmpBuffer, outBuffer );
			}
		}
	}

	//
	//	Write to Output File
	//
	FILE *outFd = fopen(outFileName, "wb");

	if( outFd != NULL )
	{
		for( int i=0 ; i<dstHeight ; i++ )
		{
			fwrite( (unsigned char*)(outBuffer->luVirAddr + dstWidth*i), 1, dstWidth, outFd );
		}
		for( int i=0 ; i<dstHeight/2 ; i++ )
		{
			fwrite( (unsigned char*)(outBuffer->cbVirAddr + i * dstWidth/2), 1, dstWidth/2, outFd );
		}
		for( int i=0 ; i<dstHeight/2 ; i++ )
		{
			fwrite( (unsigned char*)(outBuffer->crVirAddr + i * dstWidth/2), 1, dstWidth/2, outFd );
		}
		fclose(outFd);
	}
	else
	{
		printf("Cannot open output file(%s)\n", outFileName);
		exit(-1);
	}


	if( outBuffer )
		NX_FreeVideoMemory(outBuffer);

	if( tmpBuffer )
		NX_FreeVideoMemory(tmpBuffer);

	if( hDeint != NULL )
	{
		NX_GTDeintClose(hDeint);
	}

	if( hScale != NULL )
	{
		NX_GTSclClose(hScale);
	}

	return 0;
}
