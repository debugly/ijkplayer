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

#include "ijksdl/gles2/internal.h"

void IJK_GLES2_getVertexShader_default(char *out,int ver)
{
    *out = '\0';
    
    sprintf(out, "#version %d\n",ver);

#if TARGET_OS_IOS
    strcat(out,"precision highp float;\n");
#endif
    
    if (ver >= 330) {
        strcat(out, IJK_GLES_STRING(
                    out vec2 vv2_Texcoord;
                    in vec2 av2_Texcoord;
                    in vec4 av4_Position;
                    uniform mat4 um4_ModelViewProjection;
                                    ));
    } else {
        strcat(out, IJK_GLES_STRING(
                    varying   vec2 vv2_Texcoord;
                    attribute vec4 av4_Position;
                    attribute vec2 av2_Texcoord;
                    uniform   mat4 um4_ModelViewProjection;
                                    ));
    }
    
    strcat(out, IJK_GLES_STRING(
                void main()
                {
                    gl_Position  = um4_ModelViewProjection * av4_Position;
                    vv2_Texcoord = av2_Texcoord.xy;
                }
                                ));
}
