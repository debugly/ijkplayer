/*
 * IJKFFMoviePlayerController.m
 *
 * Copyright (c) 2013 Bilibili
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
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

#import "IJKFFMoviePlayerController.h"
#import "IJKSDLGLView.h"
#import "IJKSDLHudControl.h"
#import "IJKFFMoviePlayerDef.h"
#import "IJKMediaPlayback.h"
#import "IJKMediaModule.h"
#import "IJKNotificationManager.h"
#import "NSString+IJKMedia.h"
#if ! IJK_IO_OFF
#import "ijkioapplication.h"
#endif
#include "string.h"
#if TARGET_OS_IOS
#import "IJKAudioKit.h"
#endif

#include "../ijkmedia/ijkplayer/apple/ijkplayer_ios.h"
#include "../ijkmedia/ijkplayer/ijkmeta.h"
#include "../ijkmedia/ijkplayer/ff_ffmsg_queue.h"

static const char *kIJKFFRequiredFFmpegVersion = "n4.0-2-g5bf1d483f23"; //"ff4.0--ijk0.8.8--20210426--001";
static void (^_logHandler)(IJKLogLevel level, NSString *tag, NSString *msg);

// It means you didn't call shutdown if you found this object leaked.
@interface IJKWeakHolder : NSObject
@property (nonatomic, weak) id object;
@end

@implementation IJKWeakHolder
@end

@interface IJKFFMoviePlayerController()

@property (nonatomic, strong) NSURL *contentURL;

@end

@implementation IJKFFMoviePlayerController {
    IjkMediaPlayer *_mediaPlayer;
    GLView<IJKSDLGLViewProtocol>* _glView;
    IJKFFMoviePlayerMessagePool *_msgPool;

    NSInteger _videoWidth;
    NSInteger _videoHeight;
    NSInteger _sampleAspectRatioNumerator;
    NSInteger _sampleAspectRatioDenominator;
    NSInteger _videoZRotateDegrees;
    BOOL      _seeking;
    NSInteger _bufferingTime;
    NSInteger _bufferingPosition;

    BOOL _keepScreenOnWhilePlaying;
    BOOL _pauseInBackground;
    BOOL _isVideoToolboxOpen;
    BOOL _playingBeforeInterruption;

#if ! IJK_IO_OFF
    AVAppAsyncStatistic _asyncStat;
    IjkIOAppCacheStatistic _cacheStat;
#endif
    NSTimer *_hudTimer;
    IJKSDLHudControl *_hudCtrl;
#if TARGET_OS_IOS
    IJKNotificationManager *_notificationManager;
#endif
}

@synthesize view = _view;
@synthesize currentPlaybackTime;
@synthesize duration;
@synthesize playableDuration;
@synthesize bufferingProgress = _bufferingProgress;

@synthesize numberOfBytesTransferred = _numberOfBytesTransferred;

@synthesize isPreparedToPlay = _isPreparedToPlay;
@synthesize playbackState = _playbackState;
@synthesize loadState = _loadState;

@synthesize naturalSize = _naturalSize;
@synthesize scalingMode = _scalingMode;
@synthesize shouldAutoplay = _shouldAutoplay;

@synthesize allowsMediaAirPlay = _allowsMediaAirPlay;
@synthesize airPlayMediaActive = _airPlayMediaActive;

@synthesize isDanmakuMediaAirPlay = _isDanmakuMediaAirPlay;

@synthesize monitor = _monitor;
@synthesize shouldShowHudView           = _shouldShowHudView;
@synthesize isSeekBuffering = _isSeekBuffering;
@synthesize isAudioSync = _isAudioSync;
@synthesize isVideoSync = _isVideoSync;

#define FFP_IO_STAT_STEP (50 * 1024)

// as an example
void IJKFFIOStatDebugCallback(const char *url, int type, int bytes)
{
    static int64_t s_ff_io_stat_check_points = 0;
    static int64_t s_ff_io_stat_bytes = 0;
    if (!url)
        return;

    if (type != IJKMP_IO_STAT_READ)
        return;

    if (!av_strstart(url, "http:", NULL))
        return;

    s_ff_io_stat_bytes += bytes;
    if (s_ff_io_stat_bytes < s_ff_io_stat_check_points ||
        s_ff_io_stat_bytes > s_ff_io_stat_check_points + FFP_IO_STAT_STEP) {
        s_ff_io_stat_check_points = s_ff_io_stat_bytes;
        NSLog(@"io-stat: %s, +%d = %"PRId64"\n", url, bytes, s_ff_io_stat_bytes);
    }
}

void IJKFFIOStatRegister(void (*cb)(const char *url, int type, int bytes))
{
    ijkmp_io_stat_register(cb);
}

void IJKFFIOStatCompleteDebugCallback(const char *url,
                                      int64_t read_bytes, int64_t total_size,
                                      int64_t elpased_time, int64_t total_duration)
{
    if (!url)
        return;

    if (!av_strstart(url, "http:", NULL))
        return;

    NSLog(@"io-stat-complete: %s, %"PRId64"/%"PRId64", %"PRId64"/%"PRId64"\n",
          url, read_bytes, total_size, elpased_time, total_duration);
}

void IJKFFIOStatCompleteRegister(void (*cb)(const char *url,
                                            int64_t read_bytes, int64_t total_size,
                                            int64_t elpased_time, int64_t total_duration))
{
    ijkmp_io_stat_complete_register(cb);
}

- (void)setScreenOn: (BOOL)on
{
    [IJKMediaModule sharedModule].mediaModuleIdleTimerDisabled = on;
    // [UIApplication sharedApplication].idleTimerDisabled = on;
}

- (void)_initWithContent:(NSURL *)aUrl options:(IJKFFOptions *)options glView:(GLView <IJKSDLGLViewProtocol> *)glView
{
    // init media resource
    _contentURL = aUrl;
    
    ijkmp_global_init();
#if ! IJK_IO_OFF
    ijkmp_global_set_inject_callback(ijkff_inject_callback);
#endif
    [IJKFFMoviePlayerController checkIfFFmpegVersionMatch:NO];

    if (options == nil)
        options = [IJKFFOptions optionsByDefault];

    // IJKFFIOStatRegister(IJKFFIOStatDebugCallback);
    // IJKFFIOStatCompleteRegister(IJKFFIOStatCompleteDebugCallback);

    // init fields
    _scalingMode = IJKMPMovieScalingModeAspectFit;
    _shouldAutoplay = YES;
#if ! IJK_IO_OFF
    memset(&_asyncStat, 0, sizeof(_asyncStat));
    memset(&_cacheStat, 0, sizeof(_cacheStat));
#endif
    _monitor = [[IJKFFMonitor alloc] init];

    // init player
    _mediaPlayer = ijkmp_ios_create(media_player_msg_loop);
    _msgPool = [[IJKFFMoviePlayerMessagePool alloc] init];
    IJKWeakHolder *weakHolder = [IJKWeakHolder new];
    weakHolder.object = self;

    ijkmp_set_weak_thiz(_mediaPlayer, (__bridge_retained void *) self);
#if ! IJK_IO_OFF
    ijkmp_set_inject_opaque(_mediaPlayer, (__bridge_retained void *) weakHolder);
    ijkmp_set_ijkio_inject_opaque(_mediaPlayer, (__bridge_retained void *)weakHolder);
#endif
    ijkmp_set_option_int(_mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "start-on-prepared", _shouldAutoplay ? 1 : 0);

    _view = _glView = glView;
    ijkmp_ios_set_glview(_mediaPlayer, glView);
    ijkmp_set_option(_mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "overlay-format", "fcc-_es2");
    //ijkmp_set_option(_mediaPlayer,IJKMP_OPT_CATEGORY_FORMAT,"safe", 0);
    //ijkmp_set_option(_mediaPlayer,IJKMP_OPT_CATEGORY_PLAYER,"protocol_whitelist","ffconcat,file,http,https");
    ijkmp_set_option(_mediaPlayer,IJKMP_OPT_CATEGORY_FORMAT,"protocol_whitelist","ijkhttphook,concat,http,tcp,https,tls,file");
    
    // init hud
    _hudCtrl = [IJKSDLHudControl new];

    self.shouldShowHudView = options.showHudView;

    [options applyTo:_mediaPlayer];
    _pauseInBackground = NO;

    // init extra
    _keepScreenOnWhilePlaying = YES;
    [self setScreenOn:YES];

#if TARGET_OS_IOS
    _notificationManager = [[IJKNotificationManager alloc] init];
    // init audio sink
    [[IJKAudioKit sharedInstance] setupAudioSession];
    [self registerApplicationObservers];
#endif
    
}

- (id)initWithContentURL:(NSURL *)aUrl withOptions:(IJKFFOptions *)options
{
    if (aUrl == nil)
        return nil;

    self = [super init];
    if (self) {
        // init video sink
    #if TARGET_OS_IOS
        CGRect rect = [[UIScreen mainScreen] bounds];
    #else
        CGRect rect = [[NSScreen mainScreen]frame];
        rect.origin = CGPointZero;
    #endif
        IJKSDLGLView *glView = [[IJKSDLGLView alloc] initWithFrame:rect];
        glView.isThirdGLView = NO;
        [self _initWithContent:aUrl options:options glView:glView];
    }
    return self;
}

- (id)initWithMoreContent:(NSURL *)aUrl
              withOptions:(IJKFFOptions *)options
               withGLView:(GLView<IJKSDLGLViewProtocol> *)glView
{
    if (aUrl == nil)
        return nil;

    self = [super init];
    if (self) {
        // init video sink
        glView.isThirdGLView = YES;
        [self _initWithContent:aUrl options:options glView:glView];
    }
    return self;
}

- (void)dealloc
{
//    [self unregisterApplicationObservers];
}

- (void)setShouldAutoplay:(BOOL)shouldAutoplay
{
    _shouldAutoplay = shouldAutoplay;

    if (!_mediaPlayer)
        return;

    ijkmp_set_option_int(_mediaPlayer, IJKMP_OPT_CATEGORY_PLAYER, "start-on-prepared", _shouldAutoplay ? 1 : 0);
}

- (BOOL)shouldAutoplay
{
    return _shouldAutoplay;
}

- (void)prepareToPlay
{
    if (!_mediaPlayer)
        return;

    [self setScreenOn:_keepScreenOnWhilePlaying];
    if (![_contentURL isFileURL]) {
        [self setHudValue:nil forKey:@"scheme"];
        [self setHudValue:nil forKey:@"host"];
        [self setHudValue:nil forKey:@"path"];
        [self setHudValue:nil forKey:@"ip"];
        [self setHudValue:nil forKey:@"tcp-info"];
        [self setHudValue:nil forKey:@"http"];
        [self setHudValue:nil forKey:@"tcp-spd"];
        [self setHudValue:nil forKey:@"t-prepared"];
        [self setHudValue:nil forKey:@"t-render"];
        [self setHudValue:nil forKey:@"t-preroll"];
        [self setHudValue:nil forKey:@"t-http-open"];
        [self setHudValue:nil forKey:@"t-http-seek"];
    }
    
    // Detect if URL is file path and return proper string for it
    NSString *urlString = [_contentURL isFileURL] ? [_contentURL path] : [_contentURL absoluteString];
    
    ijkmp_set_data_source(_mediaPlayer, [urlString UTF8String]);
    ijkmp_set_option_int(_mediaPlayer, IJKMP_OPT_CATEGORY_FORMAT, "safe", 0); // for concat demuxer

    _monitor.prepareStartTick = (int64_t)SDL_GetTickHR();
    ijkmp_prepare_async(_mediaPlayer);
}

- (void)loadThenActiveSubtitleFile:(NSString *)url
{
    if (!_mediaPlayer)
        return;
    
    ijkmp_set_external_subtitle(_mediaPlayer, [url UTF8String]);
}

- (void)loadSubtitleFileOnly:(NSString *)url
{
    if (!_mediaPlayer)
        return;
    
    ijkmp_load_external_subtitle(_mediaPlayer, [url UTF8String]);
}

- (void)play
{
    if (!_mediaPlayer)
        return;

    [self setScreenOn:_keepScreenOnWhilePlaying];

    [self startHudTimer];
    ijkmp_start(_mediaPlayer);
}

- (void)pause
{
    if (!_mediaPlayer)
        return;

//    [self stopHudTimer];
    ijkmp_pause(_mediaPlayer);
}

- (void)stop
{
    if (!_mediaPlayer)
        return;

    [self setScreenOn:NO];
    [self stopHudTimer];
    ijkmp_stop(_mediaPlayer);
}

- (BOOL)isPlaying
{
    if (!_mediaPlayer)
        return NO;

    return ijkmp_is_playing(_mediaPlayer);
}

- (void)setPauseInBackground:(BOOL)pause
{
    _pauseInBackground = pause;
}

- (BOOL)isVideoToolboxOpen
{
    if (!_mediaPlayer)
        return NO;

    return _isVideoToolboxOpen;
}

- (BOOL)isUsingHardwareAccelerae
{
    int64_t vdec = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_DECODER, FFP_PROPV_DECODER_UNKNOWN);
    return vdec == FFP_PROPV_DECODER_AVCODEC_HW;
}

inline static int getPlayerOption(IJKFFOptionCategory category)
{
    int mp_category = -1;
    switch (category) {
        case kIJKFFOptionCategoryFormat:
            mp_category = IJKMP_OPT_CATEGORY_FORMAT;
            break;
        case kIJKFFOptionCategoryCodec:
            mp_category = IJKMP_OPT_CATEGORY_CODEC;
            break;
        case kIJKFFOptionCategorySws:
            mp_category = IJKMP_OPT_CATEGORY_SWS;
            break;
        case kIJKFFOptionCategoryPlayer:
            mp_category = IJKMP_OPT_CATEGORY_PLAYER;
            break;
        default:
            NSLog(@"unknown option category: %d\n", category);
    }
    return mp_category;
}

- (void)setOptionValue:(NSString *)value
                forKey:(NSString *)key
            ofCategory:(IJKFFOptionCategory)category
{
    assert(_mediaPlayer);
    if (!_mediaPlayer)
        return;

    ijkmp_set_option(_mediaPlayer, getPlayerOption(category), [key UTF8String], [value UTF8String]);
}

- (void)setOptionIntValue:(int64_t)value
                   forKey:(NSString *)key
               ofCategory:(IJKFFOptionCategory)category
{
    assert(_mediaPlayer);
    if (!_mediaPlayer)
        return;

    ijkmp_set_option_int(_mediaPlayer, getPlayerOption(category), [key UTF8String], value);
}

#ifdef __APPLE__
void ffp_apple_log_extra_vprint(int level, const char *tag, const char *fmt, va_list ap)
{
    IJKLogLevel curr_lv = [IJKFFMoviePlayerController getLogLevel];
    if (level < curr_lv) {
        return;
    }
    
    if (_logHandler) {
        NSString *tagStr = tag ? [[NSString alloc] initWithUTF8String:tag] : @"";
        NSString *fmtStr = fmt ? [[NSString alloc] initWithUTF8String:fmt] : @"";
        NSString *msgStr = [[NSString alloc] initWithFormat:fmtStr arguments: ap];
        _logHandler(level, tagStr, msgStr);
    } else {
        size_t len = 0;
        if (fmt && (len = strlen(fmt)) > 0) {
            char end = fmt[len - 1];
            if (end == '\n') {
                if (len == 1) {
                    vprintf(fmt, ap);
                } else {
                    char new_fmt[1024];
                    sprintf(new_fmt, "[%s]%s", tag, fmt);
                    vprintf(new_fmt, ap);
                }
            } else {
                vprintf(fmt, ap);
            }
        }
    }
}

void ffp_apple_log_extra_print(int level, const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ffp_apple_log_extra_vprint(level, tag, fmt, args);
    va_end(args);
}
#endif
+ (void)setLogReport:(BOOL)preferLogReport
{
    ijkmp_global_set_log_report(preferLogReport ? 1 : 0);
}

+ (void)setLogLevel:(IJKLogLevel)logLevel
{
    ijkmp_global_set_log_level(logLevel);
}

+ (IJKLogLevel)getLogLevel
{
    return ijkmp_global_get_log_level();
}

+ (void)setLogHandler:(void (^)(IJKLogLevel, NSString *, NSString *))handler
{
    _logHandler = handler;
}

+ (BOOL)checkIfFFmpegVersionMatch:(BOOL)showAlert;
{
    const char *actualVersion = av_version_info();
    const char *expectVersion = kIJKFFRequiredFFmpegVersion;
    if (0 == strcmp(actualVersion, expectVersion)) {
        return YES;
    } else {
        NSString *message = [NSString stringWithFormat:@"actual: %s\nexpect: %s\n", actualVersion, expectVersion];
        NSLog(@"\n!!!!!!!!!!\n%@!!!!!!!!!!\n", message);
#if TARGET_OS_IOS
        if (showAlert) {
            UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:@"Unexpected FFmpeg version"
                                                                message:message
                                                               delegate:nil
                                                      cancelButtonTitle:@"OK"
                                                      otherButtonTitles:nil];
            [alertView show];
        }
#endif
        return NO;
    }
}

+ (BOOL)checkIfPlayerVersionMatch:(BOOL)showAlert
                          version:(NSString *)version
{
    const char *actualVersion = ijkmp_version();
    const char *expectVersion = version.UTF8String;
    if (0 == strcmp(actualVersion, expectVersion)) {
        return YES;
    } else {
#if TARGET_OS_IOS
        if (showAlert) {
            NSString *message = [NSString stringWithFormat:@"actual: %s\n expect: %s\n",
                                 actualVersion, expectVersion];
            UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:@"Unexpected ijkplayer version"
                                                                message:message
                                                               delegate:nil
                                                      cancelButtonTitle:@"OK"
                                                      otherButtonTitles:nil];
            [alertView show];
        }
#endif
        return NO;
    }
}

- (void)shutdown
{
    if (!_mediaPlayer)
        return;
#if TARGET_OS_IOS
    [self unregisterApplicationObservers];
#endif
    [self setScreenOn:NO];
    [self destroyHud];
    //release glview in main thread.
    _view = _glView = nil;
    ijkmp_ios_set_glview(_mediaPlayer, nil);
    [self performSelectorInBackground:@selector(shutdownWaitStop:) withObject:self];
}

- (void)shutdownWaitStop:(IJKFFMoviePlayerController *) mySelf
{
    if (!_mediaPlayer)
        return;

    ijkmp_stop(_mediaPlayer);
    ijkmp_shutdown(_mediaPlayer);

    [self performSelectorOnMainThread:@selector(shutdownClose:) withObject:self waitUntilDone:YES];
}

- (void)shutdownClose:(IJKFFMoviePlayerController *) mySelf
{
    if (!_mediaPlayer)
        return;

    _segmentOpenDelegate    = nil;
    _tcpOpenDelegate        = nil;
    _httpOpenDelegate       = nil;
    _liveOpenDelegate       = nil;
    _nativeInvokeDelegate   = nil;

#if ! IJK_IO_OFF
    __unused id weakPlayer = (__bridge_transfer IJKFFMoviePlayerController*)ijkmp_set_weak_thiz(_mediaPlayer, NULL);
    __unused id weakHolder = (__bridge_transfer IJKWeakHolder*)ijkmp_set_inject_opaque(_mediaPlayer, NULL);
    __unused id weakijkHolder = (__bridge_transfer IJKWeakHolder*)ijkmp_set_ijkio_inject_opaque(_mediaPlayer, NULL);
#endif
    ijkmp_dec_ref_p(&_mediaPlayer);

    [self didShutdown];
}

- (void)didShutdown
{
}

- (IJKMPMoviePlaybackState)playbackState
{
    if (!_mediaPlayer)
        return NO;

    IJKMPMoviePlaybackState mpState = IJKMPMoviePlaybackStateStopped;
    int state = ijkmp_get_state(_mediaPlayer);
    switch (state) {
        case MP_STATE_STOPPED:
        case MP_STATE_COMPLETED:
        case MP_STATE_ERROR:
        case MP_STATE_END:
            mpState = IJKMPMoviePlaybackStateStopped;
            break;
        case MP_STATE_IDLE:
        case MP_STATE_INITIALIZED:
        case MP_STATE_ASYNC_PREPARING:
        case MP_STATE_PAUSED:
            mpState = IJKMPMoviePlaybackStatePaused;
            break;
        case MP_STATE_PREPARED:
        case MP_STATE_STARTED: {
            if (_seeking)
                mpState = IJKMPMoviePlaybackStateSeekingForward;
            else
                mpState = IJKMPMoviePlaybackStatePlaying;
            break;
        }
    }
    // IJKMPMoviePlaybackStatePlaying,
    // IJKMPMoviePlaybackStatePaused,
    // IJKMPMoviePlaybackStateStopped,
    // IJKMPMoviePlaybackStateInterrupted,
    // IJKMPMoviePlaybackStateSeekingForward,
    // IJKMPMoviePlaybackStateSeekingBackward
    return mpState;
}

- (void)setCurrentPlaybackTime:(NSTimeInterval)aCurrentPlaybackTime
{
    if (!_mediaPlayer)
        return;

    _seeking = YES;
    [[NSNotificationCenter defaultCenter]
     postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
     object:self];

    _bufferingPosition = 0;
    ijkmp_seek_to(_mediaPlayer, aCurrentPlaybackTime * 1000);
}

- (NSTimeInterval)currentPlaybackTime
{
    if (!_mediaPlayer)
        return 0.0f;

    NSTimeInterval ret = ijkmp_get_current_position(_mediaPlayer);
    if (isnan(ret) || isinf(ret))
        return -1;

    return ret / 1000;
}

- (NSTimeInterval)duration
{
    if (!_mediaPlayer)
        return 0.0f;

    NSTimeInterval ret = ijkmp_get_duration(_mediaPlayer);
    if (isnan(ret) || isinf(ret))
        return -1;

    return ret / 1000;
}

- (NSTimeInterval)playableDuration
{
    if (!_mediaPlayer)
        return 0.0f;

    NSTimeInterval demux_cache = ((NSTimeInterval)ijkmp_get_playable_duration(_mediaPlayer)) / 1000;
#if ! IJK_IO_OFF
    int64_t buf_forwards = _asyncStat.buf_forwards;
    int64_t bit_rate = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_BIT_RATE, 0);

    if (buf_forwards > 0 && bit_rate > 0) {
        NSTimeInterval io_cache = ((float)buf_forwards) * 8 / bit_rate;
        demux_cache += io_cache;
    }
#endif
    return demux_cache;
}

- (CGSize)naturalSize
{
    return _naturalSize;
}

- (void)changeNaturalSize
{
    CGSize naturalSize = CGSizeZero;
    if (_sampleAspectRatioNumerator > 0 && _sampleAspectRatioDenominator > 0) {
        naturalSize = CGSizeMake(1.0f * _videoWidth * _sampleAspectRatioNumerator / _sampleAspectRatioDenominator, _videoHeight);
    } else {
        naturalSize = CGSizeMake(_videoWidth, _videoHeight);
    }
    
    if (CGSizeEqualToSize(self->_naturalSize, naturalSize)) {
        return;
    }
    
    if (naturalSize.width > 0 && naturalSize.height > 0) {
        [self willChangeValueForKey:@"naturalSize"];
        self->_naturalSize = naturalSize;
        [self didChangeValueForKey:@"naturalSize"];
#if TARGET_OS_IOS
        [[NSNotificationCenter defaultCenter]
         postNotificationName:IJKMPMovieNaturalSizeAvailableNotification
         object:self userInfo:@{@"size":NSStringFromCGSize(self->_naturalSize)}];
#else
        [[NSNotificationCenter defaultCenter]
         postNotificationName:IJKMPMovieNaturalSizeAvailableNotification
         object:self userInfo:@{@"size":NSStringFromSize(self->_naturalSize)}];
#endif
        if ([self.view respondsToSelector:@selector(videoNaturalSizeChanged:)]) {
            [self.view videoNaturalSizeChanged:self->_naturalSize];
        }
    }
}

- (NSInteger)videoZRotateDegrees
{
    return _videoZRotateDegrees;
}

- (void)setScalingMode:(IJKMPMovieScalingMode) aScalingMode
{
    IJKMPMovieScalingMode newScalingMode = aScalingMode;
    self.view.scalingMode = aScalingMode;
    _scalingMode = newScalingMode;
}

// deprecated, for MPMoviePlayerController compatiable
- (UIImage *)thumbnailImageAtTime:(NSTimeInterval)playbackTime timeOption:(IJKMPMovieTimeOption)option
{
    return nil;
}

#if TARGET_OS_IOS
- (UIImage *)thumbnailImageAtCurrentTime
{
    if ([_view conformsToProtocol:@protocol(IJKSDLGLViewProtocol)]) {
        GLView<IJKSDLGLViewProtocol>* glView = (GLView<IJKSDLGLViewProtocol>*)_view;
        return [glView snapshot];
    }

    return nil;
}
#endif

- (CGFloat)fpsAtOutput
{
    return _mediaPlayer ? ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND, .0f) : .0f;
}

#pragma mark IJKFFHudController

- (NSDictionary *)allHudItem
{
    if (!self.shouldShowHudView) {
        [self refreshHudView];
    }
    return [_hudCtrl allHudItem];
}

- (void)setHudValue:(NSString *)value forKey:(NSString *)key
{
    if ([[NSThread currentThread] isMainThread]) {
        [_hudCtrl setHudValue:value forKey:key];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self setHudValue:value forKey:key];
        });
    }
}

- (void)setHudUrl:(NSString *)urlString
{
    if ([[NSThread currentThread] isMainThread]) {
        NSURL *url = [NSURL URLWithString:urlString];
        [self setHudValue:url.scheme forKey:@"scheme"];
        [self setHudValue:url.host   forKey:@"host"];
        [self setHudValue:url.path   forKey:@"path"];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self setHudUrl:urlString];
        });
    }
}

inline static NSString *formatedDurationMilli(int64_t duration) {
    if (duration >=  1000) {
        return [NSString stringWithFormat:@"%.2f sec", ((float)duration) / 1000];
    } else {
        return [NSString stringWithFormat:@"%ld msec", (long)duration];
    }
}

//#if ! IJK_IO_OFF
//inline static NSString *formatedDurationBytesAndBitrate(int64_t bytes, int64_t bitRate) {
//    if (bitRate <= 0) {
//        return @"inf";
//    }
//    return formatedDurationMilli(((float)bytes) * 8 * 1000 / bitRate);
//}
//#endif

inline static NSString *formatedSize(int64_t bytes) {
    if (bytes >= 100 * 1024) {
        return [NSString stringWithFormat:@"%.2f MB", ((float)bytes) / 1000 / 1024];
    } else if (bytes >= 100) {
        return [NSString stringWithFormat:@"%.1f KB", ((float)bytes) / 1000];
    } else {
        return [NSString stringWithFormat:@"%ld B", (long)bytes];
    }
}

inline static NSString *formatedSpeed(int64_t bytes, int64_t elapsed_milli) {
    if (elapsed_milli <= 0) {
        return @"N/A";
    }

    if (bytes <= 0) {
        return @"0";
    }

    float bytes_per_sec = ((float)bytes) * 1000.f /  elapsed_milli;
    if (bytes_per_sec >= 1000 * 1000) {
        return [NSString stringWithFormat:@"%.2f MB/s", ((float)bytes_per_sec) / 1000 / 1000];
    } else if (bytes_per_sec >= 1000) {
        return [NSString stringWithFormat:@"%.1f KB/s", ((float)bytes_per_sec) / 1000];
    } else {
        return [NSString stringWithFormat:@"%ld B/s", (long)bytes_per_sec];
    }
}

- (NSString *)coderNameWithVdecType:(int)vdec
{
    switch (vdec) {
        case FFP_PROPV_DECODER_VIDEOTOOLBOX:
            return @"VideoToolbox";
        case FFP_PROPV_DECODER_VIDEOTOOLBOX_ASYNC:
            return @"VideoToolbox-Async";
        case FFP_PROPV_DECODER_AVCODEC:
            return [NSString stringWithFormat:@"avcodec %d.%d.%d",
                                 LIBAVCODEC_VERSION_MAJOR,
                                 LIBAVCODEC_VERSION_MINOR,
                                 LIBAVCODEC_VERSION_MICRO];
        case FFP_PROPV_DECODER_AVCODEC_HW:
            return [NSString stringWithFormat:@"avcodec-hw %d.%d.%d",
                                 LIBAVCODEC_VERSION_MAJOR,
                                 LIBAVCODEC_VERSION_MINOR,
                                 LIBAVCODEC_VERSION_MICRO];
        default:
            return @"N/A";
    }
}

- (void)refreshHudView
{
    if (_mediaPlayer == nil)
        return;

    [self setHudValue:_monitor.vdecoder forKey:@"vdec"];
//    int64_t vdec2 = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_ANOTHER_VIDEO_DECODER, FFP_PROPV_DECODER_UNKNOWN);
//    [self setHudValue:[self coderNameWithVdecType:(int)vdec2] forKey:@"vdec-swithing"];
    
    [self setHudValue:[NSString stringWithFormat:@"%d / %.2f", [self dropFrameCount], [self dropFrameRate]] forKey:@"drop-frame(c/r)"];
    
    float vdps = ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_VIDEO_DECODE_FRAMES_PER_SECOND, .0f);
    float vfps = ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_VIDEO_OUTPUT_FRAMES_PER_SECOND, .0f);
    [self setHudValue:[NSString stringWithFormat:@"%.2f / %.2f / %.2f", vdps, vfps, self.fpsInMeta] forKey:@"fps(d/o/f)"];
    int pic_remaining = ijkmp_get_video_frame_cache_remaining(_mediaPlayer);
    [self setHudValue:[NSString stringWithFormat:@"%d", pic_remaining] forKey:@"p-cache"];
    
    int64_t vcacheb = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_CACHED_BYTES, 0);
    int64_t acacheb = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_AUDIO_CACHED_BYTES, 0);
    int64_t vcached = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_CACHED_DURATION, 0);
    int64_t acached = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_AUDIO_CACHED_DURATION, 0);
    int64_t vcachep = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_CACHED_PACKETS, 0);
    int64_t acachep = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_AUDIO_CACHED_PACKETS, 0);
    [self setHudValue:[NSString stringWithFormat:@"%@, %@, %"PRId64" packets",
                          formatedDurationMilli(vcached),
                          formatedSize(vcacheb),
                          vcachep]
                  forKey:@"v-cache"];
    [self setHudValue:[NSString stringWithFormat:@"%@, %@, %"PRId64" packets",
                          formatedDurationMilli(acached),
                          formatedSize(acacheb),
                          acachep]
                  forKey:@"a-cache"];

    float avdelay = ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_AVDELAY, .0f);
    float vmdiff  = ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_VMDIFF, .0f);
    [self setHudValue:[NSString stringWithFormat:@"%.3f %.3f", avdelay, -vmdiff] forKey:@"delay-avdiff"];

    if (self.monitor.httpUrl) {
//#if ! IJK_IO_OFF
//        int64_t bitRate = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_BIT_RATE, 0);
//        [self setHudValue:[NSString stringWithFormat:@"-%@, %@",
//                             formatedSize(_cacheStat.cache_file_forwards),
//                              formatedDurationBytesAndBitrate(_cacheStat.cache_file_forwards, bitRate)] forKey:@"cache-forwards"];
//        [self setHudValue:formatedSize(_cacheStat.cache_physical_pos) forKey:@"cache-physical-pos"];
//        [self setHudValue:formatedSize(_cacheStat.cache_file_pos) forKey:@"cache-file-pos"];
//        [self setHudValue:formatedSize(_cacheStat.cache_count_bytes) forKey:@"cache-bytes"];
//        [self setHudValue:[NSString stringWithFormat:@"-%@, %@",
//                              formatedSize(_asyncStat.buf_backwards),
//                              formatedDurationBytesAndBitrate(_asyncStat.buf_backwards, bitRate)]
//                      forKey:@"async-backward"];
//        [self setHudValue:[NSString stringWithFormat:@"+%@, %@",
//                              formatedSize(_asyncStat.buf_forwards),
//                              formatedDurationBytesAndBitrate(_asyncStat.buf_forwards, bitRate)]
//                      forKey:@"async-forward"];
//#endif
        int64_t tcpSpeed = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_TCP_SPEED, 0);
        [self setHudValue:[NSString stringWithFormat:@"%@", formatedSpeed(tcpSpeed, 1000)]
                   forKey:@"tcp-spd"];
        
        [self setHudValue:formatedDurationMilli(_monitor.prepareDuration) forKey:@"t-prepared"];
        [self setHudValue:formatedDurationMilli(_monitor.firstVideoFrameLatency) forKey:@"t-render"];
        [self setHudValue:formatedDurationMilli(_monitor.lastPrerollDuration) forKey:@"t-preroll"];
        [self setHudValue:[NSString stringWithFormat:@"%@ / %d",
                           formatedDurationMilli(_monitor.lastHttpOpenDuration),
                           _monitor.httpOpenCount]
                   forKey:@"t-http-open"];
//        [self setHudValue:[NSString stringWithFormat:@"%@ / %d",
//                           formatedDurationMilli(_monitor.lastHttpSeekDuration),
//                           _monitor.httpSeekCount]
//                   forKey:@"t-http-seek"];
    }
}

- (void)startHudTimer
{
    if (!_shouldShowHudView)
        return;

    if (_hudTimer != nil)
        return;

    if ([[NSThread currentThread] isMainThread]) {
        UIView *hudView = [_hudCtrl contentView];
        [hudView setHidden:NO];
        CGRect rect = self.view.bounds;
#if TARGET_OS_IOS
        hudView.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleLeftMargin;
        CGFloat screenWidth = [[UIScreen mainScreen]bounds].size.width;
#else
        hudView.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin;
        CGFloat screenWidth = [[NSScreen mainScreen]frame].size.width;
#endif
        rect.size.width = MIN(screenWidth / 3.0, 350);
        rect.origin.x = CGRectGetWidth(self.view.bounds) - rect.size.width;
        hudView.frame = rect;
        [self.view addSubview:hudView];
        
        _hudTimer = [NSTimer scheduledTimerWithTimeInterval:.5f
                                                     target:self
                                                   selector:@selector(refreshHudView)
                                                   userInfo:nil
                                                    repeats:YES];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self startHudTimer];
        });
    }
}

- (void)stopHudTimer
{
    if (_hudTimer == nil)
        return;

    if ([[NSThread currentThread] isMainThread]) {
        UIView *hudView = [_hudCtrl contentView];
        [hudView setHidden:YES];
        [_hudTimer invalidate];
        _hudTimer = nil;
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self stopHudTimer];
        });
    }
}

- (void)destroyHud
{
    if ([[NSThread currentThread] isMainThread]) {
        [_hudCtrl destroyContentView];
        [_hudTimer invalidate];
        _hudTimer = nil;
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self destroyHud];
        });
    }
}

- (void)setShouldShowHudView:(BOOL)shouldShowHudView
{
    if (shouldShowHudView == _shouldShowHudView) {
        return;
    }
    _shouldShowHudView = shouldShowHudView;
    if (shouldShowHudView)
        [self startHudTimer];
    else
        [self stopHudTimer];
}

- (BOOL)shouldShowHudView
{
    return _shouldShowHudView;
}

- (void)setPlaybackRate:(float)playbackRate
{
    if (!_mediaPlayer)
        return;

    return ijkmp_set_playback_rate(_mediaPlayer, playbackRate);
}

- (float)playbackRate
{
    if (!_mediaPlayer)
        return 0.0f;

    return ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_PLAYBACK_RATE, 0.0f);
}

- (void)setPlaybackVolume:(float)volume
{
    if (!_mediaPlayer)
        return;
    return ijkmp_set_playback_volume(_mediaPlayer, volume);
}

- (float)playbackVolume
{
    if (!_mediaPlayer)
        return 0.0f;
    return ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_PLAYBACK_VOLUME, 1.0f);
}

- (int64_t)getFileSize
{
    if (!_mediaPlayer)
        return 0;
    return ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_LOGICAL_FILE_SIZE, 0);
}

- (int64_t)trafficStatistic
{
    if (!_mediaPlayer)
        return 0;
    return ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_TRAFFIC_STATISTIC_BYTE_COUNT, 0);
}

- (float)dropFrameRate
{
    if (!_mediaPlayer)
        return 0;
    return ijkmp_get_property_float(_mediaPlayer, FFP_PROP_FLOAT_DROP_FRAME_RATE, 0.0f);
}

- (int)dropFrameCount
{
    if (!_mediaPlayer)
        return 0;
    return (int)ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_FLOAT_DROP_FRAME_COUNT, 0);
}

inline static void fillMetaInternal(NSMutableDictionary *meta, IjkMediaMeta *rawMeta, const char *name, NSString *defaultValue)
{
    if (!meta || !rawMeta || !name)
        return;

    NSString *key = [NSString stringWithUTF8String:name];
    const char *value = ijkmeta_get_string_l(rawMeta, name);
    if (value) {
        [meta setObject:[NSString stringWithUTF8String:value] forKey:key];
    } else if (defaultValue) {
        [meta setObject:defaultValue forKey:key];
    } else {
        [meta removeObjectForKey:key];
    }
}

- (void) traverseIJKMetaData:(IjkMediaMeta*)rawMeta
{
    if (rawMeta) {
        ijkmeta_lock(rawMeta);

        NSMutableDictionary *newMediaMeta = [[NSMutableDictionary alloc] init];

        fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_FORMAT, nil);
        fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_DURATION_US, nil);
        fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_START_US, nil);
        fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_BITRATE, nil);

        fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_VIDEO_STREAM, nil);
        fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_AUDIO_STREAM, nil);
        fillMetaInternal(newMediaMeta, rawMeta, IJKM_KEY_TIMEDTEXT_STREAM, nil);
        
        int64_t video_stream = ijkmeta_get_int64_l(rawMeta, IJKM_KEY_VIDEO_STREAM, -1);
        int64_t audio_stream = ijkmeta_get_int64_l(rawMeta, IJKM_KEY_AUDIO_STREAM, -1);
        int64_t subtitle_stream = ijkmeta_get_int64_l(rawMeta, IJKM_KEY_TIMEDTEXT_STREAM, -1);
        if (-1 == video_stream) {
            _monitor.videoMeta = nil;
        }
        if (-1 == audio_stream) {
            _monitor.audioMeta = nil;
        }
        if (-1 == subtitle_stream) {
            _monitor.subtitleMeta = nil;
        }
        
        NSMutableArray *streams = [[NSMutableArray alloc] init];

        size_t count = ijkmeta_get_children_count_l(rawMeta);
        for(size_t i = 0; i < count; ++i) {
            IjkMediaMeta *streamRawMeta = ijkmeta_get_child_l(rawMeta, i);
            NSMutableDictionary *streamMeta = [[NSMutableDictionary alloc] init];

            if (streamRawMeta) {
                fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_TYPE, k_IJKM_VAL_TYPE__UNKNOWN);
                const char *type = ijkmeta_get_string_l(streamRawMeta, IJKM_KEY_TYPE);
                if (type) {
                    fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_CODEC_NAME, nil);
                    fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_CODEC_PROFILE, nil);
                    fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_CODEC_LONG_NAME, nil);
                    fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_BITRATE, nil);
                    fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_STREAM_IDX, nil);
                    if (0 == strcmp(type, IJKM_VAL_TYPE__VIDEO)) {
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_WIDTH, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_HEIGHT, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_FPS_NUM, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_FPS_DEN, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_TBR_NUM, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_TBR_DEN, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_SAR_NUM, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_SAR_DEN, nil);

                        if (video_stream == i) {
                            _monitor.videoMeta = streamMeta;

                            int64_t fps_num = ijkmeta_get_int64_l(streamRawMeta, IJKM_KEY_FPS_NUM, 0);
                            int64_t fps_den = ijkmeta_get_int64_l(streamRawMeta, IJKM_KEY_FPS_DEN, 0);
                            if (fps_num > 0 && fps_den > 0) {
                                _fpsInMeta = ((CGFloat)(fps_num)) / fps_den;
                                NSLog(@"fps in meta %f\n", _fpsInMeta);
                            }
                        }

                    } else if (0 == strcmp(type, IJKM_VAL_TYPE__AUDIO)) {
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_SAMPLE_RATE, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_CHANNEL_LAYOUT, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_LANGUAGE, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_TITLE, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_ARTIST, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_ALBUM, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_TYER, nil);
                        if (audio_stream == i) {
                            _monitor.audioMeta = streamMeta;
                        }
                    } else if (0 == strcmp(type, IJKM_VAL_TYPE__TIMEDTEXT)) {
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_LANGUAGE, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_TITLE, nil);
                        fillMetaInternal(streamMeta, streamRawMeta, IJKM_KEY_EX_SUBTITLE_URL, nil);
                        if (subtitle_stream == i) {
                            _monitor.subtitleMeta = streamMeta;
                        }
                    }
                }
            }

            [streams addObject:streamMeta];
        }

        [newMediaMeta setObject:streams forKey:kk_IJKM_KEY_STREAMS];

        ijkmeta_unlock(rawMeta);
        _monitor.mediaMeta = newMediaMeta;
    }
}

- (void)updateMonitor4VideoDecoder:(int64_t)vdec
{
    _monitor.vdecoder = [self coderNameWithVdecType:(int)vdec];
}

- (NSString *)averrToString:(int)errnum
{
    char errbuf[128] = { '\0' };
    const char *errbuf_ptr = errbuf;

    if (av_strerror(errnum, errbuf, sizeof(errbuf)) < 0) {
        errbuf_ptr = strerror(AVUNERROR(errnum));
    }
    return [[NSString alloc] initWithUTF8String:errbuf];
}

- (void)postEvent: (IJKFFMoviePlayerMessage *)msg
{
    if (!msg)
        return;

    AVMessage *avmsg = msg.msg;
    switch (avmsg->what) {
        case FFP_MSG_FLUSH:
            break;
        case FFP_MSG_ERROR: {
            [self setScreenOn:NO];

            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
             object:self];
            
            [[NSNotificationCenter defaultCenter]
                postNotificationName:IJKMPMoviePlayerPlaybackDidFinishNotification
                object:self
                userInfo:@{
                    IJKMPMoviePlayerPlaybackDidFinishReasonUserInfoKey: @(IJKMPMovieFinishReasonPlaybackError),
                    @"msg":[self averrToString:avmsg->arg1],@"code": @(avmsg->arg1)}];
            break;
        }
        case FFP_MSG_SELECTED_STREAM_CHANGED:  {//stream changed msg
            IjkMediaMeta *rawMeta = ijkmp_get_meta_l(_mediaPlayer);
            [self traverseIJKMetaData:rawMeta];
            
            [[NSNotificationCenter defaultCenter] postNotificationName:IJKMPMoviePlayerSelectedStreamDidChangeNotification object:self];
            break;
        }
        case FFP_MSG_PREPARED: {
            _monitor.prepareDuration = (int64_t)SDL_GetTickHR() - _monitor.prepareStartTick;
            //prepared not send,beacuse FFP_MSG_VIDEO_DECODER_OPEN event already send
            //int64_t vdec = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_DECODER, FFP_PROPV_DECODER_UNKNOWN);
            //[self updateMonitor4VideoDecoder:vdec];

            IjkMediaMeta *rawMeta = ijkmp_get_meta_l(_mediaPlayer);
            [self traverseIJKMetaData:rawMeta];
            
            ijkmp_set_playback_rate(_mediaPlayer, [self playbackRate]);
            ijkmp_set_playback_volume(_mediaPlayer, [self playbackVolume]);

            [self startHudTimer];
            _isPreparedToPlay = YES;

            [[NSNotificationCenter defaultCenter] postNotificationName:IJKMPMediaPlaybackIsPreparedToPlayDidChangeNotification object:self];
            _loadState = IJKMPMovieLoadStatePlayable | IJKMPMovieLoadStatePlaythroughOK;

            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerLoadStateDidChangeNotification
             object:self];

            break;
        }
        case FFP_MSG_COMPLETED: {

            [self setScreenOn:NO];

            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
             object:self];

            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerPlaybackDidFinishNotification
             object:self
             userInfo:@{IJKMPMoviePlayerPlaybackDidFinishReasonUserInfoKey: @(IJKMPMovieFinishReasonPlaybackEnded)}];
            break;
        }
        case FFP_MSG_VIDEO_SIZE_CHANGED:
            if (avmsg->arg1 > 0)
                _videoWidth = avmsg->arg1;
            if (avmsg->arg2 > 0)
                _videoHeight = avmsg->arg2;
            [self changeNaturalSize];
            break;
        case FFP_MSG_SAR_CHANGED:
            if (avmsg->arg1 > 0)
                _sampleAspectRatioNumerator = avmsg->arg1;
            if (avmsg->arg2 > 0)
                _sampleAspectRatioDenominator = avmsg->arg2;
            [self changeNaturalSize];
            break;
        case FFP_MSG_BUFFERING_START: {
            _monitor.lastPrerollStartTick = (int64_t)SDL_GetTickHR();

            _loadState = IJKMPMovieLoadStateStalled;
            _isSeekBuffering = avmsg->arg1;

            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerLoadStateDidChangeNotification
             object:self];
            _isSeekBuffering = 0;
            break;
        }
        case FFP_MSG_BUFFERING_END: {
            _monitor.lastPrerollDuration = (int64_t)SDL_GetTickHR() - _monitor.lastPrerollStartTick;

            _loadState = IJKMPMovieLoadStatePlayable | IJKMPMovieLoadStatePlaythroughOK;
            _isSeekBuffering = avmsg->arg1;

            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerLoadStateDidChangeNotification
             object:self];
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
             object:self];
            _isSeekBuffering = 0;
            break;
        }
        case FFP_MSG_BUFFERING_UPDATE:
            _bufferingPosition = avmsg->arg1;
            _bufferingProgress = avmsg->arg2;
            // NSLog(@"FFP_MSG_BUFFERING_UPDATE: %d, %%%d\n", _bufferingPosition, _bufferingProgress);
            break;
        case FFP_MSG_BUFFERING_BYTES_UPDATE:
            // NSLog(@"FFP_MSG_BUFFERING_BYTES_UPDATE: %d\n", avmsg->arg1);
            break;
        case FFP_MSG_BUFFERING_TIME_UPDATE:
            _bufferingTime       = avmsg->arg1;
            // NSLog(@"FFP_MSG_BUFFERING_TIME_UPDATE: %d\n", avmsg->arg1);
            break;
        case FFP_MSG_PLAYBACK_STATE_CHANGED:
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerPlaybackStateDidChangeNotification
             object:self];
            break;
        case FFP_MSG_SEEK_COMPLETE: {
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerDidSeekCompleteNotification
             object:self
             userInfo:@{IJKMPMoviePlayerDidSeekCompleteTargetKey: @(avmsg->arg1),
                        IJKMPMoviePlayerDidSeekCompleteErrorKey: @(avmsg->arg2)}];
            _seeking = NO;
            break;
        }
        case FFP_MSG_VIDEO_DECODER_OPEN: {
            _isVideoToolboxOpen = avmsg->arg1 > 0;
            int64_t vdec = ijkmp_get_property_int64(_mediaPlayer, FFP_PROP_INT64_VIDEO_DECODER, FFP_PROPV_DECODER_UNKNOWN);
            [self updateMonitor4VideoDecoder:vdec];
            
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerVideoDecoderOpenNotification
             object:self];
            break;
        }
        case FFP_MSG_VIDEO_RENDERING_START: {
            _monitor.firstVideoFrameLatency = (int64_t)SDL_GetTickHR() - _monitor.prepareStartTick;
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerFirstVideoFrameRenderedNotification
             object:self];
            break;
        }
        case FFP_MSG_AUDIO_RENDERING_START: {
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerFirstAudioFrameRenderedNotification
             object:self];
            break;
        }
        case FFP_MSG_AUDIO_DECODED_START: {
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerFirstAudioFrameDecodedNotification
             object:self];
            break;
        }
        case FFP_MSG_VIDEO_DECODED_START: {
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerFirstVideoFrameDecodedNotification
             object:self];
            break;
        }
        case FFP_MSG_OPEN_INPUT: {
            const char *name = avmsg->obj;
            NSString *str = nil;
            if (name) {
                str = [[NSString alloc] initWithUTF8String:name];
            }
            if (!str) {
                str = @"";
            }
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerOpenInputNotification
             object:self
             userInfo:@{@"name": str}];
            break;
        }
        case FFP_MSG_FIND_STREAM_INFO: {
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerFindStreamInfoNotification
             object:self];
            break;
        }
        case FFP_MSG_COMPONENT_OPEN: {
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerComponentOpenNotification
             object:self];
            break;
        }
        case FFP_MSG_ACCURATE_SEEK_COMPLETE: {
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerAccurateSeekCompleteNotification
             object:self
             userInfo:@{IJKMPMoviePlayerDidAccurateSeekCompleteCurPos: @(avmsg->arg1)}];
            break;
        }
        case FFP_MSG_VIDEO_SEEK_RENDERING_START: {
            _isVideoSync = avmsg->arg1;
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerSeekVideoStartNotification
             object:self];
            _isVideoSync = 0;
            break;
        }
        case FFP_MSG_AUDIO_SEEK_RENDERING_START: {
            _isAudioSync = avmsg->arg1;
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerSeekAudioStartNotification
             object:self];
            _isAudioSync = 0;
            break;
        }
        case FFP_MSG_VIDEO_Z_ROTATE_DEGREE:
            if (_videoZRotateDegrees != avmsg->arg1) {
                _videoZRotateDegrees = avmsg->arg1;
                
                if ([self.view respondsToSelector:@selector(videoZRotateDegrees:)]) {
                    [self.view videoZRotateDegrees:_videoZRotateDegrees];
                }
                
                [[NSNotificationCenter defaultCenter]
                         postNotificationName:IJKMPMovieZRotateAvailableNotification
                         object:self userInfo:@{@"degrees":@(_videoZRotateDegrees)}];
            }
            break;
        case FFP_MSG_NO_CODEC_FOUND: {
            NSString *name = [NSString stringWithCString:avcodec_get_name(avmsg->arg1) encoding:NSUTF8StringEncoding];
            [[NSNotificationCenter defaultCenter]
                     postNotificationName:IJKMPMovieNoCodecFoundNotification
             object:self userInfo:@{@"codecName":name}];
            break;
        }
        case FFP_MSG_AFTER_SEEK_FIRST_FRAME: {
            int du = avmsg->arg1;
            [[NSNotificationCenter defaultCenter]
             postNotificationName:IJKMPMoviePlayerAfterSeekFirstVideoFrameDisplayNotification
             object:self userInfo:@{@"du" : @(du)}];
            break;
        }
        default:
            // NSLog(@"unknown FFP_MSG_xxx(%d)\n", avmsg->what);
            break;
    }

    [_msgPool recycle:msg];
}

- (IJKFFMoviePlayerMessage *) obtainMessage {
    return [_msgPool obtain];
}

inline static IJKFFMoviePlayerController *ffplayerRetain(void *arg) {
    return (__bridge_transfer IJKFFMoviePlayerController *) arg;
}

static int media_player_msg_loop(void* arg)
{
    @autoreleasepool {
        IjkMediaPlayer *mp = (IjkMediaPlayer*)arg;
        __weak IJKFFMoviePlayerController *ffpController = ffplayerRetain(ijkmp_set_weak_thiz(mp, NULL));
        while (ffpController) {
            @autoreleasepool {
                IJKFFMoviePlayerMessage *msg = [ffpController obtainMessage];
                if (!msg)
                    break;

                int retval = ijkmp_get_msg(mp, msg.msg, 1);
                if (retval < 0)
                    break;

                // block-get should never return 0
                assert(retval > 0);
                [ffpController performSelectorOnMainThread:@selector(postEvent:) withObject:msg waitUntilDone:NO];
            }
        }

        // retained in prepare_async, before SDL_CreateThreadEx
        ijkmp_dec_ref_p(&mp);
        return 0;
    }
}

#if ! IJK_IO_OFF

#pragma mark av_format_control_message

static int onInjectIOControl(IJKFFMoviePlayerController *mpc, id<IJKMediaUrlOpenDelegate> delegate, int type, void *data, size_t data_size)
{
    AVAppIOControl *realData = data;
    assert(realData);
    assert(sizeof(AVAppIOControl) == data_size);
    realData->is_handled     = NO;
    realData->is_url_changed = NO;

    if (delegate == nil)
        return 0;

    NSString *urlString = [NSString stringWithUTF8String:realData->url];

    IJKMediaUrlOpenData *openData =
    [[IJKMediaUrlOpenData alloc] initWithUrl:urlString
                                       event:(IJKMediaEvent)type
                                segmentIndex:realData->segment_index
                                retryCounter:realData->retry_counter];

    [delegate willOpenUrl:openData];
    if (openData.error < 0)
        return -1;

    if (openData.isHandled) {
        realData->is_handled = YES;
        if (openData.isUrlChanged && openData.url != nil) {
            realData->is_url_changed = YES;
            const char *newUrlUTF8 = [openData.url UTF8String];
            strlcpy(realData->url, newUrlUTF8, sizeof(realData->url));
            realData->url[sizeof(realData->url) - 1] = 0;
        }
    }
    
    return 0;
}

static int onInjectTcpIOControl(IJKFFMoviePlayerController *mpc, id<IJKMediaUrlOpenDelegate> delegate, int type, void *data, size_t data_size)
{
    AVAppTcpIOControl *realData = data;
    assert(realData);
    assert(sizeof(AVAppTcpIOControl) == data_size);

    switch (type) {
        case IJKMediaCtrl_WillTcpOpen:

            break;
        case IJKMediaCtrl_DidTcpOpen:
            mpc->_monitor.tcpError = realData->error;
            mpc->_monitor.remoteIp = [NSString stringWithUTF8String:realData->ip];
            [mpc setHudValue: mpc->_monitor.remoteIp forKey:@"ip"];
            break;
        default:
            assert(!"unexcepted type for tcp io control");
            break;
    }

    if (delegate == nil)
        return 0;

    NSString *urlString = [NSString stringWithUTF8String:realData->ip];

    IJKMediaUrlOpenData *openData =
    [[IJKMediaUrlOpenData alloc] initWithUrl:urlString
                                       event:(IJKMediaEvent)type
                                segmentIndex:0
                                retryCounter:0];
    openData.fd = realData->fd;

    [delegate willOpenUrl:openData];
    if (openData.error < 0)
        return -1;
    [mpc setHudValue: [NSString stringWithFormat:@"fd:%d %@", openData.fd, openData.msg?:@"unknown"] forKey:@"tcp-info"];
    return 0;
}

static int onInjectAsyncStatistic(IJKFFMoviePlayerController *mpc, int type, void *data, size_t data_size)
{
    AVAppAsyncStatistic *realData = data;
    assert(realData);
    assert(sizeof(AVAppAsyncStatistic) == data_size);

    mpc->_asyncStat = *realData;
    return 0;
}

static int onInectIJKIOStatistic(IJKFFMoviePlayerController *mpc, int type, void *data, size_t data_size)
{
    IjkIOAppCacheStatistic *realData = data;
    assert(realData);
    assert(sizeof(IjkIOAppCacheStatistic) == data_size);

    mpc->_cacheStat = *realData;
    return 0;
}

static int64_t calculateElapsed(int64_t begin, int64_t end)
{
    if (begin <= 0)
        return -1;

    if (end < begin)
        return -1;

    return end - begin;
}

static int onInjectOnHttpEvent(IJKFFMoviePlayerController *mpc, int type, void *data, size_t data_size)
{
    AVAppHttpEvent *realData = data;
    assert(realData);
    assert(sizeof(AVAppHttpEvent) == data_size);

    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    NSURL        *nsurl   = nil;
    IJKFFMonitor *monitor = mpc->_monitor;
    NSString     *url  = monitor.httpUrl;
    NSString     *host = monitor.httpHost;
    int64_t       elapsed = 0;

    id<IJKMediaNativeInvokeDelegate> delegate = mpc.nativeInvokeDelegate;

    switch (type) {
        case AVAPP_EVENT_WILL_HTTP_OPEN:
            url   = [NSString stringWithUTF8String:realData->url];
            nsurl = [NSURL URLWithString:url];
            host  = nsurl.host;

            monitor.httpUrl      = url;
            monitor.httpHost     = host;
            monitor.httpOpenTick = SDL_GetTickHR();
            [mpc setHudUrl:url];

            if (delegate != nil) {
                dict[IJKMediaEventAttrKey_host]         = [NSString ijk_stringBeEmptyIfNil:host];
                dict[IJKMediaEventAttrKey_url]          = [NSString ijk_stringBeEmptyIfNil:monitor.httpUrl];
                [delegate invoke:type attributes:dict];
            }
            break;
        case AVAPP_EVENT_DID_HTTP_OPEN:
            elapsed = calculateElapsed(monitor.httpOpenTick, SDL_GetTickHR());
            monitor.httpError = realData->error;
            monitor.httpCode  = realData->http_code;
            monitor.filesize  = realData->filesize;
            monitor.httpOpenCount++;
            monitor.httpOpenTick = 0;
            monitor.lastHttpOpenDuration = elapsed;
            [mpc setHudValue:@(realData->http_code).stringValue forKey:@"http"];

            if (delegate != nil) {
                dict[IJKMediaEventAttrKey_time_of_event]    = @(elapsed).stringValue;
                dict[IJKMediaEventAttrKey_url]              = [NSString ijk_stringBeEmptyIfNil:monitor.httpUrl];
                dict[IJKMediaEventAttrKey_host]             = [NSString ijk_stringBeEmptyIfNil:host];
                dict[IJKMediaEventAttrKey_error]            = @(realData->error).stringValue;
                dict[IJKMediaEventAttrKey_http_code]        = @(realData->http_code).stringValue;
                dict[IJKMediaEventAttrKey_file_size]        = @(realData->filesize).stringValue;
                [delegate invoke:type attributes:dict];
            }
            break;
        case AVAPP_EVENT_WILL_HTTP_SEEK:
            monitor.httpSeekTick = SDL_GetTickHR();

            if (delegate != nil) {
                dict[IJKMediaEventAttrKey_host]         = [NSString ijk_stringBeEmptyIfNil:host];
                dict[IJKMediaEventAttrKey_offset]       = @(realData->offset).stringValue;
                [delegate invoke:type attributes:dict];
            }
            break;
        case AVAPP_EVENT_DID_HTTP_SEEK:
            elapsed = calculateElapsed(monitor.httpSeekTick, SDL_GetTickHR());
            monitor.httpError = realData->error;
            monitor.httpCode  = realData->http_code;
            monitor.httpSeekCount++;
            monitor.httpSeekTick = 0;
            monitor.lastHttpSeekDuration = elapsed;
            [mpc setHudValue:@(realData->http_code).stringValue forKey:@"http"];

            if (delegate != nil) {
                dict[IJKMediaEventAttrKey_time_of_event]    = @(elapsed).stringValue;
                dict[IJKMediaEventAttrKey_url]              = [NSString ijk_stringBeEmptyIfNil:monitor.httpUrl];
                dict[IJKMediaEventAttrKey_host]             = [NSString ijk_stringBeEmptyIfNil:host];
                dict[IJKMediaEventAttrKey_offset]           = @(realData->offset).stringValue;
                dict[IJKMediaEventAttrKey_error]            = @(realData->error).stringValue;
                dict[IJKMediaEventAttrKey_http_code]        = @(realData->http_code).stringValue;
                [delegate invoke:type attributes:dict];
            }
            break;
    }

    return 0;
}

// NOTE: could be called from multiple thread
static int ijkff_inject_callback(void *opaque, int message, void *data, size_t data_size)
{
    IJKWeakHolder *weakHolder = (__bridge IJKWeakHolder*)opaque;
    IJKFFMoviePlayerController *mpc = weakHolder.object;
    if (!mpc)
        return 0;

    switch (message) {
        case AVAPP_CTRL_WILL_CONCAT_SEGMENT_OPEN:
            return onInjectIOControl(mpc, mpc.segmentOpenDelegate, message, data, data_size);
        case AVAPP_CTRL_WILL_TCP_OPEN:
            return onInjectTcpIOControl(mpc, mpc.tcpOpenDelegate, message, data, data_size);
        case AVAPP_CTRL_WILL_HTTP_OPEN:
            return onInjectIOControl(mpc, mpc.httpOpenDelegate, message, data, data_size);
        case AVAPP_CTRL_WILL_LIVE_OPEN:
            return onInjectIOControl(mpc, mpc.liveOpenDelegate, message, data, data_size);
        case AVAPP_EVENT_ASYNC_STATISTIC:
            return onInjectAsyncStatistic(mpc, message, data, data_size);
        case IJKIOAPP_EVENT_CACHE_STATISTIC:
            return onInectIJKIOStatistic(mpc, message, data, data_size);
        case AVAPP_CTRL_DID_TCP_OPEN:
            return onInjectTcpIOControl(mpc, mpc.tcpOpenDelegate, message, data, data_size);
        case AVAPP_EVENT_WILL_HTTP_OPEN:
        case AVAPP_EVENT_DID_HTTP_OPEN:
        case AVAPP_EVENT_WILL_HTTP_SEEK:
        case AVAPP_EVENT_DID_HTTP_SEEK:
            return onInjectOnHttpEvent(mpc, message, data, data_size);
        default: {
            return 0;
        }
    }
}

#endif
#pragma mark Airplay

-(BOOL)allowsMediaAirPlay
{
    if (!self)
        return NO;
    return _allowsMediaAirPlay;
}

-(void)setAllowsMediaAirPlay:(BOOL)b
{
    if (!self)
        return;
    _allowsMediaAirPlay = b;
}

-(BOOL)airPlayMediaActive
{
    if (!self)
        return NO;
    if (_isDanmakuMediaAirPlay) {
        return YES;
    }
    return NO;
}

-(BOOL)isDanmakuMediaAirPlay
{
    return _isDanmakuMediaAirPlay;
}

-(void)setIsDanmakuMediaAirPlay:(BOOL)isDanmakuMediaAirPlay
{
    _isDanmakuMediaAirPlay = isDanmakuMediaAirPlay;

#if TARGET_OS_IOS
    if (_isDanmakuMediaAirPlay) {
        _glView.scaleFactor = 1.0f;
    } else {

        CGFloat scale = [[UIScreen mainScreen] scale];
        if (scale < 0.1f)
            scale = 1.0f;
        _glView.scaleFactor = scale;
    }
#endif
     [[NSNotificationCenter defaultCenter] postNotificationName:IJKMPMoviePlayerIsAirPlayVideoActiveDidChangeNotification object:nil userInfo:nil];
}


#pragma mark Option Conventionce

- (void)setFormatOptionValue:(NSString *)value forKey:(NSString *)key
{
    [self setOptionValue:value forKey:key ofCategory:kIJKFFOptionCategoryFormat];
}

- (void)setCodecOptionValue:(NSString *)value forKey:(NSString *)key
{
    [self setOptionValue:value forKey:key ofCategory:kIJKFFOptionCategoryCodec];
}

- (void)setSwsOptionValue:(NSString *)value forKey:(NSString *)key
{
    [self setOptionValue:value forKey:key ofCategory:kIJKFFOptionCategorySws];
}

- (void)setPlayerOptionValue:(NSString *)value forKey:(NSString *)key
{
    [self setOptionValue:value forKey:key ofCategory:kIJKFFOptionCategoryPlayer];
}

- (void)setFormatOptionIntValue:(int64_t)value forKey:(NSString *)key
{
    [self setOptionIntValue:value forKey:key ofCategory:kIJKFFOptionCategoryFormat];
}

- (void)setCodecOptionIntValue:(int64_t)value forKey:(NSString *)key
{
    [self setOptionIntValue:value forKey:key ofCategory:kIJKFFOptionCategoryCodec];
}

- (void)setSwsOptionIntValue:(int64_t)value forKey:(NSString *)key
{
    [self setOptionIntValue:value forKey:key ofCategory:kIJKFFOptionCategorySws];
}

- (void)setPlayerOptionIntValue:(int64_t)value forKey:(NSString *)key
{
    [self setOptionIntValue:value forKey:key ofCategory:kIJKFFOptionCategoryPlayer];
}

- (void)setMaxBufferSize:(int)maxBufferSize
{
    [self setPlayerOptionIntValue:maxBufferSize forKey:@"max-buffer-size"];
}

#if TARGET_OS_IOS
#pragma mark app state changed

- (void)registerApplicationObservers
{
    [_notificationManager addObserver:self
                             selector:@selector(audioSessionInterrupt:)
                                 name:AVAudioSessionInterruptionNotification
                               object:nil];

    [_notificationManager addObserver:self
                             selector:@selector(applicationWillResignActive)
                                 name:UIApplicationWillResignActiveNotification
                               object:nil];

    [_notificationManager addObserver:self
                             selector:@selector(applicationDidEnterBackground)
                                 name:UIApplicationDidEnterBackgroundNotification
                               object:nil];

    [_notificationManager addObserver:self
                             selector:@selector(applicationWillTerminate)
                                 name:UIApplicationWillTerminateNotification
                               object:nil];
}

- (void)unregisterApplicationObservers
{
    [_notificationManager removeAllObservers:self];
}

- (void)audioSessionInterrupt:(NSNotification *)notification
{
    int reason = [[[notification userInfo] valueForKey:AVAudioSessionInterruptionTypeKey] intValue];
    switch (reason) {
        case AVAudioSessionInterruptionTypeBegan: {
            NSLog(@"IJKFFMoviePlayerController:audioSessionInterrupt: begin\n");
            switch (self.playbackState) {
                case IJKMPMoviePlaybackStatePaused:
                case IJKMPMoviePlaybackStateStopped:
                    _playingBeforeInterruption = NO;
                    break;
                default:
                    _playingBeforeInterruption = YES;
                    break;
            }
            [self pause];
            [[IJKAudioKit sharedInstance] setActive:NO];
            break;
        }
        case AVAudioSessionInterruptionTypeEnded: {
            NSLog(@"IJKFFMoviePlayerController:audioSessionInterrupt: end\n");
            [[IJKAudioKit sharedInstance] setActive:YES];
            if (_playingBeforeInterruption) {
                [self play];
            }
            break;
        }
    }
}

- (void)applicationWillResignActive
{
    NSLog(@"IJKFFMoviePlayerController:applicationWillResignActive: %d", (int)[UIApplication sharedApplication].applicationState);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_pauseInBackground) {
            [self pause];
        }
    });
}

- (void)applicationDidEnterBackground
{
    NSLog(@"IJKFFMoviePlayerController:applicationDidEnterBackground: %d", (int)[UIApplication sharedApplication].applicationState);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_pauseInBackground) {
            [self pause];
        }
    });
}

- (void)applicationWillTerminate
{
    NSLog(@"IJKFFMoviePlayerController:applicationWillTerminate: %d", (int)[UIApplication sharedApplication].applicationState);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_pauseInBackground) {
            [self pause];
        }
    });
}
#endif

- (void)exchangeSelectedStream:(int)streamIdx
{
    if (_mediaPlayer) {
        ijkmp_set_stream_selected(_mediaPlayer,streamIdx,1);
        //TODO: 通过seek解决切换字幕后不能立马显示问题
        long pst = ijkmp_get_current_position(_mediaPlayer);
        ijkmp_seek_to(_mediaPlayer, pst);
    }
}

- (void)closeCurrentStream:(NSString *)streamType
{
    NSDictionary *dic = self.monitor.mediaMeta;
    if (dic[streamType] != nil) {
        int streamIdx = [dic[streamType] intValue];
        if (streamIdx > -1) {
             ijkmp_set_stream_selected(_mediaPlayer,streamIdx,0);
        }
    }
}

- (void)updateSubtitleExtraDelay:(const float)delay
{
    ijkmp_set_subtitle_extra_delay(_mediaPlayer, delay);
}

- (float)currentSubtitleExtraDelay
{
    return ijkmp_get_subtitle_extra_delay(_mediaPlayer);
}

- (int)exchangeVideoDecoder
{
    return ijkmp_exchange_video_decoder(_mediaPlayer);
}

@end
