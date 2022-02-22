//
//  ijk_ffplay_decoder.h
//  IJKMediaPlayer
//
//  Created by chao on 2017/6/12.
//  Copyright © 2017 detu. All rights reserved.
//

#ifndef IJK_FFPLAYER_DECODER_H
#define IJK_FFPLAYER_DECODER_H

#ifdef  __cplusplus  
extern "C" {
#endif 

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "ijk_frame.h"
#include "ijk_metadata.h"

//get video or audio info 
#define FLOAT_VIDEO_DECODE_FRAMES_PER_SECOND   10001
#define FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND   10002
#define FLOAT_PLAYBACK_RATE                    10003
#define FLOAT_AVDELAY                          10004
#define FLOAT_AVDIFF                           10005

#define INT64_VIDEO_CACHED_DURATION            20005
#define INT64_AUDIO_CACHED_DURATION            20006
#define INT64_VIDEO_CACHED_BYTES               20007
#define INT64_AUDIO_CACHED_BYTES               20008
#define INT64_VIDEO_CACHED_PACKETS             20009
#define INT64_AUDIO_CACHED_PACKETS             20010
#define INT64_BIT_RATE_TOTAL                   20100

//ijk log level
typedef enum IJKLogLevel {
    k_IJK_LOG_UNKNOWN = 0,
    k_IJK_LOG_DEFAULT = 1,
    
    k_IJK_LOG_VERBOSE = 2,
    k_IJK_LOG_DEBUG   = 3,
    k_IJK_LOG_INFO    = 4,
    k_IJK_LOG_WARN    = 5,
    k_IJK_LOG_ERROR   = 6,
    k_IJK_LOG_FATAL   = 7,
    k_IJK_LOG_SILENT  = 8,
} IJKLogLevel;

//message state
typedef enum IjkMsgState{
	IJK_MSG_FLUSH                       = 0,
	IJK_MSG_ERROR                       = 100,     /* arg1 = error */
	IJK_MSG_PREPARED                    = 200,
	IJK_MSG_COMPLETED                   = 300,
	IJK_MSG_VIDEO_SIZE_CHANGED          = 400,     /* arg1 = width, arg2 = height */
	IJK_MSG_SAR_CHANGED                 = 401,     /* arg1 = sar.num, arg2 = sar.den */
	IJK_MSG_VIDEO_RENDERING_START       = 402,
	IJK_MSG_AUDIO_RENDERING_START       = 403,
	IJK_MSG_VIDEO_ROTATION_CHANGED      = 404,     /* arg1 = degree */
	IJK_MSG_BUFFERING_START             = 500,
	IJK_MSG_BUFFERING_END               = 501,
	IJK_MSG_BUFFERING_UPDATE            = 502,     /* arg1 = buffering head position in time, arg2 = minimum percent in time or bytes */
	IJK_MSG_BUFFERING_BYTES_UPDATE      = 503,     /* arg1 = cached data in bytes,            arg2 = high water mark */
	IJK_MSG_BUFFERING_TIME_UPDATE       = 504,     /* arg1 = cached duration in milliseconds, arg2 = high water mark */
	IJK_MSG_SEEK_COMPLETE               = 600,     /* arg1 = seek position,                   arg2 = error */
	IJK_MSG_PLAYBACK_STATE_CHANGED      = 700,
	IJK_MSG_TIMED_TEXT					= 800,
	IJK_MSG_VIDEO_DECODE_FPS			= 850,	   /* arg1 = video decode fps */
	IJK_MSG_VIDEO_GOP_SIZE				= 851,	   /* arg1 = video gop size */
	IJK_MSG_ACCURATE_SEEK_COMPLETE      = 900,     /* arg1 = current position*/
	IJK_MSG_VIDEO_DECODER_OPEN          = 10001,
}IjkMsgState;

typedef struct IjkFfplayDecoderCallBack {
	void (*func_get_frame)(void* opaque, IjkVideoFrame *frame_callback);
	void (*func_state_change)(void* opaque, IjkMsgState ijk_msgint, int arg1, int arg2);
}IjkFfplayDecoderCallBack;

struct IjkFfplayDecoder;
typedef struct IjkFfplayDecoder IjkFfplayDecoder;


/////////////////////////////////////////////////////////////////////////////////////////////////////////

int ijkFfplayDecoder_init(void);

int ijkFfplayDecoder_uninit(void);

IjkFfplayDecoder *ijkFfplayDecoder_create(void);

int ijkFfplayDecoder_setLogLevel(IJKLogLevel log_level);

int ijkFfplayDecoder_setLogCallback(void(*callback)(void*, int, const char*, va_list));

int ijkFfplayDecoder_setDecoderCallBack(IjkFfplayDecoder* decoder, void* opaque, IjkFfplayDecoderCallBack* callback);

int ijkFfplayDecoder_setDataSource(IjkFfplayDecoder* decoder, const char* file_absolute_path);

int ijkFfplayDecoder_prepare(IjkFfplayDecoder* decoder);

int ijkFfplayDecoder_start(IjkFfplayDecoder* decoder);

int ijkFfplayDecoder_pause(IjkFfplayDecoder* decoder);

int ijkFfplayDecoder_stop(IjkFfplayDecoder* decoder);

int ijkFfplayDecoder_seekTo(IjkFfplayDecoder* decoder, long msec);

bool ijkFfplayDecoder_isPlaying(IjkFfplayDecoder* decoder);

long ijkFfplayDecoder_getCurrentPosition(IjkFfplayDecoder* decoder);

long ijkFfplayDecoder_getDuration(IjkFfplayDecoder* decoder);

int ijkFfplayDecoder_release(IjkFfplayDecoder* decoder);

int ijkFfplayDecoder_setVolume(IjkFfplayDecoder* decoder, float volume);

float ijkFfplayDecoder_getVolume(IjkFfplayDecoder* decoder);

int ijkFfplayDecoder_setOptionLongValue(IjkFfplayDecoder* decoder, int opt_category, const char* key, long value);

int ijkFfplayDecoder_setOptionStringValue(IjkFfplayDecoder* decoder, int opt_category, const char* key, const char* value);

int ijkFfplayDecoder_getVideoCodecInfo(IjkFfplayDecoder* decoder, char **codec_info);

int ijkFfplayDecoder_getAudioCodecInfo(IjkFfplayDecoder* decoder, char **codec_info);

long ijkFfplayDecoder_getPropertyLong(IjkFfplayDecoder* decoder, int id, long default_value);

float ijkFfplayDecoder_getPropertyFloat(IjkFfplayDecoder* decoder, int id, float default_value);

int ijkFfplayDecoder_getMediaMeta(IjkFfplayDecoder* decoder, IjkMetadata* metadata);

//decoder_name: h264_cuvid, h264_qsv
int ijkFfplayDecoder_setHwDecoderName(IjkFfplayDecoder* decoder, const char* decoder_name);

int ijkFfplayDecoder_setDropFrameNums(IjkFfplayDecoder* decoder, int nums);

#ifdef  __cplusplus  
}
#endif

#endif /* IJK_FFPLAYER_DECODER_H */
