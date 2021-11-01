//
//  IJKMediaPlayeriOSKit.h
//  IJKMediaPlayeriOSKit
//
//  Created by Justin Qian on 2021/9/30.
//

#ifndef IJKMediaPlayeriOSKit_h
#define IJKMediaPlayeriOSKit_h


#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

//! Project version number for IJKMediaMacFramework.
FOUNDATION_EXPORT double IJKMediaMacFrameworkVersionNumber;

//! Project version string for IJKMediaMacFramework.
FOUNDATION_EXPORT const unsigned char IJKMediaMacFrameworkVersionString[];

// In this header, you should import all the public headers of your framework using statements like #import <IJKMediaMacFramework/PublicHeader.h>

#import <IJKMediaPlayeriOSKit/IJKMediaPlayback.h>
#import <IJKMediaPlayeriOSKit/IJKFFMonitor.h>
#import <IJKMediaPlayeriOSKit/IJKFFOptions.h>
#import <IJKMediaPlayeriOSKit/IJKFFMoviePlayerController.h>
#import <IJKMediaPlayeriOSKit/IJKMediaModule.h>
#import <IJKMediaPlayeriOSKit/IJKMediaPlayer.h>
#import <IJKMediaPlayeriOSKit/IJKNotificationManager.h>
#import <IJKMediaPlayeriOSKit/IJKKVOController.h>
#import <IJKMediaPlayeriOSKit/IJKSDLGLViewProtocol.h>

// backward compatible for old names
#define IJKMediaPlaybackIsPreparedToPlayDidChangeNotification IJKMPMediaPlaybackIsPreparedToPlayDidChangeNotification
#define IJKMoviePlayerLoadStateDidChangeNotification IJKMPMoviePlayerLoadStateDidChangeNotification
#define IJKMoviePlayerPlaybackDidFinishNotification IJKMPMoviePlayerPlaybackDidFinishNotification
#define IJKMoviePlayerPlaybackDidFinishReasonUserInfoKey IJKMPMoviePlayerPlaybackDidFinishReasonUserInfoKey
#define IJKMoviePlayerPlaybackStateDidChangeNotification IJKMPMoviePlayerPlaybackStateDidChangeNotification
#define IJKMoviePlayerIsAirPlayVideoActiveDidChangeNotification IJKMPMoviePlayerIsAirPlayVideoActiveDidChangeNotification
#define IJKMoviePlayerVideoDecoderOpenNotification IJKMPMoviePlayerVideoDecoderOpenNotification
#define IJKMoviePlayerFirstVideoFrameRenderedNotification IJKMPMoviePlayerFirstVideoFrameRenderedNotification
#define IJKMoviePlayerFirstAudioFrameRenderedNotification IJKMPMoviePlayerFirstAudioFrameRenderedNotification
#define IJKMoviePlayerFirstAudioFrameDecodedNotification IJKMPMoviePlayerFirstAudioFrameDecodedNotification
#define IJKMoviePlayerFirstVideoFrameDecodedNotification IJKMPMoviePlayerFirstVideoFrameDecodedNotification
#define IJKMoviePlayerOpenInputNotification IJKMPMoviePlayerOpenInputNotification
#define IJKMoviePlayerFindStreamInfoNotification IJKMPMoviePlayerFindStreamInfoNotification
#define IJKMoviePlayerComponentOpenNotification IJKMPMoviePlayerComponentOpenNotification
#define IJKMPMoviePlayerAccurateSeekCompleteNotification IJKMPMoviePlayerAccurateSeekCompleteNotification
#define IJKMoviePlayerSeekAudioStartNotification IJKMPMoviePlayerSeekAudioStartNotification
#define IJKMoviePlayerSeekVideoStartNotification IJKMPMoviePlayerSeekVideoStartNotification

#endif /* IJKMediaPlayeriOSKit_h */