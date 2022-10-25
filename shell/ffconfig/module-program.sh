#! /usr/bin/env bash

# module 文件是通过 source 的方式加载的，因此 $0 指的是调用方：./do-compile/ffmpeg.sh
# BASH_SOURCE 是个栈，里面存了调用时文件列表
# source 执行好处是在当前进程里执行的，因此所有上下文共用，声明的环境变量等在执行完文件后，仍旧有效
# "副作用": 覆盖同名变量，$0 不是当前文件路径，修改了自己的工作目录就等于修改调用者的目录等。

enter_path=$PWD
dir=$(dirname ${BASH_SOURCE[0]}) && pwd
source $dir/module-full.sh
cd ${enter_path}

# enable programs 
export COMMON_FF_CFG_FLAGS="$COMMON_FF_CFG_FLAGS --enable-avdevice"
export COMMON_FF_CFG_FLAGS="$COMMON_FF_CFG_FLAGS --enable-avfilter"
export COMMON_FF_CFG_FLAGS="$COMMON_FF_CFG_FLAGS --enable-sdl2"
export COMMON_FF_CFG_FLAGS="$COMMON_FF_CFG_FLAGS --enable-ffmpeg"
export COMMON_FF_CFG_FLAGS="$COMMON_FF_CFG_FLAGS --enable-ffplay"
export COMMON_FF_CFG_FLAGS="$COMMON_FF_CFG_FLAGS --enable-ffprobe"