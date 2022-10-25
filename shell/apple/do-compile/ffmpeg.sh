#! /usr/bin/env bash
#
# Copyright (C) 2021 Matt Reach<qianlongxu@gmail.com>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This script is based on projects below
# https://github.com/bilibili/ijkplayer

set -e

TOOLS=$(dirname "$0")
source $TOOLS/../../tools/env_assert.sh

echo "=== [$0] check env begin==="
env_assert "XC_TAGET_OS"
env_assert "XC_ARCH"
env_assert "XC_PRODUCT_ROOT"
env_assert "XC_BUILD_NAME"
env_assert "XC_BUILD_SOURCE"
env_assert "XC_BUILD_PREFIX"
env_assert "XC_DEPLOYMENT_TARGET"
echo "ARGV:$*"
echo "===check env end==="

FF_BUILD_OPT=$1

# ffmpeg build params
source `pwd`/../ffconfig/module.sh
FFMPEG_CFG_FLAGS="$COMMON_FF_CFG_FLAGS"

FFMPEG_CFG_FLAGS="--prefix=$XC_BUILD_PREFIX $FFMPEG_CFG_FLAGS"

# Developer options (useful when working on FFmpeg itself):
# FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --disable-stripping"

##
FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --arch=$XC_ARCH"
FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --target-os=$XC_TAGET_OS"
FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-static"
FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --disable-shared"

# x86_64, arm64
FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-pic"
FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-neon"
FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-asm"

if [[ "$FF_BUILD_OPT" == "debug" ]];then
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --disable-optimizations"
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-debug"
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --disable-small"
else
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-optimizations"
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --disable-debug"
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-small"
fi

# FFMPEG_C_FLAGS
FFMPEG_C_FLAGS=
FFMPEG_C_FLAGS="$FFMPEG_C_FLAGS -fno-stack-check -arch $XC_ARCH"
FFMPEG_C_FLAGS="$FFMPEG_C_FLAGS $XC_DEPLOYMENT_TARGET $XC_OTHER_CFLAGS"

# for cross compile
if [[ $(uname -m) != "$XC_ARCH" || "$XC_FORCE_CROSS" ]];then
    echo "[*] cross compile, on $(uname -m) compile $XC_PLAT $XC_ARCH."
    # https://www.gnu.org/software/automake/manual/html_node/Cross_002dCompilation.html
    FFMPEG_C_FLAGS="$FFMPEG_C_FLAGS -isysroot $XCRUN_SDK_PATH"
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-cross-compile"
fi

FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --pkg-config-flags=--static"

FFMPEG_LDFLAGS="$FFMPEG_C_FLAGS"
FFMPEG_DEP_LIBS=

echo "----------------------"
echo "[*] check OpenSSL"

# https://ffmpeg.org/doxygen/4.1/md_LICENSE.html
# https://www.openssl.org/source/license.html

#----------------------
# with openssl
# use pkg-config fix ff4.0--ijk0.8.8--20210426--001 use openssl 1_1_1m occur can't find openssl error.
if [[ -f "${XC_PRODUCT_ROOT}/openssl-$XC_ARCH/lib/pkgconfig/openssl.pc" ]]; then
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-nonfree --enable-openssl"
   
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:${XC_PRODUCT_ROOT}/openssl-$XC_ARCH/lib/pkgconfig"
   
    echo "[*] --enable-openssl"
else
    echo "[*] --disable-openssl"
fi
echo "------------------------"

echo "----------------------"
echo "[*] check x264"

#----------------------
# with x264
if [[ -f "${XC_PRODUCT_ROOT}/x264-$XC_ARCH/lib/pkgconfig/x264.pc" ]]; then
    # libx264 is gpl and --enable-gpl is not specified.
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-gpl --enable-libx264"
    
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:${XC_PRODUCT_ROOT}/x264-$XC_ARCH/lib/pkgconfig"

    echo "[*] --enable-libx264"
else
    echo "[*] --disable-libx264"
fi
echo "------------------------"

echo "----------------------"
echo "[*] check fdk-aac"

#----------------------
# with fdk-aac
if [[ -f "${XC_PRODUCT_ROOT}/fdk-aac-$XC_ARCH/lib/pkgconfig/fdk-aac.pc" ]]; then

    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-nonfree --enable-libfdk-aac"
    
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:${XC_PRODUCT_ROOT}/fdk-aac-$XC_ARCH/lib/pkgconfig"

    echo "[*] --enable-libfdk-aac"
else
    echo "[*] --disable-libfdk-aac"
fi
echo "------------------------"

echo "----------------------"
echo "[*] check mp3lame"

#----------------------
# with lame
if [[ -f "${XC_PRODUCT_ROOT}/lame-$XC_ARCH/lib/libmp3lame.a" ]]; then
    # libmp3lame is gpl and --enable-gpl is not specified.
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-gpl --enable-libmp3lame"
    
    FDKAAC_C_FLAGS="-I${XC_PRODUCT_ROOT}/lame-$XC_ARCH/include"
    FDKAAC_LD_FLAGS="-L${XC_PRODUCT_ROOT}/lame-$XC_ARCH/lib -lmp3lame"

    FFMPEG_C_FLAGS="$FFMPEG_C_FLAGS $FDKAAC_C_FLAGS"
    FFMPEG_DEP_LIBS="$FFMPEG_DEP_LIBS $FDKAAC_LD_FLAGS"
    echo "[*] --enable-libmp3lame"
else
    echo "[*] --disable-libmp3lame"
fi
echo "------------------------"

echo "----------------------"
echo "[*] check opus"

#----------------------
# with opus
if [[ -f "${XC_PRODUCT_ROOT}/opus-$XC_ARCH/lib/pkgconfig/opus.pc" ]]; then
    
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-libopus --enable-decoder=opus"
    
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:${XC_PRODUCT_ROOT}/opus-$XC_ARCH/lib/pkgconfig"

    echo "[*] --enable-libopus --enable-decoder=opus"
else
    echo "[*] --disable-libopus"
fi
echo "------------------------"

#----------------------
# with bluray
if [[ -f "${XC_PRODUCT_ROOT}/bluray-$XC_ARCH/lib/pkgconfig/libbluray.pc" ]]; then
    
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-libbluray --enable-protocol=bluray"
    
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:${XC_PRODUCT_ROOT}/bluray-$XC_ARCH/lib/pkgconfig"

    echo "[*] --enable-libbluray --enable-protocol=bluray"
else
    echo "[*] --disable-libbluray --disable-protocol=bluray"
fi
echo "------------------------"

#----------------------
# FFmpeg 4.2 支持AV1、AVS2等格式
# dav1d由VideoLAN，VLC和FFmpeg联合开发，项目由AOM联盟赞助，和libaom相比，dav1d性能普遍提升100%，最高提升400%
if [[ -f "${XC_PRODUCT_ROOT}/dav1d-$XC_ARCH/lib/pkgconfig/dav1d.pc" ]]; then
    FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-decoder=av1 --enable-libdav1d"
    
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:${XC_PRODUCT_ROOT}/dav1d-$XC_ARCH/lib/pkgconfig"

    echo "[*] --enable-decoder=av1 --enable-libdav1d"
else
    echo "[*] --disable-decoder=av1 --disable-libdav1d"
fi
echo "------------------------"

#parser subtitles
FFMPEG_CFG_FLAGS="$FFMPEG_CFG_FLAGS --enable-demuxer=ass --enable-demuxer=webvtt --enable-demuxer=srt"
    
CC="$XCRUN_CC"

#----------------------
echo "----------------------"
echo "[*] configure"
echo "------------------------"

if [[ ! -d $XC_BUILD_SOURCE ]]; then
    echo ""
    echo "!! ERROR"
    echo "!! Can not find $XC_BUILD_SOURCE directory for $XC_BUILD_NAME"
    echo "!! Run 'init-*.sh' first"
    echo ""
    exit 1
fi

cd $XC_BUILD_SOURCE
if [[ -f "./config.h" ]]; then
    echo 'reuse configure'
else
    echo 
    echo "CC: $CC"
    echo
    echo "CFLAGS: $FFMPEG_C_FLAGS"
    echo
    echo "FF_CFG_FLAGS: $FFMPEG_CFG_FLAGS"
    echo
    echo "LDFLAG:$FFMPEG_LDFLAGS $FFMPEG_DEP_LIBS"
    echo 
    ./configure \
        $FFMPEG_CFG_FLAGS \
        --cc="$CC" \
        --extra-cflags="$FFMPEG_C_FLAGS" \
        --extra-cxxflags="$FFMPEG_C_FLAGS" \
        --extra-ldflags="$FFMPEG_LDFLAGS $FFMPEG_DEP_LIBS"
fi

#----------------------
echo "----------------------"
echo "[*] compile $LIB_NAME"
echo "----------------------"

make clean
cp config.* $XC_BUILD_PREFIX
make install -j8 1>/dev/null
mkdir -p $XC_BUILD_PREFIX/include/libffmpeg
cp -f config.h $XC_BUILD_PREFIX/include/libffmpeg/config.h
# copy private header.
#cp -f $XC_BUILD_SOURCE/libavformat/avc.h $XC_BUILD_PREFIX/include/libavformat/avc.h