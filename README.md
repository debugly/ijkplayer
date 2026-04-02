# ijkplayer

<img src="https://img.shields.io/badge/Platform-%20iOS%20Android-blue.svg">

## 特色

- [x] 支持安卓15 和 iOS18
- [x] 保留了原汁原味的 ijkpalyer，功能没有变化
- [x] 预编译出了 Android 平台的 ijkpalyer.arr，取代之前的三个 so
- [x] 预编译出了 iOS 平台的 IJKMediaFramework.xcframework
- [x] ABI 支持：armv7a arm64 x86 x86_64
- [x] NDK 使用最新最稳定的 r27c
- [x] 将依赖的 ffmpeg、openssl 等所有三方库编译成静态库
- [x] openssl 升级到了最新 1.1.1w
- [x] soundtouch 升级到了 2.3.3，并且预编译成静态库
- [x] yuv 升级到最稳定分支最新提交，并且预编译成静态库
- [x] 使用 cmake 重新组织工程，抛弃之前的 ndk-build
- [x] 合并 jni 调用，缩减了 so 的数量
- [x] 使用全新的编译脚本，ffmpeg 使用 module-lite.sh
- [x] 支持 rtsp 协议 

## 安装使用

- iOS
  
  ```
  pod "IJKMediaFramework", :podspec => 'https://github.com/debugly/ijkplayer/releases/download/k0.8.9-beta-260402150035/IJKMediaFramework.spec.json'
  ```

- android
  
  ```
  https://github.com/debugly/ijkplayer/releases/download/k0.8.9-beta-260402150035/ijkplayer-cmake-release.aar
  ```
  
  更多版本，查看 [Releases](https://github.com/debugly/ijkplayer/releases) 页面。

## 运行 Demo

原版 demo 可以正常运行：

- iOS
  
  ```
  git submodule update --init
  cd ios
  ./install-ffmpeg.sh
  open IJKMediaDemo/IJKMediaDemo.xcodeproj
  ```

- android
  
  ```
  git submodule update --init
  cd android
  ./install-ffmpeg.sh
  # 使用 Android Studio 打开 ijkplayer 目录工程
  ```

## FSPlayer

如果 ijkplayer 功能不能满足当前复杂的业务需求，则可以使用 ijkplayer 的升级版 [fsplayer](https://github.com/debugly/fsplayer) ，它提供了更加强劲的功能。

## 定制功能

请邮件联系：[debugly@icloud.com](mailto:debugly@icloud.com)
