/*
 * Copyright (c) 2003 Bilibili
 * Copyright (c) 2003 Fabrice Bellard
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ff_ffplay.h"

/**
 * @file
 * simple media player based on the FFmpeg libraries
 */

#include "config.h"
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>

#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"
#include "libavformat/avformat.h"
#include "ijkavformat/ijklas.h"
#if CONFIG_AVDEVICE
#include "libavdevice/avdevice.h"
#endif
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

#if CONFIG_AVFILTER
# include "libavcodec/avcodec.h"
# include "libavfilter/avfilter.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"
#endif

#include "ijksdl/ijksdl_log.h"
#include "ijkavformat/ijkavformat.h"
#include "libavutil/display.h"
#include "ff_fferror.h"
#include "ff_ffpipeline.h"
#include "ff_ffpipenode.h"
#include "ff_ffplay_debug.h"
#include "ijkmeta.h"
#include "ijkversion.h"
#include "ijkplayer.h"
#include "ff_frame_queue.h"
#include "ff_packet_list.h"
#include "ff_subtitle.h"
#include "ijksdl/ijksdl_gpu.h"
#include <stdatomic.h>
#if defined(__ANDROID__)
#include "ijksoundtouch/ijksoundtouch_wrap.h"
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#ifndef AV_CODEC_FLAG2_FAST
#define AV_CODEC_FLAG2_FAST CODEC_FLAG2_FAST
#endif

#ifndef AV_CODEC_CAP_DR1
#define AV_CODEC_CAP_DR1 CODEC_CAP_DR1
#endif

// FIXME: 9 work around NDKr8e or gcc4.7 bug
// isnan() may not recognize some double NAN, so we test both double and float
#if defined(__ANDROID__)
#ifdef isnan
#undef isnan
#endif
#define isnan(x) (isnan((double)(x)) || isnanf((float)(x)))
#endif

#if defined(__ANDROID__)
#define printf(...) ALOGD(__VA_ARGS__)
#endif

#define FFP_IO_STAT_STEP (50 * 1024)

#define FFP_BUF_MSG_PERIOD (3)

// static const AVOption ffp_context_options[] = ...
#include "ff_ffplay_options.h"
#if CONFIG_AVFILTER
// FFP_MERGE: opt_add_vfilter
#endif

#define IJKVERSION_GET_MAJOR(x)     ((x >> 16) & 0xFF)
#define IJKVERSION_GET_MINOR(x)     ((x >>  8) & 0xFF)
#define IJKVERSION_GET_MICRO(x)     ((x      ) & 0xFF)

#if CONFIG_AVFILTER
static inline
int cmp_audio_fmts(enum AVSampleFormat fmt1, int64_t channel_count1,
                   enum AVSampleFormat fmt2, int64_t channel_count2)
{
    /* If channel count == 1, planar and non-planar formats are the same */
    if (channel_count1 == 1 && channel_count2 == 1)
        return av_get_packed_sample_fmt(fmt1) != av_get_packed_sample_fmt(fmt2);
    else
        return channel_count1 != channel_count2 || fmt1 != fmt2;
}

#endif

static void free_picture(Frame *vp);
static double consume_audio_buffer(FFPlayer *ffp, double diff);
static void update_sample_display(FFPlayer *ffp, uint8_t *samples, int samples_size);

static int packet_queue_get_or_buffering(FFPlayer *ffp, PacketQueue *q, AVPacket *pkt, int *serial, int *finished)
{
    assert(finished);
    if (!ffp->packet_buffering)
        return packet_queue_get(q, pkt, 1, serial);

    while (1) {
        int new_packet = packet_queue_get(q, pkt, 0, serial);
        if (new_packet < 0)
            return -1;
        else if (new_packet == 0) {
            if (q->is_buffer_indicator && !*finished)
                ffp_toggle_buffering(ffp, 1);
            new_packet = packet_queue_get(q, pkt, 1, serial);
            if (new_packet < 0)
                return -1;
        }

        if (*finished == *serial) {
            av_packet_unref(pkt);
            continue;
        }
        else
            break;
    }

    return 1;
}

/*
 fix crash by xql: seek continually beyound duration.
 * thread #36, stop reason = EXC_BAD_ACCESS (code=1, address=0x3948cffd0)
   * frame #0: 0x000000010650e31c IJKMediaPlayerKit`av_freep(arg=0x00000003948cffd0) at mem.c:231:5 [opt]
     frame #1: 0x0000000106335878 IJKMediaPlayerKit`ff_videotoolbox_uninit(avctx=<unavailable>) at videotoolbox.c:439:9 [opt]
     frame #2: 0x0000000106335a30 IJKMediaPlayerKit`videotoolbox_uninit(avctx=0x000000034c53bbe0) at videotoolbox.c:1008:5 [opt]
     frame #3: 0x00000001066ec734 IJKMediaPlayerKit`avcodec_close(avctx=0x000000034c53bbe0) at utils.c:1093:13 [opt]
     frame #4: 0x00000001062bf6a0 IJKMediaPlayerKit`avcodec_free_context(pavctx=0x0000000338297c50) at options.c:178:5 [opt]
     frame #5: 0x0000000106019e5c IJKMediaPlayerKit`decoder_destroy(d=0x0000000338297c40) at ff_ffplay_def.c:43:5
     frame #6: 0x0000000105fd47c8 IJKMediaPlayerKit`stream_component_close(ffp=0x0000000334267bd0, stream_index=1) at ff_ffplay.c:669:9
     frame #7: 0x0000000105fb45e0 IJKMediaPlayerKit`stream_close(ffp=0x0000000334267bd0) at ff_ffplay.c:708:9
     frame #8: 0x0000000105fbe5e4 IJKMediaPlayerKit`ffp_wait_stop_l(ffp=0x0000000334267bd0) at ff_ffplay.c:4523:9
     frame #9: 0x000000010606ab6c IJKMediaPlayerKit`ijkmp_shutdown_l(mp=0x000000033425ff30) at ijkplayer.c:301:9
     frame #10: 0x000000010606abac IJKMediaPlayerKit`ijkmp_shutdown(mp=0x000000033425ff30) at ijkplayer.c:308:12
     frame #11: 0x00000001060381ac IJKMediaPlayerKit`-[IJKFFMoviePlayerController shutdownWaitStop:](self=0x00000003295fbeb0, _cmd="shutdownWaitStop:", mySelf=0x00000003295fbeb0) at IJKFFMoviePlayerController.m:591:5
     frame #12: 0x000000019ff61470 Foundation`__NSThread__start__ + 716
     frame #13: 0x00000001045d95d4 libsystem_pthread.dylib`_pthread_start + 148
 * thread #36, stop reason = EXC_BAD_ACCESS (code=1, address=0x3948cffd0)
   * frame #0: 0x000000010650e31c IJKMediaPlayerKit`av_freep(arg=0x00000003948cffd0) at mem.c:231:5 [opt]
     frame #1: 0x0000000106335878 IJKMediaPlayerKit`ff_videotoolbox_uninit(avctx=<unavailable>) at videotoolbox.c:439:9 [opt]
     frame #2: 0x0000000106335a30 IJKMediaPlayerKit`videotoolbox_uninit(avctx=0x000000034c53bbe0) at videotoolbox.c:1008:5 [opt]
     frame #3: 0x00000001066ec734 IJKMediaPlayerKit`avcodec_close(avctx=0x000000034c53bbe0) at utils.c:1093:13 [opt]
     frame #4: 0x00000001062bf6a0 IJKMediaPlayerKit`avcodec_free_context(pavctx=0x0000000338297c50) at options.c:178:5 [opt]
     frame #5: 0x0000000106019e5c IJKMediaPlayerKit`decoder_destroy(d=0x0000000338297c40) at ff_ffplay_def.c:43:5
     frame #6: 0x0000000105fd47c8 IJKMediaPlayerKit`stream_component_close(ffp=0x0000000334267bd0, stream_index=1) at ff_ffplay.c:669:9
     frame #7: 0x0000000105fb45e0 IJKMediaPlayerKit`stream_close(ffp=0x0000000334267bd0) at ff_ffplay.c:708:9
     frame #8: 0x0000000105fbe5e4 IJKMediaPlayerKit`ffp_wait_stop_l(ffp=0x0000000334267bd0) at ff_ffplay.c:4523:9
     frame #9: 0x000000010606ab6c IJKMediaPlayerKit`ijkmp_shutdown_l(mp=0x000000033425ff30) at ijkplayer.c:301:9
     frame #10: 0x000000010606abac IJKMediaPlayerKit`ijkmp_shutdown(mp=0x000000033425ff30) at ijkplayer.c:308:12
     frame #11: 0x00000001060381ac IJKMediaPlayerKit`-[IJKFFMoviePlayerController shutdownWaitStop:](self=0x00000003295fbeb0, _cmd="shutdownWaitStop:", mySelf=0x00000003295fbeb0) at IJKFFMoviePlayerController.m:591:5
     frame #12: 0x000000019ff61470 Foundation`__NSThread__start__ + 716
     frame #13: 0x00000001045d95d4 libsystem_pthread.dylib`_pthread_start + 148
 */
static int decoder_decode_frame(FFPlayer *ffp, Decoder *d, AVFrame *frame, AVSubtitle *sub) {
    
    int status = 0;
    for (;;) {
        
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
                            int vdec_type = frame->format == AV_PIX_FMT_VIDEOTOOLBOX ? FFP_PROPV_DECODER_AVCODEC_HW : FFP_PROPV_DECODER_AVCODEC;
                            
                            if (ffp->node_vdec->vdec_type == FFP_PROPV_DECODER_UNKNOWN) {
                                ffp->node_vdec->vdec_type = vdec_type;
                                ffp_notify_msg2(ffp, FFP_MSG_VIDEO_DECODER_OPEN, vdec_type);
                            } else if (ffp->node_vdec->vdec_type != vdec_type) {
                                av_log(d->avctx, AV_LOG_ERROR, "wtf?video codec type changed from %d->%d\n",ffp->node_vdec->vdec_type,vdec_type);
                                ffp->node_vdec->vdec_type = vdec_type;
                                ffp_notify_msg2(ffp, FFP_MSG_VIDEO_DECODER_OPEN, vdec_type);
                            }
                            
//                            static Uint64 bein = 0;
//                            if (bein == 0) {
//                                bein = SDL_GetTickHR();
//                            }
//                            static int count = 0;
//                            count++;
//
//                            /*
//                             vdec 2000 frame cost:
//                             加速：23.699ms 23.782ms 23.755ms
//                                  23.647ms 23.660ms 23.735ms
//                             */
//                            if (count == 2000) {
//                                printf("vdec 2000 frame cost:%0.3fms\n",(SDL_GetTickHR() - bein)/1000.0);
//                                exit(0);
//                            }
                            
                            ffp->stat.vdps = SDL_SpeedSamplerAdd(&ffp->vdps_sampler, FFP_SHOW_VDPS_AVCODEC, "vdps[avcodec]");
                            if (ffp->decoder_reorder_pts == -1) {
                                frame->pts = frame->best_effort_timestamp;
                            } else if (!ffp->decoder_reorder_pts) {
                                frame->pts = frame->pkt_dts;
                            }
                            
                            if (ffp->copy_hw_frame) {
                                if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
                                    /* retrieve data from GPU to CPU */
                                    AVFrame *sw_frame = av_frame_alloc();
                                    //use av_hwframe_map instead of av_hwframe_transfer_data
                                    if ((ret = av_hwframe_map(sw_frame, frame, 0)) < 0) {
                                        fprintf(stderr, "Error transferring the data to system memory\n");
                                    }
                                    av_frame_unref(frame);
                                    av_frame_move_ref(frame, sw_frame);
                                    av_frame_free(&sw_frame);
                                }
                            }
                        }
                        break;
                    case AVMEDIA_TYPE_AUDIO:
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
                        break;
                    default:
                        break;
                }
                if (ret == AVERROR_EOF) {
                    d->finished = d->pkt_serial;
                    avcodec_flush_buffers(d->avctx);
                    status = 0;
                    goto abort_end;
                }
                if (ret >= 0) {
                    status = 1;
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
            if (d->queue->nb_packets == 0)
                SDL_CondSignal(d->empty_queue_cond);
            if (d->packet_pending) {
                d->packet_pending = 0;
            } else {
                int old_serial = d->pkt_serial;
                if (packet_queue_get_or_buffering(ffp, d->queue, d->pkt, &d->pkt_serial, &d->finished) < 0) {
                    status = -1;
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
        } while (1);

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
                if (send == AVERROR_INVALIDDATA
                    || send == AVERROR_UNKNOWN
                    || send == AVERROR_EXTERNAL
                    || send == AVERROR(ENOSYS)
                    || send == AVERROR(EINVAL)) {
                    //Samsung Wonderland Two HDR UHD 4K HDR10.ts decode first frame return AVERROR_INVALIDDATA 
                    //hw accel decode failed greater than twice.
                    if (d->avctx->hw_device_ctx && ++d->hw_failed_count > 2) {
                        ffp_notify_msg2(ffp, FFP_MSG_VIDEO_DECODER_FATAL, send);
                    }
                } else if (send == AVERROR_PATCHWELCOME) {
                    //ffmpeg 4.0 can't decode tiff raw fmt.
                    VideoState *is = ffp->is;
                    if (!is->viddec.first_frame_decoded) {
                        //save the error,when complete play,pass the error to caller.
                        ffp->error = send;
                    }
                } else {
                    //do what?
                }
            }
        }
    }
abort_end:
    if (d->queue->abort_request && status == -1) {
        av_log(NULL, AV_LOG_INFO, "will destroy avcodec:%s,flush buffers.\n",avcodec_get_name(d->avctx->codec_id));
        avcodec_send_packet(d->avctx, NULL);
        avcodec_flush_buffers(d->avctx);
    }
    return status;
}

// FFP_MERGE: fill_rectangle
// FFP_MERGE: fill_border
// FFP_MERGE: ALPHA_BLEND
// FFP_MERGE: RGBA_IN
// FFP_MERGE: YUVA_IN
// FFP_MERGE: YUVA_OUT
// FFP_MERGE: BPP
// FFP_MERGE: blend_subrect

static void free_picture(Frame *vp)
{
    if (vp->bmp) {
        SDL_VoutFreeYUVOverlay(vp->bmp);
        vp->bmp = NULL;
    }
}

//-1: no change. 0:close current. 1:opened new
static int ff_apply_subtitle_stream_change(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    int update_stream;
    int pre_stream;
    int r = ff_sub_update_stream_if_need(is->ffSub, &update_stream, &pre_stream);
    if (r > 0) {
        AVCodecContext * avctx = ff_sub_get_avctx(is->ffSub);
        ffp_set_subtitle_codec_info(ffp, AVCODEC_MODULE_NAME, avcodec_get_name(avctx->codec_id));
        ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_TIMEDTEXT_STREAM, update_stream);
        ffp_notify_msg1(ffp, FFP_MSG_SELECTED_STREAM_CHANGED);
        
        int type = ff_sub_current_stream_type(is->ffSub);
        if (type == 2) {
            //seek the extra subtitle
            float sec = ffp_get_current_position_l(ffp) / 1000 - 1;
            float delay = ff_sub_get_delay(is->ffSub);
            //when exchang stream force seek stream instead of ff_sub_set_delay
            ff_sub_seek_to(is->ffSub, delay, sec);
        }
    } else if (r == 0) {
        ffp_set_subtitle_codec_info(ffp, AVCODEC_MODULE_NAME, "");
        ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_TIMEDTEXT_STREAM, -1);
        ffp_notify_msg1(ffp, FFP_MSG_SELECTED_STREAM_CHANGED);
    } else if (r < -1) {
        //when closed pre stream,need send stream changed msg.
        if (pre_stream >= 0) {
            ffp_set_subtitle_codec_info(ffp, AVCODEC_MODULE_NAME, "");
            ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_TIMEDTEXT_STREAM, -1);
            ffp_notify_msg1(ffp, FFP_MSG_SELECTED_STREAM_CHANGED);
        }
        //send selecting new stream failed msg.
        ffp_notify_msg4(ffp, FFP_MSG_SELECTING_STREAM_FAILED, update_stream, pre_stream, &r, sizeof(r));
    }
    return r;
}

// FFP_MERGE: realloc_texture
// FFP_MERGE: calculate_display_rect
// FFP_MERGE: upload_texture
// FFP_MERGE: video_image_display

static void video_image_display2(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    Frame *vp = frame_queue_peek_last(&is->pictq);
    if (vp->bmp) {
        SDL_TextureOverlay *sub_overlay = NULL;
        if (is->step_on_seeking) {
            //ignore subtitle
        } else if (is->ffSub) {
            int r = ff_apply_subtitle_stream_change(ffp);
            //has stream
            if (ffp->subtitle && ff_sub_get_current_stream(is->ffSub, NULL) >= 0) {
                int got = ff_sub_get_texture(is->ffSub, vp->pts, ffp->gpu, &sub_overlay);
                //when got equal to -100 means the ass subtitle frame not ready,need retry!
                if (!sub_overlay && (r > 0 || got == FF_SUB_PENDING) && is->pause_req) {
                    //give one more chance
                    is->force_refresh_sub_changed = 1;
                    av_usleep(3*1000);
                    return;
                }
            }
        }
        
        if (ffp->render_wait_start && !ffp->start_on_prepared && is->pause_req) {
            if (!ffp->first_video_frame_rendered) {
                ffp->first_video_frame_rendered = 1;
                ffp_notify_msg1(ffp, FFP_MSG_VIDEO_RENDERING_START);
            }
            while (is->pause_req && !is->abort_request) {
                SDL_Delay(20);
            }
        }
        SDL_VoutDisplayYUVOverlay(ffp->vout, vp->bmp, sub_overlay);
        SDL_TextureOverlay_Release(&sub_overlay);
        
        ffp->stat.vfps = SDL_SpeedSamplerAdd(&ffp->vfps_sampler, FFP_SHOW_VFPS_FFPLAY, "vfps[ffplay]");
        if (!ffp->first_video_frame_rendered) {
            ffp->first_video_frame_rendered = 1;
            ffp_notify_msg1(ffp, FFP_MSG_VIDEO_RENDERING_START);
        }

        if (is->latest_video_seek_load_serial == vp->serial) {
            int latest_video_seek_load_serial = __atomic_exchange_n(&(is->latest_video_seek_load_serial), -1, memory_order_seq_cst);
            if (latest_video_seek_load_serial == vp->serial) {
                ffp->stat.latest_seek_load_duration = (av_gettime() - is->latest_seek_load_start_at) / 1000;
                if (ffp->av_sync_type == AV_SYNC_VIDEO_MASTER) {
                    ffp_notify_msg2(ffp, FFP_MSG_VIDEO_SEEK_RENDERING_START, 1);
                } else {
                    ffp_notify_msg2(ffp, FFP_MSG_VIDEO_SEEK_RENDERING_START, 0);
                }
            }
        }
    }
}

// FFP_MERGE: compute_mod
// FFP_MERGE: video_audio_display

static void stream_component_close(FFPlayer *ffp, int stream_index)
{
    VideoState *is = ffp->is;
    AVFormatContext *ic = is->ic;
    AVCodecParameters *codecpar;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    codecpar = ic->streams[stream_index]->codecpar;

    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        decoder_abort(&is->auddec, &is->sampq);
        SDL_AoutCloseAudio(ffp->aout);

        decoder_destroy(&is->auddec);
        swr_free(&is->swr_ctx);
        av_freep(&is->audio_buf1);
        is->audio_buf1_size = 0;
        is->audio_buf = NULL;

#ifdef FFP_MERGE
        if (is->rdft) {
            av_rdft_end(is->rdft);
            av_freep(&is->rdft_data);
            is->rdft = NULL;
            is->rdft_bits = 0;
        }
#endif
        break;
    case AVMEDIA_TYPE_VIDEO:
        decoder_abort(&is->viddec, &is->pictq);
        decoder_destroy(&is->viddec);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = NULL;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        assert(0);
        break;
    default:
        break;
    }
}

static void stream_close(FFPlayer *ffp)
{
    av_log(NULL, AV_LOG_INFO, "stream_close will close\n");
    VideoState *is = ffp->is;
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    is->abort_request = 1;
    packet_queue_abort(&is->videoq);
    packet_queue_abort(&is->audioq);
    ff_sub_abort(is->ffSub);
    SDL_WaitThread(is->read_tid, NULL);
    /* close each stream */
    if (is->audio_stream >= 0)
        stream_component_close(ffp, is->audio_stream);
    if (is->video_stream >= 0)
        stream_component_close(ffp, is->video_stream);
    
    avformat_close_input(&is->ic);
    
    av_log(NULL, AV_LOG_DEBUG, "wait for video_refresh_tid\n");
    SDL_WaitThread(is->video_refresh_tid, NULL);
    ff_sub_destroy(&is->ffSub);
    
    packet_queue_destroy(&is->videoq);
    packet_queue_destroy(&is->audioq);

    /* free all pictures */
    frame_queue_destory(&is->pictq);
    frame_queue_destory(&is->sampq);

    SDL_DestroyCond(is->audio_accurate_seek_cond);
    SDL_DestroyCond(is->video_accurate_seek_cond);
    SDL_DestroyCond(is->continue_read_thread);
    SDL_DestroyMutex(is->accurate_seek_mutex);
    SDL_DestroyMutex(is->play_mutex);
#if !CONFIG_AVFILTER
    sws_freeContext(is->img_convert_ctx);
#endif
#ifdef FFP_MERGE
    sws_freeContext(is->sub_convert_ctx);
#endif

#if defined(__ANDROID__)
    if (ffp->soundtouch_enable && is->handle != NULL) {
        ijk_soundtouch_destroy(is->handle);
    }
#endif
    av_free(is->filename);
    av_free(is);
    ffp->is = NULL;
    av_log(NULL, AV_LOG_INFO, "stream_close did close\n");
}

// FFP_MERGE: do_exit
// FFP_MERGE: sigterm_handler
// FFP_MERGE: video_open
// FFP_MERGE: video_display

/* display the current picture, if any */
static void video_display2(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    if (is->video_st)
        video_image_display2(ffp);
}

static double _get_clock_apply_delay(Clock *c, int apply)
{
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->pts + (apply ? c->extra_delay : 0);
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed) + (apply ? c->extra_delay : 0);
    }
}

static double get_clock(Clock *c)
{
    return _get_clock_apply_delay(c, 0);
}

static double get_clock_with_delay(Clock *c)
{
    return _get_clock_apply_delay(c, 1);
}

static void set_clock_at(Clock *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
#ifdef FFP_SHOW_SYNC_CLOCK
    av_log(NULL,AV_LOG_INFO,"set %s clock %f\n",c->name,c->pts);
#endif
}

static void set_clock(Clock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

static void set_clock_extral_delay(Clock *c, float delay)
{
    c->extra_delay = delay;
}

float get_clock_extral_delay(Clock *c)
{
    return c ? c->extra_delay : 0.0;
}

static void set_clock_speed(Clock *c, double speed)
{
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

static void init_clock(Clock *c, int *queue_serial, char *name)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    strcpy(c->name, name);
    set_clock(c, NAN, -1);
}

static void sync_clock_to_slave(Clock *c, Clock *slave)
{
    double clock = get_clock(c);
    double slave_clock = get_clock(slave);
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, slave->serial);
}

static int get_master_sync_type(VideoState *is) {
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

static double _get_master_clock_apply_delay(VideoState *is, int apply)
{
    double val;

    switch (get_master_sync_type(is)) {
        case AV_SYNC_VIDEO_MASTER:
            val = _get_clock_apply_delay(&is->vidclk, apply);
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = _get_clock_apply_delay(&is->audclk, apply);
            break;
        default:
            val = _get_clock_apply_delay(&is->extclk, apply);
            break;
    }
    return val;
}

static double get_master_clock_with_delay(VideoState *is)
{
    return _get_master_clock_apply_delay(is, 1);
}

/* get the current master clock value */
static double get_master_clock(VideoState *is)
{
    return _get_master_clock_apply_delay(is, 0);
}

static void check_external_clock_speed(VideoState *is) {
   if ((is->video_stream >= 0 && is->videoq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES) ||
       (is->audio_stream >= 0 && is->audioq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES)) {
       set_clock_speed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
   } else if ((is->video_stream < 0 || is->videoq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES) &&
              (is->audio_stream < 0 || is->audioq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES)) {
       set_clock_speed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
   } else {
       double speed = is->extclk.speed;
       if (speed != 1.0)
           set_clock_speed(&is->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
   }
}

/* seek in the stream */
static int stream_seek(VideoState *is, int64_t pos, int64_t rel, int by_bytes)
{
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        else
            is->seek_flags |= AVSEEK_FLAG_BACKWARD;
        is->seek_req = 1;
        is->viddec.start_seek_time = SDL_GetTickHR();
        SDL_CondSignal(is->continue_read_thread);
        return 0;
    }
    return -1;
}

/* pause or resume the video */
static void stream_toggle_pause_l(FFPlayer *ffp, int pause_on)
{
    VideoState *is = ffp->is;
    if (is->paused && !pause_on) {
        is->frame_timer += av_gettime_relative() / 1000000.0 - is->vidclk.last_updated;

#ifdef FFP_MERGE
        if (is->read_pause_return != AVERROR(ENOSYS)) {
            is->vidclk.paused = 0;
        }
#endif
        set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
        set_clock(&is->audclk, get_clock(&is->audclk), is->audclk.serial);
    } else {
    }
    set_clock(&is->extclk, get_clock(&is->extclk), is->extclk.serial);
    if (is->step && (is->pause_req || is->buffering_on)) {
        is->paused = is->vidclk.paused = is->extclk.paused = pause_on;
    } else {
        is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = pause_on;
        SDL_AoutPauseAudio(ffp->aout, pause_on);
    }
}

static void stream_update_pause_l(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    if (!is->step && (is->pause_req || is->buffering_on)) {
        stream_toggle_pause_l(ffp, 1);
    } else {
        stream_toggle_pause_l(ffp, 0);
    }
}

static void toggle_pause_l(FFPlayer *ffp, int pause_on)
{
    VideoState *is = ffp->is;
    //when using step mode ignore pause cmd,otherwize cause stream_toggle_pause_l(ffp, 0);
    if (is->step && pause_on) {
        return;
    }
    
    //pause->play and play->pause update clock;
    //we konw the get_clock return pts after pause.
    //during the period from last update clock to the current pause event, no one has updated the clock (pts),so after pause get_positon is on average 50ms slow(my test movie,audio pts update every 100ms)
    if ((is->pause_req && !pause_on) || (!is->pause_req && pause_on)) {
        set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
        set_clock(&is->audclk, get_clock(&is->audclk), is->audclk.serial);
    }
    
    is->pause_req = pause_on;
    ffp->auto_resume = !pause_on;
    stream_update_pause_l(ffp);
    is->step = 0;
}

static void toggle_pause(FFPlayer *ffp, int pause_on)
{
    SDL_LockMutex(ffp->is->play_mutex);
    toggle_pause_l(ffp, pause_on);
    SDL_UnlockMutex(ffp->is->play_mutex);
}

// FFP_MERGE: toggle_mute
// FFP_MERGE: update_volume

static void step_to_next_frame_l(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    is->step = 1;
    /* if the stream is paused unpause it, then step */
    if (is->paused)
        stream_toggle_pause_l(ffp, 0);
}

void ffp_step_to_next_frame(FFPlayer *ffp)
{
    SDL_LockMutex(ffp->is->play_mutex);
    step_to_next_frame_l(ffp);
    SDL_UnlockMutex(ffp->is->play_mutex);
}

static double compute_target_delay(FFPlayer *ffp, double delay, VideoState *is)
{
    double sync_threshold, diff = 0;
    
    /* skip or repeat frame. We take into account the
       delay to compute the threshold. I still don't know
       if it is the best guess */
    sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
    
    /* update delay to follow master synchronisation source */
    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        
        switch (get_master_sync_type(is)) {
            case AV_SYNC_VIDEO_MASTER:
                diff = 0;
                break;
            case AV_SYNC_AUDIO_MASTER:
            {
                if (is->eof && is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0) {
                    //auido is done,but video has many frames,maybe the is->frame_timer + delay is greather  than current time,cause repaet current video frame forever.
                    delay = delay / ffp->pf_playback_rate;
                    diff = 0;
                } else {
                    diff = get_clock(&is->vidclk) - get_clock_with_delay(&is->audclk);
                }
            }
                break;
            default:
                diff = get_clock(&is->vidclk) - get_clock_with_delay(&is->extclk);
                break;
        }
    
        /* -- by bbcallen: replace is->max_frame_duration with AV_NOSYNC_THRESHOLD */
        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }
    //for only video stream movie,using playbackRate speed play.
    else if (fabsf(ffp->pf_playback_rate) > 0.00001){
        delay = delay / ffp->pf_playback_rate;
    }

    if (ffp) {
        ffp->stat.avdelay = delay;
        ffp->stat.vmdiff  = diff;
    }
#ifdef FFP_SHOW_AUDIO_DELAY
    av_log(NULL, AV_LOG_INFO, "video: delay=%0.3f A-V=%f\n",
            delay, -diff);
#endif

    return delay;
}

static double vp_duration(VideoState *is, Frame *vp, Frame *nextvp) {
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (isnan(duration) || duration <= 0 || duration > is->max_frame_duration)
            return vp->duration;
        else
            return duration;
    } else {
        return 0.0;
    }
}

static void update_video_pts(VideoState *is, double pts, int64_t pos, int serial) {
    /* update current video pts */
    set_clock(&is->vidclk, pts, serial);
    sync_clock_to_slave(&is->extclk, &is->vidclk);
}

/* called to display each frame */
static void video_refresh(FFPlayer *opaque, double *remaining_time)
{
    FFPlayer *ffp = opaque;
    VideoState *is = ffp->is;
    double time;
    
    //applay subtitle preference changed when the palyer was paused.
    if (is->paused && is->force_refresh_sub_changed) {
        is->force_refresh_sub_changed = 0;
        video_display2(ffp);
        return;
    }
    
    if (!is->paused && get_master_sync_type(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime)
        check_external_clock_speed(is);

    if (!ffp->display_disable && is->show_mode != SHOW_MODE_VIDEO && is->audio_st) {
        time = av_gettime_relative() / 1000000.0;
        if (is->force_refresh || is->last_vis_time + ffp->rdftspeed < time) {
            video_display2(ffp);
            is->last_vis_time = time;
        }
        *remaining_time = FFMIN(*remaining_time, is->last_vis_time + ffp->rdftspeed - time);
    }
    
    if (ffp->audio_disable && ffp->display_disable) {
        frame_queue_next(&is->pictq);
        ff_sub_drop_old_frames(is->ffSub);
        return;
    }
    
    if (is->video_st) {
retry:
        if (frame_queue_nb_remaining(&is->pictq) == 0) {
            /*
             fix fix after seek near the end of the video, not play problem.
             when no picture to display in the queue and video is finished,
             need send AFTER_SEEK notifi,because top level is paused.
             */
            if (is->viddec.finished == is->videoq.serial && is->viddec.after_seek_frame) {
                int du = (int)(SDL_GetTickHR() - is->viddec.start_seek_time);
                is->viddec.after_seek_frame = 0;
                ffp_notify_msg2(ffp, FFP_MSG_AFTER_SEEK_FIRST_FRAME, du);
            }
            //when no video frame can step display,need pause play,otherwise cause FFP_MSG_COMPLETED.
            if (is->eof && is->viddec.finished == is->videoq.serial) {
                SDL_LockMutex(ffp->is->play_mutex);
                if (is->step) {
                    is->step = 0;
                    if (!is->paused)
                        stream_update_pause_l(ffp);
                } else if (is->step_on_seeking) {
                    is->step_on_seeking = 0;
                }
                SDL_UnlockMutex(ffp->is->play_mutex);
            }
        } else {
            double last_duration, duration, delay;
            Frame *vp, *lastvp;

            /* dequeue the picture */
            lastvp = frame_queue_peek_last(&is->pictq);
            vp = frame_queue_peek(&is->pictq);

            //when fast seek,we want update video frame,no drop frame. but we can't identify seek is continuously.
            if (vp->serial != is->videoq.serial) {
                frame_queue_next(&is->pictq);
                ff_sub_drop_old_frames(is->ffSub);
                goto retry;
            }

            if (lastvp->serial != vp->serial)
                is->frame_timer = av_gettime_relative() / 1000000.0;

            if (is->paused && !is->step_on_seeking)
                goto display;
            /*
             fix after finish seek, video picture display slowly a few seconds bug.
             video and audio pts are NAN,usually video is firstly display,at this time video picture delay is zero, so we wait until audio clock right,because we need use auido sync video.
             check audioq.duration avoid video picture wait audio forever.
             */
            if (!is->step_on_seeking && !is->step && get_master_sync_type(is) == AV_SYNC_AUDIO_MASTER && !is->audio_accurate_seek_req) {
                if (is->audio_stream >= 0 && isnan(get_master_clock(is)) && is->auddec.finished != is->audioq.serial && vp->pts > 0 && is->audioq.duration > 1) {
                    av_usleep(1000);
                    av_log(NULL,AV_LOG_DEBUG,"wait master clock,video pts is:%0.3f,serial:%d\n", vp->pts, vp->serial);
                    goto display;
                }
            }
            
            /* compute nominal last_duration */
            last_duration = vp_duration(is, lastvp, vp);
            delay = compute_target_delay(ffp, last_duration, is);
            
            //This strategy is only used when the audio track delay is set and not step and not step_on_seeking, otherwise step play failed.
            double audio_delay = is->audio_st ? get_clock_extral_delay(&is->audclk) : 0;
            if (!is->step && !is->step_on_seeking && audio_delay != 0) {
                //video is later,so use drop frame instead of dispaly quickly
                if (ffp->stat.vmdiff < -AV_SYNC_THRESHOLD_MAX && delay < 0.001) {
                    av_log(NULL,AV_LOG_INFO,"video is later,drop video:%0.3f,audio clk:%0.3f\n", vp->pts, get_clock_with_delay(&is->audclk));
                    SDL_LockMutex(is->pictq.mutex);
                    if (!isnan(vp->pts))
                        update_video_pts(is, vp->pts, vp->pos, vp->serial);
                    SDL_UnlockMutex(is->pictq.mutex);
                    frame_queue_next(&is->pictq);
                    goto retry;
                }
                //audio is later, so keep current frame for waiting
                else if (ffp->stat.vmdiff > AV_SYNC_THRESHOLD_MIN*2) {
                    SDL_LockMutex(is->pictq.mutex);
                    if (!isnan(vp->pts))
                        update_video_pts(is, lastvp->pts, lastvp->pos, lastvp->serial);
                    SDL_UnlockMutex(is->pictq.mutex);
                    
                    av_log(NULL,AV_LOG_DEBUG,"vmdiff is %0.3f, repeat video:%0.3f,audio clk:%0.3f\n", ffp->stat.vmdiff, lastvp->pts, get_clock_with_delay(&is->audclk));
                    goto display;
                }
            }
            
            time= av_gettime_relative() / 1000000.0;
            if (isnan(is->frame_timer) || time < is->frame_timer)
                is->frame_timer = time;
            if (time < is->frame_timer + delay) {
                *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
                goto display;
            }

            is->frame_timer += delay;
            if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
                is->frame_timer = time;

            SDL_LockMutex(is->pictq.mutex);
            if (!isnan(vp->pts))
                update_video_pts(is, vp->pts, vp->pos, vp->serial);
            SDL_UnlockMutex(is->pictq.mutex);

            if (!is->step_on_seeking && frame_queue_nb_remaining(&is->pictq) > 1) {
                Frame *nextvp = frame_queue_peek_next(&is->pictq);
                
                int delta = 0;
                if (nextvp->pts > 0 && vp->pts > 0) {
                    if (nextvp->pts > vp->pts) {
                        delta = (int)(nextvp->pts / vp->pts);
                    } else {
                        delta = (int)(vp->pts / nextvp->pts);
                    }
                }
                
                if (delta > 1000) {
                    ffp_notify_msg2(ffp, FFP_MSG_WARNING, SALTATION_RETURN_VALUE);
                }
                
                duration = vp_duration(is, vp, nextvp);
                if(!is->step && (ffp->framedrop > 0 || (ffp->framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration) {
                    frame_queue_next(&is->pictq);
                    ff_sub_drop_old_frames(is->ffSub);
                    goto retry;
                }
            }
            
            frame_queue_next(&is->pictq);
            is->force_refresh = 1;

            SDL_LockMutex(ffp->is->play_mutex);
            if (is->step) {
                if (is->audio_st) {
                    //when use step play mode,we use video pts sync audio,so drop the behind audio.
                    //audio pts is behind,need fast forwad,otherwise cause video picture delay and not smoothly!
                    double diff = 0.01;
                    int counter = 3;
                    while (counter--) {
                        if (diff >= 0.01) {
                            diff = consume_audio_buffer(ffp, diff);
                        } else {
                            break;
                        }
                    }
                    SDL_AoutFlushAudio(ffp->aout);
                    update_sample_display(ffp, NULL, -1);
                }
                
                is->step = 0;
                if (!is->paused)
                    stream_update_pause_l(ffp);
            } else if (is->step_on_seeking) {
                is->step_on_seeking = 0;
            }
            SDL_UnlockMutex(ffp->is->play_mutex);
        }
display:
        /* display picture */
        if (!ffp->display_disable && is->force_refresh && is->show_mode == SHOW_MODE_VIDEO && is->pictq.rindex_shown)
            video_display2(ffp);
    }
    is->force_refresh = 0;
    
    if (ffp->show_status == 1 && AV_LOG_INFO > av_log_get_level()) {
        AVBPrint buf;
        static int64_t last_time;
        int64_t cur_time;
        int aqsize, vqsize, sqsize __unused;
        double av_diff;

        cur_time = av_gettime_relative();
        if (!last_time || (cur_time - last_time) >= 30000) {
            aqsize = 0;
            vqsize = 0;
            sqsize = 0;
            if (is->audio_st)
                aqsize = is->audioq.size;
            if (is->video_st)
                vqsize = is->videoq.size;
#ifdef FFP_MERGE
            if (is->subtitle_st)
                sqsize = is->subtitleq.size;
#else
            sqsize = 0;
#endif
            av_diff = 0;
            if (is->audio_st && is->video_st)
                av_diff = get_clock_with_delay(&is->audclk) - get_clock_with_delay(&is->vidclk);
            else if (is->video_st)
                av_diff = get_master_clock_with_delay(is) - get_clock_with_delay(&is->vidclk);
            else if (is->audio_st)
                av_diff = get_master_clock_with_delay(is) - get_clock_with_delay(&is->audclk);
            
            av_bprint_init(&buf, 0, AV_BPRINT_SIZE_AUTOMATIC);
                        av_bprintf(&buf,
                                  "%7.2f %s:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%"PRId64"/%"PRId64"   \r",
                                  get_master_clock_with_delay(is),
                                  (is->audio_st && is->video_st) ? "A-V" : (is->video_st ? "M-V" : (is->audio_st ? "M-A" : "   ")),
                                  av_diff,
                                  is->frame_drops_early + is->frame_drops_late,
                                  aqsize / 1024,
                                  vqsize / 1024,
                                  sqsize,
                                  is->video_st ? is->viddec.avctx->pts_correction_num_faulty_dts : 0,
                                  is->video_st ? is->viddec.avctx->pts_correction_num_faulty_pts : 0);
            av_log(NULL, AV_LOG_INFO, "%s", buf.str);
            av_bprint_finalize(&buf, NULL);
            
            last_time = cur_time;
        }
    }
}

/* allocate a picture (needs to do that in main thread to avoid
   potential locking problems */
static void alloc_picture(FFPlayer *ffp, int src_format)
{
    VideoState *is = ffp->is;
    Frame *vp;
#ifdef FFP_MERGE
    int sdl_format;
#endif

    vp = &is->pictq.queue[is->pictq.windex];

    free_picture(vp);

#ifdef FFP_MERGE
    video_open(is, vp);
#endif
    vp->bmp = SDL_Vout_CreateOverlay(vp->width,
                                     vp->height,
                                     src_format,
                                   ffp->vout);
#ifdef FFP_MERGE
    if (vp->format == AV_PIX_FMT_YUV420P)
        sdl_format = SDL_PIXELFORMAT_YV12;
    else
        sdl_format = SDL_PIXELFORMAT_ARGB8888;

    if (realloc_texture(&vp->bmp, sdl_format, vp->width, vp->height, SDL_BLENDMODE_NONE, 0) < 0)
#else
    /* RV16, RV32 contains only one plane */
    if (!vp->bmp || (!vp->bmp->is_private && vp->bmp->pitches[0] < vp->width))
#endif
    {
        /* SDL allocates a buffer smaller than requested if the video
         * overlay hardware is unable to support the requested size. */
        av_log(NULL, AV_LOG_FATAL,
               "Error: the video system does not support an image\n"
                        "size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
                        "to reduce the image size.\n", vp->width, vp->height );
        free_picture(vp);
    }

    SDL_LockMutex(is->pictq.mutex);
    vp->allocated = 1;
    SDL_CondSignal(is->pictq.cond);
    SDL_UnlockMutex(is->pictq.mutex);
}

static int queue_picture(FFPlayer *ffp, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
    VideoState *is = ffp->is;
    Frame *vp;
    int video_accurate_seek_fail = 0;
    int64_t now = 0;
    
    monkey_log(NULL, AV_LOG_INFO,"xql video queue_picture\n");
    
    /*
     去掉了 && !is->step_on_seeking，因为解码可能非常快，在 step_on_seeking 期间，就把视频frame queue 填满了，但实际上跳过了精准过滤，接着处于等待队列不满状态，可只有播放才会消耗队列；
     (seek 后，开启精准seek时，则 video_accurate_seek_req = 1;)
     此时音频精准seek在继续，判断 video_accurate_seek_req = 1，进而陷入无条件等待视频精准seek结束，直到达到设定的精准seek超时阈值。
     */
    if (ffp->enable_accurate_seek && is->video_accurate_seek_req && !is->seek_req) {
        if (!isnan(pts)) {
            int64_t video_seek_pos = is->seek_pos;
            double audio_delay = is->audio_st ? get_clock_extral_delay(&is->audclk) : 0;
            int64_t target_pos = is->seek_pos + audio_delay * 1000 * 1000;
            int64_t deviation = target_pos - (int64_t)(pts * 1000 * 1000);
            is->accurate_seek_vframe_pts = pts * 1000 * 1000;
            if (deviation > MAX_DEVIATION) {
                now = av_gettime_relative() / 1000;
                /*
                 fix fast rewind and fast forward continually,however accurate seek may not ended,cause accurate seek timeout.
                 */
                int force_reset_accurate_seek_time = 0;
                if (is->drop_vframe_serial != is->videoq.serial) {
                    force_reset_accurate_seek_time = 1;
                    is->drop_vframe_serial = is->videoq.serial;
                }
                if (is->drop_vframe_count == 0 || force_reset_accurate_seek_time) {
                    SDL_LockMutex(is->accurate_seek_mutex);
                    is->drop_vframe_count = 0;
                    if (force_reset_accurate_seek_time) {
                        is->accurate_seek_start_time = now;
                        av_log(NULL, AV_LOG_INFO,"video force reset accurate_seek time\n");
                    } else if (is->accurate_seek_start_time <= 0 && (is->audio_stream < 0 || is->audio_accurate_seek_req)) {
                        is->accurate_seek_start_time = now;
                    }
                    SDL_UnlockMutex(is->accurate_seek_mutex);
                    
                    int64_t delta = deviation - MAX_DEVIATION;
                    double fps = ffp->stat.vfps_probe;
                    int need_drop = ceil(delta * fps / AV_TIME_BASE);
                    
                    av_log(NULL, AV_LOG_INFO, "video accurate_seek start, is->seek_pos=%0.3f, first pts=%0.3f, estimate drop=%d is->accurate_seek_start_time=%lld\n", target_pos/1000000.0, pts, need_drop, is->accurate_seek_start_time);
                }
                is->drop_vframe_count++;

//                while (is->audio_accurate_seek_req && !is->abort_request) {
//                    int64_t apts = is->accurate_seek_aframe_pts;
//                    int64_t deviation2 = apts - pts * 1000 * 1000;
//                    int64_t deviation3 = apts - is->seek_pos;
//
//                    if (deviation2 > -100 * 1000 && deviation3 < 0) {
//                        break;
//                    } else {
//                        av_usleep(20 * 1000);
//                    }
//                    now = av_gettime_relative() / 1000;
//                    if ((now - is->accurate_seek_start_time) > ffp->accurate_seek_timeout) {
//                        break;
//                    }
//                }

                if ((now - is->accurate_seek_start_time) <= ffp->accurate_seek_timeout) {
                    return 1;  // drop some old frame when do accurate seek
                } else {
                    video_accurate_seek_fail = 2;  // if KEY_FRAME interval too big, disable accurate seek
                }
            } else {
                if (video_seek_pos == is->seek_pos) {
                    int dropped = is->drop_vframe_count;
                    is->drop_vframe_count = 0;
                    
                    SDL_LockMutex(is->accurate_seek_mutex);
                    is->video_accurate_seek_req = 0;
                    SDL_CondSignal(is->audio_accurate_seek_cond);
                    
                    if (video_seek_pos == is->seek_pos && is->audio_accurate_seek_req && !is->abort_request) {
                        av_log(NULL, AV_LOG_INFO, "video accurate_seek is ok, drop frame=%d, target diff=%0.3f, waiting audio\n", dropped, is->seek_pos/1000000.0 - pts);
                        SDL_CondWaitTimeout(is->video_accurate_seek_cond, is->accurate_seek_mutex, ffp->accurate_seek_timeout);
                    } else {
                        av_log(NULL, AV_LOG_INFO, "accurate_seek is ok, video drop frame=%d, target diff=%0.3f\n", dropped, is->seek_pos/1000000.0 - pts);
                        ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE, (int)(pts * 1000));
                    }
                    
                    if (video_seek_pos != is->seek_pos && !is->abort_request) {
                        av_log(NULL, AV_LOG_INFO, "new seek is trigger, continue drop video, is->seek_pos=%0.3f, pts=%0.3f\n", is->seek_pos/1000000.0, pts);
                        is->video_accurate_seek_req = 1;
                        SDL_UnlockMutex(is->accurate_seek_mutex);
                        return 1;
                    }
                    
                    SDL_UnlockMutex(is->accurate_seek_mutex);
                }
            }
        } else {
            video_accurate_seek_fail = 1;
        }

        if (video_accurate_seek_fail) {
            int dropped = is->drop_vframe_count;
            if (video_accurate_seek_fail == 2) {
                av_log(NULL, AV_LOG_WARNING, "video accurate_seek is timeout, drop frame=%d, pts=%lf\n", dropped, pts);
            } else {
                if (is->accurate_seek_start_time > 0) {
                    av_log(NULL, AV_LOG_INFO, "video accurate_seek is error, drop frame=%d, pts=%lf\n", dropped, pts);
                } else {
                    av_log(NULL, AV_LOG_INFO, "video accurate_seek is skipped,pts=%lf\n", pts);
                }
            }
            is->drop_vframe_count = 0;
            SDL_LockMutex(is->accurate_seek_mutex);
            is->video_accurate_seek_req = 0;
            SDL_CondSignal(is->audio_accurate_seek_cond);
            
            if (is->audio_accurate_seek_req && !is->abort_request) {
                SDL_CondWaitTimeout(is->video_accurate_seek_cond, is->accurate_seek_mutex, ffp->accurate_seek_timeout);
            } else {
                if (!isnan(pts)) {
                    ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE, (int)(pts * 1000));
                } else {
                    ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE, 0);
                }
            }
            SDL_UnlockMutex(is->accurate_seek_mutex);
        }
        is->accurate_seek_start_time = 0;
        video_accurate_seek_fail = 0;
        is->accurate_seek_vframe_pts = 0;
    }

#if defined(DEBUG_SYNC)
    printf("frame_type=%c pts=%0.3f\n",
           av_get_picture_type_char(src_frame->pict_type), pts);
#endif

    monkey_log("will put frame, dropped vframe_count=%d, pts=%lf\n", is->drop_vframe_count, pts);
    
    if (!(vp = frame_queue_peek_writable(&is->pictq)))
        return -1;

    vp->sar = src_frame->sample_aspect_ratio;
#ifdef FFP_MERGE
    vp->uploaded = 0;
#endif
    
    //TODO: windows and android plat.
    //软解时，上层指定了明确的overlay-format时需要转格式
    if (src_frame->format != AV_PIX_FMT_VIDEOTOOLBOX) {
        
        const int src_format = src_frame->format;
        Uint32 overlay_format = ffp->vout->overlay_format;
        if (SDL_FCC__GLES2 == overlay_format) {
        #if defined(__ANDROID__)
            overlay_format = SDL_FCC_YV12;
        #elif defined(__APPLE__)
        #if TARGET_OS_OSX
            if (src_format == AV_PIX_FMT_UYVY422) {
                overlay_format = SDL_FCC_UYVY;
            } else if (src_format == AV_PIX_FMT_YUYV422) {
                overlay_format = SDL_FCC_YUV2;
            } else
        #endif
            if (src_format == AV_PIX_FMT_YUV420P && src_frame->color_range == AVCOL_RANGE_JPEG) {
                overlay_format = SDL_FCC_J420;
            } else if (src_format == AV_PIX_FMT_YUV420P) {
                overlay_format = SDL_FCC_I420;
            } else if (src_format == AV_PIX_FMT_YUVJ420P) {
                overlay_format = SDL_FCC_J420;
            } else if (src_format == AV_PIX_FMT_YUV420P10) {
                overlay_format = SDL_FCC_P010;
            } else if (src_format == AV_PIX_FMT_YUV422P10) {
                overlay_format = SDL_FCC_P010;
            } else if (src_format == AV_PIX_FMT_YUV444P10) {
                overlay_format = SDL_FCC_P010;
            } else if (src_format == AV_PIX_FMT_YUV444P16 || src_format == AV_PIX_FMT_P416) {
                overlay_format = SDL_FCC_P416;
            } else if (src_format == AV_PIX_FMT_YUV422P16 || src_format == AV_PIX_FMT_P216) {
                overlay_format = SDL_FCC_P216;
            } else if (src_format == AV_PIX_FMT_YUVA444P16 || src_format == AV_PIX_FMT_AYUV64) {
                overlay_format = SDL_FCC_AYUV64;
            } else {
                const AVPixFmtDescriptor *pfd = av_pix_fmt_desc_get(src_format);
                if (pfd->nb_components > 0) {
                    if (pfd->comp[0].depth == 10) {
                        overlay_format = SDL_FCC_P010;
                    } else {
                        overlay_format = SDL_FCC_NV12;
                    }
                }
            }
        #endif
            //
            ffp->vout->overlay_format = overlay_format;
        }
        
        enum AVPixelFormat dst_format = AV_PIX_FMT_NONE;
        switch (overlay_format) {
            case SDL_FCC_J420:
            case SDL_FCC_I420:
            case SDL_FCC_YV12:
            {
                if (overlay_format == SDL_FCC_J420) {
                    dst_format = AV_PIX_FMT_YUVJ420P;
                } else {
                    dst_format = AV_PIX_FMT_YUV420P;
                }
                break;
            }
            case SDL_FCC_NV12: {
                dst_format = AV_PIX_FMT_NV12;
                break;
            }
            case SDL_FCC_BGRA: {
                dst_format = AV_PIX_FMT_BGRA;
                break;
            }
            case SDL_FCC_BGR0: {
                dst_format = AV_PIX_FMT_BGR0;
                break;
            }
            case SDL_FCC_ARGB: {
                dst_format = AV_PIX_FMT_ARGB;
                break;
            }
            case SDL_FCC_0RGB: {
                dst_format = AV_PIX_FMT_0RGB;
                break;
            }
            case SDL_FCC_UYVY: {
                dst_format = AV_PIX_FMT_UYVY422;
                break;
            }
            case SDL_FCC_YUV2: {
                dst_format = AV_PIX_FMT_YUYV422;
                break;
            }
            case SDL_FCC_P010: {
                dst_format = AV_PIX_FMT_P010;
            }
                break;
            case SDL_FCC_P416: {
                dst_format = AV_PIX_FMT_P416;
            }
                break;
            case SDL_FCC_P216: {
                dst_format = AV_PIX_FMT_P216;
            }
                break;
            case SDL_FCC_AYUV64: {
                dst_format = AV_PIX_FMT_AYUV64;
            }
                break;
            default:
                ALOGE("unknow overly format:%.4s(0x%x)\n", (char*)&overlay_format, overlay_format);
                return -1000;
                break;
        }
        
        if (src_format != dst_format) {
            const AVFrame *outFrame = NULL;
            if (SDL_VoutConvertFrame(ffp->vout, dst_format, src_frame, &outFrame)) {
                //convert failed.
                return -2;
            }
            src_frame = (AVFrame *)outFrame;
        }
    }
    
    /* alloc or resize hardware picture buffer */
    if (!vp->bmp || !vp->allocated ||
        vp->width  != src_frame->width ||
        vp->height != src_frame->height ||
        vp->format != src_frame->format) {

        if (vp->width != src_frame->width || vp->height != src_frame->height)
            ffp_notify_msg3(ffp, FFP_MSG_VIDEO_SIZE_CHANGED, src_frame->width, src_frame->height);

        vp->allocated = 0;
        vp->width = src_frame->width;
        vp->height = src_frame->height;
        vp->format = src_frame->format;

        /* the allocation must be done in the main thread to avoid
           locking problems. */
        if (src_frame->format == AV_PIX_FMT_YUV420P && src_frame->color_range == AVCOL_RANGE_JPEG) {
            alloc_picture(ffp, AV_PIX_FMT_YUVJ420P);
        } else {
            alloc_picture(ffp, src_frame->format);
        }

        if (is->videoq.abort_request)
            return -1;
    }

    /* if the frame is not skipped, then display it */
    if (vp->bmp) {
        /* get a pointer on the bitmap */
        SDL_VoutLockYUVOverlay(vp->bmp);

#ifdef FFP_MERGE
#if CONFIG_AVFILTER
        // FIXME use direct rendering
        av_image_copy(data, linesize, (const uint8_t **)src_frame->data, src_frame->linesize,
                        src_frame->format, vp->width, vp->height);
#else
        // sws_getCachedContext(...);
#endif
#endif
        // FIXME: set swscale options
        if (SDL_VoutFillFrameYUVOverlay(vp->bmp, src_frame) < 0) {
            av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
            return -3;
        }
        
        /* update the bitmap content */
        SDL_VoutUnlockYUVOverlay(vp->bmp);

        vp->pts = pts;
        vp->duration = duration;
        vp->pos = pos;
        vp->serial = serial;
        vp->sar = src_frame->sample_aspect_ratio;
        vp->bmp->sar_num = vp->sar.num;
        vp->bmp->sar_den = vp->sar.den;
        vp->bmp->fps = ffp->stat.vfps_probe;
        if (ffp->autorotate) {
            //fill video ratate degrees
            vp->bmp->auto_z_rotate_degrees = - ffp->vout->z_rotate_degrees;
        }
        
#ifdef FFP_MERGE
        av_frame_move_ref(vp->frame, src_frame);
#endif
        frame_queue_push(&is->pictq);
        /*
         paused player firstly,then seek stream,because frame queue is full,waiting readable slot;
         after seek file,flushed packet queue,step to display next frame,will buffet out frame queue,because the frame queue's serial is not equal videoq's serail.
         the frame which before seek will push to queue! cause send FFP_MSG_AFTER_SEEK_FIRST_FRAME ahead!
         */
        if (is->videoq.serial == serial) {
            if (!is->viddec.first_frame_decoded) {
                ALOGD("Video: first frame decoded\n");
                ffp_notify_msg1(ffp, FFP_MSG_VIDEO_DECODED_START);
                is->viddec.first_frame_decoded_time = SDL_GetTickHR();
                is->viddec.first_frame_decoded = 1;
            } else if (is->viddec.after_seek_frame) {
                int du = (int)(SDL_GetTickHR() - is->viddec.start_seek_time);
                is->viddec.after_seek_frame = 0;
                ffp_notify_msg2(ffp, FFP_MSG_AFTER_SEEK_FIRST_FRAME, du);
            }
        } else {
            ALOGD("push old video frame:%d\n",serial);
        }
    }
    return 0;
}

static void ffp_track_statistic_l(FFPlayer *ffp, AVStream *st, PacketQueue *q, FrameQueue *fq, FFTrackCacheStatistic *cache)
{
    assert(cache);

    if (q) {
        cache->bytes   = q->size;
        cache->packets = q->nb_packets;
    }

    if (q && st && st->time_base.den > 0 && st->time_base.num > 0) {
        cache->duration = q->duration * av_q2d(st->time_base) * 1000 + (fq ? fq->duration * 1000 : 0);
    }
}

static void ffp_audio_statistic_l(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    
    ffp_track_statistic_l(ffp, is->audio_st, &is->audioq, &is->sampq, &ffp->stat.audio_cache);

    if (ffp->is_manifest) {
          las_set_audio_cached_duration_ms(&ffp->las_player_statistic, ffp->stat.audio_cache.duration);
    }
}

static void ffp_video_statistic_l(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    ffp_track_statistic_l(ffp, is->video_st, &is->videoq, &is->pictq, &ffp->stat.video_cache);
    if (ffp->is_manifest) {
        las_set_video_cached_duration_ms(&ffp->las_player_statistic, ffp->stat.video_cache.duration);
    }
}

static void update_playable_duration(FFPlayer *ffp)
{
    if (!ffp) {
        return;
    }
    VideoState *is           = ffp->is;
    if (!is) {
        return;
    }
    int     cached_duration_in_ms = -1;
    int64_t audio_cached_duration = -1;
    int64_t video_cached_duration = -1;
    
    if (is->audio_st) {
        audio_cached_duration = ffp->stat.audio_cache.duration;
    }
    
    if (is->video_st) {
        video_cached_duration = ffp->stat.video_cache.duration;
    }
    
    if (video_cached_duration > 0 && audio_cached_duration > 0) {
        cached_duration_in_ms = (int)IJKMIN(video_cached_duration, audio_cached_duration);
    } else if (video_cached_duration > 0) {
        cached_duration_in_ms = (int)video_cached_duration;
    } else if (audio_cached_duration > 0) {
        cached_duration_in_ms = (int)audio_cached_duration;
    }
    
    if (cached_duration_in_ms >= 0) {
        int64_t buf_time_position = ffp_get_current_position_l(ffp) + cached_duration_in_ms;
        if (ffp->playable_duration_ms != buf_time_position) {
            ffp->playable_duration_ms = buf_time_position;
        }
    }
}

static void ffp_statistic_l(FFPlayer *ffp)
{
    ffp_audio_statistic_l(ffp);
    ffp_video_statistic_l(ffp);
    //when paused player,need update playable statistics after seek
    update_playable_duration(ffp);
}

static int get_video_frame(FFPlayer *ffp, AVFrame *frame)
{
    VideoState *is = ffp->is;
    int got_picture;

    ffp_video_statistic_l(ffp);
    if ((got_picture = decoder_decode_frame(ffp, &is->viddec, frame, NULL)) < 0)
        return got_picture;

    if (got_picture) {
        double dpts = NAN;

        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(is->video_st->time_base) * frame->pts;

        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

        if (!is->step_on_seeking && (ffp->framedrop > 0 || (ffp->framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER))) {
            ffp->stat.decode_frame_count++;
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - get_master_clock_with_delay(is);
                if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
                    diff - is->frame_last_filter_delay < 0 &&
                    is->viddec.pkt_serial == is->vidclk.serial &&
                    is->videoq.nb_packets) {
                    is->frame_drops_early++;
                    is->continuous_frame_drops_early++;
                    if (is->continuous_frame_drops_early > ffp->framedrop) {
                        is->continuous_frame_drops_early = 0;
                    } else {
                        ffp->stat.drop_frame_count++;
                        ffp->stat.drop_frame_rate = (float)(ffp->stat.drop_frame_count) / (float)(ffp->stat.decode_frame_count);
                        av_frame_unref(frame);
                        got_picture = 0;
                    }
                }
            }
        }
    }

    return got_picture;
}

static double get_rotation(int32_t *displaymatrix)
{
    double theta = 0;
    if (displaymatrix)
        theta = -round(av_display_rotation_get((int32_t*) displaymatrix));

    theta -= 360*floor(theta/360 + 0.9/360);

    if (fabs(theta - 90*round(theta/90)) > 2)
        av_log(NULL, AV_LOG_WARNING, "Odd rotation angle.\n"
               "If you want to help, upload a sample "
               "of this file to https://streams.videolan.org/upload/ "
               "and contact the ffmpeg-devel mailing list. (ffmpeg-devel@ffmpeg.org)");

    return theta;
}

#if CONFIG_AVFILTER
static int configure_filtergraph(AVFilterGraph *graph, const char *filtergraph,
                                 AVFilterContext *source_ctx, AVFilterContext *sink_ctx)
{
    int ret, i;
    int nb_filters = graph->nb_filters;
    AVFilterInOut *outputs = NULL, *inputs = NULL;

    if (filtergraph) {
        av_log(NULL, AV_LOG_INFO, "Video filtergraph:%s", filtergraph);
        outputs = avfilter_inout_alloc();
        inputs  = avfilter_inout_alloc();
        if (!outputs || !inputs) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        outputs->name       = av_strdup("in");
        outputs->filter_ctx = source_ctx;
        outputs->pad_idx    = 0;
        outputs->next       = NULL;

        inputs->name        = av_strdup("out");
        inputs->filter_ctx  = sink_ctx;
        inputs->pad_idx     = 0;
        inputs->next        = NULL;

        if ((ret = avfilter_graph_parse_ptr(graph, filtergraph, &inputs, &outputs, NULL)) < 0)
            goto fail;
    } else {
        if ((ret = avfilter_link(source_ctx, 0, sink_ctx, 0)) < 0)
            goto fail;
    }

    /* Reorder the filters to ensure that inputs of the custom filters are merged first */
    for (i = 0; i < graph->nb_filters - nb_filters; i++)
        FFSWAP(AVFilterContext*, graph->filters[i], graph->filters[i + nb_filters]);

    ret = avfilter_graph_config(graph, NULL);
fail:
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);
    return ret;
}

static int configure_video_filters(FFPlayer *ffp, AVFilterGraph *graph, VideoState *is, const char *vfilters, AVFrame *frame)
{
    static const enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_BGRA, AV_PIX_FMT_NONE };
    char sws_flags_str[512] = "";
    char buffersrc_args[256];
    int ret;
    AVFilterContext *filt_src = NULL, *filt_out = NULL, *last_filter = NULL;
    AVCodecParameters *codecpar = is->video_st->codecpar;
    AVRational fr = av_guess_frame_rate(is->ic, is->video_st, NULL);
    AVDictionaryEntry *e = NULL;

    while ((e = av_dict_get(ffp->sws_dict, "", e, AV_DICT_IGNORE_SUFFIX))) {
        if (!strcmp(e->key, "sws_flags")) {
            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", "flags", e->value);
        } else
            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", e->key, e->value);
    }
    if (strlen(sws_flags_str))
        sws_flags_str[strlen(sws_flags_str)-1] = '\0';

    graph->scale_sws_opts = av_strdup(sws_flags_str);

    snprintf(buffersrc_args, sizeof(buffersrc_args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             frame->width, frame->height, frame->format,
             is->video_st->time_base.num, is->video_st->time_base.den,
             codecpar->sample_aspect_ratio.num, FFMAX(codecpar->sample_aspect_ratio.den, 1));
    if (fr.num && fr.den)
        av_strlcatf(buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d", fr.num, fr.den);

    if ((ret = avfilter_graph_create_filter(&filt_src,
                                            avfilter_get_by_name("buffer"),
                                            "ffplay_buffer", buffersrc_args, NULL,
                                            graph)) < 0)
        goto fail;

    ret = avfilter_graph_create_filter(&filt_out,
                                       avfilter_get_by_name("buffersink"),
                                       "ffplay_buffersink", NULL, NULL, graph);
    if (ret < 0)
        goto fail;

    if ((ret = av_opt_set_int_list(filt_out, "pix_fmts", pix_fmts,  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
        goto fail;

    last_filter = filt_out;

/* Note: this macro adds a filter before the lastly added filter, so the
 * processing order of the filters is in reverse */
#define INSERT_FILT(name, arg) do {                                          \
    AVFilterContext *filt_ctx;                                               \
                                                                             \
    ret = avfilter_graph_create_filter(&filt_ctx,                            \
                                       avfilter_get_by_name(name),           \
                                       "ffplay_" name, arg, NULL, graph);    \
    if (ret < 0)                                                             \
        goto fail;                                                           \
                                                                             \
    ret = avfilter_link(filt_ctx, 0, last_filter, 0);                        \
    if (ret < 0)                                                             \
        goto fail;                                                           \
                                                                             \
    last_filter = filt_ctx;                                                  \
} while (0)

    if (ffp->autorotate) {
        int32_t *displaymatrix = (int32_t *)av_stream_get_side_data(is->video_st, AV_PKT_DATA_DISPLAYMATRIX, NULL);
        double theta  = get_rotation(displaymatrix);

        if (fabs(theta - 90) < 1.0) {
            INSERT_FILT("transpose", "clock");
        } else if (fabs(theta - 180) < 1.0) {
            INSERT_FILT("hflip", NULL);
            INSERT_FILT("vflip", NULL);
        } else if (fabs(theta - 270) < 1.0) {
            INSERT_FILT("transpose", "cclock");
        } else if (fabs(theta) > 1.0) {
            char rotate_buf[64];
            snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
            INSERT_FILT("rotate", rotate_buf);
        }
    }

#ifdef FFP_AVFILTER_PLAYBACK_RATE
    if (fabsf(ffp->pf_playback_rate) > 0.00001 &&
        fabsf(ffp->pf_playback_rate - 1.0f) > 0.00001) {
        char setpts_buf[256];
        float rate = 1.0f / ffp->pf_playback_rate;
        rate = av_clipf_c(rate, 0.5f, 2.0f);
        av_log(ffp, AV_LOG_INFO, "vf_rate=%f(1/%f)\n", ffp->pf_playback_rate, rate);
        snprintf(setpts_buf, sizeof(setpts_buf), "%f*PTS", rate);
        INSERT_FILT("setpts", setpts_buf);
    }
#endif

    if ((ret = configure_filtergraph(graph, vfilters, filt_src, last_filter)) < 0)
        goto fail;

    is->in_video_filter  = filt_src;
    is->out_video_filter = filt_out;

fail:
    return ret;
}

static int configure_audio_filters(FFPlayer *ffp, const char *afilters, int force_output_format)
{
    VideoState *is = ffp->is;
    static const enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
    int sample_rates[2] = { 0, -1 };
    AVFilterContext *filt_asrc = NULL, *filt_asink = NULL;
    char aresample_swr_opts[512] = "";
    AVDictionaryEntry *e = NULL;
    AVBPrint bp;
    char asrc_args[256];
    int ret;
    char afilters_args[4096];

    avfilter_graph_free(&is->agraph);
    if (!(is->agraph = avfilter_graph_alloc()))
        return AVERROR(ENOMEM);
    //just use single thread per graph
    int filter_nbthreads = 1;
    is->agraph->nb_threads = filter_nbthreads;

    av_bprint_init(&bp, 0, AV_BPRINT_SIZE_AUTOMATIC);
    
    while ((e = av_dict_get(ffp->swr_opts, "", e, AV_DICT_IGNORE_SUFFIX)))
        av_strlcatf(aresample_swr_opts, sizeof(aresample_swr_opts), "%s=%s:", e->key, e->value);
    
    if (strlen(aresample_swr_opts))
        aresample_swr_opts[strlen(aresample_swr_opts)-1] = '\0';
    av_opt_set(is->agraph, "aresample_swr_opts", aresample_swr_opts, 0);

    av_channel_layout_describe_bprint(&is->audio_filter_src.ch_layout, &bp);

    ret = snprintf(asrc_args, sizeof(asrc_args),
                   "sample_rate=%d:sample_fmt=%s:time_base=%d/%d:channel_layout=%s",
                   is->audio_filter_src.freq, av_get_sample_fmt_name(is->audio_filter_src.fmt),
                   1, is->audio_filter_src.freq, bp.str);

    ret = avfilter_graph_create_filter(&filt_asrc,
                                       avfilter_get_by_name("abuffer"), "ffplay_abuffer",
                                       asrc_args, NULL, is->agraph);
    if (ret < 0)
        goto end;


    ret = avfilter_graph_create_filter(&filt_asink,
                                       avfilter_get_by_name("abuffersink"), "ffplay_abuffersink",
                                       NULL, NULL, is->agraph);
    if (ret < 0)
        goto end;

    if ((ret = av_opt_set_int_list(filt_asink, "sample_fmts", sample_fmts,  AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
        goto end;
    if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 1, AV_OPT_SEARCH_CHILDREN)) < 0)
        goto end;

    if (force_output_format) {
        sample_rates   [0] = is->audio_tgt.freq;
        if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
        if ((ret = av_opt_set(filt_asink, "ch_layouts", bp.str, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
        if ((ret = av_opt_set_int_list(filt_asink, "sample_rates"   , sample_rates   ,  -1, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
    }

    afilters_args[0] = 0;
    if (afilters)
        snprintf(afilters_args, sizeof(afilters_args), "%s", afilters);
#ifdef FFP_AVFILTER_PLAYBACK_RATE
    if (fabsf(ffp->pf_playback_rate) > 0.00001 &&
        fabsf(ffp->pf_playback_rate - 1.0f) > 0.00001) {
        if (afilters_args[0])
            av_strlcatf(afilters_args, sizeof(afilters_args), ",");

        av_log(ffp, AV_LOG_INFO, "af_rate=%f\n", ffp->pf_playback_rate);
        av_strlcatf(afilters_args, sizeof(afilters_args), "atempo=%f", ffp->pf_playback_rate);
    }
#endif

    if ((ret = configure_filtergraph(is->agraph, afilters_args[0] ? afilters_args : NULL, filt_asrc, filt_asink)) < 0)
        goto end;

    is->in_audio_filter  = filt_asrc;
    is->out_audio_filter = filt_asink;

end:
    if (ret < 0)
        avfilter_graph_free(&is->agraph);
    av_bprint_finalize(&bp, NULL);
    return ret;
}
#endif  /* CONFIG_AVFILTER */

static int audio_thread(void *arg)
{
    FFPlayer *ffp = arg;
    VideoState *is = ffp->is;
    AVFrame *frame = av_frame_alloc();
    Frame *af;
#if CONFIG_AVFILTER
    int last_serial = -1;
    int reconfigure;
#endif
    int got_frame = 0;
    AVRational tb;
    int ret = 0;
    int audio_accurate_seek_fail = 0;
    double frame_pts = 0;
    double audio_clock = 0;
    int64_t now = 0;
    double samples_duration = 0;
    
    if (!frame)
        return AVERROR(ENOMEM);

    do {
        ffp_audio_statistic_l(ffp);
        if ((got_frame = decoder_decode_frame(ffp, &is->auddec, frame, NULL)) < 0)
            goto the_end;
        if (got_frame) {
                tb = (AVRational){1, frame->sample_rate};
                monkey_log("decoder audio frame: %0.2f\n", frame->pts * av_q2d(tb));
                if (ffp->enable_accurate_seek && is->audio_accurate_seek_req && !is->seek_req) {
                    frame_pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                    now = av_gettime_relative() / 1000;
                    if (!isnan(frame_pts)) {
                        int64_t audio_seek_pos = is->seek_pos;
                        samples_duration = (double) frame->nb_samples / frame->sample_rate;
                        audio_clock = frame_pts + samples_duration;
                        is->accurate_seek_aframe_pts = audio_clock * 1000 * 1000;
                        int64_t deviation = is->seek_pos - (int64_t)(audio_clock * 1000 * 1000);
                        double audio_delay = is->audio_st ? get_clock_extral_delay(&is->audclk) : 0;
                        if (deviation > MAX_DEVIATION) {
                            /*
                             fix fast rewind and fast forward continually,however accurate seek may not ended,cause accurate seek timeout.
                             */
                            int force_reset_accurate_seek_time = 0;
                            if (is->drop_aframe_serial != is->audioq.serial) {
                                force_reset_accurate_seek_time = 1;
                                is->drop_aframe_serial = is->audioq.serial;
                            }
                            if (is->drop_aframe_count == 0 || force_reset_accurate_seek_time) {
                                SDL_LockMutex(is->accurate_seek_mutex);
                                is->drop_aframe_count = 0;
                                if (force_reset_accurate_seek_time) {
                                    is->accurate_seek_start_time = now;
                                    av_log(NULL, AV_LOG_INFO,"audio force reset accurate_seek time\n");
                                } else if (is->accurate_seek_start_time <= 0 && (is->video_stream < 0 || is->video_accurate_seek_req)) {
                                    is->accurate_seek_start_time = now;
                                }
                                SDL_UnlockMutex(is->accurate_seek_mutex);
                                
                                int64_t delta = is->seek_pos - frame_pts * 1000 * 1000 - MAX_DEVIATION;
                                double fps = 1.0 / samples_duration;
                                int need_drop = ceil(delta * fps / 1000 / 1000);
                                
                                av_log(NULL, AV_LOG_INFO, "audio accurate_seek start, target_pos=%0.3f, audio_clock=%0.3f, delay:%0.3f, estimate drop=%d, is->accurate_seek_start_time=%lld\n", is->seek_pos/1000000.0, audio_clock, audio_delay, need_drop, is->accurate_seek_start_time);
                            }
                            is->drop_aframe_count++;
/*
 decode audio stream is faster, audio accurate seek finished in a flash.
 some video stream decode is slow, accurate seek maybe timeout, thus video picture display in a flash, because video clock is later, so dropping frames need keep a safe distance.
 */
                            while (audio_delay == 0 && is->video_accurate_seek_req && !is->abort_request) {
                                int64_t vpts = is->accurate_seek_vframe_pts;
                                int64_t deviation2 = vpts - audio_clock * 1000 * 1000;
                                int64_t deviation3 = vpts - is->seek_pos;
                                //video is behind audio grather than 1s;
                                if (deviation2 > -1000 * 1000 && deviation3 < 0) {
                                    break;
                                } else {
                                    av_log(NULL, AV_LOG_INFO, "audio accurate_seek waiting video\n");
                                    av_usleep(20 * 1000);
                                }
                                now = av_gettime_relative() / 1000;
                                if ((now - is->accurate_seek_start_time) > ffp->accurate_seek_timeout) {
                                    break;
                                }
                            }

                            now = av_gettime_relative() / 1000;
                            if ((now - is->accurate_seek_start_time) <= ffp->accurate_seek_timeout) {
                                av_frame_unref(frame);
                                continue;  //continue drop more old frame
                            } else {
                                audio_accurate_seek_fail = 2;
                            }
                        } else {
                            int dropped = is->drop_aframe_count;
                            is->drop_aframe_count = 0;
                            
                            SDL_LockMutex(is->accurate_seek_mutex);
                            is->audio_accurate_seek_req = 0;
                            SDL_CondSignal(is->video_accurate_seek_cond);
                            
                            if (audio_seek_pos == is->seek_pos && is->video_accurate_seek_req && !is->abort_request) {
                                if (audio_seek_pos == is->seek_pos) {
                                    av_log(NULL, AV_LOG_INFO, "audio accurate_seek is ok, drop frame=%d, target diff=%0.3f, waitting video\n", dropped, is->seek_pos/1000000.0 - audio_clock);
                                }
                                SDL_CondWaitTimeout(is->audio_accurate_seek_cond, is->accurate_seek_mutex, ffp->accurate_seek_timeout);
                            } else {
                                av_log(NULL, AV_LOG_INFO, "accurate_seek is ok, audio drop frame=%d, target diff=%0.3f\n", dropped, is->seek_pos/1000000.0 - audio_clock);
                                ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE, (int)(audio_clock * 1000));
                            }
                            
                            if (audio_seek_pos != is->seek_pos && !is->abort_request) {
                                av_log(NULL, AV_LOG_INFO, "new seek is trigger, continue drop audio, is->seek_pos=%0.3f, audio_clock=%0.3f\n", is->seek_pos/1000000.0, audio_clock);
                                is->audio_accurate_seek_req = 1;
                                SDL_UnlockMutex(is->accurate_seek_mutex);
                                av_frame_unref(frame);
                                continue;
                            }
                            SDL_UnlockMutex(is->accurate_seek_mutex);
                        }
                    } else {
                        audio_accurate_seek_fail = 1;
                    }
                    if (audio_accurate_seek_fail) {
                        int dropped = is->drop_aframe_count;
                        is->drop_aframe_count = 0;
                        if (audio_accurate_seek_fail == 2) {
                            av_log(NULL, AV_LOG_WARNING, "audio accurate_seek is timeout, drop frame=%d, audio_clock=%lf\n", dropped, audio_clock);
                        } else {
                            if (is->accurate_seek_start_time > 0) {
                                av_log(NULL, AV_LOG_INFO, "audio accurate_seek is error, drop frame=%d, audio_clock=%lf\n", dropped, audio_clock);
                            } else {
                                av_log(NULL, AV_LOG_INFO, "audio accurate_seek is skipped, audio_clock=%lf\n", audio_clock);
                            }
                        }
                        SDL_LockMutex(is->accurate_seek_mutex);
                        is->audio_accurate_seek_req = 0;
                        SDL_CondSignal(is->video_accurate_seek_cond);
                        if (is->video_accurate_seek_req && !is->abort_request) {
                            SDL_CondWaitTimeout(is->audio_accurate_seek_cond, is->accurate_seek_mutex, ffp->accurate_seek_timeout);
                        } else {
                            ffp_notify_msg2(ffp, FFP_MSG_ACCURATE_SEEK_COMPLETE, (int)(audio_clock * 1000));
                        }
                        SDL_UnlockMutex(is->accurate_seek_mutex);
                    }
                    is->accurate_seek_start_time = 0;
                    audio_accurate_seek_fail = 0;
                }

#if CONFIG_AVFILTER
            reconfigure =
                cmp_audio_fmts(is->audio_filter_src.fmt, is->audio_filter_src.ch_layout.nb_channels,
                               frame->format, frame->ch_layout.nb_channels)    ||
                av_channel_layout_compare(&is->audio_filter_src.ch_layout, &frame->ch_layout) ||
                is->audio_filter_src.freq           != frame->sample_rate ||
                is->auddec.pkt_serial               != last_serial;

                if (reconfigure) {
                    SDL_LockMutex(ffp->af_mutex);
                    ffp->af_changed = 0;
                    char buf1[1024], buf2[1024];
                    av_channel_layout_describe(&is->audio_filter_src.ch_layout, buf1, sizeof(buf1));
                    av_channel_layout_describe(&frame->ch_layout, buf2, sizeof(buf2));
                    av_log(NULL, AV_LOG_DEBUG,
                           "Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
                           is->audio_filter_src.freq, is->audio_filter_src.ch_layout.nb_channels, av_get_sample_fmt_name(is->audio_filter_src.fmt), buf1, last_serial,
                           frame->sample_rate, frame->ch_layout.nb_channels, av_get_sample_fmt_name(frame->format), buf2, is->auddec.pkt_serial);
                    
                    is->audio_filter_src.fmt            = frame->format;
                    ret = av_channel_layout_copy(&is->audio_filter_src.ch_layout, &frame->ch_layout);
                    if (ret < 0)
                        goto the_end;
                    is->audio_filter_src.freq           = frame->sample_rate;
                    last_serial                         = is->auddec.pkt_serial;

                    if ((ret = configure_audio_filters(ffp, ffp->afilters, 1)) < 0) {
                        SDL_UnlockMutex(ffp->af_mutex);
                        goto the_end;
                    }
                    SDL_UnlockMutex(ffp->af_mutex);
                }

            if ((ret = av_buffersrc_add_frame(is->in_audio_filter, frame)) < 0)
                goto the_end;

            while ((ret = av_buffersink_get_frame_flags(is->out_audio_filter, frame, 0)) >= 0) {
                tb = av_buffersink_get_time_base(is->out_audio_filter);
#endif
                if (!(af = frame_queue_peek_writable(&is->sampq)))
                    goto the_end;

                af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                af->pos = frame->pkt_pos;
                af->serial = is->auddec.pkt_serial;
                af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});

                av_frame_move_ref(af->frame, frame);
                frame_queue_push(&is->sampq);
                if (is->audioq.serial != af->serial) {
                    ALOGD("push old audio frame:%d\n",af->serial);
                }
#if CONFIG_AVFILTER
                if (is->audioq.serial != is->auddec.pkt_serial)
                    break;
            }
            if (ret == AVERROR_EOF)
                is->auddec.finished = is->auddec.pkt_serial;
#endif
        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
 the_end:
#if CONFIG_AVFILTER
    avfilter_graph_free(&is->agraph);
#endif
    av_frame_free(&frame);
    return ret;
}

static int ffplay_video_thread(void *arg)
{
    FFPlayer *ffp = arg;
    VideoState *is = ffp->is;
    AVFrame *frame = av_frame_alloc();
    double pts;
    double duration;
    int ret;
    AVRational tb = is->video_st->time_base;
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);
    int convert_frame_count = 0;

#if CONFIG_AVFILTER
    AVFilterGraph *graph = NULL;
    AVFilterContext *filt_out = NULL, *filt_in = NULL;
    int last_w = 0;
    int last_h = 0;
    enum AVPixelFormat last_format = -2;
    int last_serial = -1;
    int last_vfilter_idx = 0;
#else
    ffp_notify_msg2(ffp, FFP_MSG_VIDEO_ROTATION_CHANGED, ffp_get_video_rotate_degrees(ffp));
#endif

    if (!frame) {
        return AVERROR(ENOMEM);
    }

    for (;;) {
        ret = get_video_frame(ffp, frame);
        if (ret < 0)
            goto the_end;
        if (!ret)
            continue;

#if CONFIG_AVFILTER
        if (   last_w != frame->width
            || last_h != frame->height
            || last_format != frame->format
            || last_serial != is->viddec.pkt_serial
            || ffp->vf_changed
            || last_vfilter_idx != is->vfilter_idx) {
            SDL_LockMutex(ffp->vf_mutex);
            ffp->vf_changed = 0;
            av_log(NULL, AV_LOG_INFO,
                   "Video frame changed from size:%dx%d format:%s serial:%d to size:%dx%d format:%s serial:%d\n",
                   last_w, last_h,
                   (const char *)av_x_if_null(av_get_pix_fmt_name(last_format), "none"), last_serial,
                   frame->width, frame->height,
                   (const char *)av_x_if_null(av_get_pix_fmt_name(frame->format), "none"), is->viddec.pkt_serial);
            avfilter_graph_free(&graph);
            graph = avfilter_graph_alloc();
            if (!graph) {
                ret = AVERROR(ENOMEM);
                goto the_end;
            }
            if ((ret = configure_video_filters(ffp, graph, is, ffp->vfilters_list ? ffp->vfilters_list[is->vfilter_idx] : NULL, frame)) < 0) {
                // FIXME: post error
                SDL_UnlockMutex(ffp->vf_mutex);
                goto the_end;
            }
            filt_in  = is->in_video_filter;
            filt_out = is->out_video_filter;
            last_w = frame->width;
            last_h = frame->height;
            last_format = frame->format;
            last_serial = is->viddec.pkt_serial;
            last_vfilter_idx = is->vfilter_idx;
            frame_rate = av_buffersink_get_frame_rate(filt_out);
            SDL_UnlockMutex(ffp->vf_mutex);
        }

        ret = av_buffersrc_add_frame(filt_in, frame);
        if (ret < 0)
            goto the_end;

        while (ret >= 0) {
            is->frame_last_returned_time = av_gettime_relative() / 1000000.0;

            ret = av_buffersink_get_frame_flags(filt_out, frame, 0);
            if (ret < 0) {
                if (ret == AVERROR_EOF)
                    is->viddec.finished = is->viddec.pkt_serial;
                ret = 0;
                break;
            }

            is->frame_last_filter_delay = av_gettime_relative() / 1000000.0 - is->frame_last_returned_time;
            if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
                is->frame_last_filter_delay = 0;
            tb = av_buffersink_get_time_base(filt_out);
#endif
            duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);
            pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            ret = queue_picture(ffp, frame, pts, duration, frame->pkt_pos, is->viddec.pkt_serial);
            av_frame_unref(frame);
#if CONFIG_AVFILTER
            if (is->videoq.serial != is->viddec.pkt_serial)
                break;
        }
#endif

        if (ret < 0)
            goto the_end;
    }
 the_end:
#if CONFIG_AVFILTER
    avfilter_graph_free(&graph);
#endif
    av_log(NULL, AV_LOG_INFO, "convert image convert_frame_count = %d,err = %d\n", convert_frame_count,ret);
    av_frame_free(&frame);
    return ret;
}

static int video_thread(void *arg)
{
    FFPlayer *ffp = (FFPlayer *)arg;
    int       ret = 0;

    if (ffp->node_vdec) {
        ret = ffpipenode_run_sync(ffp->node_vdec);
    }
    return ret;
}

/* copy samples for viewing in editor window */
static void update_sample_display(FFPlayer *ffp, uint8_t *samples, int samples_size)
{
    VideoState *is = ffp->is;
    //flush
    if (samples_size == -1) {
        is->sample_array_index = 0;
        if (ffp->audio_samples_callback) {
            ffp->audio_samples_callback(ffp->inject_opaque,
                                        NULL,
                                        -1,
                                        is->audio_src.freq,
                                        is->audio_src.ch_layout.nb_channels
                                        );
        }
        return;
    }
    
    //以下计算以 2 bytes（int16_t) 为单位
    int size, len;
    size = samples_size / sizeof(int16_t);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - is->sample_array_index;
        if (len > size)
            len = size;
        memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(int16_t));
        samples += len * sizeof(int16_t);
        is->sample_array_index += len;
        if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
            is->sample_array_index = 0;
        size -= len;
    }

    //以下计算以 byte（int8_t) 为单位
    if (ffp->audio_samples_callback) {
        int windowSize = 2048;
        int i = 0;
        for (; i < (is->sample_array_index * 2) / windowSize; i++) {
            ffp->audio_samples_callback(
                                        ffp->inject_opaque,
                                        (int16_t*)((int8_t*)is->sample_array + i * windowSize),
                                        windowSize,
                                        is->audio_tgt.freq,
                                        is->audio_src.ch_layout.nb_channels
                                        );
        }
        if (i > 0) {
            memcpy(is->sample_array, (int8_t*)is->sample_array + windowSize * i, is->sample_array_index * 2 - windowSize * i);
            is->sample_array_index -= (windowSize * i) / 2;
        }
    }
}

/* return the wanted number of samples to get better sync if sync_type is video
 * or external master clock */
static int synchronize_audio(VideoState *is, int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_clock_with_delay(&is->audclk) - get_master_clock_with_delay(is);

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
                av_log(NULL, AV_LOG_TRACE, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
                        diff, avg_diff, wanted_nb_samples - nb_samples,
                        is->audio_clock, is->audio_diff_threshold);
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum       = 0;
        }
    }

    return wanted_nb_samples;
}

/**
 * Decode one audio frame and return its uncompressed size.
 *
 * The processed audio frame is decoded, converted if required, and
 * stored in is->audio_buf, with size in bytes given by the return
 * value.
 */
static int audio_decode_frame(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    int data_size, resampled_data_size;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af;
#if defined(__ANDROID__)
    int translate_time = 1;
#endif

    if (is->paused)
        return -1;

    if (ffp->sync_av_start &&                       /* sync enabled */
        is->video_st &&                             /* has video stream */
        !is->viddec.first_frame_decoded &&          /* not hot */
        is->viddec.finished != is->videoq.serial) { /* not finished */
        /* waiting for first video frame */
        Uint64 now = SDL_GetTickHR();
        if (now < is->viddec.first_frame_decoded_time ||
            now > is->viddec.first_frame_decoded_time + 2000) {
            is->viddec.first_frame_decoded = 1;
        } else {
            /* video pipeline is not ready yet */
            return -1;
        }
    }
reload:
    do {
#if defined(_WIN32) || defined(__APPLE__)
        while (frame_queue_nb_remaining(&is->sampq) == 0) {
            if ((av_gettime_relative() - ffp->audio_callback_time) > 1000000LL * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2)
                return -1;
            av_usleep (1000);
        }
#endif
        if (!(af = frame_queue_peek_readable(&is->sampq)))
            return -1;
        frame_queue_next(&is->sampq);
        //skip old audio frames.
    } while (af->serial != is->audioq.serial);
    
    if (frame_queue_nb_remaining(&is->sampq) > 1) {
        Frame *next_af = frame_queue_peek_next(&is->sampq);
        int delta = 0;
        if (next_af->pts > 0 && af->pts > 0) {
            if (next_af->pts > af->pts) {
                delta = (int)(next_af->pts / af->pts);
            } else {
                delta = (int)(af->pts / next_af->pts);
            }
        }
        if (delta > 1000) {
            ffp_notify_msg2(ffp, FFP_MSG_WARNING, SALTATION_RETURN_VALUE);
        }
    }
    
    data_size = av_samples_get_buffer_size(NULL, af->frame->ch_layout.nb_channels,
                                           af->frame->nb_samples,
                                           af->frame->format, 1);
    
    wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);

    if (af->frame->format        != is->audio_src.fmt            ||
        av_channel_layout_compare(&af->frame->ch_layout, &is->audio_src.ch_layout) ||
        af->frame->sample_rate   != is->audio_src.freq           ||
        (wanted_nb_samples       != af->frame->nb_samples && !is->swr_ctx)) {
        AVDictionary *swr_opts = NULL;
        swr_free(&is->swr_ctx);
        swr_alloc_set_opts2(&is->swr_ctx,
                            &is->audio_tgt.ch_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
                            &af->frame->ch_layout, af->frame->format, af->frame->sample_rate,
                            0, NULL);
        if (!is->swr_ctx) {
            av_log(NULL, AV_LOG_ERROR,
                   "swr_alloc_set_opts2 failed!\n");
            return -1;
        }
        av_dict_copy(&swr_opts, ffp->swr_opts, 0);
        if (af->frame->channel_layout == AV_CH_LAYOUT_5POINT1_BACK)
            av_opt_set_double(is->swr_ctx, "center_mix_level", ffp->preset_5_1_center_mix_level, 0);
        av_opt_set_dict(is->swr_ctx, &swr_opts);
        av_dict_free(&swr_opts);

        if (swr_init(is->swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                    af->frame->sample_rate, av_get_sample_fmt_name(af->frame->format), af->frame->ch_layout.nb_channels,
                    is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.ch_layout.nb_channels);
            swr_free(&is->swr_ctx);
            return -1;
        }
        
        if (av_channel_layout_copy(&is->audio_src.ch_layout, &af->frame->ch_layout) < 0)
            return -1;
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = af->frame->format;
    }

    if (is->swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &is->audio_buf1;
        int out_count = (int)((int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256);
        int out_size  = av_samples_get_buffer_size(NULL, is->audio_tgt.ch_layout.nb_channels, out_count, is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
                                        wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
                av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);

        if (!is->audio_buf1)
            return AVERROR(ENOMEM);
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
            if (swr_init(is->swr_ctx) < 0)
                swr_free(&is->swr_ctx);
        }
        is->audio_buf = is->audio_buf1;
        int bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt.fmt);
        resampled_data_size = len2 * is->audio_tgt.ch_layout.nb_channels * bytes_per_sample;
#if defined(__ANDROID__)
        if (ffp->soundtouch_enable && ffp->pf_playback_rate != 1.0f && !is->abort_request) {
            av_fast_malloc(&is->audio_new_buf, &is->audio_new_buf_size, out_size * translate_time);
            for (int i = 0; i < (resampled_data_size / 2); i++)
            {
                is->audio_new_buf[i] = (is->audio_buf1[i * 2] | (is->audio_buf1[i * 2 + 1] << 8));
            }

            int ret_len = ijk_soundtouch_translate(is->handle, is->audio_new_buf, (float)(ffp->pf_playback_rate), (float)(1.0f/ffp->pf_playback_rate),
                    resampled_data_size / 2, bytes_per_sample, is->audio_tgt.channels, af->frame->sample_rate);
            if (ret_len > 0) {
                is->audio_buf = (uint8_t*)is->audio_new_buf;
                resampled_data_size = ret_len;
            } else {
                translate_time++;
                goto reload;
            }
        }
#endif
    } else {
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }
    
    audio_clock0 = is->audio_clock;
    /* update the audio clock with the pts */
    if (!isnan(af->pts))
        is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
    else
        is->audio_clock = NAN;
    is->audio_clock_serial = af->serial;
#ifdef FFP_SHOW_AUDIO_DELAY
    {
        static double last_clock;
        printf("audio: delay=%0.3f clock=%0.3f clock0=%0.3f\n",
               is->audio_clock - last_clock,
               is->audio_clock, audio_clock0);
        last_clock = is->audio_clock;
    }
#endif
    if (!is->auddec.first_frame_decoded) {
        ALOGD("avcodec/Audio: first frame decoded\n");
        ffp_notify_msg1(ffp, FFP_MSG_AUDIO_DECODED_START);
        is->auddec.first_frame_decoded_time = SDL_GetTickHR();
        is->auddec.first_frame_decoded = 1;
    }
    return resampled_data_size;
}

/* consume audio buffer */
static double consume_audio_buffer(FFPlayer *ffp, double diff)
{
    VideoState *is = ffp->is;
    
    if (!ffp || !is) {
        return 0.0;
    }
    
    av_log(NULL, AV_LOG_DEBUG, "consume audio buffer:%0.3f\n", diff);
    
    const int sample = is->audio_tgt.bytes_per_sec / is->audio_tgt.freq;
    
    if (isnan(sample) || sample == 0) {
        return 0.0;
    }
    
    int len = diff * is->audio_tgt.bytes_per_sec;
    len = len / sample * sample;
    
    int audio_size, rest_len = 0;
    const int len_want = len;
    
    int gotFrame = 0;
    while (len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
            audio_size = audio_decode_frame(ffp);
            if (audio_size < 0) {
                /* if error, just output silence */
                is->audio_buf = NULL;
                is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
            } else {
                gotFrame = 1;
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;
        }
        if (is->auddec.pkt_serial != is->audioq.serial) {
            is->audio_buf_index = is->audio_buf_size;
            gotFrame = 0;
            break;
        }
        rest_len = is->audio_buf_size - is->audio_buf_index;
        if (rest_len > len)
            rest_len = len;
        
        len -= rest_len;
        is->audio_buf_index += rest_len;
    }
    
    /* Let's assume the audio driver that is used by SDL has two periods. */
    if (!isnan(is->audio_clock)) {
        double pts = 0.0;
        if (!gotFrame && rest_len == 0) {
            if (!isnan(is->audclk.pts)) {
                //none audio frame,already used out. is->audio_clock is the lastest audio frame pts and audio_write_buf_size is audio_buf_size(512 = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size).
                //use last pts and increase by audio callback eated bytes.
                pts = is->audclk.pts + (double)(len_want) / is->audio_tgt.bytes_per_sec;
            }
        } else {
            int audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
            pts = is->audio_clock - (double)(audio_write_buf_size) / is->audio_tgt.bytes_per_sec - SDL_AoutGetLatencySeconds(ffp->aout);
        }
        
        set_clock_at(&is->audclk, pts, is->audio_clock_serial, ffp->audio_callback_time / 1000000.0);
        sync_clock_to_slave(&is->extclk, &is->audclk);
        
        //when use step play mode,we use video pts sync audio,so drop the behind audio.
        double video_pts = is->step ? is->vidclk.pts : get_clock_with_delay(&is->vidclk);
        if (!isnan(video_pts)) {
            //audio pts is behind,need fast forwad,otherwise cause video picture delay and not smoothly!
            double threshold = is->step ? AV_SYNC_THRESHOLD_MIN : AV_SYNC_THRESHOLD_MAX;
            double delta = video_pts - pts - get_clock_extral_delay(&is->audclk);
            if (delta > threshold) {
                av_log(NULL, AV_LOG_INFO, "audio is behind:%0.3f\n", delta);
                return delta;
            }
        }
        return 0.0;
    }
    return 0.0;
}

/* prepare a new audio buffer */
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    FFPlayer *ffp = opaque;
    VideoState *is = ffp->is;
    int audio_size, rest_len = 0;
    const int len_want = len;
    
    if (!ffp || !is) {
        memset(stream, 0, len);
        return;
    }

    ffp->audio_callback_time = av_gettime_relative();

    if (ffp->pf_playback_rate_changed) {
        ffp->pf_playback_rate_changed = 0;
#if defined(__ANDROID__)
        if (!ffp->soundtouch_enable) {
            SDL_AoutSetPlaybackRate(ffp->aout, ffp->pf_playback_rate);
        }
#else
        SDL_AoutSetPlaybackRate(ffp->aout, ffp->pf_playback_rate);
#endif
    }
    if (ffp->pf_playback_volume_changed) {
        ffp->pf_playback_volume_changed = 0;
        SDL_AoutSetPlaybackVolume(ffp->aout, ffp->pf_playback_volume);
    }
    int gotFrame = 0;
    while (len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
           audio_size = audio_decode_frame(ffp);
           if (audio_size < 0) {
                /* if error, just output silence */
               memset(stream, 0, len);
               is->audio_buf = NULL;
               is->audio_buf_size = 0;
               break;
           } else {
               gotFrame = 1;
               is->audio_buf_size = audio_size;
           }
           is->audio_buf_index = 0;
        }
        if (is->auddec.pkt_serial != is->audioq.serial) {
            is->audio_buf_index = is->audio_buf_size;
            memset(stream, 0, len);
            // stream += len;
            // len = 0;
            SDL_AoutFlushAudio(ffp->aout);
            update_sample_display(ffp, NULL, -1);
            gotFrame = 0;
            break;
        }
        rest_len = is->audio_buf_size - is->audio_buf_index;
        if (rest_len > len)
            rest_len = len;
        if (!is->muted && is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME) {
            memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, rest_len);
            //give same data to upper.
            update_sample_display(ffp, is->audio_buf + is->audio_buf_index, rest_len);
        } else {
            memset(stream, 0, rest_len);
            if (!is->muted && is->audio_buf)
                SDL_MixAudio(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, rest_len, is->audio_volume);
        }
        len -= rest_len;
        stream += rest_len;
        is->audio_buf_index += rest_len;
    }
    /* Let's assume the audio driver that is used by SDL has two periods. */
    if (!isnan(is->audio_clock)) {
        double pts = 0.0;
        if (len_want - len == 0) {
            if (!isnan(is->audclk.pts)) {
                //none audio frame,already used out. is->audio_clock is the lastest audio frame pts and audio_write_buf_size is audio_buf_size(512 = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size).
                //use last pts and increase by audio callback eated bytes.
                pts = is->audclk.pts + (double)(len_want) / is->audio_tgt.bytes_per_sec;
                set_clock_at(&is->audclk, pts, is->audio_clock_serial, ffp->audio_callback_time / 1000000.0);
                sync_clock_to_slave(&is->extclk, &is->audclk);
            }
        } else {
            int audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
            pts = is->audio_clock - (double)(audio_write_buf_size) / is->audio_tgt.bytes_per_sec - SDL_AoutGetLatencySeconds(ffp->aout);
            
            set_clock_at(&is->audclk, pts, is->audio_clock_serial, ffp->audio_callback_time / 1000000.0);
            sync_clock_to_slave(&is->extclk, &is->audclk);
            
            if (is->video_stream >= 0 && is->viddec.finished != is->videoq.serial && is->auddec.finished != is->audioq.serial && 0 == is->audio_accurate_seek_req) {
                //when use step play mode,we use video pts sync audio,so drop the behind audio.
                double video_pts = is->step ? is->vidclk.pts : get_clock(&is->vidclk);
                //audio pts is behind,need fast forwad,otherwise cause video picture delay and not smoothly!
                double threshold = is->step ? AV_SYNC_THRESHOLD_MIN : AV_SYNC_THRESHOLD_MAX;
                double diff = video_pts - get_clock(&is->audclk) - get_clock_extral_delay(&is->audclk);
                //when set audio delay, can not drop audio, because the diff will be handle by video repeat or drop.
                int auto_drop = (is->step || get_clock_extral_delay(&is->audclk) == 0) && !isnan(video_pts) && diff > threshold;
                if (auto_drop) {
                    av_log(NULL, AV_LOG_INFO, "audio pts is behind,need fast forwad,diff:%f\n", diff);
                    int counter = 3;
                    while (counter--) {
                        if (diff >= 0.01) {
                            diff = consume_audio_buffer(ffp, diff);
                        } else {
                            break;
                        }
                    }
                    SDL_AoutFlushAudio(ffp->aout);
                    update_sample_display(ffp, NULL, -1);
                    return;
                }
            }
        }

        if (gotFrame) {
            if (!ffp->first_audio_frame_rendered) {
                ffp->first_audio_frame_rendered = 1;
                ffp_notify_msg1(ffp, FFP_MSG_AUDIO_RENDERING_START);
            }
        }
    }

    if (is->latest_audio_seek_load_serial == is->audio_clock_serial) {
        int latest_audio_seek_load_serial = __atomic_exchange_n(&(is->latest_audio_seek_load_serial), -1, memory_order_seq_cst);
        if (latest_audio_seek_load_serial == is->audio_clock_serial) {
            if (ffp->av_sync_type == AV_SYNC_AUDIO_MASTER) {
                ffp_notify_msg2(ffp, FFP_MSG_AUDIO_SEEK_RENDERING_START, 1);
            } else {
                ffp_notify_msg2(ffp, FFP_MSG_AUDIO_SEEK_RENDERING_START, 0);
            }
        }
    }

    if (ffp->render_wait_start && !ffp->start_on_prepared && is->pause_req) {
        while (is->pause_req && !is->abort_request) {
            SDL_Delay(20);
        }
    }
}

static int audio_open(FFPlayer *opaque, AVChannelLayout *wanted_channel_layout, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
    FFPlayer *ffp = opaque;
    VideoState *is = ffp->is;
    SDL_AudioSpec wanted_spec, spec;
    const char *env;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;
    int wanted_nb_channels = wanted_channel_layout->nb_channels;
    
    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    wanted_nb_channels = wanted_channel_layout->nb_channels;
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
        return -1;
    }
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMIN(0xFFFF, FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AoutGetAudioPerSecondCallBacks(ffp->aout))));
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = opaque;
    while (SDL_AoutOpenAudio(ffp->aout, &wanted_spec, &spec) < 0) {
        /* avoid infinity loop on exit. --by bbcallen */
        if (is->abort_request)
            return -1;
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz)\n",
               wanted_spec.channels, wanted_spec.freq);
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log(NULL, AV_LOG_ERROR,
                       "No more combinations to try, audio open failed\n");
                return -1;
            }
        }
        av_channel_layout_default(wanted_channel_layout, wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        av_log(NULL, AV_LOG_ERROR,
               "SDL advised audio format %d is not supported!\n", spec.format);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, spec.channels);
        if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
            av_log(NULL, AV_LOG_ERROR,
                   "SDL advised channel count %d is not supported!\n", spec.channels);
            return -1;
        }
    }

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    if (av_channel_layout_copy(&audio_hw_params->ch_layout, wanted_channel_layout) < 0)
        return -1;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->ch_layout.nb_channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->ch_layout.nb_channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }

    SDL_AoutSetDefaultLatencySeconds(ffp->aout, ((double)(2 * spec.size)) / audio_hw_params->bytes_per_sec);
    return spec.size;
}

#ifdef __APPLE__
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts)
{
#warning metal todo rbg24
    const enum AVPixelFormat supported_fmts[] = {AV_PIX_FMT_VIDEOTOOLBOX,AV_PIX_FMT_NV12,AV_PIX_FMT_YUV420P,AV_PIX_FMT_UYVY422,AV_PIX_FMT_RGB24,AV_PIX_FMT_ARGB,AV_PIX_FMT_0RGB,AV_PIX_FMT_BGRA,AV_PIX_FMT_BGR0};
    
    for (const enum AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        for (int i = 0; i < sizeof(supported_fmts) / sizeof(enum AVPixelFormat); i++) {
            if (*p == supported_fmts[i])
                return *p;
        }
    }
    
    return AV_PIX_FMT_NONE;
}

static int hw_decoder_init(AVCodecContext * ctx, const AVCodecHWConfig* config) {
    int err = 0;
    AVBufferRef *hw_device_ctx = NULL;
    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, config->device_type, NULL, NULL, 0)) < 0) {
        ALOGE("create mac HW device failed for type: %d\n", config->device_type);
        return err;
    }
    //将硬件支持的图像格式传给解码器的方法
    ctx->get_format = get_hw_format;
    av_opt_set_int(ctx, "refcounted_frames", 1, 0);
    //创建hw_device_ctx传给解码器上下文，必须在avcodec_open2之前并且之后不能修改
    ctx->hw_device_ctx = hw_device_ctx;
    return err;
}
#endif

static int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
    int ret = avformat_match_stream_specifier(s, st, spec);
    if (ret < 0)
        av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
    return ret;
}

static AVDictionary *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
                                AVFormatContext *s, AVStream *st,const AVCodec *codec)
{
    AVDictionary    *ret = NULL;
    AVDictionaryEntry *t = NULL;
    int            flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM
                                      : AV_OPT_FLAG_DECODING_PARAM;
    char          prefix = 0;
    const AVClass    *cc = avcodec_get_class();

    if (!codec)
        codec            = s->oformat ? avcodec_find_encoder(codec_id)
                                      : avcodec_find_decoder(codec_id);

    switch (st->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        prefix  = 'v';
        flags  |= AV_OPT_FLAG_VIDEO_PARAM;
        break;
    case AVMEDIA_TYPE_AUDIO:
        prefix  = 'a';
        flags  |= AV_OPT_FLAG_AUDIO_PARAM;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        prefix  = 's';
        flags  |= AV_OPT_FLAG_SUBTITLE_PARAM;
        break;
    default:
        break;
    }

    while ((t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX))) {
        const AVClass *priv_class;
        char *p = strchr(t->key, ':');

        /* check stream specification in opt name */
        if (p)
            switch (check_stream_specifier(s, st, p + 1)) {
            case  1: *p = 0; break;
            case  0:         continue;
            default:         return NULL;
            }

        if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
            !codec ||
            ((priv_class = codec->priv_class) &&
                        av_opt_find(&priv_class, t->key, NULL, flags,
                                    AV_OPT_SEARCH_FAKE_OBJ)))
            av_dict_set(&ret, t->key, t->value, 0);
        else if (t->key[0] == prefix &&
                 av_opt_find(&cc, t->key + 1, NULL, flags,
                             AV_OPT_SEARCH_FAKE_OBJ))
            av_dict_set(&ret, t->key + 1, t->value, 0);

        if (p)
            *p = ':';
    }
    return ret;
}

static void _ijkmeta_set_stream(FFPlayer* ffp, int type, int stream)
{
    switch (type) {
        case AVMEDIA_TYPE_VIDEO:
            ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_VIDEO_STREAM, stream);
            break;
        case AVMEDIA_TYPE_AUDIO:
            ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_AUDIO_STREAM, stream);
            break;
        case AVMEDIA_TYPE_SUBTITLE:
        case AVMEDIA_TYPE_NB + 1:
            ijkmeta_set_int64_l(ffp->meta, IJKM_KEY_TIMEDTEXT_STREAM, stream);
            break;
        default:
            break;
    }
}

/* open a given stream. Return 0 if OK */
static int stream_component_open(FFPlayer *ffp, int stream_index)
{
    VideoState *is = ffp->is;
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    const AVCodec *codec = NULL;
    const char *forced_codec_name = NULL;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *t = NULL;
    int sample_rate;
    AVChannelLayout ch_layout = { 0 };
    int ret = 0;
    int stream_lowres = ffp->lowres;
    
    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;
    
    AVStream *st = ic->streams[stream_index];
    assert(st->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE);
    
    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    ret = avcodec_parameters_to_context(avctx, st->codecpar);
    if (ret < 0)
        goto fail;
    avctx->pkt_timebase = st->time_base;

    codec = avcodec_find_decoder(avctx->codec_id);

    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO   : forced_codec_name = ffp->audio_codec_name; break;
        case AVMEDIA_TYPE_VIDEO   : forced_codec_name = ffp->video_codec_name; break;
        default: break;
    }
    if (forced_codec_name)
        codec = avcodec_find_decoder_by_name(forced_codec_name);
    if (!codec) {
        if (forced_codec_name) av_log(NULL, AV_LOG_WARNING,
                                      "No codec could be found with name '%s'\n", forced_codec_name);
        else                   av_log(NULL, AV_LOG_WARNING,
                                      "No codec could be found with id %s\n", avcodec_get_name(avctx->codec_id));
        ret = AVERROR(EINVAL);
        ffp_notify_msg2(ffp, FFP_MSG_NO_CODEC_FOUND, avctx->codec_id);
        goto fail;
    }

    avctx->codec_id = codec->id;
    
    if(stream_lowres > codec->max_lowres){
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
               codec->max_lowres);
        stream_lowres = codec->max_lowres;
    }
    avctx->lowres = stream_lowres;

    if (ffp->fast)
        avctx->flags2 |= AV_CODEC_FLAG2_FAST;

    opts = filter_codec_opts(ffp->codec_opts, avctx->codec_id, ic, st, (AVCodec *)codec);
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    if (stream_lowres)
        av_dict_set_int(&opts, "lowres", stream_lowres, 0);
    
#ifdef __APPLE__
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO && !(st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
        ALOGI("videotoolbox hwaccel switch:%s\n",ffp->videotoolbox_hwaccel ? "on" : "off");
        if (ffp->videotoolbox_hwaccel) {
            enum AVHWDeviceType type = av_hwdevice_find_type_by_name("videotoolbox");
            const AVCodecHWConfig *config = NULL;
            for (int i = 0;; i++) {
                const AVCodecHWConfig *node = avcodec_get_hw_config(codec, i);
                if (!node) {
                    ALOGE("avdec %s does not support device type %s.\n",
                            codec->name, av_hwdevice_get_type_name(type));
                    break;
                }
                if (node->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && node->device_type == type) {
                    config = node;
                    break;
                }
            }
            
            if (config && hw_decoder_init(avctx, config) == 0) {
                ALOGI("try use videotoolbox accel\n");
            }
        }
    }
#endif
    if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
        goto fail;
    }
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
#ifdef FFP_MERGE
        ret =  AVERROR_OPTION_NOT_FOUND;
        goto fail;
#endif
    }

    is->eof = 0;
    st->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
#if CONFIG_AVFILTER
        {
            AVFilterContext *sink;

            is->audio_filter_src.freq           = avctx->sample_rate;
            ret = av_channel_layout_copy(&is->audio_filter_src.ch_layout, &avctx->ch_layout);
            if (ret < 0)
                goto fail;
            is->audio_filter_src.fmt            = avctx->sample_fmt;
            SDL_LockMutex(ffp->af_mutex);
            if ((ret = configure_audio_filters(ffp, ffp->afilters, 0)) < 0) {
                SDL_UnlockMutex(ffp->af_mutex);
                goto fail;
            }
            ffp->af_changed = 0;
            SDL_UnlockMutex(ffp->af_mutex);
            sink = is->out_audio_filter;
            sample_rate    = av_buffersink_get_sample_rate(sink);
            ret = av_buffersink_get_ch_layout(sink, &ch_layout);
            if (ret < 0)
                goto fail;
        }
#else
        sample_rate    = avctx->sample_rate;
        ret = av_channel_layout_copy(&ch_layout, &avctx->ch_layout);
        if (ret < 0)
            goto fail;
#endif
        /* prepare audio output */
        if ((ret = audio_open(ffp, &ch_layout, sample_rate, &is->audio_tgt)) < 0)
            goto fail;
        ffp_set_audio_codec_info(ffp, AVCODEC_MODULE_NAME, avcodec_get_name(avctx->codec_id));
        is->audio_hw_buf_size = ret;
        is->audio_src = is->audio_tgt;
        is->audio_buf_size  = 0;
        is->audio_buf_index = 0;

        /* init averaging filter */
        is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio FIFO fullness,
           we correct audio sync only if larger than this threshold */
        is->audio_diff_threshold = 2.0 * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec;

        is->audio_stream = stream_index;
        is->audio_st = st;

        if((ret = decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread)) < 0)
            goto fail;
        if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
            is->auddec.start_pts = is->audio_st->start_time;
            is->auddec.start_pts_tb = is->audio_st->time_base;
        }
        if ((ret = decoder_start(&is->auddec, audio_thread, ffp, "ff_audio_dec")) < 0)
            goto out;
        SDL_AoutPauseAudio(ffp->aout, 0);
        _ijkmeta_set_stream(ffp, avctx->codec_type, stream_index);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = st;

        if (ffp->async_init_decoder) {
            while (!is->initialized_decoder) {
                SDL_Delay(5);
            }
            if (ffp->node_vdec) {
                is->viddec.avctx = avctx;
                ret = ffpipeline_config_video_decoder(ffp->pipeline, ffp);
            }
            if (ret || !ffp->node_vdec) {
                if((ret = decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread)) < 0)
                    goto fail;
                ffp->node_vdec = ffpipeline_open_video_decoder(ffp->pipeline, ffp);
                if (!ffp->node_vdec)
                    goto fail;
            }
        } else {
            if((ret = decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread)) < 0)
                goto fail;
            ffp->node_vdec = ffpipeline_open_video_decoder(ffp->pipeline, ffp);
            if (!ffp->node_vdec)
                goto fail;
        }
        if ((ret = decoder_start(&is->viddec, video_thread, ffp, "ff_video_dec")) < 0)
            goto out;

        is->queue_attachments_req = 1;

        if (ffp->max_fps >= 0) {
            if(is->video_st->avg_frame_rate.den && is->video_st->avg_frame_rate.num) {
                double fps = av_q2d(is->video_st->avg_frame_rate);
                SDL_ProfilerReset(&is->viddec.decode_profiler, fps + 0.5);
                if (fps > ffp->max_fps && fps < 130.0) {
                    is->is_video_high_fps = 1;
                    av_log(ffp, AV_LOG_WARNING, "fps: %lf (too high)\n", fps);
                } else {
                    av_log(ffp, AV_LOG_DEBUG, "fps: %lf (normal)\n", fps);
                }
                ffp->stat.vfps_probe = fps;
            }
            if(is->video_st->r_frame_rate.den && is->video_st->r_frame_rate.num) {
                double tbr = av_q2d(is->video_st->r_frame_rate);
                if (tbr > ffp->max_fps && tbr < 130.0) {
                    is->is_video_high_fps = 1;
                    av_log(ffp, AV_LOG_WARNING, "fps: %lf (too high)\n", tbr);
                } else {
                    av_log(ffp, AV_LOG_DEBUG, "fps: %lf (normal)\n", tbr);
                }
                if (ffp->stat.vfps_probe < 1) {
                    ffp->stat.vfps_probe = tbr;
                }
            }
        }
        // hevc high fps video use discard_nonref cause hw decode failed.
        if ((codec->id != AV_CODEC_ID_HEVC) && is->is_video_high_fps) {
            avctx->skip_frame       = FFMAX(avctx->skip_frame, AVDISCARD_NONREF);
            avctx->skip_loop_filter = FFMAX(avctx->skip_loop_filter, AVDISCARD_NONREF);
            avctx->skip_idct        = FFMAX(avctx->skip_loop_filter, AVDISCARD_NONREF);
        }
        _ijkmeta_set_stream(ffp, avctx->codec_type, stream_index);
        break;
    default:
        break;
    }
    goto out;

fail:
    avcodec_free_context(&avctx);
out:
    av_channel_layout_uninit(&ch_layout);
    av_dict_free(&opts);
    return ret;
}

static int decode_interrupt_cb(void *ctx)
{
    VideoState *is = ctx;
    return is->abort_request;
}

static int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue, int min_frames) {
    return stream_id < 0 ||
           queue->abort_request ||
           (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
#ifdef FFP_MERGE
           queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
#endif
           queue->nb_packets > min_frames;
}

static int is_realtime(AVFormatContext *s)
{
    if(   !strcmp(s->iformat->name, "rtp")
       || !strcmp(s->iformat->name, "rtsp")
       || !strcmp(s->iformat->name, "sdp")
    )
        return 1;

    if(s->pb && (   !strncmp(s->url, "rtp:", 4)
                 || !strncmp(s->url, "udp:", 4)
                )
    )
        return 1;
    return 0;
}

static AVDictionary **setup_find_stream_info_opts(AVFormatContext *s,
                                                  AVDictionary *codec_opts)
{
   int i;
   AVDictionary **opts;

   if (!s->nb_streams)
       return NULL;
   opts = av_mallocz(s->nb_streams * sizeof(*opts));
   if (!opts) {
       av_log(NULL, AV_LOG_ERROR,
              "Could not alloc memory for stream options.\n");
       return NULL;
   }
   for (i = 0; i < s->nb_streams; i++)
       opts[i] = filter_codec_opts(codec_opts, s->streams[i]->codecpar->codec_id,
                                   s, s->streams[i], NULL);
   return opts;
}

int ffp_apply_subtitle_preference(FFPlayer *ffp);

static void reset_buffer_size(FFPlayer *ffp)
{
    if (!ffp->is) {
        return;
    }
    SDL_LockMutex(ffp->is->play_mutex);
    int buffer_size = DEFAULT_QUEUE_SIZE;
    double audio_delay = ffp->is->audio_st ? get_clock_extral_delay(&ffp->is->audclk) : 0;
    AVFormatContext *ic = ffp->is->ic;
    if (ic) {
        if (ic->bit_rate > 0) {
            buffer_size = (int)(ic->bit_rate / 8) * (MAX_PACKETS_CACHE_DURATION);
    //        if (ic->bit_rate < 10000000) {
    //            buffer_size += ic->bit_rate/1000000 * 1024 * 1024;
    //        } else {
    //            buffer_size += 10 * 1024 * 1024;
    //            int rate = (int)(ic->bit_rate / 1000000);
    //            buffer_size += rate * 1024 * 1024;
    //        }
            buffer_size = FFMAX(DEFAULT_QUEUE_SIZE, buffer_size);
            buffer_size = FFMIN(MAX_QUEUE_SIZE, buffer_size);
        } else {
            buffer_size = (DEFAULT_QUEUE_SIZE + MAX_QUEUE_SIZE) / 2 + fabs(audio_delay) * 1024 * 1024;
        }
        ffp->dcc.max_buffer_size = buffer_size + fabs(audio_delay) * DEFAULT_QUEUE_SIZE;
        av_log(NULL, AV_LOG_INFO, "auto decision max buffer size:%dMB\n",ffp->dcc.max_buffer_size/1024/1024);
    }
    SDL_UnlockMutex(ffp->is->play_mutex);
}

/* this thread gets the stream from the disk or the network */
static int read_thread(void *arg)
{
    FFPlayer *ffp = arg;
    VideoState *is = ffp->is;
    AVFormatContext *ic = NULL;
    int err, i, ret __unused;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket *pkt = NULL;
    int64_t stream_start_time;
    int completed = 0;
    int pkt_in_play_range = 0;
    AVDictionaryEntry *t;
    SDL_mutex *wait_mutex = SDL_CreateMutex();
    int scan_all_pmts_set = 0;
    int64_t pkt_ts;
    int last_error = 0;
    int64_t prev_io_tick_counter = 0;
    int64_t io_tick_counter = 0;
    int init_ijkmeta = 0;
    int64_t icy_last_update_time = 0;
    
    if (!wait_mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    memset(st_index, -1, sizeof(st_index));
    is->eof = 0;

    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate packet.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    
    ic = avformat_alloc_context();
    if (!ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = is;
    if (!av_dict_get(ffp->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
        av_dict_set(&ffp->format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        scan_all_pmts_set = 1;
    }
    if (av_stristart(is->filename, "rtmp", NULL) ||
        av_stristart(is->filename, "rtsp", NULL)) {
        // There is total different meaning for 'timeout' option in rtmp
        av_log(ffp, AV_LOG_WARNING, "remove 'timeout' option for rtmp.\n");
        av_dict_set(&ffp->format_opts, "timeout", NULL, 0);
    }

    if (ffp->skip_calc_frame_rate) {
        av_dict_set_int(&ic->metadata, "skip-calc-frame-rate", ffp->skip_calc_frame_rate, 0);
        av_dict_set_int(&ffp->format_opts, "skip-calc-frame-rate", ffp->skip_calc_frame_rate, 0);
    }

    if (ffp->iformat_name)
        is->iformat = av_find_input_format(ffp->iformat_name);
 
    if (ffp->is_manifest) {
        extern AVInputFormat ijkff_las_demuxer;
        is->iformat = &ijkff_las_demuxer;
        av_dict_set_int(&ffp->format_opts, "las_player_statistic", (intptr_t) (&ffp->las_player_statistic), 0);
        ffp->find_stream_info = false;
    }
    
//    ijk_custom_avio_protocol * ijk_io = ijk_custom_avio_create(is->filename);
//    if (ijk_io) {
//        is->ijk_io = ijk_io;
//        ic->pb = ijk_io->get_avio(ijk_io);
//        is->filename = av_strdup(ijk_io->get_dummy_url(ijk_io));
//    }
    
    err = avformat_open_input(&ic, is->filename, is->iformat, &ffp->format_opts);
    if (err < 0) {
        ret = -1;
        
        av_log(NULL, AV_LOG_ERROR, "open [%s] failed:%s,err:%d\n", is->filename, av_err2str(err), err);
        last_error = err;
        goto fail;
    }
    
    ffp_notify_str2(ffp, FFP_MSG_OPEN_INPUT, ic->iformat->name);
    if (scan_all_pmts_set)
        av_dict_set(&ffp->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

    if ((t = av_dict_get(ffp->format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
#ifdef FFP_MERGE
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
#endif
    }
    is->ic = ic;
    //ic->debug |= FF_FDEBUG_TS;
    if (ffp->genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;

    av_format_inject_global_side_data(ic);
    //
    //AVDictionary **opts;
    //int orig_nb_streams;
    //opts = setup_find_stream_info_opts(ic, ffp->codec_opts);
    //orig_nb_streams = ic->nb_streams;


    if (ffp->find_stream_info) {
        AVDictionary **opts = setup_find_stream_info_opts(ic, ffp->codec_opts);
        int orig_nb_streams = ic->nb_streams;

        do {
            if (av_stristart(is->filename, "data:", NULL) && orig_nb_streams > 0) {
                for (i = 0; i < orig_nb_streams; i++) {
                    if (!ic->streams[i] || !ic->streams[i]->codecpar || ic->streams[i]->codecpar->profile == FF_PROFILE_UNKNOWN) {
                        break;
                    }
                }

                if (i == orig_nb_streams) {
                    break;
                }
            }
            err = avformat_find_stream_info(ic, opts);
        } while(0);
        ffp_notify_msg1(ffp, FFP_MSG_FIND_STREAM_INFO);

        for (i = 0; i < orig_nb_streams; i++)
            av_dict_free(&opts[i]);
        av_freep(&opts);

        if (err < 0) {
            av_log(NULL, AV_LOG_WARNING,
                   "%s: could not find codec parameters\n", is->filename);
            ret = -1;
            goto fail;
        }
    }
    if (ic->pb)
        ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

    if (ffp->seek_by_bytes < 0)
        ffp->seek_by_bytes = !(ic->iformat->flags & AVFMT_NO_BYTE_SEEK) &&
        !!(ic->iformat->flags & AVFMT_TS_DISCONT) &&
        strcmp("ogg", ic->iformat->name);


    is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
    is->max_frame_duration = 10.0;
    av_log(ffp, AV_LOG_INFO, "max_frame_duration: %.3f\n", is->max_frame_duration);

#ifdef FFP_MERGE
    if (!window_title && (t = av_dict_get(ic->metadata, "title", NULL, 0)))
        window_title = av_asprintf("%s - %s", t->value, input_filename);

#endif
    /* if seeking requested, we execute it */
    if (ffp->start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = ffp->start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING, "%s: could not seek to position %0.3f\n",
                    is->filename, (double)timestamp / AV_TIME_BASE);
        }
    }

    is->realtime = is_realtime(ic);

    av_dump_format(ic, 0, is->filename, 0);

    int video_stream_count = 0;
    int h264_stream_count = 0;
    int first_h264_stream = -1;
    for (i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL;
        if (type >= 0 && ffp->wanted_stream_spec[type] && st_index[type] == -1)
            if (avformat_match_stream_specifier(ic, st, ffp->wanted_stream_spec[type]) > 0)
                st_index[type] = i;

        // choose first h264
        if (type == AVMEDIA_TYPE_VIDEO) {
            enum AVCodecID codec_id = st->codecpar->codec_id;
            video_stream_count++;
            if (codec_id == AV_CODEC_ID_H264) {
                h264_stream_count++;
                if (first_h264_stream < 0)
                    first_h264_stream = i;
            }
        }
    }
    if (video_stream_count > 1 && st_index[AVMEDIA_TYPE_VIDEO] < 0) {
        st_index[AVMEDIA_TYPE_VIDEO] = first_h264_stream;
        av_log(NULL, AV_LOG_WARNING, "multiple video stream found, prefer first h264 stream: %d\n", first_h264_stream);
    }
    if (!ffp->video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    if (!ffp->audio_disable)
        st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                st_index[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);
    if (!ffp->video_disable && !ffp->subtitle_disable)
        st_index[AVMEDIA_TYPE_SUBTITLE] =
            av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                                st_index[AVMEDIA_TYPE_SUBTITLE],
                                (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                 st_index[AVMEDIA_TYPE_AUDIO] :
                                 st_index[AVMEDIA_TYPE_VIDEO]),
                                NULL, 0);

    is->show_mode = ffp->show_mode;
#ifdef FFP_MERGE // bbc: dunno if we need this
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters *codecpar = st->codecpar;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
        if (codecpar->width)
            set_default_window_size(codecpar->width, codecpar->height, sar);
    }
#endif

    /* open the streams */
    ret = -1;
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        ret = stream_component_open(ffp, st_index[AVMEDIA_TYPE_AUDIO]);
    }
    
    //when open audio stream failed or no audio stream use video as av sync master.
    if (ret || st_index[AVMEDIA_TYPE_AUDIO] == -1) {
        ffp->av_sync_type = AV_SYNC_VIDEO_MASTER;
        is->av_sync_type  = ffp->av_sync_type;
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = stream_component_open(ffp, st_index[AVMEDIA_TYPE_VIDEO]);
    }
    if (is->show_mode == SHOW_MODE_NONE)
        is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;
    
    //tell subtitle stream is ready.
    if (is->video_st) {
        AVCodecParameters *codecpar = is->video_st->codecpar;
                        
        int v_width = codecpar->width;
        if (codecpar->sample_aspect_ratio.num > 0 && codecpar->sample_aspect_ratio.den > 0) {
            float ratio = 1.0 * codecpar->sample_aspect_ratio.num / codecpar->sample_aspect_ratio.den;
            v_width = (int)(v_width * ratio);
        }
        ff_sub_stream_ic_ready(is->ffSub, ic, v_width, codecpar->height);
        if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
            AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_SUBTITLE]];
            st->discard = AVDISCARD_DEFAULT;
            ff_sub_record_need_select_stream(is->ffSub, st_index[AVMEDIA_TYPE_SUBTITLE]);
            ffp_apply_subtitle_preference(ffp);
        }
    }
    
    ffp_notify_msg1(ffp, FFP_MSG_COMPONENT_OPEN);

    if (!ffp->ijkmeta_delay_init) {
        ijkmeta_set_avformat_context_l(ffp->meta, ic);
    }

    /*a hdr 4k video,bit rate is 71404785,when max buffer size less than 10MB,
     the audio play may be pause-play-pause,so max buffer size need increase by movie bit rate
     */
    ffp->stat.bit_rate = ic->bit_rate;
    
    if (ffp->dcc.max_buffer_size == 0) {
        reset_buffer_size(ffp);
    }
    
    if (is->video_stream < 0 && is->audio_stream < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
               is->filename);
        //record open stream err code.
        if (is->audio_stream < 0) {
            last_error |= 1;
        } else if (is->video_stream < 0) {
            last_error |= 2;
        }
        ret = -1;
        goto fail;
    }
    if (is->audio_stream >= 0) {
        is->audioq.is_buffer_indicator = 1;
        is->buffer_indicator_queue = &is->audioq;
    } else if (is->video_stream >= 0) {
        is->videoq.is_buffer_indicator = 1;
        is->buffer_indicator_queue = &is->videoq;
    } else {
        assert("invalid streams");
    }

    if (ffp->infinite_buffer < 0 && is->realtime)
        ffp->infinite_buffer = 1;

    if (!ffp->render_wait_start && !ffp->start_on_prepared)
        toggle_pause(ffp, 1);
    if (is->video_st && is->video_st->codecpar) {
        //recode video z rotate degrees
        int deg = ffp_get_video_rotate_degrees(ffp);
        ffp->vout->z_rotate_degrees = deg;
        ffp_notify_msg2(ffp, FFP_MSG_VIDEO_Z_ROTATE_DEGREE, deg);
        AVCodecParameters *codecpar = is->video_st->codecpar;
        ffp_notify_msg3(ffp, FFP_MSG_VIDEO_SIZE_CHANGED, codecpar->width, codecpar->height);
        ffp_notify_msg3(ffp, FFP_MSG_SAR_CHANGED, codecpar->sample_aspect_ratio.num, codecpar->sample_aspect_ratio.den);
    }
    ffp->prepared = true;
    ffp_notify_msg1(ffp, FFP_MSG_PREPARED);
    if (!ffp->render_wait_start && !ffp->start_on_prepared) {
        while (is->pause_req && !is->abort_request) {
            SDL_Delay(20);
        }
    }
    if (ffp->auto_resume) {
        ffp_notify_msg1(ffp, FFP_REQ_START);
        ffp->auto_resume = 0;
    }
    /* offset should be seeked*/
    if (ffp->seek_at_start > 0) {
        ffp_seek_to_l(ffp, (long)(ffp->seek_at_start));
    }

    for (;;) {
        if (is->abort_request)
            break;
#ifdef FFP_MERGE
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }
#endif
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        if (is->paused &&
                (!strcmp(ic->iformat->name, "rtsp") ||
                 (ic->pb && !strncmp(ffp->input_filename, "mmsh:", 5)))) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
            SDL_Delay(10);
            continue;
        }
#endif
        if (is->seek_req) {
            int64_t seek_target_origin = is->seek_pos;
            int64_t seek_target = is->seek_pos;
            double audio_delay = is->audio_st ? get_clock_extral_delay(&is->audclk) : 0;
            //当音轨提前时，为了显示视频画面，所以比正常时间点往前偏移 audio_delay
            if (audio_delay < 0) {
                seek_target += audio_delay * AV_TIME_BASE;
                if (seek_target < 0) {
                    seek_target = 0;
                }
            }
            
            int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2: INT64_MIN;
            int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2: INT64_MAX;
// FIXME the +-2 is due to rounding being not done in the correct direction in generation
//      of the seek_pos/seek_rel variables

            ffp_toggle_buffering(ffp, 1);
            //fix after seek audio queue not flush cause wrong sound.
            SDL_AoutFlushAudio(ffp->aout);
            ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_UPDATE, 0, 0);
            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "%s: error while seeking\n", is->ic->url);
            } else {
                if (is->audio_stream >= 0)
                    packet_queue_flush(&is->audioq);
                if (is->video_stream >= 0) {
                    if (ffp->node_vdec) {
                        ffpipenode_flush(ffp->node_vdec);
                    }
                    packet_queue_flush(&is->videoq);
                }
                ff_sub_packet_queue_flush(is->ffSub);
                if (is->seek_flags & AVSEEK_FLAG_BYTE) {
                   set_clock(&is->extclk, NAN, 0);
                } else {
                   set_clock(&is->extclk, seek_target_origin / (double)AV_TIME_BASE, 0);
                }

                is->latest_video_seek_load_serial = is->videoq.serial;
                is->latest_audio_seek_load_serial = is->audioq.serial;
                is->latest_seek_load_start_at = av_gettime();
            }
            
            //seek the extra subtitle
            int sec = (int)fftime_to_seconds(seek_target_origin);
            float delay = ff_sub_get_delay(is->ffSub);
            ff_sub_seek_to(is->ffSub, delay, sec);
            //ff_sub_set_delay(is->ffSub, delay, sec);
            
            //seek 后降低水位线，让播放器更快满足条件
            ffp->dcc.current_high_water_mark_in_ms = ffp->dcc.first_high_water_mark_in_ms;
            is->seek_req = 0;
            is->viddec.after_seek_frame = 1;
            is->queue_attachments_req = 1;
            is->eof = 0;
#ifdef FFP_MERGE
            if (is->paused)
                step_to_next_frame(is);
#endif
            completed = 0;
            SDL_LockMutex(ffp->is->play_mutex);
            if (ffp->auto_resume) {
                is->pause_req = 0;
                if (ffp->packet_buffering)
                    is->buffering_on = 1;
                ffp->auto_resume = 0;
                stream_update_pause_l(ffp);
            }
            //after seek file,just try force step next video frame.
            is->step_on_seeking = 1;
            //if (is->pause_req)
              //  step_to_next_frame_l(ffp);
            SDL_UnlockMutex(ffp->is->play_mutex);

            if (ffp->enable_accurate_seek) {
                /*
                 pop old audio frames is necessary.
                 when enable accurate seek,video decoder thread waiting until the accurate seek timeout！
                 because the audio is paused,nobody cunsume the samples in sampq,and audio thread is waiting
                 sampq empty condition,can't drop any audio frmame,so video decoder thread wait forever.
                 */
                while (frame_queue_nb_remaining(&is->sampq) > 0) {
                    Frame *af = frame_queue_peek_readable(&is->sampq);
                    if (af && af->serial != is->audioq.serial) {
                        frame_queue_next(&is->sampq);
                    } else {
                        break;
                    }
                }
                is->audio_buf_index = 0;
                is->audio_buf_size = 0;
                
                is->drop_aframe_count = 0;
                is->drop_vframe_count = 0;
                SDL_LockMutex(is->accurate_seek_mutex);
                if (is->video_stream >= 0) {
                    is->video_accurate_seek_req = 1;
                }
                if (is->audio_stream >= 0) {
                    is->audio_accurate_seek_req = 1;
                }
                SDL_CondSignal(is->audio_accurate_seek_cond);
                SDL_CondSignal(is->video_accurate_seek_cond);
                SDL_UnlockMutex(is->accurate_seek_mutex);
            }

            ffp_notify_msg3(ffp, FFP_MSG_SEEK_COMPLETE, (int)fftime_to_milliseconds(seek_target_origin), ret);
            ffp_toggle_buffering(ffp, 1);
            
            if (is->show_mode != SHOW_MODE_VIDEO) {
                if (is->viddec.after_seek_frame) {
                    int du = (int)(SDL_GetTickHR() - is->viddec.start_seek_time);
                    is->viddec.after_seek_frame = 0;
                    ffp_notify_msg2(ffp, FFP_MSG_AFTER_SEEK_FIRST_FRAME, du);
                }
            }
        }
        if (is->queue_attachments_req) {
            if (is->video_st && (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                if ((ret = av_packet_ref(pkt, &is->video_st->attached_pic)) < 0)
                    goto fail;
                packet_queue_put(&is->videoq, pkt);
                packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
            }
            is->queue_attachments_req = 0;
        }

        /* if the queue are full, no need to read more */
        if (ffp->infinite_buffer < 1 && !is->seek_req &&
#ifdef FFP_MERGE
              (is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE)
#else
               (is->audioq.size + is->videoq.size + ff_sub_frame_cache_remaining(is->ffSub) > ffp->dcc.max_buffer_size
#endif
            || (   stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq, MIN_FRAMES)
                && stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq, MIN_FRAMES)
                && ff_sub_has_enough_packets(is->ffSub, MIN_FRAMES)))) {
            if (!is->eof) {
                ffp_toggle_buffering(ffp, 0);
            }
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        if ((!is->paused || completed) &&
            (!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0)) &&
            (!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0)) &&
            !is->step) {
            if (ffp->loop != 1 && (!ffp->loop || --ffp->loop)) {
                stream_seek(is, ffp->start_time != AV_NOPTS_VALUE ? ffp->start_time : 0, 0, 0);
            } else if (ffp->autoexit) {
                ret = AVERROR_EOF;
                goto fail;
            } else {
                ffp_statistic_l(ffp);
                if (completed) {
                    av_log(ffp, AV_LOG_INFO, "ffp_toggle_buffering: eof\n");
                    SDL_LockMutex(wait_mutex);
                    // infinite wait may block shutdown
                    while(!is->abort_request && !is->seek_req)
                        SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 100);
                    SDL_UnlockMutex(wait_mutex);
                    if (!is->abort_request)
                        continue;
                } else {
                    completed = 1;
                    ffp->auto_resume = 0;

                    // TODO: 0 it's a bit early to notify complete here
                    ffp_toggle_buffering(ffp, 0);
                    toggle_pause(ffp, 1);
                    if (ffp->error) {
                        av_log(ffp, AV_LOG_INFO, "ffp_toggle_buffering: error: %d\n", ffp->error);
                        ffp_notify_msg2(ffp, FFP_MSG_ERROR, ffp->error);
                    } else {
                        av_log(ffp, AV_LOG_INFO, "ffp_toggle_buffering: completed: OK\n");
                        ffp_notify_msg1(ffp, FFP_MSG_COMPLETED);
                    }
                }
            }
        }
        
        pkt->flags = 0;
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            int pb_eof = 0;
            int pb_error = 0;
            //monkey_log("av_read_frame failed:%4s\n",av_err2str(ret));
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                ffp_check_buffering_l(ffp);
                pb_eof = 1;
                // check error later
            }
            if (ic->pb && ic->pb->error) {
                pb_eof = 1;
                pb_error = ic->pb->error;
            }
            if (ret == AVERROR_EXIT) {
                pb_eof = 1;
                pb_error = AVERROR_EXIT;
            }

            if (pb_eof) {
                if (is->video_stream >= 0)
                    packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket(&is->audioq, pkt, is->audio_stream);
                int st = ff_sub_get_current_stream(is->ffSub, NULL);
                if (st >= 0) {
                    ff_sub_put_null_packet(is->ffSub, pkt, st);
                }
                is->eof = 1;
            }
            if (pb_error) {
                if (is->video_stream >= 0)
                    packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket(&is->audioq, pkt, is->audio_stream);
                int st = ff_sub_get_current_stream(is->ffSub, NULL);
                if (st >= 0) {
                    ff_sub_put_null_packet(is->ffSub, pkt, st);
                }
                is->eof = 1;
                ffp->error = pb_error;
                av_log(ffp, AV_LOG_ERROR, "av_read_frame error: %s\n", ffp_get_error_string(ffp->error));
                // break;
            } else {
                ffp->error = 0;
            }
            if (is->eof) {
                ffp_toggle_buffering(ffp, 0);
                //SDL_Delay(100);
                SDL_LockMutex(wait_mutex);
                SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 100);
                SDL_UnlockMutex(wait_mutex);
            }
//            ffpplay code
//            if (ic->pb && ic->pb->error) {
//                if (ffp->autoexit)
//                    goto fail;
//                else
//                    break;
//            }
            
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            ffp_statistic_l(ffp);
            continue;
        } else {
            is->eof = 0;
        }
        
        monkey_log("av_read_frame %s : %0.2f\n",pkt->stream_index == is->audio_stream ? "audio" : "video", pkt->pts * av_q2d(ic->streams[pkt->stream_index]->time_base));
        
        int64_t now = av_gettime_relative() / 1000;
        if (now - icy_last_update_time > ffp->icy_update_period) {
            icy_last_update_time = now;
            int r = ijkmeta_update_icy_from_avformat_context_l(ffp->meta, ic);
            if (r > 0) {
                ffp_notify_msg1(ffp, FFP_MSG_ICY_META_CHANGED);
            }
        }
        
        if (pkt->flags & AV_PKT_FLAG_DISCONTINUITY) {
        #warning TESTME
            if (is->audio_stream >= 0) {
                is->audioq.serial++;
                //packet_queue_put(&is->audioq, &flush_pkt);
            }
            if (is->video_stream >= 0) {
                is->videoq.serial++;
                //packet_queue_put(&is->videoq, &flush_pkt);
            }
        }
        AVStream *st = ic->streams[pkt->stream_index];
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        stream_start_time = st->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        pkt_in_play_range = ffp->duration == AV_NOPTS_VALUE ||
                (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                (double)(ffp->start_time != AV_NOPTS_VALUE ? ffp->start_time : 0) / AV_TIME_BASE
                <= ((double)ffp->duration / AV_TIME_BASE);
        if (!pkt_in_play_range) {
            av_packet_unref(pkt);
        } else {
            int stream_index = pkt->stream_index;
            if (stream_index == is->audio_stream) {
                packet_queue_put(&is->audioq, pkt);
            } else if (stream_index == is->video_stream
                       && !(is->video_st && (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC))) {
                packet_queue_put(&is->videoq, pkt);
            } else {
                int sub_pending_stream;
                int sub_stream = ff_sub_get_current_stream(is->ffSub, &sub_pending_stream);
                if (stream_index == sub_stream && sub_pending_stream == -2) {
                    ff_sub_put_packet(is->ffSub, pkt);
                } else if (stream_index == sub_pending_stream) {
                    ff_sub_put_packet_backup(is->ffSub, pkt);
                } else {
                    av_packet_unref(pkt);
                }
            }
        }
        
        ffp_statistic_l(ffp);

        if (ffp->ijkmeta_delay_init && !init_ijkmeta &&
                (ffp->first_video_frame_rendered || !is->video_st) && (ffp->first_audio_frame_rendered || !is->audio_st)) {
            ijkmeta_set_avformat_context_l(ffp->meta, ic);
            init_ijkmeta = 1;
        }

        if (ffp->packet_buffering) {
            io_tick_counter = SDL_GetTickHR();
            //首帧秒开，每隔50ms检查一次缓冲情况
            if ((!ffp->first_video_frame_rendered && is->video_st) || (!ffp->first_audio_frame_rendered && is->audio_st)) {
                if (abs((int)(io_tick_counter - prev_io_tick_counter)) > FAST_BUFFERING_CHECK_PER_MILLISECONDS) {
                    prev_io_tick_counter = io_tick_counter;
                    ffp->dcc.current_high_water_mark_in_ms = ffp->dcc.first_high_water_mark_in_ms;
                    ffp_check_buffering_l(ffp);
                }
            } else if (is->seek_buffering) {
                //seek之后，执行快速检查，50ms一次；让播放器更快的查询是否满足播放条件
                if (abs((int)(io_tick_counter - prev_io_tick_counter)) > FAST_BUFFERING_CHECK_PER_MILLISECONDS) {
                    prev_io_tick_counter = io_tick_counter;
                    ffp_check_buffering_l(ffp);
                }
            } else {
                //非首帧，每隔500ms检查一次缓冲情况
                if (abs((int)(io_tick_counter - prev_io_tick_counter)) > BUFFERING_CHECK_PER_MILLISECONDS) {
                    prev_io_tick_counter = io_tick_counter;
                    ffp_check_buffering_l(ffp);
                }
            }
        }
    }

    ret = 0;
 fail:
    if (ic && !is->ic)
        avformat_close_input(&ic);
    
    av_packet_free(&pkt);
            
    if (!ffp->prepared || !is->abort_request) {
        ffp_notify_msg2(ffp, FFP_MSG_ERROR, last_error);
    }
    SDL_DestroyMutex(wait_mutex);
    return 0;
}

static int video_refresh_thread(void *arg);
static VideoState *stream_open(FFPlayer *ffp, const char *filename, AVInputFormat *iformat)
{
    assert(!ffp->is);
    VideoState *is;

    is = av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;
    is->video_stream = -1;
    is->audio_stream = -1;
    is->filename = av_strdup(filename);
    if (!is->filename)
        goto fail;
    is->iformat = iformat;
    is->ytop    = 0;
    is->xleft   = 0;
#if defined(__ANDROID__)
    if (ffp->soundtouch_enable) {
        is->handle = ijk_soundtouch_create();
    }
#endif

    /* start video display */
    if (frame_queue_init(&is->pictq, &is->videoq, ffp->pictq_size, 1) < 0)
        goto fail;
    if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
        goto fail;

    if (packet_queue_init(&is->videoq) < 0 ||
        packet_queue_init(&is->audioq) < 0)
        goto fail;
    
    
    AVDictionary *opts = NULL;
    
    char *ic_string_val_keys[] = {"user_agent", "http_proxy", NULL};
    {
        char **ic_key_header = ic_string_val_keys;
        const char *ic_key;
        while ((ic_key = *ic_key_header)) {
            AVDictionaryEntry *entry = av_dict_get(ffp->format_opts, ic_key, NULL, AV_DICT_IGNORE_SUFFIX);
            if (entry && entry->value)
                av_dict_set(&opts, ic_key, entry->value, 0);
            ic_key_header++;
        }
    }
//    av_dict_set(&opts, "http_proxy", "http://127.0.0.1:8888", 0);
    av_dict_set_intptr(&opts, "ijkapplication", (intptr_t)ffp->app_ctx, 0);
    
    if (ff_sub_init(&is->ffSub, opts) < 0) {
        av_dict_free(&opts);
        goto fail;
    }
    av_dict_free(&opts);
    if (!(is->continue_read_thread = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        goto fail;
    }

    if (!(is->video_accurate_seek_cond = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        ffp->enable_accurate_seek = 0;
    }

    if (!(is->audio_accurate_seek_cond = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        ffp->enable_accurate_seek = 0;
    }

    init_clock(&is->vidclk, &is->videoq.serial, "video");
    init_clock(&is->audclk, &is->audioq.serial, "audio");
    init_clock(&is->extclk, &is->extclk.serial, "etx");
    is->audio_clock_serial = -1;
    is->audio_clock = NAN;
    if (ffp->startup_volume < 0)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d < 0, setting to 0\n", ffp->startup_volume);
    if (ffp->startup_volume > 100)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d > 100, setting to 100\n", ffp->startup_volume);
    ffp->startup_volume = av_clip(ffp->startup_volume, 0, 100);
    ffp->startup_volume = av_clip(SDL_MIX_MAXVOLUME * ffp->startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
    is->audio_volume = ffp->startup_volume;
    is->muted = 0;
    is->av_sync_type = ffp->av_sync_type;

    is->play_mutex = SDL_CreateMutex();
    is->accurate_seek_mutex = SDL_CreateMutex();
    ffp->is = is;
    is->pause_req = !ffp->start_on_prepared;

    is->video_refresh_tid = SDL_CreateThreadEx(&is->_video_refresh_tid, video_refresh_thread, ffp, "ff_vout");
    if (!is->video_refresh_tid) {
        av_freep(&ffp->is);
        return NULL;
    }

    is->initialized_decoder = 0;
    is->read_tid = SDL_CreateThreadEx(&is->_read_tid, read_thread, ffp, "ff_read");
    if (!is->read_tid) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateThread(): %s\n", SDL_GetError());
        goto fail;
    }

    if (ffp->async_init_decoder && !ffp->video_disable && ffp->video_mime_type && strlen(ffp->video_mime_type) > 0
                    && ffp->mediacodec_default_name && strlen(ffp->mediacodec_default_name) > 0) {
        if (ffp->mediacodec_all_videos || ffp->mediacodec_avc || ffp->mediacodec_hevc || ffp->mediacodec_mpeg2) {
            decoder_init(&is->viddec, NULL, &is->videoq, is->continue_read_thread);
            ffp->node_vdec = ffpipeline_init_video_decoder(ffp->pipeline, ffp);
        }
    }
    is->initialized_decoder = 1;

    return is;
fail:
    is->initialized_decoder = 1;
    is->abort_request = true;
    if (is->video_refresh_tid)
        SDL_WaitThread(is->video_refresh_tid, NULL);
    stream_close(ffp);
    return NULL;
}

// FFP_MERGE: stream_cycle_channel
// FFP_MERGE: toggle_full_screen
// FFP_MERGE: toggle_audio_display
// FFP_MERGE: refresh_loop_wait_event
// FFP_MERGE: event_loop
// FFP_MERGE: opt_width
// FFP_MERGE: opt_height
// FFP_MERGE: opt_format
// FFP_MERGE: opt_sync
// FFP_MERGE: opt_seek
// FFP_MERGE: opt_duration
// FFP_MERGE: opt_show_mode
// FFP_MERGE: opt_input_file
// FFP_MERGE: opt_codec
// FFP_MERGE: dummy
// FFP_MERGE: options
// FFP_MERGE: show_usage
// FFP_MERGE: show_help_default
static int video_refresh_thread(void *arg)
{
    FFPlayer *ffp = arg;
    VideoState *is = ffp->is;
    double remaining_time = 0.0;
    while (!is->abort_request) {
        if (remaining_time > 0.0)
            av_usleep((int)(int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        if (is->show_mode != SHOW_MODE_NONE && (!is->paused || is->force_refresh || is->step_on_seeking || is->force_refresh_sub_changed))
            video_refresh(ffp, &remaining_time);
    }
    //clean GLView's attach,because the attach retained sub_overlay;
    //otherwise sub_overlay will be free in main thread!
    SDL_VoutDisplayYUVOverlay(ffp->vout, NULL, NULL);
    ff_sub_desctoy_objs(is->ffSub);
    return 0;
}

// FFP_MERGE: main

/*****************************************************************************
 * end last line in ffplay.c
 ****************************************************************************/

static bool g_ffmpeg_global_inited = false;

inline static int log_level_av_to_ijk(int av_level)
{
    int ijk_level = IJK_LOG_VERBOSE;
    if      (av_level <= AV_LOG_PANIC)      ijk_level = IJK_LOG_FATAL;
    else if (av_level <= AV_LOG_FATAL)      ijk_level = IJK_LOG_FATAL;
    else if (av_level <= AV_LOG_ERROR)      ijk_level = IJK_LOG_ERROR;
    else if (av_level <= AV_LOG_WARNING)    ijk_level = IJK_LOG_WARN;
    else if (av_level <= AV_LOG_INFO)       ijk_level = IJK_LOG_INFO;
    // AV_LOG_VERBOSE means detailed info
    else if (av_level <= AV_LOG_VERBOSE)    ijk_level = IJK_LOG_INFO;
    else if (av_level <= AV_LOG_DEBUG)      ijk_level = IJK_LOG_DEBUG;
    else if (av_level <= AV_LOG_TRACE)      ijk_level = IJK_LOG_VERBOSE;
    else                                    ijk_level = IJK_LOG_VERBOSE;
    return ijk_level;
}

inline static int log_level_ijk_to_av(int ijk_level)
{
    int av_level = IJK_LOG_VERBOSE;
    if      (ijk_level >= IJK_LOG_SILENT)   av_level = AV_LOG_QUIET;
    else if (ijk_level >= IJK_LOG_FATAL)    av_level = AV_LOG_FATAL;
    else if (ijk_level >= IJK_LOG_ERROR)    av_level = AV_LOG_ERROR;
    else if (ijk_level >= IJK_LOG_WARN)     av_level = AV_LOG_WARNING;
    else if (ijk_level >= IJK_LOG_INFO)     av_level = AV_LOG_INFO;
    // AV_LOG_VERBOSE means detailed info
    else if (ijk_level >= IJK_LOG_DEBUG)    av_level = AV_LOG_DEBUG;
    else if (ijk_level >= IJK_LOG_VERBOSE)  av_level = AV_LOG_TRACE;
    else if (ijk_level >= IJK_LOG_DEFAULT)  av_level = AV_LOG_TRACE;
    else if (ijk_level >= IJK_LOG_UNKNOWN)  av_level = AV_LOG_TRACE;
    else                                    av_level = AV_LOG_TRACE;
    return av_level;
}

static void ffp_log_callback_brief(void *ptr, int level, const char *fmt, va_list vl)
{
    if (level > av_log_get_level())
        return;

    int ffplv __unused = log_level_av_to_ijk(level);
    VLOG(ffplv, IJK_LOG_TAG, fmt, vl);
}

static void ffp_log_callback_report(void *ptr, int level, const char *fmt, va_list vl)
{
    if (level > av_log_get_level())
        return;

    int ffplv __unused = log_level_av_to_ijk(level);

    va_list vl2;
    char line[1024];
    static int print_prefix = 1;

    va_copy(vl2, vl);
    // av_log_default_callback(ptr, level, fmt, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);

    ALOG(ffplv, IJK_LOG_TAG, "%s", line);
}

int ijkav_register_all(void);

int jik_log_callback_is_set = 0;

void ffp_global_init(void)
{
    if (g_ffmpeg_global_inited)
        return;
#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif

    ijkav_register_all();

    avformat_network_init();

    if (!jik_log_callback_is_set) {
        av_log_set_callback(ffp_log_callback_brief);
    }

    g_ffmpeg_global_inited = true;
}

void ffp_global_uninit(void)
{
    if (!g_ffmpeg_global_inited)
        return;

    // FFP_MERGE: uninit_opts

    avformat_network_deinit();

    g_ffmpeg_global_inited = false;
}

void ffp_global_set_log_report(int use_report)
{
    jik_log_callback_is_set = 1;
    if (use_report) {
        av_log_set_callback(ffp_log_callback_report);
    } else {
        av_log_set_callback(ffp_log_callback_brief);
    }
}

int ffp_global_get_log_level(void)
{
    int avlv = av_log_get_level();
    return log_level_av_to_ijk(avlv);
}

void ffp_global_set_log_level(int log_level)
{
    int av_level = log_level_ijk_to_av(log_level);
    av_log_set_level(av_level);
}

static ijk_inject_callback s_inject_callback;
int inject_callback(void *opaque, int type, void *data, size_t data_size)
{
    if (s_inject_callback)
        return s_inject_callback(opaque, type, data, data_size);
    return 0;
}

void ffp_global_set_inject_callback(ijk_inject_callback cb)
{
    s_inject_callback = cb;
}

static const char *ffp_context_to_name(void *ptr)
{
    return "FFPlayer";
}

static void *ffp_context_child_next(void *obj, void *prev)
{
    return NULL;
}

//static const AVClass *ffp_context_child_class_next(const AVClass *prev)
//{
//    return NULL;
//}

const AVClass ffp_context_class = {
    .class_name       = "FFPlayer",
    .item_name        = ffp_context_to_name,
    .option           = ffp_context_options,
    .version          = LIBAVUTIL_VERSION_INT,
    .child_next       = ffp_context_child_next,
//    .child_class_next = ffp_context_child_class_next,
};

static const char *ijk_version_info(void)
{
    return IJKPLAYER_VERSION;
}

FFPlayer *ffp_create(void)
{
    FFPlayer* ffp = (FFPlayer*) av_mallocz(sizeof(FFPlayer));
    if (!ffp)
        return NULL;

    msg_queue_init(&ffp->msg_queue);
    ffp->af_mutex = SDL_CreateMutex();
    ffp->vf_mutex = SDL_CreateMutex();

    ffp_reset_internal(ffp);
    ffp->av_class = &ffp_context_class;
    ffp->meta = ijkmeta_create();

    av_opt_set_defaults(ffp);

    las_stat_init(&ffp->las_player_statistic);

    ffp->audio_samples_callback = NULL;
    return ffp;
}

void ffp_destroy(FFPlayer *ffp)
{
    if (!ffp)
        return;

    if (ffp->is) {
        av_log(NULL, AV_LOG_WARNING, "ffp_destroy_ffplayer: force stream_close()");
        stream_close(ffp);
        ffp->is = NULL;
    }

    SDL_VoutFreeP(&ffp->vout);
    SDL_AoutFreeP(&ffp->aout);
    SDL_GPUFreeP(&ffp->gpu);
    ffpipenode_free_p(&ffp->node_vdec);
    ffpipeline_free_p(&ffp->pipeline);
    ijkmeta_destroy_p(&ffp->meta);

    las_stat_destroy(&ffp->las_player_statistic);

    ffp_reset_internal(ffp);

    SDL_DestroyMutexP(&ffp->af_mutex);
    SDL_DestroyMutexP(&ffp->vf_mutex);

    msg_queue_destroy(&ffp->msg_queue);

    av_free(ffp);
}

void ffp_destroy_p(FFPlayer **pffp)
{
    if (!pffp)
        return;

    ffp_destroy(*pffp);
    *pffp = NULL;
}

static AVDictionary **ffp_get_opt_dict(FFPlayer *ffp, int opt_category)
{
    assert(ffp);

    switch (opt_category) {
        case FFP_OPT_CATEGORY_FORMAT:   return &ffp->format_opts;
        case FFP_OPT_CATEGORY_CODEC:    return &ffp->codec_opts;
        case FFP_OPT_CATEGORY_SWS:      return &ffp->sws_dict;
        case FFP_OPT_CATEGORY_PLAYER:   return &ffp->player_opts;
        case FFP_OPT_CATEGORY_SWR:      return &ffp->swr_opts;
        default:
            av_log(ffp, AV_LOG_ERROR, "unknown option category %d\n", opt_category);
            return NULL;
    }
}

static int app_func_event(AVApplicationContext *h, int message ,void *data, size_t size)
{
    if (!h || !h->opaque || !data)
        return 0;

    FFPlayer *ffp = (FFPlayer *)h->opaque;
    if (!ffp->inject_opaque)
        return 0;
    if (message == AVAPP_EVENT_IO_TRAFFIC && sizeof(AVAppIOTraffic) == size) {
        AVAppIOTraffic *event = (AVAppIOTraffic *)(intptr_t)data;
        if (event->bytes > 0) {
            ffp->stat.byte_count += event->bytes;
            SDL_SpeedSampler2Add(&ffp->stat.tcp_read_sampler, event->bytes);
        }
    } else if (message == AVAPP_EVENT_ASYNC_STATISTIC && sizeof(AVAppAsyncStatistic) == size) {
        AVAppAsyncStatistic *statistic =  (AVAppAsyncStatistic *) (intptr_t)data;
        ffp->stat.buf_backwards = statistic->buf_backwards;
        ffp->stat.buf_forwards = statistic->buf_forwards;
        ffp->stat.buf_capacity = statistic->buf_capacity;
    }
    return inject_callback(ffp->inject_opaque, message , data, size);
}

static int ijkio_app_func_event(IjkIOApplicationContext *h, int message ,void *data, size_t size)
{
    if (!h || !h->opaque || !data)
        return 0;

    FFPlayer *ffp = (FFPlayer *)h->opaque;
    if (!ffp->ijkio_inject_opaque)
        return 0;

    if (message == IJKIOAPP_EVENT_CACHE_STATISTIC && sizeof(IjkIOAppCacheStatistic) == size) {
        IjkIOAppCacheStatistic *statistic =  (IjkIOAppCacheStatistic *) (intptr_t)data;
        ffp->stat.cache_physical_pos      = statistic->cache_physical_pos;
        ffp->stat.cache_file_forwards     = statistic->cache_file_forwards;
        ffp->stat.cache_file_pos          = statistic->cache_file_pos;
        ffp->stat.cache_count_bytes       = statistic->cache_count_bytes;
        ffp->stat.logical_file_size       = statistic->logical_file_size;
    }

    return 0;
}

void *ffp_set_ijkio_inject_opaque(FFPlayer *ffp, void *opaque)
{
    if (!ffp)
        return NULL;
    void *prev_weak_thiz = ffp->ijkio_inject_opaque;
    ffp->ijkio_inject_opaque = opaque;

    ijkio_manager_destroyp(&ffp->ijkio_manager_ctx);
    ijkio_manager_create(&ffp->ijkio_manager_ctx, ffp);
    ijkio_manager_set_callback(ffp->ijkio_manager_ctx, ijkio_app_func_event);
    ffp_set_option_int(ffp, FFP_OPT_CATEGORY_FORMAT, "ijkiomanager", (int64_t)(intptr_t)ffp->ijkio_manager_ctx);

    return prev_weak_thiz;
}

void *ffp_set_inject_opaque(FFPlayer *ffp, void *opaque)
{
    if (!ffp)
        return NULL;
    void *prev_weak_thiz = ffp->inject_opaque;
    ffp->inject_opaque = opaque;
    av_application_closep(&ffp->app_ctx);
    av_application_open(&ffp->app_ctx, ffp);
    ffp_set_option_intptr(ffp, FFP_OPT_CATEGORY_FORMAT, "ijkapplication", (intptr_t)ffp->app_ctx);
    //can't use int, av_dict_strtoptr is NULL in libavformat/http.c
    //ffp_set_option_int(ffp, FFP_OPT_CATEGORY_FORMAT, "ijkapplication", (int64_t)(intptr_t)ffp->app_ctx);
    ffp->app_ctx->func_on_app_event = app_func_event;
    return prev_weak_thiz;
}

void ffp_set_option(FFPlayer *ffp, int opt_category, const char *name, const char *value)
{
    if (!ffp)
        return;

    AVDictionary **dict = ffp_get_opt_dict(ffp, opt_category);
    av_dict_set(dict, name, value, 0);
}

void ffp_set_option_int(FFPlayer *ffp, int opt_category, const char *name, int64_t value)
{
    if (!ffp)
        return;

    AVDictionary **dict = ffp_get_opt_dict(ffp, opt_category);
    av_dict_set_int(dict, name, value, 0);
}

void ffp_set_option_intptr(FFPlayer *ffp, int opt_category, const char *name, uintptr_t value)
{
    if (!ffp)
        return;

    AVDictionary **dict = ffp_get_opt_dict(ffp, opt_category);
    av_dict_set_intptr(dict, name, value, 0);
}

int ffp_get_video_codec_info(FFPlayer *ffp, char **codec_info)
{
    if (!codec_info)
        return -1;

    // FIXME: not thread-safe
    if (ffp->video_codec_info) {
        *codec_info = strdup(ffp->video_codec_info);
    } else {
        *codec_info = NULL;
    }
    return 0;
}

int ffp_get_audio_codec_info(FFPlayer *ffp, char **codec_info)
{
    if (!codec_info)
        return -1;

    // FIXME: not thread-safe
    if (ffp->audio_codec_info) {
        *codec_info = strdup(ffp->audio_codec_info);
    } else {
        *codec_info = NULL;
    }
    return 0;
}

static void ffp_show_dict(FFPlayer *ffp, const char *tag, AVDictionary *dict)
{
    AVDictionaryEntry *t = NULL;

    while ((t = av_dict_get(dict, "", t, AV_DICT_IGNORE_SUFFIX))) {
        av_log(ffp, AV_LOG_INFO, "%-*s: %-*s = %s\n", 12, tag, 28, t->key, t->value);
    }
}

#define FFP_VERSION_MODULE_NAME_LENGTH 13
static void ffp_show_version_str(FFPlayer *ffp, const char *module, const char *version)
{
        av_log(ffp, AV_LOG_INFO, "%-*s: %s\n", FFP_VERSION_MODULE_NAME_LENGTH, module, version);
}

static void ffp_show_version_int(FFPlayer *ffp, const char *module, unsigned version)
{
    av_log(ffp, AV_LOG_INFO, "%-*s: %u.%u.%u\n",
           FFP_VERSION_MODULE_NAME_LENGTH, module,
           (unsigned int)IJKVERSION_GET_MAJOR(version),
           (unsigned int)IJKVERSION_GET_MINOR(version),
           (unsigned int)IJKVERSION_GET_MICRO(version));
}

#if CONFIG_AVFILTER
static void *grow_array(void *array, int elem_size, int *size, int new_size)
{
    if (new_size >= INT_MAX / elem_size) {
        av_log(NULL, AV_LOG_ERROR, "Array too big.\n");
        return NULL;
    }
    if (*size < new_size) {
        uint8_t *tmp = av_realloc_array(array, new_size, elem_size);
        if (!tmp) {
            av_log(NULL, AV_LOG_ERROR, "Could not alloc buffer.\n");
            return NULL;
        }
        memset(tmp + *size*elem_size, 0, (new_size-*size) * elem_size);
        *size = new_size;
        return tmp;
    }
    return array;
}

#define GROW_ARRAY(array, nb_elems)\
    array = grow_array(array, sizeof(*array), &nb_elems, nb_elems + 1)

static void resetVideoFilter(FFPlayer *ffp, const char *filter) {
    if (filter) {
        av_freep(&ffp->vfilters_list);
        VideoState *is = ffp->is;
        is->vfilter_idx = 0;
        GROW_ARRAY(ffp->vfilters_list, ffp->nb_vfilters);
        if (ffp->vfilters_list == NULL) {
            return;
        }
        ffp->vfilters_list[ffp->nb_vfilters - 1] = filter;
        ffp->vf_changed = 1;
    }
}
#endif

int ffp_prepare_async_l(FFPlayer *ffp, const char *file_name)
{
    assert(ffp);
    assert(!ffp->is);
    assert(file_name);

    if (av_stristart(file_name, "rtmp", NULL) ||
        av_stristart(file_name, "rtsp", NULL)) {
        // There is total different meaning for 'timeout' option in rtmp
        av_log(ffp, AV_LOG_WARNING, "remove 'timeout' option for rtmp.\n");
        av_dict_set(&ffp->format_opts, "timeout", NULL, 0);
    }

    static int once_flag = 1;
    
    if (once_flag) {
        once_flag = 0;
        av_log(NULL, AV_LOG_INFO, "===== versions =====\n");
        ffp_show_version_str(ffp, "ijkplayer",      ijk_version_info());
        ffp_show_version_str(ffp, "FFmpeg",         av_version_info());
        ffp_show_version_int(ffp, "libavutil",      avutil_version());
        ffp_show_version_int(ffp, "libavcodec",     avcodec_version());
        ffp_show_version_int(ffp, "libavformat",    avformat_version());
        ffp_show_version_int(ffp, "libswscale",     swscale_version());
        ffp_show_version_int(ffp, "libswresample",  swresample_version());
    }
    
    av_log(NULL, AV_LOG_INFO, "===== options =====\n");
    ffp_show_dict(ffp, "player-opts", ffp->player_opts);
    ffp_show_dict(ffp, "format-opts", ffp->format_opts);
    ffp_show_dict(ffp, "codec-opts ", ffp->codec_opts);
    ffp_show_dict(ffp, "sws-opts   ", ffp->sws_dict);
    ffp_show_dict(ffp, "swr-opts   ", ffp->swr_opts);
    av_log(NULL, AV_LOG_INFO, "===================\n");

    av_opt_set_dict(ffp, &ffp->player_opts);
    if (!ffp->aout) {
        ffp->aout = ffpipeline_open_audio_output(ffp->pipeline, ffp);
        if (!ffp->aout)
            return -1;
    }
    
    ffp->vout->cvpixelbufferpool = ffp->cvpixelbufferpool;
    ffp->vout->overlay_format    = ffp->overlay_format;
#if CONFIG_AVFILTER
    resetVideoFilter(ffp, ffp->vfilter0);
#endif

    VideoState *is = stream_open(ffp, file_name, NULL);
    if (!is) {
        av_log(NULL, AV_LOG_WARNING, "ffp_prepare_async_l: stream_open failed OOM");
        return EIJK_OUT_OF_MEMORY;
    }

    ffp->is = is;
    ffp->input_filename = av_strdup(file_name);
    return 0;
}

int ffp_start_from_l(FFPlayer *ffp, long msec)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is)
        return EIJK_NULL_IS_PTR;

    ffp->auto_resume = 1;
    ffp_toggle_buffering(ffp, 1);
    ffp_seek_to_l(ffp, msec);
    return 0;
}

int ffp_start_l(FFPlayer *ffp)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is)
        return EIJK_NULL_IS_PTR;

    toggle_pause(ffp, 0);
    return 0;
}

int ffp_pause_l(FFPlayer *ffp)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is)
        return EIJK_NULL_IS_PTR;

    toggle_pause(ffp, 1);
    return 0;
}

int ffp_is_paused_l(FFPlayer *ffp)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is)
        return 1;

    return is->paused;
}

int ffp_stop_l(FFPlayer *ffp)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (is) {
        is->abort_request = 1;
        toggle_pause(ffp, 1);
    }

    msg_queue_abort(&ffp->msg_queue);
    if (ffp->enable_accurate_seek && is && is->accurate_seek_mutex
        && is->audio_accurate_seek_cond && is->video_accurate_seek_cond) {
        SDL_LockMutex(is->accurate_seek_mutex);
        is->audio_accurate_seek_req = 0;
        is->video_accurate_seek_req = 0;
        SDL_CondSignal(is->audio_accurate_seek_cond);
        SDL_CondSignal(is->video_accurate_seek_cond);
        SDL_UnlockMutex(is->accurate_seek_mutex);
    }
    return 0;
}

int ffp_wait_stop_l(FFPlayer *ffp)
{
    assert(ffp);

    if (ffp->is) {
        ffp_stop_l(ffp);
        stream_close(ffp);
        ffp->is = NULL;
    }
    return 0;
}

int ffp_seek_to_l(FFPlayer *ffp, long msec)
{
    assert(ffp);
    VideoState *is = ffp->is;
    int64_t start_time = 0;
    int64_t seek_pos = milliseconds_to_fftime(msec);
    int64_t duration = milliseconds_to_fftime(ffp_get_duration_l(ffp));

    if (!is)
        return EIJK_NULL_IS_PTR;

    if (duration > 0 && seek_pos >= duration && ffp->enable_accurate_seek) {
        toggle_pause(ffp, 1);
        ffp_notify_msg1(ffp, FFP_MSG_COMPLETED);
        return 0;
    }

    start_time = is->ic->start_time;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE)
        seek_pos += start_time;

    // FIXME: 9 seek by bytes
    // FIXME: 9 seek out of range
    // FIXME: 9 seekable
    
    if (stream_seek(is, seek_pos, 0, 0) == 0) {
        av_log(ffp, AV_LOG_DEBUG, "stream_seek %"PRId64"(%d) + %"PRId64", \n", seek_pos, (int)msec, start_time);
    } else {
        av_log(ffp, AV_LOG_INFO, "ignore stream_seek %"PRId64"(%d) + %"PRId64", \n", seek_pos, (int)msec, start_time);
        return EIJK_FAILED;
    }
    return 0;
}

long ffp_get_current_position_l(FFPlayer *ffp)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is || !is->ic)
        return 0;

    int64_t start_time = is->ic->start_time;
    int64_t start_diff = 0;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE)
        start_diff = fftime_to_milliseconds(start_time);

    int64_t pos = 0;
    double pos_clock = get_master_clock(is);
    if (isnan(pos_clock)) {
        pos = fftime_to_milliseconds(is->seek_pos);
    } else {
        pos = pos_clock * 1000;
    }

    // If using REAL time and not ajusted, then return the real pos as calculated from the stream
    // the use case for this is primarily when using a custom non-seekable data source that starts
    // with a buffer that is NOT the start of the stream.  We want the get_current_position to
    // return the time in the stream, and not the player's internal clock.
    if (ffp->no_time_adjust) {
        return (long)pos;
    }

    if (pos < 0 || pos < start_diff)
        return 0;

    int64_t adjust_pos = pos - start_diff;
    return (long)adjust_pos;
}

long ffp_get_duration_l(FFPlayer *ffp)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is || !is->ic)
        return 0;

    int64_t duration = fftime_to_milliseconds(is->ic->duration);
    if (duration < 0)
        return 0;

    return (long)duration;
}

long ffp_get_playable_duration_l(FFPlayer *ffp)
{
    assert(ffp);
    if (!ffp)
        return 0;
    //when read frame eof,the playable duration is close to but less than the total duration,cause the playable progress bar is never full.
    if (ffp->is && ffp->is->eof) {
        return ffp_get_duration_l(ffp);
    }
    return (long)ffp->playable_duration_ms;
}

void ffp_set_loop(FFPlayer *ffp, int loop)
{
    assert(ffp);
    if (!ffp)
        return;
    ffp->loop = loop;
}

int ffp_get_loop(FFPlayer *ffp)
{
    assert(ffp);
    if (!ffp)
        return 1;
    return ffp->loop;
}

int ffp_packet_queue_get_or_buffering(FFPlayer *ffp, PacketQueue *q, AVPacket *pkt, int *serial, int *finished)
{
    return packet_queue_get_or_buffering(ffp, q, pkt, serial, finished);
}

int ffp_queue_picture(FFPlayer *ffp, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
    return queue_picture(ffp, src_frame, pts, duration, pos, serial);
}

int ffp_get_master_sync_type(VideoState *is)
{
    return get_master_sync_type(is);
}

double ffp_get_master_clock(VideoState *is)
{
    return get_master_clock(is);
}

void ffp_toggle_buffering_l(FFPlayer *ffp, int buffering_on)
{
    if (!ffp->packet_buffering)
        return;

    VideoState *is = ffp->is;
    if (buffering_on && !is->buffering_on) {
        av_log(ffp, AV_LOG_DEBUG, "ffp_toggle_buffering_l: start\n");
        is->buffering_on = 1;
        stream_update_pause_l(ffp);
        if (is->seek_req) {
            is->seek_buffering = 1;
            ffp_notify_msg2(ffp, FFP_MSG_BUFFERING_START, 1);
        } else {
            ffp_notify_msg2(ffp, FFP_MSG_BUFFERING_START, 0);
        }
    } else if (!buffering_on && is->buffering_on){
        av_log(ffp, AV_LOG_DEBUG, "ffp_toggle_buffering_l: end\n");
        is->buffering_on = 0;
        stream_update_pause_l(ffp);
        
        if (is->seek_buffering) {
            is->seek_buffering = 0;
            ffp_notify_msg2(ffp, FFP_MSG_BUFFERING_END, 1);
        } else {
            ffp_notify_msg2(ffp, FFP_MSG_BUFFERING_END, 0);
        }
    }
}

void ffp_toggle_buffering(FFPlayer *ffp, int start_buffering)
{
    SDL_LockMutex(ffp->is->play_mutex);
    ffp_toggle_buffering_l(ffp, start_buffering);
    SDL_UnlockMutex(ffp->is->play_mutex);
}

void ffp_check_buffering_l(FFPlayer *ffp)
{
    VideoState *is            = ffp->is;
    int hwm_in_ms             = ffp->dcc.current_high_water_mark_in_ms; // use fast water mark for first loading
    int buf_size_percent      = -1;
    int buf_time_percent      = -1;
    int hwm_in_bytes          = ffp->dcc.high_water_mark_in_bytes;
    int need_start_buffering  = 0;
    int audio_time_base_valid = 0;
    int video_time_base_valid = 0;
    int64_t buf_time_position = -1;

    if(is->audio_st)
        audio_time_base_valid = is->audio_st->time_base.den > 0 && is->audio_st->time_base.num > 0;
    if(is->video_st)
        video_time_base_valid = is->video_st->time_base.den > 0 && is->video_st->time_base.num > 0;

    if (hwm_in_ms > 0) {
        int     cached_duration_in_ms = -1;
        int64_t audio_cached_duration = -1;
        int64_t video_cached_duration = -1;

        if (is->audio_st && audio_time_base_valid) {
            audio_cached_duration = ffp->stat.audio_cache.duration;
#ifdef FFP_SHOW_DEMUX_CACHE
            int audio_cached_percent = (int)av_rescale(audio_cached_duration, 1005, hwm_in_ms * 10);
            av_log(ffp, AV_LOG_DEBUG, "audio cache=%%%d milli:(%d/%d) bytes:(%d/%d) packet:(%d/%d)\n", audio_cached_percent,
                  (int)audio_cached_duration, hwm_in_ms,
                  is->audioq.size, hwm_in_bytes,
                  is->audioq.nb_packets, MIN_FRAMES);
#endif
        }

        if (is->video_st && video_time_base_valid) {
            video_cached_duration = ffp->stat.video_cache.duration;
#ifdef FFP_SHOW_DEMUX_CACHE
            int video_cached_percent = (int)av_rescale(video_cached_duration, 1005, hwm_in_ms * 10);
            av_log(ffp, AV_LOG_DEBUG, "video cache=%%%d milli:(%d/%d) bytes:(%d/%d) packet:(%d/%d)\n", video_cached_percent,
                  (int)video_cached_duration, hwm_in_ms,
                  is->videoq.size, hwm_in_bytes,
                  is->videoq.nb_packets, MIN_FRAMES);
#endif
        }

        if (video_cached_duration > 0 && audio_cached_duration > 0) {
            cached_duration_in_ms = (int)IJKMIN(video_cached_duration, audio_cached_duration);
        } else if (video_cached_duration > 0) {
            cached_duration_in_ms = (int)video_cached_duration;
        } else if (audio_cached_duration > 0) {
            cached_duration_in_ms = (int)audio_cached_duration;
        }

        if (cached_duration_in_ms >= 0) {
            buf_time_position = ffp_get_current_position_l(ffp) + cached_duration_in_ms;
            ffp->playable_duration_ms = buf_time_position;

            buf_time_percent = (int)av_rescale(cached_duration_in_ms, 1005, hwm_in_ms * 10);
#ifdef FFP_SHOW_DEMUX_CACHE
            av_log(ffp, AV_LOG_DEBUG, "time cache=%%%d (%d/%d)\n", buf_time_percent, cached_duration_in_ms, hwm_in_ms);
#endif
#ifdef FFP_NOTIFY_BUF_TIME
            ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_TIME_UPDATE, cached_duration_in_ms, hwm_in_ms);
#endif
        }
    }

    int cached_size = is->audioq.size + is->videoq.size;
    if (hwm_in_bytes > 0) {
        buf_size_percent = (int)av_rescale(cached_size, 1005, hwm_in_bytes * 10);
#ifdef FFP_SHOW_DEMUX_CACHE
        av_log(ffp, AV_LOG_DEBUG, "size cache=%%%d (%d/%d)\n", buf_size_percent, cached_size, hwm_in_bytes);
#endif
#ifdef FFP_NOTIFY_BUF_BYTES
        ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_BYTES_UPDATE, cached_size, hwm_in_bytes);
#endif
    }

    int buf_percent = -1;
    if (buf_time_percent >= 0) {
        // alwas depend on cache duration if valid
        if (buf_time_percent >= 100)
            need_start_buffering = 1;
        buf_percent = buf_time_percent;
    } else {
        if (buf_size_percent >= 100)
            need_start_buffering = 1;
        buf_percent = buf_size_percent;
    }

    if (buf_time_percent >= 0 && buf_size_percent >= 0) {
        buf_percent = FFMIN(buf_time_percent, buf_size_percent);
    }
    if (buf_percent) {
#ifdef FFP_SHOW_BUF_POS
        av_log(ffp, AV_LOG_INFO, "buf pos=%"PRId64", %%%d\n", buf_time_position, buf_percent);
#endif
        ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_UPDATE, (int)buf_time_position, buf_percent);
    }

    if (need_start_buffering) {
        if (hwm_in_ms < ffp->dcc.next_high_water_mark_in_ms) {
            hwm_in_ms = ffp->dcc.next_high_water_mark_in_ms;
        } else {
            hwm_in_ms *= 2;
        }

        if (hwm_in_ms > ffp->dcc.last_high_water_mark_in_ms)
            hwm_in_ms = ffp->dcc.last_high_water_mark_in_ms;

        ffp->dcc.current_high_water_mark_in_ms = hwm_in_ms;

        if (is->buffer_indicator_queue && is->buffer_indicator_queue->nb_packets > 0) {
            if (   (is->audioq.nb_packets >= MIN_MIN_FRAMES || is->audio_stream < 0 || is->audioq.abort_request)
                && (is->videoq.nb_packets >= MIN_MIN_FRAMES || is->video_stream < 0 || is->videoq.abort_request)) {
                ffp_toggle_buffering(ffp, 0);
            }
        }
    }
}

int ffp_video_thread(FFPlayer *ffp)
{
    return ffplay_video_thread(ffp);
}

void ffp_set_video_codec_info(FFPlayer *ffp, const char *module, const char *codec)
{
    av_freep(&ffp->video_codec_info);
    ffp->video_codec_info = av_asprintf("%s, %s", module ? module : "", codec ? codec : "");
    av_log(ffp, AV_LOG_INFO, "VideoCodec: %s\n", ffp->video_codec_info);
}

void ffp_set_audio_codec_info(FFPlayer *ffp, const char *module, const char *codec)
{
    av_freep(&ffp->audio_codec_info);
    ffp->audio_codec_info = av_asprintf("%s, %s", module ? module : "", codec ? codec : "");
    av_log(ffp, AV_LOG_INFO, "AudioCodec: %s\n", ffp->audio_codec_info);
}

void ffp_set_subtitle_codec_info(FFPlayer *ffp, const char *module, const char *codec)
{
    av_freep(&ffp->subtitle_codec_info);
    ffp->subtitle_codec_info = av_asprintf("%s, %s", module ? module : "", codec ? codec : "");
    av_log(ffp, AV_LOG_INFO, "SubtitleCodec: %s\n", ffp->subtitle_codec_info);
}

void ffp_set_playback_rate(FFPlayer *ffp, float rate)
{
    if (!ffp)
        return;

    av_log(ffp, AV_LOG_INFO, "Playback rate: %f\n", rate);
    ffp->pf_playback_rate = rate;
    ffp->pf_playback_rate_changed = 1;
}

void ffp_set_playback_volume(FFPlayer *ffp, float volume)
{
    if (!ffp)
        return;
    ffp->pf_playback_volume = volume;
    ffp->pf_playback_volume_changed = 1;
}

int ffp_get_video_rotate_degrees(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    if (!is)
        return 0;
    int32_t *displaymatrix = (int32_t *)av_stream_get_side_data(is->video_st, AV_PKT_DATA_DISPLAYMATRIX, NULL);
    int theta  = abs((int)((int64_t)round(fabs(get_rotation(displaymatrix))) % 360));
    switch (theta) {
        case 0:
        case 90:
        case 180:
        case 270:
            break;
        case 360:
            theta = 0;
            break;
        default:
            ALOGW("Unknown rotate degress: %d\n", theta);
            theta = 0;
            break;
    }

    return theta;
}

//return value :
//err: less than zero;
//changed: greater than zero;
//already ok: zero
static int ffp_set_internal_stream_selected(FFPlayer *ffp, int stream, int selected)
{
    VideoState        *is = ffp->is;
    AVFormatContext   *ic = NULL;
    AVCodecParameters *codecpar = NULL;
    if (!is)
        return -1;
    ic = is->ic;
    if (!ic)
        return -1;
    
    int type = ic->streams[stream]->codecpar->codec_type;
    int opened = 0,closed = 0;
    
    switch (type) {
        case AVMEDIA_TYPE_VIDEO:
        {
            if (selected && stream == is->video_stream) {
                av_log(ffp, AV_LOG_INFO, "video stream has already been selected: %d\n", stream);
                return 0;
            }
            if (is->video_stream >= 0) {
                av_log(ffp, AV_LOG_INFO, "will close video stream : %d\n", is->video_stream);
                stream_component_close(ffp, is->video_stream);
                closed = 1;
            }
            if (selected) {
                av_log(ffp, AV_LOG_INFO, "will open video stream : %d\n", stream);
                opened = stream_component_open(ffp, stream) == 0;
            }
        }
            break;
        case AVMEDIA_TYPE_AUDIO:
        {
            if (selected && stream == is->audio_stream) {
                av_log(ffp, AV_LOG_INFO, "auido stream has already been selected: %d\n", stream);
                return 0;
            }
            if (is->audio_stream >= 0) {
                av_log(ffp, AV_LOG_INFO, "will close audio stream : %d\n", is->audio_stream);
                stream_component_close(ffp, is->audio_stream);
                closed = 1;
            }
            if (selected) {
                av_log(ffp, AV_LOG_INFO, "will open audio stream : %d\n", stream);
                //keep play rate and volume.
                ffp->pf_playback_rate_changed = 1;
                ffp->pf_playback_volume_changed = 1;
                opened = stream_component_open(ffp, stream) == 0;
            }
        }
            break;
        case AVMEDIA_TYPE_SUBTITLE:
        {
            int current = ff_sub_get_current_stream(is->ffSub, NULL);
            if (selected && stream == current) {
                av_log(ffp, AV_LOG_INFO, "subtitle stream has already been selected: %d\n", stream);
                return 0;
            }
            
            if (current >= 0 && current < ic->nb_streams) {
                AVStream *st = ic->streams[current];
                st->discard = AVDISCARD_ALL;
            }
            
            if (selected) {
                //let av_read_frame not discard
                AVStream *st = ic->streams[stream];
                st->discard = AVDISCARD_DEFAULT;
            }
            
            int r = ff_sub_record_need_select_stream(is->ffSub, selected ? stream : -1);
            if (r == 1) {
                ffp_apply_subtitle_preference(ffp);
                if (is->paused) {
                    is->force_refresh_sub_changed = 1;
                }
            }
            return r;
        }
        default:
            av_log(ffp, AV_LOG_ERROR, "select invalid stream %d of video type %d\n", stream, codecpar->codec_type);
            return -1;
    }
    
    if (opened || closed) {
        int idx = opened ? stream : -1;
        _ijkmeta_set_stream(ffp, type, idx);
        ffp_notify_msg1(ffp, FFP_MSG_SELECTED_STREAM_CHANGED);
    }
    return 1;
}

//return value :
//err: less than zero;
//already ok: zero
//greater than zero;means top caller need seek;
int ffp_set_stream_selected(FFPlayer *ffp, int stream, int selected)
{
    VideoState *is = ffp->is;
    if (!is)
        return -1;
    AVFormatContext *ic = is->ic;
    if (!ic)
        return -2;
    
    if (stream >= 0 && stream < ic->nb_streams) {
        return ffp_set_internal_stream_selected(ffp, stream, selected);
    } else {
        int r = ff_sub_record_need_select_stream(is->ffSub, selected ? stream : -1);
        if (r == 1) {
            ffp_apply_subtitle_preference(ffp);
            if (is->paused) {
                is->force_refresh_sub_changed = 1;
            }
            return 0;
        } else if (r == 0) {
            av_log(ffp, AV_LOG_INFO, "keep current selected ex subtile stream index: %d\n", stream);
        } else {
            av_log(ffp, AV_LOG_INFO, "can't selecet ext stream index: %d\n", stream);
        }
        return r;
    }
}

float ffp_get_property_float(FFPlayer *ffp, int id, float default_value)
{
    switch (id) {
        case FFP_PROP_FLOAT_VIDEO_DECODE_FRAMES_PER_SECOND:
            return ffp ? ffp->stat.vdps : default_value;
        case FFP_PROP_FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND:
            return ffp ? ffp->stat.vfps : default_value;
        case FFP_PROP_FLOAT_PLAYBACK_RATE:
            return ffp ? ffp->pf_playback_rate : default_value;
        case FFP_PROP_FLOAT_AVDELAY:
            return ffp ? ffp->stat.avdelay : default_value;
        case FFP_PROP_FLOAT_VMDIFF:
            return ffp ? ffp->stat.vmdiff : default_value;
        case FFP_PROP_FLOAT_PLAYBACK_VOLUME:
            return ffp ? ffp->pf_playback_volume : default_value;
        case FFP_PROP_FLOAT_DROP_FRAME_RATE:
            return ffp ? ffp->stat.drop_frame_rate : default_value;
        default:
            return default_value;
    }
}

void ffp_set_property_float(FFPlayer *ffp, int id, float value)
{
    switch (id) {
        case FFP_PROP_FLOAT_PLAYBACK_RATE:
            ffp_set_playback_rate(ffp, value);
            break;
        case FFP_PROP_FLOAT_PLAYBACK_VOLUME:
            ffp_set_playback_volume(ffp, value);
            break;
        default:
            return;
    }
}

int64_t ffp_get_property_int64(FFPlayer *ffp, int id, int64_t default_value)
{
    switch (id) {
        case FFP_PROP_INT64_SELECTED_VIDEO_STREAM:
            if (!ffp || !ffp->is)
                return default_value;
            return ffp->is->video_stream;
        case FFP_PROP_INT64_SELECTED_AUDIO_STREAM:
            if (!ffp || !ffp->is)
                return default_value;
            return ffp->is->audio_stream;
        case FFP_PROP_INT64_SELECTED_TIMEDTEXT_STREAM:
            if (!ffp || !ffp->is)
                return default_value;
            VideoState *is = ffp->is;
            int idx = ff_sub_get_current_stream(is->ffSub, NULL);
            if (idx >= 0) {
                return idx;
            } else {
                return default_value;
            }
        case FFP_PROP_INT64_VIDEO_DECODER:
            if (!ffp)
                return default_value;
            if (ffp->node_vdec) {
                return ffp->node_vdec->vdec_type;
            } else {
                return default_value;
            }
        case FFP_PROP_INT64_AUDIO_DECODER:
            return FFP_PROPV_DECODER_AVCODEC;

        case FFP_PROP_INT64_VIDEO_CACHED_DURATION:
            if (!ffp)
                return default_value;
            return ffp->stat.video_cache.duration;
        case FFP_PROP_INT64_AUDIO_CACHED_DURATION:
            if (!ffp)
                return default_value;
            return ffp->stat.audio_cache.duration;
        case FFP_PROP_INT64_VIDEO_CACHED_BYTES:
            if (!ffp)
                return default_value;
            return ffp->stat.video_cache.bytes;
        case FFP_PROP_INT64_AUDIO_CACHED_BYTES:
            if (!ffp)
                return default_value;
            return ffp->stat.audio_cache.bytes;
        case FFP_PROP_INT64_VIDEO_CACHED_PACKETS:
            if (!ffp)
                return default_value;
            return ffp->stat.video_cache.packets;
        case FFP_PROP_INT64_AUDIO_CACHED_PACKETS:
            if (!ffp)
                return default_value;
            return ffp->stat.audio_cache.packets;
        case FFP_PROP_INT64_BIT_RATE:
            return ffp ? ffp->stat.bit_rate : default_value;
        case FFP_PROP_INT64_TCP_SPEED:
            return ffp ? SDL_SpeedSampler2GetSpeed(&ffp->stat.tcp_read_sampler) : default_value;
        case FFP_PROP_INT64_ASYNC_STATISTIC_BUF_BACKWARDS:
            if (!ffp)
                return default_value;
            return ffp->stat.buf_backwards;
        case FFP_PROP_INT64_ASYNC_STATISTIC_BUF_FORWARDS:
            if (!ffp)
                return default_value;
            return ffp->stat.buf_forwards;
        case FFP_PROP_INT64_ASYNC_STATISTIC_BUF_CAPACITY:
            if (!ffp)
                return default_value;
            return ffp->stat.buf_capacity;
        case FFP_PROP_INT64_LATEST_SEEK_LOAD_DURATION:
            return ffp ? ffp->stat.latest_seek_load_duration : default_value;
        case FFP_PROP_INT64_TRAFFIC_STATISTIC_BYTE_COUNT:
            return ffp ? ffp->stat.byte_count : default_value;
        case FFP_PROP_INT64_CACHE_STATISTIC_PHYSICAL_POS:
            if (!ffp)
                return default_value;
            return ffp->stat.cache_physical_pos;
       case FFP_PROP_INT64_CACHE_STATISTIC_FILE_FORWARDS:
            if (!ffp)
                return default_value;
            return ffp->stat.cache_file_forwards;
       case FFP_PROP_INT64_CACHE_STATISTIC_FILE_POS:
            if (!ffp)
                return default_value;
            return ffp->stat.cache_file_pos;
       case FFP_PROP_INT64_CACHE_STATISTIC_COUNT_BYTES:
            if (!ffp)
                return default_value;
            return ffp->stat.cache_count_bytes;
       case FFP_PROP_INT64_LOGICAL_FILE_SIZE:
            if (!ffp)
                return default_value;
            return ffp->stat.logical_file_size;
        case FFP_PROP_FLOAT_DROP_FRAME_COUNT:
            return ffp ? ffp->stat.drop_frame_count : default_value;
        default:
            return default_value;
    }
}

void ffp_set_property_int64(FFPlayer *ffp, int id, int64_t value)
{
    switch (id) {
        // case FFP_PROP_INT64_SELECTED_VIDEO_STREAM:
        // case FFP_PROP_INT64_SELECTED_AUDIO_STREAM:
        case FFP_PROP_INT64_SHARE_CACHE_DATA:
            if (ffp) {
                if (value) {
                    ijkio_manager_will_share_cache_map(ffp->ijkio_manager_ctx);
                } else {
                    ijkio_manager_did_share_cache_map(ffp->ijkio_manager_ctx);
                }
            }
            break;
        case FFP_PROP_INT64_IMMEDIATE_RECONNECT:
            if (ffp) {
                ijkio_manager_immediate_reconnect(ffp->ijkio_manager_ctx);
            }
        default:
            break;
    }
}

IjkMediaMeta *ffp_get_meta_l(FFPlayer *ffp)
{
    if (!ffp)
        return NULL;

    return ffp->meta;
}

void ffp_set_audio_extra_delay(FFPlayer *ffp, const float delay)
{
    if (!ffp)
        return;
    VideoState *is = ffp->is;
    if (!is)
        return;
    set_clock_extral_delay(&is->audclk, delay);
    reset_buffer_size(ffp);
}

float ffp_get_audio_extra_delay(FFPlayer *ffp)
{
    if (!ffp)
        return 0.0f;
    VideoState *is = ffp->is;
    if (!is)
        return 0.0f;
    return get_clock_extral_delay(&is->audclk);
}

void ffp_set_subtitle_extra_delay(FFPlayer *ffp, const float delay, int64_t * need_seek)
{
    SDL_LockMutex(ffp->is->play_mutex);
    VideoState *is = ffp->is;
    int64_t ms = ffp_get_current_position_l(ffp);
    int r = ff_sub_set_delay(is->ffSub, delay, ms/1000.0);
    SDL_UnlockMutex(ffp->is->play_mutex);
    if (need_seek) {
        *need_seek = -1;
    }
    if (r == 1) {
        if (need_seek) {
            *need_seek = ms > 500 ? ms - 500 : 0;
        }
    }
}

float ffp_get_subtitle_extra_delay(FFPlayer *ffp)
{
    if (!ffp)
        return 0.0f;
    VideoState *is = ffp->is;
    if (!is)
        return 0.0f;
    return ff_sub_get_delay(is->ffSub);
}

//add + active
int ffp_add_active_external_subtitle(FFPlayer *ffp, const char *file_name)
{
#if CONFIG_AVFILTER
    //use subtitles filter
    char buffer[1024] = { 0 };
    sprintf(buffer, "subtitles=%s", file_name);
    resetVideoFilter(ffp, av_strdup(buffer));
    return 1;
#endif
    
    VideoState *is = ffp->is;
    
    int idx = -1;
    IjkMediaMeta *stream_meta = NULL;
    int r = ff_sub_add_ex_subtitle(is->ffSub, file_name, &stream_meta, &idx);
    if (r == 1) {
        //exist
    } else if (r == 0) {
        ijkmeta_append_child_l(ffp->meta, stream_meta);
        //succ,not send STREAM_CHANGED msg because would send after selected the new stream int vout thread.
        int ret = ff_sub_record_need_select_stream(is->ffSub, idx);
        if (ret == 1) {
            ffp_apply_subtitle_preference(ffp);
            if (is->paused) {
                is->force_refresh_sub_changed = 1;
            }
        }
        return r;
    } else {
        //fail
    }
    return r;
}

//add only
int ffp_addOnly_external_subtitle(FFPlayer *ffp, const char *file_name)
{
    VideoState *is = ffp->is;
    IjkMediaMeta *stream_meta = NULL;
    int r = ff_sub_add_ex_subtitle(is->ffSub, file_name, &stream_meta, NULL);
    if (r == 0) {
        ijkmeta_append_child_l(ffp->meta, stream_meta);
        ffp_notify_msg1(ffp, FFP_MSG_SELECTED_STREAM_CHANGED);
    }
    return r;
}

//add only
int ffp_addOnly_external_subtitles(FFPlayer *ffp, const char *file_names [], int count)
{
    VideoState *is = ffp->is;
    int ret = 0;
    for(int i = 0; i < count; i++) {
        const char *file = file_names[i];
        if (file) {
            IjkMediaMeta *stream_meta = NULL;
            if (ff_sub_add_ex_subtitle(is->ffSub, file, &stream_meta, NULL) == 0) {
                ijkmeta_append_child_l(ffp->meta, stream_meta);
                ret ++;
            }
        } else {
            break;
        }
    }
    if (ret > 0) {
        ffp_notify_msg1(ffp, FFP_MSG_SELECTED_STREAM_CHANGED);
    }
    return ret;
}

int ffp_get_frame_cache_remaining(FFPlayer *ffp, int type)
{
    if (!ffp || !ffp->is) {
        return 0;
    }
    if (type == 1) {
        return frame_queue_nb_remaining(&ffp->is->sampq);
    } else if (type == 2) {
        return frame_queue_nb_remaining(&ffp->is->pictq);
    } else if (type == 3) {
        return ff_sub_frame_cache_remaining(ffp->is->ffSub);
    }
    return 0;
}

void *ffp_set_inject_opaque(FFPlayer *ffp, void *opaque);

void ffp_set_audio_sample_observer(FFPlayer *ffp, ijk_audio_samples_callback cb)
{
    if (!ffp) {
        return;
    }
    ffp->audio_samples_callback = cb;
}

void ffp_set_enable_accurate_seek(FFPlayer *ffp, int open)
{
    if (!ffp || !ffp->is) {
        return;
    }
    SDL_LockMutex(ffp->is->accurate_seek_mutex);
    if (!ffp->is->video_accurate_seek_req && !ffp->is->audio_accurate_seek_req &&!ffp->is->seek_req) {
        ffp->enable_accurate_seek = open;
    }
    SDL_UnlockMutex(ffp->is->accurate_seek_mutex);
}

int ffp_apply_subtitle_preference(FFPlayer *ffp)
{
    if (!ffp || !ffp->is) {
        return 0;
    }
    return ff_update_sub_preference(ffp->is->ffSub, &ffp->sp);
}

void ffp_set_subtitle_preference(FFPlayer *ffp, IJKSDLSubtitlePreference* sp)
{
    if (!ffp || !sp) {
        return;
    }
    
    ffp->sp = *sp;
    int r = ffp_apply_subtitle_preference(ffp);
    //if subtitle preference changed and the player is paused,record need refresh vout
    if (r && ffp->is && ffp->is->paused) {
        ffp->is->force_refresh_sub_changed = 1;
    }
}
