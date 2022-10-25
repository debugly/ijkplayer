//
//  ff_sub_component.h
//  IJKMediaPlayerKit
//
//  Created by Reach Matt on 2022/5/20.
//

#ifndef ff_sub_component_h
#define ff_sub_component_h

#include <stdio.h>

typedef struct FFSubComponent FFSubComponent;
typedef struct AVStream AVStream;
typedef struct AVCodecContext AVCodecContext;
typedef struct PacketQueue PacketQueue;
typedef struct FrameQueue FrameQueue;
typedef struct AVFormatContext AVFormatContext;
//when hasn't ic, not support seek;
int subComponent_open(FFSubComponent **subp, int stream_index, AVFormatContext* ic, AVCodecContext *avctx, PacketQueue* packetq, FrameQueue* frameq);
int subComponent_close(FFSubComponent **subp);
int subComponent_get_stream(FFSubComponent *sub);
int subComponent_seek_to(FFSubComponent *sub, int sec);

#endif /* ff_sub_component_h */
