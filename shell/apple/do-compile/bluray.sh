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
env_assert "XC_PRODUCT_ROOT"
env_assert "XC_BUILD_PREFIX"
env_assert "XC_BUILD_NAME"
env_assert "XC_DEPLOYMENT_TARGET"
env_assert "XCRUN_SDK_PATH"
env_assert "XCRUN_CC"
echo "ARGV:$*"
echo "===check env end==="

# prepare build config
BLURAY_CFG_FLAGS="--prefix=$XC_BUILD_PREFIX --disable-dependency-tracking --disable-silent-rules --disable-bdjava-jar --without-freetype --without-fontconfig --disable-doxygen-doc"
CFLAGS="-arch $XC_ARCH $XC_DEPLOYMENT_TARGET $XC_OTHER_CFLAGS"

# for cross compile
if [[ $(uname -m) != "$XC_ARCH" || "$XC_FORCE_CROSS" ]];then
    echo "[*] cross compile, on $(uname -m) compile $XC_PLAT $XC_ARCH."
    # https://www.gnu.org/software/automake/manual/html_node/Cross_002dCompilation.html
    CFLAGS="$CFLAGS -isysroot $XCRUN_SDK_PATH"
    BLURAY_CFG_FLAGS="$BLURAY_CFG_FLAGS --host=$XC_ARCH-apple-darwin --with-sysroot=$XCRUN_SDK_PATH"
fi

echo "----------------------"
echo "[*] configurate $LIB_NAME"
echo "----------------------"

cd $XC_BUILD_SOURCE

if [[ -f 'configure' ]]; then
   echo "reuse configure"
else
   echo "auto generate configure"
   ./bootstrap 1>/dev/null
fi


echo 
echo "CC: $XCRUN_CC"
echo "BLURAY_CFG_FLAGS: $BLURAY_CFG_FLAGS"
echo "CFLAGS: $CFLAGS"
echo 

./configure $BLURAY_CFG_FLAGS \
   CC="$XCRUN_CC" \
   CFLAGS="$CFLAGS" \
   LDFLAGS="$CFLAGS" \
   1>/dev/null

#----------------------
echo "----------------------"
echo "[*] compile $LIB_NAME"
echo "----------------------"

make install -j8 1>/dev/null