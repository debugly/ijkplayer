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

//https://github.com/mpv-player/mpv

//vda: add support for nv12 image formats
//
//The hardware always decodes to nv12 so using this image format causes less cpu
//usage than uyvy (which we are currently using, since Apple examples and other
//free software use that). The reduction in cpu usage can add up to quite a bit,
//especially for 4k or high fps video.
//
//This needs an accompaning commit in libavcodec.
//提交：
//5258c012febdfba0ef56ad8ce6f7cb003611c47b

#ifndef IJKSDL__renderer_pixfmt__INTERNAL__H
#define IJKSDL__renderer_pixfmt__INTERNAL__H

#import <CoreVideo/CoreVideo.h>

#define USE_LEGACY_OPENGL 0

#if TARGET_OS_OSX
    #define OpenGLTextureCacheRef   CVOpenGLTextureCacheRef
    #define OpenGLTextureRef        CVOpenGLTextureRef
    #define OpenGLTextureCacheFlush CVOpenGLTextureCacheFlush
    #define OpenGLTextureGetTarget  CVOpenGLTextureGetTarget
    #define OpenGLTextureGetName    CVOpenGLTextureGetName
    #define OpenGL_RED_EXT          GL_RED
    #define OpenGL_RG_EXT           GL_RG
#else
    #define OpenGLTextureCacheRef   CVOpenGLESTextureCacheRef
    #define OpenGLTextureRef        CVOpenGLESTextureRef
    #define OpenGLTextureCacheFlush CVOpenGLESTextureCacheFlush
    #define OpenGLTextureGetTarget  CVOpenGLESTextureGetTarget
    #define OpenGLTextureGetName    CVOpenGLESTextureGetName
    #define OpenGL_RED_EXT          GL_RED_EXT
    #define OpenGL_RG_EXT           GL_RG_EXT
#endif

enum mp_imgfmt {
    IMGFMT_NONE = 0,

    // Offset to make confusing with ffmpeg formats harder
    IMGFMT_START = 1000,

    // Planar YUV formats
    IMGFMT_444P,                // 1x1
    IMGFMT_420P,                // 2x2

    // Gray
    IMGFMT_Y8,
    IMGFMT_Y16,

    // Packed YUV formats (components are byte-accessed)
    IMGFMT_UYVY,                // U  Y0 V  Y1

    // Y plane + packed plane for chroma
    IMGFMT_NV12,

    // Like IMGFMT_NV12, but with 10 bits per component (and 6 bits of padding)
    IMGFMT_P010,

    // Like IMGFMT_NV12, but for 4:4:4
    IMGFMT_NV24,

    // RGB/BGR Formats

    // Byte accessed (low address to high address)
    IMGFMT_ARGB,
    IMGFMT_BGRA,
    IMGFMT_ABGR,
    IMGFMT_RGBA,
    IMGFMT_BGR24,               // 3 bytes per pixel
    IMGFMT_RGB24,

    // Like e.g. IMGFMT_ARGB, but has a padding byte instead of alpha
    IMGFMT_0RGB,
    IMGFMT_BGR0,
    IMGFMT_0BGR,
    IMGFMT_RGB0,

    IMGFMT_RGB0_START = IMGFMT_0RGB,
    IMGFMT_RGB0_END = IMGFMT_RGB0,

    // Like IMGFMT_RGBA, but 2 bytes per component.
    IMGFMT_RGBA64,

    // Accessed with bit-shifts after endian-swapping the uint16_t pixel
    IMGFMT_RGB565,              // 5r 6g 5b (MSB to LSB)

    // Hardware accelerated formats. Plane data points to special data
    // structures, instead of pixel data.
    IMGFMT_VDPAU,           // VdpVideoSurface
    IMGFMT_VDPAU_OUTPUT,    // VdpOutputSurface
    IMGFMT_VAAPI,
    // plane 0: ID3D11Texture2D
    // plane 1: slice index casted to pointer
    IMGFMT_D3D11,
    IMGFMT_DXVA2,           // IDirect3DSurface9 (NV12/P010/P016)
    IMGFMT_MMAL,            // MMAL_BUFFER_HEADER_T
    IMGFMT_VIDEOTOOLBOX,    // CVPixelBufferRef
    IMGFMT_MEDIACODEC,      // AVMediaCodecBuffer
    IMGFMT_DRMPRIME,        // AVDRMFrameDescriptor
    IMGFMT_CUDA,            // CUDA Buffer

    // Generic pass-through of AV_PIX_FMT_*. Used for formats which don't have
    // a corresponding IMGFMT_ value.
    IMGFMT_AVPIXFMT_START,
    IMGFMT_AVPIXFMT_END = IMGFMT_AVPIXFMT_START + 500,

    IMGFMT_END
};

#define MP_MAX_PLANES 4
#define MP_ARRAY_SIZE(s) (sizeof(s) / sizeof((s)[0]))

struct vt_gl_plane_format {
    GLenum gl_format;
    GLenum gl_type;
    GLenum gl_internal_format;
};

struct vt_format {
    uint32_t cvpixfmt;
    int imgfmt;
    int planes;
    struct vt_gl_plane_format gl[MP_MAX_PLANES];
};

static struct vt_format vt_formats[] = {
    {
        .cvpixfmt = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
        .imgfmt = IMGFMT_NV12,
        .planes = 2,
        .gl = {
//           when use RED/RG,the fsh must use r and rg!
            { GL_RED, GL_UNSIGNED_BYTE, GL_RED },
            { GL_RG,  GL_UNSIGNED_BYTE, GL_RG } ,
//           330 后使用这个绿屏，when use LUMINANCE/LUMINANCE_ALPHA,the fsh must use r and ra!
//            { GL_LUMINANCE, GL_UNSIGNED_BYTE, GL_LUMINANCE },
//            { GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, GL_LUMINANCE_ALPHA }
        }
    },
    {
        .cvpixfmt = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
        .imgfmt = IMGFMT_NV12,
        .planes = 2,
        .gl = {
            { GL_RED, GL_UNSIGNED_BYTE, GL_RED },
            { GL_RG,  GL_UNSIGNED_BYTE, GL_RG }
        }
    },
#if TARGET_OS_OSX
    {
        .cvpixfmt = kCVPixelFormatType_422YpCbCr8,
        .imgfmt = IMGFMT_UYVY,
        .planes = 1,
        .gl = {
            //330
#if USE_LEGACY_OPENGL
            { GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, GL_RGB }
#else
            { GL_RGB_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, GL_RGB }
#endif
        }
    },
#endif
    {
        .cvpixfmt = kCVPixelFormatType_420YpCbCr8Planar,
        .imgfmt = IMGFMT_420P,
        .planes = 3,
        .gl = {
#if TARGET_OS_OSX
            { GL_RED, GL_UNSIGNED_BYTE, GL_RED },
            { GL_RED, GL_UNSIGNED_BYTE, GL_RED },
            { GL_RED, GL_UNSIGNED_BYTE, GL_RED }
#else
            { GL_RED_EXT, GL_UNSIGNED_BYTE, GL_RED_EXT },
            { GL_RED_EXT, GL_UNSIGNED_BYTE, GL_RED_EXT },
            { GL_RED_EXT, GL_UNSIGNED_BYTE, GL_RED_EXT }
#endif
        }
    },
    {
        .cvpixfmt = kCVPixelFormatType_420YpCbCr8PlanarFullRange,
        .imgfmt = IMGFMT_420P,
        .planes = 3,
        .gl = {
#if TARGET_OS_OSX
            { GL_RED, GL_UNSIGNED_BYTE, GL_RED },
            { GL_RED, GL_UNSIGNED_BYTE, GL_RED },
            { GL_RED, GL_UNSIGNED_BYTE, GL_RED }
#else
            { GL_RED_EXT, GL_UNSIGNED_BYTE, GL_RED_EXT },
            { GL_RED_EXT, GL_UNSIGNED_BYTE, GL_RED_EXT },
            { GL_RED_EXT, GL_UNSIGNED_BYTE, GL_RED_EXT }
#endif
        }
    },
    {
        .cvpixfmt = kCVPixelFormatType_32BGRA,
        .imgfmt = IMGFMT_BGR0,
        .planes = 1,
        .gl = {
        #if TARGET_OS_OSX
            { GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, GL_RGBA }
        #else
            //cvpixelbuffer not support kCVPixelFormatType_32RGBA return -6680 errer code.
            { GL_BGRA, GL_UNSIGNED_BYTE, GL_RGBA }
        #endif
        }
    },
    {
        .cvpixfmt = kCVPixelFormatType_32ARGB,
        .imgfmt = IMGFMT_ARGB,
        .planes = 1,
        .gl = {
#if TARGET_OS_OSX
            { GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, GL_RGBA }
#else
            { GL_BGRA, GL_UNSIGNED_INT, GL_RGBA }
#endif
        }
    },
#if 0
    {
//        creating IOSurface texture invalid numerical value: kCVPixelFormatType_24RGB
        .cvpixfmt = kCVPixelFormatType_24RGB,
        .imgfmt = IMGFMT_RGB24,
        .planes = 1,
        .gl = {
#if TARGET_OS_OSX
            { GL_BGR, GL_UNSIGNED_INT_8_8_8_8_REV, GL_RGBA }
#else
            { GL_BGR, GL_UNSIGNED_INT, GL_RGBA }
#endif
        }
    }
#endif
};

static inline struct vt_format *vt_get_gl_format(uint32_t cvpixfmt)
{
    for (int i = 0; i < MP_ARRAY_SIZE(vt_formats); i++) {
        if (vt_formats[i].cvpixfmt == cvpixfmt)
            return &vt_formats[i];
    }
    return NULL;
}

#endif
