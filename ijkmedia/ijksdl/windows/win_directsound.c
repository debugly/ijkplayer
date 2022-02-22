#include "win_directsound.h"
#include "ijksdl/ijksdl_inc_internal.h"
#include "ijksdl/ijksdl_timer.h"
#include <math.h>

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif


static int set_dsound_error(const char *function, int code)
{
	switch (code) {
	case E_NOINTERFACE:
		ALOGE("%s: Unsupported interface -- Is DirectX 8.0 or later installed", function);
		break;
	case DSERR_ALLOCATED:
		ALOGE("%s: Audio device in use", function);
		break;
	case DSERR_BADFORMAT:
		ALOGE("%s: Unsupported audio format", function);
		break;
	case DSERR_BUFFERLOST:
		ALOGE("%s: Mixing buffer was lost", function);
		break;
	case DSERR_CONTROLUNAVAIL:
		ALOGE("%s: Control requested is not available", function);
		break;
	case DSERR_INVALIDCALL:
		ALOGE("%s: Invalid call for the current state", function);
		break;
	case DSERR_INVALIDPARAM:
		ALOGE("%s: Invalid parameter", function);
		break;
	case DSERR_NODRIVER:
		ALOGE("%s: No audio device found", function);
		break;
	case DSERR_OUTOFMEMORY:
		ALOGE("%s: Out of memory", function);
		break;
	case DSERR_PRIOLEVELNEEDED:
		ALOGE("%s: Caller doesn't have priority", function);
		break;
	case DSERR_UNSUPPORTED:
		ALOGE("%s: Function not supported", function);
		break;
	default:
		ALOGE("%s: Unknown DirectSound error: 0x%x", function, code);
		break;
	}
	return 0;
}

void SDL_Win_DSound_WaitDevice(SDL_Win_DirectSound *dsound, SDL_AudioSpec *sdl_spec)
{
	DWORD status = 0;
	DWORD cursor = 0;
	DWORD junk = 0;
	HRESULT result = DS_OK;

	/** 
	* Semi-busy wait, since we have no way of getting play notification
	* on a primary mixing buffer located in hardware (DirectX 5.0)
	**/
	result = IDirectSoundBuffer_GetCurrentPosition(dsound->mixbuf, &junk, &cursor);
	if (result != DS_OK) {
		if (result == DSERR_BUFFERLOST) {
			IDirectSoundBuffer_Restore(dsound->mixbuf);
		}
		return;
	}

	while ((cursor / sdl_spec->size) == dsound->lastchunk) {

		// FIXME: find out how much time is left and sleep that long
		SDL_Delay(1);

		// Try to restore a lost sound buffer
		IDirectSoundBuffer_GetStatus(dsound->mixbuf, &status);
		if ((status & DSBSTATUS_BUFFERLOST)) {
			IDirectSoundBuffer_Restore(dsound->mixbuf);
			IDirectSoundBuffer_GetStatus(dsound->mixbuf, &status);
			if ((status & DSBSTATUS_BUFFERLOST)) {
				break;
			}
		}
		if (!(status & DSBSTATUS_PLAYING)) {
			result = IDirectSoundBuffer_Play(dsound->mixbuf, 0, 0, DSBPLAY_LOOPING);
			if (result == DS_OK) {
				continue;
			}
			return;
		}

		// Find out where we are playing
		result = IDirectSoundBuffer_GetCurrentPosition(dsound->mixbuf, &junk, &cursor);
		if (result != DS_OK) {
			set_dsound_error("DirectSound GetCurrentPosition", result);
			return;
		}
	}
}

void SDL_Win_DSound_SetVolume(SDL_Win_DirectSound *dsound, float left_volume, float right_volume)
{
	HRESULT result = DS_OK;
	double decibels = 50 * log10((double)left_volume / 100.0);
	long ds_volume = (long)(decibels * 100.0);
	if (ds_volume <= DSBVOLUME_MIN){
		ds_volume = DSBVOLUME_MIN;
	}
	result = IDirectSoundBuffer_SetVolume(dsound->mixbuf, ds_volume);
	if (result != DS_OK) {
		ALOGE("directsound set volume failed");
	}
	return;
}

void SDL_Win_DSound_PlayDevice(SDL_Win_DirectSound *dsound, SDL_AudioSpec *sdl_spec)
{
	// Unlock the buffer, allowing it to play
	if (dsound->locked_buf) {
		IDirectSoundBuffer_Unlock(dsound->mixbuf, dsound->locked_buf, sdl_spec->size, NULL, 0);
	}
}

Uint8 * SDL_Win_DSound_GetDeviceBuf(SDL_Win_DirectSound *dsound, SDL_AudioSpec *sdl_spec)
{
	DWORD cursor = 0;
	DWORD junk = 0;
	HRESULT result = DS_OK;
	DWORD rawlen = 0;

	// Figure out which blocks to fill next
	dsound->locked_buf = NULL;
	result = IDirectSoundBuffer_GetCurrentPosition(dsound->mixbuf, &junk, &cursor);
	if (result == DSERR_BUFFERLOST) {
		IDirectSoundBuffer_Restore(dsound->mixbuf);
		result = IDirectSoundBuffer_GetCurrentPosition(dsound->mixbuf, &junk, &cursor);
	}
	if (result != DS_OK) {
		set_dsound_error("DirectSound GetCurrentPosition", result);
		return (NULL);
	}
	cursor /= sdl_spec->size;
	dsound->lastchunk = cursor;
	cursor = (cursor + 1) % dsound->num_buffers;
	cursor *= sdl_spec->size;

	// Lock the audio buffer
	result = IDirectSoundBuffer_Lock(dsound->mixbuf, cursor, sdl_spec->size, (LPVOID *)& dsound->locked_buf, &rawlen, NULL, &junk, 0);
	if (result == DSERR_BUFFERLOST) {
		IDirectSoundBuffer_Restore(dsound->mixbuf);
		result = IDirectSoundBuffer_Lock(dsound->mixbuf, cursor, sdl_spec->size, (LPVOID *)& dsound->locked_buf, &rawlen, NULL, &junk, 0);
	}
	if (result != DS_OK) {
		set_dsound_error("DirectSound Lock", result);
		return (NULL);
	}
	return (dsound->locked_buf);
}

void SDL_Win_DSound_CloseDevice(SDL_Win_DirectSound *dsound)
{
	if (dsound->mixbuf != NULL) {
		IDirectSoundBuffer_Stop(dsound->mixbuf);
		IDirectSoundBuffer_Release(dsound->mixbuf);
	}
	if (dsound->sound != NULL) {
		IDirectSound_Release(dsound->sound);
	}
	if (dsound->capturebuf != NULL) {
		IDirectSoundCaptureBuffer_Stop(dsound->capturebuf);
		IDirectSoundCaptureBuffer_Release(dsound->capturebuf);
	}
	if (dsound->capture != NULL) {
		IDirectSoundCapture_Release(dsound->capture);
	}
}

/** 
* This function tries to create a secondary audio buffer, and returns the
* number of audio chunks available in the created buffer. This is for
* playback devices, not capture.
**/
static int create_secondary(SDL_Win_DirectSound *dsound, const DWORD bufsize, WAVEFORMATEX *wfmt, Uint8 silence)
{
	LPDIRECTSOUND sndObj = dsound->sound;
	LPDIRECTSOUNDBUFFER *sndbuf = &dsound->mixbuf;
	HRESULT result = DS_OK;
	DSBUFFERDESC format;
	LPVOID pvAudioPtr1, pvAudioPtr2;
	DWORD dwAudioBytes1, dwAudioBytes2;

	// Try to create the secondary buffer
	memset(&format, 0, sizeof(DSBUFFERDESC));
	format.dwSize = sizeof(format);
	format.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
	format.dwFlags |= DSBCAPS_GLOBALFOCUS;
	format.dwFlags |= DSBCAPS_CTRLVOLUME;
	format.dwBufferBytes = bufsize;
	format.lpwfxFormat = wfmt;
	result = IDirectSound_CreateSoundBuffer(sndObj, &format, sndbuf, NULL);
	if (result != DS_OK) {
		return set_dsound_error("DirectSound CreateSoundBuffer", result);
	}
	IDirectSoundBuffer_SetFormat(*sndbuf, wfmt);

	// Silence the initial audio buffer
	result = IDirectSoundBuffer_Lock(*sndbuf, 0, format.dwBufferBytes,
		(LPVOID *)& pvAudioPtr1, &dwAudioBytes1,
		(LPVOID *)& pvAudioPtr2, &dwAudioBytes2,
		DSBLOCK_ENTIREBUFFER);
	if (result == DS_OK) {
		memset(pvAudioPtr1, silence, dwAudioBytes1);
		IDirectSoundBuffer_Unlock(*sndbuf,(LPVOID)pvAudioPtr1, dwAudioBytes1,(LPVOID)pvAudioPtr2, dwAudioBytes2);
	}

	// We're ready to go
	return 0;
}

int SDL_Win_DSound_OpenDevice(SDL_Win_DirectSound *dsound, SDL_AudioSpec *sdl_spec)
{
	DWORD bufsize;
	HRESULT result;
	const DWORD numchunks = 8;

	// Open the audio device
	{
		result = DirectSoundCreate8(NULL, &dsound->sound, NULL);
		if (result != DS_OK) {
			set_dsound_error("DirectSoundCreate8", result);
			return -1;
		}
		result = IDirectSound_SetCooperativeLevel(dsound->sound,GetDesktopWindow(),DSSCL_NORMAL);
		if (result != DS_OK) {
			set_dsound_error("DirectSound SetCooperativeLevel", result);
			return -1;
		}
	}
	
	// Update the fragment size as size in bytes and create audio buffer
	{
		SDL_CalculateAudioSpec(sdl_spec);
		bufsize = numchunks * sdl_spec->size;
		if ((bufsize < DSBSIZE_MIN) || (bufsize > DSBSIZE_MAX)) {
			ALOGE("Sound buffer size must be between %d and %d",(DSBSIZE_MIN < numchunks) ? 1 : DSBSIZE_MIN / numchunks,DSBSIZE_MAX / numchunks);
			return -1;
		}

		int rc;
		WAVEFORMATEX wfmt;
		memset(&wfmt,0,sizeof(WAVEFORMATEX));
		if (SDL_AUDIO_ISFLOAT(sdl_spec->format)) {
			wfmt.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		}
		else {
			wfmt.wFormatTag = WAVE_FORMAT_PCM;
		}

		wfmt.wBitsPerSample = SDL_AUDIO_BITSIZE(sdl_spec->format);
		wfmt.nChannels = sdl_spec->channels;
		wfmt.nSamplesPerSec = sdl_spec->freq;
		wfmt.nBlockAlign = wfmt.nChannels * (wfmt.wBitsPerSample / 8);
		wfmt.nAvgBytesPerSec = wfmt.nSamplesPerSec * wfmt.nBlockAlign;

		rc = create_secondary(dsound, bufsize, &wfmt, sdl_spec->silence);
		if (rc == 0) {
			dsound->num_buffers = numchunks;
		}
	}

	//Playback buffers will auto-start playing in DSOUND_WaitDevice
	return 0;          
}
