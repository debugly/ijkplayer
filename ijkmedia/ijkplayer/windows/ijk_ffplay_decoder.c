//
//  ijk_ffplay_decoder.c
//  IJKMediaPlayer
//
//  Created by chao on 2017/6/12.
//  Copyright © 2017 detu. All rights reserved.
//

#include "ijk_ffplay_decoder.h"
#include "ijkplayer/ijkplayer_internal.h"
#include "ijksdl/ijksdl_vout_internal.h"
#include "pipeline/ffpipeline_win.h"
#include "ijksdl/windows/ijksdl_vout_win_ffmpeg.h"
#include "ijksdl/windows/ijksdl_vout_win_nativewindows.h"
//#include "ijksdl_vout_win_surface.h"
#include "ijkplayer/ff_ffmsg_queue.h"

#define container_of(ptr, type, member) ((type *)((char *)ptr - offsetof(type, member)))

struct IjkFfplayDecoder{
	IjkMediaPlayer *ijk_media_player;

	void *opaque;
	IjkFfplayDecoderCallBack *ijk_ffplayer_deocdecallback;

	IjkVideoFrame *current_frame;
};

typedef void(*msg_call_back)(void* opaque, IjkMsgState ijk_msgint, int arg1, int arg2);
static msg_call_back s_user_msg_callback = NULL;

static void message_loop_n(IjkMediaPlayer *mp)
{
	void* opaque = (void*)ijkmp_get_weak_thiz(mp);

	while (1) {
		AVMessage msg;
		int retval = ijkmp_get_msg(mp, &msg, 1);
		if (retval < 0)
			break;

		assert(retval > 0);

		switch (msg.what) {
		case FFP_MSG_VIDEO_DECODE_FPS:
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_VIDEO_DECODE_FPS, msg.arg1, 0);
			break;
		case FFP_MSG_VIDEO_GOP_SIZE:
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_VIDEO_GOP_SIZE, msg.arg1, 0);
			break;
		case FFP_MSG_FLUSH:
			MPTRACE("FFP_MSG_FLUSH:\n");
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_FLUSH, 0, 0);
			break;
		case FFP_MSG_ERROR:
			MPTRACE("FFP_MSG_ERROR: %d\n", msg.arg1);
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_ERROR, msg.arg1, 0);
			break;
		case FFP_MSG_PREPARED:
			MPTRACE("FFP_MSG_PREPARED.\n");
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_PREPARED, 0, 0);
			break;
		case FFP_MSG_COMPLETED:
			MPTRACE("FFP_MSG_COMPLETED.\n");
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_COMPLETED, 0, 0);
			break;
		case FFP_MSG_VIDEO_SIZE_CHANGED:
			MPTRACE("FFP_MSG_VIDEO_SIZE_CHANGED: %d, %d\n", msg.arg1, msg.arg2);
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_VIDEO_SIZE_CHANGED, msg.arg1, msg.arg2);
			break;
		case FFP_MSG_SAR_CHANGED:
			MPTRACE("FFP_MSG_SAR_CHANGED: %d, %d\n", msg.arg1, msg.arg2);
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_SAR_CHANGED, msg.arg1, msg.arg2);
			break;
		case FFP_MSG_VIDEO_RENDERING_START:
			MPTRACE("FFP_MSG_VIDEO_RENDERING_START.\n");
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_VIDEO_RENDERING_START, 0, 0);
			break;
		case FFP_MSG_AUDIO_RENDERING_START:
			MPTRACE("FFP_MSG_AUDIO_RENDERING_START.\n");
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_AUDIO_RENDERING_START, 0, 0);
			break;
		case FFP_MSG_VIDEO_ROTATION_CHANGED:
			MPTRACE("FFP_MSG_VIDEO_ROTATION_CHANGED: %d\n", msg.arg1);
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_VIDEO_ROTATION_CHANGED, msg.arg1, 0);
			break;
		case FFP_MSG_BUFFERING_START:
			MPTRACE("FFP_MSG_BUFFERING_START.\n");
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_BUFFERING_START, 0, 0);
			break;
		case FFP_MSG_BUFFERING_END:
			MPTRACE("FFP_MSG_BUFFERING_END:\n");
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_BUFFERING_END, 0, 0);
			break;
		case FFP_MSG_BUFFERING_UPDATE:
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_BUFFERING_UPDATE, msg.arg1, msg.arg2);
			break;
		case FFP_MSG_BUFFERING_BYTES_UPDATE:
			//if (s_user_msg_callback)
			//s_user_msg_callback(opaque, IJK_MSG_BUFFERING_BYTES_UPDATE, msg.arg1, msg.arg2);
			break;
		case FFP_MSG_BUFFERING_TIME_UPDATE:
			//if (s_user_msg_callback)
			//s_user_msg_callback(opaque, IJK_MSG_BUFFERING_TIME_UPDATE, msg.arg1, msg.arg2);
			break;
		case FFP_MSG_SEEK_COMPLETE:
			MPTRACE("FFP_MSG_SEEK_COMPLETE:\n");
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_SEEK_COMPLETE, 0, 0);
			break;
		case FFP_MSG_PLAYBACK_STATE_CHANGED:
			if (s_user_msg_callback)
				s_user_msg_callback(opaque, IJK_MSG_PLAYBACK_STATE_CHANGED, 0, 0);
			break;
		default:
			//ALOGE("unknown FFP_MSG_xxx(%d)\n", msg.what);
			break;
		}
	}
}

static int message_loop(void *arg)
{
	IjkMediaPlayer *mp = (IjkMediaPlayer*)arg;

	message_loop_n(mp);

	ijkmp_dec_ref_p(&mp);

	ALOGI("message_loop exit.\n");
	return 0;
}

static int video_callback(void *arg, SDL_VoutOverlay* overlay)
{
	IjkFfplayDecoder *play_decoder = (IjkFfplayDecoder*)arg;

	switch (overlay->format) {
	case SDL_FCC_YV12:
	case SDL_FCC_I420:
		play_decoder->current_frame->format = PIX_FMT_YUV420P;
		break;
	case SDL_FCC_NV12:
		play_decoder->current_frame->format = PIX_FMT_NV12;
		break;
	case SDL_FCC_I444P10LE:
		play_decoder->current_frame->format = PIX_FMT_YUV444P10LE;
		break;
	case SDL_FCC_RV24:
		play_decoder->current_frame->format = PIX_FMT_RGB24;
		break;
	default:
		play_decoder->current_frame->format = PIX_FMT_YUV420P;
	}
	play_decoder->current_frame->w = overlay->w;
	play_decoder->current_frame->h = overlay->h;
	play_decoder->current_frame->planes = overlay->planes;

	for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
		play_decoder->current_frame->data[i] = overlay->pixels[i];
		play_decoder->current_frame->linesize[i] = overlay->pitches[i];
	}

	if (play_decoder->ijk_ffplayer_deocdecallback){
		play_decoder->ijk_ffplayer_deocdecallback->func_get_frame(play_decoder->opaque, play_decoder->current_frame);
	}

	return 0;
}

static ijk_inject_callback s_decoder_inject_callback;
int decoder_inject_callback(void *opaque, int type, void *data, size_t data_size)
{
	if (s_decoder_inject_callback)
		return s_decoder_inject_callback(opaque, type, data, data_size);
	return 0;
}

int ijkFfplayDecoder_init(void)
{
	ijkmp_global_init();
	ijkmp_global_set_inject_callback(decoder_inject_callback);

	return 0;
}

int ijkFfplayDecoder_uninit(void)
{
	ijkmp_global_uninit();

	return 0;
}

IjkFfplayDecoder *ijkFfplayDecoder_create(void)
{
	IjkMediaPlayer *mp = ijkmp_create(message_loop);
	if (!mp)
		goto fail;

	//mp->ffplayer->vout = SDL_VoutWin_CreateForWindows();
	mp->ffplayer->vout = SDL_VoutWindows_CreateForANativeWindow();
	if (!mp->ffplayer->vout)
		goto fail;
	ALOGI("create vout success.\n");

	mp->ffplayer->pipeline = ffpipeline_create_from_win(mp->ffplayer);
	if (!mp->ffplayer->pipeline)
		goto fail;
	ALOGI("create pipeline success.\n");

	IjkFfplayDecoder *ijk_ffplay_decoder = (IjkFfplayDecoder *)malloc(sizeof(IjkFfplayDecoder));
	memset(ijk_ffplay_decoder, 0, sizeof(IjkFfplayDecoder));
	ijk_ffplay_decoder->ijk_media_player = mp;
	ijk_ffplay_decoder->current_frame = (IjkVideoFrame *)malloc(sizeof(IjkVideoFrame));
	ijk_ffplay_decoder->ijk_ffplayer_deocdecallback = (IjkFfplayDecoderCallBack *)malloc(sizeof(IjkFfplayDecoderCallBack));
	memset(ijk_ffplay_decoder->current_frame, 0, sizeof(IjkVideoFrame));
	memset(ijk_ffplay_decoder->ijk_ffplayer_deocdecallback, 0, sizeof(IjkFfplayDecoderCallBack));

	return ijk_ffplay_decoder;

fail:
	ijkmp_dec_ref_p(&mp);
	return NULL;
}

int ijkFfplayDecoder_setLogLevel(IJKLogLevel log_level)
{
	ijkmp_global_set_log_level(log_level);

	return 0;
}

int ijkFfplayDecoder_setLogCallback(void(*callback)(void*, int, const char*, va_list))
{
	ijkmp_global_set_log_callback(callback);

	return 0;
}

int ijkFfplayDecoder_setDecoderCallBack(IjkFfplayDecoder* decoder, void* opaque, IjkFfplayDecoderCallBack* callback)
{
	if (!decoder || !decoder->ijk_media_player){
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	//save opaque, used in callback func
	ijkmp_set_weak_thiz(decoder->ijk_media_player, opaque);
	decoder->opaque = opaque;

	decoder->ijk_ffplayer_deocdecallback->func_get_frame = callback->func_get_frame;
	decoder->ijk_ffplayer_deocdecallback->func_state_change = callback->func_state_change;
	if (callback){
		s_user_msg_callback = callback->func_state_change;
	} else {
		s_user_msg_callback = NULL;
	}


	//set callback to ijksdl_vout_win_ffmpeg
	SDL_VoutWinNative_SetVideoDataCallback((void *)decoder, decoder->ijk_media_player->ffplayer->vout, video_callback);

	return 0;
}

int ijkFfplayDecoder_setDataSource(IjkFfplayDecoder* decoder, const char* file_absolute_path)
{
	if (!decoder || !decoder->ijk_media_player){
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	int ret = ijkmp_set_data_source(decoder->ijk_media_player, file_absolute_path);
	if (ret == 0){
		ALOGI("setDataSource success: path %s.\n", file_absolute_path);
	}else {
		ALOGE("setDataSource failed: path %s.\n", file_absolute_path);
		return -1;
	}

	return 0;
}

int ijkFfplayDecoder_prepare(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player){
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	int ret = ijkmp_prepare_async(decoder->ijk_media_player);
	if (ret == 0){
		ALOGI("ijkFfplayDecoder_prepare success.\n");
	} else {
		ALOGE("ijkFfplayDecoder_prepare failed.\n");
		return -1;
	}

	return 0;
}

int ijkFfplayDecoder_start(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player){
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	int ret = ijkmp_start(decoder->ijk_media_player);
	if (ret == 0){
		ALOGI("ijkFfplayDecoder_start success.\n");
	} else {
		ALOGE("ijkFfplayDecoder_start failed.\n");
		return -1;
	}

	return 0;
}

int ijkFfplayDecoder_pause(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	int ret = ijkmp_pause(decoder->ijk_media_player);
	if (ret == 0){
		ALOGI("ijkFfplayDecoder_pause success.\n");
	} else {
		ALOGE("ijkFfplayDecoder_pause failed.\n");
		return -1;
	}

	return 0;
}

int ijkFfplayDecoder_stop(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	int ret = ijkmp_stop(decoder->ijk_media_player);
	if (ret == 0){
		ALOGI("ijkFfplayDecoder_stop success.\n");
	}
	else {
		ALOGE("ijkFfplayDecoder_stop failed.\n");
		return -1;
	}

	ijkmp_shutdown(decoder->ijk_media_player);

	return 0;
}

int ijkFfplayDecoder_seekTo(IjkFfplayDecoder* decoder, long msec)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	int ret = ijkmp_seek_to(decoder->ijk_media_player, msec);
	if (ret == 0){
		ALOGI("ijkFfplayDecoder_seekTo success.\n");
	} else {
		ALOGE("ijkFfplayDecoder_seekTo failed.\n");
		return -1;
	}

	return 0;
}

bool ijkFfplayDecoder_isPlaying(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return false;
	}

	bool ret = ijkmp_is_playing(decoder->ijk_media_player);
	ALOGI("ijkFfplayDecoder is %s.\n", ret ? "playing" : "not playing");
	return ret;

}

long ijkFfplayDecoder_getCurrentPosition(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	long ret = ijkmp_get_current_position(decoder->ijk_media_player);
	ALOGI("current position %d\n", ret);
	return ret;
}

long ijkFfplayDecoder_getDuration(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	long ret = ijkmp_get_duration(decoder->ijk_media_player);
	ALOGI("duration long: %d\n", ret);
	return ret;
}

int ijkFfplayDecoder_release(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	ijkmp_dec_ref_p(&(decoder->ijk_media_player));

	if (decoder->current_frame){
		free(decoder->current_frame); 
	}
	if (decoder->ijk_ffplayer_deocdecallback){
		free(decoder->ijk_ffplayer_deocdecallback);
	}
	if (decoder){
		free(decoder);
	}

	return 0;
}

int ijkFfplayDecoder_setVolume(IjkFfplayDecoder* decoder, float volume)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	pthread_mutex_lock(&decoder->ijk_media_player->mutex);
	if (decoder->ijk_media_player && decoder->ijk_media_player->ffplayer && decoder->ijk_media_player->ffplayer->pipeline) {
		ffpipeline_win_set_volume(decoder->ijk_media_player->ffplayer->pipeline, volume, volume);
	}
	pthread_mutex_unlock(&decoder->ijk_media_player->mutex);
	ALOGI("set volume: %f.\n", volume);
	
	return 0;
}

float ijkFfplayDecoder_getVolume(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	float volume = -1;
	pthread_mutex_lock(&decoder->ijk_media_player->mutex);
	if (decoder->ijk_media_player && decoder->ijk_media_player->ffplayer && decoder->ijk_media_player->ffplayer->pipeline) {
		volume = ffpipeline_win_get_volume(decoder->ijk_media_player->ffplayer->pipeline);
	}
	pthread_mutex_unlock(&decoder->ijk_media_player->mutex);
	ALOGI("get volume: %f.\n", volume);

	return volume;
}

int ijkFfplayDecoder_setStreamSelected(IjkFfplayDecoder* decoder, int stream, int selected)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	ijkmp_set_stream_selected(decoder->ijk_media_player, stream, selected);

	return 0;
}

int ijkFfplayDecoder_setOptionLongValue(IjkFfplayDecoder* decoder, int opt_category, const char* key, long value)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	ijkmp_set_option_int(decoder->ijk_media_player, opt_category, key, value);

	return 0;
}

int ijkFfplayDecoder_setOptionStringValue(IjkFfplayDecoder* decoder, int opt_category, const char* key, const char* value)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	ijkmp_set_option(decoder->ijk_media_player, opt_category, key, value);

	return 0;
}

int ijkFfplayDecoder_getVideoCodecInfo(IjkFfplayDecoder* decoder, char **codec_info)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	pthread_mutex_lock(&decoder->ijk_media_player->mutex);
	int ret = ffp_get_video_codec_info(decoder->ijk_media_player->ffplayer, codec_info);
	pthread_mutex_unlock(&decoder->ijk_media_player->mutex);
	
	return 0;
}

int ijkFfplayDecoder_getAudioCodecInfo(IjkFfplayDecoder* decoder, char **codec_info)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	pthread_mutex_lock(&decoder->ijk_media_player->mutex);
	int ret = ffp_get_audio_codec_info(decoder->ijk_media_player->ffplayer, codec_info);
	pthread_mutex_unlock(&decoder->ijk_media_player->mutex);
	
	return 0;
}

long ijkFfplayDecoder_getPropertyLong(IjkFfplayDecoder* decoder, int id, long default_value)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	long ret = ijkmp_get_property_int64(decoder->ijk_media_player, id, default_value);
	return ret;
}

float ijkFfplayDecoder_getPropertyFloat(IjkFfplayDecoder* decoder, int id, float default_value)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	float ret = ijkmp_get_property_float(decoder->ijk_media_player, id, default_value);
	return ret;
}

static char* fillMetaInternal(IjkMediaMeta *meta, const char *key, const char *default_value)
{
	char *value = ijkmeta_get_string_l(meta, key);
	if (value == NULL){
		value = default_value;
	} else {
		ALOGI("meta: %s\n", value);
	}

	return value;
}

int ijkFfplayDecoder_setPlaybackRate(IjkFfplayDecoder *decoder, float rate)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	ijkmp_set_playback_rate(decoder->ijk_media_player, rate);

	return 0;
}

int ijkFfplayDecoder_setSubtitleExtraDelay(IjkFfplayDecoder* decoder, float delay)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	ffp_set_subtitle_extra_delay(decoder->ijk_media_player->ffplayer, delay);

	return 0;
}

float ijkFfplayDecoder_getSubtitleExtraDelay(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	return ffp_get_subtitle_extra_delay(decoder->ijk_media_player->ffplayer);
}

int ijkFfplayDecoder_setExternalSubtitle(IjkFfplayDecoder* decoder, const char* file_name)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	return ijkmp_addOnly_external_subtitle(decoder->ijk_media_player->ffplayer, file_name);
}

/*only load ex-subtitle, not apply*/
int ijkFfplayDecoder_loadExternalSubtitle(IjkFfplayDecoder* decoder, const char* file_name)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	return ijkmp_add_active_external_subtitle(decoder->ijk_media_player->ffplayer, file_name);
}

int ijkFfplayDecoder_getMediaMeta(IjkFfplayDecoder* decoder, IjkMetadata* metadata)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}
	memset(metadata, 0, sizeof(IjkMetadata));

	bool is_locked = false;
	char *media_info = NULL;
	IjkMediaMeta *meta = NULL;
	meta = ijkmp_get_meta_l(decoder->ijk_media_player);
	if (!meta){
		ALOGE("IjkMediaPlayer get meta error.\n");
		return -1;
	}

	ijkmeta_lock(meta);
	is_locked = true;

	media_info = fillMetaInternal(meta, IJKM_KEY_DURATION_US, NULL);
	if (media_info){
		metadata->duration_ms = atol(media_info) / 1000;
	}

	media_info = fillMetaInternal(meta, IJKM_KEY_FORMAT, NULL);
	if (media_info){
		memcpy(metadata->format, media_info, strlen(media_info));
	}

	media_info = fillMetaInternal(meta, IJKM_KEY_START_US, NULL);
	if (media_info){
		metadata->start_us = atol(media_info) / 1000;
	}

	media_info = fillMetaInternal(meta, IJKM_KEY_BITRATE, NULL);
	if (media_info){
		metadata->bitrate = atoi(media_info);
	}

	int video_stream, audio_stream, subtitle_stream;
	media_info = fillMetaInternal(meta, IJKM_KEY_VIDEO_STREAM, NULL);
	if (media_info) {
		video_stream = atoi(media_info);
	}

	media_info = fillMetaInternal(meta, IJKM_KEY_AUDIO_STREAM, NULL);
	if (media_info) {
		audio_stream = atoi(media_info);
	}

	media_info = fillMetaInternal(meta, IJKM_KEY_TIMEDTEXT_STREAM, NULL);
	if (media_info) {
		subtitle_stream = atoi(media_info);
	}

	size_t count = ijkmeta_get_children_count_l(meta);
	for (size_t i = 0; i < count; ++i) {
		IjkMediaMeta *streamRawMeta = ijkmeta_get_child_l(meta, i);
		if (streamRawMeta) {
			const char *type = ijkmeta_get_string_l(streamRawMeta, IJKM_KEY_TYPE);
			if (type) {
				if (0 == strcmp(type, IJKM_VAL_TYPE__VIDEO)) {
					struct IjkVideoMetadata* video_meta = (struct IjkVideoMetadata*)calloc(1, sizeof(struct IjkVideoMetadata));
					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_CODEC_NAME, NULL);
					if (media_info){
						memcpy(video_meta->stream_meta.codec_name, media_info, strlen(media_info));
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_CODEC_LONG_NAME, NULL);
					if (media_info){
						memcpy(video_meta->stream_meta.codec_long_name, media_info, strlen(media_info));
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_BITRATE, NULL);
					if (media_info){
						video_meta->stream_meta.stream_bitrate = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_STREAM_IDX, NULL);
					if (media_info) {
						video_meta->stream_meta.stream_bitrate = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_WIDTH, NULL);
					if (media_info){
						video_meta->width = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_HEIGHT, NULL);
					if (media_info){
						video_meta->height = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_FPS_NUM, NULL);
					if (media_info){
						video_meta->video_fps_num = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_FPS_DEN, NULL);
					if (media_info){
						video_meta->video_fps_den = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_TBR_NUM, NULL);
					if (media_info){
						video_meta->video_tbr_num = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_TBR_DEN, NULL);
					if (media_info){
						video_meta->video_tbr_den = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_SAR_DEN, NULL);
					if (media_info) {
						video_meta->video_sar_den = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_SAR_NUM, NULL);
					if (media_info) {
						video_meta->video_sar_num = atoi(media_info);
					}

					struct IjkVideoMetadata* tail = metadata->video_stream_list;
					if (!tail) {
						metadata->video_stream_list = video_meta;
					} else {
						struct IjkVideoMetadata* next = tail->next;
						while (next) {
							tail = next;
							next = next->next;
						}
						tail->next = video_meta;
					}

					if (video_stream == i) {
						metadata->video_stream = i;
					}
				}
				else if (0 == strcmp(type, IJKM_VAL_TYPE__AUDIO)) {
					struct IjkAudioMetadata* audio_meta = (struct IjkAudioMetadata*)calloc(1, sizeof(struct IjkAudioMetadata));

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_CODEC_NAME, NULL);
					if (media_info){
						memcpy(audio_meta->stream_meta.codec_name, media_info, strlen(media_info));
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_CODEC_LONG_NAME, NULL);
					if (media_info){
						memcpy(audio_meta->stream_meta.codec_long_name, media_info, strlen(media_info));
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_CODEC_PROFILE, NULL);
					if (media_info) {
						memcpy(audio_meta->stream_meta.codec_profile, media_info, strlen(media_info));
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_BITRATE, NULL);
					if (media_info){
						audio_meta->stream_meta.stream_bitrate = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_STREAM_IDX, NULL);
					if (media_info) {
						audio_meta->stream_meta.stream_index = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_SAMPLE_RATE, NULL);
					if (media_info){
						audio_meta->audio_samples_per_sec = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_CHANNEL_LAYOUT, NULL);
					if (media_info){
						audio_meta->audio_channel_layout = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_LANGUAGE, NULL);
					if (media_info) {
						memcpy_s(audio_meta->language, 32, media_info, strlen(media_info));
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_TITLE, NULL);
					if (media_info) {
						memcpy_s(audio_meta->title, 128, media_info, strlen(media_info));
					}

					struct IjkAudioMetadata* tail = metadata->audio_stream_list;
					if (!tail) {
						metadata->audio_stream_list = audio_meta;
					}
					else {
						struct IjkAudioMetadata* next = tail->next;
						while (next) {
							tail = next;
							next = next->next;
						}
						tail->next = audio_meta;
					}

					if (audio_stream == i) {
						metadata->audio_stream = i;
					}
				} 
				else if (0 == strcmp(type, IJKM_VAL_TYPE__TIMEDTEXT)) {
					struct IjkSubtitleMetadata* sub_meta = (struct IjkSubtitleMetadata*)calloc(1, sizeof(struct IjkSubtitleMetadata));

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_STREAM_IDX, NULL);
					if (media_info) {
						sub_meta->stream_meta.stream_index = atoi(media_info);
					}

					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_LANGUAGE, NULL);
					if (media_info) {
						memcpy_s(sub_meta->timed_text_languge, 128, media_info, strlen(media_info));
					}
					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_TITLE, NULL);
					if (media_info) {
						memcpy_s(sub_meta->title, 128, media_info, strlen(media_info));
					}
					media_info = fillMetaInternal(streamRawMeta, IJKM_KEY_EX_SUBTITLE_URL, NULL);
					if (media_info) {
						memcpy_s(sub_meta->ex_subtitle_url, 1024, media_info, strlen(media_info));
					}

					struct IjkSubtitleMetadata* tail = metadata->subtitle_stream_list;
					if (!tail) {
						metadata->subtitle_stream_list = sub_meta;
					}
					else {
						struct IjkSubtitleMetadata* next = tail->next;
						while (next) {
							tail = next;
							next = next->next;
						}
						tail->next = sub_meta;
					}

					if (subtitle_stream == i) {
						metadata->subtitle_stream = i;
					}
				}
			}
		}
	}

	if (is_locked && meta){
		ijkmeta_unlock(meta);
	}

	return 0;
}

void release_metadata(IjkMetadata* metadata)
{
	struct IjkVideoMetadata* video_tail = metadata->video_stream_list;
	while (video_tail) {
		struct IjkVideoMetadata* next = video_tail->next;
		free(video_tail);
		video_tail = next;
	}

	struct IjkAudioMetadata* audio_tail = metadata->audio_stream_list;
	while (audio_tail) {
		struct IjkAudioMetadata* next = audio_tail->next;
		free(audio_tail);
		audio_tail = next;
	}

	struct IjkSubtitleMetadata* sub_tail = metadata->subtitle_stream_list;
	while (sub_tail) {
		struct IjkSubtitleMetadata* next = sub_tail->next;
		free(sub_tail);
		sub_tail = next;
	}
}

int ijkFfplayDecoder_releaseMetadata(IjkFfplayDecoder* decoder, IjkMetadata* metadata)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}
	
	release_metadata(metadata);

	return 0;
}

int ijkFfplayDecoder_setHwDecoderName(IjkFfplayDecoder* decoder, const char* decoder_name)
{
	if (!decoder || !decoder->ijk_media_player){
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	ijkmp_set_decoder_name(decoder->ijk_media_player, decoder_name);

	return 0;
}


void ijkFfplayDecoder_setWindowHwnd(IjkFfplayDecoder* decoder, HWND window)
{
	SDL_VoutWindows_SetWindowsSurface(decoder->ijk_media_player->ffplayer->vout, window);
}

float ijkFfplayDecoder_getSubtitleFontSize(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	return SDL_GetSubtileFontSize(decoder->ijk_media_player->ffplayer->vout);
}

int ijkFfplayDecoder_setSubtitleFontSize(IjkFfplayDecoder* decoder, float size)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	SDL_SetSubtitleFontSize(decoder->ijk_media_player->ffplayer->vout, size);

	return 0;
}

char* ijkFfplayDecoder_getSubtitleFontName(IjkFfplayDecoder* decoder)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return NULL;
	}

	return SDL_GetSubtitleFontName(decoder->ijk_media_player->ffplayer->vout);
}

int ijkFfplayDecoder_setSubtitleFontName(IjkFfplayDecoder* decoder, const char* font_name)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	SDL_SetSubtitleFontName(decoder->ijk_media_player->ffplayer->vout, font_name);
	return 0;
}

void* ijkFfplayDecoder_snapshot(IjkFfplayDecoder* decoder, int with_sub, void** pixel_data_out, int* w, int* h)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	SDL_Snapshot(decoder->ijk_media_player->ffplayer->vout, with_sub, pixel_data_out, w, h);
}

void  ijkFfplayDecoder_setColorPreference(IjkFfplayDecoder* decoder, double brightness, double saturaion, double contrast)
{
	if (!decoder || !decoder->ijk_media_player) {
		ALOGE("IjkMediaPlayer is NULL.\n");
		return -1;
	}

	SDL_SetColorPreference(decoder->ijk_media_player->ffplayer->vout, brightness, saturaion, contrast);
}

SimpleStreamInfo ijkUtil_extractStreamInfo(const char* file_name)
{
	AVFormatContext *formatCtx = avformat_alloc_context();
	if (!formatCtx) {
		ALOGE("创建 AVFormatContext 失败！\n");
		goto end;
	}

	//打开文件流，读取头信息；
	if (0 != avformat_open_input(&formatCtx, file_name, NULL, NULL)) {
		ALOGE("文件打开失败！\n");
		goto end;
	}

	if (0 != avformat_find_stream_info(formatCtx, NULL)) {
		ALOGE("不能找到流！\n");
		goto end;
	}

	SimpleStreamInfo info;
	memset(&info, -1, sizeof(SimpleStreamInfo));

	if (formatCtx->duration > 0) {
		float duration = (formatCtx->duration / 1000000.0);
		info.duration = duration;
	}

	//查找H264格式的视频流
	int idx_cnt[AVMEDIA_TYPE_NB] = { 0 };
	for (int i = 0; i < formatCtx->nb_streams; i++) {
		AVStream *st = formatCtx->streams[i];
		//skip attached picture/"cover art" (e.g.APIC frame in ID3v2)
		if (st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
			continue;
		}
		enum AVMediaType type = st->codecpar->codec_type;
		//这里设置为了丢弃所有帧，解码器里会进行修改！
		st->discard = AVDISCARD_ALL;
		if (type < AVMEDIA_TYPE_NB) {
			int i = idx_cnt[type]++;
			info.streams[type][i] = st->index;
		};
	}

end:
	if (formatCtx) {
		avformat_close_input(&formatCtx);
	}

	return info;
}
