//
//  RootViewController.m
//  IJKMediaMacDemo
//
//  Created by Matt Reach on 2021/11/1.
//  Copyright © 2021 IJK Mac. All rights reserved.
//

#import "RootViewController.h"
#import "MRDragView.h"
#import "MRUtil+SystemPanel.h"
#import <IJKMediaPlayerKit/IJKMediaPlayerKit.h>
#import <Carbon/Carbon.h>
#import "NSFileManager+Sandbox.h"
#import "SHBaseView.h"
#import <Quartz/Quartz.h>
#import "MRGlobalNotification.h"
#import "AppDelegate.h"
#import "MRProgressSlider.h"
#import "MRBaseView.h"
#import <IOKit/pwr_mgt/IOPMLib.h>

static NSString* lastPlayedKey = @"__lastPlayedKey";

@interface RootViewController ()<MRDragViewDelegate,SHBaseViewDelegate,NSMenuDelegate>
{
    FILE *my_stderr;
    FILE *my_stdout;
}
@property (weak) IBOutlet NSView *moreView;
@property (weak) IBOutlet NSLayoutConstraint *moreViewBottomCons;
@property (assign) BOOL isMoreViewAnimating;

@property (weak) IBOutlet MRBaseView *playerCtrlPanel;

@property (strong) IJKFFMoviePlayerController * player;
@property (strong) IJKKVOController * kvoCtrl;

@property (weak) IBOutlet NSTextField *playedTimeLb;
@property (weak) IBOutlet NSTextField *durationTimeLb;

@property (weak) IBOutlet NSButton *playCtrlBtn;
@property (weak) IBOutlet MRProgressSlider *playerSlider;

@property (nonatomic, strong) NSMutableArray *playList;
@property (copy) NSURL *playingUrl;
@property (weak) NSTimer *tickTimer;

@property (weak) IBOutlet NSPopUpButton *subtitlePopUpBtn;
@property (weak) IBOutlet NSPopUpButton *audioPopUpBtn;
@property (weak) IBOutlet NSPopUpButton *videoPopUpBtn;
@property (weak) NSTrackingArea *trackingArea;

//for cocoa binding begin
@property (assign) float volume;
@property (assign) float subtitleFontRatio;
@property (assign) float subtitleDelay;
@property (assign) float subtitleMargin;

@property (assign) float brightness;
@property (assign) float saturation;
@property (assign) float contrast;

@property (assign) int useVideoToolBox;
@property (assign) int useAsyncVTB;
@property (copy) NSString *fcc;
@property (assign) int snapshot;

//for cocoa binding end

@property (assign) BOOL seeking;
@property (weak) id eventMonitor;

@property (assign) BOOL autoTest;
//
@property (assign) BOOL autoSeeked;
@property (assign) BOOL snapshot2;
@property (assign) int tickCount;

@end

@implementation RootViewController

- (void)dealloc
{
    if (self.tickTimer) {
        [self.tickTimer invalidate];
        self.tickTimer = nil;
        self.tickCount = 0;
    }
    
    [NSEvent removeMonitor:self.eventMonitor];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do view setup here.
    
    //for debug
    //[self.view setWantsLayer:YES];
    //self.view.layer.backgroundColor = [[NSColor redColor] CGColor];
    
    [self.moreView setWantsLayer:YES];
    //self.ctrlView.layer.backgroundColor = [[NSColor colorWithWhite:0.2 alpha:0.5] CGColor];
    self.moreView.layer.cornerRadius = 4;
    self.moreView.layer.masksToBounds = YES;

    self.subtitleFontRatio = 1.0;
    self.subtitleMargin = 0.7;
    self.fcc = @"fcc-_es2";
    self.useAsyncVTB = 0;
    self.useVideoToolBox = 2;
    self.snapshot = 3;
    self.volume = 0.4;
    [self onReset:nil];
    [self reSetLoglevel:@"info"];
    
    [self.playList addObject:[NSURL URLWithString:@"https://kvideo01.youju.sohu.com/f559f6ad-df2f-42c9-9f47-841cf6e4086f1_0_0.mp4"]];
    
    NSArray *bundleNameArr = @[@"5003509-693880-3.m3u8",@"996747-5277368-31.m3u8"];
    
    for (NSString *fileName in bundleNameArr) {
        NSString *localM3u8 = [[NSBundle mainBundle] pathForResource:[fileName stringByDeletingPathExtension] ofType:[fileName pathExtension]];
        [self.playList addObject:[NSURL fileURLWithPath:localM3u8]];
    }
    [self.playList addObject:[NSURL URLWithString:@"https://data.vod.itc.cn/?new=/73/15/oFed4wzSTZe8HPqHZ8aF7J.mp4&vid=77972299&plat=14&mkey=XhSpuZUl_JtNVIuSKCB05MuFBiqUP7rB&ch=null&user=api&qd=8001&cv=3.13&uid=F45C89AE5BC3&ca=2&pg=5&pt=1&prod=ifox"]];
    
    if ([self.view isKindOfClass:[SHBaseView class]]) {
        SHBaseView *baseView = (SHBaseView *)self.view;
        baseView.delegate = self;
        baseView.needTracking = YES;
    }

    __weakSelf__
    self.eventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent * _Nullable(NSEvent * _Nonnull theEvent) {
        __strongSelf__
        if ([theEvent keyCode] == kVK_ANSI_Period && theEvent.modifierFlags & NSEventModifierFlagCommand){
            [self stopPlay:nil];
        }
        return theEvent;
    }];
    
    OBSERVER_NOTIFICATION(self, _playExplorerMovies:,kPlayExplorerMovieNotificationName_G, nil);
    OBSERVER_NOTIFICATION(self, _playNetMovies:,kPlayNetMovieNotificationName_G, nil);
    [self prepareRightMenu];
    
    [self.playerSlider onDraggedIndicator:^(double progress, MRProgressSlider * _Nonnull indicator, BOOL isEndDrag) {
        __strongSelf__
        if (self.autoTest) {
            self.autoSeeked = 1;
        }
        [self seekTo:progress];
    }];
    
    self.playedTimeLb.stringValue = @"--:--";
    self.durationTimeLb.stringValue = @"--:--";
}

- (void)prepareRightMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Root"];
    menu.delegate = self;
    self.view.menu = menu;
}

- (void)menuWillOpen:(NSMenu *)menu
{
    if (menu == self.view.menu) {
        
        [menu removeAllItems];
        
        [menu addItemWithTitle:@"打开文件" action:@selector(openFile:)keyEquivalent:@""];
        
        if (self.playingUrl) {
            if ([self.player isPlaying]) {
                [menu addItemWithTitle:@"暂停" action:@selector(pauseOrPlay:)keyEquivalent:@""];
            } else {
                [menu addItemWithTitle:@"播放" action:@selector(pauseOrPlay:)keyEquivalent:@""];
            }
            [menu addItemWithTitle:@"停止" action:@selector(stopPlay:) keyEquivalent:@"."];
            [menu addItemWithTitle:@"下一集" action:@selector(playNext:)keyEquivalent:@""];
            [menu addItemWithTitle:@"上一集" action:@selector(playPrevious:)keyEquivalent:@""];
            
            [menu addItemWithTitle:@"前进50s" action:@selector(fastForward:)keyEquivalent:@""];
            [menu addItemWithTitle:@"后退50s" action:@selector(fastRewind:)keyEquivalent:@""];
            
            NSMenuItem *speedItem = [menu addItemWithTitle:@"倍速" action:nil keyEquivalent:@""];
            
            [menu setSubmenu:({
                NSMenu *menu = [[NSMenu alloc] initWithTitle:@"倍速"];
                menu.delegate = self;
                ;menu;
            }) forItem:speedItem];
        } else {
            if ([self.playList count] > 0) {
                [menu addItemWithTitle:@"下一集" action:@selector(playNext:)keyEquivalent:@""];
                [menu addItemWithTitle:@"上一集" action:@selector(playPrevious:)keyEquivalent:@""];
            }
        }
    } else if ([menu.title isEqualToString:@"倍速"]) {
        [menu removeAllItems];
        [menu addItemWithTitle:@"0.8x" action:@selector(updateSpeed:) keyEquivalent:@""].tag = 80;
        [menu addItemWithTitle:@"1.0x" action:@selector(updateSpeed:) keyEquivalent:@""].tag = 100;
        [menu addItemWithTitle:@"1.25x" action:@selector(updateSpeed:) keyEquivalent:@""].tag = 125;
        [menu addItemWithTitle:@"1.5x" action:@selector(updateSpeed:) keyEquivalent:@""].tag = 150;
        [menu addItemWithTitle:@"2.0x" action:@selector(updateSpeed:) keyEquivalent:@""].tag = 200;
    }
}

- (void)openFile:(NSMenuItem *)sender
{
    AppDelegate *delegate = NSApp.delegate;
    [delegate openDocument:sender];
}

- (void)_playExplorerMovies:(NSNotification *)notifi
{
    NSDictionary *info = notifi.userInfo;
    NSArray *movies = info[@"obj"];
    
    if ([movies count] > 0) {
        [self.playList removeAllObjects];
        // 开始播放
        [self appendToPlayList:movies];
    }
}

- (void)_playNetMovies:(NSNotification *)notifi
{
    NSDictionary *info = notifi.userInfo;
    NSArray *links = info[@"links"];
    NSMutableArray *videos = [NSMutableArray array];
    
    for (NSString *link in links) {
        NSURL *url = [NSURL URLWithString:link];
        [videos addObject:url];
    }
    
    if ([videos count] > 0) {
        // 开始播放
        [self.playList removeAllObjects];
        [self.playList addObjectsFromArray:videos];
        [self stopPlay:nil];
        [self playFirstIfNeed];
    }
}

- (void)switchMoreView:(BOOL)wantShow
{
    float constant = wantShow ? 0 : - self.moreView.bounds.size.height;
    
    if (self.moreViewBottomCons.constant == constant) {
        return;
    }
    
    if (self.isMoreViewAnimating) {
        return;
    }
    self.isMoreViewAnimating = YES;
    
    __weakSelf__
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * _Nonnull context) {
        context.duration = 0.35;
        context.allowsImplicitAnimation = YES;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        __strongSelf__
        self.moreViewBottomCons.animator.constant = wantShow ? 0 : - self.moreView.bounds.size.height;
    } completionHandler:^{
        __strongSelf__
        self.isMoreViewAnimating = NO;
    }];
}

- (void)toggleMoreViewShow
{
    BOOL isShowing = self.moreView.frame.origin.y >= 0;
    [self switchMoreView:!isShowing];
}

- (void)toggleTitleBar:(BOOL)show
{
    if (!show && !self.playingUrl) {
        return;
    }
    
    if (show == self.view.window.titlebarAppearsTransparent) {
        self.view.window.titlebarAppearsTransparent = !show;
        self.view.window.titleVisibility = show ? NSWindowTitleVisible : NSWindowTitleHidden;
        [[self.view.window standardWindowButton:NSWindowCloseButton] setHidden:!show];
        [[self.view.window standardWindowButton:NSWindowMiniaturizeButton] setHidden:!show];
        [[self.view.window standardWindowButton:NSWindowZoomButton] setHidden:!show];
        
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext * _Nonnull context) {
            context.duration = 0.45;
            self.playerCtrlPanel.animator.alphaValue = show ? 1.0 : 0.0;
        }];
    }
}

- (void)baseView:(SHBaseView *)baseView mouseEntered:(NSEvent *)event
{
    if ([event locationInWindow].y > self.view.bounds.size.height - 35) {
        return;
    }
    [self toggleTitleBar:YES];
}

- (void)baseView:(SHBaseView *)baseView mouseMoved:(NSEvent *)event
{
    if ([event locationInWindow].y > self.view.bounds.size.height - 35) {
        return;
    }
    [self toggleTitleBar:YES];
}

- (void)baseView:(SHBaseView *)baseView mouseExited:(NSEvent *)event
{
    [self switchMoreView:NO];
    [self toggleTitleBar:NO];
}

- (void)keyDown:(NSEvent *)event
{
    if (event.modifierFlags & NSEventModifierFlagCommand) {
        switch ([event keyCode]) {
            case kVK_LeftArrow:
            {
                [self playPrevious:nil];
            }
                break;
            case kVK_RightArrow:
            {
                [self playNext:nil];
            }
                break;
            case kVK_ANSI_B:
            {
                [self toggleMoreViewShow];
            }
                break;
            case kVK_ANSI_R:
            {
                IJKSDLRotatePreference preference = self.player.view.rotatePreference;
                
                if (preference.type == IJKSDLRotateNone) {
                    preference.type = IJKSDLRotateZ;
                }
                
                if (event.modifierFlags & NSEventModifierFlagOption) {
                    
                    preference.type --;
                    
                    if (preference.type <= IJKSDLRotateNone) {
                        preference.type = IJKSDLRotateZ;
                    }
                }
                
                if (event.modifierFlags & NSEventModifierFlagShift) {
                    preference.degrees --;
                } else {
                    preference.degrees ++;
                }
                
                if (preference.degrees >= 360) {
                    preference.degrees = 0;
                }
                self.player.view.rotatePreference = preference;
                
                NSLog(@"rotate:%@ %d",@[@"X",@"Y",@"Z"][preference.type-1],(int)preference.degrees);
            }
                break;
            case kVK_ANSI_S:
            {
                [self onCaptureShot:nil];
            }
                break;
            case kVK_ANSI_Period:
            {
                [self stopPlay:nil];
            }
                break;
            case kVK_ANSI_I:
            {
                [self toggleHUD:nil];
            }
                break;
            case kVK_ANSI_0:
            {
                self.autoTest = NO;
            }
                break;
            default:
            {
                NSLog(@"0x%X",[event keyCode]);
            }
                break;
        }
    } else if (event.modifierFlags & NSEventModifierFlagControl) {
        switch ([event keyCode]) {
            case kVK_ANSI_H:
            {
                
            }
                break;
        }
    } else if (event.modifierFlags & NSEventModifierFlagOption) {
        switch ([event keyCode]) {
            case kVK_ANSI_S:
            {
                //loop exchange subtitles
                NSInteger idx = [self.subtitlePopUpBtn indexOfSelectedItem];
                idx ++;
                if (idx >= [self.subtitlePopUpBtn numberOfItems]) {
                    idx = 0;
                }
                NSMenuItem *item = [self.subtitlePopUpBtn itemAtIndex:idx];
                if (item) {
                    [self.subtitlePopUpBtn selectItem:item];
                    [self.subtitlePopUpBtn.target performSelector:self.subtitlePopUpBtn.action withObject:self.subtitlePopUpBtn];
                }
            }
                break;
        }
    }  else {
        switch ([event keyCode]) {
            case kVK_RightArrow:
            {
                [self fastForward:nil];
            }
                break;
            case kVK_LeftArrow:
            {
                [self fastRewind:nil];
            }
                break;
            case kVK_DownArrow:
            {
                float volume = self.volume;
                volume -= 0.1;
                if (volume < 0) {
                    volume = .0f;
                }
                self.volume = volume;
                [self onVolumeChange:nil];
            }
                break;
            case kVK_UpArrow:
            {
                float volume = self.volume;
                volume += 0.1;
                if (volume > 1) {
                    volume = 1.0f;
                }
                self.volume = volume;
                [self onVolumeChange:nil];
            }
                break;
            case kVK_Space:
            {
                [self pauseOrPlay:nil];
            }
                break;
            case kVK_ANSI_Minus:
            {
                if (self.player) {
                    float delay = [self.player currentSubtitleExtraDelay];
                    delay -= 2;
                    self.subtitleDelay = delay;
                    [self.player updateSubtitleExtraDelay:delay];
                }
            }
                break;
            case kVK_ANSI_Equal:
            {
                if (self.player) {
                    float delay = [self.player currentSubtitleExtraDelay];
                    delay += 2;
                    self.subtitleDelay = delay;
                    [self.player updateSubtitleExtraDelay:delay];
                }
            }
                break;
            default:
            {
                NSLog(@"0x%X",[event keyCode]);
            }
                break;
        }
    }
}

- (void)loadNASPlayList:(NSURL*)url
{
    NSString *nas_text = [[NSString alloc] initWithContentsOfFile:[url path] encoding:NSUTF8StringEncoding error:nil];
    nas_text = [nas_text stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSArray *lines = [nas_text componentsSeparatedByString:@"\n"];
    NSString *host = [lines firstObject];
    [self.playList removeAllObjects];
    NSString *lastVideo = [[NSUserDefaults standardUserDefaults] objectForKey:lastPlayedKey];
    NSURL *lastUrl = nil;
    for (int i = 1; i < lines.count; i++) {
        NSString *path = lines[i];
        if (!path || [path length] == 0 || [path hasPrefix:@"#"]) {
            continue;
        }
        
        NSString *urlStr = [host stringByAppendingString:path];
        NSURL *url = [NSURL URLWithString:urlStr];
        [self.playList addObject:url];
        
        if (lastVideo && !lastUrl && [path containsString:lastVideo]) {
            lastUrl = url;
        }
    }
    if (lastUrl) {
        [self stopPlay:nil];
        [self playURL:lastUrl];
    } else {
        [self playFirstIfNeed];
    }
}

- (NSMutableArray *)playList
{
    if (!_playList) {
        _playList = [NSMutableArray array];
    }
    return _playList;
}

- (void)perpareIJKPlayer:(NSURL *)url
{
    if (self.playingUrl) {
        [self stopPlay:nil];
    }
    
    self.playingUrl = url;
    
    if (my_stdout) {
        fflush(my_stdout);
        fclose(my_stdout);
        my_stdout = NULL;
    }
    if (my_stderr) {
        fflush(my_stderr);
        fclose(my_stderr);
        my_stderr = NULL;
    }
    
    self.seeking = NO;
    
    if (self.autoTest) {
        
        [IJKFFMoviePlayerController setLogLevel:k_IJK_LOG_INFO];
        
        NSString *dir = [self dirForCurrentPlayingUrl];
        NSString *movieName = [[url absoluteString] lastPathComponent];
        NSString *fileName = [NSString stringWithFormat:@"%@.txt",movieName];
        NSString *filePath = [dir stringByAppendingPathComponent:fileName];
        
        my_stdout = freopen([filePath cStringUsingEncoding:NSASCIIStringEncoding], "a+", stdout);
        my_stderr = freopen([filePath cStringUsingEncoding:NSASCIIStringEncoding], "a+", stderr);
        
        self.autoSeeked = NO;
        self.snapshot2 = NO;
    }
    
    IJKFFOptions *options = [IJKFFOptions optionsByDefault];
    //视频帧处理不过来的时候丢弃一些帧达到同步的效果
    [options setPlayerOptionIntValue:1 forKey:@"framedrop"];
    [options setPlayerOptionIntValue:6      forKey:@"video-pictq-size"];
    //    [options setPlayerOptionIntValue:50000      forKey:@"min-frames"];
    [options setPlayerOptionIntValue:119     forKey:@"max-fps"];
    
//    [options setCodecOptionValue:@"48" forKey:@"skip_loop_filter"];
//    [options setFormatOptionValue:@"100L" forKey:@"analyzemaxduration"];
//    [options setFormatOptionValue:@"1" forKey:@"flush_packets"];
//    [options setPlayerOptionIntValue:0      forKey:@"packet-buffering"];
//    [options setPlayerOptionIntValue:1      forKey:@"render-wait-start"];
    
    if ([url isFileURL]) {
        [options setPlayerOptionIntValue:10*1024*1024      forKey:@"max-buffer-size"];
    }
    
//    [options setPlayerOptionValue:@"fcc-bgra"        forKey:@"overlay-format"];
//    [options setPlayerOptionValue:@"fcc-bgr0"        forKey:@"overlay-format"];
//    [options setPlayerOptionValue:@"fcc-argb"        forKey:@"overlay-format"];
//    [options setPlayerOptionValue:@"fcc-0rgb"        forKey:@"overlay-format"];
//    [options setPlayerOptionValue:@"fcc-uyvy"        forKey:@"overlay-format"];
//    [options setPlayerOptionValue:@"fcc-i420"        forKey:@"overlay-format"];
//    [options setPlayerOptionValue:@"fcc-nv12"        forKey:@"overlay-format"];
    
    [options setPlayerOptionValue:self.fcc forKey:@"overlay-format"];
    [options setPlayerOptionIntValue:self.useVideoToolBox forKey:@"videotoolbox"];
    [options setPlayerOptionIntValue:self.useAsyncVTB forKey:@"videotoolbox-async"];
    options.showHudView = YES;
    
    [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:url];
    self.player = [[IJKFFMoviePlayerController alloc] initWithContentURL:url withOptions:options];
    CGRect rect = self.view.frame;
    rect.origin = CGPointZero;
    self.player.view.frame = rect;
    
    NSView <IJKSDLGLViewProtocol>*playerView = self.player.view;
    playerView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self.view addSubview:playerView positioned:NSWindowBelow relativeTo:nil];
    //playerView.preventDisplay = YES;
    //test
    [playerView setBackgroundColor:100 g:10 b:20];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerPreparedToPlay:) name:IJKMPMediaPlaybackIsPreparedToPlayDidChangeNotification object:self.player];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerPreparedToPlay:) name:IJKMPMoviePlayerSelectedStreamDidChangeNotification object:self.player];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerDidFinish:) name:IJKMPMoviePlayerPlaybackDidFinishNotification object:self.player];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerCouldNotFindCodec:) name:IJKMPMovieNoCodecFoundNotification object:self.player];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerNaturalSizeAvailable:) name:IJKMPMovieNaturalSizeAvailableNotification object:self.player];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerAfterSeekFirstVideoFrameDisplay:) name:IJKMPMoviePlayerAfterSeekFirstVideoFrameDisplayNotification object:self.player];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerOpenInput:) name:IJKMPMoviePlayerOpenInputNotification object:self.player];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerVideoDecoderFatal:) name:IJKMPMoviePlayerVideoDecoderFatalNotification object:self.player];
    
    self.kvoCtrl = [[IJKKVOController alloc] initWithTarget:self.player.monitor];
    [self.kvoCtrl safelyAddObserver:self forKeyPath:@"vdecoder" options:NSKeyValueObservingOptionNew context:nil];
    self.player.shouldAutoplay = YES;
    [self onVolumeChange:nil];
}

- (void)ijkPlayerVideoDecoderFatal:(NSNotification *)notifi
{
    NSLog(@"decoder fatal:%@",notifi.userInfo[@"code"]);
    if (self.useVideoToolBox == 2) {
        self.useVideoToolBox = 0;
        NSURL *playingUrl = self.playingUrl;
        [self stopPlay:nil];
        [self playURL:playingUrl];
    }
}

- (void)ijkPlayerOpenInput:(NSNotification *)notifi
{
    NSLog(@"demuxer:%@",notifi.userInfo[@"name"]);
}

- (void)ijkPlayerAfterSeekFirstVideoFrameDisplay:(NSNotification *)notifi
{
    NSLog(@"seek cost time:%@ms",notifi.userInfo[@"du"]);
    self.seeking = NO;
}

- (void)ijkPlayerCouldNotFindCodec:(NSNotification *)notifi
{
    NSLog(@"找不到解码器，联系开发小帅锅：%@",notifi.userInfo);
}

- (void)ijkPlayerNaturalSizeAvailable:(NSNotification *)notifi
{
    if (self.player == notifi.object) {
        CGSize videoSize = NSSizeFromString(notifi.userInfo[@"size"]);
        if (!CGSizeEqualToSize(self.view.window.aspectRatio, videoSize)) {
            
            [self.view.window setAspectRatio:videoSize];
            CGRect rect = self.view.window.frame;
            
            if (videoSize.width > videoSize.height) {
                rect.size.width = rect.size.height / videoSize.height * videoSize.width;
            } else {
                rect.size.height = rect.size.width / videoSize.width * videoSize.height;
            }
            
            [NSAnimationContext runAnimationGroup:^(NSAnimationContext * _Nonnull context) {
                [self.view.window.animator setFrame:CGRectIntegral(rect) display:YES];
            }];
        }
    }
}

- (void)ijkPlayerDidFinish:(NSNotification *)notifi
{
    if (self.player == notifi.object) {
        int reason = [notifi.userInfo[IJKMPMoviePlayerPlaybackDidFinishReasonUserInfoKey] intValue];
        if (IJKMPMovieFinishReasonPlaybackError == reason) {
            int errCode = [notifi.userInfo[@"code"] intValue];
            NSLog(@"播放出错:%d",errCode);
            if (self.autoTest) {
                NSString *dir = [self saveDir:nil];
                NSString *fileName = [NSString stringWithFormat:@"a错误汇总.txt"];
                NSString *filePath = [dir stringByAppendingPathComponent:fileName];
                FILE *pf = fopen([filePath UTF8String], "a+");
                fprintf(pf, "%d:%s\n",errCode,[[self.playingUrl absoluteString]UTF8String]);
                fflush(pf);
                fclose(pf);
                
                //-5 网络错误
                if (errCode != -5) {
                    [self playNext:nil];
                }
            } else {
                NSAlert *alert = [[NSAlert alloc] init];
                NSString *urlString = [self.player.contentURL isFileURL] ? [self.player.contentURL path] : [self.player.contentURL absoluteString];
                alert.informativeText = urlString;
                
                alert.messageText = [NSString stringWithFormat:@"%@(%d)",notifi.userInfo[@"msg"],errCode];
                
                if ([self.playList count] > 1) {
                    [alert addButtonWithTitle:@"Next"];
                }
                [alert addButtonWithTitle:@"Retry"];
                [alert addButtonWithTitle:@"OK"];
                [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse returnCode) {
                    if ([[alert buttons] count] == 3) {
                        if (returnCode == NSAlertFirstButtonReturn) {
                            [self playNext:nil];
                        } else if (returnCode == NSAlertSecondButtonReturn) {
                            //retry
                            NSURL *url = self.playingUrl;
                            [self stopPlay:nil];
                            [self playURL:url];
                        } else {
                            //
                        }
                    } else if ([[alert buttons] count] == 2) {
                        if (returnCode == NSAlertFirstButtonReturn) {
                            //retry
                            NSURL *url = self.playingUrl;
                            [self stopPlay:nil];
                            [self playURL:url];
                        } else if (returnCode == NSAlertSecondButtonReturn) {
                            //
                        }
                    }
                }];
            }
        } else if (IJKMPMovieFinishReasonPlaybackEnded == reason) {
            
            if (![[MRUtil pictureType] containsObject:[[self.playingUrl lastPathComponent] pathExtension]]) {
                NSLog(@"播放结束");
                [self playNext:nil];
            }
        }
    }
}

- (void)ijkPlayerPreparedToPlay:(NSNotification *)notifi
{
    if (self.player.isPreparedToPlay) {
        
        NSDictionary *dic = self.player.monitor.mediaMeta;
        
        [self.subtitlePopUpBtn removeAllItems];
        NSString *currentTitle = @"选择字幕";
        [self.subtitlePopUpBtn addItemWithTitle:currentTitle];
        
        [self.audioPopUpBtn removeAllItems];
        NSString *currentAudio = @"选择音轨";
        [self.audioPopUpBtn addItemWithTitle:currentAudio];
        
        [self.videoPopUpBtn removeAllItems];
        NSString *currentVideo = @"选择视轨";
        [self.videoPopUpBtn addItemWithTitle:currentVideo];
        
        for (NSDictionary *stream in dic[kk_IJKM_KEY_STREAMS]) {
            NSString *type = stream[k_IJKM_KEY_TYPE];
            int streamIdx = [stream[k_IJKM_KEY_STREAM_IDX] intValue];
            if ([type isEqualToString:k_IJKM_VAL_TYPE__SUBTITLE]) {
                NSLog(@"subtile all meta:%@",stream);
                NSString *url = stream[k_IJKM_KEY_EX_SUBTITLE_URL];
                NSString *title = nil;
                if (url) {
                    title = [[url lastPathComponent] stringByDeletingPathExtension];
                } else {
                    title = stream[k_IJKM_KEY_TITLE];
                    if (title.length == 0) {
                        title = stream[k_IJKM_KEY_LANGUAGE];
                    }
                    if (title.length == 0) {
                        title = @"未知";
                    }
                }
                title = [NSString stringWithFormat:@"%@-%d",title,streamIdx];
                if ([dic[k_IJKM_VAL_TYPE__SUBTITLE] intValue] == streamIdx) {
                    currentTitle = title;
                }
                [self.subtitlePopUpBtn addItemWithTitle:title];
            } else if ([type isEqualToString:k_IJKM_VAL_TYPE__AUDIO]) {
                NSLog(@"audio all meta:%@",stream);
                NSString *title = stream[k_IJKM_KEY_TITLE];
                if (title.length == 0) {
                    title = stream[k_IJKM_KEY_LANGUAGE];
                }
                if (title.length == 0) {
                    title = @"未知";
                }
                title = [NSString stringWithFormat:@"%@-%d",title,streamIdx];
                if ([dic[k_IJKM_VAL_TYPE__AUDIO] intValue] == streamIdx) {
                    currentAudio = title;
                }
                [self.audioPopUpBtn addItemWithTitle:title];
            } else if ([type isEqualToString:k_IJKM_VAL_TYPE__VIDEO]) {
                NSLog(@"video all meta:%@",stream);
                NSString *title = stream[k_IJKM_KEY_TITLE];
                if (title.length == 0) {
                    title = stream[k_IJKM_KEY_LANGUAGE];
                }
                if (title.length == 0) {
                    title = @"未知";
                }
                title = [NSString stringWithFormat:@"%@-%d",title,streamIdx];
                if ([dic[k_IJKM_VAL_TYPE__VIDEO] intValue] == streamIdx) {
                    currentVideo = title;
                }
                [self.videoPopUpBtn addItemWithTitle:title];
            }
        }
        [self.subtitlePopUpBtn selectItemWithTitle:currentTitle];
        [self.audioPopUpBtn selectItemWithTitle:currentAudio];
        [self.videoPopUpBtn selectItemWithTitle:currentVideo];
    }
}

- (void)playURL:(NSURL *)url
{
    [self perpareIJKPlayer:url];
    NSString *videoName = [url isFileURL] ? [url path] : [[url resourceSpecifier] lastPathComponent];
    
    NSInteger idx = [self.playList indexOfObject:self.playingUrl] + 1;
    
    [[NSUserDefaults standardUserDefaults] setObject:videoName forKey:lastPlayedKey];
    
    NSString *title = [NSString stringWithFormat:@"(%ld/%ld)%@",(long)idx,[[self playList] count],videoName];
    [self.view.window setTitle:title];
    
    [self onReset:nil];
    self.playCtrlBtn.state = NSControlStateValueOn;
    
    IJKSDLSubtitlePreference p = self.player.view.subtitlePreference;
    p.bottomMargin = self.subtitleMargin;
    self.player.view.subtitlePreference = p;
    
    if (self.tickTimer) {
        [self.tickTimer invalidate];
        self.tickTimer = nil;
        self.tickCount = 0;
    }
    
    self.tickTimer = [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(onTick:) userInfo:nil repeats:YES];
    
    [self.player prepareToPlay];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey,id> *)change context:(void *)context
{
    if (object == self.player.monitor) {
        if ([keyPath isEqualToString:@"vdecoder"]) {
            NSLog(@"current video decoder:%@",change[NSKeyValueChangeNewKey]);
        }
    }
}

static IOPMAssertionID g_displaySleepAssertionID;

- (void)enableComputerSleep:(BOOL)enable
{
    if (!g_displaySleepAssertionID && !enable)
    {
        NSLog(@"enableComputerSleep:NO");
        IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
                                    (__bridge CFStringRef)[[NSBundle mainBundle] bundleIdentifier],&g_displaySleepAssertionID);
    }
    else if (g_displaySleepAssertionID && enable)
    {
        NSLog(@"enableComputerSleep:YES");
        IOPMAssertionRelease(g_displaySleepAssertionID);
        g_displaySleepAssertionID = 0;
    }
}

- (void)onTick:(NSTimer *)sender
{
    if ([self.player isPlaying]) {
        self.tickCount ++;
        long interval = (long)self.player.currentPlaybackTime;
        long duration = self.player.monitor.duration / 1000;
        self.playedTimeLb.stringValue = [NSString stringWithFormat:@"%02d:%02d",(int)(interval/60),(int)(interval%60)];
        self.durationTimeLb.stringValue = [NSString stringWithFormat:@"%02d:%02d",(int)(duration/60),(int)(duration%60)];
        self.playerSlider.currentValue = interval;
        self.playerSlider.minValue = 0;
        self.playerSlider.maxValue = duration;
        
        if (self.autoTest) {
            //auto seek
            if (duration > 0) {
                if (interval >= 10) {
                    if (!self.autoSeeked) {
                        NSLog(@"\n-----------\n%@\n-----------\n",[self.player allHudItem]);
                        [self onCaptureShot:nil];
                        [self seekTo:duration - 10];
                        self.autoSeeked = YES;
                    }
                    
                    if (interval > duration - 5) {
                        if (!self.snapshot2) {
                            NSLog(@"\n-----------\n%@\n-----------\n",[self.player allHudItem]);
                            [self onCaptureShot:nil];
                            self.snapshot2 = YES;
                        }
                    }
                }
            }
            
            if (self.tickCount >= 60) {
                NSLog(@"\nwtf? why played %ds\n",self.tickCount);
                NSLog(@"\n-----------\n%@\n-----------\n",[self.player allHudItem]);
                [self onCaptureShot:nil];
                [self playNext:nil];
            }
        }
        [self enableComputerSleep:NO];
    }
}

- (NSURL *)existTaskForUrl:(NSURL *)url
{
    NSURL *t = nil;
    for (NSURL *item in [self.playList copy]) {
        if ([[item absoluteString] isEqualToString:[url absoluteString]]) {
            t = item;
            break;
        }
    }
    return t;
}

- (void)appendToPlayList:(NSArray *)bookmarkArr
{
    NSMutableArray *videos = [NSMutableArray array];
    NSMutableArray *subtitles = [NSMutableArray array];
    
    if (bookmarkArr.count == 1) {
        NSDictionary *dic = bookmarkArr[0];
        NSURL *url = dic[@"url"];
        if ([[[url pathExtension] lowercaseString] isEqualToString:@"xlist"]) {
            self.autoTest = YES;
            [self loadNASPlayList:url];
            return;
        }
    }
    
    for (NSDictionary *dic in bookmarkArr) {
        NSURL *url = dic[@"url"];
        
        if ([self existTaskForUrl:url]) {
            continue;
        }
        if ([dic[@"type"] intValue] == 0) {
            [videos addObject:url];
        } else if ([dic[@"type"] intValue] == 1) {
            [subtitles addObject:url];
        } else {
            NSAssert(NO, @"没有处理的文件:%@",url);
        }
    }
    
    if ([videos count] > 0) {
        [self.playList addObjectsFromArray:videos];
        [self playFirstIfNeed];
    }
    
    NSURL *lastUrl = [subtitles lastObject];
    [subtitles removeLastObject];
    for (NSURL *url in subtitles) {
        [self.player loadSubtitleFileOnly:[url path]];
    }
    if (lastUrl) {
        [self.player loadThenActiveSubtitleFile:[lastUrl path]];
    }
}

#pragma mark - 拖拽

- (void)handleDragFileList:(nonnull NSArray<NSURL *> *)fileUrls
{
    NSMutableArray *bookmarkArr = [NSMutableArray array];
    for (NSURL *url in fileUrls) {
        //先判断是不是文件夹
        BOOL isDirectory = NO;
        BOOL isExist = [[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&isDirectory];
        if (isExist) {
            if (isDirectory) {
                //扫描文件夹
                NSString *dir = [url path];
                NSArray *dicArr = [MRUtil scanFolderWithPath:dir filter:[MRUtil acceptMediaType]];
                if ([dicArr count] > 0) {
                    [bookmarkArr addObjectsFromArray:dicArr];
                }
            } else {
                NSString *pathExtension = [[url pathExtension] lowercaseString];
                if ([[MRUtil acceptMediaType] containsObject:pathExtension]) {
                    NSDictionary *dic = [MRUtil makeBookmarkWithURL:url];
                    [bookmarkArr addObject:dic];
                }
            }
        }
    }
    //拖拽播放时清空原先的列表
    [self.playList removeAllObjects];
    [self appendToPlayList:bookmarkArr];
}

- (NSDragOperation)acceptDragOperation:(NSArray<NSURL *> *)list
{
    for (NSURL *url in list) {
        if (url) {
            //先判断是不是文件夹
            BOOL isDirectory = NO;
            BOOL isExist = [[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&isDirectory];
            if (isExist) {
                if (isDirectory) {
                   //扫描文件夹
                   NSString *dir = [url path];
                   NSArray *dicArr = [MRUtil scanFolderWithPath:dir filter:[MRUtil acceptMediaType]];
                    if ([dicArr count] > 0) {
                        return NSDragOperationCopy;
                    }
                } else {
                    NSString *pathExtension = [[url pathExtension] lowercaseString];
                    if ([[MRUtil acceptMediaType] containsObject:pathExtension]) {
                        return NSDragOperationCopy;
                    }
                }
            }
        }
    }
    return NSDragOperationNone;
}

- (void)playFirstIfNeed
{
    if (!self.playingUrl) {
        [self pauseOrPlay:nil];
    }
}

#pragma mark - 点击事件

- (IBAction)pauseOrPlay:(NSButton *)sender
{
    if (!sender) {
        if (self.playCtrlBtn.state == NSControlStateValueOff) {
            self.playCtrlBtn.state = NSControlStateValueOn;
        } else {
            self.playCtrlBtn.state = NSControlStateValueOff;
        }
    }
    
    if (self.playingUrl) {
        if (self.playCtrlBtn.state == NSControlStateValueOff) {
            [self enableComputerSleep:YES];
            [self.player pause];
            [self toggleTitleBar:YES];
        } else {
            [self.player play];
        }
    } else {
        [self playNext:nil];
    }
}

- (IBAction)toggleHUD:(id)sender
{
    self.player.shouldShowHudView = !self.player.shouldShowHudView;
}

- (IBAction)onMoreFunc:(id)sender
{
    [self toggleMoreViewShow];
}

- (void)stopPlay:(NSButton *)sender
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMediaPlaybackIsPreparedToPlayDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMoviePlayerSelectedStreamDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMoviePlayerPlaybackDidFinishNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMovieNoCodecFoundNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMovieNaturalSizeAvailableNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMoviePlayerAfterSeekFirstVideoFrameDisplayNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMoviePlayerOpenInputNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMoviePlayerVideoDecoderFatalNotification object:nil];
    
    [self.kvoCtrl safelyRemoveAllObservers];
    if (self.player) {
        [self.player.view removeFromSuperview];
        [self.player pause];
        [self.player shutdown];
        self.player = nil;
    }
    
    if (self.tickTimer) {
        [self.tickTimer invalidate];
        self.tickTimer = nil;
        self.tickCount = 0;
    }
    
    if (self.playingUrl) {
        self.playingUrl = nil;
    }
    
    [self.view.window setTitle:@""];
    self.playedTimeLb.stringValue = @"--:--";
    self.durationTimeLb.stringValue = @"--:--";
    [self enableComputerSleep:YES];
    self.playCtrlBtn.state = NSControlStateValueOff;
}

- (IBAction)playPrevious:(NSButton *)sender
{
    if ([self.playList count] == 0) {
        return;
    }
    
    NSUInteger idx = [self.playList indexOfObject:self.playingUrl];
    if (idx == NSNotFound) {
        idx = 0;
    } else if (idx <= 0) {
        idx = [self.playList count] - 1;
    } else {
        idx --;
    }
    
    NSURL *url = self.playList[idx];
    [self playURL:url];
}

- (IBAction)playNext:(NSButton *)sender
{
    if ([self.playList count] == 0) {
        [self stopPlay:nil];
        return;
    }
    
    NSUInteger idx = [self.playList indexOfObject:self.playingUrl];
    //when autotest not loop
    if (self.autoTest && idx == self.playList.count - 1) {
        [self stopPlay:nil];
        self.autoTest = NO;
        [self.playList removeAllObjects];
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:lastPlayedKey];
        return;
    }
    
    if (idx == NSNotFound) {
        idx = 0;
    } else if (idx >= [self.playList count] - 1) {
        idx = 0;
    } else {
        idx ++;
    }
    
    NSURL *url = self.playList[idx];
    [self playURL:url];
}

- (void)seekTo:(float)cp
{
    if (self.seeking) {
        NSLog(@"xql ignore seek.");
        return;
    }
    self.seeking = YES;
    if (cp < 0) {
        cp = 0;
    }
    if (self.player.monitor.duration > 0) {
        if (cp >= self.player.monitor.duration) {
            cp = self.player.monitor.duration - 5;
        }
        self.player.currentPlaybackTime = cp;
    }
}

- (IBAction)fastRewind:(NSButton *)sender
{
    float cp = self.player.currentPlaybackTime;
    cp -= 50;
    [self seekTo:cp];
}

- (IBAction)fastForward:(NSButton *)sender
{
    float cp = self.player.currentPlaybackTime;
    cp += 50;
    [self seekTo:cp];
}

- (IBAction)onVolumeChange:(NSSlider *)sender
{
    self.player.playbackVolume = self.volume;
}

#pragma mark 倍速设置

- (void)updateSpeed:(NSButton *)sender
{
    NSInteger tag = sender.tag;
    float speed = tag / 100.0;
    self.player.playbackRate = speed;
}

#pragma mark 字幕设置

- (IBAction)onChangeSubtitleColor:(NSPopUpButton *)sender
{
    NSMenuItem *item = [sender selectedItem];
    int bgrValue = (int)item.tag;
    IJKSDLSubtitlePreference p = self.player.view.subtitlePreference;
    p.color = bgrValue;
    self.player.view.subtitlePreference = p;
    if (!self.player.isPlaying) {
        [self.player.view setNeedsRefreshCurrentPic];
    }
}

- (IBAction)onChangeSubtitleSize:(NSStepper *)sender
{
    IJKSDLSubtitlePreference p = self.player.view.subtitlePreference;
    p.ratio = sender.floatValue;
    self.player.view.subtitlePreference = p;
    if (!self.player.isPlaying) {
        [self.player.view setNeedsRefreshCurrentPic];
    }
}

- (IBAction)onSelectSubtitle:(NSPopUpButton*)sender
{
    if (sender.indexOfSelectedItem == 0) {
        [self.player closeCurrentStream:k_IJKM_VAL_TYPE__SUBTITLE];
    } else {
        NSString *title = sender.selectedItem.title;
        NSArray *items = [title componentsSeparatedByString:@"-"];
        int idx = [[items lastObject] intValue];
        NSLog(@"SelectSubtitleTrack:%d",idx);
        [self.player exchangeSelectedStream:idx];
    }
}

- (IBAction)onChangeSubtitleDelay:(NSStepper *)sender
{
    float delay = sender.floatValue;
    [self.player updateSubtitleExtraDelay:delay];
}

- (IBAction)onChangeSubtitleBottomMargin:(NSSlider *)sender
{
    IJKSDLSubtitlePreference p = self.player.view.subtitlePreference;
    p.bottomMargin = sender.floatValue;
    self.player.view.subtitlePreference = p;
    if (!self.player.isPlaying) {
        [self.player.view setNeedsRefreshCurrentPic];
    }
}

#pragma mark 画面设置

- (IBAction)onChangeScaleMode:(NSPopUpButton *)sender
{
    NSMenuItem *item = [sender selectedItem];
    if (item.tag == 1) {
        //scale to fill
        [self.player setScalingMode:IJKMPMovieScalingModeFill];
    } else if (item.tag == 2) {
        //aspect fill
        [self.player setScalingMode:IJKMPMovieScalingModeAspectFill];
    } else if (item.tag == 3) {
        //aspect fit
        [self.player setScalingMode:IJKMPMovieScalingModeAspectFit];
    }
    
    if (!self.player.isPlaying) {
        [self.player.view setNeedsRefreshCurrentPic];
    }
}

- (IBAction)onRotate:(NSPopUpButton *)sender
{
    NSMenuItem *item = [sender selectedItem];
    
    IJKSDLRotatePreference preference = self.player.view.rotatePreference;
    
    if (item.tag == 0) {
        preference.type = IJKSDLRotateNone;
        preference.degrees = 0;
    } else if (item.tag == 1) {
        preference.type = IJKSDLRotateZ;
        preference.degrees = -90;
    } else if (item.tag == 2) {
        preference.type = IJKSDLRotateZ;
        preference.degrees = -180;
    } else if (item.tag == 3) {
        preference.type = IJKSDLRotateZ;
        preference.degrees = -270;
    } else if (item.tag == 4) {
        preference.type = IJKSDLRotateY;
        preference.degrees = 180;
    } else if (item.tag == 5) {
        preference.type = IJKSDLRotateX;
        preference.degrees = 180;
    }
    
    self.player.view.rotatePreference = preference;
    if (!self.player.isPlaying) {
        [self.player.view setNeedsRefreshCurrentPic];
    }
    NSLog(@"rotate:%@ %d",@[@"None",@"X",@"Y",@"Z"][preference.type],(int)preference.degrees);
}

- (NSString *)saveDir:(NSString *)subDir
{
    NSArray *subDirs = nil;
    if (self.autoTest) {
        subDirs = subDir ? @[@"auto-test",subDir] : @[@"auto-test"];
    } else {
        subDirs = subDir ? @[@"ijkPro",subDir] : @[@"ijkPro"];
    }
    NSString * path = [NSFileManager mr_DirWithType:NSPicturesDirectory WithPathComponents:subDirs];
    return path;
}

- (NSString *)dirForCurrentPlayingUrl
{
    if ([self.playingUrl isFileURL]) {
        if (![[MRUtil pictureType] containsObject:[[self.playingUrl lastPathComponent] pathExtension]]) {
            return [self saveDir:[[self.playingUrl path] lastPathComponent]];
        } else {
            return [self saveDir:nil];
        }
    }
    return [self saveDir:[[self.playingUrl path] stringByDeletingLastPathComponent]];
}

- (IBAction)onCaptureShot:(id)sender
{
    CGImageRef img = [self.player.view snapshot:self.snapshot];
    if (img) {
        NSString * dir = [self dirForCurrentPlayingUrl];
        NSString *movieName = [self.playingUrl lastPathComponent];
        NSString *fileName = [NSString stringWithFormat:@"%@-%ld.jpg",movieName,(long)(CFAbsoluteTimeGetCurrent() * 1000)];
        NSString *filePath = [dir stringByAppendingPathComponent:fileName];
        NSLog(@"截屏:%@",filePath);
        [MRUtil saveImageToFile:img path:filePath];
    }
}

- (IBAction)onChangeBSC:(NSSlider *)sender
{
    if (sender.tag == 1) {
        self.brightness = sender.floatValue;
    } else if (sender.tag == 2) {
        self.saturation = sender.floatValue;
    } else if (sender.tag == 3) {
        self.contrast = sender.floatValue;
    }
    
    IJKSDLColorConversionPreference colorPreference = self.player.view.colorPreference;
    colorPreference.brightness = self.brightness;//B
    colorPreference.saturation = self.saturation;//S
    colorPreference.contrast = self.contrast;//C
    self.player.view.colorPreference = colorPreference;
    if (!self.player.isPlaying) {
        [self.player.view setNeedsRefreshCurrentPic];
    }
}

- (IBAction)onChangeDAR:(NSPopUpButton *)sender
{
    int dar_num = 0;
    int dar_den = 1;
    if (![sender.titleOfSelectedItem isEqual:@"还原"]) {
        const char* str = sender.titleOfSelectedItem.UTF8String;
        sscanf(str, "%d:%d", &dar_num, &dar_den);
    }
    self.player.view.darPreference = (IJKSDLDARPreference){1.0 * dar_num/dar_den};
    if (!self.player.isPlaying) {
        [self.player.view setNeedsRefreshCurrentPic];
    }
}

- (IBAction)onReset:(NSButton *)sender
{
    if (sender.tag == 1) {
        self.brightness = 1.0;
    } else if (sender.tag == 2) {
        self.saturation = 1.0;
    } else if (sender.tag == 3) {
        self.contrast = 1.0;
    } else {
        self.brightness = 1.0;
        self.saturation = 1.0;
        self.contrast = 1.0;
    }
    
    [self onChangeBSC:nil];
}

#pragma mark 音轨设置

- (IBAction)onSelectAudioTrack:(NSPopUpButton*)sender
{
    if (sender.indexOfSelectedItem == 0) {
        [self.player closeCurrentStream:k_IJKM_VAL_TYPE__AUDIO];
    } else {
        NSString *title = sender.selectedItem.title;
        NSArray *items = [title componentsSeparatedByString:@"-"];
        int idx = [[items lastObject] intValue];
        NSLog(@"SelectAudioTrack:%d",idx);
        [self.player exchangeSelectedStream:idx];
    }
}

- (IBAction)onSelectVideoTrack:(NSPopUpButton*)sender
{
    if (sender.indexOfSelectedItem == 0) {
        [self.player closeCurrentStream:k_IJKM_VAL_TYPE__VIDEO];
    } else {
        NSString *title = sender.selectedItem.title;
        NSArray *items = [title componentsSeparatedByString:@"-"];
        int idx = [[items lastObject] intValue];
        NSLog(@"SelectVideoTrack:%d",idx);
        [self.player exchangeSelectedStream:idx];
    }
}

#pragma mark 解码设置

- (IBAction)onSelectFCC:(NSPopUpButton*)sender
{
    NSString *title = sender.selectedItem.title;
    NSString *fcc = [@"fcc-" stringByAppendingString:title];
    self.fcc = fcc;
}

#pragma mark 日志级别

- (int)levelWithString:(NSString *)str
{
    str = [str lowercaseString];
    if ([str isEqualToString:@"default"]) {
        return k_IJK_LOG_DEFAULT;
    } else if ([str isEqualToString:@"verbose"]) {
        return k_IJK_LOG_VERBOSE;
    } else if ([str isEqualToString:@"debug"]) {
        return k_IJK_LOG_DEBUG;
    } else if ([str isEqualToString:@"info"]) {
        return k_IJK_LOG_INFO;
    } else if ([str isEqualToString:@"warn"]) {
        return k_IJK_LOG_WARN;
    } else if ([str isEqualToString:@"error"]) {
        return k_IJK_LOG_ERROR;
    } else if ([str isEqualToString:@"fatal"]) {
        return k_IJK_LOG_FATAL;
    } else if ([str isEqualToString:@"silent"]) {
        return k_IJK_LOG_SILENT;
    } else {
        return k_IJK_LOG_UNKNOWN;
    }
}

- (void)reSetLoglevel:(NSString *)loglevel
{
    int level = [self levelWithString:loglevel];
    [IJKFFMoviePlayerController setLogLevel:level];
}

- (IBAction)onChangeLogLevel:(NSPopUpButton*)sender
{
    NSString *title = sender.selectedItem.title;
    [self reSetLoglevel:title];
}

@end
