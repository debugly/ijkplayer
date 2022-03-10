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

#include "ijksdl/gles2/internal.h"

static const char g_shader[] = IJK_GLES_STRING(
    precision highp float;
    varying   highp vec2 vv2_Texcoord;
    uniform         mat3 um3_ColorConversion;
	uniform			vec3 um3_rgbAdjustment;

    uniform   lowp  sampler2D us2_SamplerX;
    uniform   lowp  sampler2D us2_SamplerY;
    uniform   lowp  sampler2D us2_SamplerZ;

	uniform   int   isSubtitle;

	vec3 rgb_adjust(vec3 rgb, vec3 rgbAdjustment) {
		float B = rgbAdjustment.x;
		float S = rgbAdjustment.y;
		float C = rgbAdjustment.z;
		rgb = (rgb - 0.5) * C + 0.5;
		rgb = rgb + (0.75 * B - 0.5) / 2.5 - 0.1;
		vec3 intensity = vec3(dot(rgb, vec3(0.299, 0.587, 0.114)));
		return intensity + S * (rgb - intensity);
	}

    void main()
    {
		if (isSubtitle == 1) {
			gl_FragColor = texture2D(us2_SamplerX, vv2_Texcoord);
		}
		else {
			mediump vec3 yuv;
			lowp    vec3 rgb;

			yuv.x = (texture2D(us2_SamplerX, vv2_Texcoord).r - (16.0 / 255.0));
			yuv.y = (texture2D(us2_SamplerY, vv2_Texcoord).r - 0.5);
			yuv.z = (texture2D(us2_SamplerZ, vv2_Texcoord).r - 0.5);
			rgb = um3_ColorConversion * yuv;
			rgb = rgb_adjust(rgb, um3_rgbAdjustment);
			gl_FragColor = vec4(rgb, 1);
		}
    }
);

const char *IJK_GLES2_getFragmentShader_yuv420p()
{
    return g_shader;
}
