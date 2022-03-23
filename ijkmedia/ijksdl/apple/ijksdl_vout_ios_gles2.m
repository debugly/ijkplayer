/*
 * ijksdl_vout_ios_gles2.c
 *
 * Copyright (c) 2013 Bilibili
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

#import "ijksdl_vout_ios_gles2.h"

#include <assert.h>
#include "ijksdl/ijksdl_vout.h"
#include "ijksdl/ijksdl_vout_internal.h"
#include "ijksdl_vout_overlay_ffmpeg.h"
#include "ijksdl_vout_overlay_videotoolbox.h"
#import "IJKSDLGLView.h"

typedef struct SDL_VoutSurface_Opaque {
    SDL_Vout *vout;
} SDL_VoutSurface_Opaque;

struct SDL_Vout_Opaque {
    __strong GLView<IJKSDLGLViewProtocol> *gl_view;
    const char * subtitle;
    IJKSDLSubtitlePicture * subtitlePict;
};

static SDL_VoutOverlay *vout_create_overlay_l(int width, int height, int frame_format, SDL_Vout *vout)
{
    switch (frame_format) {
        case AV_PIX_FMT_VIDEOTOOLBOX:
        case IJK_AV_PIX_FMT__VIDEO_TOOLBOX:
            return SDL_VoutVideoToolBox_CreateOverlay(width, height, vout);
        default:
            return SDL_VoutFFmpeg_CreateOverlay(width, height, frame_format, vout);
    }
}

static SDL_VoutOverlay *vout_create_overlay(int width, int height, int frame_format, SDL_Vout *vout)
{
    SDL_LockMutex(vout->mutex);
    SDL_VoutOverlay *overlay = vout_create_overlay_l(width, height, frame_format, vout);
    SDL_UnlockMutex(vout->mutex);
    return overlay;
}

static void vout_free_l(SDL_Vout *vout)
{
    if (!vout)
        return;

    SDL_Vout_Opaque *opaque = vout->opaque;
    if (opaque) {
        if (opaque->gl_view) {
            // TODO: post to MainThread?
            opaque->gl_view = nil;
        }
        
        if (opaque->subtitle) {
            av_freep((void *)&opaque->subtitle);
        }
        
        if (opaque->subtitlePict) {
            if (opaque->subtitlePict->data)
                av_freep((void *)&opaque->subtitlePict->data);
            av_freep((void *)&opaque->subtitlePict);
        }
    }

    SDL_Vout_FreeInternal(vout);
}

static int vout_display_overlay_l(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    SDL_Vout_Opaque *opaque = vout->opaque;
    GLView<IJKSDLGLViewProtocol>* gl_view = opaque->gl_view;

    if (!gl_view) {
        ALOGE("vout_display_overlay_l: NULL gl_view\n");
        return -1;
    }

    if (!overlay) {
        ALOGE("vout_display_overlay_l: NULL overlay\n");
        return -1;
    }

    if (overlay->w <= 0 || overlay->h <= 0) {
        ALOGE("vout_display_overlay_l: invalid overlay dimensions(%d, %d)\n", overlay->w, overlay->h);
        return -1;
    }

    if (gl_view.isThirdGLView) {
        IJKOverlay ijk_overlay;

        ijk_overlay.w = overlay->w;
        ijk_overlay.h = overlay->h;
        ijk_overlay.format = overlay->format;
        ijk_overlay.planes = overlay->planes;
        ijk_overlay.pitches = overlay->pitches;
        ijk_overlay.sar_num = overlay->sar_num;
        ijk_overlay.sar_den = overlay->sar_den;
#ifdef __APPLE__
        if (ijk_overlay.format == SDL_FCC__VTB) {
            ijk_overlay.pixel_buffer = SDL_VoutOverlayVideoToolBox_GetCVPixelBufferRef(overlay);
        } else if (ijk_overlay.format == SDL_FCC__FFVTB) {
            ijk_overlay.pixel_buffer = SDL_VoutFFmpeg_GetCVPixelBufferRef(overlay);
        } else {
#if ! USE_FF_VTB
            ijk_overlay.pixels = overlay->pixels;
#endif
        }
#else
        ijk_overlay.pixels = overlay->pixels;
#endif
        if ([gl_view respondsToSelector:@selector(display_pixels:)]) {
            [gl_view display_pixels:&ijk_overlay];
        }
    } else {
        [gl_view display:overlay subtitle:opaque->subtitle subPict:opaque->subtitlePict];
    }
    return 0;
}

static int vout_display_overlay(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    @autoreleasepool {
        SDL_LockMutex(vout->mutex);
        int retval = vout_display_overlay_l(vout, overlay);
        SDL_UnlockMutex(vout->mutex);
        return retval;
    }
}

static void vout_update_subtitle(SDL_Vout *vout, const char *text)
{
    SDL_Vout_Opaque *opaque = vout->opaque;
    if (!opaque) {
        return;
    }
    
    if (opaque->subtitle) {
        av_freep((void *)&opaque->subtitle);
    }
    
    if (opaque->subtitlePict) {
        if (opaque->subtitlePict->data)
            av_freep((void *)&opaque->subtitlePict->data);
        av_freep((void *)&opaque->subtitlePict);
    }
    
    if (!text || strlen(text) == 0) {
        return;
    }
    
    opaque->subtitle = av_strdup(text);
}

static uint32_t* copy_pal8_to_bgra(const AVSubtitleRect* rect)
{
    size_t size = rect->w * rect->h * 4; /* times 4 because 4 bytes per pixel */
    uint32_t colours[255];
    uint32_t *buff = NULL;
    
    buff = av_malloc(size);
    if (buff == NULL) {
        ALOGE("Error allocating memory for subtitle bitmap.\n");
        return NULL;
    }
    
    for (int i = 0; i < 256; ++i) {
        /* Colour conversion. */
        int idx = i * 4; /* again, 4 bytes per pixel */
        uint8_t r = rect->data[1][idx],
        g = rect->data[1][idx + 1],
        b = rect->data[1][idx + 2],
        a = rect->data[1][idx + 3];
        colours[i] = (b << 24) | (g << 16) | (r << 8) | a;
    }
    
    for (int y = 0; y < rect->h; ++y) {
        for (int x = 0; x < rect->w; ++x) {
            /* 1 byte per pixel */
            int coordinate = x + y * rect->linesize[0];
            /* 32bpp color table */
            int idx = rect->data[0][coordinate];
            buff[x + (y * rect->w)] = colours[idx];
        }
    }
    
    return buff;
}

static void vout_copy_subtitle_picture(IJKSDLSubtitlePicture **dst, const AVSubtitleRect *rect)
{
    (*dst)->x = rect->x;
    (*dst)->y = rect->y;
    (*dst)->w = rect->w;
    (*dst)->h = rect->h;
    (*dst)->nb_colors = rect->nb_colors;
    
    /// the graphic subtitles' bitmap with pixel format AV_PIX_FMT_PAL8,
    /// https://ffmpeg.org/doxygen/trunk/pixfmt_8h.html#a9a8e335cf3be472042bc9f0cf80cd4c5
    /// need to be converted to BGRA32 before use
    (*dst)->data[0] = (uint8_t*)copy_pal8_to_bgra(rect);
    /// PAL8 to BGRA32, bytes per line increased by multiplied 4
    (*dst)->linesize[0] = rect->linesize[0] * 4;
}

static void vout_update_subtitle_picture(SDL_Vout *vout, const AVSubtitleRect *rect)
{
    SDL_Vout_Opaque *opaque = vout->opaque;
    if (!opaque) {
        return;
    }
    
    if (opaque->subtitle) {
        av_freep((void *)&opaque->subtitle);
    }
    
    if (opaque->subtitlePict) {
        for (int i = 0; i < 4; i++) {
            if (opaque->subtitlePict->data[i])
                av_freep((void *)&opaque->subtitlePict->data[i]);
        }
        av_freep((void *)&opaque->subtitlePict);
    }
    
    IJKSDLSubtitlePicture *pict = av_mallocz(sizeof(IJKSDLSubtitlePicture));
    vout_copy_subtitle_picture(&pict, rect);
    
    opaque->subtitlePict = pict;
}

SDL_Vout *SDL_VoutIos_CreateForGLES2()
{
    SDL_Vout *vout = SDL_Vout_CreateInternal(sizeof(SDL_Vout_Opaque));
    if (!vout)
        return NULL;

    SDL_Vout_Opaque *opaque = vout->opaque;
    opaque->gl_view = nil;
    opaque->subtitle = NULL;
    vout->create_overlay = vout_create_overlay;
    vout->free_l = vout_free_l;
    vout->display_overlay = vout_display_overlay;
    vout->update_subtitle = vout_update_subtitle;
    vout->update_subtitle_picture = vout_update_subtitle_picture;
    return vout;
}

static void SDL_VoutIos_SetGLView_l(SDL_Vout *vout, GLView<IJKSDLGLViewProtocol>* view)
{
    SDL_Vout_Opaque *opaque = vout->opaque;

    if (opaque->gl_view == view)
        return;

    if (opaque->gl_view) {
        opaque->gl_view = nil;
    }

    if (view)
        opaque->gl_view = view;
}

void SDL_VoutIos_SetGLView(SDL_Vout *vout, GLView<IJKSDLGLViewProtocol>* view)
{
    SDL_LockMutex(vout->mutex);
    SDL_VoutIos_SetGLView_l(vout, view);
    SDL_UnlockMutex(vout->mutex);
}
