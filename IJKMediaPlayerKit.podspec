#
# Be sure to run `pod lib lint IJKMediaPlayerKit.podspec' to ensure this is a
# valid spec before submitting.
#
# Any lines starting with a # are optional, but their use is encouraged
# To learn more about a Podspec see https://guides.cocoapods.org/syntax/podspec.html
#

Pod::Spec.new do |s|
  s.name             = 'IJKMediaPlayerKit'
  s.version          = '0.9.0.5'
  s.summary          = 'IJKMediaPlayerKit for ios/macOS.'
  
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

  s.osx.deployment_target = '10.11'
  s.ios.deployment_target = '9.0'

  s.osx.pod_target_xcconfig = {
    'ALWAYS_SEARCH_USER_PATHS' => 'YES',
    'HEADER_SEARCH_PATHS' => [
      '$(inherited)',
      '${PODS_TARGET_SRCROOT}/shell/build/product/macos/universal/ffmpeg/include',
      '${PODS_TARGET_SRCROOT}/shell/build/product/macos/universal/libyuv/include',
      '${PODS_TARGET_SRCROOT}/ijkmedia'
    ],
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) IJK_IO_OFF=0'
  }

  s.ios.pod_target_xcconfig = {
    'ALWAYS_SEARCH_USER_PATHS' => 'YES',
    'HEADER_SEARCH_PATHS' => [
      '$(inherited)',
      '${PODS_TARGET_SRCROOT}/shell/build/product/ios/universal/ffmpeg/include',
      '${PODS_TARGET_SRCROOT}/shell/build/product/ios/universal/libyuv/include',
      '${PODS_TARGET_SRCROOT}/ijkmedia'
    ],
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) IJK_IO_OFF=0'
  }

  s.script_phases = [
    { 
      :name => 'ijkversion.h',
      :shell_path => '/bin/sh',
      :script => 'sh "${PODS_TARGET_SRCROOT}/ijkmedia/ijkplayer/version.sh" "${PODS_TARGET_SRCROOT}/ijkmedia/ijkplayer" "ijkversion.h"',
      :execution_position => :before_compile
    }
  ]

  s.subspec 'IJKMediaPlayerKit' do |ss|
    ss.source_files = 'IJKMediaPlayerKit/*.{h,m}'
    ss.public_header_files =
      'IJKMediaPlayerKit/IJKMediaPlayback.h',
      'IJKMediaPlayerKit/IJKFFOptions.h',
      'IJKMediaPlayerKit/IJKFFMonitor.h',
      'IJKMediaPlayerKit/IJKFFMoviePlayerController.h',
      'IJKMediaPlayerKit/IJKMediaModule.h',
      'IJKMediaPlayerKit/IJKMediaPlayer.h',
      'IJKMediaPlayerKit/IJKNotificationManager.h',
      'IJKMediaPlayerKit/IJKKVOController.h',
      'IJKMediaPlayerKit/IJKSDLGLViewProtocol.h',
      'IJKMediaPlayerKit/IJKMediaPlayerKit.h'
    ss.osx.exclude_files = 'IJKMediaPlayerKit/IJKAudioKit.*'
  end

  s.subspec 'ijkmedia' do |ss|
    ss.source_files = 
      'ijkmedia/ijkplayer/**/*.{h,c,m,cpp}',
      'ijkmedia/ijksdl/**/*.{h,c,m,cpp}'
    ss.project_header_files = 'ijkmedia/**/*.{h}'

    ss.exclude_files = 
      'ijkmedia/ijksdl/ijksdl_extra_log.c',
      'ijkmedia/ijkplayer/ijkversion.h',
      'ijkmedia/ijkplayer/ijkavformat/ijkioandroidio.c',
      'ijkmedia/ijkplayer/android/**/*.*',
      'ijkmedia/ijksdl/android/**/*.*',
      'ijkmedia/ijksdl/ffmpeg/ijksdl_vout_overlay_ffmpeg.{h,c}'
      # need exclude when IJK_IO_OFF is 1.
      #'ijkmedia/ijkplayer/ijkavformat/*.*'
    ss.osx.exclude_files = 'ijkmedia/ijksdl/ijksdl_egl.*', 'ijkmedia/ijksdl/ios/*.*'
    ss.ios.exclude_files = 'ijkmedia/ijksdl/mac/*.*'

  end

  s.library = 'z', 'iconv', 'xml2', 'bz2', 'c++', 'lzma'
  s.osx.vendored_libraries = 'shell/build/product/macos/universal/**/*.a'
  s.ios.vendored_libraries = 'shell/build/product/ios/universal/**/*.a'
  s.frameworks = 'AVFoundation', 'AudioToolbox', 'CoreMedia', 'CoreVideo', 'VideoToolbox'
  s.osx.frameworks = 'Cocoa', 'AudioUnit', 'OpenGL', 'GLKit'
  s.ios.frameworks = 'UIKit', 'OpenGLES'
  
end
