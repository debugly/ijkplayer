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

set -e

# 当前脚本所在目录
TOOLS=$(dirname "$0")
source $TOOLS/../../tools/env_assert.sh

echo "=== [$0] check env begin==="
env_assert "TARGET_ARCHS"
env_assert "LIB_NAME"
env_assert "CMD"
echo "ARGV:$*"
echo "===check env end==="



do_install_all () {
    local archs="$1"
    rm -rf $UNI_PROD_DIR/$LIB_NAME
    mkdir -p $UNI_PROD_DIR/$LIB_NAME/lib
    echo "lipo archs: $archs"

    for arch in $archs
    do
        local ARCH_INC_DIR="$PRODUCT_ROOT/$LIB_NAME-$arch/include"
        local ARCH_OUT_DIR="$UNI_PROD_DIR/$LIB_NAME/include"
        if [[ -d "$ARCH_INC_DIR" && ! -d "$ARCH_OUT_DIR" ]]; then
            echo "copy include dir to $ARCH_OUT_DIR"
            cp -R "$ARCH_INC_DIR" "$ARCH_OUT_DIR"
            break
        fi
    done
}

function export_arch_env() {
    # x86_64
    export ARCH=$1
    # ffmpeg-x86_6
    export BUILD_NAME="${LIB_NAME}-${ARCH}"
    # ios/ffmpeg-x86_64
    export BUILD_SOURCE="${SRC_ROOT}/${BUILD_NAME}"
    # ios/ffmpeg-x86_64
    export BUILD_PREFIX="${PRODUCT_ROOT}/${BUILD_NAME}"
}

function do_compile() {
    export_arch_env $1
    if [ ! -d $BUILD_SOURCE ]; then
        echo ""
        echo "!! ERROR"
        echo "!! Can not find $BUILD_SOURCE directory for $BUILD_NAME"
        echo "!! Run init-any.sh ${LIB_NAME} first"
        echo ""
        exit 1
    fi

    mkdir -p "$BUILD_PREFIX"
    local opt=$2
    ./do-compile/$LIB_NAME.sh $opt
}


function do_clean() {
    export_arch_env $1
    echo "BUILD_SOURCE:$XC_BUILD_SOURCE"
    cd $BUILD_SOURCE && git clean -xdf && cd - >/dev/null
    rm -rf $BUILD_PREFIX >/dev/null
}

function main() {

    local cmd="$CMD"
    local archs="$TARGET_ARCHS"
    local opt="$OPTS"

    case "$cmd" in
        'clean')
            for arch in $archs
            do
                do_clean $arch
            done
            rm -rf $UNI_PROD_DIR/$LIB_NAME
            echo 'done.'
        ;;
        'build')
            for arch in $archs
            do
                do_compile $arch "$opt"
                echo
            done

            do_install_all "$archs"
        ;;
        *)
            echo "Usage:"
            echo "    $0 [build|clean] [win32|x64]"
            exit 1
        ;;
    esac
}

main