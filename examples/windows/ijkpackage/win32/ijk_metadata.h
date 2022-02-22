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
#define k_IJKM_KEY_FORMAT         @"format"
#define k_IJKM_KEY_DURATION_US    @"duration_us"
#define k_IJKM_KEY_START_US       @"start_us"
#define k_IJKM_KEY_BITRATE        @"bitrate"

// stream meta
#define k_IJKM_KEY_TYPE           @"type"
#define k_IJKM_VAL_TYPE__VIDEO    @"video"
#define k_IJKM_VAL_TYPE__AUDIO    @"audio"
#define k_IJKM_VAL_TYPE__UNKNOWN  @"unknown"

#define k_IJKM_KEY_CODEC_NAME      @"codec_name"
#define k_IJKM_KEY_CODEC_PROFILE   @"codec_profile"
#define k_IJKM_KEY_CODEC_LONG_NAME @"codec_long_name"

// stream: video
#define k_IJKM_KEY_WIDTH          @"width"
#define k_IJKM_KEY_HEIGHT         @"height"
#define k_IJKM_KEY_FPS_NUM        @"fps_num"
#define k_IJKM_KEY_FPS_DEN        @"fps_den"
#define k_IJKM_KEY_TBR_NUM        @"tbr_num"
#define k_IJKM_KEY_TBR_DEN        @"tbr_den"
#define k_IJKM_KEY_SAR_NUM        @"sar_num"
#define k_IJKM_KEY_SAR_DEN        @"sar_den"
// stream: audio
#define k_IJKM_KEY_SAMPLE_RATE    @"sample_rate"
#define k_IJKM_KEY_CHANNEL_LAYOUT @"channel_layout"

#define kk_IJKM_KEY_STREAMS       @"streams"


typedef struct IjkMetadata{
	int				video_bitrate;
	int				audio_bitrate;
	int				width;
	int				height;
	long			duration_ms;

	char			video_code_name[32];
	char			video_code_long_name[128];
	int				video_fps_num;
	int				video_fps_den;
	int				video_tbr_num;
	int				video_tbr_den;

	char			audio_code_name[32];
	char			audio_code_long_name[128];
	int				audio_samples_per_sec;
	int				audio_channel_layout;

	char			comment[1024];
	char			original_format[1024];
	char			lens_param[1024];
	char			device_sn[128];
	char			cdn_ip[128];
}IjkMetadata;

#endif
