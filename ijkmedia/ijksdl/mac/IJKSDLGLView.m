/*
 * IJKSDLGLView.m
 *
 * Copyright (c) 2013 Bilibili
 * Copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
 *
 * based on https://github.com/kolyvan/kxmovie
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

#import "IJKSDLGLView.h"
#import "ijksdl/ijksdl_timer.h"
#import <CoreVideo/CVDisplayLink.h>
#import "ijksdl/ijksdl_gles2.h"
#import <CoreVideo/CoreVideo.h>
#import "ijksdl_vout_overlay_videotoolbox.h"
#import <AVFoundation/AVFoundation.h>
#import "renderer_pixfmt.h"
#import "MRTextureString.h"
#import "IJKMediaPlayback.h"

@interface IJKSDLGLView()

@property(atomic) CVPixelBufferRef currentVideoPic;
@property(atomic) CVPixelBufferRef currentSubtitle;
@property(atomic) NSString *subtitle;
@property(atomic) CGSize videoNaturalSize;
@property(atomic) int videoDegrees;

@end

@implementation IJKSDLGLView{
    IJK_GLES2_Renderer *_renderer;
    int                 _rendererGravity;
    GLint               _backingWidth;
    GLint               _backingHeight;
    //for snapshot.
    CGSize _FBOTextureSize;
    GLuint _FBO;
    GLuint _ColorTexture;
    float _videoSar;
    
    BOOL _subtitlePreferenceChanged;
}

@synthesize scalingMode = _scalingMode;
@synthesize isThirdGLView = _isThirdGLView;
// subtitle preference
@synthesize subtitlePreference = _subtitlePreference;
// rotate preference
@synthesize rotatePreference = _rotatePreference;
// color conversion perference
@synthesize colorPreference = _colorPreference;
// user defined display aspect ratio
@synthesize darPreference = _darPreference;

- (void)dealloc
{
    if (self.currentVideoPic) {
        CVPixelBufferRelease(self.currentVideoPic);
        self.currentVideoPic = NULL;
    }
    
    if (self.currentSubtitle) {
        CVPixelBufferRelease(self.currentSubtitle);
        self.currentSubtitle = NULL;
    }
    
    [self destroyFBO];
    
    if (_renderer) {
        IJK_GLES2_Renderer_freeP(&_renderer);
    }
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _videoSar = 1.0;
        [self setup];
        _subtitlePreference = (IJKSDLSubtitlePreference){25, 0xFFFFFF, 0.1};
        _rotatePreference   = (IJKSDLRotatePreference){IJKSDLRotateNone, 0.0};
        _colorPreference    = (IJKSDLColorConversionPreference){1.0, 1.0, 1.0};
        _darPreference      = (IJKSDLDARPreference){0.0};
    }
    return self;
}

- (void)setup
{
    NSOpenGLPixelFormatAttribute attrs[] =
    {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 24,
#if ! USE_LEGACY_OPENGL
        NSOpenGLPFAOpenGLProfile,NSOpenGLProfileVersion3_2Core,
#endif
//        NSOpenGLPFAAllowOfflineRenderers, 1,
        0
    };
   
    NSOpenGLPixelFormat *pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    
    if (!pf)
    {
        ALOGE("No OpenGL pixel format");
        return;
    }
    
    NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
    
#if ESSENTIAL_GL_PRACTICES_SUPPORT_GL3 && defined(DEBUG)
    // When we're using a CoreProfile context, crash if we call a legacy OpenGL function
    // This will make it much more obvious where and when such a function call is made so
    // that we can remove such calls.
    // Without this we'd simply get GL_INVALID_OPERATION error for calling legacy functions
    // but it would be more difficult to see where that function was called.
    CGLEnable([context CGLContextObj], kCGLCECrashOnRemovedFunctions);
#endif
    
    [self setPixelFormat:pf];
    [self setOpenGLContext:context];
    [self setWantsBestResolutionOpenGLSurface:YES];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(videoNaturalSizeChanged:)
                                                 name:IJKMPMovieNaturalSizeAvailableNotification object:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(videoZRotateChanged:)
                                                 name:IJKMPMovieZRotateAvailableNotification object:nil];
}

- (void)videoNaturalSizeChanged:(NSNotification *)notifi
{
    NSString *sizeStr = notifi.userInfo[@"size"];
    CGSize size = NSSizeFromString(sizeStr);
    self.videoNaturalSize = size;
}

- (void)videoZRotateChanged:(NSNotification *)notifi
{
    NSNumber *num = notifi.userInfo[@"degrees"];
    int degrees = [num intValue];
    self.videoDegrees = degrees;
}

- (BOOL)setupRenderer:(SDL_VoutOverlay *)overlay
{
    if (overlay == nil)
        return _renderer != nil;
    
    if (overlay->sar_num > 0 && overlay->sar_den > 0) {
        _videoSar = 1.0 * overlay->sar_num / overlay->sar_den;
    }
    
    if (!IJK_GLES2_Renderer_isValid(_renderer) ||
        !IJK_GLES2_Renderer_isFormat(_renderer, overlay->format)) {
        
        IJK_GLES2_Renderer_reset(_renderer);
        IJK_GLES2_Renderer_freeP(&_renderer);
        int openglVer = 330;
    #if USE_LEGACY_OPENGL
        openglVer = 120;
    #endif
        _renderer = IJK_GLES2_Renderer_create(overlay,openglVer);
        if (!IJK_GLES2_Renderer_isValid(_renderer))
            return NO;
        
        if (!IJK_GLES2_Renderer_use(_renderer))
            return NO;
        
        IJK_GLES2_Renderer_setGravity(_renderer, _rendererGravity, _backingWidth, _backingHeight);
        
        IJK_GLES2_Renderer_updateRotate(_renderer, _rotatePreference.type, _rotatePreference.degrees);
        
        IJK_GLES2_Renderer_updateAutoZRotate(_renderer, overlay->auto_z_rotate_degrees);
        
        IJK_GLES2_Renderer_updateSubtitleBottomMargin(_renderer, _subtitlePreference.bottomMargin);
        
        IJK_GLES2_Renderer_updateColorConversion(_renderer, _colorPreference.brightness, _colorPreference.saturation,_colorPreference.contrast);
        
        IJK_GLES2_Renderer_updateUserDefinedDAR(_renderer, _darPreference.ratio);
    }
    
    return YES;
}

- (void)layout
{
    [super layout];
    [self resetViewPort];
}

- (void)reshape
{
    [super reshape];
    [self resetViewPort];
}

- (void)resetViewPort
{
    NSRect viewRectPixels = [self convertRectToBacking:[self bounds]];
    _backingWidth  = viewRectPixels.size.width;
    _backingHeight = viewRectPixels.size.height;
    if (IJK_GLES2_Renderer_isValid(_renderer)) {
        IJK_GLES2_Renderer_setGravity(_renderer, _rendererGravity, _backingWidth, _backingHeight);
    }
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    [self resetViewPort];
}

- (void)setNeedsRefreshCurrentPic
{
    [[self openGLContext] makeCurrentContext];
    CGLLockContext([[self openGLContext] CGLContextObj]);
    // Bind the FBO to screen.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, _backingWidth, _backingHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (_renderer) {
        //for video
        if (self.currentVideoPic) {
            if (!IJK_GLES2_Renderer_updateVetex(_renderer, NULL))
                ALOGE("[GL] Renderer_updateVetex failed\n");
            
            if (!IJK_GLES2_Renderer_uploadTexture(_renderer, (void *)self.currentVideoPic))
                ALOGE("[GL] Renderer_updateVetex failed\n");
            
            IJK_GLES2_Renderer_drawArrays();
        }
        
        //for subtitle
        if (_subtitlePreferenceChanged) {
            [self _generateSubtitlePixel:self.subtitle];
        }
        
        if (self.currentSubtitle) {
            IJK_GLES2_Renderer_beginDrawSubtitle(_renderer);
            IJK_GLES2_Renderer_updateSubtitleVetex(_renderer, CVPixelBufferGetWidth(self.currentSubtitle), CVPixelBufferGetHeight(self.currentSubtitle));
            if (!IJK_GLES2_Renderer_uploadSubtitleTexture(_renderer, (void *)self.currentSubtitle))
                ALOGE("[GL] GLES2 Render Subtitle failed\n");
            IJK_GLES2_Renderer_drawArrays();
            IJK_GLES2_Renderer_endDrawSubtitle(_renderer);
        }
    } else {
        ALOGW("IJKSDLGLView: not ready.\n");
    }
   
    CGLFlushDrawable([[self openGLContext] CGLContextObj]);
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

- (void)_generateSubtitlePixel:(NSString *)subtitle
{
    if (subtitle.length == 0) {
        return;
    }
    
    IJKSDLSubtitlePreference sp = self.subtitlePreference;
        
    int fontSize = sp.fontSize;
    int32_t bgrValue = sp.color;
    //以800为标准，定义出字幕字体默认大小为25pt
    float ratio = 1.0;
    if (!CGSizeEqualToSize(self.videoNaturalSize, CGSizeZero)) {
        int degrees = self.videoDegrees;
        if (degrees / 90 % 2 == 1) {
            ratio = self.videoNaturalSize.height / 800.0;
        } else {
            ratio = self.videoNaturalSize.width / 800.0;
        }
    }
    //字幕默认配置
    NSMutableDictionary * attributes = [[NSMutableDictionary alloc] init];
    
    UIFont *subtitleFont = [UIFont systemFontOfSize:fontSize * ratio];
    [attributes setObject:subtitleFont forKey:NSFontAttributeName];
    
    NSColor *subtitleColor = [NSColor colorWithRed:((float)(bgrValue & 0xFF)) / 255.0 green:((float)((bgrValue & 0xFF00) >> 8)) / 255.0 blue:(float)(((bgrValue & 0xFF0000) >> 16)) / 255.0 alpha:1.0];
    
    [attributes setObject:subtitleColor forKey:NSForegroundColorAttributeName];
    
    MRTextureString *textureString = [[MRTextureString alloc] initWithString:subtitle withAttributes:attributes withBoxColor:[NSColor colorWithRed:0.5f green:0.5f blue:0.5f alpha:0.5f] withBorderColor:nil];
    
    float inset = subtitleFont.pointSize / 2.0;
    textureString.edgeInsets = NSEdgeInsetsMake(inset, inset, inset, inset);
    textureString.cRadius = inset / 2.0;
    
    if (self.currentSubtitle) {
        CVPixelBufferRelease(self.currentSubtitle);
        self.currentSubtitle = NULL;
    }
    
    self.currentSubtitle = [textureString createPixelBuffer];
    
    _subtitlePreferenceChanged = NO;
}

- (void)display:(SDL_VoutOverlay *)overlay subtitle:(const char *)text
{
    if (!overlay) {
        ALOGW("IJKSDLGLView: overlay is nil\n");
        return;
    }
    
    if (text && strlen(text) > 0) {
        NSString *subStr = [[NSString alloc] initWithUTF8String:text];
        if (_subtitlePreferenceChanged || ![subStr isEqualToString:self.subtitle]) {
            [self _generateSubtitlePixel:subStr];
            self.subtitle = subStr;
        }
    } else {
        if (self.currentSubtitle) {
            CVPixelBufferRelease(self.currentSubtitle);
            self.currentSubtitle = NULL;
        }
        self.subtitle = nil;
    }
    
    [[self openGLContext] makeCurrentContext];
    CGLLockContext([[self openGLContext] CGLContextObj]);
    // Bind the FBO to screen.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, _backingWidth, _backingHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if ([self setupRenderer:overlay] && _renderer) {
        //for video
        if (self.currentVideoPic) {
            CVPixelBufferRelease(self.currentVideoPic);
            self.currentVideoPic = NULL;
        }
        
        CVPixelBufferRef videoPic = (CVPixelBufferRef)IJK_GLES2_Renderer_getVideoImage(_renderer, overlay);
        if (videoPic) {
            
            if (!IJK_GLES2_Renderer_updateVetex(_renderer, overlay))
                ALOGE("[GL] Renderer_updateVetex failed\n");
            
            if (!IJK_GLES2_Renderer_uploadTexture(_renderer, (void *)videoPic))
                ALOGE("[GL] Renderer_updateVetex failed\n");
            
            IJK_GLES2_Renderer_drawArrays();
            
            self.currentVideoPic = CVPixelBufferRetain(videoPic);
        }
        
        //for subtitle
        if (self.currentSubtitle) {
            IJK_GLES2_Renderer_beginDrawSubtitle(_renderer);
            IJK_GLES2_Renderer_updateSubtitleVetex(_renderer, CVPixelBufferGetWidth(self.currentSubtitle), CVPixelBufferGetHeight(self.currentSubtitle));
            if (!IJK_GLES2_Renderer_uploadSubtitleTexture(_renderer, (void *)self.currentSubtitle))
                ALOGE("[GL] GLES2 Render Subtitle failed\n");
            IJK_GLES2_Renderer_drawArrays();
            IJK_GLES2_Renderer_endDrawSubtitle(_renderer);
        }
    } else {
        ALOGW("IJKSDLGLView: not ready.\n");
    }
   
    CGLFlushDrawable([[self openGLContext] CGLContextObj]);
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

- (void)initGL
{
    // The reshape function may have changed the thread to which our OpenGL
    // context is attached before prepareOpenGL and initGL are called.  So call
    // makeCurrentContext to ensure that our OpenGL context current to this
    // thread (i.e. makeCurrentContext directs all OpenGL calls on this thread
    // to [self openGLContext])
    [[self openGLContext] makeCurrentContext];
    
    // Synchronize buffer swaps with vertical refresh rate
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
}

- (void)prepareOpenGL
{
    [super prepareOpenGL];

    // Make all the OpenGL calls to setup rendering
    //  and build the necessary rendering objects
    [self initGL];
}

- (void)windowWillClose:(NSNotification*)notification
{
    // Stop the display link when the window is closing because default
    // OpenGL render buffers will be destroyed.  If display link continues to
    // fire without renderbuffers, OpenGL draw calls will set errors.
    // todo
}

#pragma mark - for snapshot

- (void)destroyFBO
{
    glDeleteFramebuffers(1, &_FBO);
    glDeleteFramebuffers(1, &_ColorTexture);
    _FBOTextureSize = CGSizeZero;
}

// Create texture and framebuffer objects to render and snapshot.
- (BOOL)prepareFBOIfNeed:(CGSize)size
{
    if (CGSizeEqualToSize(CGSizeZero, size)) {
        return NO;
    }
    
    if (CGSizeEqualToSize(_FBOTextureSize, size)) {
        return YES;
    } else {
        [self destroyFBO];
    }
    
    // Create a texture object that you apply to the model.
    glGenTextures(1, &_ColorTexture);
    glBindTexture(GL_TEXTURE_2D, _ColorTexture);

    // Set up filter and wrap modes for the texture object.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // Allocate a texture image to which you can render to. Pass `NULL` for the data parameter
    // becuase you don't need to load image data. You generate the image by rendering to the texture.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width, size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, _FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _ColorTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        _FBOTextureSize = size;
        return YES;
    } else {
    #if DEBUG
        NSAssert(NO, @"Failed to make complete framebuffer object %x.",  glCheckFramebufferStatus(GL_FRAMEBUFFER));
    #endif
        return NO;
    }
}

- (CGImageRef)_snapshotEffectOriginWithSubtitle:(BOOL)containSub
{
    CGImageRef img = NULL;
    [[self openGLContext] makeCurrentContext];
    CGLLockContext([[self openGLContext] CGLContextObj]);
    if (self.currentVideoPic && _renderer) {
        CGSize picSize = CGSizeMake(CVPixelBufferGetWidth(self.currentVideoPic) * _videoSar, CVPixelBufferGetHeight(self.currentVideoPic));
        //视频带有旋转 90 度倍数时需要将显示宽高交换后计算
        if (IJK_GLES2_Renderer_isZRotate90oddMultiple(_renderer)) {
            float pic_width = picSize.width;
            float pic_height = picSize.height;
            float tmp = pic_width;
            pic_width = pic_height;
            pic_height = tmp;
            picSize = CGSizeMake(pic_width, pic_height);
        }
        
        //保持用户定义宽高比
        if (self.darPreference.ratio > 0) {
            float pic_width = picSize.width;
            float pic_height = picSize.height;
           
            if (pic_width / pic_height > self.darPreference.ratio) {
                pic_height = pic_width * 1.0 / self.darPreference.ratio;
            } else {
                pic_width = pic_height * self.darPreference.ratio;
            }
            picSize = CGSizeMake(pic_width, pic_height);
        }
        
        if ([self prepareFBOIfNeed:picSize]) {
            if (self.currentVideoPic) {
                // Bind the snapshot FBO and render the scene.
                glBindFramebuffer(GL_FRAMEBUFFER, _FBO);
                glViewport(0, 0, picSize.width, picSize.height);
                glClear(GL_COLOR_BUFFER_BIT);
                // Bind the texture that you previously render to (i.e. the snapshot texture).
                glBindTexture(GL_TEXTURE_2D, _ColorTexture);
                
                if (!IJK_GLES2_Renderer_resetVao(_renderer))
                    ALOGE("[GL] Renderer_resetVao failed\n");
                
                if (!IJK_GLES2_Renderer_uploadTexture(_renderer, (void *)self.currentVideoPic))
                    ALOGE("[GL] Renderer_updateVetex failed\n");
                
                IJK_GLES2_Renderer_drawArrays();
            }
            
            if (self.currentSubtitle && containSub) {
                IJK_GLES2_Renderer_beginDrawSubtitle(_renderer);
                IJK_GLES2_Renderer_updateSubtitleVetex(_renderer, CVPixelBufferGetWidth(self.currentSubtitle), CVPixelBufferGetHeight(self.currentSubtitle));
                if (!IJK_GLES2_Renderer_uploadSubtitleTexture(_renderer, (void *)self.currentSubtitle))
                    ALOGE("[GL] GLES2 Render Subtitle failed\n");
                IJK_GLES2_Renderer_drawArrays();
                IJK_GLES2_Renderer_endDrawSubtitle(_renderer);
            }
            
            img = [self _snapshotTheContextWithSize:picSize];
        }
    }
    CGLFlushDrawable([[self openGLContext] CGLContextObj]);
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
    return img;
}

- (CGImageRef)_snapshot_origin
{
    CVPixelBufferRef pixelBuffer = CVPixelBufferRetain(self.currentVideoPic);
    CIImage *ciImage = [CIImage imageWithCVPixelBuffer:pixelBuffer];
    
    static CIContext *context = nil;
    if (!context) {
        context = [CIContext contextWithOptions:NULL];
    }
    CGRect rect = CGRectMake(0,0,
                             CVPixelBufferGetWidth(pixelBuffer),
                             CVPixelBufferGetHeight(pixelBuffer));
    CGImageRef imageRef = [context createCGImage:ciImage fromRect:rect];
    CVPixelBufferRelease(pixelBuffer);
    return (CGImageRef)CFAutorelease(imageRef);
}

static CGContextRef _CreateCGBitmapContext(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, int bmi)
{
    assert(bpp != 24);
    /*
     AV_PIX_FMT_RGB24 bpp is 24! not supported!
     Crash:
     2020-06-06 00:08:20.245208+0800 FFmpegTutorial[23649:2335631] [Unknown process name] CGBitmapContextCreate: unsupported parameter combination: set CGBITMAP_CONTEXT_LOG_ERRORS environmental variable to see the details
     2020-06-06 00:08:20.245417+0800 FFmpegTutorial[23649:2335631] [Unknown process name] CGBitmapContextCreateImage: invalid context 0x0. If you want to see the backtrace, please set CG_CONTEXT_SHOW_BACKTRACE environmental variable.
     */
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmapContext = CGBitmapContextCreate(
        NULL,
        w,
        h,
        bpc,
        bpr,
        colorSpace,
        bmi
    );
    
    CGColorSpaceRelease(colorSpace);
    if (bitmapContext) {
        return (CGContextRef)CFAutorelease(bitmapContext);
    }
    return NULL;
}

static CGImageRef _FlipCGImage(CGImageRef src)
{
    if (!src) {
        return NULL;
    }
    
    const size_t height = CGImageGetHeight(src);
    const size_t width  = CGImageGetWidth(src);
    const size_t bpc    = CGImageGetBitsPerComponent(src);
    const size_t bpr    = bpc * CGImageGetWidth(src);
    const CGContextRef ctx = _CreateCGBitmapContext(width,
                                              height,
                                              bpc,
                                              CGImageGetBitsPerPixel(src),
                                              bpr,
                                              CGImageGetBitmapInfo(src));
    CGContextTranslateCTM(ctx, 0, height);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), src);
    CGImageRef dst = CGBitmapContextCreateImage(ctx);
    return (CGImageRef)CFAutorelease(dst);
}

- (CGImageRef)_snapshotTheContextWithSize:(const CGSize)size
{
    const int height = size.height;
    const int width  = size.width;
    
    GLint bytesPerRow = width * 4;
    const GLint bitsPerPixel = 32;
    CGContextRef ctx = _CreateCGBitmapContext(width, height, 8, 32, bytesPerRow, kCGBitmapByteOrderDefault |kCGImageAlphaNoneSkipLast);
    if (ctx) {
        void * bitmapData = CGBitmapContextGetData(ctx);
        if (bitmapData) {
            glPixelStorei(GL_PACK_ROW_LENGTH, 8 * bytesPerRow / bitsPerPixel);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bitmapData);
            CGImageRef cgImage = CGBitmapContextCreateImage(ctx);
            if (cgImage) {
                CGImageRef result = _FlipCGImage(cgImage);
                CFRelease(cgImage);
                return result;
            }
        }
    }
    return NULL;
}

- (CGImageRef)_snapshot_screen
{
    NSOpenGLContext *openGLContext = [self openGLContext];
    if (!openGLContext) {
        return nil;
    }
 
    NSRect bounds = [self bounds];
    CGSize size =  [self convertSizeToBacking:bounds.size];;
    
    if (CGSizeEqualToSize(CGSizeZero, size)) {
        return nil;
    }
    [openGLContext makeCurrentContext];
    return [self _snapshotTheContextWithSize:size];
}

- (CGImageRef)snapshot:(IJKSDLSnapshotType)aType
{
    switch (aType) {
        case IJKSDLSnapshot_Origin:
            return [self _snapshot_origin];
        case IJKSDLSnapshot_Screen:
            return [self _snapshot_screen];
        case IJKSDLSnapshot_Effect_Origin:
            return [self _snapshotEffectOriginWithSubtitle:NO];
        case IJKSDLSnapshot_Effect_Subtitle_Origin:
            return [self _snapshotEffectOriginWithSubtitle:YES];
    }
}

#pragma mark - override setter methods

- (void)setScalingMode:(IJKMPMovieScalingMode)scalingMode
{
    switch (scalingMode) {
        case IJKMPMovieScalingModeFill:
            _rendererGravity = IJK_GLES2_GRAVITY_RESIZE;
            break;
        case IJKMPMovieScalingModeAspectFit:
            _rendererGravity = IJK_GLES2_GRAVITY_RESIZE_ASPECT;
            break;
        case IJKMPMovieScalingModeAspectFill:
            _rendererGravity = IJK_GLES2_GRAVITY_RESIZE_ASPECT_FILL;
            break;
    }
    _scalingMode = scalingMode;
    if (IJK_GLES2_Renderer_isValid(_renderer)) {
        IJK_GLES2_Renderer_setGravity(_renderer, _rendererGravity, _backingWidth, _backingHeight);
    }
}

- (void)setRotatePreference:(IJKSDLRotatePreference)rotatePreference
{
    if (_rotatePreference.type != rotatePreference.type || _rotatePreference.degrees != rotatePreference.degrees) {
        _rotatePreference = rotatePreference;
        if (IJK_GLES2_Renderer_isValid(_renderer)) {
            IJK_GLES2_Renderer_updateRotate(_renderer, _rotatePreference.type, _rotatePreference.degrees);
        }
    }
}

- (void)setColorPreference:(IJKSDLColorConversionPreference)colorPreference
{
    if (_colorPreference.brightness != colorPreference.brightness || _colorPreference.saturation != colorPreference.saturation || _colorPreference.contrast != colorPreference.contrast) {
        _colorPreference = colorPreference;
        if (IJK_GLES2_Renderer_isValid(_renderer)) {
            IJK_GLES2_Renderer_updateColorConversion(_renderer, _colorPreference.brightness, _colorPreference.saturation,_colorPreference.contrast);
        }
    }
}

- (void)setDarPreference:(IJKSDLDARPreference)darPreference
{
    if (_darPreference.ratio != darPreference.ratio) {
        _darPreference = darPreference;
        if (IJK_GLES2_Renderer_isValid(_renderer)) {
            IJK_GLES2_Renderer_updateUserDefinedDAR(_renderer, _darPreference.ratio);
        }
    }
}

- (void)setSubtitlePreference:(IJKSDLSubtitlePreference)subtitlePreference
{
    if (_subtitlePreference.bottomMargin != subtitlePreference.bottomMargin) {
        _subtitlePreference = subtitlePreference;
        if (IJK_GLES2_Renderer_isValid(_renderer)) {
            IJK_GLES2_Renderer_updateSubtitleBottomMargin(_renderer, _subtitlePreference.bottomMargin);
        }
    }
    
    if (_subtitlePreference.fontSize != subtitlePreference.fontSize || _subtitlePreference.color != subtitlePreference.color) {
        _subtitlePreference = subtitlePreference;
        _subtitlePreferenceChanged = YES;
    }
}

- (NSView *)hitTest:(NSPoint)point
{
    return nil;
}

- (BOOL)acceptsFirstResponder
{
    return NO;
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

@end
