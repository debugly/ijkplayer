//
//  ijk_metadata.h
//
//  Created by chen on 2017/7/28.
//  Copyright © 2017 detu. All rights reserved.
//

#ifndef IJK_METADATA_H
#define IJK_METADATA_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


// media meta
#define k_IJKM_KEY_FORMAT         "format"
#define k_IJKM_KEY_DURATION_US    "duration_us"
#define k_IJKM_KEY_START_US       "start_us"
#define k_IJKM_KEY_BITRATE        "bitrate"

// stream meta
#define k_IJKM_KEY_TYPE           "type"
#define k_IJKM_VAL_TYPE__VIDEO    "video"
#define k_IJKM_VAL_TYPE__AUDIO    "audio"
#define k_IJKM_VAL_TYPE__UNKNOWN  "unknown"

#define k_IJKM_KEY_CODEC_NAME      "codec_name"
#define k_IJKM_KEY_CODEC_PROFILE   "codec_profile"
#define k_IJKM_KEY_CODEC_LONG_NAME "codec_long_name"

// stream: video
#define k_IJKM_KEY_WIDTH          "width"
#define k_IJKM_KEY_HEIGHT         "height"
#define k_IJKM_KEY_FPS_NUM        "fps_num"
#define k_IJKM_KEY_FPS_DEN        "fps_den"
#define k_IJKM_KEY_TBR_NUM        "tbr_num"
#define k_IJKM_KEY_TBR_DEN        "tbr_den"
#define k_IJKM_KEY_SAR_NUM        "sar_num"
#define k_IJKM_KEY_SAR_DEN        "sar_den"
// stream: audio
#define k_IJKM_KEY_SAMPLE_RATE    "sample_rate"
#define k_IJKM_KEY_CHANNEL_LAYOUT "channel_layout"

#define kk_IJKM_KEY_STREAMS       "streams"

struct IjkStreamMetadata {
	char			codec_name[32];
	char			codec_profile[128];
	char			codec_long_name[128];
	long			stream_bitrate;
	int				stream_index;
};

struct IjkVideoMetadata {
	struct IjkStreamMetadata	stream_meta;
	int				width;
	int				height;
	int				video_fps_num;
	int				video_fps_den;
	int				video_tbr_num;
	int				video_tbr_den;
	int				video_sar_num;
	int				video_sar_den;

	struct IjkVideoMetadata*	next;
};

struct IjkAudioMetadata {
	struct IjkStreamMetadata	stream_meta;
	int				audio_samples_per_sec;
	int				audio_channel_layout;
	char			language[32];
	wchar_t			title[128];

	struct IjkAudioMetadata*	next;
};

struct IjkSubtitleMetadata {
	struct IjkStreamMetadata	stream_meta;

	char			timed_text_languge[128];
	wchar_t			title[128];
	wchar_t			ex_subtitle_url[1024];

	struct IjkSubtitleMetadata* next;
};

typedef struct IjkMetadata{
	char			format[16];
	int				bitrate;
	long			duration_ms;
	long			start_us;

	int				video_stream;
	int				audio_stream;
	int				subtitle_stream;

	struct IjkVideoMetadata*	video_stream_list;
	struct IjkAudioMetadata*	audio_stream_list;
	struct IjkSubtitleMetadata*	subtitle_stream_list;
} IjkMetadata;

void release_metadata(IjkMetadata* meta);

#endif
