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
# https://wiki.openssl.org/index.php/Compilation_and_Installation#OS_X

set -e

TOOLS=$(dirname "$0")
source $TOOLS/../../tools/env_assert.sh

echo "=== [$0] check env begin==="
env_assert "ARCH"
env_assert "BUILD_SOURCE"
env_assert "BUILD_PREFIX"
env_assert "BUILD_NAME"

echo "ARGV:$*"
echo "===check env end==="

# prepare build config
OPENSSL_CFG_FLAGS="--prefix=$BUILD_PREFIX --openssldir=$BUILD_PREFIX shared no-hw no-engine no-asm"

if [ "$ARCH" = "x86_64" ]; then
    OPENSSL_CFG_FLAGS="$OPENSSL_CFG_FLAGS  mingw64 enable-ec_nistp_64_gcc_128"
elif [ "$ARCH" = "x86" ]; then
    OPENSSL_CFG_FLAGS="$OPENSSL_CFG_FLAGS mingw"
else
    echo "unknown architecture $FF_ARCH";
    exit 1
fi

CFLAGS=""

# for cross compile
if [[ $(uname -m) != "$ARCH" || "$FORCE_CROSS" ]];then
    echo "[*] cross compile, on $(uname -m) compile $PLAT $ARCH."
    # https://www.gnu.org/software/automake/manual/html_node/Cross_002dCompilation.html
    #CFLAGS="$CFLAGS  --enable-cross-compile"
fi

#----------------------
echo "----------------------"
echo "[*] configurate $LIB_NAME"
echo "----------------------"

cd $BUILD_SOURCE
if [ -f "./Makefile" ]; then
    echo 'reuse configure'
else
    echo 
    echo "CC: $CC"
    echo "CFLAGS: $CFLAGS"
    echo "Openssl CFG: $OPENSSL_CFG_FLAGS"
    echo 
    ./Configure $OPENSSL_CFG_FLAGS \
        CFLAGS="$CFLAGS" \
        CXXFLAG="$CFLAGS" 

    make clean 1>/dev/null
fi

#----------------------
echo "----------------------"
echo "[*] compile $LIB_NAME"
echo "----------------------"
set +e
make -j8 1>/dev/null
make install_sw
