//
//  ff_play_def.c
//  IJKMediaPlayerKit
//
//  Created by Reach Matt on 2022/5/16.
//

#include "ff_ffplay_def.h"
#include "ff_packet_list.h"
#include "ff_frame_queue.h"

int decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond) {
    memset(d, 0, sizeof(Decoder));
    d->pkt = av_packet_alloc();
    if (!d->pkt)
        return AVERROR(ENOMEM);
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;
    d->pkt_serial = 1;
    d->first_frame_decoded_time = SDL_GetTickHR();
    d->first_frame_decoded = 0;
    d->after_seek_frame = 0;
    SDL_ProfilerReset(&d->decode_profiler, -1);
    return 0;
}

int decoder_start(Decoder *d, int (*fn)(void *), void *arg, const char *name)
{
    packet_queue_start(d->queue);
    d->decoder_tid = SDL_CreateThreadEx(&d->_decoder_tid, fn, arg, name);
    if (!d->decoder_tid) {
        av_log(NULL, AV_LOG_ERROR, "SDL_CreateThread(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    return 0;
}

void decoder_destroy(Decoder *d)
{
    av_packet_free(&d->pkt);
    avcodec_free_context(&d->avctx);
}

void decoder_abort(Decoder *d, FrameQueue *fq)
{
    packet_queue_abort(d->queue);
    frame_queue_signal(fq);
    SDL_WaitThread(d->decoder_tid, NULL);
    d->decoder_tid = NULL;
    packet_queue_flush(d->queue);
}
