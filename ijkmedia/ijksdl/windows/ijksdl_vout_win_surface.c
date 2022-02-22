#include "ijksdl_vout_win_surface.h"


#include "ijksdl/ijksdl_inc_internal.h"
#include "ijksdl_vout_win_nativewindows.h"

SDL_Vout *SDL_VoutWindows_CreateForWindowsSurface()
{
    return SDL_VoutWindows_CreateForANativeWindow();
}

void SDL_VoutWindows_SetWindowsSurface(SDL_Vout *vout, HWND hwnd)
{
    SDL_VoutWindows_SetNativeWindow(vout, hwnd);
}