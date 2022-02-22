#include "ijksdl_aout_win_audio_directsound.h"

#include <stdbool.h>
#include <assert.h>
#include "ijksdl/ijksdl_inc_internal.h"
#include "ijksdl/ijksdl_thread.h"
#include "ijksdl/ijksdl_aout_internal.h"
#include "ijksdl/ijksdl_timer.h"

#include "win_directsound.h"


static SDL_Class g_dsound_class = {
	.name = "DirectSound",
};

typedef struct SDL_Aout_Opaque {
	SDL_cond *wakeup_cond;
	SDL_mutex *wakeup_mutex;

	SDL_AudioSpec spec;
	SDL_Win_DirectSound* atrack;
	uint8_t *buffer;
	int buffer_size;

	volatile bool need_flush;
	volatile bool pause_on;
	volatile bool abort_request;

	volatile bool need_set_volume;
	volatile float right_volume;
	volatile float left_volume;

	SDL_Thread *audio_tid;
	SDL_Thread _audio_tid;

	volatile float speed;
	volatile bool speed_changed;
} SDL_Aout_Opaque;

static int aout_thread_n(SDL_Aout *aout)
{
	SDL_Aout_Opaque *opaque = aout->opaque;
	SDL_Win_DirectSound *atrack = opaque->atrack;
	SDL_AudioCallback audio_cblk = opaque->spec.callback;
	void *userdata = opaque->spec.userdata;
	uint8_t *buffer = opaque->buffer;
	const Uint32 delay = ((opaque->spec.samples * 1000) / opaque->spec.freq);

	Uint8 *stream;
	const int stream_len = opaque->spec.size;

	assert(atrack);
	assert(buffer);

	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

	if (!opaque->abort_request && !opaque->pause_on)
		SDL_Win_DSound_PlayDevice(atrack, &opaque->spec);

	while (!opaque->abort_request) {
		stream = SDL_Win_DSound_GetDeviceBuf(atrack, &opaque->spec);
		if (stream == NULL){
			stream = buffer;
		}
		SDL_LockMutex(opaque->wakeup_mutex);
		if (!opaque->abort_request) {
			if (opaque->pause_on){
				memset(stream, 0, stream_len);
			} else {
				audio_cblk(userdata, stream, stream_len);
			}
		}
		if (opaque->need_set_volume) {
			opaque->need_set_volume = 0;
			SDL_Win_DSound_SetVolume(atrack, opaque->left_volume, opaque->right_volume);
		}
		SDL_UnlockMutex(opaque->wakeup_mutex);

		if (stream == buffer){
			SDL_Delay(delay);
		} else {
			SDL_Win_DSound_PlayDevice(atrack, &opaque->spec);
			SDL_Win_DSound_WaitDevice(atrack, &opaque->spec);
		}
	}

	SDL_Win_DSound_CloseDevice(atrack);
	return 0;
}

static int aout_thread(void *arg)
{
	SDL_Aout *aout = arg;
	return aout_thread_n(aout);
}

static int aout_open_audio_n(SDL_Aout *aout, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
	assert(desired);
	SDL_Aout_Opaque *opaque = aout->opaque;

	opaque->spec = *desired;
	opaque->atrack = (struct SDL_Win_DirectSound *)malloc(sizeof(SDL_Win_DirectSound));
	if (!opaque->atrack) {
		ALOGE("aout_open_audio_n: failed to new SDL_Win_DirectSound");
		return -1;
	}
	memset(opaque->atrack, 0, sizeof(SDL_Win_DirectSound));

	int ret = SDL_Win_DSound_OpenDevice(opaque->atrack, &opaque->spec);
	if (ret < 0) {
		free(opaque->atrack);
		opaque->atrack = NULL;
		return -1;
	}

	opaque->buffer_size = opaque->spec.size;
	opaque->buffer = malloc(opaque->buffer_size);
	if (!opaque->buffer) {
		ALOGE("aout_open_audio_n: failed to allocate buffer");
		free(opaque->atrack);
		opaque->atrack = NULL;
		return -1;
	}

	if (obtained) {
		obtained->freq = desired->freq;
		obtained->format = desired->format;
		obtained->channels = desired->channels;
		obtained->silence = desired->silence;
		obtained->samples = desired->samples;
		obtained->size = desired->size;
		obtained->callback = desired->callback;
		SDLTRACE("audio target format fmt:0x%x, channel:0x%x", (int)obtained->format, (int)obtained->channels);
	}

	opaque->pause_on = 1;
	opaque->abort_request = 0;
	opaque->audio_tid = SDL_CreateThreadEx(&opaque->_audio_tid, aout_thread, aout, "ff_aout_win");
	if (!opaque->audio_tid) {
		ALOGE("aout_open_audio_n: failed to create audio thread\n");
		free(opaque->atrack);opaque->atrack = NULL;
		free(opaque->buffer); opaque->buffer = NULL;
		return -1;
	}

	return 0;
}

static int aout_open_audio(SDL_Aout *aout, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
	return aout_open_audio_n(aout, desired, obtained);
}

static void aout_pause_audio(SDL_Aout *aout, int pause_on)
{
	SDL_Aout_Opaque *opaque = aout->opaque;

	SDL_LockMutex(opaque->wakeup_mutex);
	SDLTRACE("aout_pause_audio(%d)\n", pause_on);
	opaque->pause_on = pause_on;
	SDL_UnlockMutex(opaque->wakeup_mutex);
}

static void aout_set_volume(SDL_Aout *aout, float left_volume, float right_volume)
{
	SDL_Aout_Opaque *opaque = aout->opaque;
	SDL_LockMutex(opaque->wakeup_mutex);
	SDLTRACE("aout_flush_audio()\n");
	opaque->left_volume = left_volume;
	opaque->need_set_volume = 1;
	SDL_UnlockMutex(opaque->wakeup_mutex);
}

static void aout_close_audio(SDL_Aout *aout)
{
	SDL_Aout_Opaque *opaque = aout->opaque;

	SDL_LockMutex(opaque->wakeup_mutex);
	opaque->abort_request = true;
	SDL_UnlockMutex(opaque->wakeup_mutex);

	if (opaque->audio_tid){
		SDL_WaitThread(opaque->audio_tid, NULL);
	}

	opaque->audio_tid = NULL;
}

static void aout_free_l(SDL_Aout *aout)
{
	if (!aout)
		return;

	aout_close_audio(aout);

	SDL_Aout_Opaque *opaque = aout->opaque;
	if (opaque) {
		if (opaque->buffer){
			free(opaque->buffer);
			opaque->buffer = NULL;
			opaque->buffer_size = 0;
		}

		if (opaque->atrack){
			free(opaque->atrack);
			opaque->atrack = NULL;
		}

		SDL_DestroyCond(opaque->wakeup_cond);
		SDL_DestroyMutex(opaque->wakeup_mutex);
	}

	SDL_Aout_FreeInternal(aout);
}

static void aout_get_spec(SDL_Aout *aout)
{
	SDL_Aout_Opaque *opaque;
	if (!aout)
		return;

	opaque = aout->opaque;
	SDL_AudioSpec* spec = &(opaque->spec);
	return spec;
}

static void func_set_playback_rate(SDL_Aout *aout, float speed)
{
	SDL_Aout_Opaque *opaque;
	if (!aout)
		return;

	opaque = aout->opaque;
	SDL_LockMutex(opaque->wakeup_mutex);
	opaque->speed = speed;
	opaque->speed_changed = 1;
	SDL_UnlockMutex(opaque->wakeup_mutex);
}

SDL_Aout *SDL_AoutWin_CreateForAudio()
{
	SDL_Aout_Opaque *opaque;
	SDL_Aout *aout = SDL_Aout_CreateInternal(sizeof(SDL_Aout_Opaque));
	if (!aout)
		return NULL;

	opaque = aout->opaque;
	memset(opaque, 0, sizeof(SDL_Aout_Opaque));
	opaque->wakeup_cond = SDL_CreateCond();
	opaque->wakeup_mutex = SDL_CreateMutex();
	opaque->speed = 1.0f;

	aout->opaque_class = &g_dsound_class;
	aout->free_l = aout_free_l;
	aout->open_audio = aout_open_audio;
	aout->pause_audio = aout_pause_audio;
	aout->set_volume = aout_set_volume;
	aout->close_audio = aout_close_audio;
	aout->func_set_playback_rate = func_set_playback_rate;
	aout->get_spec = aout_get_spec;

	return aout;
}

