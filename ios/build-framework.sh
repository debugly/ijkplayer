#! /usr/bin/env bash
#
# Copyright (C) 2024 Matt Reach<qianlongxu@gmail.com>

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

THIS_DIR=$(DIRNAME=$(dirname "$0"); cd "$DIRNAME"; pwd)
cd "$THIS_DIR"

set -e

# 1
WORKSPACE_NAME="IJKMediaPlayer/IJKMediaPlayer.xcodeproj"
TARGET_NAME="IJKMediaFramework"

WORK_DIR="IJKMediaPlayer/Release/Release-iphoneos"
SIM_WORK_DIR="IJKMediaPlayer/Release/Release-iphonesimulator"

# 2
if [ -d ${WORK_DIR} ]; then
    rm -rf ${WORK_DIR}
fi

if [ -d ${SIM_WORK_DIR} ]; then
    rm -rf ${SIM_WORK_DIR}
fi

# 3
# project方式
# xcodebuild -showsdks
# Build the framework for device and simulator with all architectures.
export IPHONEOS_DEPLOYMENT_TARGET=11.0

xcodebuild -project ${WORKSPACE_NAME} -scheme ${TARGET_NAME} \
-configuration Release  \
-destination 'generic/platform=iOS' \
-destination 'generic/platform=iOS Simulator' \
BUILD_DIR=. \
clean build

echo "ios framework dir:$WORK_DIR"
echo "ios simulator framework dir: $SIM_WORK_DIR"