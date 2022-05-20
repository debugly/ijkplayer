/*
 * ijkmeta.h
 *
 * Copyright (c) 2014 Bilibili
 * Copyright (c) 2014 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef IJKPLAYER__IJKMETA_H
#define IJKPLAYER__IJKMETA_H

#include <stdint.h>
#include <stdlib.h>

// media meta
#define IJKM_KEY_FORMAT         "format"
#define IJKM_KEY_DURATION_US    "duration_us"
#define IJKM_KEY_START_US       "start_us"
#define IJKM_KEY_BITRATE        "bitrate"
#define IJKM_KEY_VIDEO_STREAM   "video"
#define IJKM_KEY_AUDIO_STREAM   "audio"
#define IJKM_KEY_TIMEDTEXT_STREAM "timedtext"

// stream meta
#define IJKM_KEY_TYPE               "type"
#define IJKM_VAL_TYPE__VIDEO        "video"
#define IJKM_VAL_TYPE__AUDIO        "audio"
#define IJKM_VAL_TYPE__TIMEDTEXT    "timedtext"
#define IJKM_VAL_TYPE__UNKNOWN      "unknown"
#define IJKM_KEY_LANGUAGE           "language"
#define IJKM_KEY_TITLE              "title"
#define IJKM_KEY_STREAM_IDX         "stream_idx"
#define IJKM_KEY_ARTIST             "artist"
#define IJKM_KEY_ALBUM              "album"
#define IJKM_KEY_TYER               "TYER"

#define IJKM_KEY_CODEC_NAME         "codec_name"
#define IJKM_KEY_CODEC_PROFILE      "codec_profile"
#define IJKM_KEY_CODEC_LEVEL        "codec_level"
#define IJKM_KEY_CODEC_LONG_NAME    "codec_long_name"
#define IJKM_KEY_CODEC_PIXEL_FORMAT "codec_pixel_format"
#define IJKM_KEY_CODEC_PROFILE_ID   "codec_profile_id"

// stream: video
#define IJKM_KEY_WIDTH          "width"
#define IJKM_KEY_HEIGHT         "height"
#define IJKM_KEY_FPS_NUM        "fps_num"
#define IJKM_KEY_FPS_DEN        "fps_den"
#define IJKM_KEY_TBR_NUM        "tbr_num"
#define IJKM_KEY_TBR_DEN        "tbr_den"
#define IJKM_KEY_SAR_NUM        "sar_num"
#define IJKM_KEY_SAR_DEN        "sar_den"
// stream: audio
#define IJKM_KEY_SAMPLE_RATE    "sample_rate"
#define IJKM_KEY_CHANNEL_LAYOUT "channel_layout"
#define IJKM_KEY_CHANNELS		"channels"

#define IJKM_KEY_EX_SUBTITLE_URL "ex_subtile_url"
// reserved for user
#define IJKM_KEY_STREAMS        "streams"

//others
#define IJK_COMMENT				"comment"
#define IJK_ORIGINAL_FORMAT		"original_format"
#define	IJK_LENS_PARAM			"lens_param"
#define IJK_DEVICE_SN			"device_sn"
#define IJK_CDN_IP				"cdn_ip"

struct AVFormatContext;
struct FFPlayer;
struct VideoState;
typedef struct IjkMediaMeta IjkMediaMeta;

IjkMediaMeta *ijkmeta_create(void);
void ijkmeta_reset(IjkMediaMeta *meta);
void ijkmeta_destroy(IjkMediaMeta *meta);
void ijkmeta_destroy_p(IjkMediaMeta **meta);

void ijkmeta_lock(IjkMediaMeta *meta);
void ijkmeta_unlock(IjkMediaMeta *meta);

void ijkmeta_append_child_l(IjkMediaMeta *meta, IjkMediaMeta *child);
void ijkmeta_set_int64_l(IjkMediaMeta *meta, const char *name, int64_t value);
void ijkmeta_set_string_l(IjkMediaMeta *meta, const char *name, const char *value);
void ijkmeta_set_avformat_context_l(IjkMediaMeta *meta, struct AVFormatContext *ic);

void ijkmeta_set_ex_subtitle_context_l(IjkMediaMeta *meta, struct AVFormatContext *ic, struct VideoState *is, int actived);

// must be freed with free();
const char   *ijkmeta_get_string_l(IjkMediaMeta *meta, const char *name);
int64_t       ijkmeta_get_int64_l(IjkMediaMeta *meta, const char *name, int64_t defaultValue);
size_t        ijkmeta_get_children_count_l(IjkMediaMeta *meta);
// do not free
IjkMediaMeta *ijkmeta_get_child_l(IjkMediaMeta *meta, size_t index);

#endif//IJKPLAYER__IJKMETA_H
