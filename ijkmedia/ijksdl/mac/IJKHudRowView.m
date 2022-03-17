//
//  IJKHudRowView.m
//  FFmpegTutorial-macOS
//
//  Created by qianlongxu on 2020/11/27.
//

#import "IJKHudRowView.h"

@interface IJKHudRowView ()

@property (nonatomic, weak) NSTextField *titleLb;
@property (nonatomic, weak) NSTextField *detailLb;

@end

@implementation IJKHudRowView

- (NSTextField *)createLabel
{
    NSTextField *tx = [[NSTextField alloc] init];
    tx.editable = NO;
    //点击的时候不显示蓝色外框
    tx.focusRingType = NSFocusRingTypeNone;
    tx.bordered = NO;
    tx.backgroundColor = [NSColor clearColor];
    tx.font = [NSFont systemFontOfSize:14];
    tx.textColor = [NSColor whiteColor];
    return tx;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        
        [self setWantsLayer:YES];
        
        NSTextField *titleLb = [self createLabel];
        NSTextField *detailLB = [self createLabel];
        detailLB.alignment = NSTextAlignmentRight;
        
        [self addSubview:titleLb];
        [self addSubview:detailLB];
        
        self.titleLb = titleLb;
        self.detailLb = detailLB;
    }
    return self;
}

- (void)updateTitle:(NSString *)title
{
    if (!title) {
        title = @"";
    }
    self.titleLb.stringValue = title;
}

- (void)updateDetail:(NSString *)title
{
    if (!title) {
        title = @"";
    }
    self.detailLb.stringValue = title;
}

- (void)layout
{
    CGRect frameRect = self.bounds;
    CGFloat padding = 6;
    
    CGFloat minX = padding;
    CGFloat maxX = CGRectGetWidth(frameRect) - padding;
    CGFloat height = CGRectGetHeight(frameRect);
    
    {
        NSRect rect = self.bounds;
        rect.size.width = (maxX - minX)/2.0;
        CGSize labelSize = [self.titleLb sizeThatFits:rect.size];
        rect.origin.x = minX;
        rect.origin.y = (height - labelSize.height)/2.0;
        rect.size = labelSize;
        self.titleLb.frame = rect;
        minX = CGRectGetMaxX(rect);
        minX += padding;
    }
    
    {
        NSRect rect = self.bounds;
        rect.size.width = maxX - minX;
        CGSize labelSize = [self.detailLb sizeThatFits:rect.size];
        rect.origin.x = minX;
        rect.origin.y = (height - labelSize.height)/2.0;
        rect.size.height = labelSize.height;
        self.detailLb.frame = rect;
    }
}

//如果放在drawSeparatorInRect画，必须设置gridStyleMask不为None，但是多余空白行的线会顶到头
- (void)drawBackgroundInRect:(NSRect)dirtyRect
{
    switch (self.sepStyle) {
        case KSeparactorStyleFull:
        case KSeparactorStyleHeadPadding:
        {
            CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
            CGContextSetStrokeColorWithColor(context, [[NSColor colorWithWhite:0.1 alpha:1] CGColor]);
            CGContextSetLineWidth(context, 1);
            //    CGFloat dashArray[] = {3,1};
            //    CGContextSetLineDash(context, 1, dashArray, 1);//跳过3个再画虚线，所以刚开始有6-（3-2）=5个虚点
            CGFloat y = CGRectGetHeight(dirtyRect);
            if (self.sepStyle == KSeparactorStyleFull) {
                CGContextMoveToPoint(context, 0, y);
            } else {
                CGContextMoveToPoint(context, 15, y);
            }
            
            CGContextAddLineToPoint(context, CGRectGetWidth(dirtyRect), y);
            CGContextStrokePath(context);
        }
            break;
        case KSeparactorStyleNone:
        {
            
        }
            break;
    }
}

@end
