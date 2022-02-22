
#ifndef IJKSDL_WINDOWS__IJKSDL_VOUT_WINDOWS_SURFACE_H
#define IJKSDL_WINDOWS__IJKSDL_VOUT_WINDOWS_SURFACE_H

#include <windows.h>
#include "ijksdl/ijksdl_stdinc.h"
#include "ijksdl/ijksdl_vout.h"

SDL_Vout* SDL_VoutWindows_CreateForWindowsSurface();
void  SDL_VoutWindows_SetWindowsSurface(SDL_Vout *vout, HWND hwnd);

#endif