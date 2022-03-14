/*
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
#include "ijksdl/windows/win_text_to_bitmap.h"

static GLboolean yuv420p_use(IJK_GLES2_Renderer *renderer)
{
    ALOGI("use render yuv420p\n");
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glUseProgram(renderer->program);            IJK_GLES2_checkError_TRACE("glUseProgram");

    if (0 == renderer->plane_textures[0])
        glGenTextures(3, renderer->plane_textures);

    for (int i = 0; i < 3; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, renderer->plane_textures[i]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glUniform1i(renderer->us2_sampler[i], i);
    }

    glUniformMatrix3fv(renderer->um3_color_conversion, 1, GL_FALSE, IJK_GLES2_getColorMatrix_bt709());

    return GL_TRUE;
}

static GLsizei yuv420p_getBufferWidth(IJK_GLES2_Renderer *renderer, SDL_VoutOverlay *overlay)
{
    if (!overlay)
        return 0;

    return overlay->pitches[0] / 1;
}

static GLboolean yuv420p_uploadTexture(IJK_GLES2_Renderer *renderer, SDL_VoutOverlay *vout_overlay)
{
    if (!renderer || !vout_overlay)
        return GL_FALSE;

	SDL_VoutOverlay* overlay = (SDL_VoutOverlay*)vout_overlay;
          int     planes[3]    = { 0, 1, 2 };
    const GLsizei widths[3]    = { overlay->pitches[0], overlay->pitches[1], overlay->pitches[2] };
    const GLsizei heights[3]   = { overlay->h,          overlay->h / 2,      overlay->h / 2 };
    const GLubyte *pixels[3]   = { overlay->pixels[0],  overlay->pixels[1],  overlay->pixels[2] };

    switch (overlay->format) {
        case SDL_FCC_I420:
            break;
        case SDL_FCC_YV12:
            planes[1] = 2;
            planes[2] = 1;
            break;
        default:
            ALOGE("[yuv420p] unexpected format %x\n", overlay->format);
            return GL_FALSE;
    }

    for (int i = 0; i < 3; ++i) {
        int plane = planes[i];

		glUniform1i(renderer->us2_sampler[i], i);
		glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, renderer->plane_textures[i]);

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_LUMINANCE,
                     widths[plane],
                     heights[plane],
                     0,
                     GL_LUMINANCE,
                     GL_UNSIGNED_BYTE,
                     pixels[plane]);
    }

    return GL_TRUE;
}

static GLvoid yuv420p_useSubtile(IJK_GLES2_Renderer* renderer, GLboolean subtitle)
{
	glUniform1i(renderer->opaque->isSubtitle, (GLint)subtitle); IJK_GLES2_checkError_TRACE("glUniform1i(renderer->opaque->isSubtitle, (GLint)subtitle)");
}

static GLboolean yuv420p_uploadSubtitle(IJK_GLES2_Renderer* renderer, void* subtitle)
{
	if (!subtitle) {
		return GL_FALSE;
	}

	int     planes[1] = { 0 };
	Subtitle_Overlay*   overlay = (Subtitle_Overlay*)subtitle;
	const GLsizei widths[1] = { overlay->w };
	const GLsizei heights[3] = { overlay->h };
	const GLubyte *pixels[3] = { overlay->pixels };

	for (int i = 0; i < 1; ++i) {
		int plane = planes[i];

		glUniform1i(renderer->us2_sampler[i], i);
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, renderer->plane_textures[i]);
		IJK_GLES2_checkError_TRACE("glBindTexture(GL_TEXTURE_2D, renderer->plane_textures[i])");

		glTexImage2D(GL_TEXTURE_2D,
			0,
			GL_RGBA,
			widths[plane],
			heights[plane],
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			pixels[plane]);

		IJK_GLES2_checkError_TRACE("glTexImage2D");
	}

	return GL_TRUE;
}

IJK_GLES2_Renderer *IJK_GLES2_Renderer_create_yuv420p()
{
    ALOGI("create render yuv420p\n");
	int openglVer = 300;
    IJK_GLES2_Renderer *renderer = IJK_GLES2_Renderer_create_base(IJK_GLES2_getFragmentShader_yuv420p(), openglVer);
    if (!renderer)
        goto fail;

    renderer->us2_sampler[0] = glGetUniformLocation(renderer->program, "us2_SamplerX"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(us2_SamplerX)");
    renderer->us2_sampler[1] = glGetUniformLocation(renderer->program, "us2_SamplerY"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(us2_SamplerY)");
    renderer->us2_sampler[2] = glGetUniformLocation(renderer->program, "us2_SamplerZ"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(us2_SamplerZ)");

    renderer->um3_color_conversion = glGetUniformLocation(renderer->program, "um3_ColorConversion"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(um3_ColorConversionMatrix)");
	renderer->um3_rgb_adjustment = glGetUniformLocation(renderer->program, "um3_rgbAdjustment"); IJK_GLES2_checkError_TRACE("glGetUniformLocation(um3_rgbAdjustment)");

    renderer->func_use            = yuv420p_use;
    renderer->func_getBufferWidth = yuv420p_getBufferWidth;
    renderer->func_uploadTexture  = yuv420p_uploadTexture;
	renderer->func_useSubtitle = yuv420p_useSubtile;
	renderer->func_uploadSubtitle = yuv420p_uploadSubtitle;

	renderer->opaque = calloc(1, sizeof(IJK_GLES2_Renderer_Opaque));
	if (!renderer->opaque)
		goto fail;
	memset(renderer->opaque, sizeof(IJK_GLES2_Renderer_Opaque), 0);

	GLint isSubtitle = glGetUniformLocation(renderer->program, "isSubtitle");
	assert(isSubtitle >= 0);
	renderer->opaque->isSubtitle = isSubtitle;

	//todo
	renderer->subtitle_bottom_margin = 0.7;
    return renderer;
fail:
    IJK_GLES2_Renderer_free(renderer);
    return NULL;
}
