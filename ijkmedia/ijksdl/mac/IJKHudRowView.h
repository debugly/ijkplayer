//
//  IJKHudRowView.h
//  FFmpegTutorial-macOS
//
//  Created by qianlongxu on 2020/11/27.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

typedef enum : NSUInteger {
    KSeparactorStyleFull,
    KSeparactorStyleHeadPadding,
    KSeparactorStyleNone,
} KSeparactorStyle;

@interface IJKHudRowView : NSTableRowView <NSUserInterfaceItemIdentification>

@property KSeparactorStyle sepStyle;

- (void)updateTitle:(NSString *)title;
- (void)updateDetail:(NSString *)title;

@end

NS_ASSUME_NONNULL_END
