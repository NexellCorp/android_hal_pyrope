#ifndef __DecodeFrame_h__
#define __DecodeFrame_h__

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


int NX_DecodeAvcFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeMpeg4Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeDiv3Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeRVFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeVC1Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);

int InitializeCodaVpu(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, unsigned char *buf, int size);


#endif	//	__DecodeFrame_h__
