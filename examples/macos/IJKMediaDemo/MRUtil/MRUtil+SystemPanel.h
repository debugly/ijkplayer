//
//  MRUtil+SystemPanel.h
//  IJKMediaMacDemo
//
//  Created by Matt Reach on 2020/3/11.
//  Copyright © 2021 IJK Mac.. All rights reserved.
//

#import "MRUtil.h"

NS_ASSUME_NONNULL_BEGIN

@interface MRUtil(SystemPanel)

///展示系统选择文件面板:可以选择单个视频，也可以选择文件夹
+ (NSArray<NSDictionary *> *)showSystemChooseVideoPanalForLocal;
///展示系统选择文件面板;选择视频文件[{bookmark,url,type}]//type:0,movie;1,subtitle;
+ (NSArray<NSDictionary *> *)showSystemChooseVideoPanel;
///展示系统选择文件面板;选择视频文件[{bookmark,url,type}]//type:0,movie;1,subtitle; 自动关联（下载、影片）文件夹
+ (NSArray<NSDictionary *> *)showSystemChooseVideoPanelAutoScan;
///展示系统选择文件面板;选择视频、字幕文件[{bookmark,url,type}]//type:0,movie;1,subtitle
+ (NSArray<NSDictionary *> *)showSystemChooseVideoPanel4Share;
///展示系统选择文件夹面板;
+ (NSArray<NSDictionary *> *)showSystemChooseFolderPanel;

+ (NSArray <NSDictionary *>*)scanFolderWithPath:(NSString *)dir filter:(NSArray<NSString *>*)types;

+ (NSDictionary *)makeBookmarkWithURL:(NSURL *)url;

@end

NS_ASSUME_NONNULL_END
