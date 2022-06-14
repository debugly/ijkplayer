//
//  ijk_text_demuxer.h
//  IJKMediaPlayer



#ifndef IJK_TEXT_DEMUXER_H
#define IJK_TEXT_DEMUXER_H

#ifdef  __cplusplus  
extern "C" {
#endif 

#include <stdint.h>
#include <stdbool.h>


#define kMovieSubFmt "kMovieSubFmt"

typedef struct IjkTextDemuxer IjkTextDemuxer;
typedef struct IjkSubDecoder IjkSubDecoder;
typedef struct AVPacket AVPacket;
typedef struct AVSubtitle AVSubtitle;

#define ITEM_NUM 5
typedef void(*OnStreamOpenedBlock)(IjkTextDemuxer* demux, wchar_t** pairs);
typedef void (*OnFrameComingBlock)(IjkTextDemuxer*, wchar_t*, uint32_t);
typedef void (*OnFinishedBlock)(IjkTextDemuxer*, wchar_t*);


int decoderWantAPacket(IjkTextDemuxer* demux, IjkSubDecoder* decoder, AVPacket* pkt);
bool decoderHasMorePacket(IjkTextDemuxer* demux);
void decoderReceivedASub(IjkTextDemuxer* demux, IjkSubDecoder* decoder, AVSubtitle* sub);

IjkTextDemuxer* Ijk_createTextDemuxer(const char*, OnStreamOpenedBlock, OnFinishedBlock, OnFrameComingBlock);

///开始提取
void Ijk_startTextDemuxer(IjkTextDemuxer* demux);
///停止读包
void Ijk_stopTextDemuxer(IjkTextDemuxer* demux);

#ifdef  __cplusplus  
}
#endif

#endif /* IJK_TEXT_DEMUXER_H */
