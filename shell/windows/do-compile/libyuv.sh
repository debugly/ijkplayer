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

TOOLS=$(dirname "$0")
source $TOOLS/../../tools/env_assert.sh

echo "=== [$0] check env begin==="
env_assert "XC_ARCH"
env_assert "XC_BUILD_SOURCE"
env_assert "XC_BUILD_NAME"
env_assert "XC_DEPLOYMENT_TARGET"
env_assert "XCRUN_SDK_PATH"
env_assert "XC_BUILD_PREFIX"
echo "ARGV:$*"
echo "===check env end==="

CFLAGS="-arch $XC_ARCH $XC_DEPLOYMENT_TARGET $XC_OTHER_CFLAGS -fomit-frame-pointer -Iinclude/"

# for cross compile
if [[ $(uname -m) != "$XC_ARCH" || "$XC_FORCE_CROSS" ]];then
    echo "[*] cross compile, on $(uname -m) compile $XC_PLAT $XC_ARCH."
    # https://www.gnu.org/software/automake/manual/html_node/Cross_002dCompilation.html
    CFLAGS="$CFLAGS -isysroot $XCRUN_SDK_PATH"
fi

echo "CC: $XCRUN_CC"
echo "CXX: $XCRUN_CXX"
echo "CFLAGS: $CFLAGS"
echo 

cd "$XC_BUILD_SOURCE"

echo "----------------------"
echo "[*] configurate $LIB_NAME"
echo "----------------------"

make -f linux.mk clean >/dev/null

#----------------------
echo "----------------------"
echo "[*] compile libyuv"
echo "----------------------"

make -f linux.mk CC="$XCRUN_CC" CXX="$XCRUN_CXX" CFLAGS="$CFLAGS" CXXFLAGS="$CFLAGS" 1>/dev/null

mkdir -p "${XC_BUILD_PREFIX}/lib"
cp libyuv.a "${XC_BUILD_PREFIX}/lib"
cp -r include "${XC_BUILD_PREFIX}"

cd -