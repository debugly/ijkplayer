/*****************************************************************************
 * ijksdl_vout.h
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

#ifndef IJKSDL__IJKSDL_VOUT_H
#define IJKSDL__IJKSDL_VOUT_H

#include "ijksdl_stdinc.h"
#include "ijksdl_class.h"
#include "ijksdl_mutex.h"
#include "ijksdl_video.h"
#include "ffmpeg/ijksdl_inc_ffmpeg.h"
#include "ijksdl_fourcc.h"

typedef struct SDL_VoutOverlay_Opaque SDL_VoutOverlay_Opaque;
typedef struct SDL_VoutOverlay SDL_VoutOverlay;
struct SDL_VoutOverlay {
    int w; /**< Read-only */
    int h; /**< Read-only */
    Uint32 format; /**< Read-only such as SDL_FCC__VTB */
    Uint32 ff_format;/**< FFmpeg AV_PIXEL_FORMAT ; when format is SDL_FCC__VTB the value is CVPixelFormatType*/
#if USE_FF_VTB
    Uint32 cv_format;/**< when format is SDL_FCC__FFVTB the value is CVPixelFormatType*/
#else
    Uint8 **pixels; /**< Read-write */
#endif
    int planes; /**< Read-only */
    Uint16 *pitches; /**< in bytes, Read-only */
    
    int is_private;
    int sar_num;
    int sar_den;
    //for auto rotate video
    int auto_z_rotate_degrees;
    SDL_Class               *opaque_class;
    SDL_VoutOverlay_Opaque  *opaque;

    void    (*free_l)(SDL_VoutOverlay *overlay);
    int     (*lock)(SDL_VoutOverlay *overlay);
    int     (*unlock)(SDL_VoutOverlay *overlay);
    void    (*unref)(SDL_VoutOverlay *overlay);

    int     (*func_fill_frame)(SDL_VoutOverlay *overlay, const AVFrame *frame);
};

typedef struct SDL_Vout_Opaque SDL_Vout_Opaque;
typedef struct SDL_Vout SDL_Vout;
struct SDL_Vout {
    SDL_mutex *mutex;
    SDL_Class       *opaque_class;
    SDL_Vout_Opaque *opaque;
    SDL_VoutOverlay *(*create_overlay)(int width, int height, int frame_format, SDL_Vout *vout);
    void (*free_l)(SDL_Vout *vout);
    int (*display_overlay)(SDL_Vout *vout, SDL_VoutOverlay *overlay);
    void (*update_subtitle)(SDL_Vout *vout, const char *text);
    void (*update_subtitle_picture)(SDL_Vout *vout, const AVSubtitleRect *rect);
    
    Uint32 overlay_format;
    Uint32 ff_format;//(Read Only)same as SDL_VoutOverlay's ff_format.
    int z_rotate_degrees;
    //convert image
    void *image_converter;
};

void SDL_VoutFree(SDL_Vout *vout);
void SDL_VoutFreeP(SDL_Vout **pvout);
int  SDL_VoutDisplayYUVOverlay(SDL_Vout *vout, SDL_VoutOverlay *overlay);
int  SDL_VoutSetOverlayFormat(SDL_Vout *vout, Uint32 overlay_format);
//convert a frame use vout. not free outFrame,when free vout the outFrame will free. if convert failed return greater then 0.
int  SDL_VoutConvertFrame(SDL_Vout *vout, const AVFrame *inFrame, const AVFrame **outFrame);

SDL_VoutOverlay *SDL_Vout_CreateOverlay(int width, int height, int frame_format, SDL_Vout *vout);
int     SDL_VoutLockYUVOverlay(SDL_VoutOverlay *overlay);
int     SDL_VoutUnlockYUVOverlay(SDL_VoutOverlay *overlay);
void    SDL_VoutFreeYUVOverlay(SDL_VoutOverlay *overlay);
void    SDL_VoutUnrefYUVOverlay(SDL_VoutOverlay *overlay);
int     SDL_VoutFillFrameYUVOverlay(SDL_VoutOverlay *overlay, const AVFrame *frame);

#endif
