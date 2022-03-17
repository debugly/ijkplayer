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

#ifndef IJKSDL__IJKSDL_GLES2__INTERNAL__H
#define IJKSDL__IJKSDL_GLES2__INTERNAL__H

#include <assert.h>
#include <stdlib.h>
#include "ijksdl/ijksdl_fourcc.h"
#include "ijksdl/ijksdl_log.h"
#include "ijksdl/ijksdl_gles2.h"
#include "ijksdl/ijksdl_vout.h"
#include "math_util.h"

#define IJK_GLES_STRINGIZE(x)   #x
#define IJK_GLES_STRINGIZE2(x)  IJK_GLES_STRINGIZE(x)
#define IJK_GLES_STRING(x)      IJK_GLES_STRINGIZE2(x)

typedef struct IJK_GLES2_Renderer_Opaque
{
	GLint                 isSubtitle;
	GLint                 isFullRange;
	int samples;
} IJK_GLES2_Renderer_Opaque;

typedef struct IJK_GLES2_Renderer_Opaque IJK_GLES2_Renderer_Opaque;

#ifdef __APPLE__
typedef enum : int {
    NONE_SHADER,
    BGRX_SHADER,
    XRGB_SHADER,
    YUV_2P_SHADER,//for 420sp
    YUV_3P_SHADER,//for 420p
    UYVY_SHADER   //for uyvy
} IJK_SHADER_TYPE;

static inline const int IJK_Sample_Count_For_Shader(IJK_SHADER_TYPE type)
{
    switch (type) {
        case BGRX_SHADER:
        case XRGB_SHADER:
        case UYVY_SHADER:
        {
            return 1;
        }
        case YUV_2P_SHADER:
        {
            return 2;
        }
        case YUV_3P_SHADER:
        {
            return 3;
        }
        case NONE_SHADER:
        {
            assert(0);
            return 0;
        }
    }
}
#endif

typedef struct IJK_GLES2_Renderer
{
    IJK_GLES2_Renderer_Opaque *opaque;

    GLuint program;

    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint plane_textures[IJK_GLES2_MAX_PLANE];

    GLint av4_position;
    GLint av2_texcoord;
    GLint um4_mvp;

    GLint us2_sampler[IJK_GLES2_MAX_PLANE];
    GLint um3_color_conversion;
    GLint um3_rgb_adjustment;
    
    GLboolean (*func_use)(IJK_GLES2_Renderer *renderer);
    GLsizei   (*func_getBufferWidth)(IJK_GLES2_Renderer *renderer, SDL_VoutOverlay *overlay);
	GLboolean(*func_uploadTexture)(IJK_GLES2_Renderer *renderer, void *texture);
    GLvoid    (*func_useSubtitle)(IJK_GLES2_Renderer *renderer,GLboolean subtitle);
    GLboolean (*func_uploadSubtitle)(IJK_GLES2_Renderer *renderer,void* subtitle);
    void*     (*func_getVideoImage)(IJK_GLES2_Renderer *renderer, SDL_VoutOverlay *overlay);
    GLvoid    (*func_destroy)(IJK_GLES2_Renderer *renderer);

    GLsizei buffer_width;
    GLsizei visible_width;

    GLfloat texcoords[8];

    GLfloat vertices[8];
    int     vertices_changed;
    int     mvp_changed;
    int     drawingSubtitle;
    /// 顶点对象
    GLuint vbo;
    GLuint vao;
    
    int     format;
    int     gravity;
    GLsizei layer_width;
    GLsizei layer_height;
    
    //record last overly info
    int     frame_width;
    int     frame_height;
    int     frame_sar_num;
    int     frame_sar_den;
    
    //user defined video ratio
    float   user_dar_ratio;

    GLsizei last_buffer_width;
    
    //for auto rotate video
    int auto_z_rotate_degrees;
    //for rotate
    int rotate_type;//x=1;y=2;z=3
    int rotate_degrees;
    float subtitle_bottom_margin;
    GLfloat rgb_adjustment[3];
} IJK_GLES2_Renderer;

ijk_matrix IJK_GLES2_makeOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near, GLfloat far);

ijk_matrix IJK_GLES2_defaultOrtho(void);

#ifdef __APPLE__
void IJK_GLES2_getVertexShader_default(char *out,int ver);
#else
const char *IJK_GLES2_getVertexShader_default();
#endif

#ifdef __APPLE__
IJK_GLES2_Renderer *IJK_GL_Renderer_create_common_vtb(SDL_VoutOverlay *overlay,IJK_SHADER_TYPE type,int openglVer);
void IJK_GL_getAppleCommonFragmentShader(IJK_SHADER_TYPE type,char *out,int ver);
#endif

#ifndef __APPLE__
const char *IJK_GLES2_getFragmentShader_yuv420p();
const char *IJK_GLES2_getFragmentShader_yuv444p10le();
const char *IJK_GLES2_getFragmentShader_yuv420sp();
const char *IJK_GLES2_getFragmentShader_rgb();

const GLfloat *IJK_GLES2_getColorMatrix_bt709();
const GLfloat *IJK_GLES2_getColorMatrix_bt601();

IJK_GLES2_Renderer *IJK_GLES2_Renderer_create_yuv420p();
IJK_GLES2_Renderer *IJK_GLES2_Renderer_create_yuv444p10le();
IJK_GLES2_Renderer *IJK_GLES2_Renderer_create_yuv420sp();
IJK_GLES2_Renderer *IJK_GLES2_Renderer_create_rgb565();
IJK_GLES2_Renderer *IJK_GLES2_Renderer_create_rgb888();
IJK_GLES2_Renderer *IJK_GLES2_Renderer_create_rgbx8888();
#endif

IJK_GLES2_Renderer *IJK_GLES2_Renderer_create_base(const char *fragment_shader_source, int openglVer);

#endif
