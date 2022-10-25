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

/*
 2022.10.19
 on low macOS (below 10.13) CGLLock is no effect for multiple NSOpenGLContext!
 multiple thread may get lock same time, and block in CGLFlushDrawable function.
 try use apple fence and flushBuffer instead of CGLFlushDrawable are no effect.
 so record IJKSDLGLView's count,when create more than one NSOpenGLContext just dispath all gl* task to main thread execute.
 */

#import "IJKSDLGLView.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import "ijksdl_timer.h"
#import "ijksdl_gles2.h"
#import "ijksdl_vout_overlay_videotoolbox.h"
#import "ijksdl_vout_ios_gles2.h"
#import "MRTextureString.h"
#import "IJKMediaPlayback.h"
#import <OpenGL/glext.h>

static NSHashTable *IJKRefTable() {
    static NSHashTable *refTable = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        refTable = [[NSHashTable alloc] initWithOptions:NSPointerFunctionsWeakMemory capacity:1];
    });
    return refTable;
}

#define ijkRefTable IJKRefTable()

static void _IJK_add_GLView_Ref(id<NSObject>obj)
{
    [ijkRefTable addObject:obj];
}

static void _IJK_minus_GLView_Ref(id<NSObject>obj)
{
    [ijkRefTable removeObject:obj];
}

static bool _has_more_than_one_GLView()
{
    return [ijkRefTable count] > 1;
}

// greather than 10.15 no need dispatch to main.
static bool _is_low_os_version(void)
{
    NSOperatingSystemVersion sysVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
    if (sysVersion.majorVersion > 10) {
        return false;
    } else if (sysVersion.minorVersion >= 15) {
        return false;
    }
    return true;
}

static bool _is_need_dispath_to_main(void)
{
    bool low_os = _is_low_os_version();
    if (low_os) {
        return true;//_has_more_than_one_GLView();
    } else {
        return false;
    }
}

@interface IJKSDLGLView()

@property(atomic) CVPixelBufferRef currentVideoPic;
@property(atomic) CVPixelBufferRef currentSubtitle;
@property(atomic) IJKSDLSubtitle *sub;
@property(nonatomic) NSInteger videoDegrees;
@property(nonatomic) CGSize videoNaturalSize;
//display window size / screen
@property(atomic) float displayScreenScale;
//display window size / video size
@property(atomic) float displayVideoScale;
@property(atomic) GLint backingWidth;
@property(atomic) GLint backingHeight;
@property(atomic) BOOL subtitlePreferenceChanged;
@property(atomic) int  sar_num;
@property(atomic) int  sar_den;
@property(atomic) Uint32 overlayFormat;
@property(atomic) Uint32 ffFormat;
@property(atomic) int zRotateDegrees;
@property(atomic) int overlayH;
@property(atomic) int overlayW;
@property(atomic) int bufferW;
@end

@implementation IJKSDLGLView
{
    IJK_GLES2_Renderer *_renderer;
    int    _rendererGravity;
    //for snapshot.
    CGSize _FBOTextureSize;
    GLuint _FBO;
    GLuint _ColorTexture;
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
@synthesize preventDisplay;

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
    _IJK_minus_GLView_Ref(self);
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _IJK_add_GLView_Ref(self);
        [self setup];
        _sar_den = _sar_num = 0;
        _subtitlePreference = (IJKSDLSubtitlePreference){1.0, 0xFFFFFF, 0.1};
        _rotatePreference   = (IJKSDLRotatePreference){IJKSDLRotateNone, 0.0};
        _colorPreference    = (IJKSDLColorConversionPreference){1.0, 1.0, 1.0};
        _darPreference      = (IJKSDLDARPreference){0.0};
        _displayScreenScale = 1.0;
        _displayVideoScale  = 1.0;
        _rendererGravity    = IJK_GLES2_GRAVITY_RESIZE_ASPECT;
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
    
    ///Fix the default red background color on the Intel platform
    [[self openGLContext] makeCurrentContext];
    glClear(GL_COLOR_BUFFER_BIT);
    [[self openGLContext]flushBuffer];
}

- (void)videoZRotateDegrees:(NSInteger)degrees
{
    self.videoDegrees = degrees;
}

- (void)videoNaturalSizeChanged:(CGSize)size
{
    self.videoNaturalSize = size;
    CGRect viewBounds = [self bounds];
    if (!CGSizeEqualToSize(CGSizeZero, self.videoNaturalSize)) {
        self.displayVideoScale = FFMIN(1.0 * viewBounds.size.width / self.videoNaturalSize.width, 1.0 * viewBounds.size.height / self.videoNaturalSize.height);
    }
}

- (BOOL)setupRendererIfNeed
{
    if (!IJK_GLES2_Renderer_isValid(_renderer) ||
        !IJK_GLES2_Renderer_isFormat(_renderer, self.overlayFormat)) {
        
        IJK_GLES2_Renderer_reset(_renderer);
        IJK_GLES2_Renderer_freeP(&_renderer);
        int openglVer = 330;
    #if USE_LEGACY_OPENGL
        openglVer = 120;
    #endif
        
        _renderer = IJK_GLES2_Renderer_create2(self.overlayFormat,self.ffFormat,openglVer);
        if (!IJK_GLES2_Renderer_isValid(_renderer))
            return NO;
        
        if (!IJK_GLES2_Renderer_use(_renderer))
            return NO;
        
        IJK_GLES2_Renderer_setGravity(_renderer, _rendererGravity, self.backingWidth, self.backingHeight);
        
        IJK_GLES2_Renderer_updateRotate(_renderer, _rotatePreference.type, _rotatePreference.degrees);
        
        IJK_GLES2_Renderer_updateAutoZRotate(_renderer, self.zRotateDegrees);
        
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
    CGSize viewSize = [self bounds].size;
    CGSize viewSizePixels = [self convertSizeToBacking:viewSize];
    
    if (self.backingWidth != viewSizePixels.width || self.backingHeight != viewSizePixels.height) {
        self.backingWidth  = viewSizePixels.width;
        self.backingHeight = viewSizePixels.height;
        
        CGSize screenSize = [[NSScreen mainScreen]frame].size;
        self.displayScreenScale = FFMIN(1.0 * viewSize.width / screenSize.width,1.0 * viewSize.height / screenSize.height);
        if (!CGSizeEqualToSize(CGSizeZero, self.videoNaturalSize)) {
            self.displayVideoScale = FFMIN(1.0 * viewSize.width / self.videoNaturalSize.width,1.0 * viewSize.height / self.videoNaturalSize.height);
        }
        
        if (IJK_GLES2_Renderer_isValid(_renderer)) {
            IJK_GLES2_Renderer_setGravity(_renderer, _rendererGravity, self.backingWidth, self.backingHeight);
        }
    }
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    [self resetViewPort];
}

- (void)doUploadSubtitle
{
    if (self.currentSubtitle) {
        float ratio = 1.0;
        if (self.sub.pixels) {
            ratio = self.subtitlePreference.ratio * self.displayVideoScale * 1.5;
        } else {
            //for text subtitle scale display_scale.
            ratio *= self.displayScreenScale;
        }
        
        IJK_GLES2_Renderer_beginDrawSubtitle(_renderer);
        IJK_GLES2_Renderer_updateSubtitleVetex(_renderer, ratio * CVPixelBufferGetWidth(self.currentSubtitle), ratio * CVPixelBufferGetHeight(self.currentSubtitle));
        if (IJK_GLES2_Renderer_uploadSubtitleTexture(_renderer, (void *)self.currentSubtitle)) {
            IJK_GLES2_Renderer_drawArrays();
        } else {
            ALOGE("[GL] GLES2 Render Subtitle failed\n");
        }
        IJK_GLES2_Renderer_endDrawSubtitle(_renderer);
    }
}

- (void)setFrame:(NSRect)frame
{
    [super setFrame:frame];
}

- (void)doUploadVideoPicture
{
    if (self.currentVideoPic) {
        if (IJK_GLES2_Renderer_updateVetex2(_renderer, self.overlayH, self.overlayW, self.bufferW, self.sar_num, self.sar_den)) {
            if (IJK_GLES2_Renderer_uploadTexture(_renderer, (void *)self.currentVideoPic)) {
                IJK_GLES2_Renderer_drawArrays();
            } else {
                ALOGE("[GL] Renderer_updateVetex failed\n");
            }
        } else {
            ALOGE("[GL] Renderer_updateVetex failed\n");
        }
    }
}

- (void)setNeedsRefreshCurrentPic
{
    if (_is_need_dispath_to_main()) {
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self setNeedsRefreshCurrentPic];
            });
            return;
        }
    }
    CGLLockContext([[self openGLContext] CGLContextObj]);
    [[self openGLContext] makeCurrentContext];
    
    if (IJK_GLES2_Renderer_isValid(_renderer)) {
        // Bind the FBO to screen.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, self.backingWidth, self.backingHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        
        //for video
        [self doUploadVideoPicture];
        
        //for subtitle
        if (self.subtitlePreferenceChanged) {
            if (self.sub.text) {
                [self _generateSubtitlePixel:self.sub.text];
            }
            self.subtitlePreferenceChanged = NO;
        }
        
        [self doUploadSubtitle];
        
        [[self openGLContext]flushBuffer];
    } else {
        ALOGW("IJKSDLGLView: Renderer not ready.\n");
    }
   
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

- (void)_generateSubtitlePixel:(NSString *)subtitle
{
    if (subtitle.length == 0) {
        return;
    }
    
    IJKSDLSubtitlePreference sp = self.subtitlePreference;
        
    float ratio = sp.ratio;
    int32_t bgrValue = sp.color;
    //以800为标准，定义出字幕字体默认大小为30pt
    float scale = 1.0;
    CGSize screenSize = [[NSScreen mainScreen]frame].size;
    
    NSInteger degrees = self.videoDegrees;
    if (degrees / 90 % 2 == 1) {
        scale = screenSize.height / 800.0;
    } else {
        scale = screenSize.width / 800.0;
    }
    //字幕默认配置
    NSMutableDictionary * attributes = [[NSMutableDictionary alloc] init];
    
    UIFont *subtitleFont = [UIFont systemFontOfSize:ratio * scale * 60];
    [attributes setObject:subtitleFont forKey:NSFontAttributeName];
    
    NSColor *subtitleColor = [NSColor colorWithRed:((float)(bgrValue & 0xFF)) / 255.0 green:((float)((bgrValue & 0xFF00) >> 8)) / 255.0 blue:(float)(((bgrValue & 0xFF0000) >> 16)) / 255.0 alpha:1.0];
    
    [attributes setObject:subtitleColor forKey:NSForegroundColorAttributeName];
    
    MRTextureString *textureString = [[MRTextureString alloc] initWithString:subtitle withAttributes:attributes];
    
    if (self.currentSubtitle) {
        CVPixelBufferRelease(self.currentSubtitle);
        self.currentSubtitle = NULL;
    }
    self.currentSubtitle = [textureString createPixelBuffer];
}

- (void)_generateSubtitlePixelFromPicture:(IJKSDLSubtitle*)pict
{
    CVPixelBufferRef pixelBuffer = NULL;
    NSDictionary *options = @{
        (__bridge NSString*)kCVPixelBufferOpenGLCompatibilityKey : @YES,
        (__bridge NSString*)kCVPixelBufferIOSurfacePropertiesKey : [NSDictionary dictionary]
    };
    
    CVReturn ret = CVPixelBufferCreate(kCFAllocatorDefault, pict.w, pict.h, kCVPixelFormatType_32BGRA, (__bridge CFDictionaryRef)options, &pixelBuffer);
    
    NSParameterAssert(ret == kCVReturnSuccess && pixelBuffer != NULL);
    
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    
    uint8_t *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
    int linesize = (int)CVPixelBufferGetBytesPerRow(pixelBuffer);

    uint8_t *dst_data[4] = {baseAddress,NULL,NULL,NULL};
    int dst_linesizes[4] = {linesize,0,0,0};

    const uint8_t *src_data[4] = {pict.pixels,NULL,NULL,NULL};
    const int src_linesizes[4] = {pict.w * 4,0,0,0};

    av_image_copy(dst_data, dst_linesizes, src_data, src_linesizes, AV_PIX_FMT_BGRA, pict.w, pict.h);
    
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    
    if (kCVReturnSuccess == ret) {
        if (self.currentSubtitle) {
            CVPixelBufferRelease(self.currentSubtitle);
            self.currentSubtitle = NULL;
        }
        self.currentSubtitle = pixelBuffer;
    }
}

- (void)_handleSubtitle:(IJKSDLSubtitle *)sub
{
    if (sub.text.length > 0) {
        if (self.subtitlePreferenceChanged || ![self.sub.text isEqualToString:sub.text]) {
            [self _generateSubtitlePixel:sub.text];
            self.subtitlePreferenceChanged = NO;
        }
    } else if (sub.pixels != NULL) {
        if (self.subtitlePreferenceChanged || sub.pixels != self.sub.pixels) {
            [self _generateSubtitlePixelFromPicture:sub];
            self.subtitlePreferenceChanged = NO;
        }
    } else {
        if (self.currentSubtitle) {
            CVPixelBufferRelease(self.currentSubtitle);
            self.currentSubtitle = NULL;
        }
    }
    
    self.sub = sub;
}

- (void)display:(SDL_VoutOverlay *)overlay subtitle:(IJKSDLSubtitle *)sub
{
    if (!overlay) {
        ALOGW("IJKSDLGLView: overlay is nil\n");
        return;
    }
    //overlay is not thread safe, maybe need dispatch from sub thread to main thread,so hold overlay's property to GLView.
    Uint32 overlay_format = overlay->format;
    Uint32 ff_format;
    if (SDL_FCC__VTB == overlay_format) {
        ff_format = overlay->ff_format;
    }
    #if USE_FF_VTB
    else if (SDL_FCC__FFVTB == overlay_format) {
        ff_format = overlay->cv_format;
    }
    #endif
    else {
        ff_format = 0;
        NSAssert(NO, @"wtf?");
    }
    self.ffFormat = ff_format;
    
    self.overlayFormat = overlay_format;
    self.zRotateDegrees = overlay->auto_z_rotate_degrees;
    self.overlayW = overlay->w;
    self.overlayH = overlay->h;
    self.bufferW = SDL_VoutGetBufferWidth(overlay);
    //update video sar.
    if (overlay->sar_num > 0 && overlay->sar_den > 0) {
        self.sar_num = overlay->sar_num;
        self.sar_den = overlay->sar_den;
    }
    
    //replace the current video picture.
    if (self.currentVideoPic) {
        CVPixelBufferRelease(self.currentVideoPic);
        self.currentVideoPic = NULL;
    }
    
    CVPixelBufferRef videoPic = SDL_Overlay_getCVPixelBufferRef(overlay);
    if (videoPic) {
        self.currentVideoPic = CVPixelBufferRetain(videoPic);
    }
    
    //replace the current subtitle.
    [self _handleSubtitle:sub];
    
    if (self.preventDisplay) {
        return;
    }
    
    //when more than one glview dispatch all task to main thread!
    if (_is_need_dispath_to_main()) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self doDisplayVideoPicAndSubtitle];
        });
    } else {
        [self doDisplayVideoPicAndSubtitle];
    }
}

- (void)doDisplayVideoPicAndSubtitle
{
    CGLLockContext([[self openGLContext] CGLContextObj]);
    [[self openGLContext] makeCurrentContext];
    [self setupRendererIfNeed];
    
    if (_renderer) {
        // Bind the FBO to screen.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, self.backingWidth, self.backingHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        
        //for video
        [self doUploadVideoPicture];
        //for subtitle
        [self doUploadSubtitle];
    } else {
        ALOGW("IJKSDLGLView: Renderer not ok.\n");
    }
    
    [[self openGLContext]flushBuffer];
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
    
    glClearColor(0.0, 0.0, 0.0, 1.0f);
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
    if (!self.currentVideoPic) {
        return NULL;
    }
    NSAssert([NSThread isMainThread], @"must be called on main thread.");
    CGLLockContext([[self openGLContext] CGLContextObj]);
    [[self openGLContext] makeCurrentContext];
    [self setupRendererIfNeed];
    CGImageRef img = NULL;
    if (_renderer) {
        float videoSar = 1.0;
        if (self.sar_num > 0 && self.sar_den > 0) {
            videoSar = self.sar_num / self.sar_den;
        }
        CGSize picSize = CGSizeMake(CVPixelBufferGetWidth(self.currentVideoPic) * videoSar, CVPixelBufferGetHeight(self.currentVideoPic));
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
            
            if (containSub) {
                [self doUploadSubtitle];
            }
            img = [self _snapshotTheContextWithSize:picSize];
        }
        [[self openGLContext]flushBuffer];
    }
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
    CGRect bounds = [self bounds];
    CGSize size =  [self convertSizeToBacking:bounds.size];;
    
    if (CGSizeEqualToSize(CGSizeZero, size)) {
        return nil;
    }
    NSOpenGLContext *openGLContext = [self openGLContext];
    if (!openGLContext) {
        return nil;
    }
    CGLLockContext([openGLContext CGLContextObj]);
    [openGLContext makeCurrentContext];
    CGImageRef img = [self _snapshotTheContextWithSize:size];
    CGLUnlockContext([openGLContext CGLContextObj]);
    return img;
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
        IJK_GLES2_Renderer_setGravity(_renderer, _rendererGravity, self.backingWidth, self.backingHeight);
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
    
    if (_subtitlePreference.ratio != subtitlePreference.ratio || _subtitlePreference.color != subtitlePreference.color) {
        _subtitlePreference = subtitlePreference;
        self.subtitlePreferenceChanged = YES;
    }
}

- (void)setBackgroundColor:(uint8_t)r g:(uint8_t)g b:(uint8_t)b
{
    [[self openGLContext] makeCurrentContext];
    glClearColor(r/255.0, g/255.0, b/255.0, 1.0f);
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
