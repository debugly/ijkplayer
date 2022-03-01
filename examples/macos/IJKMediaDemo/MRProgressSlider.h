//
//  MRProgressSlider.h
//  IJKMediaDemo
//
//  Created by Matt Reach on 2021/5/17.
//  Copyright © 2021 IJK Mac 版. All rights reserved.
//

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface MRProgressSlider : NSView

@property (nonatomic, strong) IBInspectable NSImage * knobImage;
@property (nonatomic, strong) IBInspectable NSImage * hoverknobImage;
//主色调
@property (nonatomic, strong) IBInspectable NSColor *mainColor;
//背景颜色
@property (nonatomic, strong) IBInspectable NSColor *bgColor;
//进度[min-max];
@property (nonatomic, assign) IBInspectable CGFloat currentValue;
//最大值
@property (nonatomic, assign) IBInspectable CGFloat maxValue;
//最小值
@property (nonatomic, assign) IBInspectable CGFloat minValue;

//分段个数和颜色
@property (nonatomic, assign) IBInspectable NSInteger segementCount;
@property (nonatomic, strong) IBInspectable NSColor *segementColor;

@property (assign, nonatomic) BOOL userInteraction;
@property (readwrite) NSInteger tag;
//默认横向，可以使用竖向的
@property (assign, nonatomic) BOOL useVertical;
//停止拖拽时回调
- (void)onDraggedIndicator:(void(^)(double progress,MRProgressSlider *indicator,BOOL isEndDrag))handler;
- (int)currentIndex;

@end

NS_ASSUME_NONNULL_END
