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
# 

PLAT=$1
CMD=$2
LIBS=$3
ARCH=$4
OPTS=$5

set -e

# 当前脚本所在目录
cd "$(dirname "$0")"
SHELL_DIR="$PWD"

function usage() {
    echo " useage:"
    echo "  $0 [ios|macos] [build|lipo|clean] [all|ffmpeg|libyuv|openssl|opus|dav1d] [arm64|x86_64|all] [opts...]"
}

if [[ "$PLAT" != 'ios' && "$PLAT" != 'macos' ]]; then
    echo "plat must be: [ios|macos]"
    usage
    exit 1
fi

source 'init-env.sh'

if [[ -z "$LIBS" || "$LIBS" == "all" ]]; then
    LIBS=$(cat compile-cfgs/list.txt)
fi

if [[ -z "$ARCH" || "$ARCH" == 'all' ]]; then
    ARCH="$ALL_ARCHS"
fi

if [[ -z "$CMD" ]]; then
    echo "cmd must be: [build|lipo|clean]"
    usage
    exit 1
fi

export XC_SRC_ROOT="${SHELL_DIR}/../build/src/${PLAT}"
export XC_PRODUCT_ROOT="${SHELL_DIR}/../build/product/${PLAT}"
export XC_UNI_PROD_DIR="${XC_PRODUCT_ROOT}/universal"

export XC_PLAT="$PLAT"
export XC_CMD="$CMD"
export XC_TARGET_ARCHS="$ARCH"
export XC_OPTS="$OPTS"
export XC_VENDOR_LIBS="$LIBS"

if [[ "$PLAT" == 'ios' ]]; then
export XC_FORCE_CROSS=true
fi

echo '------------------------------------------'
echo "XC_PLAT         : [$XC_PLAT]"
echo "XC_CMD          : [$XC_CMD]"
echo "XC_VENDOR_LIBS  : [$XC_VENDOR_LIBS]"
echo "XC_TARGET_ARCHS : [$ARCH]"
echo "XC_OPTS         : [$XC_OPTS]"
echo "XC_FORCE_CROSS  : [$XC_FORCE_CROSS]"
echo '------------------------------------------'

# 循环编译所有的库
for lib in $LIBS
do
    echo "===[$CMD $lib]===================="
    source compile-cfgs/"$lib"
    
    ./do-compile/any.sh
    if [[ $? -eq 0 ]];then
        echo "🎉  Congrats"
        echo "🚀  ${LIB_NAME} successfully $CMD."
        echo
    fi
    echo "===================================="
done