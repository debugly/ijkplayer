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

#ifndef IJKSDL__IJKSDL_GLES2__MATHUTIL__H
#define IJKSDL__IJKSDL_GLES2__MATHUTIL__H

#define _USE_MATH_DEFINES
#include <math.h>

typedef struct { float x;float y;float z; } ijk_float3_vector;
typedef struct { float c1;float c2;float c3;float c4; } ijk_float4_vector;
/*! @abstract A matrix with 4 rows and 4 columns.*/
typedef struct { float e[4][4]; } ijk_matrix;

static inline ijk_float3_vector vector_make_float3(float x, float y, float z) {
    return (ijk_float3_vector){ x, y, z };
}

static inline ijk_matrix make_matrix_use_rows(
             float m00, float m10, float m20, float m30,
             float m01, float m11, float m21, float m31,
             float m02, float m12, float m22, float m32,
             float m03, float m13, float m23, float m33) {
    return (ijk_matrix)
    {{
        { m00, m01, m02, m03 },     // each line here provides column data
        { m10, m11, m12, m13 },
        { m20, m21, m22, m23 },
        { m30, m31, m32, m33 }
    }};
}

static inline ijk_matrix ijk_make_matrix_use_arr(float m[16]) {
    return (ijk_matrix)
    {{
        { m[0], m[4], m[8],  m[12] },     // each line here provides column data
        { m[1], m[5], m[9],  m[13] },
        { m[2], m[6], m[10], m[14] },
        { m[3], m[7], m[11], m[15] }
    }};
}

static inline float degrees_from_radians(float radians) {
    return (radians / M_PI) * 180;
}

static inline float radians_from_degrees(float degrees) {
    return (degrees / 180) * M_PI;
}

ijk_matrix ijk_make_rotate_matrix(float radians, ijk_float3_vector axis);
ijk_matrix ijk_make_rotate_matrix_xyz(float radians, float x, float y, float z);
void ijk_matrix_multiply(const ijk_matrix * m, const ijk_matrix * n, ijk_matrix * r);

#endif
