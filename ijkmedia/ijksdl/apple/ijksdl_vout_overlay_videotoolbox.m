/*****************************************************************************
 * ijksdl_vout_overlay_videotoolbox.m
 *****************************************************************************
 *
 * copyright (c) 2014 ZhouQuan <zhouqicy@gmail.com>
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

#include "ijksdl_vout_overlay_videotoolbox.h"

#include <assert.h>
#include "ijksdl_stdinc.h"
#include "ijksdl_mutex.h"
#include "ijksdl_vout_internal.h"
#include "ijksdl_video.h"


struct SDL_VoutOverlay_Opaque {
    SDL_mutex *mutex;
    CVPixelBufferRef pixel_buffer;
    Uint16 pitches[AV_NUM_DATA_POINTERS];
#if ! USE_FF_VTB
    Uint8 *pixels[AV_NUM_DATA_POINTERS];
#endif
};


static void func_free_l(SDL_VoutOverlay *overlay)
{
    if (!overlay)
        return;
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    if (!opaque)
        return;
    overlay->unref(overlay);
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

static void func_unref(SDL_VoutOverlay *overlay)
{
    if (!overlay) {
        return;
    }
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    if (!opaque) {
        return;
    }

    CVBufferRelease(opaque->pixel_buffer);

    opaque->pixel_buffer = NULL;
#if ! USE_FF_VTB
    overlay->pixels[0] = NULL;
    overlay->pixels[1] = NULL;
#endif
    return;
}

static int func_fill_frame(SDL_VoutOverlay *overlay, const AVFrame *frame)
{
    CVPixelBufferRef pixel_buffer = NULL;
    if (frame->format == IJK_AV_PIX_FMT__VIDEO_TOOLBOX) {
        pixel_buffer = CVPixelBufferRetain(frame->opaque);
    } else if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
        pixel_buffer = (CVPixelBufferRef)frame->data[3];
    } else {
        assert(0);
        return -100;
    }
    
    if (NULL == pixel_buffer) {
        return -1;
    }
    
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    if (opaque->pixel_buffer != NULL) {
        CVPixelBufferRelease(opaque->pixel_buffer);
    }
    opaque->pixel_buffer = CVPixelBufferRetain(pixel_buffer);
    overlay->format = SDL_FCC__VTB;
    overlay->ff_format = CVPixelBufferGetPixelFormatType(pixel_buffer);

    if (CVPixelBufferIsPlanar(pixel_buffer)) {
        overlay->planes = (int)CVPixelBufferGetPlaneCount(pixel_buffer);
        for (int i = 0; i < overlay->planes; i ++) {
            overlay->pitches[i] = CVPixelBufferGetWidthOfPlane(pixel_buffer, i);
        }
    } else {
        overlay->planes = 1;
        overlay->pitches[0] = CVPixelBufferGetWidth(pixel_buffer);
    }

#if 0
    if (CVPixelBufferLockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly) == kCVReturnSuccess) {
        
        if (CVPixelBufferIsPlanar(pixel_buffer)) {
            overlay->planes = (int)CVPixelBufferGetPlaneCount(pixel_buffer);
            for (int i = 0; i < overlay->planes; i ++) {
                overlay->pixels[i] = CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, i);
            }
        } else {
            overlay->pixels[0] = CVPixelBufferGetBaseAddress(pixel_buffer);
        }
        
        CVPixelBufferUnlockBaseAddress(pixel_buffer, kCVPixelBufferLock_ReadOnly);
    }
#endif
    
    overlay->is_private = 1;
    overlay->w = (int)frame->width;
    overlay->h = (int)frame->height;
    return 0;
}

static SDL_Class g_vout_overlay_videotoolbox_class = {
    .name = "VideoToolboxVoutOverlay",
};

static bool check_object(SDL_VoutOverlay* object, const char *func_name)
{
    if (!object || !object->opaque || !object->opaque_class) {
        ALOGE("%s: invalid pipeline\n", func_name);
        return false;
    }

    if (object->opaque_class != &g_vout_overlay_videotoolbox_class) {
        ALOGE("%s.%s: unsupported method\n", object->opaque_class->name, func_name);
        return false;
    }

    return true;
}

CVPixelBufferRef SDL_VoutOverlayVideoToolBox_GetCVPixelBufferRef(SDL_VoutOverlay *overlay)
{
    if (!check_object(overlay, __func__))
        return NULL;

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    return opaque->pixel_buffer;
}

SDL_VoutOverlay *SDL_VoutVideoToolBox_CreateOverlay(int width, int height, SDL_Vout *display)
{
    SDLTRACE("SDL_VoutVideoToolBox_CreateOverlay(w=%d, h=%d, fmt=_VTB, dp=%p)\n",
             width, height, display);
    SDL_VoutOverlay *overlay = SDL_VoutOverlay_CreateInternal(sizeof(SDL_VoutOverlay_Opaque));
    if (!overlay) {
        ALOGE("overlay allocation failed");
        return NULL;
    }
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    overlay->opaque_class = &g_vout_overlay_videotoolbox_class;
    overlay->format     = SDL_FCC__VTB;
    overlay->w          = width;
    overlay->h          = height;
    overlay->pitches    = opaque->pitches;
#if ! USE_FF_VTB
    overlay->pixels     = opaque->pixels;
#endif
    overlay->is_private = 1;

    overlay->free_l             = func_free_l;
    overlay->lock               = func_lock;
    overlay->unlock             = func_unlock;
    overlay->unref              = func_unref;
    overlay->func_fill_frame    = func_fill_frame;

    opaque->mutex = SDL_CreateMutex();
    return overlay;
}
