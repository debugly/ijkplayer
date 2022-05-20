/*
 * Copyright (c) 2016 Bilibili
 * copyright (c) 2016 Zhang Rui <bbcallen@gmail.com>
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

#include "internal.h"
#if TARGET_OS_OSX
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>
#else
#if __OBJC__
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/EAGL.h>
//https://github.com/lechium/iOS14Headers/blob/c0b7e9f0049c6b2571358584eed67c841140624f/System/Library/Frameworks/OpenGLES.framework/EAGLContext.h
@interface EAGLContext ()
- (BOOL)texImageIOSurface:(IOSurfaceRef)arg1 target:(unsigned long long)arg2 internalFormat:(unsigned long long)arg3 width:(unsigned)arg4 height:(unsigned)arg5 format:(unsigned long long)arg6 type:(unsigned long long)arg7 plane:(unsigned)arg8 invert:(BOOL)arg9;
//begin ios 11
- (BOOL)texImageIOSurface:(IOSurfaceRef)arg1 target:(unsigned long long)arg2 internalFormat:(unsigned long long)arg3 width:(unsigned)arg4 height:(unsigned)arg5 format:(unsigned long long)arg6 type:(unsigned long long)arg7 plane:(unsigned)arg8;
@end

#endif
#endif

#import <CoreVideo/CoreVideo.h>
#include "ijksdl_vout_overlay_videotoolbox.h"
#include "ijksdl_vout_overlay_ffmpeg.h"
#include "renderer_pixfmt.h"

#if TARGET_OS_OSX
#define GL_TEXTURE_TARGET GL_TEXTURE_RECTANGLE
#else
#define GL_TEXTURE_TARGET GL_TEXTURE_2D
#endif

typedef struct _Frame_Size
{
    int w;
    int h;
}Frame_Size;

typedef struct IJK_GLES2_Renderer_Opaque
{
    CFTypeRef             color_attachments;
    GLint                 isSubtitle;
    GLint                 isFullRange;
    int samples;
#if TARGET_OS_OSX
    GLint                 textureDimension[3];
    Frame_Size            frameSize[3];
#endif
} IJK_GLES2_Renderer_Opaque;

static GLboolean use(IJK_GLES2_Renderer *renderer)
{
    ALOGI("use common vtb render\n");
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glEnable(GL_TEXTURE_TARGET);
    glUseProgram(renderer->program);            IJK_GLES2_checkError_TRACE("glUseProgram");
    IJK_GLES2_Renderer_Opaque * opaque = renderer->opaque;
    assert(opaque->samples);
    
    if (0 == renderer->plane_textures[0])
        glGenTextures(opaque->samples, renderer->plane_textures);
    return GL_TRUE;
}

static GLsizei getBufferWidth(IJK_GLES2_Renderer *renderer, SDL_VoutOverlay *overlay)
{
    if (!overlay)
        return 0;

    if (overlay->format == SDL_FCC__VTB || overlay->format == SDL_FCC__FFVTB) {
        return overlay->pitches[0];
    } else {
        assert(0);
    }
}

static GLboolean upload_texture_use_IOSurface(CVPixelBufferRef pixel_buffer,IJK_GLES2_Renderer *renderer)
{
    uint32_t cvpixfmt = CVPixelBufferGetPixelFormatType(pixel_buffer);
    struct vt_format *f = vt_get_gl_format(cvpixfmt);
    if (!f) {
        ALOGE("CVPixelBuffer has unsupported format type\n");
        return GL_FALSE;
    }

    const bool planar = CVPixelBufferIsPlanar(pixel_buffer);
    const int planes  = (int)CVPixelBufferGetPlaneCount(pixel_buffer);
    assert(planar && planes == f->planes || f->planes == 1);
    
    GLenum gl_target = GL_TEXTURE_TARGET;
    
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixel_buffer);
    
    if (!surface) {
        printf("CVPixelBuffer has no IOSurface\n");
        return GL_FALSE;
    }
    
    for (int i = 0; i < f->planes; i++) {
        GLfloat w = (GLfloat)CVPixelBufferGetWidthOfPlane(pixel_buffer, i);
        GLfloat h = (GLfloat)CVPixelBufferGetHeightOfPlane(pixel_buffer, i);

        //设置采样器位置，保证了每个uniform采样器对应着正确的纹理单元
        glUniform1i(renderer->us2_sampler[i], i);
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(gl_target, renderer->plane_textures[i]);
        struct vt_gl_plane_format plane_format = f->gl[i];
#if TARGET_OS_IOS
        if (![[EAGLContext currentContext] texImageIOSurface:surface target:gl_target internalFormat:plane_format.gl_internal_format width:w height:h format:plane_format.gl_format type:plane_format.gl_type plane:i invert:NO]) {
            ALOGE("creating IOSurface texture for plane %d failed.\n",i);
            return GL_FALSE;
        }
//        
//        //(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels)
//        glTexImage2D(gl_target, 0, plane_format.gl_internal_format, w, h, 0, plane_format.gl_format, plane_format.gl_type, CVPixelBufferGetBaseAddressOfPlane(pixel_buffer,i));
        
#else
        Frame_Size size = renderer->opaque->frameSize[i];
        if (size.w != w || size.h != h) {
            glUniform2f(renderer->opaque->textureDimension[i], w, h);
            size.w = w;
            size.h = h;
            renderer->opaque->frameSize[i] = size;
        }
        
        CGLError err = CGLTexImageIOSurface2D(CGLGetCurrentContext(),
                                              gl_target,
                                              plane_format.gl_internal_format,
                                              w,
                                              h,
                                              plane_format.gl_format,
                                              plane_format.gl_type,
                                              surface,
                                              i);
        if (err != kCGLNoError) {
            ALOGE("creating IOSurface texture for plane %d failed: %s\n",
                   i, CGLErrorString(err));
            return GL_FALSE;
        }
#endif
        {
            glTexParameteri(gl_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(gl_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(gl_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(gl_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
    return GL_TRUE;
}

static GLboolean upload_Texture(IJK_GLES2_Renderer *renderer, void *texture)
{
    if (!texture) {
        return GL_FALSE;
    }
    CVPixelBufferRef pixel_buffer = (CVPixelBufferRef)texture;
    
    IJK_GLES2_Renderer_Opaque *opaque = renderer->opaque;
    if (!opaque) {
        return GL_FALSE;
    }
    CVPixelBufferRetain(pixel_buffer);
    if (!opaque->color_attachments) {
        CFTypeRef color_attachments = CVBufferGetAttachment(pixel_buffer, kCVImageBufferYCbCrMatrixKey, NULL);
        if (color_attachments == nil ||
            CFStringCompare(color_attachments, kCVImageBufferYCbCrMatrix_ITU_R_709_2, 0) == kCFCompareEqualTo) {
            color_attachments = kCVImageBufferYCbCrMatrix_ITU_R_709_2;
            glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix_bt709());
        } else if (CFStringCompare(color_attachments, kCVImageBufferYCbCrMatrix_ITU_R_601_4, 0) == kCFCompareEqualTo) {
            glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix_bt601());
        } else {
            glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix_bt709());
        }

        opaque->color_attachments = CFRetain(color_attachments);
        
        int pixel_fmt = CVPixelBufferGetPixelFormatType(pixel_buffer);
        
        //full color range
        if (kCVPixelFormatType_420YpCbCr8BiPlanarFullRange == pixel_fmt ||
            kCVPixelFormatType_420YpCbCr8PlanarFullRange == pixel_fmt) {
            glUniform1i(renderer->opaque->isFullRange, GL_TRUE);
        }
    }
    
    GLboolean uploaded = upload_texture_use_IOSurface(pixel_buffer, renderer);
    
    CVPixelBufferRelease(pixel_buffer);
    return uploaded;
}

static CVPixelBufferRef getCVPixelBufferRef(SDL_VoutOverlay *overlay)
{
    switch (overlay->format) {
        case SDL_FCC__VTB:
            return SDL_VoutOverlayVideoToolBox_GetCVPixelBufferRef(overlay);
#if USE_FF_VTB
        case SDL_FCC__FFVTB:
            return SDL_VoutFFmpeg_GetCVPixelBufferRef(overlay);
#endif
        default:
            return NULL;
    }
}

static void * getVideoImage(IJK_GLES2_Renderer *renderer, SDL_VoutOverlay *overlay)
{
    if (!renderer || !renderer->opaque || !overlay)
        return NULL;
    return getCVPixelBufferRef(overlay);
}

static GLvoid destroy(IJK_GLES2_Renderer *renderer)
{
    if (!renderer || !renderer->opaque)
        return;
    
    IJK_GLES2_Renderer_Opaque *opaque = renderer->opaque;

    if (opaque->color_attachments != nil) {
        CFRelease(opaque->color_attachments);
        opaque->color_attachments = nil;
    }
    free(renderer->opaque);
    renderer->opaque = nil;
}

static GLvoid useSubtitle(IJK_GLES2_Renderer *renderer,GLboolean subtitle)
{
    glUniform1i(renderer->opaque->isSubtitle, (GLint)subtitle);
}

static GLboolean uploadSubtitle(IJK_GLES2_Renderer *renderer,void *subtitle)
{
    if (!subtitle) {
        return GL_FALSE;
    }
        
    IJK_GLES2_Renderer_Opaque *opaque = renderer->opaque;
    if (!opaque) {
        return GL_FALSE;
    }
    
    CVPixelBufferRef cvPixelRef = (CVPixelBufferRef)subtitle;
    CVPixelBufferRetain(cvPixelRef);
    GLboolean uploaded = upload_texture_use_IOSurface(cvPixelRef, renderer);
    CVPixelBufferRelease(cvPixelRef);
    return uploaded;
}

IJK_GLES2_Renderer *IJK_GL_Renderer_create_common_vtb(SDL_VoutOverlay *overlay,IJK_SHADER_TYPE type,int openglVer)
{
    assert(overlay->format == SDL_FCC__VTB || overlay->format == SDL_FCC__FFVTB);
    char shader_buffer[2048] = { '\0' };
    
    IJK_GL_getAppleCommonFragmentShader(type,shader_buffer,openglVer);
    IJK_GLES2_Renderer *renderer = IJK_GLES2_Renderer_create_base(shader_buffer,openglVer);

    if (!renderer)
        goto fail;
    
    const int samples = IJK_Sample_Count_For_Shader(type);
    assert(samples);
    
    for (int i = 0; i < samples; i++) {
        char name[20] = "us2_Sampler";
        name[strlen(name)] = (char)i + '0';
        renderer->us2_sampler[i] = glGetUniformLocation(renderer->program, name); IJK_GLES2_checkError_TRACE("glGetUniformLocation(us2_Sampler)");
    }
    
    //yuv to rgb
    if (samples > 1) {
        renderer->um3_color_conversion = glGetUniformLocation(renderer->program, "um3_ColorConversion"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(um3_ColorConversionMatrix)");
    }
    
    renderer->um3_rgb_adjustment = glGetUniformLocation(renderer->program, "um3_rgbAdjustment"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(um3_rgb_adjustmentVector)");
    
    renderer->func_use            = use;
    renderer->func_getBufferWidth = getBufferWidth;
    renderer->func_uploadTexture  = upload_Texture;
    renderer->func_getVideoImage  = getVideoImage;
    renderer->func_destroy        = destroy;
    renderer->func_useSubtitle    = useSubtitle;
    renderer->func_uploadSubtitle = uploadSubtitle;
    renderer->opaque = calloc(1, sizeof(IJK_GLES2_Renderer_Opaque));
    if (!renderer->opaque)
        goto fail;
    bzero(renderer->opaque, sizeof(IJK_GLES2_Renderer_Opaque));
    renderer->opaque->samples = samples;
    renderer->opaque->isSubtitle  = -1;
    renderer->opaque->isFullRange = -1;
    
    if (samples > 1) {
        GLint isFullRange = glGetUniformLocation(renderer->program, "isFullRange");
        assert(isFullRange >= 0);
        renderer->opaque->isFullRange = isFullRange;
    }
    
    GLint isSubtitle = glGetUniformLocation(renderer->program, "isSubtitle");
    assert(isSubtitle >= 0);
    renderer->opaque->isSubtitle = isSubtitle;
    
#if TARGET_OS_OSX
    if (overlay->format == SDL_FCC__VTB || overlay->format == SDL_FCC__FFVTB) {
        
        for (int i = 0; i < samples; i++) {
            char name[20] = "textureDimension";
            name[strlen(name)] = (char)i + '0';
            GLint textureDimension = glGetUniformLocation(renderer->program, name);
            assert(textureDimension >= 0);
            renderer->opaque->textureDimension[i] = textureDimension;
        }
    }
#endif
    
    return renderer;
fail:
    IJK_GLES2_Renderer_free(renderer);
    return NULL;
}
