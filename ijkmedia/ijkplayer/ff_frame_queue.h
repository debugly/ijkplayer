//
//  ff_frame_queue.h
//  IJKMediaPlayerKit
//
//  Created by Reach Matt on 2022/5/16.
//

#ifndef ff_frame_queue_h
#define ff_frame_queue_h

#include "ff_ffplay_def.h"

void frame_queue_unref_item(Frame *vp);
int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);
void frame_queue_destory(FrameQueue *f);
void frame_queue_signal(FrameQueue *f);
Frame *frame_queue_peek(FrameQueue *f);
Frame *frame_queue_peek_next(FrameQueue *f);
Frame *frame_queue_peek_last(FrameQueue *f);
Frame *frame_queue_peek_writable(FrameQueue *f);
Frame *frame_queue_peek_readable(FrameQueue *f);
void frame_queue_push(FrameQueue *f);
int frame_queue_nb_remaining(FrameQueue *f);
void frame_queue_next(FrameQueue *f);

#endif /* ff_frame_queue_h */
