//
//  mr_stream_component.c
//
// ijkplayer not use the file, but the file will be used by other module in app.
//
//  Created by Reach Matt on 2023/9/7.
//

#include "mr_stream_component.h"
#include "ff_frame_queue.h"
#include "ff_packet_list.h"

typedef struct MRStreamComponent{
    PacketQueue* packetq;
    Decoder decoder;
    FrameQueue* frameq;
    AVFormatContext *ic;
    int64_t seek_req;
    AVPacket *pkt;
    int st_idx;
    int eof;
}MRStreamComponent;

static int stream_has_enough_packets(PacketQueue *queue, int min_frames)
{
    return queue->abort_request || queue->nb_packets > min_frames;
}

static int read_enough_packets(MRStreamComponent *sc)
{
    if (!sc || sc->eof || !sc->ic) {
        goto end;
    }
    
    do {
        if (stream_has_enough_packets(sc->packetq, 5)) {
            break;
        }
        sc->pkt->flags = 0;
        int ret = av_read_frame(sc->ic, sc->pkt);
        if (ret >= 0) {
            if (sc->pkt->stream_index != sc->st_idx) {
                av_packet_unref(sc->pkt);
            } else {
                packet_queue_put(sc->packetq, sc->pkt);
            }
            continue;
        } else if (ret == AVERROR_EOF) {
            packet_queue_put_nullpacket(sc->packetq, sc->pkt, sc->st_idx);
            sc->eof = 1;
            break;
        } else {
            break;
        }
    } while (sc->packetq->abort_request == 0);
    
end:
    return stream_has_enough_packets(sc->packetq, 0);
}

static int fetch_a_packet(MRStreamComponent *sc, Decoder *d)
{
    while (sc->packetq->abort_request == 0) {
        if (sc->seek_req >= 0) {
            av_log(NULL, AV_LOG_DEBUG,"sub seek to:%lld\n",fftime_to_seconds(sc->seek_req));
            if (avformat_seek_file(sc->ic, -1, INT64_MIN, sc->seek_req, INT64_MAX, 0) < 0) {
                av_log(NULL, AV_LOG_WARNING, "%d: could not seek to position %lld\n",
                       sc->st_idx, sc->seek_req);
                sc->seek_req = -1;
                return -2;
            }
            sc->seek_req = -1;
            packet_queue_flush(sc->packetq);
            continue;
        }
        
        int r = packet_queue_get(d->queue, d->pkt, 0, &d->pkt_serial);
        if (r < 0) {
            return -1;
        } else if (r == 0) {
            if (read_enough_packets(sc) > 0) {
                continue;
            } else {
                av_usleep(1000 * 3);
            }
        } else {
            return 0;
        }
    }
    return -3;
}

static int decoder_decode_frame(MRStreamComponent *sc, AVFrame *frame, AVSubtitle *sub) {
    
    Decoder *d = &sc->decoder;
    int status = 0;
    
    for (;sc->packetq->abort_request == 0;) {
        
        if (d->queue->serial == d->pkt_serial) {
            
            int ret = AVERROR(EAGAIN);
            do {
                if (d->queue->abort_request) {
                    status = -1;
                    goto abort_end;
                }
                
                switch (d->avctx->codec_type) {
                    case AVMEDIA_TYPE_VIDEO:
                        ret = avcodec_receive_frame(d->avctx, frame);
                        if (ret >= 0) {
                            frame->pts = frame->best_effort_timestamp;
                            if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
                                /* retrieve data from GPU to CPU */
                                AVFrame *sw_frame = av_frame_alloc();
                                //use av_hwframe_map instead of av_hwframe_transfer_data
                                if ((ret = av_hwframe_map(sw_frame, frame, 0)) < 0) {
                                    av_log(NULL, AV_LOG_ERROR, "Error transferring the data to system memory\n");
                                }
                                av_frame_unref(frame);
                                av_frame_move_ref(frame, sw_frame);
                                av_frame_free(&sw_frame);
                            }
                        }
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                    {
                        ret = avcodec_receive_frame(d->avctx, frame);
                        if (ret >= 0) {
                            AVRational tb = (AVRational){1, frame->sample_rate};
                            if (frame->pts != AV_NOPTS_VALUE)
                                frame->pts = av_rescale_q(frame->pts, d->avctx->pkt_timebase, tb);
                            else if (d->next_pts != AV_NOPTS_VALUE)
                                frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                            if (frame->pts != AV_NOPTS_VALUE) {
                                d->next_pts = frame->pts + frame->nb_samples;
                                d->next_pts_tb = tb;
                            }
                        }
                    }
                        break;
                    default:
                        break;
                }
                
                if (ret >= 0) {
                    status = 1;
                    goto abort_end;
                } else if (ret == AVERROR_EOF) {
                    d->finished = d->pkt_serial;
                    avcodec_flush_buffers(d->avctx);
                    status = 0;
                    goto abort_end;
                }
            } while (ret != AVERROR(EAGAIN));
        } else {
            if (d->queue->abort_request) {
                status = -1;
                goto abort_end;
            }
        }
        
        do {
            if (d->packet_pending) {
                d->packet_pending = 0;
            } else {
                int old_serial = d->pkt_serial;
                if (fetch_a_packet(sc, d) < 0) {
                    status = -2;
                    goto abort_end;
                }
                if (old_serial != d->pkt_serial) {
                    avcodec_flush_buffers(d->avctx);
                    d->finished = 0;
                    d->hw_failed_count = 0;
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            }
            if (d->queue->serial == d->pkt_serial)
                break;
            av_packet_unref(d->pkt);
        } while (sc->packetq->abort_request == 0);
        
        if (d->avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            int got_frame = 0;
            int ret = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, d->pkt);
            if (ret < 0) {
                ret = AVERROR(EAGAIN);
            } else {
                if (got_frame && !d->pkt->data) {
                    d->packet_pending = 1;
                }
                ret = got_frame ? 0 : (d->pkt->data ? AVERROR(EAGAIN) : AVERROR_EOF);
            }
            if (ret >= 0) {
                status = 1;
                goto abort_end;
            } else if (ret == AVERROR_EOF) {
                d->finished = d->pkt_serial;
                avcodec_flush_buffers(d->avctx);
                status = 0;
                goto abort_end;
            }
            av_packet_unref(d->pkt);
        } else {
            if (d->queue->abort_request){
                status = -1;
                goto abort_end;
            }
            int send = avcodec_send_packet(d->avctx, d->pkt);
            if (send == AVERROR(EAGAIN)) {
                av_log(d->avctx, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                d->packet_pending = 1;
            } else {
                av_packet_unref(d->pkt);
                
                if (send != 0) {
                    char errbuf[128] = { '\0' };
                    av_strerror(send, errbuf, sizeof(errbuf));
                    av_log(d->avctx, AV_LOG_ERROR, "avcodec_send_packet failed:%s(%d).\n",errbuf,send);
                }
            }
        }
        
    }
    
abort_end:
    if (d->queue->abort_request && status == -1) {
        av_log(NULL, AV_LOG_INFO, "will destroy avcodec:%d,flush buffers.\n",d->avctx->codec_type);
        avcodec_send_packet(d->avctx, NULL);
        avcodec_flush_buffers(d->avctx);
    }
    return status;
}

static int get_video_frame(MRStreamComponent *sc, AVFrame *frame)
{
    return decoder_decode_frame(sc, frame, NULL);
}

static int get_audio_frame(MRStreamComponent *sc, AVFrame *frame)
{
    return decoder_decode_frame(sc, frame, NULL);
}

static int get_subtitle_frame(MRStreamComponent *sc, AVSubtitle *pkt)
{
    return decoder_decode_frame(sc, NULL, pkt);
}

static int sub_component_thread(void *arg)
{
    MRStreamComponent *sc = arg;
    int ret = 0;
    AVFrame *frame = av_frame_alloc();
    for (;sc->packetq->abort_request == 0;) {
        
        switch (sc->decoder.avctx->codec_type) {
            case AVMEDIA_TYPE_VIDEO: {
                if (get_video_frame(sc, frame) > 0) {
                    AVStream *stream = sc->ic->streams[sc->st_idx];
                    AVRational tb = stream->time_base;
                    AVRational frame_rate = av_guess_frame_rate(sc->ic, stream, NULL);
                    double duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);
                    double pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                    
                    Frame *sp = frame_queue_peek_writable(sc->frameq);
                    if (!sp)
                        return 0;
                    
                    sp->pos = frame->pkt_pos;
                    sp->pts = pts;
                    sp->duration = duration;
                    sp->serial = sc->decoder.pkt_serial;
                    sp->sar = frame->sample_aspect_ratio;
                    av_frame_move_ref(sp->frame, frame);
                    frame_queue_push(sc->frameq);
                } else {
                    av_usleep(10);
                    ret = -1;
                }
                break;
            }
                break;
            case AVMEDIA_TYPE_AUDIO: {
                if (get_audio_frame(sc, frame) > 0) {
                    AVRational tb = (AVRational){1, frame->sample_rate};
                    
                    Frame *sp = frame_queue_peek_writable(sc->frameq);
                    if (!sp)
                        return 0;
                    sp->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                    sp->pos = frame->pkt_pos;
                    sp->serial = sc->decoder.pkt_serial;
                    sp->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});
                    av_frame_move_ref(sp->frame, frame);
                    frame_queue_push(sc->frameq);
                } else {
                    av_usleep(10);
                    ret = -1;
                }
                break;
            }
            case AVMEDIA_TYPE_SUBTITLE: {
                Frame *sp = frame_queue_peek_writable(sc->frameq);
                if (!sp)
                    return 0;
                if (get_subtitle_frame(sc, &sp->sub) > 0) {
                    
                    int serial = sc->decoder.pkt_serial;
                    if (sc->packetq->serial == serial) {
                        Frame *sp = frame_queue_peek_writable(sc->frameq);
                        if (!sp)
                            return 0;
                        
                        double pts = 0;
                        if (sp->sub.pts != AV_NOPTS_VALUE)
                            pts = sp->sub.pts / (double)AV_TIME_BASE;
                        sp->pts = pts;
                        sp->serial = serial;
                        sp->width  = sc->decoder.avctx->width;
                        sp->height = sc->decoder.avctx->height;
                        sp->shown = 0;
                        frame_queue_push(sc->frameq);
                        //av_log(NULL, AV_LOG_DEBUG,"sub received frame:%f\n",pts);
                    } else {
                        av_log(NULL, AV_LOG_DEBUG,"sub stream push old frame:%d\n",serial);
                    }
                } else {
                    av_usleep(10);
                    ret = -1;
                }
                break;
            }
            default:
            {
                ret = -1;
            }
                break;
        }
    }
    av_frame_free(&frame);
    return ret;
}

int streamComponent_open(MRStreamComponent **scp, int stream_index, AVFormatContext* ic, AVCodecContext *avctx, PacketQueue* packetq, FrameQueue* frameq)
{
    if (!scp) {
        return -1;
    }
    
    MRStreamComponent *sc = av_mallocz(sizeof(MRStreamComponent));
    if (!sc) {
        return -2;
    }
    
    assert(frameq);
    assert(packetq);
    
    sc->frameq = frameq;
    sc->packetq = packetq;
    sc->seek_req = -1;
    sc->ic = ic;
    sc->pkt = av_packet_alloc();
    sc->eof = 0;
    int ret = decoder_init(&sc->decoder, avctx, sc->packetq, NULL);
    
    if (ret < 0) {
        av_free(sc);
        return ret;
    }
    
    ret = decoder_start(&sc->decoder, sub_component_thread, sc, "ff_sc_dec");
    if (ret < 0) {
        decoder_destroy(&sc->decoder);
        av_free(sc);
        return ret;
    }
    sc->st_idx = stream_index;
    av_log(NULL, AV_LOG_DEBUG, "sub stream opened:%d,serial:%d\n",stream_index,packetq->serial);
    *scp = sc;
    return 0;
}

int streamComponent_close(MRStreamComponent **scp)
{
    if (!scp) {
        return -1;
    }
    
    MRStreamComponent *sub = *scp;
    if (!sub) {
        return -2;
    }
    
    if (sub->st_idx == -1) {
        return -3;
    }
    
    decoder_abort(&sub->decoder, sub->frameq);
    decoder_destroy(&sub->decoder);
    av_packet_free(&sub->pkt);
    av_log(NULL, AV_LOG_DEBUG, "sub stream closed:%d\n",sub->st_idx);
    sub->st_idx = -1;
    av_freep(scp);
    return 0;
}

int streamComponent_get_stream(MRStreamComponent *sc)
{
    if (sc) {
        return sc->st_idx;
    }
    return -1;
}

int streamComponent_seek_to(MRStreamComponent *sc, int sec)
{
    if (!sc || !sc->ic) {
        return -1;
    }
    if (sec < 0) {
        sec = 0;
    }
    sc->seek_req = seconds_to_fftime(sec);
    sc->eof = 0;
    return 0;
}

int streamComponent_get_pkt_serial(MRStreamComponent *sc)
{
    if (!sc || !sc->ic) {
        return -1;
    }
    return sc->decoder.pkt_serial;
}

int streamComponent_eof_and_pkt_empty(MRStreamComponent *sc)
{
    if (!sc) {
        return -1;
    }
    return sc->eof && sc->decoder.finished == sc->packetq->serial && frame_queue_nb_remaining(sc->frameq) == 0;
}
