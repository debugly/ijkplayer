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
    echo "  $0 [windows] [build|clean] [all|fdk-aac|ffmpeg|lame|libyuv|openssl|opus|x264] [win32|x86_64|all] [opts...]"
}



ALL_ARCHS="win32 x64"

if [[ "$PLAT" != 'windows' ]]; then
    echo "plat must be: [windows]"
    usage
    exit 1
fi

if [[ -z "$LIBS" || "$LIBS" == "all" ]]; then
    LIBS=$(cat compile-cfgs/list.txt)
fi

if [[ -z "$ARCH" || "$ARCH" == 'all' ]]; then
    ARCH="$ALL_ARCHS"
fi

if [[ -z "$CMD" ]]; then
    echo "cmd must be: [build| clean]"
    usage
    exit 1
fi

export SRC_ROOT="${SHELL_DIR}/../build/src/${PLAT}"
export PRODUCT_ROOT="${SHELL_DIR}/../build/product/${PLAT}"

export PLAT="$PLAT"
export CMD="$CMD"
export OPTS="$OPTS"
export TARGET_ARCHS="$ARCH"
export VENDOR_LIBS="$LIBS"
export TARGET_OS="mingw32-w64"


echo '------------------------------------------'
echo "XC_PLAT         : [$PLAT]"
echo "XC_CMD          : [$CMD]"
echo "XC_VENDOR_LIBS  : [$VENDOR_LIBS]"
echo "XC_TARGET_ARCHS : [$ARCH]"
echo "XC_OPTS         : [$OPTS]"
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