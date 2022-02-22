#ifndef IJKSDL_VOUT_WIN_FFMPEG_H
#define IJKSDL_VOUT_WIN_FFMPEG_H

#include "ijksdl/ijksdl_stdinc.h"
#include "ijksdl/ijksdl_vout.h"

SDL_Vout *SDL_VoutWin_CreateForWindows();

void SDL_VoutWin_SetVideoDataCallback(void *arg, SDL_Vout *vout, int(*video_callback)(void *arg, SDL_VoutOverlay* overlay));

#endif
