#include <unistd.h>


//  FFMPEG Headers
#ifdef __cplusplus
 #define __STDC_CONSTANT_MACROS
 #ifdef _STDINT_H
  #undef _STDINT_H
 #endif
 # include <stdint.h>
#endif

#ifdef __cplusplus
extern "C"{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/opt.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#ifdef __cplusplus
}
#endif


#include <nx_video_api.h>
#include <nx_dsp.h>
#include "codec_info.h"


#define DUMP_FILE_FORMAT
#define	ENABLE_DISPLAY
#ifdef	ENABLE_DISPLAY
#define	ENABLE_MLC_SCALER
#endif

#define	LCD_WIDTH	1280
#define	LCD_HEIGHT	800

#define	RCV_V2

#define	NX_MAX_NUM_SPS		3
#define	NX_MAX_SPS_SIZE		1024
#define	NX_MAX_NUM_PPS		3
#define	NX_MAX_PPS_SIZE		1024

typedef struct {
	int				version;
	int				profile_indication;
	int				compatible_profile;
	int				level_indication;
	int				nal_length_size;
	int				num_sps;
	int				sps_length[NX_MAX_NUM_SPS];
	unsigned char	sps_data  [NX_MAX_NUM_SPS][NX_MAX_SPS_SIZE];
	int				num_pps;
	int				pps_length[NX_MAX_NUM_PPS];
	unsigned char	pps_data  [NX_MAX_NUM_PPS][NX_MAX_PPS_SIZE];
} NX_AVCC_TYPE;


typedef struct tFFMPEG_STREAM_READER {
	AVFormatContext		*fmt_ctx;

	//	Video Stream Information
	int					video_stream_idx;
	AVStream			*video_stream;
	AVCodec				*video_codec;

	NX_AVCC_TYPE		h264AvcCHeader;		//	for h.264

	//	Audio Stream Information
	int					audio_stream_idx;
	AVStream			*audio_stream;
	AVCodec				*audio_codec;
} FFMPEG_STREAM_READER;


unsigned char streamBuffer[8*1024*1024];
unsigned char seqData[1024*4];


//==============================================================================
static int NX_ParseSpsPpsFromAVCC( unsigned char *extraData, int extraDataSize, NX_AVCC_TYPE *avcCInfo );
static void NX_MakeH264StreamAVCCtoANNEXB( NX_AVCC_TYPE *avcc, unsigned char *pBuf, int *size );
static void dumpdata( void *data, int len, const char *msg );



#ifndef MKTAG
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#endif

#define PUT_LE32(_p, _var) \
	*_p++ = (unsigned char)((_var)>>0);  \
	*_p++ = (unsigned char)((_var)>>8);  \
	*_p++ = (unsigned char)((_var)>>16); \
	*_p++ = (unsigned char)((_var)>>24); 

#define PUT_BE32(_p, _var) \
	*_p++ = (unsigned char)((_var)>>24);  \
	*_p++ = (unsigned char)((_var)>>16);  \
	*_p++ = (unsigned char)((_var)>>8); \
	*_p++ = (unsigned char)((_var)>>0); 

#define PUT_LE16(_p, _var) \
	*_p++ = (unsigned char)((_var)>>0);  \
	*_p++ = (unsigned char)((_var)>>8);  


#define PUT_BE16(_p, _var) \
	*_p++ = (unsigned char)((_var)>>8);  \
	*_p++ = (unsigned char)((_var)>>0);  




FFMPEG_STREAM_READER *OpenMediaFile( const char *fileName )
{
	FFMPEG_STREAM_READER *streamReader = NULL;

	AVFormatContext *fmt_ctx = NULL;
	AVInputFormat *iformat = NULL;

	AVCodec *video_codec = NULL;
	AVCodec *audio_codec = NULL;
	AVStream *video_stream = NULL;
	AVStream *audio_stream = NULL;
	int video_stream_idx = -1;
	int audio_stream_idx = -1;
	int i;

	fmt_ctx = avformat_alloc_context();

	fmt_ctx->flags |= CODEC_FLAG_TRUNCATED;

	if ( avformat_open_input(&fmt_ctx, fileName, iformat, NULL) < 0 )
	{
		return NULL;
	}

	/* fill the streams in the format context */
	if ( av_find_stream_info(fmt_ctx) < 0)
	{
		av_close_input_file( fmt_ctx );
		return NULL;
	}

#ifdef	DUMP_FILE_FORMAT
	av_dump_format(fmt_ctx, 0, fileName, 0);
	printf("                                                                                            \n");
	printf("                                                                                            \n");
#endif

	//	Video Codec Binding
	for( i=0; i<(int)fmt_ctx->nb_streams ; i++ )
	{
		AVStream *stream = fmt_ctx->streams[i];

		if( stream->codec->codec_type == AVMEDIA_TYPE_VIDEO )
		{
			if( !(video_codec=avcodec_find_decoder(stream->codec->codec_id)) )
			{
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
				goto ErrorExit;
			}

			if( avcodec_open(stream->codec, video_codec)<0 )
			{
				printf( "Error while opening codec for input stream %d\n", stream->index );
				goto ErrorExit;
			}
			else
			{
				if( video_stream_idx == -1 )
				{
					video_stream_idx = i;
					video_stream = stream;;
				}
				else
				{
					avcodec_close( stream->codec );
				}
			}
		}
		else if( stream->codec->codec_type == AVMEDIA_TYPE_AUDIO )
		{
			if( !(audio_codec=avcodec_find_decoder(stream->codec->codec_id)) )
			{
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
				goto ErrorExit;
			}

			if( avcodec_open(stream->codec, audio_codec)<0 )
			{
				printf( "Error while opening codec for input stream %d\n", stream->index );
				goto ErrorExit;
			}
			else
			{
				if( audio_stream_idx == -1 )
				{
					audio_stream_idx = i;
					audio_stream = stream;;
				}
				else
				{
					avcodec_close( stream->codec );
				}
			}
		}
	}

	streamReader = (FFMPEG_STREAM_READER *)malloc(sizeof(FFMPEG_STREAM_READER));
	streamReader->fmt_ctx = fmt_ctx;

	streamReader->video_stream_idx = video_stream_idx;
	streamReader->video_stream     = video_stream;
	streamReader->video_codec      = video_codec;

	streamReader->audio_stream_idx = audio_stream_idx;
	streamReader->audio_stream     = audio_stream;
	streamReader->audio_codec      = audio_codec;

	return streamReader;

ErrorExit:
	if( streamReader )
	{
		free( streamReader );
	}
	if( fmt_ctx )
	{
		av_close_input_file( fmt_ctx );
	}
	return NULL;

}

 void CloseFile( FFMPEG_STREAM_READER *fmt_ctx )
{
	if( fmt_ctx )
	{
		av_close_input_file( fmt_ctx );
	}
}


int GetSequenceInformation( FFMPEG_STREAM_READER *streamReader, AVStream *stream, unsigned char *buffer, int size )
{
	unsigned char *pbHeader = buffer;
	enum CodecID codecId = stream->codec->codec_id;
	int fourcc;
	int frameRate = 0;
	int nMetaData = stream->codec->extradata_size;
	unsigned char *pbMetaData = stream->codec->extradata;
	int retSize = 0;
	unsigned int tag = stream->codec->codec_tag;

	if (stream->avg_frame_rate.den && stream->avg_frame_rate.num)
		frameRate = (int)((double)stream->avg_frame_rate.num/(double)stream->avg_frame_rate.den);
	if (!frameRate && stream->r_frame_rate.den && stream->r_frame_rate.num)
		frameRate = (int)((double)stream->r_frame_rate.num/(double)stream->r_frame_rate.den);

	if( (codecId == CODEC_ID_H264) && (stream->codec->extradata_size>0) )
	{
		if( stream->codec->extradata[0] == 0x1 )
		{
			int retSize;
			NX_ParseSpsPpsFromAVCC( pbMetaData, nMetaData, &streamReader->h264AvcCHeader );
			NX_MakeH264StreamAVCCtoANNEXB(&streamReader->h264AvcCHeader, buffer, &retSize );
			return retSize;
		}
	}
    else if ( (codecId == CODEC_ID_VC1) )
    {
		retSize = nMetaData;
        memcpy(pbHeader, pbMetaData, retSize);
		//if there is no seq startcode in pbMetatData. VPU will be failed at seq_init stage.
		return retSize;
	}
    else if ( (codecId == CODEC_ID_MSMPEG4V3) )
	{
		switch( tag )
		{
			case MKTAG('D','I','V','3'):
			case MKTAG('M','P','4','3'):
			case MKTAG('M','P','G','3'):
			case MKTAG('D','V','X','3'):
			case MKTAG('A','P','4','1'):
				if( !nMetaData )
				{
					PUT_LE32(pbHeader, MKTAG('C', 'N', 'M', 'V'));	//signature 'CNMV'
					PUT_LE16(pbHeader, 0x00);						//version
					PUT_LE16(pbHeader, 0x20);						//length of header in bytes
					PUT_LE32(pbHeader, MKTAG('D', 'I', 'V', '3'));	//codec FourCC
					PUT_LE16(pbHeader, stream->codec->width);		//width
					PUT_LE16(pbHeader, stream->codec->height);		//height
					PUT_LE32(pbHeader, stream->r_frame_rate.num);	//frame rate
					PUT_LE32(pbHeader, stream->r_frame_rate.den);	//time scale(?)
					PUT_LE32(pbHeader, stream->nb_index_entries);	//number of frames in file
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
			default:
				break;
			
		}
	}
	else if(  (codecId == CODEC_ID_WMV1) || (codecId == CODEC_ID_WMV2) || (codecId == CODEC_ID_WMV3) )
	{
#ifdef	RCV_V2	//	RCV_V2
        PUT_LE32(pbHeader, ((0xC5 << 24)|0));
        retSize += 4; //version
        PUT_LE32(pbHeader, nMetaData);
        retSize += 4;

        memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
        retSize += nMetaData;

        PUT_LE32(pbHeader, stream->codec->height);
        retSize += 4;
        PUT_LE32(pbHeader, stream->codec->width);
        retSize += 4;
        PUT_LE32(pbHeader, 12);
        retSize += 4;
        PUT_LE32(pbHeader, 2 << 29 | 1 << 28 | 0x80 << 24 | 1 << 0);
        retSize += 4; // STRUCT_B_FRIST (LEVEL:3|CBR:1:RESERVE:4:HRD_BUFFER|24)
        PUT_LE32(pbHeader, stream->codec->bit_rate);
        retSize += 4; // hrd_rate
		PUT_LE32(pbHeader, frameRate);            
        retSize += 4; // frameRate
#else	//RCV_V1
        PUT_LE32(pbHeader, (0x85 << 24) | 0x00);
        retSize += 4; //frames count will be here
        PUT_LE32(pbHeader, nMetaData);
        retSize += 4;
        memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
        retSize += nMetaData;
        PUT_LE32(pbHeader, stream->codec->height);
        retSize += 4;
        PUT_LE32(pbHeader, stream->codec->width);
        retSize += 4;
#endif
		return retSize;
	}
	else if( (stream->codec->codec_id == CODEC_ID_RV40 || stream->codec->codec_id == CODEC_ID_RV30 )
		&& (stream->codec->extradata_size>0) )
	{
		if( CODEC_ID_RV40 == stream->codec->codec_id )
		{
			fourcc = MKTAG('R','V','4','0');
		}
		else
		{
			fourcc = MKTAG('R','V','3','0');
		}
        retSize = 26 + nMetaData;
        PUT_BE32(pbHeader, retSize); //Length
        PUT_LE32(pbHeader, MKTAG('V', 'I', 'D', 'O')); //MOFTag
		PUT_LE32(pbHeader, fourcc); //SubMOFTagl
        PUT_BE16(pbHeader, stream->codec->width);
        PUT_BE16(pbHeader, stream->codec->height);
        PUT_BE16(pbHeader, 0x0c); //BitCount;
        PUT_BE16(pbHeader, 0x00); //PadWidth;
        PUT_BE16(pbHeader, 0x00); //PadHeight;
        PUT_LE32(pbHeader, frameRate);
        memcpy(pbHeader, pbMetaData, nMetaData);
		return retSize;
	}
	memcpy( buffer, stream->codec->extradata, stream->codec->extradata_size );

	return stream->codec->extradata_size;
}




static int PasreAVCStream( AVPacket *pkt, int nalLengthSize, unsigned char *buffer, int outBufSize )
{
	int nalLength;

	//	input
	unsigned char *inBuf = pkt->data;
	int inSize = pkt->size;
	int pos=0;

	//	'avcC' format
	do{
		nalLength = 0;

		if( nalLengthSize == 2 )
		{
			nalLength = inBuf[0]<< 8 | inBuf[1];
		}
		else if( nalLengthSize == 3 )
		{
			nalLength = inBuf[0]<<16 | inBuf[1]<<8  | inBuf[2];
		}
		else if( nalLengthSize == 4 )
		{
			nalLength = inBuf[0]<<24 | inBuf[1]<<16 | inBuf[2]<<8 | inBuf[3];
		}
		else if( nalLengthSize == 1 )
		{
			nalLength = inBuf[0];
		}

		inBuf  += nalLengthSize;
		inSize -= nalLengthSize;

		if( 0==nalLength || inSize<(int)nalLength )
		{
			printf("Error : avcC type nal length error (nalLength = %d, inSize=%d, nalLengthSize=%d)\n", nalLength, inSize, nalLengthSize);
			return -1;
		}

		//	put nal start code
		buffer[pos + 0] = 0x00;
		buffer[pos + 1] = 0x00;
		buffer[pos + 2] = 0x00;
		buffer[pos + 3] = 0x01;
		pos += 4;

		memcpy( buffer + pos, inBuf, nalLength );
		pos += nalLength;

		inSize -= nalLength;
		inBuf += nalLength;
	}while( 2<inSize );
	return pos;
}

static int MakeRvStream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	unsigned char *p = pkt->data;
    int cSlice, nSlice;
    int i, val, offset;
	int has_st_code = 0;
	int size;

	cSlice = p[0] + 1;
	nSlice =  pkt->size - 1 - (cSlice * 8);
	size = 20 + (cSlice*8);

	PUT_BE32(buffer, nSlice);
	if (AV_NOPTS_VALUE == pkt->pts)
	{
		PUT_LE32(buffer, 0);
	}
	else
	{
		PUT_LE32(buffer, (int)((double)(pkt->pts/stream->time_base.den))); // milli_sec
	}
	PUT_BE16(buffer, stream->codec->frame_number);
	PUT_BE16(buffer, 0x02); //Flags
	PUT_BE32(buffer, 0x00); //LastPacket
	PUT_BE32(buffer, cSlice); //NumSegments
	offset = 1;
	for (i = 0; i < (int) cSlice; i++)
	{
		val = (p[offset+3] << 24) | (p[offset+2] << 16) | (p[offset+1] << 8) | p[offset];
		PUT_BE32(buffer, val); //isValid
		offset += 4;
		val = (p[offset+3] << 24) | (p[offset+2] << 16) | (p[offset+1] << 8) | p[offset];
		PUT_BE32(buffer, val); //Offset
		offset += 4;
	}

	memcpy(buffer, pkt->data+(1+(cSlice*8)), nSlice);		
	size += nSlice;

	//printf("size = %6d, nSlice = %6d, cSlice = %4d, pkt->size=%6d, frameNumber=%d\n", size, nSlice, cSlice, pkt->size, stream->codec->frame_number );

	return size;
}

static int MakeVC1Stream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	int size=0;
	unsigned char *p = pkt->data;

	if( stream->codec->codec_id == CODEC_ID_VC1 )
	{
		if (p[0] != 0 || p[1] != 0 || p[2] != 1) // check start code as prefix (0x00, 0x00, 0x01)
		{
			*buffer++ = 0x00;
			*buffer++ = 0x00;
			*buffer++ = 0x01;
			*buffer++ = 0x0D;
			size = 4;
			memcpy(buffer, pkt->data, pkt->size);
			size += pkt->size;
		}
		else
		{
			memcpy(buffer, pkt->data, pkt->size);
			size = pkt->size; // no extra header size, there is start code in input stream.
		}
	}
	else
	{
		PUT_LE32(buffer, pkt->size | ((pkt->flags & AV_PKT_FLAG_KEY) ? 0x80000000 : 0));
		size += 4;
#ifdef RCV_V2	//	RCV_V2
		if (AV_NOPTS_VALUE == pkt->pts)
		{
			PUT_LE32(buffer, 0);
		}
		else
		{
			PUT_LE32(buffer, (int)((double)(pkt->pts/stream->time_base.den))); // milli_sec
		}
		size += 4;
#endif
		memcpy(buffer, pkt->data, pkt->size);
		size += pkt->size;
	}
	return size;
}


static int MakeDIVX3Stream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	int size = pkt->size;
	unsigned int tag = stream->codec->codec_tag;
	if( tag == MKTAG('D', 'I', 'V', '3') || tag == MKTAG('M', 'P', '4', '3') ||
		tag == MKTAG('M', 'P', 'G', '3') || tag == MKTAG('D', 'V', 'X', '3') || tag == MKTAG('A', 'P', '4', '1') )
	{
 		PUT_LE32(buffer,pkt->size);
 		PUT_LE32(buffer,0);
 		PUT_LE32(buffer,0);
 		size += 12;
	}
	memcpy( buffer, pkt->data, pkt->size );
	return size;
}
int ReadStream( FFMPEG_STREAM_READER *streamReader, AVStream *stream, unsigned char *buffer, int *size, int *isKey )
{
	int ret;
	AVPacket pkt;
	enum CodecID codecId = stream->codec->codec_id;
	*size = 0;
	do{
		ret = av_read_frame( streamReader->fmt_ctx, &pkt );
		if( ret < 0 )
			return -1;
		if( pkt.stream_index == stream->index )
		{
			//	check codec type
			if( codecId == CODEC_ID_H264 && stream->codec->extradata_size > 0 && stream->codec->extradata[0]==1 )
			{
				*size = PasreAVCStream( &pkt, streamReader->h264AvcCHeader.nal_length_size, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
			else if(  (codecId == CODEC_ID_VC1) || (codecId == CODEC_ID_WMV1) || (codecId == CODEC_ID_WMV2) || (codecId == CODEC_ID_WMV3) )
			{
				*size = MakeVC1Stream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
			else if( codecId == CODEC_ID_RV30 || codecId == CODEC_ID_RV40 )
			{
				*size = MakeRvStream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
			else if( codecId == CODEC_ID_MSMPEG4V3 )
			{
				*size = MakeDIVX3Stream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
			else
			{
				memcpy(buffer, pkt.data, pkt.size );
				*size = pkt.size;
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				av_free_packet( &pkt );
				return 0;
			}
		}
		av_free_packet( &pkt );
	}while(1);
	return -1;
}

int ReadStream2( FFMPEG_STREAM_READER *streamReader, AVStream *stream,  AVPacket *pkt )
{
	int ret;
	enum CodecID codecId = stream->codec->codec_id;
	do{
		ret = av_read_frame( streamReader->fmt_ctx, pkt );
		if( ret < 0 )
			return -1;

		if( pkt->stream_index == stream->index )
		{
			return 0;
		}

		av_free_packet( pkt );
		av_init_packet( pkt );
	}while(1);
	return -1;
}


static void dumpdata( void *data, int len, const char *msg )
{
	int i=0;
	unsigned char *byte = (unsigned char *)data;
	printf("Dump Data : %s", msg);
	for( i=0 ; i<len ; i ++ )
	{
		if( i%32 == 0 )	printf("\n\t");
		printf("%.2x", byte[i] );
		if( i%4 == 3 ) printf(" ");
	}
	printf("\n");
}

int isIdrFrame( unsigned char *buf, int size )
{
	int i;
	int isIdr=0;

	for( i=0 ; i<size-4; i++ )
	{
		if( buf[i]==0 && buf[i+1]==0 && ((buf[i+2]&0x1F)==0x05) )	//	Check Nal Start Code & Nal Type
		{
			isIdr = 1;
			break;
		}
	}

	return isIdr;
}


static int NX_ParseSpsPpsFromAVCC( unsigned char *extraData, int extraDataSize, NX_AVCC_TYPE *avcCInfo )
{
	int pos = 0;
	int i;
	int length;
	if( 1!=extraData[0] || 11>extraDataSize ){
		printf( "Error : Invalid \"avcC\" data\n" );
		return -1;
	}

	//	Parser "avcC" format data
	avcCInfo->version				= (int)extraData[pos];			pos++;
	avcCInfo->profile_indication	= (int)extraData[pos];			pos++;
	avcCInfo->compatible_profile	= (int)extraData[pos];			pos++;
	avcCInfo->level_indication		= (int)extraData[pos];			pos++;
	avcCInfo->nal_length_size		= (int)(extraData[pos]&0x03)+1;	pos++;
	//	parser SPSs
	avcCInfo->num_sps				= (int)(extraData[pos]&0x1f);	pos++;
	for( i=0 ; i<avcCInfo->num_sps ; i++){
		length = avcCInfo->sps_length[i] = (int)(extraData[pos]<<8)|extraData[pos+1];
		pos+=2;
		if( (pos+length) > extraDataSize ){
			printf("Error : extraData size too small(SPS)\n" );
			return -1;
		}
		memcpy( avcCInfo->sps_data[i], extraData+pos, length );
		pos += length;
	}

	//	parse PPSs
	avcCInfo->num_pps				= (int)extraData[pos];			pos++;
	for( i=0 ; i<avcCInfo->num_pps ; i++ ){
		length = avcCInfo->pps_length[i] = (int)(extraData[pos]<<8)|extraData[pos+1];
		pos+=2;
		if( (pos+length) > extraDataSize ){
			printf( "Error : extraData size too small(PPS)\n");
			return -1;
		}
		memcpy( avcCInfo->pps_data[i], extraData+pos, length );
		pos += length;
	}
	return 0;
}

static void NX_MakeH264StreamAVCCtoANNEXB( NX_AVCC_TYPE *avcc, unsigned char *pBuf, int *size )
{
	int i;
	int pos = 0;
	for( i=0 ; i<avcc->num_sps ; i++ )
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, avcc->sps_data[i], avcc->sps_length[i] );
		pos += avcc->sps_length[i];
	}
	for( i=0 ; i<avcc->num_pps ; i++ )
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, avcc->pps_data[i], avcc->pps_length[i] );
		pos += avcc->pps_length[i];
	}
	*size = pos;
}


static int CodecIdToVpuType( int codecId, unsigned int fourcc )
{
	int vpuCodecType =-1;
	printf("codecId = %d, fourcc=%c%c%c%c\n", codecId, fourcc, fourcc>>8, fourcc>>16, fourcc>>24);
	if( codecId == CODEC_ID_MPEG4 || codecId == CODEC_ID_FLV1 )
	{
		vpuCodecType = NX_MP4_DEC;
	}
	else if( codecId == CODEC_ID_MSMPEG4V3 )
	{
		switch( fourcc )
		{
			case MKTAG('D','I','V','3'):
			case MKTAG('M','P','4','3'):
			case MKTAG('M','P','G','3'):
			case MKTAG('D','V','X','3'):
			case MKTAG('A','P','4','1'):
				vpuCodecType = NX_DIV3_DEC;
				break;
			default:
				vpuCodecType = NX_MP4_DEC;
				break;
		}
	}
	else if( codecId == CODEC_ID_H263 || codecId == CODEC_ID_H263P || codecId == CODEC_ID_H263I )
	{
		vpuCodecType = NX_H263_DEC;
	}
	else if( codecId == CODEC_ID_H264 )
	{
		vpuCodecType = NX_AVC_DEC;
	}
	else if( codecId == CODEC_ID_MPEG2VIDEO )
	{
		vpuCodecType = NX_MP2_DEC;
	}
	else if( (codecId == CODEC_ID_WMV3) || (codecId == CODEC_ID_VC1) )
	{
		vpuCodecType = NX_VC1_DEC;
	}
	else if( (codecId == CODEC_ID_RV30) || (codecId == CODEC_ID_RV40) )
	{
		vpuCodecType = NX_RV_DEC;
	}
	else
	{
		printf("Cannot supprot codecid(%d)\n", codecId);
		exit(-1);
	}
	return vpuCodecType;
}


long long GetCurrentTime( void )
{
	long long msec;
	struct timeval tv;
	gettimeofday( &tv, NULL );

	msec = ((long long)tv.tv_sec*1000) + tv.tv_usec*1000;
	return msec;
}


int transcoding( const char *inFileName, const char *outFileName )
{
	FFMPEG_STREAM_READER *pReader;
#ifdef ENABLE_DISPLAY
	DISPLAY_HANDLE hDsp;
	DISPLAY_INFO dspInfo;
#endif
	int vpuCodecType;
	NX_VID_RET vidRet;
	NX_VID_SEQ_OUT seqOut;
	NX_VID_DEC_HANDLE hDec;
	NX_VID_DEC_OUT decOut;
	NX_VID_ENC_INIT_PARAM encInitParam;
	int seqSize = 0;
	int bInit=0, pos=0, decSize=0;
	int readSize, frameCount = 0, outCount=0;
	int prevIdx = -1;
	int isKey = 0;
	int needKey = 1;
	int dspWidth, dspHeight, dspX, dspY;
	double xRatio, yRatio;
	AVPacket pkt;
	unsigned int mp4Class=0;
	double fps = 0;

	struct timeval startTime, endTime;
	struct timeval totStart, totEnd;

	long long totalDecTime = 0;
	long long totalEncTime = 0;
	long long totalTime = 0;

	//	for Encoder
	NX_VID_ENC_HANDLE hEnc;
	NX_VID_ENC_OUT encOut;
	FILE *outFd = fopen(outFileName, "wb");

	av_register_all();

	pReader = OpenMediaFile( inFileName );
	if( !pReader )
	{
		printf("Cannot open file!!!\n");
		return -1;
	}

	if( pReader->video_stream_idx == -1 )
	{
		printf("Cannot found video stream!!!\n");
		return -1;
	}

	vpuCodecType = CodecIdToVpuType( pReader->video_stream->codec->codec_id, pReader->video_stream->codec->codec_tag );
	if( vpuCodecType < 0 )
	{
		return -1;
	}

	mp4Class = fourCCToMp4Class( pReader->video_stream->codec->codec_tag );
	if( mp4Class == -1 )
		mp4Class = codecIdToMp4Class( pReader->video_stream->codec->codec_id );

	hDec = NX_VidDecOpen(vpuCodecType, mp4Class);
	if( hDec == NULL )
	{
		printf("NX_VidDecOpen(%d) failed!!!\n", vpuCodecType);
		return -1;
	}

	seqSize = GetSequenceInformation( pReader, pReader->video_stream, seqData, sizeof(seqData) );
	if( seqSize == 0 )
	{
		printf("Have no SeqData!!!\n", vpuCodecType);
//		return -1;
	}

	gettimeofday(&totStart, NULL);

	hEnc = NX_VidEncOpen( NX_AVC_ENC );
	memset( &encInitParam, 0, sizeof(encInitParam) );
	encInitParam.width = pReader->video_stream->codec->coded_width;
	encInitParam.height = pReader->video_stream->codec->coded_height;
	encInitParam.gopSize = 30;
	encInitParam.bitrate = 5000000;
	encInitParam.fpsNum = 30;
	encInitParam.fpsDen = 1;
	//	Rate Control
	encInitParam.enableRC = 1;		//	Enable Rate Control
	encInitParam.enableSkip = 0;	//	Enable Skip
	encInitParam.maxQScale = 51;	//	Max Qunatization Scale
	encInitParam.userQScale = 23;	//	Default Encoder API ( enableRC == 0 )

	NX_VidEncInit( hEnc, &encInitParam );
	if( outFd )
	{
		int size;
		unsigned char *seqBuffer = (unsigned char *)malloc(2048);
		//	Write Sequence Data
		NX_VidEncGetSeqInfo( hEnc, seqBuffer, &size );
		fwrite( seqBuffer, 1, size, outFd );
		free (seqBuffer);
	}

#ifdef ENABLE_DISPLAY

	dspX = 0;
	dspY = 0;
	dspWidth  = pReader->video_stream->codec->coded_width;
	dspHeight = pReader->video_stream->codec->coded_height;

#ifdef ENABLE_MLC_SCALER
	xRatio = (double)LCD_WIDTH / (double)pReader->video_stream->codec->coded_width;
	yRatio = (double)LCD_HEIGHT / (double)pReader->video_stream->codec->coded_height;

	if( xRatio > yRatio )		//	Heigth
	{
		dspWidth = pReader->video_stream->codec->coded_width * yRatio;
		dspHeight = LCD_HEIGHT;
		dspX = abs(LCD_WIDTH - dspWidth)/2;
	}
	else
	{
		dspWidth = LCD_WIDTH;
		dspHeight = pReader->video_stream->codec->coded_height * xRatio;
		dspY = abs(LCD_HEIGHT - dspHeight)/2;
	}
#endif	//	ENABLE_MLC_SCALER

	dspInfo.port = 0;
	dspInfo.module = 0;
	dspInfo.width = pReader->video_stream->codec->coded_width;
	dspInfo.height = pReader->video_stream->codec->coded_height;
	dspInfo.left = dspX;
	dspInfo.top = dspY;
	dspInfo.right = dspWidth+dspX;
	dspInfo.bottom = dspHeight+dspY;
	hDsp = NX_DspInit( &dspInfo );
	if( hDsp == NULL )
	{
		printf("Display Failed!!!\n");
		exit(-1);
	}
#endif	//	ENABLE_DISPLAY

	while( 1 )
	{
		if( 0 == bInit )
		{
			if( seqSize > 0 )
				memcpy( streamBuffer, seqData, seqSize );

			if( 0 != ReadStream( pReader, pReader->video_stream,  (streamBuffer+seqSize), &readSize, &isKey ) )
			{
				break;
			}

			if( needKey )
			{
				if( !isKey )
					continue;
				needKey = 0;
			}

			if( 0 != NX_VidDecInit( hDec, streamBuffer, readSize+seqSize, 0, 0, &seqOut ) )
			{
				//printf("Initialize Failed!!!\n");
				return -1;
			}

			pos = 0;
			bInit = 1;
		}

#if 0
		av_init_packet( &pkt );
		if( 0 != ReadStream2( pReader, pReader->video_stream, &pkt ) )
		{
			break;
		}

		gettimeofday( &startTime, NULL );
		vidRet = NX_VidDecDecodeFrame( hDec, pkt.data, pkt.size, &decOut );
		gettimeofday( &endTime, NULL );
		totalDecTime += (endTime.tv_sec - startTime.tv_sec)*1000000 + endTime.tv_usec - startTime.tv_usec;
		av_free_packet( &pkt );
#else
		if( 0 != ReadStream( pReader, pReader->video_stream,  (streamBuffer+seqSize), &readSize, &isKey ) )
		{
			break;
		}
		pos += readSize;

		gettimeofday( &startTime, NULL );
		vidRet = NX_VidDecDecodeFrame( hDec, streamBuffer, pos, &decOut );
		gettimeofday( &endTime, NULL );
		totalDecTime += (endTime.tv_sec - startTime.tv_sec)*1000000 + endTime.tv_usec - startTime.tv_usec;
#endif
		pos = 0;
		if( vidRet == VID_NEED_MORE_BUF )
		{
			frameCount ++;
			printf("VID_NEED_MORE_BUF\n");
			continue;
		}
		if( vidRet < 0 )
		{
			printf("Decoding Error!!!\n");
			exit(-2);
		}
		//printf("decOut.outImgIdx = %d\n", decOut.outImgIdx);
		if( decOut.outImgIdx >= 0  )
		{
			gettimeofday( &startTime, NULL );
			NX_VidEncEncodeFrame( hEnc, &decOut.outImg, &encOut );
			if( outFd && encOut.bufSize>0 )
			{
				//	Write Sequence Data
				fwrite( encOut.outBuf, 1, encOut.bufSize, outFd );
			}
			gettimeofday( &endTime, NULL );
			totalEncTime += (endTime.tv_sec - startTime.tv_sec)*1000000 + (endTime.tv_usec - startTime.tv_usec);

#ifdef ENABLE_DISPLAY
			NX_DspQueueBuffer( hDsp, &decOut.outImg );
			if( outCount != 0 )
			{
				NX_DspDequeueBuffer( hDsp );
			}
#endif
			outCount ++;

			if( prevIdx != -1 )
			{
				NX_VidDecClrDspFlag( hDec, &decOut.outImg, prevIdx );
			}
			prevIdx = decOut.outImgIdx;
		}

		//if( outCount == 1000 )
		//	break;
	}
	NX_VidDecClose( hDec );

	if( outFd )
	{
		fclose( outFd );
	}

	NX_VidEncClose( hEnc );

	gettimeofday(&totEnd, NULL);
	totalTime = (totEnd.tv_sec - totStart.tv_sec)*1000000 + totEnd.tv_usec - totStart.tv_usec;

	fps = (double)outCount * 1000000. / (double)totalTime;

	printf("Total Frame Count = %d, DecTime = %lld usec, EncTime = %lld usec, totalTime = %lld, %3.3f fps\n",
		outCount, totalDecTime, totalEncTime, totalTime );
#ifdef ENABLE_DISPLAY
	NX_DspClose(hDsp);
#endif
	return 0;
}


int main( int argc, char *argv[] )
{
	if( argc < 3 )
	{
		printf("Usage : %s [Input File] [Output File]\n", argv[0]);
		return -1;
	}

	return transcoding( argv[1], argv[2] );

}
