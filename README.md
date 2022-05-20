# ijkplayer

 Platform | Build Status
 -------- | ------------
 Android | [![Build Status](https://github.com/debugly/ijkplayer/actions/workflows/android.yml/badge.svg)](https://github.com/debugly/ijkplayer/actions/workflows/android.yml) ⚠️ unknown state 
 iOS | [![Build Status](https://github.com/debugly/ijkplayer/actions/workflows/ios.yml/badge.svg)](https://github.com/debugly/ijkplayer/actions/workflows/ios.yml)
macOS | [![Build Status](https://github.com/debugly/ijkplayer/actions/workflows/macos.yml/badge.svg)](https://github.com/debugly/ijkplayer/actions/workflows/macos.yml)

Video player based on [ffplay](http://ffmpeg.org)

### My Build Environment

- macOS Monterey(12.1)
- Xcode Version 13.1 (13A1030d)
- cocoapods 1.11.2

TODO check:
- Android
 - [NDK r10e](http://developer.android.com/tools/sdk/ndk/index.html)
 - Android Studio 2.1.3
 - Gradle 2.14.1

### Latest Changes

- [NEWS.md](NEWS.md)

### Features

- Common
    - remove rarely used ffmpeg components to reduce binary size [config/module-lite.sh](config/module-lite.sh)
    - workaround for some buggy online video.
- iOS/macOS
    - platform: iOS 9.0/macOS 10.11
    - cpu: arm64,x86_64
    - api: [MediaPlayer.framework-like](IJKMediaPlayerKit/IJKMediaPlayback.h)
    - video-output: OpenGL ES 2.0/OpenGL 3.3
    - audio-output: AudioQueue, AudioUnit
    - hw-decoder: auto use VideoToolbox accel by default
    - subtitle: use Quartz to draw text into a CVPixelBufferRef then use OpenGL render
    - alternative-backend: AVFoundation.Framework.AVPlayer, MediaPlayer.Framework.MPMoviePlayerControlelr (obselete since iOS 8)
- Android (⚠️ unknown state)
    - platform: API 9~23
    - cpu: ARMv7a, ARM64v8a, x86 (ARMv5 is not tested on real devices)
    - api: [MediaPlayer-like](android/ijkplayer/ijkplayer-java/src/main/java/tv/danmaku/ijk/media/player/IMediaPlayer.java)
    - video-output: NativeWindow, OpenGL ES 2.0
    - audio-output: AudioTrack, OpenSL ES
    - hw-decoder: MediaCodec (API 16+, Android 4.1+)
    - alternative-backend: android.media.MediaPlayer, ExoPlayer

### ON-PLAN

- use Metal instead of OpenGL
- avfilter support

### Before Build

```
# install homebrew, git, yasm
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew install git
brew install yasm

# add these lines to your ~/.bash_profile or ~/.profile
# export ANDROID_SDK=<your sdk path>
# export ANDROID_NDK=<your ndk path>

# on Cygwin (unmaintained)
# install git, make, yasm
```

- If you prefer more codec/format

```
cd config
rm module.sh
ln -s module-default.sh module.sh
cd android/contrib
# cd ios
sh compile-ffmpeg.sh clean
```

- If you prefer less codec/format for smaller binary size (include hevc function)

```
cd config
rm module.sh
ln -s module-lite-hevc.sh module.sh
cd android/contrib
# cd ios
sh compile-ffmpeg.sh clean
```

- If you prefer less codec/format for smaller binary size (by default)

```
cd config
rm module.sh
ln -s module-lite.sh module.sh
cd android/contrib
# cd ios
sh compile-ffmpeg.sh clean
```

- For Ubuntu/Debian users.

```
# choose [No] to use bash
sudo dpkg-reconfigure dash
```

- If you'd like to share your config, pull request is welcome.

### Build macOS

```
git clone https://github.com/debugly/ijkplayer.git ijkplayer
cd ijkplayer
git checkout -B latest k0.9.0.5

cd shell
./init-any.sh macos
cd macos
./compile-any.sh build all
pod install --project-directory=../../examples/macos
open ../../examples/macos/IJKMediaMacDemo.xcworkspace
```

### Build iOS

```
git clone https://github.com/debugly/ijkplayer.git ijkplayer
cd ijkplayer
git checkout -B latest k0.9.0.5

cd shell
./init-any.sh ios
cd ios
./compile-any.sh build all
pod install --project-directory=../../examples/ios
open ../../examples/macos/IJKMediaDemo.xcworkspace
```

### Build Android

```
git clone https://github.com/Bilibili/ijkplayer.git ijkplayer-android
cd ijkplayer-android
git checkout -B latest k0.8.8

./init-android.sh

cd android/contrib
./compile-ffmpeg.sh clean
./compile-ffmpeg.sh all

cd ..
./compile-ijk.sh all

# Android Studio:
#     Open an existing Android Studio project
#     Select android/ijkplayer/ and import
#
#     define ext block in your root build.gradle
#     ext {
#       compileSdkVersion = 23       // depending on your sdk version
#       buildToolsVersion = "23.0.0" // depending on your build tools version
#
#       targetSdkVersion = 23        // depending on your sdk version
#     }
#
# If you want to enable debugging ijkplayer(native modules) on Android Studio 2.2+: (experimental)
#     sh android/patch-debugging-with-lldb.sh armv7a
#     Install Android Studio 2.2(+)
#     Preference -> Android SDK -> SDK Tools
#     Select (LLDB, NDK, Android SDK Build-tools,Cmake) and install
#     Open an existing Android Studio project
#     Select android/ijkplayer
#     Sync Project with Gradle Files
#     Run -> Edit Configurations -> Debugger -> Symbol Directories
#     Add "ijkplayer-armv7a/.externalNativeBuild/ndkBuild/release/obj/local/armeabi-v7a" to Symbol Directories
#     Run -> Debug 'ijkplayer-example'
#     if you want to reverse patches:
#     sh patch-debugging-with-lldb.sh reverse armv7a
#
# Eclipse: (obselete)
#     File -> New -> Project -> Android Project from Existing Code
#     Select android/ and import all project
#     Import appcompat-v7
#     Import preference-v7
#
# Gradle
#     cd ijkplayer
#     gradle

```

### Support (支持)

- Please do not send e-mail to me. Public technical discussion on github is preferred.
- 请尽量在 github 上公开讨论[技术问题](https://github.com/debugly/ijkplayer/issues)，不要以邮件方式私下询问，恕不一一回复。

### License

```
Copyright (c) 2017 Bilibili
Licensed under LGPLv2.1 or later
```

ijkplayer required features are based on or derives from projects below:
- LGPL
  - [FFmpeg](http://git.videolan.org/?p=ffmpeg.git)
  - [libVLC](http://git.videolan.org/?p=vlc.git)
  - [kxmovie](https://github.com/kolyvan/kxmovie)
  - [soundtouch](http://www.surina.net/soundtouch/sourcecode.html)
- zlib license
  - [SDL](http://www.libsdl.org)
- BSD-style license
  - [libyuv](https://code.google.com/p/libyuv/)
- ISC license
  - [libyuv/source/x86inc.asm](https://code.google.com/p/libyuv/source/browse/trunk/source/x86inc.asm)

android/ijkplayer-exo is based on or derives from projects below:
- Apache License 2.0
  - [ExoPlayer](https://github.com/google/ExoPlayer)

android/example is based on or derives from projects below:
- GPL
  - [android-ndk-profiler](https://github.com/richq/android-ndk-profiler) (not included by default)

ios/IJKMediaDemo is based on or derives from projects below:
- Unknown license
  - [iOS7-BarcodeScanner](https://github.com/jpwiddy/iOS7-BarcodeScanner)

ijkplayer's build scripts are based on or derives from projects below:
- [gas-preprocessor](http://git.libav.org/?p=gas-preprocessor.git)
- [VideoLAN](http://git.videolan.org)
- [yixia/FFmpeg-Android](https://github.com/yixia/FFmpeg-Android)
- [kewlbear/FFmpeg-iOS-build-script](https://github.com/kewlbear/FFmpeg-iOS-build-script) 

### Commercial Use

ijkplayer is licensed under LGPLv2.1 or later, so itself is free for commercial use under LGPLv2.1 or later

But ijkplayer is also based on other different projects under various licenses, which I have no idea whether they are compatible to each other or to your product.

[IANAL](https://en.wikipedia.org/wiki/IANAL), you should always ask your lawyer for these stuffs before use it in your product.

### Demo Icon

- [Mac](https://iconduck.com/icons/506/bilibili)
