//
//  MRUtil+SystemPanel.m
//  IJKMediaMacDemo
//
//  Created by Matt Reach on 2020/3/11.
//  Copyright © 2021 IJK Mac.. All rights reserved.
//

#import "MRUtil+SystemPanel.h"
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UTType.h>

@implementation MRUtil (SystemPanel)

+ (NSDictionary *)makeBookmarkWithURL:(NSURL *)url {
    
    NSString *pathExtension = [[url pathExtension] lowercaseString];
    [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:url];
    NSData *bookmark = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                        | NSURLBookmarkCreationSecurityScopeAllowOnlyReadAccess includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
    if (bookmark) {
        NSMutableDictionary *dic = [NSMutableDictionary dictionary];
        [dic setObject:url forKey:@"url"];
        [dic setObject:bookmark forKey:@"bookmark"];
        
        if ([[self subtitleType] containsObject:pathExtension]) {
            [dic setObject:@(1) forKey:@"type"];
        } else {
            [dic setObject:@(0) forKey:@"type"];
        }
        return [dic copy];
    }
    return nil;
}

+ (NSArray <NSDictionary *>*)scanFolderWithPath:(NSString *)dir filter:(NSArray<NSString *>*)types{
    NSError *error = nil;
    NSMutableArray *bookmarkArr = [NSMutableArray arrayWithCapacity:3];
    NSArray<NSString *> *contents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:&error];
    if (!error && contents) {
        for (NSString *c in contents) {
            NSString*item = [dir stringByAppendingPathComponent:c];
            NSURL*item_url = [NSURL fileURLWithPath:item];
            NSString *pathExtension = [[item_url pathExtension] lowercaseString];
            BOOL add = NO;
            if ([types count] > 0) {
                if ([types containsObject:pathExtension]) {
                    add = YES;
                }
            } else {
                add = YES;
            }
            if (add) {
                NSDictionary *dic = [self makeBookmarkWithURL:item_url];
                [bookmarkArr addObject:dic];
            }
        }
        ///按照文件名排序
        [bookmarkArr sortUsingComparator:^NSComparisonResult(NSDictionary * obj1, NSDictionary * obj2) {
            NSURL *url1 = obj1[@"url"];
            NSURL *url2 = obj2[@"url"];
            return (NSComparisonResult)[[url1 lastPathComponent] compare:[url2 lastPathComponent] options:NSNumericSearch];
        }];
        
        return [bookmarkArr copy];
    }
    return nil;
}

+ (NSArray<NSDictionary *> *)_showSystemChooseFileOrFolderPanelWithType:(NSArray<NSString *>*)types {
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    openPanel.canChooseFiles = YES;
    openPanel.canChooseDirectories = YES;
    openPanel.allowsMultipleSelection = YES;
    
    NSString *downloadsDir = [NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory, NSUserDomainMask, YES) firstObject];
    
    [openPanel setDirectoryURL:[NSURL URLWithString:downloadsDir]];
    [openPanel setAllowedFileTypes:types];
    
    NSMutableArray *bookmarkArr = [NSMutableArray array];
    
    if ([openPanel runModal] == NSModalResponseOK) {
        NSArray<NSURL *> *urls = [openPanel URLs];
        for (NSURL *url in urls) {
            BOOL isDirectory = NO;
            BOOL isExist = [[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&isDirectory];
            if (isExist) {
                if (isDirectory) {
                    ///扫描文件夹
                    NSString *dir = [url path];
                    
                    NSArray *dicArr = [self scanFolderWithPath:dir filter:types];
                    if ([dicArr count] > 0) {
                        [bookmarkArr addObjectsFromArray:dicArr];
                    }
                } else {
                    NSDictionary *dic = [self makeBookmarkWithURL:url];
                    [bookmarkArr addObject:dic];
                }
            }
        }
    }
    
    if ([bookmarkArr count] > 0) {
        return bookmarkArr;
    } else {
        return nil;
    }
}

+ (NSArray<NSDictionary *> *)_showSystemChoosePanelWithType:(NSArray<NSString *>*)types autoScan:(BOOL)autoScan
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    openPanel.canChooseFiles = YES;
    openPanel.canChooseDirectories = NO;
    openPanel.allowsMultipleSelection = YES;
    
    NSString *downloadsDir = [NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory, NSUserDomainMask, YES) firstObject];
    
    [openPanel setDirectoryURL:[NSURL URLWithString:downloadsDir]];
    [openPanel setAllowedFileTypes:types];
    
    NSMutableArray *bookmarkArr = [NSMutableArray array];
    
    if ([openPanel runModal] == NSModalResponseOK)
    {
        BOOL singleSelect = [[openPanel URLs] count] == 1;
        ///单选时才尝试自动扫描
        if (singleSelect && autoScan) {
            NSURL *url = [openPanel URL];
            NSString *dir = [[url path] stringByDeletingLastPathComponent];
            NSArray *dicArr = [self scanFolderWithPath:dir filter:types];
            
            if ([dicArr count] > 0) {
                for (int i = 0; i < [dicArr count]; i ++) {
                    NSDictionary *dic = dicArr[i];
                    NSURL *_url = dic[@"url"];
                    ///标记下
                    if ([url isEqualTo:_url]) {
                        NSMutableDictionary *m_dic = [NSMutableDictionary dictionaryWithDictionary:dic];
                        [m_dic setObject:@"1" forKey:@"default"];
                        [bookmarkArr addObject:m_dic];
                    } else {
                        [bookmarkArr addObject:dic];
                    }
                }
            } else {
                ///不是download或者movie目录
                NSArray *urls = [openPanel URLs];
                
                for (NSURL *url in urls) {
                    NSDictionary *dic = [self makeBookmarkWithURL:url];
                    [bookmarkArr addObject:dic];
                }
            }
        } else {
            NSArray *urls = [openPanel URLs];
            
            for (NSURL *url in urls) {
                NSDictionary *dic = [self makeBookmarkWithURL:url];
                [bookmarkArr addObject:dic];
            }
        }
    }
    
    if ([bookmarkArr count] > 0) {
        return bookmarkArr;
    } else {
        return nil;
    }
}

+ (NSArray<NSDictionary *> *)_showSystemChooseFolderPanelWithType:(NSArray<NSString *>*)types
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    openPanel.canChooseFiles = NO;
    openPanel.canChooseDirectories = YES;
    openPanel.allowsMultipleSelection = YES;
    
    NSString *downloadsDir = [NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory, NSUserDomainMask, YES) firstObject];
    
    [openPanel setDirectoryURL:[NSURL URLWithString:downloadsDir]];
    [openPanel setAllowedFileTypes:types];
    
    NSMutableArray *bookmarkArr = [NSMutableArray array];
    
    if ([openPanel runModal] == NSModalResponseOK)
    {
        ///扫描文件夹
        NSURL *url = [openPanel URL];
        NSString *dir = [url path];
        
        NSArray *dicArr = [self scanFolderWithPath:dir filter:types];
        if ([dicArr count] > 0) {
            [bookmarkArr addObjectsFromArray:dicArr];
        }
    }
    
    if ([bookmarkArr count] > 0) {
        return bookmarkArr;
    } else {
        return nil;
    }
}

+ (NSArray<NSDictionary *> *)showSystemChooseVideoPanalForLocal {
    return [self _showSystemChooseFileOrFolderPanelWithType:[self videoType]];
}

+ (NSArray<NSDictionary *> *)showSystemChooseVideoPanel
{
    return [self _showSystemChoosePanelWithType:[self videoType] autoScan:NO];
}

+ (NSArray<NSDictionary *> *)showSystemChooseVideoPanelAutoScan
{
    return [self _showSystemChoosePanelWithType:[self videoType] autoScan:YES];
}

+ (NSArray<NSDictionary *> *)showSystemChooseFolderPanel
{
    return [self _showSystemChooseFolderPanelWithType:[self videoType]];
}

+ (NSArray<NSDictionary *> *)showSystemChooseVideoPanel4Share
{
    NSMutableArray *videoTypes = [NSMutableArray array];
    [videoTypes addObjectsFromArray:[self videoType]];
    [videoTypes addObjectsFromArray:[self subtitleType]];
    
    return [self _showSystemChoosePanelWithType:videoTypes autoScan:NO];
}

+ (NSDictionary *)showSystemChooseSubtitlePanel4Share:(NSString *)optDir
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    openPanel.canChooseFiles = YES;
    openPanel.canChooseDirectories = NO;
    openPanel.allowsMultipleSelection = NO;
    
    if (!optDir) {
        optDir = [NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory, NSUserDomainMask, YES) firstObject];
    }
    
    optDir = [optDir stringByAddingPercentEncodingWithAllowedCharacters:NSCharacterSet.URLPathAllowedCharacterSet];
    
    [openPanel setDirectoryURL:[NSURL URLWithString:optDir]];
    
    [openPanel setAllowedFileTypes:[self subtitleType]];
    
    if ([openPanel runModal] == NSModalResponseOK)
    {
        NSURL *url = [[openPanel URLs] firstObject];
        return [self makeBookmarkWithURL:url];
    }
    return nil;
}

// 中文字符串转换成拼音
+ (NSString *) chineseStringTransformPinyin:(NSString *)chineseString {
    if (chineseString == nil) {
        return nil;
    }
    // 拼音字段
    NSMutableString *tempNamePinyin = [chineseString mutableCopy];
    CFStringTransform((__bridge CFMutableStringRef)tempNamePinyin, NULL, kCFStringTransformMandarinLatin, NO);
    CFStringTransform((__bridge CFMutableStringRef)tempNamePinyin, NULL, kCFStringTransformStripDiacritics, NO);
    return tempNamePinyin.uppercaseString;
}

// 对中文字符串数组进行排序
+ (NSArray *)chineseSortWithStringArray:(NSArray *)stringArray
{
    if (stringArray == nil) {
        return nil;
    }
    NSMutableArray *tempArray = [[NSMutableArray alloc] init];
    for (int i = 0 ; i < [stringArray count] ; i++) {
        if (![[stringArray objectAtIndex:i] isKindOfClass:[NSString class]]) {
            return nil;
        }
        NSDictionary *tempDic = [[NSDictionary alloc] initWithObjectsAndKeys:[stringArray objectAtIndex:i], @"chinese", [self chineseStringTransformPinyin:[stringArray objectAtIndex:i]], @"pinyin", nil];
        [tempArray addObject:tempDic];
    }
    // 排序
    [tempArray sortUsingComparator:^NSComparisonResult(id obj1, id obj2) {
        return [[obj1 objectForKey:@"pinyin"] compare:[obj2 objectForKey:@"pinyin"] options:NSNumericSearch];
    }];
    NSMutableArray *resultArray = [[NSMutableArray alloc] init];
    for (NSDictionary *tempDic in tempArray) {
        [resultArray addObject:[tempDic objectForKey:@"chinese"]];
    }
    return resultArray;
}


+ (NSArray<NSDictionary *> *)showSystemChooseSubtitlesPanel4Share:(NSString *)optDir
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    openPanel.canChooseFiles = YES;
    openPanel.canChooseDirectories = NO;
    openPanel.allowsMultipleSelection = YES;
    
    if (!optDir) {
        optDir = [NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory, NSUserDomainMask, YES) firstObject];
    }
    
    optDir = [optDir stringByAddingPercentEncodingWithAllowedCharacters:NSCharacterSet.URLPathAllowedCharacterSet];
    
    [openPanel setDirectoryURL:[NSURL URLWithString:optDir]];
    
    [openPanel setAllowedFileTypes:[self subtitleType]];
    
    NSMutableArray *bookmarkArr = [NSMutableArray array];
    
    if ([openPanel runModal] == NSModalResponseOK)
    {
        NSArray <NSURL *>* urls = [openPanel URLs];
        NSString *lastPathComponent;
        NSMutableArray *sortArray = [NSMutableArray array];
        for (NSURL *url in urls) {
            NSString *name = [[url path] lastPathComponent];
            lastPathComponent = [[url path] stringByDeletingLastPathComponent];
            [sortArray addObject:name];
        }
        //根据名称排序
        NSArray *sortedArray = [self chineseSortWithStringArray:sortArray];
        sortedArray = [[sortedArray reverseObjectEnumerator] allObjects];
        for (NSString *name in sortedArray) {
            NSURL *url = [NSURL fileURLWithPath:[NSString stringWithFormat:@"%@/%@",lastPathComponent,name]];
            NSDictionary *dic = [self makeBookmarkWithURL:url];
            [bookmarkArr addObject:dic];
        }
        
        return [bookmarkArr count] > 0 ? [bookmarkArr copy] : nil;
    }
    return nil;
}

@end
