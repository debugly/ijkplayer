//
//  MRUtil.h
//  FFmpegTutorial-macOS
//
//  Created by qianlongxu on 2020/12/2.
//

#import <Foundation/Foundation.h>
#import <CoreGraphics/CGImage.h>

NS_ASSUME_NONNULL_BEGIN

@interface MRUtil : NSObject

+ (NSArray <NSString *>*)audioType;
+ (NSArray <NSString *>*)videoType;
+ (NSArray <NSString *>*)subtitleType;
+ (NSArray <NSString *>*)pictureType;
+ (NSArray <NSString *>*)acceptMediaType;

+ (BOOL)saveImageToFile:(CGImageRef)img path:(NSString *)imgPath;

@end

NS_ASSUME_NONNULL_END
