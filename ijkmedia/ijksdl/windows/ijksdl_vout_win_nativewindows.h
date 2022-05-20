#ifndef IJKSDL_WINDOWS__IJKSDL_VOUT_WINDOWS_NATIVEWINDOW_H
#define IJKSDL_WINDOWS__IJKSDL_VOUT_WINDOWS_NATIVEWINDOW_H

#include "ijksdl/ijksdl_stdinc.h"
#include "ijksdl/ijksdl_vout.h"

typedef struct ANativeWindow   ANativeWindow;


SDL_Vout*  SDL_VoutWindows_CreateForANativeWindow();
void  SDL_VoutWindows_SetNativeWindow(SDL_Vout *vout,  HWND native_window);

void SDL_VoutWinNative_SetVideoDataCallback(void *arg, SDL_Vout *vout, int(*video_callback)(void *arg, SDL_VoutOverlay* overlay));

float SDL_GetSubtileFontSize(SDL_Vout* vout);

void SDL_SetSubtitleFontSize(SDL_Vout* vout, float size);

char* SDL_GetSubtitleFontName(SDL_Vout* vout);

void SDL_SetSubtitleFontName(SDL_Vout* vout, const char* font_name);

void* SDL_Snapshot(SDL_Vout* vout, int with_sub, void** pixel_data_out, int* w, int* h);

#endif