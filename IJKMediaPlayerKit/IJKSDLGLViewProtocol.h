/*
 * IJKSDLGLViewProtocol.h
 *
 * Copyright (c) 2017 Bilibili
 * Copyright (c) 2017 raymond <raymondzheng1412@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef IJKSDLGLViewProtocol_h
#define IJKSDLGLViewProtocol_h
#import <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#import <CoreGraphics/CGImage.h>
typedef NSFont UIFont;
typedef NSColor UIColor;
typedef NSOpenGLView GLView;
typedef NSImage UIImage;
#else
#import <UIKit/UIKit.h>
typedef UIView GLView;
#endif


typedef NS_ENUM(NSInteger, IJKMPMovieScalingMode) {
    IJKMPMovieScalingModeAspectFit,  // Uniform scale until one dimension fits
    IJKMPMovieScalingModeAspectFill, // Uniform scale until the movie fills the visible bounds. One dimension may have clipped contents
    IJKMPMovieScalingModeFill        // Non-uniform scale. Both render dimensions will exactly match the visible bounds
};

typedef struct SDL_VoutOverlay SDL_VoutOverlay;
typedef struct IJKOverlay IJKOverlay;
struct IJKOverlay {
    int w;
    int h;
    UInt32 format;
    int planes;
    UInt16 *pitches;
    UInt8 **pixels;
    int sar_num;
    int sar_den;
    CVPixelBufferRef pixel_buffer;
};

typedef struct _IJKSDLSubtitlePreference IJKSDLSubtitlePreference;
struct _IJKSDLSubtitlePreference {
    int fontSize;
    int32_t color;
    float bottomMargin;//[0.0,1.0]
};

typedef enum _IJKSDLRotateType {
    IJKSDLRotateNone,
    IJKSDLRotateX,
    IJKSDLRotateY,
    IJKSDLRotateZ
} IJKSDLRotateType;


typedef struct _IJKSDLRotatePreference IJKSDLRotatePreference;
struct _IJKSDLRotatePreference {
    IJKSDLRotateType type;
    float degrees;
};

typedef struct _IJKSDLColorConversionPreference IJKSDLColorConversionPreference;
struct _IJKSDLColorConversionPreference {
    float brightness;
    float saturation;
    float contrast;
};

typedef struct _IJKSDLDARPreference IJKSDLDARPreference;
struct _IJKSDLDARPreference {
    float ratio; //ratio is width / height;
};

typedef enum : NSUInteger {
    IJKSDLSnapshot_Origin, //keep original video size,without subtitle and video effect
    IJKSDLSnapshot_Screen, //current glview's picture as you see
    IJKSDLSnapshot_Effect_Origin,//keep original video size,with subtitle,without video effect
    IJKSDLSnapshot_Effect_Subtitle_Origin //keep original video size,with subtitle and video effect
} IJKSDLSnapshotType;

typedef struct _IJKSDLSubtitlePicture IJKSDLSubtitlePicture;
struct _IJKSDLSubtitlePicture {
    int x;
    int y;
    int w;
    int h;
    int nb_colors;
    uint8_t *data[4]; // data[0] - pixels with length w * h, in BGRA pixel format
    int linesize[4];
};

@protocol IJKSDLGLViewProtocol <NSObject>

@property(nonatomic) IJKMPMovieScalingMode scalingMode;
#if TARGET_OS_IOS
@property(nonatomic) CGFloat scaleFactor;
#endif
@property(nonatomic) BOOL isThirdGLView;
/*
 if you update these preference blow, when player paused,
 you can call -[setNeedsRefreshCurrentPic] method let current picture refresh right now.
 */
// subtitle preference
@property(nonatomic) IJKSDLSubtitlePreference subtitlePreference;
// rotate preference
@property(nonatomic) IJKSDLRotatePreference rotatePreference;
// color conversion perference
@property(nonatomic) IJKSDLColorConversionPreference colorPreference;
// user defined display aspect ratio
@property(nonatomic) IJKSDLDARPreference darPreference;
// refresh current video picture and subtitle (when player paused change video pic preference, you can invoke this method)
- (void)setNeedsRefreshCurrentPic;

// private method for jik internal.
- (void)display:(SDL_VoutOverlay *)overlay subtitle:(const char *)subtitle subPict:(IJKSDLSubtitlePicture *)subPict;

#if !TARGET_OS_OSX
- (UIImage *)snapshot;
#else
- (CGImageRef)snapshot:(IJKSDLSnapshotType)aType;
#endif

@optional;//when isThirdGLView,will call display_pixels method.
- (void)display_pixels:(IJKOverlay *)overlay;
//when video size changed will call videoNaturalSizeChanged.
- (void)videoNaturalSizeChanged:(CGSize)size;
//when video z rotate degrees changed will call videoZRotateDegrees.
- (void)videoZRotateDegrees:(NSInteger)degrees;
@end

#endif /* IJKSDLGLViewProtocol_h */
