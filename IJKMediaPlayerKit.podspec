#
# Be sure to run `pod lib lint IJKMediaPlayerKit.podspec' to ensure this is a
# valid spec before submitting.
#
# Any lines starting with a # are optional, but their use is encouraged
# To learn more about a Podspec see https://guides.cocoapods.org/syntax/podspec.html
#

Pod::Spec.new do |s|
  s.name             = 'IJKMediaPlayerKit'
  s.version          = '0.12.0'
  s.summary          = 'IJKMediaPlayerKit for ios/macOS/tvOS.'
  
# This description is used to generate tags and improve search results.
#   * Think: What does it do? Why did you write it? What is the focus?
#   * Try to keep it short, snappy and to the point.
#   * Write the description between the DESC delimiters below.
#   * Finally, don't worry about the indent, CocoaPods strips it!

  s.description      = <<-DESC
TODO: Add long description of the pod here.
                       DESC

  s.homepage         = 'https://github.com/debugly/ijkplayer'
  s.license          = { :type => 'LGPLv2.1', :text => 'LICENSE' }
  s.author           = { 'MattReach' => 'qianlongxu@gmail.com' }
  s.source           = { :git => 'https://github.com/debugly/ijkplayer', :tag => s.version.to_s }

  #metal 2.0 required
  s.osx.deployment_target = '10.11'
  s.ios.deployment_target = '11.0'
  s.tvos.deployment_target = '12.0'

  s.osx.pod_target_xcconfig = {
    'ALWAYS_SEARCH_USER_PATHS' => 'YES',
    'HEADER_SEARCH_PATHS' => [
      '$(inherited)',
      '${PODS_TARGET_SRCROOT}/FFToolChain/build/product/macos/universal/ffmpeg/include',
      '${PODS_TARGET_SRCROOT}/FFToolChain/build/product/macos/universal/bluray/include',
      '${PODS_TARGET_SRCROOT}/FFToolChain/build/product/macos/universal/dvdread/include',
      '${PODS_TARGET_SRCROOT}/FFToolChain/build/product/macos/universal/ass/include',
      '${PODS_TARGET_SRCROOT}/ijkmedia'
    ],
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) IJK_USE_METAL_2=1',
    'METAL_LIBRARY_OUTPUT_DIR' => '${CONFIGURATION_BUILD_DIR}/IJKMediaPlayerKit.framework/Resources',
    'MTL_LANGUAGE_REVISION' => 'Metal20'
  }

  s.ios.pod_target_xcconfig = {
    'ALWAYS_SEARCH_USER_PATHS' => 'YES',
    'HEADER_SEARCH_PATHS[sdk=iphoneos*]' => '$(inherited) ${PODS_TARGET_SRCROOT}/ijkmedia ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/ffmpeg/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/dvdread/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/ass/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/bluray/include',
    'HEADER_SEARCH_PATHS[sdk=iphonesimulator*]' => '$(inherited) ${PODS_TARGET_SRCROOT}/ijkmedia ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/ffmpeg/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/dvdread/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/ass/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/bluray/include',
    'LIBRARY_SEARCH_PATHS[sdk=iphoneos*]' => '$(inherited) ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/ass/lib $(inherited) ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/uavs3d/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/dav1d/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/dvdread/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/ffmpeg/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/freetype/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/fribidi/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/harfbuzz/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/openssl/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/opus/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/unibreak/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/smb2/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal/bluray/lib',
    'LIBRARY_SEARCH_PATHS[sdk=iphonesimulator*]' => '$(inherited) ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/ass/lib $(inherited) ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/uavs3d/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/dav1d/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/dvdread/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/ffmpeg/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/freetype/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/fribidi/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/harfbuzz/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/openssl/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/opus/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/unibreak/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/smb2/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/ios/universal-simulator/bluray/lib',
    'OTHER_LDFLAGS' => '$(inherited) -l"opus" -l"crypto" -l"ssl" -l"dav1d" -l"dvdread" -l"freetype" -l"fribidi" -l"harfbuzz" -l"harfbuzz-subset" -l"unibreak" -l"ass" -l"uavs3d" -l"avcodec" -l"avdevice" -l"avfilter" -l"avformat" -l"avutil" -l"swresample" -l"swscale" -l"smb2" -l"bluray"',
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) IJK_USE_METAL_2=1',
    'METAL_LIBRARY_OUTPUT_DIR' => '${CONFIGURATION_BUILD_DIR}/IJKMediaPlayerKit.framework',
    'MTL_LANGUAGE_REVISION' => 'Metal20'
  }

  s.tvos.pod_target_xcconfig = {
    'ALWAYS_SEARCH_USER_PATHS' => 'YES',
    'HEADER_SEARCH_PATHS[sdk=appletvos*]' => '$(inherited) ${PODS_TARGET_SRCROOT}/ijkmedia ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/ffmpeg/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/dvdread/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/ass/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/bluray/include',
    'HEADER_SEARCH_PATHS[sdk=appletvsimulator*]' => '$(inherited) ${PODS_TARGET_SRCROOT}/ijkmedia ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/ffmpeg/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/dvdread/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/ass/include ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/bluray/include',
    'LIBRARY_SEARCH_PATHS[sdk=appletvos*]' => '$(inherited) ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/ass/lib $(inherited) ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/uavs3d/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/dav1d/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/dvdread/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/ffmpeg/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/freetype/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/fribidi/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/harfbuzz/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/openssl/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/opus/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/unibreak/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/smb2/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal/bluray/lib',
    'LIBRARY_SEARCH_PATHS[sdk=appletvsimulator*]' => '$(inherited) ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/ass/lib $(inherited) ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/uavs3d/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/dav1d/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/dvdread/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/ffmpeg/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/freetype/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/fribidi/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/harfbuzz/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/openssl/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/opus/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/unibreak/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/smb2/lib ${PODS_TARGET_SRCROOT}/FFToolChain/build/product/tvos/universal-simulator/bluray/lib',
    'OTHER_LDFLAGS' => '$(inherited) -l"opus" -l"crypto" -l"ssl" -l"dav1d" -l"dvdread" -l"freetype" -l"fribidi" -l"harfbuzz" -l"harfbuzz-subset" -l"unibreak" -l"ass" -l"uavs3d" -l"avcodec" -l"avdevice" -l"avfilter" -l"avformat" -l"avutil" -l"swresample" -l"swscale" -l"smb2" -l"bluray"',
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) IJK_USE_METAL_2=1',
    'METAL_LIBRARY_OUTPUT_DIR' => '${CONFIGURATION_BUILD_DIR}/IJKMediaPlayerKit.framework',
    'MTL_LANGUAGE_REVISION' => 'Metal20'
  }

  s.script_phases = [
    { 
      :name => 'ijkversion.h',
      :shell_path => '/bin/sh',
      :script => 'sh "${PODS_TARGET_SRCROOT}/ijkmedia/ijkplayer/version.sh" "${PODS_TARGET_SRCROOT}/ijkmedia/ijkplayer" "ijkversion.h"',
      :execution_position => :before_compile
    }
  ]

  s.source_files = 
    'ijkmedia/ijkplayer/**/*.{h,c,m,cpp}',
    'ijkmedia/ijksdl/**/*.{h,c,m,cpp,metal}',
    'ijkmedia/wrapper/apple/*.{h,m}',
    'ijkmedia/tools/*.{h,c}'
  # s.project_header_files = 'ijkmedia/**/*.{h}'
  s.public_header_files =
    'ijkmedia/wrapper/apple/IJKMediaPlayback.h',
    'ijkmedia/wrapper/apple/IJKFFOptions.h',
    'ijkmedia/wrapper/apple/IJKFFMonitor.h',
    'ijkmedia/wrapper/apple/IJKFFMoviePlayerController.h',
    'ijkmedia/wrapper/apple/IJKMediaModule.h',
    'ijkmedia/wrapper/apple/IJKMediaPlayer.h',
    'ijkmedia/wrapper/apple/IJKNotificationManager.h',
    'ijkmedia/wrapper/apple/IJKKVOController.h',
    'ijkmedia/wrapper/apple/IJKVideoRenderingProtocol.h',
    'ijkmedia/wrapper/apple/IJKMediaPlayerKit.h',
    'ijkmedia/wrapper/apple/IJKInternalRenderView.h',
    'ijkmedia/ijkplayer/ff_subtitle_def.h',
    'ijkmedia/ijksdl/ijksdl_rectangle.h',
    'ijkmedia/tools/*.{h}'
  s.exclude_files = 
    'ijkmedia/ijksdl/ijksdl_extra_log.c',
    'ijkmedia/ijkplayer/ijkversion.h',
    'ijkmedia/ijkplayer/ijkavformat/ijkioandroidio.c',
    'ijkmedia/ijkplayer/android/**/*.*',
    'ijkmedia/ijksdl/android/**/*.*',
    'ijkmedia/ijksdl/ijksdl_egl.*',
    'ijkmedia/ijksdl/ijksdl_container.*',
    'ijkmedia/ijksdl/ffmpeg/ijksdl_vout_overlay_ffmpeg.{h,c}'
  s.osx.exclude_files = 
    'ijkmedia/ijksdl/ios/*.*',
    'ijkmedia/wrapper/apple/IJKAudioKit.*'
  s.ios.exclude_files = 
    'ijkmedia/ijksdl/mac/*.*',
    'ijkmedia/ijksdl/apple/ijksdl_gpu_opengl_macos.{h,m}',
    'ijkmedia/ijksdl/apple/ijksdl_gpu_opengl_fbo_macos.{h,m}',
    'ijkmedia/ijksdl/apple/ijksdl_gpu_opengl_renderer_macos.{h,m}',
    'ijkmedia/ijksdl/apple/ijksdl_gpu_opengl_shader_compiler.{h,m}',
    'ijkmedia/ijksdl/gles2/**/*.*',
    'ijkmedia/ijksdl/ijksdl_gles2.h'

  s.tvos.exclude_files = 
  'ijkmedia/ijksdl/mac/*.*',
  'ijkmedia/ijksdl/apple/ijksdl_gpu_opengl_macos.{h,m}',
  'ijkmedia/ijksdl/apple/ijksdl_gpu_opengl_fbo_macos.{h,m}',
  'ijkmedia/ijksdl/apple/ijksdl_gpu_opengl_renderer_macos.{h,m}',
  'ijkmedia/ijksdl/apple/ijksdl_gpu_opengl_shader_compiler.{h,m}',
  'ijkmedia/ijksdl/gles2/**/*.*',
  'ijkmedia/ijksdl/ijksdl_gles2.h'

  s.osx.vendored_libraries = 'FFToolChain/build/product/macos/universal/**/*.a'
  s.osx.frameworks = 'Cocoa', 'AudioUnit', 'OpenGL', 'GLKit', 'CoreImage'
  s.ios.frameworks = 'UIKit', 'OpenGLES'
  s.tvos.frameworks = 'UIKit', 'OpenGLES'

  s.library = 'z', 'iconv', 'xml2', 'bz2', 'lzma'
  s.frameworks = 'AVFoundation', 'AudioToolbox', 'CoreMedia', 'CoreVideo', 'VideoToolbox', 'Metal'
  
end
