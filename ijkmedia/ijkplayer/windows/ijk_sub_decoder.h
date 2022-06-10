//
//  ijk_sub_decoder.h
//

#ifndef IJK_SUB_DECODER_H
#define IJK_SUB_DECODER_H

#include <stdint.h>
#include <stdbool.h>


typedef struct AVStream AVStream;
typedef struct AVFormatContext AVFormatContext;
typedef struct AVPacket AVPacket;
typedef struct AVSubtitle AVSubtitle;
typedef struct AVCodecContext AVCodecContext;
typedef struct IjkTextDemuxer IjkTextDemuxer;

typedef struct IjkSubDecoder {
	IjkTextDemuxer	*delegDemuxer;
	AVFormatContext *ic;
	AVCodecContext * avctx;

	int streamIdx;
	char* name;
	AVStream* stream;
	char* codecName;
	int codecID;

	bool abort_request;
	//for video
	int picWidth;
	int picHeight;
	//the position of pts begin,in AV_TIME_BASE fractional seconds
	int64_t startTime;
} IjkSubDecoder;

IjkSubDecoder* ijk_createSubDecoder();

void ijk_dumpStreamFormat(IjkSubDecoder* dec);

char* ijk_getCodecName(IjkSubDecoder* dec);

bool ijk_openSubDecoder(IjkSubDecoder* dec);

void ijk_startSubDecoder(IjkSubDecoder* dec);

void ijk_cancelSubDecoder(IjkSubDecoder* dec);

#endif
