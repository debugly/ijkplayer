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

#ifndef IJKSDL__IJKSDL_GLES2_H
#define IJKSDL__IJKSDL_GLES2_H

#ifdef __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_OS_OSX
        #include <OpenGL/OpenGL.h>
        #include <OpenGL/gl3.h>
        #include <OpenGL/gl3ext.h>
    #else
        #include <OpenGLES/ES2/gl.h>
        #include <OpenGLES/ES2/glext.h>
    #endif /* TARGET_OS_OSX */
#else
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
    #include <GLES2/gl2platform.h>
#endif /* __APPLE__ */

typedef struct SDL_VoutOverlay SDL_VoutOverlay;

/*
 * Common
 */

//#ifdef DEBUG
//#define IJK_GLES2_checkError_TRACE(op)
//#define IJK_GLES2_checkError_DEBUG(op)
//#else
#define IJK_GLES2_checkError_TRACE(op) IJK_GLES2_checkError(op) 
#define IJK_GLES2_checkError_DEBUG(op) IJK_GLES2_checkError(op)
//#endif

void IJK_GLES2_printString(const char *name, GLenum s);
void IJK_GLES2_checkError(const char *op);

GLuint IJK_GLES2_loadShader(GLenum shader_type, const char *shader_source);


/*
 * Renderer
 */
#define IJK_GLES2_MAX_PLANE 3
typedef struct IJK_GLES2_Renderer IJK_GLES2_Renderer;
//openglVer greater than 330 use morden opengl, otherwise use legacy opengl
IJK_GLES2_Renderer *IJK_GLES2_Renderer_create(SDL_VoutOverlay *overlay,int openglVer);
void      IJK_GLES2_Renderer_reset(IJK_GLES2_Renderer *renderer);
void      IJK_GLES2_Renderer_free(IJK_GLES2_Renderer *renderer);
void      IJK_GLES2_Renderer_freeP(IJK_GLES2_Renderer **renderer);

GLboolean IJK_GLES2_Renderer_setupGLES(void);
GLboolean IJK_GLES2_Renderer_isValid(IJK_GLES2_Renderer *renderer);
GLboolean IJK_GLES2_Renderer_isFormat(IJK_GLES2_Renderer *renderer, int format);
GLboolean IJK_GLES2_Renderer_use(IJK_GLES2_Renderer *renderer);
void IJK_GLES2_Renderer_updateColorConversion(IJK_GLES2_Renderer *renderer,float brightness,float satutaion,float contrast);

GLboolean IJK_GLES2_Renderer_updateVetex(IJK_GLES2_Renderer *renderer, SDL_VoutOverlay *overlay);
GLboolean IJK_GLES2_Renderer_uploadTexture(IJK_GLES2_Renderer *renderer, void *texture);
GLboolean IJK_GLES2_Renderer_resetVao(IJK_GLES2_Renderer *renderer);
void IJK_GLES2_Renderer_drawArrays(void);

void* IJK_GLES2_Renderer_getVideoImage(IJK_GLES2_Renderer *renderer, SDL_VoutOverlay *overlay);
void IJK_GLES2_Renderer_beginDrawSubtitle(IJK_GLES2_Renderer *renderer);
void IJK_GLES2_Renderer_updateSubtitleVetex(IJK_GLES2_Renderer *renderer, float width, float height);
GLboolean IJK_GLES2_Renderer_uploadSubtitleTexture(IJK_GLES2_Renderer *renderer, void *texture);
void IJK_GLES2_Renderer_endDrawSubtitle(IJK_GLES2_Renderer *renderer);

#define IJK_GLES2_GRAVITY_MIN                   (0)
#define IJK_GLES2_GRAVITY_RESIZE                (0) // Stretch to fill layer bounds.
#define IJK_GLES2_GRAVITY_RESIZE_ASPECT         (1) // Preserve aspect ratio; fit within layer bounds.
#define IJK_GLES2_GRAVITY_RESIZE_ASPECT_FILL    (2) // Preserve aspect ratio; fill layer bounds.
#define IJK_GLES2_GRAVITY_MAX                   (2)

GLboolean IJK_GLES2_Renderer_setGravity(IJK_GLES2_Renderer *renderer, int gravity, GLsizei view_width, GLsizei view_height);

void      IJK_GLES2_Renderer_updateRotate(IJK_GLES2_Renderer *renderer,int type,int degrees);
void      IJK_GLES2_Renderer_updateSubtitleBottomMargin(IJK_GLES2_Renderer *renderer,float value);
void      IJK_GLES2_Renderer_updateAutoZRotate(IJK_GLES2_Renderer *renderer,int degrees);
void      IJK_GLES2_Renderer_updateUserDefinedDAR(IJK_GLES2_Renderer *renderer,float ratio);
int       IJK_GLES2_Renderer_isZRotate90oddMultiple(IJK_GLES2_Renderer *renderer);

#endif
