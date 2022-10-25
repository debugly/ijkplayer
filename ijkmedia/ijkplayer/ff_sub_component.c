//
//  ff_sub_component.c
//  IJKMediaPlayerKit
//
//  Created by Reach Matt on 2022/5/24.
//

#include "ff_sub_component.h"
#include "ff_frame_queue.h"
#include "ff_packet_list.h"

typedef struct FFSubComponent{
    int st_idx;
    PacketQueue* packetq;
    Decoder decoder;
    FrameQueue* frameq;
    AVFormatContext *ic;
    int64_t seek_req;
    AVPacket *pkt;
    int eof;
}FFSubComponent;

static int stream_has_enough_packets(PacketQueue *queue, int min_frames)
{
    return queue->abort_request || queue->nb_packets > min_frames;
}

static int read_packets(FFSubComponent *sub)
{
    if (!sub) {
        return -1;
    }
    
    if (sub->eof) {
        return -2;
    }
    
    if (sub->ic) {
        sub->pkt->flags = 0;
        do {
            if (stream_has_enough_packets(sub->packetq, 5)) {
                return 1;
            }
            int ret = av_read_frame(sub->ic, sub->pkt);
            if (ret >= 0) {
                if (sub->pkt->stream_index != sub->st_idx) {
                    av_packet_unref(sub->pkt);
                    continue;
                }
                packet_queue_put(sub->packetq, sub->pkt);
                continue;
            } else if (ret == AVERROR_EOF) {
                packet_queue_put_nullpacket(sub->packetq, sub->pkt->stream_index);
                sub->eof = 1;
                return 1;
            } else {
                return -3;
            }
        } while (1);
    }
    return -4;
}

static int get_packet(FFSubComponent *sub, Decoder *d)
{
    while (1) {
        
        if (sub->seek_req >= 0) {
            if (avformat_seek_file(sub->ic, -1, INT64_MIN, sub->seek_req, INT64_MAX, 0) < 0) {
                av_log(NULL, AV_LOG_WARNING, "%d: could not seek to position %lld\n",
                       sub->st_idx, sub->seek_req);
                sub->seek_req = -1;
                return -2;
            }
            sub->seek_req = -1;
            packet_queue_flush(sub->packetq);
            continue;
        }
        
        int r = packet_queue_get(d->queue, d->pkt, 0, &d->pkt_serial);
        if (r < 0) {
            return -1;
        } else if (r == 0) {
            if (read_packets(sub) >= 0) {
                continue;
            } else {
                av_usleep(1000 * 3);
            }
        } else {
            return 0;
        }
    }
}

static int decode_a_frame(FFSubComponent *c, Decoder *d, AVSubtitle *sub)
{
    int ret = AVERROR(EAGAIN);

    for (;;) {
        
        do {
            if (d->packet_pending) {
                d->packet_pending = 0;
            } else {
                int old_serial = d->pkt_serial;
                if (get_packet(c, d) < 0)
                    return -1;
                if (old_serial != d->pkt_serial) {
                    avcodec_flush_buffers(d->avctx);
                    d->finished = 0;
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            }
            if (d->queue->serial == d->pkt_serial)
                break;
            av_packet_unref(d->pkt);
        } while (1);

        int got_frame = 0;
        ret = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, d->pkt);
        if (ret < 0) {
            ret = AVERROR(EAGAIN);
        } else {
            if (got_frame && !d->pkt->data) {
                d->packet_pending = 1;
            }
            ret = got_frame ? 0 : (d->pkt->data ? AVERROR(EAGAIN) : AVERROR_EOF);
        }
        av_packet_unref(d->pkt);
        
        if (ret >= 0)
            return 1;
    }
}

static int subtitle_thread(void *arg)
{
    FFSubComponent *sub = arg;
    Frame *sp;
    int got_subtitle;
    double pts;

    for (;;) {
        if (!(sp = frame_queue_peek_writable(sub->frameq)))
            return 0;
        
        if ((got_subtitle = decode_a_frame(sub, &sub->decoder, &sp->sub)) < 0)
            break;

        pts = 0;
        if (got_subtitle) {
            if (sp->sub.pts != AV_NOPTS_VALUE)
                pts = sp->sub.pts / (double)AV_TIME_BASE;
            sp->pts = pts;
            sp->serial = sub->decoder.pkt_serial;
            sp->width = sub->decoder.avctx->width;
            sp->height = sub->decoder.avctx->height;
            sp->uploaded = 0;

            /* now we can update the picture count */
            frame_queue_push(sub->frameq);
        }
    }
    return 0;
}

int subComponent_open(FFSubComponent **subp, int stream_index, AVFormatContext* ic, AVCodecContext *avctx, PacketQueue* packetq, FrameQueue* frameq)
{
    if (!subp) {
        return -1;
    }
    
    FFSubComponent *sub = av_mallocz(sizeof(FFSubComponent));
    if (!sub) {
        return -2;
    }
    
    assert(frameq);
    assert(packetq);
    
    sub->frameq = frameq;
    sub->packetq = packetq;
    sub->seek_req = -1;
    sub->ic = ic;
    sub->pkt = av_packet_alloc();
    sub->eof = 0;
    //packet queue is reused.
    packetq->serial = 0;
    int ret = decoder_init(&sub->decoder, avctx, sub->packetq, NULL);
    
    if (ret < 0) {
        av_free(sub);
        return ret;
    }
    
    ret = decoder_start(&sub->decoder, subtitle_thread, sub, "ff_subtitle_dec");
    if (ret < 0) {
        decoder_destroy(&sub->decoder);
        av_free(sub);
        return ret;
    }
    sub->st_idx = stream_index;
    *subp = sub;
    return 0;
}

int subComponent_close(FFSubComponent **subp)
{
    if (!subp) {
        return -1;
    }
    FFSubComponent *sub = *subp;
    if (!sub) {
        return -2;
    }
    
    if (sub->st_idx == -1) {
        return -3;
    }
    
    av_packet_free(&sub->pkt);
    decoder_abort(&sub->decoder, sub->frameq);
    decoder_destroy(&sub->decoder);
    
    av_freep(subp);
    return 0;
}

int subComponent_get_stream(FFSubComponent *sub)
{
    if (sub) {
        return sub->st_idx;
    }
    return -1;
}

int subComponent_seek_to(FFSubComponent *sub, int sec)
{
    if (!sub || !sub->ic) {
        return -1;
    }
    if (sec < 0) {
        sec = 0;
    }
    sub->seek_req = seconds_to_fftime(sec);
    sub->eof = 0;
    return 0;
}


