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
env_assert "XC_BUILD_PREFIX"
env_assert "XC_BUILD_NAME"
env_assert "XC_DEPLOYMENT_TARGET"
env_assert "XCRUN_SDK_PATH"
env_assert "XCRUN_CC"
echo "ARGV:$*"
echo "===check env end==="

# prepare build config
DAV1D_CFG_FLAGS="--prefix=$XC_BUILD_PREFIX --buildtype release --default-library static"
CFLAGS="-arch $XC_ARCH $XC_DEPLOYMENT_TARGET $XC_OTHER_CFLAGS"

echo "----------------------"
echo "[*] compile $LIB_NAME"
echo "CC: $XCRUN_CC"
echo "DAV1D_CFG_FLAGS: $DAV1D_CFG_FLAGS"
echo "CFLAGS: $CFLAGS"
echo "----------------------"
echo

cd $XC_BUILD_SOURCE
export CC="$XCRUN_CC"
export CXX="$XCRUN_CXX"

if [[ $(uname -m) != "$XC_ARCH" || "$XC_FORCE_CROSS" ]]; then
   echo "[*] cross compile, on $(uname -m) compile $XC_PLAT $XC_ARCH."
   # https://www.gnu.org/software/automake/manual/html_node/Cross_002dCompilation.html
   CFLAGS="$CFLAGS -isysroot $XCRUN_SDK_PATH"
   BLURAY_CFG_FLAGS="$BLURAY_CFG_FLAGS --host=$XC_ARCH-apple-darwin --with-sysroot=$XCRUN_SDK_PATH"
   DAV1D_CFG_FLAGS="$DAV1D_CFG_FLAGS --cross-file package/crossfiles/$XC_ARCH-macos.meson"
fi

if [[ -d build ]]; then
   rm -rf build
fi

meson setup build $DAV1D_CFG_FLAGS >/dev/null

ninja -C build
ninja -C build install
