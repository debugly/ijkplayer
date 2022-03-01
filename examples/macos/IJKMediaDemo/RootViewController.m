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

@interface RootViewController ()<MRDragViewDelegate,SHBaseViewDelegate,NSMenuDelegate>

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

@property (weak) NSTrackingArea *trackingArea;

//for cocoa binding begin
@property (assign) float volume;
@property (assign) float subtitleFontSize;
@property (assign) float subtitleDelay;
@property (assign) float subtitleMargin;

@property (assign) float brightness;
@property (assign) float saturation;
@property (assign) float contrast;

@property (assign) BOOL useVideoToolBox;
@property (assign) int useAsyncVTB;
@property (copy) NSString *fcc;
@property (assign) int snapshot;
//for cocoa binding end

@property (weak) id eventMonitor;

@end

@implementation RootViewController

- (void)dealloc
{
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

    self.subtitleFontSize = 25;
    self.subtitleMargin = 0.7;
    self.useVideoToolBox = YES;
    self.fcc = @"fcc-_es2";
    self.snapshot = 3;
    self.volume = 0.4;
    [self onReset:nil];
    
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
    
    [self prepareRightMenu];
    
    [self.playerSlider onDraggedIndicator:^(double progress, MRProgressSlider * _Nonnull indicator, BOOL isEndDrag) {
        __strongSelf__
        self.player.currentPlaybackTime = progress;
    }];
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
            [menu addItemWithTitle:@"停止" action:@selector(stop:)keyEquivalent:@""];
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
        // 开始播放
        [self appendToPlayList:movies];
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

- (void)baseView:(SHBaseView *)baseView mouseEntered:(NSEvent *)event
{
    
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * _Nonnull context) {
        context.duration = 0.35;
        self.playerCtrlPanel.animator.alphaValue = 1.0;
        [[self.view.window standardWindowButton:NSWindowCloseButton] setHidden:NO];
        [[self.view.window standardWindowButton:NSWindowMiniaturizeButton] setHidden:NO];
        [[self.view.window standardWindowButton:NSWindowZoomButton] setHidden:NO];
    }];
}

- (void)baseView:(SHBaseView *)baseView mouseExited:(NSEvent *)event
{
    [self switchMoreView:NO];
    if (self.playingUrl) {
        [[self.view.window standardWindowButton:NSWindowCloseButton] setHidden:YES];
        [[self.view.window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
        [[self.view.window standardWindowButton:NSWindowZoomButton] setHidden:YES];
        
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext * _Nonnull context) {
            context.duration = 0.35;
            self.playerCtrlPanel.animator.alphaValue = 0.0;
        }];
    }
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
                [self exchangeVideoDecoder];
            }
                break;
        }
    } else {
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
            default:
            {
                NSLog(@"0x%X",[event keyCode]);
            }
                break;
        }
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
    IJKFFOptions *options = [IJKFFOptions optionsByDefault];
    //视频帧处理不过来的时候丢弃一些帧达到同步的效果
    //    [options setPlayerOptionIntValue:2 forKey:@"framedrop"];
    [options setPlayerOptionIntValue:16      forKey:@"video-pictq-size"];
    //    [options setPlayerOptionIntValue:50000      forKey:@"min-frames"];
    //    [options setPlayerOptionIntValue:50*1024*1024      forKey:@"max-buffer-size"];
    [options setPlayerOptionIntValue:30     forKey:@"max-fps"];
    [options setPlayerOptionIntValue:1      forKey:@"packet-buffering"];
    
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
    [options setPlayerOptionIntValue:3840 forKey:@"videotoolbox-max-frame-width"];
    [options setShowHudView:NO];
    
    [self stopPlay:nil];
    [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:url];
    self.player = [[IJKFFMoviePlayerController alloc] initWithContentURL:url withOptions:options];
    CGRect rect = self.view.frame;
    rect.origin = CGPointZero;
    self.player.view.frame = rect;
    
    NSView <IJKSDLGLViewProtocol>*playerView = self.player.view;
    playerView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self.view addSubview:playerView positioned:NSWindowBelow relativeTo:nil];
    
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMediaPlaybackIsPreparedToPlayDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerPreparedToPlay:) name:IJKMPMediaPlaybackIsPreparedToPlayDidChangeNotification object:self.player];
    
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMoviePlayerSelectedStreamDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerPreparedToPlay:) name:IJKMPMoviePlayerSelectedStreamDidChangeNotification object:self.player];
    
    [[NSNotificationCenter defaultCenter] removeObserver:self name:IJKMPMoviePlayerPlaybackDidFinishNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(ijkPlayerDidFinish:) name:IJKMPMoviePlayerPlaybackDidFinishNotification object:self.player];
    
    self.player.scalingMode = IJKMPMovieScalingModeAspectFit;
    self.player.shouldAutoplay = YES;
    [self onVolumeChange:nil];
}

- (void)ijkPlayerDidFinish:(NSNotification *)notifi
{
    if (self.player == notifi.object) {
        int reason = [notifi.userInfo[IJKMPMoviePlayerPlaybackDidFinishReasonUserInfoKey] intValue];
        if (IJKMPMovieFinishReasonPlaybackError == reason) {
            int errCode = [notifi.userInfo[@"error"] intValue];
            NSLog(@"播放出错:%d",errCode);
            [self.player stop];
        } else if (IJKMPMovieFinishReasonPlaybackEnded == reason) {
            NSLog(@"播放结束");
            [self playNext:nil];
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
        
        for (NSDictionary *stream in dic[kk_IJKM_KEY_STREAMS]) {
            NSString *type = stream[k_IJKM_KEY_TYPE];
            int streamIdx = [stream[k_IJKM_KEY_STREAM_IDX] intValue];
            if ([type isEqualToString:k_IJKM_VAL_TYPE__SUBTITLE]) {
                NSString *title = stream[k_IJKM_KEY_TITLE];
                if (title.length == 0) {
                    title = stream[k_IJKM_KEY_LANGUAGE];
                }
                if (title.length == 0) {
                    title = @"未知";
                }
                title = [NSString stringWithFormat:@"%@-%d",title,streamIdx];
                if ([dic[k_IJKM_VAL_TYPE__SUBTITLE] intValue] == streamIdx) {
                    currentTitle = title;
                }
                [self.subtitlePopUpBtn addItemWithTitle:title];
            } else if ([type isEqualToString:k_IJKM_VAL_TYPE__AUDIO]) {
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
            }
        }
        [self.subtitlePopUpBtn selectItemWithTitle:currentTitle];
        [self.audioPopUpBtn selectItemWithTitle:currentAudio];
    }
}

- (void)playURL:(NSURL *)url
{
    [self perpareIJKPlayer:url];
    self.playingUrl = url;

    NSString *title = [url isFileURL] ? [url path] : [[url resourceSpecifier] lastPathComponent];
    [self.view.window setTitle:title];
    
    [self onReset:nil];
    
    IJKSDLSubtitlePreference p = self.player.view.subtitlePreference;
    p.bottomMargin = self.subtitleMargin;
    self.player.view.subtitlePreference = p;
    
    if (!self.tickTimer) {
        self.tickTimer = [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(onTick:) userInfo:nil repeats:YES];
    }
    
    [self.player prepareToPlay];
    self.kvoCtrl = [[IJKKVOController alloc] initWithTarget:self.player.monitor];
    [self.kvoCtrl safelyAddObserver:self forKeyPath:@"vdecoder" options:NSKeyValueObservingOptionNew context:nil];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey,id> *)change context:(void *)context
{
    if (object == self.player.monitor) {
        if ([keyPath isEqualToString:@"vdecoder"]) {
            NSLog(@"current video decoder:%@",change[NSKeyValueChangeNewKey]);
        }
    }
}

- (void)onTick:(NSTimer *)sender
{
    if (self.player) {
        
        long interval = (long)self.player.currentPlaybackTime;
        long duration = self.player.monitor.duration / 1000;
        self.playedTimeLb.stringValue = [NSString stringWithFormat:@"%02d:%02d",(int)(interval/60),(int)(interval%60)];
        self.durationTimeLb.stringValue = [NSString stringWithFormat:@"%02d:%02d",(int)(duration/60),(int)(duration%60)];
        self.playerSlider.currentValue = interval;
        self.playerSlider.minValue = 0;
        self.playerSlider.maxValue = duration;
    } else {
        self.playedTimeLb.stringValue = @"--:--";
        self.durationTimeLb.stringValue = @"--:--";
        [sender invalidate];
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
    
    //拖进来新的视频时，清理老的视频列表
    if ([videos count] > 0) {
        [self.playList addObjectsFromArray:videos];
        [self playFirstIfNeed];
    }
    
    for (NSURL *url in subtitles) {
        [self.player loadThenActiveSubtitleFile:[url path]];
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
        NSURL *url = [self.playList firstObject];
        if (url) {
            [self playURL:url];
        }
    }
}

#pragma mark - 点击事件

- (IBAction)pauseOrPlay:(NSButton *)sender
{
    if (self.playingUrl) {
        if (self.playCtrlBtn.state == NSControlStateValueOff) {
            [self.player pause];
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
    if (self.player) {
        [self.player.view removeFromSuperview];
        [self.player stop];
        [self.player shutdown];
        self.player = nil;
    }
    
    if (self.playingUrl) {
        self.playingUrl = nil;
    }
    
    [self.view.window setTitle:@""];
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
        return;
    }
    
    NSUInteger idx = [self.playList indexOfObject:self.playingUrl];
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

- (IBAction)fastRewind:(NSButton *)sender
{
    float cp = self.player.currentPlaybackTime;
    cp -= 50;
    if (cp < 0) {
        cp = 0;
    }
    self.player.currentPlaybackTime = cp;
}

- (IBAction)fastForward:(NSButton *)sender
{
    float cp = self.player.currentPlaybackTime;
    cp += 50;
    if (cp < 0) {
        cp = 0;
    }
    self.player.currentPlaybackTime = cp;
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
    [self.player invalidateSubtitleEffect];
}

- (IBAction)onChangeSubtitleSize:(NSStepper *)sender
{
    IJKSDLSubtitlePreference p = self.player.view.subtitlePreference;
    p.fontSize = sender.intValue;
    self.player.view.subtitlePreference = p;
    [self.player invalidateSubtitleEffect];
}

- (IBAction)onSelectSubtitle:(NSPopUpButton*)sender
{
    NSString *title = sender.selectedItem.title;
    NSArray *items = [title componentsSeparatedByString:@"-"];
    if ([items count] == 2) {
        int idx = [[items lastObject] intValue];
        NSLog(@"SelectSubtitle:%d",idx);
        [self.player exchangeSelectedStream:idx];
    } else {
        [self.player closeCurrentStream:k_IJKM_VAL_TYPE__SUBTITLE];
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
    [self.player invalidateSubtitleEffect];
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
    
    NSLog(@"rotate:%@ %d",@[@"None",@"X",@"Y",@"Z"][preference.type],(int)preference.degrees);
}

- (IBAction)onCaptureShot:(id)sender
{
    CGImageRef img = [self.player.view snapshot:self.snapshot];
    if (img) {
        //,[self.playingUrl lastPathComponent]
        NSString * path = [NSFileManager mr_DirWithType:NSPicturesDirectory WithPathComponents:@[@"ijkPro"]];
        NSString *fileName = [NSString stringWithFormat:@"%ld.jpg",(long)CFAbsoluteTimeGetCurrent()];
        NSString *filePath = [path stringByAppendingPathComponent:fileName];
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
}

- (IBAction)onChangeDAR:(NSPopUpButton *)sender
{
    int dar_num = 1;
    int dar_den = 1;
    if ([sender.titleOfSelectedItem isEqual:@"还原"]) {
        dar_num = dar_den = 0;
    }
    else {
        const char* str = sender.titleOfSelectedItem.UTF8String;
        sscanf(str, "%d:%d", &dar_num, &dar_den);
    }
    self.player.view.darPreference = (IJKSDLDARPreference){dar_num,dar_den};
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
    NSString *title = sender.selectedItem.title;
    NSArray *items = [title componentsSeparatedByString:@"-"];
    if ([items count] == 2) {
        int idx = [[items lastObject] intValue];
        NSLog(@"SelectAudioTrack:%d",idx);
        [self.player exchangeSelectedStream:idx];
    } else {
        [self.player closeCurrentStream:k_IJKM_VAL_TYPE__AUDIO];
    }
}

#pragma mark 解码设置

- (IBAction)onSelectFCC:(NSPopUpButton*)sender
{
    NSString *title = sender.selectedItem.title;
    NSString *fcc = [@"fcc-" stringByAppendingString:title];
    self.fcc = fcc;
}

- (void)exchangeVideoDecoder
{
    int r = [self.player exchangeVideoDecoder];
    if (r == 1) {
        NSLog(@"exchang decoder begin");
    } else if (r == -1) {
        NSLog(@"exchanging decoder");
    } else if (r == -2) {
        NSLog(@"can't exchange decoder,try later");
    } else if (r == -3) {
        NSLog(@"no more decoder can exchange.");
    } else if (r == -4) {
        NSLog(@"exchange decoder faild.");
    }
}

@end
