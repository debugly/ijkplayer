/*****************************************************************************
 * ijksdl_vout_overlay_ffmpeg.c
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

#include "ijksdl_vout_overlay_ffmpeg.h"

#include <stdbool.h>
#include <assert.h>
#include "../ijksdl_stdinc.h"
#include "../ijksdl_misc.h"
#include "../ijksdl_mutex.h"
#include "../ijksdl_vout_internal.h"
#include "../ijksdl_video.h"
#include "ijksdl_inc_ffmpeg.h"
#include "ijksdl_image_convert.h"

struct SDL_VoutOverlay_Opaque {
    SDL_mutex *mutex;
    
    int planes;
    int no_neon_warned;

    struct SwsContext *img_convert_ctx;
    int sws_flags;
    
    AVFrame *managed_frame;
    AVBufferRef *frame_buffer;
    
    AVFrame *linked_frame;

    Uint8 *pixels[AV_NUM_DATA_POINTERS];
    Uint16 pitches[AV_NUM_DATA_POINTERS];
#if USE_FF_VTB
    CVPixelBufferRef pixelBuffer;
    CVPixelBufferPoolRef pixelBufferPool;
#endif

};

static SDL_Class g_vout_overlay_ffmpeg_class = {
    .name = "FFmpegVoutOverlay",
};

#if USE_FF_VTB

static NSDictionary* prepareCVPixelBufferAttibutes(const int format,const bool fullRange, const int h, const int w)
{
    //CoreVideo does not provide support for all of these formats; this list just defines their names.
    int pixelFormatType = 0;
    
    if (format == AV_PIX_FMT_RGB24) {
        pixelFormatType = kCVPixelFormatType_24RGB;
    } else if (format == AV_PIX_FMT_ARGB || format == AV_PIX_FMT_0RGB) {
        pixelFormatType = kCVPixelFormatType_32ARGB;
    } else if (format == AV_PIX_FMT_NV12) {
        pixelFormatType = fullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    } else if (format == AV_PIX_FMT_BGRA || format == AV_PIX_FMT_BGR0) {
        pixelFormatType = kCVPixelFormatType_32BGRA;
    } else if (format == AV_PIX_FMT_YUV420P) {
        pixelFormatType = kCVPixelFormatType_420YpCbCr8PlanarFullRange;
    } else if (format == AV_PIX_FMT_YUVJ420P) {
        pixelFormatType = kCVPixelFormatType_420YpCbCr8Planar;
    } else if (format == AV_PIX_FMT_UYVY422) {
        pixelFormatType = fullRange ? kCVPixelFormatType_422YpCbCr8FullRange : kCVPixelFormatType_422YpCbCr8;
    }
//    ffmpeg only;
//    else if (format == AV_PIX_FMT_YUYV422) {
//        pixelFormatType = kCVPixelFormatType_422YpCbCr8_yuvs;
//    } else if (format == AV_PIX_FMT_YUV444P10) {
//       pixelFormatType = kCVPixelFormatType_444YpCbCr10;
//    } else if (format == AV_PIX_FMT_NV16) {
//       pixelFormatType = fullRange ? kCVPixelFormatType_422YpCbCr8BiPlanarFullRange : kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange;
//    }
    //    kCVReturnInvalidPixelFormat
//    else if (format == AV_PIX_FMT_BGR24) {
//        pixelFormatType = kCVPixelFormatType_24BGR;
//    }
//    else if (format == AV_PIX_FMT_RGB565BE) {
//        pixelFormatType = kCVPixelFormatType_16BE565;
//    } else if (format == AV_PIX_FMT_RGB565LE) {
//        pixelFormatType = kCVPixelFormatType_16LE565;
//    }
//    else if (format == AV_PIX_FMT_RGB0 || format == AV_PIX_FMT_RGBA) {
//        pixelFormatType = kCVPixelFormatType_32RGBA;
//    }
//    RGB555 可以创建出 CVPixelBuffer，但是显示时失败了。
//    else if (format == AV_PIX_FMT_RGB555BE) {
//        pixelFormatType = kCVPixelFormatType_16BE555;
//    } else if (format == AV_PIX_FMT_RGB555LE) {
//        pixelFormatType = kCVPixelFormatType_16LE555;
//    }
    else {
        ALOGE("unsupported pixel format:%d!",format);
        assert(false);
        return nil;
    }
    
    const int linesize = 32;//FFmpeg 解码数据对齐是32，这里期望CVPixelBuffer也能使用32对齐，但实际来看却是64！
    NSMutableDictionary*attributes = [NSMutableDictionary dictionary];
    [attributes setObject:@(pixelFormatType) forKey:(NSString*)kCVPixelBufferPixelFormatTypeKey];
    [attributes setObject:[NSNumber numberWithInt:w] forKey:(NSString*)kCVPixelBufferWidthKey];
    [attributes setObject:[NSNumber numberWithInt:h] forKey:(NSString*)kCVPixelBufferHeightKey];
    [attributes setObject:@(linesize) forKey:(NSString*)kCVPixelBufferBytesPerRowAlignmentKey];
    [attributes setObject:[NSDictionary dictionary] forKey:(NSString*)kCVPixelBufferIOSurfacePropertiesKey];
    return attributes;
}

static CVReturn createCVPixelBufferPoolFromAVFrame(CVPixelBufferPoolRef * poolRef, int width, int height, int format)
{
    if (NULL == poolRef) {
        return kCVReturnInvalidArgument;
    }
    
    CVReturn result = kCVReturnError;
    //FIXME TODO
    const bool fullRange = true;
    NSDictionary * attributes = prepareCVPixelBufferAttibutes(format, fullRange, height, width);
    
    result = CVPixelBufferPoolCreate(NULL, NULL, (__bridge CFDictionaryRef) attributes, poolRef);
    
    if (result != kCVReturnSuccess) {
        ALOGE("CVPixelBufferCreate Failed:%d\n", result);
    }
    return result;
}

static CVPixelBufferRef createCVPixelBufferFromAVFrame(const AVFrame *frame,CVPixelBufferPoolRef poolRef)
{
    if (NULL == frame) {
        return NULL;
    }
    CVPixelBufferRef pixelBuffer = NULL;
    CVReturn result = kCVReturnError;
    
    const int w = frame->width;
    const int h = frame->height;
    const int format = frame->format;
    
    assert(w);
    assert(h);
    
    if (poolRef) {
        result = CVPixelBufferPoolCreatePixelBuffer(NULL, poolRef, &pixelBuffer);
    } else {
        //AVCOL_RANGE_MPEG对应tv，AVCOL_RANGE_JPEG对应pc
        //Y′ values are conventionally shifted and scaled to the range [16, 235] (referred to as studio swing or "TV levels") rather than using the full range of [0, 255] (referred to as full swing or "PC levels").
        //https://en.wikipedia.org/wiki/YUV#Numerical_approximations
        
        const bool fullRange = frame->color_range != AVCOL_RANGE_MPEG;
        NSDictionary * attributes = prepareCVPixelBufferAttibutes(format, fullRange, h, w);
        
        if (!attributes) {
            return NULL;
        }
        const int pixelFormatType = [attributes[(NSString*)kCVPixelBufferPixelFormatTypeKey] intValue];
        
        result = CVPixelBufferCreate(kCFAllocatorDefault,
                                     w,
                                     h,
                                     pixelFormatType,
                                     (__bridge CFDictionaryRef)(attributes),
                                     &pixelBuffer);
    }
    
    if (kCVReturnSuccess == result) {
        
        int planes = 1;
        if (CVPixelBufferIsPlanar(pixelBuffer)) {
            planes = (int)CVPixelBufferGetPlaneCount(pixelBuffer);
        }
        
        CVPixelBufferLockBaseAddress(pixelBuffer,0);
        for (int p = 0; p < planes; p++) {
            uint8_t *src = frame->data[p];
            uint8_t *dst = CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, p);
            if (!src || !dst) {
                continue;
            }
            
            int src_linesize = (int)frame->linesize[p];
            int dst_linesize = (int)CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, p);
            int height = (int)CVPixelBufferGetHeightOfPlane(pixelBuffer, p);
            
            if (src_linesize == dst_linesize) {
                memcpy(dst, src, dst_linesize * height);
            } else {
                int bytewidth = MIN(src_linesize, dst_linesize);
                av_image_copy_plane(dst, dst_linesize, src, src_linesize, bytewidth, height);
            }
        }
        CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
        return pixelBuffer;
    } else {
        ALOGE("CVPixelBufferCreate Failed:%d\n", result);
        assert(0);
        return NULL;
    }
}

static bool check_object(SDL_VoutOverlay* object, const char *func_name)
{
    if (!object || !object->opaque || !object->opaque_class) {
        ALOGE("%s: invalid pipeline\n", func_name);
        return false;
    }

    if (object->opaque_class != &g_vout_overlay_ffmpeg_class) {
        ALOGE("%s.%s: unsupported method\n", object->opaque_class->name, func_name);
        return false;
    }

    return true;
}

CVPixelBufferRef SDL_VoutFFmpeg_GetCVPixelBufferRef(SDL_VoutOverlay *overlay)
{
    if (!check_object(overlay, __func__))
        return NULL;

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    return opaque->pixelBuffer;
}

#else

/* Always assume a linesize alignment of 1 here */
// TODO: 9 alignment to speed up memcpy when display
static AVFrame *opaque_setup_frame(SDL_VoutOverlay_Opaque* opaque, enum AVPixelFormat format, int width, int height)
{
    AVFrame *managed_frame = av_frame_alloc();
    if (!managed_frame) {
        return NULL;
    }
    
    AVFrame *linked_frame = av_frame_alloc();
    if (!linked_frame) {
        av_frame_free(&managed_frame);
        return NULL;
    }
    
    /*-
     * Lazily allocate frame buffer in opaque_obtain_managed_frame_buffer
     *
     * For refererenced frame management, we use buffer allocated by decoder
     *
    int frame_bytes = avpicture_get_size(format, width, height);
    AVBufferRef *frame_buffer_ref = av_buffer_alloc(frame_bytes);
    if (!frame_buffer_ref)
        return NULL;
    opaque->frame_buffer  = frame_buffer_ref;
     */

    managed_frame->format = format;
    managed_frame->width  = width;
    managed_frame->height = height;
    av_image_fill_arrays(managed_frame->data, managed_frame->linesize ,NULL,
                         format, width, height, 1);
    opaque->managed_frame = managed_frame;
    opaque->linked_frame  = linked_frame;
    return managed_frame;
}

static AVFrame *opaque_obtain_managed_frame_buffer(SDL_VoutOverlay_Opaque* opaque)
{
    if (opaque->frame_buffer != NULL)
        return opaque->managed_frame;

    AVFrame *managed_frame = opaque->managed_frame;
    int frame_bytes = av_image_get_buffer_size(managed_frame->format, managed_frame->width, managed_frame->height, 1);
    AVBufferRef *frame_buffer_ref = av_buffer_alloc(frame_bytes);
    if (!frame_buffer_ref)
        return NULL;

    av_image_fill_arrays(managed_frame->data, managed_frame->linesize,
                         frame_buffer_ref->data, managed_frame->format, managed_frame->width, managed_frame->height, 1);
    opaque->frame_buffer  = frame_buffer_ref;
    return opaque->managed_frame;
}

static void overlay_fill(SDL_VoutOverlay *overlay, AVFrame *frame, int planes)
{
    overlay->planes = planes;

    for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
        overlay->pixels[i] = frame->data[i];
        overlay->pitches[i] = frame->linesize[i];
    }
}

#endif

static void func_free_l(SDL_VoutOverlay *overlay)
{
    ALOGE("SDL_Overlay(ffmpeg): overlay_free_l(%p)\n", overlay);
    if (!overlay)
        return;

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    if (!opaque)
        return;

    sws_freeContext(opaque->img_convert_ctx);

    if (opaque->managed_frame)
        av_frame_free(&opaque->managed_frame);

    if (opaque->linked_frame) {
        av_frame_unref(opaque->linked_frame);
        av_frame_free(&opaque->linked_frame);
    }

    if (opaque->frame_buffer)
        av_buffer_unref(&opaque->frame_buffer);
    
#if USE_FF_VTB
    if (opaque->pixelBuffer) {
        CVPixelBufferRelease(opaque->pixelBuffer);
        opaque->pixelBuffer = NULL;
    }
#endif
    
    if (opaque->mutex)
        SDL_DestroyMutex(opaque->mutex);
    
    SDL_VoutOverlay_FreeInternal(overlay);
}

static int func_lock(SDL_VoutOverlay *overlay)
{
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    return SDL_LockMutex(opaque->mutex);
}

static int func_unlock(SDL_VoutOverlay *overlay)
{
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    return SDL_UnlockMutex(opaque->mutex);
}

#if ! USE_FF_VTB
static int func_fill_frame(SDL_VoutOverlay *overlay, const AVFrame *frame)
{
    assert(overlay);
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    AVFrame swscale_dst_pic = { { 0 } };

    av_frame_unref(opaque->linked_frame);

    int need_swap_uv = 0;
    int use_linked_frame = 0;
    
    
    enum AVPixelFormat dst_format = AV_PIX_FMT_NONE;
    switch (overlay->format) {
        case SDL_FCC_YV12:
            need_swap_uv = 1;
            // no break;
        case SDL_FCC_I420:
        case SDL_FCC_J420:
            if (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P) {
                // ALOGE("direct draw frame");
                use_linked_frame = 1;
                dst_format = frame->format;
            } else {
                // ALOGE("copy draw frame");
                dst_format = AV_PIX_FMT_YUV420P;
            }
            break;
        case SDL_FCC_I444P10LE:
            if (frame->format == AV_PIX_FMT_YUV444P10LE) {
                // ALOGE("direct draw frame");
                use_linked_frame = 1;
                dst_format = frame->format;
            } else {
                // ALOGE("copy draw frame");
                dst_format = AV_PIX_FMT_YUV444P10LE;
            }
            break;
        default:
            dst_format = overlay->ff_format;
    }


    // setup frame
    if (use_linked_frame) {
        // linked frame
        av_frame_ref(opaque->linked_frame, frame);

        overlay_fill(overlay, opaque->linked_frame, opaque->planes);

        if (need_swap_uv)
            FFSWAP(Uint8*, overlay->pixels[1], overlay->pixels[2]);
    } else {
        // managed frame
        AVFrame* managed_frame = opaque_obtain_managed_frame_buffer(opaque);
        if (!managed_frame) {
            ALOGE("OOM in opaque_obtain_managed_frame_buffer");
            return -1;
        }

        overlay_fill(overlay, opaque->managed_frame, opaque->planes);

        // setup frame managed
        for (int i = 0; i < overlay->planes; ++i) {
            swscale_dst_pic.data[i] = overlay->pixels[i];
            swscale_dst_pic.linesize[i] = overlay->pitches[i];
        }

        if (need_swap_uv)
            FFSWAP(Uint8*, swscale_dst_pic.data[1], swscale_dst_pic.data[2]);
    }


    // swscale / direct draw
    /*
     ALOGE("ijk_image_convert w=%d, h=%d, df=%d, dd=%d, dl=%d, sf=%d, sd=%d, sl=%d",
     (int)frame->width,
     (int)frame->height,
     (int)dst_format,
     (int)swscale_dst_pic.data[0],
     (int)swscale_dst_pic.linesize[0],
     (int)frame->format,
     (int)(const uint8_t**) frame->data,
     (int)frame->linesize);
     */
    if (use_linked_frame) {
        // do nothing
    } else if (ijk_image_convert(frame->width, frame->height,
                                 dst_format, swscale_dst_pic.data, swscale_dst_pic.linesize,
                                 frame->format, (const uint8_t**) frame->data, frame->linesize)) {
        opaque->img_convert_ctx = sws_getCachedContext(opaque->img_convert_ctx,
                                                       frame->width, frame->height, frame->format, frame->width, frame->height,
                                                       dst_format, opaque->sws_flags, NULL, NULL, NULL);
        if (opaque->img_convert_ctx == NULL) {
            ALOGE("sws_getCachedContext failed");
            return -1;
        }

        sws_scale(opaque->img_convert_ctx, (const uint8_t**) frame->data, frame->linesize,
                  0, frame->height, swscale_dst_pic.data, swscale_dst_pic.linesize);

        if (!opaque->no_neon_warned) {
            opaque->no_neon_warned = 1;
            ALOGE("non-neon image convert %s -> %s", av_get_pix_fmt_name(frame->format), av_get_pix_fmt_name(dst_format));
        }
    }
    
    // TODO: 9 draw black if overlay is larger than screen
    return 0;
}

#else

static int func_fill_avframe_to_cvpixelbuffer(SDL_VoutOverlay *overlay, const AVFrame *frame)
{
    assert(overlay);
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    
    if (opaque->pixelBuffer) {
        CVPixelBufferRelease(opaque->pixelBuffer);
        opaque->pixelBuffer = NULL;
    }
    
    CVPixelBufferPoolRef poolRef = NULL;
    if (opaque->pixelBufferPool) {
        NSDictionary *attributes = (__bridge NSDictionary *)CVPixelBufferPoolGetPixelBufferAttributes(opaque->pixelBufferPool);
        int _width = [[attributes objectForKey:(NSString*)kCVPixelBufferWidthKey] intValue];
        int _height = [[attributes objectForKey:(NSString*)kCVPixelBufferHeightKey] intValue];
        if (frame->width == _width && frame->height == _height) {
            poolRef = opaque->pixelBufferPool;
        }
    }
    CVPixelBufferRef pixel_buffer = createCVPixelBufferFromAVFrame(frame, poolRef);
    if (pixel_buffer) {
        opaque->pixelBuffer = pixel_buffer;
        overlay->cv_format = CVPixelBufferGetPixelFormatType(pixel_buffer);
        
        if (CVPixelBufferIsPlanar(pixel_buffer)) {
            overlay->planes = (int)CVPixelBufferGetPlaneCount(pixel_buffer);
            for (int i = 0; i < overlay->planes; i ++) {
                overlay->pitches[i] = CVPixelBufferGetWidthOfPlane(pixel_buffer, i);
            }
        } else {
            overlay->planes = 1;
            overlay->pitches[0] = CVPixelBufferGetWidth(pixel_buffer);
        }
        
        return 0;
    }
    return 1;
}
#endif

#ifndef __clang_analyzer__
SDL_VoutOverlay *SDL_VoutFFmpeg_CreateOverlay(int width, int height, int frame_format, SDL_Vout *display)
{
    Uint32 overlay_format = display->overlay_format;
    if (SDL_FCC__GLES2 == overlay_format) {
#if defined(__ANDROID__)
        overlay_format = SDL_FCC_YV12;
#elif defined(__APPLE__)
    #if TARGET_OS_IOS
        if (frame_format == AV_PIX_FMT_YUV444P10LE) {
            overlay_format = SDL_FCC_I444P10LE;
        }
    #else
        if (frame_format == AV_PIX_FMT_UYVY422) {
            overlay_format = SDL_FCC_UYVY;
        }
    #endif
        
        if (frame_format == AV_PIX_FMT_YUV420P) {
            overlay_format = SDL_FCC_I420;
        } else if (frame_format == AV_PIX_FMT_YUVJ420P) {
            overlay_format = SDL_FCC_J420;
        } else {
            overlay_format = SDL_FCC_NV12;
        }
#endif
        //set as specific format. because one video maybe has more than a format, but glview's shader must be single.
        display->overlay_format = overlay_format;
    }

    SDL_VoutOverlay *overlay = SDL_VoutOverlay_CreateInternal(sizeof(SDL_VoutOverlay_Opaque));
    if (!overlay) {
        ALOGE("overlay allocation failed");
        return NULL;
    }

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    opaque->mutex         = SDL_CreateMutex();
    opaque->sws_flags     = SWS_BILINEAR;

    overlay->opaque_class = &g_vout_overlay_ffmpeg_class;
#if USE_FF_VTB
    overlay->format       = SDL_FCC__FFVTB;
    overlay->is_private   = 1;
#else
    overlay->format       = overlay_format;
    overlay->pixels       = opaque->pixels;
#endif
    overlay->pitches      = opaque->pitches;
    overlay->w            = width;
    overlay->h            = height;
    overlay->free_l             = func_free_l;
    overlay->lock               = func_lock;
    overlay->unlock             = func_unlock;
#if USE_FF_VTB
    overlay->func_fill_frame    = func_fill_avframe_to_cvpixelbuffer;
#else
    overlay->func_fill_frame    = func_fill_frame;
#endif
    enum AVPixelFormat ff_format = AV_PIX_FMT_NONE;
    int buf_width = width;
    switch (overlay_format) {
        case SDL_FCC_J420:
        case SDL_FCC_I420:
        case SDL_FCC_YV12:
        {
            if (overlay_format == SDL_FCC_J420) {
                ff_format = AV_PIX_FMT_YUVJ420P;
            } else {
                ff_format = AV_PIX_FMT_YUV420P;
            }
            // FIXME: need runtime config
    #if defined(__ANDROID__)
            // 16 bytes align pitch for arm-neon image-convert
            buf_width = IJKALIGN(width, 16); // 1 bytes per pixel for Y-plane
    #elif defined(__APPLE__)
            // 2^n align for width
            buf_width = width;
            if (width > 0)
                buf_width = 1 << (sizeof(int) * 8 - __builtin_clz(width));
    #else
            buf_width = IJKALIGN(width, 16); // unknown platform
    #endif
            opaque->planes = 3;
            break;
        }
#if ! TARGET_OS_OSX
        case SDL_FCC_I444P10LE: {
            ff_format = AV_PIX_FMT_YUV444P10LE;
            // FIXME: need runtime config
    #if defined(__ANDROID__)
            // 16 bytes align pitch for arm-neon image-convert
            buf_width = IJKALIGN(width, 16); // 1 bytes per pixel for Y-plane
    #elif defined(__APPLE__)
            // 2^n align for width
            buf_width = width;
            if (width > 0)
                buf_width = 1 << (sizeof(int) * 8 - __builtin_clz(width));
    #else
            buf_width = IJKALIGN(width, 16); // unknown platform
    #endif
            opaque->planes = 3;
            break;
        }
#endif
        case SDL_FCC_NV12: {
            ff_format = AV_PIX_FMT_NV12;
            buf_width = IJKALIGN(width, 4); // 4 bytes per pixel
            opaque->planes = 2;
            break;
        }
        case SDL_FCC_BGRA: {
            ff_format = AV_PIX_FMT_BGRA;
            buf_width = IJKALIGN(width, 4); // 4 bytes per pixel
            opaque->planes = 1;
            break;
        }
        case SDL_FCC_BGR0: {
            ff_format = AV_PIX_FMT_BGR0;
            buf_width = IJKALIGN(width, 4); // 4 bytes per pixel
            opaque->planes = 1;
            break;
        }
        case SDL_FCC_ARGB: {
            ff_format = AV_PIX_FMT_ARGB;
            buf_width = IJKALIGN(width, 4); // 4 bytes per pixel
            opaque->planes = 1;
            break;
        }
        case SDL_FCC_0RGB: {
            ff_format = AV_PIX_FMT_0RGB;
            buf_width = IJKALIGN(width, 4); // 4 bytes per pixel
            opaque->planes = 1;
            break;
        }
        case SDL_FCC_UYVY: {
            ff_format = AV_PIX_FMT_UYVY422;
            buf_width = IJKALIGN(width, 4); // 4 bytes per pixel
            opaque->planes = 1;
            break;
        }
#if ! USE_FF_VTB
        default:
            ALOGE("SDL_VoutFFmpeg_CreateOverlay(convert %.4s(0x%x)\n", (char*)&overlay_format, overlay_format);
            goto fail;
#endif
    }

#if USE_FF_VTB
    if (ff_format != AV_PIX_FMT_NONE) {
        SDLTRACE("SDL_VoutFFmpeg_CreateOverlay(w=%d, h=%d, fmt=%.4s, dp=%p)\n",
            width, height, (const char*) &overlay_format, display);
    } else {
        ALOGW("SDL_VoutFFmpeg_CreateOverlay(force convert fmt=%.4s to NV12, dp=%p)\n", (const char*) &overlay_format, display);
        ff_format = AV_PIX_FMT_NV12;
        buf_width = IJKALIGN(width, 4); // 4 bytes per pixel
        opaque->planes = 2;
    }
    
    if (!display->cvPixelBufferPool) {
        CVPixelBufferPoolRef cvPixelBufferPool = NULL;
        createCVPixelBufferPoolFromAVFrame(&cvPixelBufferPool, width, height,ff_format);
        display->cvPixelBufferPool = cvPixelBufferPool;
    }
    
    opaque->pixelBufferPool = (CVPixelBufferPoolRef)display->cvPixelBufferPool;
#endif
    //record ff_format
    overlay->ff_format = ff_format;
#if ! USE_FF_VTB
    int buf_height = height;
    opaque->managed_frame = opaque_setup_frame(opaque, ff_format, buf_width, buf_height);
    if (!opaque->managed_frame) {
        ALOGE("overlay->opaque->frame allocation failed\n");
        goto fail;
    }
    overlay_fill(overlay, opaque->managed_frame, opaque->planes);
#endif
    return overlay;
#if ! USE_FF_VTB
fail:
    func_free_l(overlay);
#endif
    return NULL;
}
#endif//__clang_analyzer__
