#! /usr/bin/env bash
#
# Copyright (C) 2022 Matt Reach<qianlongxu@gmail.com>

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
# ./install-pre-any.sh all
# ./install-pre-any.sh ios 'libyuv openssl opus bluray dav1d'
# ./install-pre-any.sh tvos 'openssl'
# ./install-pre-any.sh macos 'openssl ffmpeg'

#----------------------------------------------------------
# 当发布新版本库时，修改对应的 TAG 值
#----------------------------------------------------------
LIBYUV_TAG='libyuv-main-231127113441'
OPUS_TAG='opus-1.4-240613155640'
MAC_BLURAY_TAG='bluray-1.3.4-240605103055'
DAV1D_TAG='dav1d-1.3.0-240613162104'
OPENSSL_TAG='openssl-1.1.1w-240613155307'
DVDREAD_TAG='dvdread-6.1.3-240613162057'
FREETYPE_TAG='freetype-2.13.2-240613162051'
UNIBREAK_TAG='unibreak-5.1-240613162028'
FRIBIDI_TAG='fribidi-1.0.13-240613162046'
HARFBUZZ_TAG='harfbuzz-8.3.0-240613170910'
ASS_TAG='ass-0.17.1-240613173921'
FFMPEG_TAG='ffmpeg-5.1.4-240613175211'
#----------------------------------------------------------

set -e

PLAT=`echo $1 | tr '[:upper:]' '[:lower:]'`
LIBS=$2

THIS_DIR=$(DIRNAME=$(dirname "$0"); cd "$DIRNAME"; pwd)
cd "$THIS_DIR"

function install_lib ()
{
    local plat=$1
    ./tools/install-pre-lib.sh "$plat" "$TAG"
}

function usage() {
    echo "=== useage ===================="
    echo "Download pre-compiled libs from github:"
    echo " $0 [ios,macos,all] [all|ffmpeg|libyuv|openssl|opus|bluray|dav1d|freetype|fribidi|harfbuzz|unibreak|ass]"
    exit 1
}

if [[ -z "$LIBS" || "$LIBS" == "all" ]]; then
    LIBS=$(ls init-cfgs)
fi

if [[ "$PLAT" == 'ios' || "$PLAT" == 'macos' || "$PLAT" == 'tvos'|| "$PLAT" == 'all' ]]; then
    for lib in $LIBS
    do
        plat=$PLAT
        TAG=
        if [[ $lib == 'bluray' ]];then
            if [[ $plat == 'macos' || $plat == 'all' ]];then
                plat='macos'
            else
                echo "===[bluray] not support iOS platform, just skip it.===================="
                continue
            fi
        fi
        
        echo "===[install pre-compile $lib]===================="
        
        case $lib in
            'ffmpeg')
                TAG=$FFMPEG_TAG
            ;;
            'libyuv')
                TAG=$LIBYUV_TAG
            ;;
            'openssl')
                TAG=$OPENSSL_TAG
            ;;
            'opus')
                TAG=$OPUS_TAG
            ;;
            'bluray')
                TAG=$MAC_BLURAY_TAG
            ;;
            'dav1d')
                TAG=$DAV1D_TAG
            ;;
            'dvdread')
                TAG=$DVDREAD_TAG
            ;;
            'freetype')
                TAG=$FREETYPE_TAG
            ;;
            'harfbuzz')
                TAG=$HARFBUZZ_TAG
            ;;
            'fribidi')
                TAG=$FRIBIDI_TAG
            ;;
            'unibreak')
                TAG=$UNIBREAK_TAG
            ;;
            'ass')
                TAG=$ASS_TAG
            ;;
            *)
                echo "wrong lib name:$lib"
                usage
            ;;
        esac
        
        if [[ -z "$TAG" ]]; then
            echo "== $lib tag is empty,just skip it."
        else
            echo "== $plat $lib -> $TAG"
            install_lib $plat
        fi
        echo "===================================="
    done
else
    usage
fi
