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

#ifndef __APPLE__

#include "ijksdl_egl.h"

#include <stdlib.h>
#include <stdbool.h>
#include "ijksdl/ijksdl_gles2.h"
#include "ijksdl/ijksdl_log.h"
#include "ijksdl/ijksdl_vout.h"
#include "ijksdl/gles2/internal.h"
#include "ijksdl/ffmpeg/ijksdl_vout_overlay_ffmpeg.h"
#include "ijksdl/windows/win_text_to_bitmap.h"

#define IJK_EGL_RENDER_BUFFER 0

typedef struct IJK_EGL_Opaque {
    IJK_GLES2_Renderer *renderer;
	Subtitle_Overlay*	sub_overlay;
} IJK_EGL_Opaque;

static EGLBoolean IJK_EGL_isValid(IJK_EGL* egl)
{
    if (egl &&
        egl->window &&
        egl->display &&
        egl->surface &&
        egl->context) {
        return EGL_TRUE;
    }

    return EGL_FALSE;
}

void IJK_EGL_terminate(IJK_EGL* egl)
{
    if (!IJK_EGL_isValid(egl))
        return;

	if (egl->opaque) {
		IJK_GLES2_Renderer_freeP(&egl->opaque->renderer);

		if (egl->opaque->sub_overlay) {
			free(egl->opaque->sub_overlay->font_name);
			free(egl->opaque->sub_overlay);
		}
	}

    if (egl->display) {
        eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl->context)
            eglDestroyContext(egl->display, egl->context);
        if (egl->surface)
            eglDestroySurface(egl->display, egl->surface);
        eglTerminate(egl->display);
        eglReleaseThread(); // FIXME: call at thread exit
    }

    egl->context = EGL_NO_CONTEXT;
    egl->surface = EGL_NO_SURFACE;
    egl->display = EGL_NO_DISPLAY;
}

static int IJK_EGL_getSurfaceWidth(IJK_EGL* egl)
{
    EGLint width = 0;
    if (!eglQuerySurface(egl->display, egl->surface, EGL_WIDTH, &width)) {
        ALOGE("[EGL] eglQuerySurface(EGL_WIDTH) returned error %d", eglGetError());
        return 0;
    }

    return width;
}

static int IJK_EGL_getSurfaceHeight(IJK_EGL* egl)
{
    EGLint height = 0;
    if (!eglQuerySurface(egl->display, egl->surface, EGL_HEIGHT, &height)) {
        ALOGE("[EGL] eglQuerySurface(EGL_HEIGHT) returned error %d", eglGetError());
        return 0;
    }

    return height;
}

static EGLBoolean IJK_EGL_setSurfaceSize(IJK_EGL* egl, int width, int height)
{
    if (!IJK_EGL_isValid(egl))
        return EGL_FALSE;

//#ifdef __ANDROID__
    egl->width  = IJK_EGL_getSurfaceWidth(egl);
    egl->height = IJK_EGL_getSurfaceHeight(egl);

    if (width != egl->width || height != egl->height) {
        //int format = ANativeWindow_getFormat(egl->window);
        //ALOGI("ANativeWindow_setBuffersGeometry(w=%d,h=%d) -> (w=%d,h=%d);",
        //    egl->width, egl->height,
        //    width, height);
        //int ret = ANativeWindow_setBuffersGeometry(egl->window, width, height, format);
        //if (ret) {
        //    ALOGE("[EGL] ANativeWindow_setBuffersGeometry() returned error %d", ret);
        //    return EGL_FALSE;
        //}

        egl->width  = IJK_EGL_getSurfaceWidth(egl);
        egl->height = IJK_EGL_getSurfaceHeight(egl);
        return (egl->width && egl->height) ? EGL_TRUE : EGL_FALSE;
    }

    return EGL_TRUE;
//#else
    // FIXME: other platform?
//#endif
    return EGL_FALSE;
}

static EGLBoolean IJK_EGL_makeCurrent(IJK_EGL* egl, EGLNativeWindowType window)
{
    if (window && window == egl->window &&
        egl->display &&
        egl->surface &&
        egl->context) {

        if (!eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context)) {
            ALOGE("[EGL] elgMakeCurrent() failed (cached)\n");
            return EGL_FALSE;
        }

        return EGL_TRUE;
    }

    IJK_EGL_terminate(egl);
    egl->window = window;

    if (!window)
        return EGL_FALSE;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        ALOGE("[EGL] eglGetDisplay failed\n");
        return EGL_FALSE;
    }


    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) {
        ALOGE("[EGL] eglInitialize failed\n");
        return EGL_FALSE;   
    }
    ALOGI("[EGL] eglInitialize %d.%d\n", (int)major, (int)minor);


    static const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,       EGL_WINDOW_BIT,
        EGL_BLUE_SIZE,          8,
        EGL_GREEN_SIZE,         8,
        EGL_RED_SIZE,           8,
        EGL_NONE
    };

    static const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfig;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfig)) {
        ALOGE("[EGL] eglChooseConfig failed\n");
        eglTerminate(display);
        return EGL_FALSE;
    }

#ifdef __ANDROID__
    {
        EGLint native_visual_id = 0;
        if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &native_visual_id)) {
            ALOGE("[EGL] eglGetConfigAttrib() returned error %d", eglGetError());
            eglTerminate(display);
            return EGL_FALSE;
        }

        int32_t width  = ANativeWindow_getWidth(window);
        int32_t height = ANativeWindow_getWidth(window);
        ALOGI("[EGL] ANativeWindow_setBuffersGeometry(f=%d);", native_visual_id);
        int ret = ANativeWindow_setBuffersGeometry(window, width, height, native_visual_id);
        if (ret) {
            ALOGE("[EGL] ANativeWindow_setBuffersGeometry(format) returned error %d", ret);
            eglTerminate(display);
            return EGL_FALSE;
        }
    }
#endif

    EGLSurface surface = eglCreateWindowSurface(display, config, window, NULL);
    if (surface == EGL_NO_SURFACE) {
        ALOGE("[EGL] eglCreateWindowSurface failed\n");
        eglTerminate(display);
        return EGL_FALSE;
    }

    EGLSurface context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        ALOGE("[EGL] eglCreateContext failed\n");
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return EGL_FALSE;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        ALOGE("[EGL] elgMakeCurrent() failed (new)\n");
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return EGL_FALSE;
    }

#if 0
#if defined(__ANDROID__)
    {
        const char *extensions = (const char *) glGetString(GL_EXTENSIONS);
        if (extensions) {
            char *dup_extensions = strdup(extensions);
            if (dup_extensions) {
                char *brk = NULL;
                char *ext = strtok_r(dup_extensions, " ", &brk);
                while (ext) {
                    if (0 == strcmp(ext, "GL_EXT_texture_rg"))
                        egl->gles2_extensions[IJK_GLES2__GL_EXT_texture_rg] = 1;

                    ext = strtok_r(NULL, " ", &brk);
                }

                free(dup_extensions);
            }
        }
    }
#elif defined(__APPLE__)
    egl->gles2_extensions[IJK_GLES2__GL_EXT_texture_rg] = 1;
#endif

    ALOGI("[EGL] GLES2 extensions begin:\n");
    ALOGI("[EGL]     GL_EXT_texture_rg: %d\n", egl->gles2_extensions[IJK_GLES2__GL_EXT_texture_rg]);
    ALOGI("[EGL] GLES2 extensions end.\n");
#endif

    IJK_GLES2_Renderer_setupGLES();

    egl->context = context;
    egl->surface = surface;
    egl->display = display;
    return EGL_TRUE;
}

static EGLBoolean IJK_EGL_prepareRenderer(IJK_EGL* egl, SDL_VoutOverlay *overlay)
{
    assert(egl);
    assert(egl->opaque);

    IJK_EGL_Opaque *opaque = egl->opaque;

    if (!IJK_GLES2_Renderer_isValid(opaque->renderer) ||
        !IJK_GLES2_Renderer_isFormat(opaque->renderer, overlay->format)) {

        IJK_GLES2_Renderer_reset(opaque->renderer);
        IJK_GLES2_Renderer_freeP(&opaque->renderer);

		int openglVer = 330;
#if USE_LEGACY_OPENGL
		openglVer = 120;
#endif
        opaque->renderer = IJK_GLES2_Renderer_create(overlay, openglVer);
        if (!opaque->renderer) {
            ALOGE("[EGL] Could not create render.");
            return EGL_FALSE;
        }

        if (!IJK_GLES2_Renderer_use(opaque->renderer)) {
            ALOGE("[EGL] Could not use render.");
            IJK_GLES2_Renderer_freeP(&opaque->renderer);
            return EGL_FALSE;
        }

		opaque->sub_overlay = (Subtitle_Overlay*)calloc(1, sizeof(Subtitle_Overlay));
		IJK_GLES2_Renderer_setGravity(opaque->renderer, IJK_GLES2_GRAVITY_RESIZE_ASPECT, overlay->w, overlay->h);

		IJK_GLES2_Renderer_updateColorConversion(opaque->renderer, 1, 1, 1);
    }

    if (!IJK_EGL_setSurfaceSize(egl, overlay->w, overlay->h)) {
        ALOGE("[EGL] IJK_EGL_setSurfaceSize(%d, %d) failed\n", overlay->w, overlay->h);
        return EGL_FALSE;
    }

    glViewport(0, 0, egl->width, egl->height);  IJK_GLES2_checkError_TRACE("glViewport");
    return EGL_TRUE;
}

static EGLBoolean IJK_EGL_display_internal(IJK_EGL* egl, SDL_VoutOverlay *overlay)
{
    IJK_EGL_Opaque *opaque = egl->opaque;
	
    if (!IJK_GLES2_Renderer_updateVetex(opaque->renderer, overlay)) {
        ALOGE("[EGL] IJK_GLES2_Renderer_updateVetex failed\n");
        return EGL_FALSE; 
    }

	if (!IJK_GLES2_Renderer_uploadTexture(opaque->renderer, overlay)) {
		ALOGE("[EGL] IJK_GLES2_Renderer_updateVetex failed\n");
		return EGL_FALSE;
	}

	IJK_GLES2_Renderer_drawArrays();

    return EGL_TRUE;
}

static EGLBoolean IJK_EGL_display_subtitle_internal(IJK_EGL* egl,  const char *text)
{
	IJK_EGL_Opaque *opaque = egl->opaque;
	Subtitle_Overlay* overlay = opaque->sub_overlay;
	Create_Bitmap(text, &overlay);

	IJK_GLES2_Renderer_beginDrawSubtitle(opaque->renderer);

	IJK_GLES2_Renderer_updateSubtitleVetex(opaque->renderer, overlay->w, overlay->h);

	if (!IJK_GLES2_Renderer_uploadSubtitleTexture(opaque->renderer, overlay)) {
		ALOGE("[EGL] IJK_GLES2_Renderer_updateVetex failed\n");
		return EGL_FALSE;
	}

	IJK_GLES2_Renderer_drawArrays();
	IJK_GLES2_Renderer_endDrawSubtitle(opaque->renderer);

	Release_Bitmap(overlay);
	return EGL_TRUE;
}

EGLBoolean IJK_EGL_display(IJK_EGL* egl, EGLNativeWindowType window, SDL_VoutOverlay *overlay, const char* text)
{
    EGLBoolean ret = EGL_FALSE;
    if (!egl)
        return EGL_FALSE;

    IJK_EGL_Opaque *opaque = egl->opaque;
    if (!opaque)
        return EGL_FALSE;

    if (!IJK_EGL_makeCurrent(egl, window))
        return EGL_FALSE;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, egl->width, egl->height);
	glClear(GL_COLOR_BUFFER_BIT);

	if (!IJK_EGL_prepareRenderer(egl, overlay)) {
		ALOGE("[EGL] IJK_EGL_prepareRenderer failed\n");
		return EGL_FALSE;
	}
    ret = IJK_EGL_display_internal(egl, overlay);

	if (text && strlen(text) > 0){
		ret = IJK_EGL_display_subtitle_internal(egl, text);
	}

	eglSwapBuffers(egl->display, egl->surface);
    eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglReleaseThread(); // FIXME: call at thread exit
    return ret;
}

void IJK_EGL_releaseWindow(IJK_EGL* egl)
{
    if (!egl || !egl->opaque || !egl->opaque->renderer)
        return;

    IJK_EGL_terminate(egl);
}

void IJK_EGL_free(IJK_EGL *egl)
{
    if (!egl)
        return;

    IJK_EGL_terminate(egl);

    memset(egl, 0, sizeof(IJK_EGL));
    free(egl);
}

void IJK_EGL_freep(IJK_EGL **egl)
{
    if (!egl || !*egl)
        return;

    IJK_EGL_free(*egl);
    *egl = NULL;
}

static SDL_Class g_class = {
    .name = "EGL",
};

IJK_EGL *IJK_EGL_create()
{
    IJK_EGL *egl = (IJK_EGL*) mallocz(sizeof(IJK_EGL));
    if (!egl)
        return NULL;

    egl->opaque_class = &g_class;
    egl->opaque = mallocz(sizeof(IJK_EGL_Opaque));
    if (!egl->opaque) {
        free(egl);
        return NULL;
    }

    return egl;
}

static void IJK_EGL_destroy_FBO(IJK_EGL *egl)
{
	glDeleteFramebuffers(1, &egl->FBO);
	glDeleteFramebuffers(1, &egl->colorTexture);
}

static bool IJK_EGL_prepare_FBO_if_need(IJK_EGL *egl)
{
	glGenTextures(1, &egl->colorTexture);        
	IJK_GLES2_checkError_TRACE("IJK_EGL_prepare_FBO_if_need:glGenTextures");

	glBindTexture(GL_TEXTURE_2D, egl->colorTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	IJK_GLES2_checkError_TRACE("IJK_EGL_prepare_FBO_if_need:glTexParameteri");

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, egl->width, egl->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	glGenFramebuffers(1, &egl->FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, egl->FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, egl->colorTexture, 0);

	IJK_GLES2_checkError_TRACE("IJK_EGL_prepare_FBO_if_need:glBindFramebuffer");

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
		return true;
	} else {
		IJK_GLES2_checkError_TRACE("IJK_EGL_prepare_FBO_if_need:glBindFramebuffer");

		glCheckFramebufferStatus(GL_FRAMEBUFFER);
		return false;
	}
}

void* IJK_EGL_snapshot_from_FBO(IJK_EGL* egl)
{
	GLint bytes_per_row = egl->width * 4;
	const GLint bits_per_pixel = 32;
	void* pixels_data = malloc(egl->width * egl->height * 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, bytes_per_row);
	IJK_GLES2_checkError_TRACE("glPixelStorei");

	glReadPixels(0, 0, egl->width, egl->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_data);
	IJK_GLES2_checkError_TRACE("glReadPixels");

	//FILE* rFp = fopen("D:\\test_r.y", "wb+");
	//FILE* gFp = fopen("D:\\test_g.y", "wb+");
	//FILE* bFp = fopen("D:\\test_b.y", "wb+");

	//int size = egl->width * egl->height;
	//unsigned char * rbuf = (unsigned char*)malloc(size);
	//unsigned char * gbuf = (unsigned char*)malloc(size);
	//unsigned char * bbuf = (unsigned char*)malloc(size);
	//int ridx, gidx, bidx;
	//ridx = gidx = bidx = 0;
	//unsigned char*  pixels = (unsigned char*)pixels_data;
	//for (int rgbidx = 0; rgbidx < size * 3; rgbidx = rgbidx + 4)
	//{
	//	rbuf[ridx++] = pixels[rgbidx];
	//	gbuf[gidx++] = pixels[rgbidx + 1];
	//	bbuf[bidx++] = pixels[rgbidx + 2];
	//}
	//fwrite(rbuf, 1, size, rFp);
	//fwrite(gbuf, 1, size, gFp);
	//fwrite(bbuf, 1, size, bFp);

	//free(rbuf);
	//free(gbuf);
	//free(bbuf);
	//fclose(rFp);
	//fclose(gFp);
	//fclose(bFp);

	free(pixels_data);
}

void* IJK_EGL_snapshot_effect_origin_with_subtitle(IJK_EGL *egl, SDL_VoutOverlay* overlay, EGLBoolean with_subtitle)
{
	if (egl->display && egl->surface && egl->context) {
		if (!eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context)) {
			ALOGE("[EGL] elgMakeCurrent() failed (cached)\n");
			return EGL_FALSE;
		}
	}

	EGLBoolean ret = false;
	if (IJK_EGL_prepare_FBO_if_need(egl)) {
		glBindFramebuffer(GL_FRAMEBUFFER, egl->FBO);
		glViewport(0, 0, egl->width, egl->height);
		glClear(GL_COLOR_BUFFER_BIT);

		glBindTexture(GL_TEXTURE_2D, egl->colorTexture);

		IJK_EGL_Opaque *opaque = egl->opaque;

		if (!IJK_GLES2_Renderer_resetVao(opaque->renderer)) {
			ALOGE("[EGL] snapshot IJK_GLES2_Renderer_resetVao failed\n");
		}

		if (!IJK_GLES2_Renderer_uploadTexture(opaque->renderer, overlay)) {
			ALOGE("[EGL] snapshot IJK_GLES2_Renderer_updateVetex failed\n");
			return EGL_FALSE;
		}

		IJK_GLES2_Renderer_drawArrays();

		IJK_EGL_snapshot_from_FBO(egl);
	}

	eglSwapBuffers(egl->display, egl->surface);
	eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglReleaseThread(); // FIXME: call at thread exit
}


float IJK_EGL_get_font_size(IJK_EGL* egl)
{
	return egl->opaque->sub_overlay->font_size;
}

void IJK_EGL_set_font_size(IJK_EGL* egl, float size)
{
	egl->opaque->sub_overlay->font_size = size;
}

char* IJK_EGL_get_font_name(IJK_EGL* egl)
{
	return egl->opaque->sub_overlay->font_name;
}

void IJK_EGL_set_font_name(IJK_EGL* egl, const char* font_name)
{
	assert(egl->opaque->sub_overlay);
	if (egl->opaque->sub_overlay->font_name) {
		free(egl->opaque->sub_overlay->font_name);
	}
	egl->opaque->sub_overlay->font_name = strdup(font_name);
}

#endif
