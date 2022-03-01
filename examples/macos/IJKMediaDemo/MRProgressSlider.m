//
//  MRProgressSlider.m
//  IJKMediaDemo
//
//  Created by Matt Reach on 2021/5/17.
//  Copyright © 2021 IJK Mac 版. All rights reserved.
//

#import "MRProgressSlider.h"

@interface MRProgressSlider ()
{
    NSTrackingArea *_trackingArea;
}
//使用 [0-1.0] 表示进度
@property (nonatomic, assign) CGFloat progressValue;

@end

//IB_DESIGNABLE
@implementation MRProgressSlider
{
    BOOL _isMouseDown;
    void (^draggedIndicatorHandler)(double progress,MRProgressSlider* indicator,BOOL isEndDrag);
    NSRect _knobRect;
    BOOL _isHover;
}

@synthesize tag = _tag;

- (void)_init
{
    if (!_mainColor) {
        _mainColor = [NSColor colorWithWhite:60/255.0 alpha:1.0];
    }
    
    if (!_bgColor) {
        _bgColor = [NSColor colorWithWhite:100/255.0 alpha:1.0];
    }
    _knobRect = CGRectZero;
    _userInteraction = YES;
    //test
    //[self setWantsLayer:YES];
    //self.layer.backgroundColor = [[NSColor blackColor] CGColor];
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }
    
    if (!_trackingArea) {
        _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                     options:(NSTrackingMouseEnteredAndExited |
                                                              NSTrackingMouseMoved |
                                                              NSTrackingActiveInActiveApp |
                                                              NSTrackingInVisibleRect |
                                                              NSTrackingAssumeInside |
                                                              NSTrackingCursorUpdate |
                                                              NSTrackingEnabledDuringMouseDrag)
                                                       owner:self
                                                    userInfo:nil];
        [self addTrackingArea:_trackingArea];
    }
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self _init];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    self = [super initWithCoder:decoder];
    if (self) {
        [self _init];
    }
    return self;
}

#pragma mark -
#pragma mark Setters

- (void)setMaxValue:(CGFloat)maxValue
{
    if (_maxValue != maxValue) {
        _maxValue = maxValue;
        [self setNeedsDisplay:YES];
    }
}

- (void)setMinValue:(CGFloat)minValue
{
    if (_minValue != minValue) {
        _minValue = minValue;
        [self setNeedsDisplay:YES];
    }
}

- (void)setProgressValue:(CGFloat)playedValue
{
    if (_progressValue != playedValue) {
        _progressValue = playedValue;
        [self setNeedsDisplay:YES];
    }
}

- (void)setCurrentValue:(CGFloat)currentValue
{
    if (!_isMouseDown) {
        CGFloat denominator = (_maxValue - _minValue);
        if (denominator > 0) {
            self.progressValue = (currentValue - _minValue) / denominator;
        }
    }
}

- (CGFloat)currentValue
{
    const CGFloat denominator = (_maxValue - _minValue);
    
    if (denominator > 0) {
        if (self.isSegment) {
            float p = (1 / (float)self.segementCount);
            int index = roundf(_progressValue/p);
            return _minValue + (_maxValue - _minValue) / (self.segementCount)*index;
        } else {
            return _minValue + (_maxValue - _minValue) * _progressValue;
        }
    } else {
        return 0.0;
    }
}

- (void)setMainColor:(NSColor *)playedColor
{
    if (_mainColor != playedColor) {
        _mainColor = playedColor;
        [self setNeedsDisplay:YES];
    }
}

- (void)setBgColor:(NSColor *)unLoadColor
{
    if (_bgColor != unLoadColor) {
        unLoadColor = unLoadColor;
        [self setNeedsDisplay:YES];
    }
}

#pragma mark -
#pragma mark DrawRect

- (CGSize)knobNormalSize
{
    const NSRect viewBounds = self.bounds;
    if (self.useVertical) {
        const CGFloat knobWidth = ceilf(CGRectGetWidth(viewBounds) / 1.4);
        return CGSizeMake(knobWidth, knobWidth);
    } else {
        const CGFloat knobHeight = ceilf(CGRectGetHeight(viewBounds) / 1.4);
        CGSize imgSize = self.knobImage.size;
        return CGSizeMake((knobHeight / imgSize.height) * imgSize.width, knobHeight);
    }
}

- (CGSize)knobScaleSize
{
    CGSize knobSize = [self knobNormalSize];
    knobSize = CGSizeMake(knobSize.width * 1.4, knobSize.height * 1.4);
    return knobSize;
}

- (CGRect)cellRect
{
    const CGFloat knobWidth = [self knobScaleSize].width;
    const CGFloat knobWidth_2 = knobWidth / 2.0;
    
    if (self.useVertical) {
        NSRect cellRect = self.bounds;
        cellRect.size.height = cellRect.size.height - knobWidth;
        cellRect.origin.y = cellRect.origin.y + knobWidth_2;
        return cellRect;
    } else {
        NSRect cellRect = self.bounds;
        cellRect.size.width = cellRect.size.width - knobWidth;
        cellRect.origin.x = cellRect.origin.x + knobWidth_2;
        return cellRect;
    }
}

- (BOOL)isSegment
{
    //最少分2断
    return self.segementCount >= 2;
}

- (void)drawRect:(NSRect)dirtyRect
{
    const CGRect cellRect = [self cellRect];
    CGRect slideRect = cellRect;
    if (self.useVertical) {
        slideRect.size.width = 4;
        slideRect.origin.x += (CGRectGetWidth(cellRect) - CGRectGetWidth(slideRect)) / 2.0;
    } else {
        slideRect.size.height = 4;
        slideRect.origin.y += (CGRectGetHeight(cellRect) - CGRectGetHeight(slideRect)) / 2.0;
    }
    
    const CGFloat sliderLength = self.useVertical ? CGRectGetHeight(slideRect) : CGRectGetWidth(slideRect);
    
    const CGFloat radius = self.useVertical ? CGRectGetWidth(slideRect) / 2.0 : CGRectGetHeight(slideRect) / 2.0;
    
    // draw background color
    {
        [_bgColor set];
        NSRect bgRect = slideRect;
        [[NSBezierPath bezierPathWithRoundedRect:bgRect xRadius:radius yRadius:radius] fill];
    }
    
    CGFloat progressLength = 0.0;
    
    progressLength = _progressValue * sliderLength;
    
    // draw main background color
    {
        [_mainColor set];
        NSRect knobRect = slideRect;
        if (self.useVertical) {
            knobRect.size.height = progressLength;
        } else {
            knobRect.size.width = progressLength;
        }
        [[NSBezierPath bezierPathWithRoundedRect:knobRect xRadius:radius yRadius:radius] fill];
    }
    
    // draw indicator
    {
        CGSize knobSize = _isMouseDown && !self.hoverknobImage ? [self knobScaleSize] : [self knobNormalSize];
        
        if (self.useVertical) {
            CGFloat x = CGRectGetMidX(slideRect) - knobSize.width / 2.0 ;
            _knobRect = NSMakeRect(x, CGRectGetMinY(slideRect) + progressLength - knobSize.height / 2.0, knobSize.width, knobSize.height);
        } else {
            CGFloat y = CGRectGetMidY(slideRect) - knobSize.height / 2.0 ;
            _knobRect = NSMakeRect(CGRectGetMinX(slideRect) + progressLength - knobSize.width / 2.0, y, knobSize.width, knobSize.height);
        }
        if (self.hoverknobImage && _isHover) {
            [[self hoverknobImage] drawInRect:_knobRect];
        } else {
            [[self knobImage] drawInRect:_knobRect];
        }
    }
    
    {
        //分段显示
        if (self.isSegment) {
            for (int i= 0; i <= self.segementCount; i++) {
                CGFloat p = (i / (float)self.segementCount) * (sliderLength-radius * 2);
                CGRect rect = slideRect;
                int padding = 1;
                rect.size.width = slideRect.size.height - padding*2;
                rect.size.height =  rect.size.width;
                rect.origin.x =  p + slideRect.origin.x + padding ;
                rect.origin.y =  rect.origin.y + padding;
                
                NSBezierPath *bezierPath = [NSBezierPath bezierPathWithRoundedRect:rect xRadius: rect.size.height/2.0 yRadius: rect.size.height/2.0];
                if (self.segementColor) {
                    [self.segementColor setFill];
                } else {
                    [[NSColor colorWithWhite:1.0 alpha:0.9] setFill];
                }
                [bezierPath fill];
            }
        }
    }
}

- (BOOL)convertEventToValue:(NSEvent * _Nonnull)theEvent percent:(double *)outV
{
    const NSPoint thePoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    const NSRect cellRect = [self cellRect];
    if (NSPointInRect(thePoint, cellRect) || NSPointInRect(thePoint, _knobRect))
    {
        double theValue;
        double maxX = self.useVertical ? cellRect.size.height : cellRect.size.width;
        if (maxX <= 0) {
            maxX = NSIntegerMax;
        }
        
        const double position = self.useVertical ? thePoint.y - CGRectGetMinY(cellRect) : thePoint.x - CGRectGetMinX(cellRect);
        if (position < 0.0) {
            theValue = 0.0;
        } else if (position >= maxX) {
            theValue = 1.0;
        } else {
            theValue = position / maxX;
        }
        
        if (outV) {
            *outV = theValue;
        }
        return YES;
    } else {
        return NO;
    }
}

#pragma mark -
#pragma mark Mouse Events

- (void)mouseDown:(NSEvent *)theEvent
{
    if ([self convertEventToValue:theEvent percent:NULL]) {
        _isMouseDown = YES;
        [self mouseDragged:theEvent];
    }
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    if (_isMouseDown)
    {
        double theValue;
        if ([self convertEventToValue:theEvent percent:&theValue]) {
            self.progressValue = theValue;
            if (draggedIndicatorHandler) {
                draggedIndicatorHandler(self.currentValue,self,NO);
            }
        }
    }
}

- (void)draggedIndicatorHandler
{
    if (_isMouseDown) {
        _isMouseDown = NO;
        [self setNeedsDisplay:YES];
        if (draggedIndicatorHandler) {
            [self setCurrentValue:self.currentValue];
            draggedIndicatorHandler(self.currentValue,self,YES);
        }
    }
}

- (void)mouseUp:(NSEvent *)theEvent
{
    [self draggedIndicatorHandler];
}

- (void)mouseEntered:(NSEvent *)event
{
    _isHover = YES;
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent *)event
{
    _isHover = NO;
    [self setNeedsDisplay:YES];
    [self draggedIndicatorHandler];
}

- (BOOL)mouseDownCanMoveWindow
{
    return NO;
}

- (void)onDraggedIndicator:(void (^)(double,MRProgressSlider*,BOOL))handler
{
    draggedIndicatorHandler = handler;
}

- (int)currentIndex
{
    if (self.isSegment) {
        return (self.currentValue - self.minValue) / ((self.maxValue - self.minValue) / self.segementCount);
    }
    return 0;
}

- (NSView *)hitTest:(NSPoint)point
{
    if (self.userInteraction) {
        return [super hitTest:point];
    } else {
        return nil;
    }
}

- (void)setUserInteraction:(BOOL)userInteraction
{
    if (_userInteraction != userInteraction) {
        _userInteraction = userInteraction;
        [self updateTrackingAreas];
    }
}

- (void)cursorUpdate:(NSEvent *)theEvent
{
    if (self.window.ignoresMouseEvents) {
        return;
    }
    [[NSCursor pointingHandCursor] set];
}

@end
