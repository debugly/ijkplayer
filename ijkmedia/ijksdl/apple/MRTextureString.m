//
// File:        MRTextureString.h
//                (Originally Apple's GLString.h)
//
// Abstract:    Uses Quartz to draw a string into a CVPixelBufferRef
//
// Version:
//          2.0 - use CVPixelBufferRef instead of glTexImage2D; use ARC
//          1.1 - Minor enhancements and bug fixes.
//          1.0 - Original release.
//
//Download Sample Code https://developer.apple.com/library/archive/samplecode/CocoaGL/Introduction/Intro.html
//https://developer.apple.com/library/archive/qa/qa1829/_index.html
//https://stackoverflow.com/questions/46879895/byte-per-row-is-wrong-when-creating-a-cvpixelbuffer-with-width-multiple-of-90
//https://github.com/johnboiles/obs-mac-virtualcam/blob/4bd585204ae220068bd55eddf7239b9c8fd8b1dc/src/dal-plugin/Stream.mm

#import "MRTextureString.h"

// The following is a NSBezierPath category to allow
// for rounded corners of the border
#if TARGET_OS_OSX
#pragma mark -
#pragma mark NSBezierPath Category

@interface NSBezierPath (RoundRect)
+ (NSBezierPath *)bezierPathWithRoundedRect:(NSRect)rect cornerRadius:(float)radius;

- (void)appendBezierPathWithRoundedRect:(NSRect)rect cornerRadius:(float)radius;
@end

@implementation NSBezierPath (RoundRect)

+ (NSBezierPath *)bezierPathWithRoundedRect:(NSRect)rect cornerRadius:(float)radius {
    NSBezierPath *result = [NSBezierPath bezierPath];
    [result appendBezierPathWithRoundedRect:rect cornerRadius:radius];
    return result;
}

- (void)appendBezierPathWithRoundedRect:(NSRect)rect cornerRadius:(float)radius {
    if (!NSIsEmptyRect(rect)) {
		if (radius > 0.0) {
			// Clamp radius to be no larger than half the rect's width or height.
			float clampedRadius = MIN(radius, 0.5 * MIN(rect.size.width, rect.size.height));
			
			NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
			NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
			NSPoint bottomRight = NSMakePoint(NSMaxX(rect), NSMinY(rect));
			
			[self moveToPoint:NSMakePoint(NSMidX(rect), NSMaxY(rect))];
			[self appendBezierPathWithArcFromPoint:topLeft     toPoint:rect.origin radius:clampedRadius];
			[self appendBezierPathWithArcFromPoint:rect.origin toPoint:bottomRight radius:clampedRadius];
			[self appendBezierPathWithArcFromPoint:bottomRight toPoint:topRight    radius:clampedRadius];
			[self appendBezierPathWithArcFromPoint:topRight    toPoint:topLeft     radius:clampedRadius];
			[self closePath];
		} else {
			// When radius == 0.0, this degenerates to the simple case of a plain rectangle.
			[self appendBezierPathWithRect:rect];
		}
    }
}

@end

#endif

#pragma mark -
#pragma mark MRTextureString

// MRTextureString follows

@interface MRTextureString ()

@property(nonatomic, strong) NSAttributedString * attributedString;
@property(nonatomic, assign) BOOL requiresUpdate;

@end

@implementation MRTextureString

#pragma mark -
#pragma mark Initializers

// designated initializer
- (id)initWithAttributedString:(NSAttributedString *)attributedString withBoxColor:(NSColor *)box withBorderColor:(NSColor *)border
{
    self = [super init];
    
    self.attributedString = attributedString;
    
    self.boxColor = box;
    self.borderColor = border;
    self.antialias = YES;
    self.requiresUpdate = YES;
	return self;
}

- (id)initWithString:(NSString *)aString withAttributes:(NSDictionary *)attribs withBoxColor:(NSColor *)box withBorderColor:(NSColor *)border
{
	return [self initWithAttributedString:[[NSAttributedString alloc] initWithString:aString attributes:attribs] withBoxColor:box withBorderColor:border];
}

// basic methods that pick up defaults
- (id)initWithAttributedString:(NSAttributedString *)attributedString;
{
	return [self initWithAttributedString:attributedString withBoxColor:[NSColor colorWithRed:1.0f green:1.0f blue:1.0f alpha:0.0f] withBorderColor:[NSColor colorWithRed:1.0f green:1.0f blue:1.0f alpha:0.0f]];
}

- (id)initWithString:(NSString *)aString withAttributes:(NSDictionary *)attribs
{
	return [self initWithAttributedString:[[NSAttributedString alloc] initWithString:aString attributes:attribs] withBoxColor:[NSColor colorWithRed:1.0f green:1.0f blue:1.0f alpha:0.0f] withBorderColor:[NSColor colorWithRed:1.0f green:1.0f blue:1.0f alpha:0.0f]];
}

#pragma mark -
#pragma mark Accessors


#pragma mark Text Color

- (void)setTextColor:(NSColor *)color // set default text color
{
    NSMutableDictionary * stanStringAttrib = [NSMutableDictionary dictionary];
    [stanStringAttrib setObject:color forKey:NSForegroundColorAttributeName];
    
    NSMutableAttributedString *aAttributedString = [[NSMutableAttributedString alloc]initWithAttributedString:self.attributedString];
    [aAttributedString addAttributes:stanStringAttrib range:NSMakeRange(0, [aAttributedString length])];
    self.attributedString = [aAttributedString copy];
	self.requiresUpdate = YES;
}

- (NSColor *)textColor
{
    __block NSColor *aColor = nil;
    [self.attributedString enumerateAttribute:NSForegroundColorAttributeName inRange:NSMakeRange(0, [self.attributedString length]) options:0 usingBlock:^(id  _Nullable value, NSRange range, BOOL * _Nonnull stop) {
        if (value) {
            aColor = value;
            *stop = YES;
        }
    }];
	return aColor;
}

#pragma mark Box Color

- (void)setBoxColor:(NSColor *)boxColor
{
    if (_boxColor != boxColor) {
        _boxColor = boxColor;
        self.requiresUpdate = YES;
    }
}

#pragma mark Border Color

- (void)setBorderColor:(NSColor *)borderColor
{
    if (_borderColor != borderColor) {
        _borderColor = borderColor;
        self.requiresUpdate = YES;
    }
}

#pragma mark Margin Size

// these will force the texture to be regenerated at the next draw
- (void)setEdgeInsets:(NSEdgeInsets)edgeInsets
{
    if (!NSEdgeInsetsEqual(_edgeInsets, edgeInsets)) {
        _edgeInsets = edgeInsets;
        self.requiresUpdate = YES;
    }
}

#pragma mark Antialiasing

- (void)setAntialias:(BOOL)antialias
{
    if (_antialias != antialias) {
        _antialias = antialias;
        self.requiresUpdate = YES;
    }
}

#pragma mark String

- (CGSize)size
{
    //on retina screen auto return 2x size.
    CGSize frameSize = [self.attributedString size]; // current string size
    return CGSizeMake(ceilf(frameSize.width), ceilf(frameSize.height));
}

- (void)setAttributedString:(NSAttributedString *)attributedString
{
    if (_attributedString != attributedString) {
        NSRange fullRange = NSMakeRange(0, [attributedString.string length]);
        if (![attributedString attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:&fullRange]) {
            NSMutableParagraphStyle *pghStyle = [[NSMutableParagraphStyle alloc] init];
            pghStyle.alignment = NSTextAlignmentCenter;
            pghStyle.lineSpacing = 10;
            //pghStyle.lineBreakMode = NSLineBreakByTruncatingTail;
            
            NSMutableAttributedString * myAttributedString = [[NSMutableAttributedString alloc] initWithAttributedString:attributedString];
            [myAttributedString addAttribute:NSParagraphStyleAttributeName value:pghStyle range:fullRange];
            _attributedString = myAttributedString;
        } else {
            _attributedString = attributedString;
        }
        
        self.requiresUpdate = YES;
    }
}

- (void)setString:(NSString *)aString withAttributes:(NSDictionary *)attribs; // set string after initial creation
{
	[self setAttributedString:[[NSAttributedString alloc] initWithString:aString attributes:attribs]];
}

#if TARGET_OS_OSX

- (void)drawBg:(CGSize)bgSize
{
    CGPoint originPoint = CGPointZero;
    NSAffineTransform *transform = nil;
    if (!CGPointEqualToPoint(originPoint, CGPointZero)) {
        transform = [NSAffineTransform transform] ;
        [transform translateXBy:originPoint.x yBy:originPoint.y];
    }
    
    if ([self.boxColor alphaComponent]) { // this should be == 0.0f but need to make sure
        [self.boxColor set];
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(NSMakeRect(0.0f, 0.0f, bgSize.width, bgSize.height) , 0.5, 0.5) cornerRadius:self.cRadius];
        if (transform) {
            [path transformUsingAffineTransform:transform];
        }
        [path fill];
    }
    
    if ([self.borderColor alphaComponent]) {
        [self.borderColor set];
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(NSMakeRect (0.0f, 0.0f, bgSize.width, bgSize.height), 0.5, 0.5)
                                                        cornerRadius:self.cRadius];
        [path setLineWidth:1.0f];
        if (transform) {
            [path transformUsingAffineTransform:transform];
        }
        [path stroke];
    }
}

- (void)drawText:(NSRect)rect
{
    // draw at offset position
//    [self.attributedString drawInRect:rect];
    NSStringDrawingContext *ctx = [[NSStringDrawingContext alloc] init];
    ctx.minimumScaleFactor = [[NSScreen mainScreen] backingScaleFactor];
    [self.attributedString drawWithRect:rect options:NSStringDrawingUsesLineFragmentOrigin context:ctx];
//    不能左右居中
//    [self.attributedString drawAtPoint:NSMakePoint(self.edgeInsets.left + originPoint.x, self.edgeInsets.top + originPoint.y)];
}

- (NSImage *)image
{
    CGSize picSize = [self size];// CGSizeMake(frameSize.width + 20, frameSize.height + 40);
    
    NSImage * image = [[NSImage alloc] initWithSize:picSize];
    [image lockFocus];
    
    [[NSGraphicsContext currentContext] setShouldAntialias:self.antialias];
    
    float width = picSize.width;
    float height = picSize.height;
    
    width  += self.edgeInsets.left + self.edgeInsets.right; // add padding
    height += self.edgeInsets.top + self.edgeInsets.bottom; // add padding
    
    [self drawBg:(CGSize){width,height}];
    
    NSRect rect = NSMakeRect(self.edgeInsets.left, self.edgeInsets.top, picSize.width, picSize.height);
    [self drawText:rect];
    
    [image unlockFocus];
    return image;
}

- (CVPixelBufferRef)createPixelBuffer
{
    CGSize picSize = [self size];
    //(width = 285914, height = 397)
    if (picSize.width > 40960 || picSize.height > 40960) {
        return NULL;
    }
     
    NSDictionary *options = [NSDictionary dictionaryWithObjectsAndKeys:
                             [NSNumber numberWithBool:YES], kCVPixelBufferOpenGLCompatibilityKey,
                             [NSNumber numberWithBool:YES], kCVPixelBufferCGImageCompatibilityKey,
                             [NSNumber numberWithBool:YES], kCVPixelBufferCGBitmapContextCompatibilityKey,
                             [NSDictionary dictionary],kCVPixelBufferIOSurfacePropertiesKey,
        nil];
    CVPixelBufferRef pxbuffer = NULL;
     
    size_t height = (size_t)picSize.height;
    size_t width  = (size_t)picSize.width;
    
    width  += self.edgeInsets.left + self.edgeInsets.right; // add padding
    height += self.edgeInsets.top  + self.edgeInsets.bottom; // add padding
    
    CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, width,
            height, kCVPixelFormatType_32BGRA, (__bridge CFDictionaryRef)options,
            &pxbuffer);
     
    NSParameterAssert(status == kCVReturnSuccess && pxbuffer != NULL);
     
    CVPixelBufferLockBaseAddress(pxbuffer, 0);
    void *pxdata = CVPixelBufferGetBaseAddress(pxbuffer);
    NSParameterAssert(pxdata != NULL);
     
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    size_t bpr = CVPixelBufferGetBytesPerRow(pxbuffer);//not use 4 * width
    CGContextRef context = CGBitmapContextCreate(pxdata, width, height, 8, bpr, rgbColorSpace, kCGImageAlphaPremultipliedLast);
    NSParameterAssert(context);
    
    NSGraphicsContext *graphicsContext = [NSGraphicsContext graphicsContextWithCGContext:context flipped:NO];
    [graphicsContext setShouldAntialias:self.antialias];
    [graphicsContext setImageInterpolation:NSImageInterpolationHigh];
    
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:graphicsContext];
    
//    [self drawBg:(CGSize){width,height}];
    
    NSRect rect = NSMakeRect(self.edgeInsets.left, self.edgeInsets.top, picSize.width, picSize.height);
    [self drawText:rect];
    
    [NSGraphicsContext restoreGraphicsState];
    
    CGColorSpaceRelease(rgbColorSpace);
    CGContextRelease(context);
    CVPixelBufferUnlockBaseAddress(pxbuffer, 0);
    
    return pxbuffer;
}

#else

- (void)drawMySelf:(CGSize)picSize
{
    CGPoint originPoint = CGPointZero;
    
    CGAffineTransform transform = CGAffineTransformIdentity;
    if (CGPointEqualToPoint(CGPointZero, originPoint)) {
        transform = CGAffineTransformMakeTranslation(originPoint.x, originPoint.y);
    }
    
    if (CGColorGetAlpha(self.boxColor.CGColor)) { // this should be == 0.0f but need to make sure
        [self.boxColor set];
        UIBezierPath *path = [UIBezierPath bezierPathWithRoundedRect:CGRectInset(CGRectMake (0.0f, 0.0f, picSize.width, picSize.height) , 0.5, 0.5) cornerRadius:0];
        if (!CGAffineTransformIsIdentity(transform)) {
            [path applyTransform:transform];
        }
        [path fill];
    }
    
    if (CGColorGetAlpha(self.borderColor.CGColor)) {
        [self.borderColor set];
        
        UIBezierPath *path = [UIBezierPath bezierPathWithRoundedRect:CGRectInset(CGRectMake (0.0f, 0.0f, picSize.width, picSize.height) , 0.5, 0.5) cornerRadius:self.cRadius];
        [path setLineWidth:1.0f];
        if (!CGAffineTransformIsIdentity(transform)) {
            [path applyTransform:transform];
        }
        [path stroke];
    }
    
    // draw at offset position
    [self.attributedString drawAtPoint:CGPointMake(self.edgeInsets.left + originPoint.x, self.edgeInsets.top + originPoint.y)];
}

- (CVPixelBufferRef)createPixelBuffer
{
    CGSize picSize = [self size];

    NSDictionary* options = @{
        (__bridge NSString*)kCVPixelBufferOpenGLESCompatibilityKey : @YES,
        (__bridge NSString*)kCVPixelBufferIOSurfaceOpenGLESTextureCompatibilityKey : [NSDictionary dictionary]
#if !TARGET_OS_SIMULATOR && TARGET_OS_IOS
        ,(__bridge NSString*)kCVPixelBufferIOSurfacePropertiesKey: [NSDictionary dictionary]
#endif
    };
    
    CVPixelBufferRef pxbuffer = NULL;
     
    size_t height = (size_t)picSize.height;
    size_t width  = (size_t)picSize.width;
    
    CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, width,
            height, kCVPixelFormatType_32BGRA, (__bridge CFDictionaryRef)options,
            &pxbuffer);
     
    NSParameterAssert(status == kCVReturnSuccess && pxbuffer != NULL);
     
    CVPixelBufferLockBaseAddress(pxbuffer, 0);
    void *pxdata = CVPixelBufferGetBaseAddress(pxbuffer);
     
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    size_t bpr = CVPixelBufferGetBytesPerRow(pxbuffer);//not use 4 * width
    CGContextRef context = CGBitmapContextCreate(pxdata, width, height, 8, bpr, rgbColorSpace, kCGImageAlphaPremultipliedLast);
    
    UIGraphicsPushContext(context);
    [self drawMySelf:picSize];
    UIGraphicsPopContext();
    
    CGColorSpaceRelease(rgbColorSpace);
    CGContextRelease(context);
    
    CVPixelBufferUnlockBaseAddress(pxbuffer, 0);
    
    return pxbuffer;
}

#endif

@end
