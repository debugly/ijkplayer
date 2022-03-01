//
//  AppDelegate.m
//  IJKMediaDemo
//
//  Created by Matt Reach on 2019/6/25.
//  Copyright © 2019 IJK Mac. All rights reserved.
//

#import "AppDelegate.h"
#import "WindowController.h"
#import "RootViewController.h"
#import <IJKMediaPlayerKit/IJKMediaPlayerKit.h>
#import "MRGlobalNotification.h"
#import "MRUtil+SystemPanel.h"

@interface AppDelegate ()

@property (strong) NSWindowController *windowCtrl;

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insert code here to initialize your application
    
    NSWindowStyleMask mask = NSWindowStyleMaskBorderless | NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView;
    
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600) styleMask:mask backing:NSBackingStoreBuffered defer:YES];
    window.contentViewController = [[RootViewController alloc] init];
    window.movableByWindowBackground = YES;
    window.titlebarAppearsTransparent = YES;
    
    self.windowCtrl = [[WindowController alloc] init];
    self.windowCtrl.window = window;
    [window center];
    [self.windowCtrl showWindow:nil];
    BOOL match = [IJKFFMoviePlayerController checkIfFFmpegVersionMatch:YES];
    
    NSLog(@"==FFmpegVersionMatch:%d",match);
    
    [IJKFFMoviePlayerController setLogReport:YES];
    [IJKFFMoviePlayerController setLogLevel:k_IJK_LOG_INFO];
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    if ([self.windowCtrl.window isMiniaturized]) {
        [self.windowCtrl.window deminiaturize:sender];
    } else {
        [self.windowCtrl.window makeKeyAndOrderFront:sender];
    }
    [NSApp activateIgnoringOtherApps:YES];
    return YES;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (void)playOpenedURL:(NSArray<NSURL *> * _Nonnull)urls
{
    if ([urls count] == 0) {
        return;
    }
    
    NSMutableArray *bookmarkArr = [NSMutableArray array];
    for (NSURL *url in urls) {
        NSDictionary *dic = [MRUtil makeBookmarkWithURL:url];
        if (dic) {
            [bookmarkArr addObject:dic];
        }
    }
    if ([bookmarkArr count] > 0) {
        NSMutableDictionary *dic = [NSMutableDictionary new];
        [dic setObject:bookmarkArr forKey:@"obj"];
        POST_NOTIFICATION(kPlayExplorerMovieNotificationName_G, self, dic);
    }
}

- (void)openDocument:(id)sender
{
    NSArray<NSDictionary *> * bookmarkArr = [MRUtil showSystemChooseVideoPanelAutoScan];
    if ([bookmarkArr count] > 0) {
        NSMutableDictionary *dic = [NSMutableDictionary new];
        [dic setObject:bookmarkArr forKey:@"obj"];
        POST_NOTIFICATION(kPlayExplorerMovieNotificationName_G, self, dic);
    }
}

- (IBAction)showPreferencesPanel:(id)sender
{
    NSStoryboard *sb = [NSStoryboard storyboardWithName:@"Setting" bundle:nil];
    [self.windowCtrl.window.contentViewController presentViewControllerAsModalWindow:[sb instantiateInitialController]];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames
{
    NSMutableArray *urlArr = [NSMutableArray array];
    for (NSString *file in filenames) {
        [urlArr addObject:[NSURL fileURLWithPath:file]];
    }
    [self playOpenedURL:urlArr];
}

- (void)application:(NSApplication *)application openURLs:(NSArray<NSURL *> *)urls
{
    [self playOpenedURL:urls];
}

@end
