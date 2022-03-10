#ifndef IJKSDL_WINDOWS__IJKSDL_VOUT_WINDOWS_NATIVEWINDOW_H
#define IJKSDL_WINDOWS__IJKSDL_VOUT_WINDOWS_NATIVEWINDOW_H

#include "ijksdl/ijksdl_stdinc.h"
#include "ijksdl/ijksdl_vout.h"

typedef struct ANativeWindow   ANativeWindow;


SDL_Vout*  SDL_VoutWindows_CreateForANativeWindow();
void  SDL_VoutWindows_SetNativeWindow(SDL_Vout *vout,  HWND native_window);

void SDL_VoutWinNative_SetVideoDataCallback(void *arg, SDL_Vout *vout, int(*video_callback)(void *arg, SDL_VoutOverlay* overlay));


#endif