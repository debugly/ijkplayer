#ifndef IJKSDL_WIN__DIRECRTSOUND_H
#define IJKSDL_WIN__DIRECRTSOUND_H

#include <dsound.h>
#include "ijksdl/ijksdl_stdinc.h"
#include "ijksdl/ijksdl_audio.h"
#include "ijksdl/ijksdl_aout.h"

typedef struct SDL_Win_DirectSound{
	LPDIRECTSOUND sound;
	LPDIRECTSOUNDBUFFER mixbuf;
	LPDIRECTSOUNDCAPTURE capture;
	LPDIRECTSOUNDCAPTUREBUFFER capturebuf;
	int num_buffers;
	DWORD lastchunk;
	Uint8 *locked_buf;
}SDL_Win_DirectSound;


int SDL_Win_DSound_OpenDevice(SDL_Win_DirectSound *dsound, SDL_AudioSpec *sdl_spec);
void SDL_Win_DSound_CloseDevice(SDL_Win_DirectSound *dsound);
Uint8 * SDL_Win_DSound_GetDeviceBuf(SDL_Win_DirectSound *dsound, SDL_AudioSpec *sdl_spec);
void SDL_Win_DSound_PlayDevice(SDL_Win_DirectSound *dsound, SDL_AudioSpec *sdl_spec);
void SDL_Win_DSound_WaitDevice(SDL_Win_DirectSound *dsound, SDL_AudioSpec *sdl_spec);
void SDL_Win_DSound_SetVolume(SDL_Win_DirectSound *dsound, float left_volume, float right_volume);

#endif
