//
//  SettingSplitViewController.m
//  IJKMediaMacDemo
//
//  Created by Reach Matt on 2022/2/24.
//  Copyright © 2022 IJK Mac. All rights reserved.
//

#import "SettingSplitViewController.h"
#import "LeftCategoryController.h"

@interface SettingSplitViewController ()

@end

@implementation SettingSplitViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do view setup here.
    
    NSSplitViewItem *leftItem = [[self splitViewItems] firstObject];
    NSSplitViewItem *rightItem = [[self splitViewItems] lastObject];
    LeftCategoryController *lvc = (LeftCategoryController *)leftItem.viewController;
    NSViewController *rvc = rightItem.viewController;
    
    [lvc updateDataSource:@[
        @{
            @"title" : @"通用",
            @"vc" : @"GeneralSettingViewController"
        },@{
            @"title" : @"字幕",
            @"vc" : @"SubtitleSettingViewController"
        },@{
            @"title" : @"画面",
            @"vc" : @"GraphicSettingViewController"
        },@{
            @"title" : @"截图",
            @"vc" : @"SnapshotSettingViewController"
        },@{
            @"title" : @"解码器",
            @"vc" : @"DecoderSettingViewController"
        }
    ]];
    
    __weakSelf__
    [lvc onSelectItem:^(NSDictionary * _Nonnull dic) {
        NSString *vcStr = dic[@"vc"];
        if (vcStr.length > 0) {
            Class clazz = NSClassFromString(vcStr);
            if (clazz) {
                NSViewController *vc = [[clazz alloc] init];
                [rvc setChildViewControllers:nil];
                [[rvc.view subviews]makeObjectsPerformSelector:@selector(removeFromSuperview)];
                [rvc addChildViewController:vc];
                vc.view.frame = rvc.view.bounds;
                vc.view.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
                [rvc.view addSubview:vc.view];
            }
        }
    }];
}

@end
