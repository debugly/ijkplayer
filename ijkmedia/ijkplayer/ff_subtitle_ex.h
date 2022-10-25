//
//  ff_subtitle_ex.h
//  IJKMediaPlayerKit
//
//  Created by Reach Matt on 2022/5/16.
//

#ifndef ff_subtitle_ex_h
#define ff_subtitle_ex_h

#include <stdio.h>

typedef struct FFSubtitle FFSubtitle;
typedef struct IJKEXSubtitle IJKEXSubtitle;
typedef struct IjkMediaMeta IjkMediaMeta;
typedef struct FrameQueue FrameQueue;
typedef struct PacketQueue PacketQueue;

int exSub_create(IJKEXSubtitle **subp, FrameQueue * frameq, PacketQueue * pktq);
int exSub_addOnly_subtitle(IJKEXSubtitle *sub, const char *file_name, IjkMediaMeta *meta);
int exSub_add_active_subtitle(IJKEXSubtitle *sub, const char *file_name, IjkMediaMeta *meta);
int exSub_open_file_idx(IJKEXSubtitle *sub, int idx);
int exSub_close_current(IJKEXSubtitle *sub);
void exSub_subtitle_destroy(IJKEXSubtitle **sub);

//when return -1 means has not opened;
int exSub_get_opened_stream_idx(IJKEXSubtitle *sub);
//when return zero means succ;
int exSub_seek_to(IJKEXSubtitle *sub, float sec);
int exSub_contain_streamIdx(IJKEXSubtitle *sub, int idx);

#endif /* ff_subtitle_ex_h */
