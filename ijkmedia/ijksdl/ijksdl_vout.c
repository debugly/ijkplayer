/*****************************************************************************
 * ijksdl_vout.c
 *****************************************************************************
 *
 * Copyright (c) 2013 Bilibili
 * copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
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

#include "ijksdl_vout.h"
#include <stdlib.h>

#include <assert.h>
#if defined(__ANDROID__)
#include <android/native_window_jni.h>
#endif
#include "ijksdl/ffmpeg/ijksdl_image_convert.h"

typedef struct _SDL_Image_Converter
{
    struct SwsContext *sws_ctx;
    AVFrame *frame;
    AVBufferRef *frame_buffer;
}_SDL_Image_Converter;

void SDL_VoutFree(SDL_Vout *vout)
{
    if (!vout)
        return;

    _SDL_Image_Converter *convert = vout->image_converter;
    if (NULL != convert) {
        if (convert->sws_ctx) {
            sws_freeContext(convert->sws_ctx);
        }
        if (convert->frame) {
            av_frame_free(&convert->frame);
            av_buffer_unref(&convert->frame_buffer);
        }
        vout->image_converter = NULL;
    }
    
    if (vout->free_l) {
        vout->free_l(vout);
    } else {
        free(vout);
    }
}

void SDL_VoutFreeP(SDL_Vout **pvout)
{
    if (!pvout)
        return;

    SDL_VoutFree(*pvout);
    *pvout = NULL;
}

int SDL_VoutDisplayYUVOverlay(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    if (vout && overlay && vout->display_overlay)
        return vout->display_overlay(vout, overlay);

    return -1;
}

int SDL_VoutSetOverlayFormat(SDL_Vout *vout, Uint32 overlay_format)
{
    if (!vout)
        return -1;

    vout->overlay_format = overlay_format;
    return 0;
}

int  SDL_VoutConvertFrame(SDL_Vout *vout, const AVFrame *inFrame, const AVFrame **outFrame)
{
    if (!vout) {
        return -1;
    }
    
    if (inFrame->format == vout->ff_format) {
        if (outFrame) {
            *outFrame = inFrame;
        }
        return 0;
    }
    
    _SDL_Image_Converter *convert = vout->image_converter;
    if (NULL == convert) {
        convert = malloc(sizeof(_SDL_Image_Converter));
        memset(convert, 0,sizeof(_SDL_Image_Converter));
        
        convert->frame = av_frame_alloc();
        
        int frame_bytes = av_image_get_buffer_size(vout->ff_format, inFrame->width, inFrame->height, 1);
        AVBufferRef *frame_buffer_ref = av_buffer_alloc(frame_bytes);
        if (!frame_buffer_ref) {
            free(convert);
            return -2;
        }
        
        av_frame_copy_props(convert->frame, inFrame);
        convert->frame->format  = vout->ff_format;
        
        av_image_fill_arrays(convert->frame->data, convert->frame->linesize,
                             frame_buffer_ref->data, vout->ff_format, inFrame->width, inFrame->height, 1);
        convert->frame_buffer  = frame_buffer_ref;
        
        vout->image_converter = convert;
    }
    
    int r = ijk_image_convert(inFrame->width, inFrame->height,
                             vout->ff_format, convert->frame->data, convert->frame->linesize,
                             inFrame->format, (const uint8_t**) inFrame->data, inFrame->linesize);
    
    if (r) {
        if (convert->sws_ctx == NULL) {
            convert->sws_ctx = sws_getCachedContext(convert->sws_ctx, inFrame->width, inFrame->height,
                                      inFrame->format, inFrame->width, inFrame->height,
                                      vout->ff_format, SWS_BILINEAR, NULL, NULL, NULL);
        }
        
        if (convert->sws_ctx == NULL) {
            ALOGE("sws_getCachedContext failed");
            return -3;
        }
        
        sws_scale(convert->sws_ctx, (const uint8_t**) inFrame->data, inFrame->linesize,
                  0, inFrame->height, convert->frame->data, convert->frame->linesize);
        r = 0;
    }
    
    if (r == 0 && outFrame) {
        convert->frame->width  = inFrame->width;
        convert->frame->height = inFrame->height;
        *outFrame = convert->frame;
    }
    
    return r;
}

SDL_VoutOverlay *SDL_Vout_CreateOverlay(int width, int height, int frame_format, SDL_Vout *vout)
{
    if (vout && vout->create_overlay)
        return vout->create_overlay(width, height, frame_format, vout);

    return NULL;
}

int SDL_VoutLockYUVOverlay(SDL_VoutOverlay *overlay)
{
    if (overlay && overlay->lock)
        return overlay->lock(overlay);

    return -1;
}

int SDL_VoutUnlockYUVOverlay(SDL_VoutOverlay *overlay)
{
    if (overlay && overlay->unlock)
        return overlay->unlock(overlay);

    return -1;
}

void SDL_VoutFreeYUVOverlay(SDL_VoutOverlay *overlay)
{
    if (!overlay)
        return;

    if (overlay->free_l) {
        overlay->free_l(overlay);
    } else {
        free(overlay);
    }
}

void SDL_VoutUnrefYUVOverlay(SDL_VoutOverlay *overlay)
{
    if (overlay && overlay->unref)
        overlay->unref(overlay);
}

int SDL_VoutFillFrameYUVOverlay(SDL_VoutOverlay *overlay, const AVFrame *frame)
{
    if (!overlay || !overlay->func_fill_frame)
        return -1;

    return overlay->func_fill_frame(overlay, frame);
}
