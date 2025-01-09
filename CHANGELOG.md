tag k0.12.0
--------------------------------
- upgrade all libs,FFmpeg use n6.1.2
- not link c++
- fix copy_hw_frame memory leak
- fix ass log memory leak
- support dash demuxer,#31

tag k0.11.9
--------------------------------

- all libs remove bitcode support
- external subtitle read support interrupt
- ic meta Use prefix matching rules,#53
- support parse av3a stream, but can't decode it,#45
- subtitle avformat support options and set ijkapplication,user_agent,http_proxy by default
- improve the problem of lagging when switching external subtitles
- fix smb2 url not allow longer than 1024 characters question
- fix external subtitle aftert seek shown have a delay bug
- fix subtitle FF_ASS_Renderer memory leak

tag k0.11.8
--------------------------------

- adapt to Xcode16(clang 16)
- add builtin smb2 protocol
- move “application.h” to “libavformat” directory
- support Network or Local Blu-ray Disc/BDMV
- http add auth_type2 option, specify http auth type, reduce the number of requests
- fix http open and http_seek (redirect) authentication bug,#52
- fix some hls stream can't seek back to 00:00 bug

tag k0.11.7
--------------------------------

- support avs3 video decoder,#45
- update playable statistics after seek,#46
- fix the palyable progress bar is never full bug
- fix ffp_get_current_position_l is on average 50ms slow after pause
- add the duration of the decoded frames to statistics

tag k0.11.6
--------------------------------

- fix real iOS device does not display picture and keeps showing a pink color,#42

tag k0.11.5
--------------------------------

- upgrade FFmpeg to n6.1.1
- The subtitle delay API is incompatible, the value is need reverse: the positive value means subtitle put off, the negative value means subtitle advanced
- fix crash: target thread exited while waiting for the perform
- support specify fonts dir for subtitle,#37
- fix ass subtitles at special positions not display bug,#39

tag k0.11.4
--------------------------------

- fix ICY meta not update bug,#28
- fix HDR animation shown bug
- fix Metal Renderer crash
- modify BT.601 matrix same as Apple demo
- improve stop runloop logic for IJKSDLThread

tag k0.11.3
--------------------------------

- parse LYRICS meta,#22
- parse ICY Header and update ICY meta
- use git submodule manage compile shell
- fix http chunked transfer get wrong size cause av_read_frame can not return eof bug
- increase ass frame cache amount

tag k0.11.2
--------------------------------

- support tvOS platform,#20 
- ass subtitle support force style,#17
- support set audio delay
- fix subtitle memory leak
- improve ass frame cache logic,#19
- palettized rgb bitmap subtitle use shader blend
- audio meta add human describe string
- fix blu-ray iso after seek video frame is not keyframe bug

tag k0.11.1
--------------------------------

- hud show subtitle frame cache remaining
- fix some scenarios ass subtitle not show bug
- fix external subtitle present at wrong position which start_time is not zero
- fix after seek, subtitle maybe disappear quickly bug

tag k0.11.0
--------------------------------

- support ass subtitle effects, and adjust position and scale in real time
- support display multiple pgs bitmap subtitle at the same moment, and adjust position and scale in real time
- dropped old subtitle renderer (text->image(Core Graphics)->CVPixelBuffer->Texture)
- subtitle preference move to player from view
- dropped iOS OpenGL renderer
- support http gzip and deflate use headers
- restore ijk dns cache and http event hook
- enable microdvd subtitle decoder
- meta add chapter info,#12

tag k0.10.5
--------------------------------

- support iso bluray and dvd disk

tag k0.10.4
--------------------------------

- support 8bit falsify hdr
- support install third pre-compiled libs
- external subtitle support GBK、BIG5-2003 character set
- fix subtitle display more bigger bug on non-retina screen using metal
- support render P216、YUV422P16、P416、YUV444P16、AYUV64、YUVA444P16 pixel format directly
- upgrade ffmpeg to 5.1.4，openssl to 1.1.1w，opus to 1.4，dav1d to 1.3.0，bluray to 1.3.4

tag k0.10.3
--------------------------------

- fix video stream accurate seek timeout bug
- dropping frames need keep a safe distance
- fix add external subtitle occur crash with certain scenes
- fix display previous stream frames after exchange subtitle stream

tag k0.10.2
--------------------------------

- improve subtitle rendering performance markedly
- support cocoapods install pre-build framework

tag k0.10.1
--------------------------------

- restore ijk custom protocols
- support idx+sub subtitle
- fix display old subtitle in a flash bug
- fix two crashes

tag k0.10.0
--------------------------------

- upgrade ffmpeg5.1.3
- opengl and metal support hw_10bit SDR/HDR video
- metal support XRGB pixel format
- enable av1 use dav1d decoder
- optimize seek speed and accuracy
- convert to new channel layout-API
- switch to new FIFO API
- sync lastest ffplay.c 
- adapt ass parser

tag k0.9.7
--------------------------------

- expose internal render view
- support multi render view
- fix shutdown player crash,some video may cause
- when pts changed abruptly throw FFP_MSG_WARNING

tag k0.9.6
--------------------------------

- fix accurate seek waiting time out bug
- support step play mode
- improve audio-video sync logic, especially drop audio frame
- metal view snapshot support origin,screen,effect origin type

tag k0.9.5
--------------------------------

- support apple metal display video picture
- add metalRenderer option,default is auto; you can set to NO means force use opengl
- enable libavdevice & libavfilter
- cut down 3% cpu usage
- fix after finish seek, video picture display slowly a few seconds bug
- fix after seek near the end of the video, not play problem

tag k0.9.4
--------------------------------

- use **videotoolbox_hwaccel** instead of videotoolbox option
- add protocol_whitelist: rtmp,rtsp,rtp,srtp,udp
- macos 10.14 later use exclusive thread for glview

tag k0.9.3
--------------------------------

- use global single thread display fix SIGSEGV crash
- support **enable-cvpixelbufferpool** option disable cvpixelbufferpool
- enable indeo5 decoder
- support Xcode 14.1
- pass cocoapod lib lint

tag k0.9.2
--------------------------------

- support morden picture format: jpg,jpeg,png,bmp,webp,pcx,tif,psd
- enable decoders (bmp,tiff,psd,webp,targa,pcx)
- extract functions from ff_play.c to some category files
- auto select overlay format yuvj420p not convert to nv12
- restructure subtitle logic,when change delay can display soon
- support set preventDisplay for snapshop feature (macOS only)
- not depends libyuv

tag k0.9.1
--------------------------------

- support morden audio format: aac,ac3,amr,wma,mp2,mp3,m4a,m4r,caf,ogg,oga,opus
- support lossless audio format: dsf,flac,wav,ape,dff,dts
- support bluray protocol
- enable decoders (movtext, dvbsub, qtrle, mss2, rawvideo, tscc2)
- improve memory copy performance
- clean ijk videotoolbox hw decoder pipeline
- fix some crash,eg: GBK encoding meta

tag k0.9.0.5
--------------------------------

- support cocoapods

- ffmpeg: use github tag 4.0 source and add pathes
  
  - disable all muxer
  - enable some demuxer for video 
  - enable all audio decoder
  - enable some video decoder
  - enable videotoolbox hwaccel for ios and macos

- macos: begin support macOS platform
  
  - subtitle
    - support add external subtitles
    - auto keep aspect ratio to video picture
    - support delay
    - text subtitle,eg: ass,srt,ssa,webvtt
      - change display size,position,color in real time 
    - graphic subtitle,eg: pgssub,dvdsub
      - change display size,position in real time
  - rotation: rotate video picture along the [x,y,z]-axis
  - snapshot: support capture current window or use origin picture size capture subtitle overlay
  - adjust brightness,saturation,constast
  - adjust video scale
  - use opengl 3.3
  - support arm64 (Apple Silicon M1)

- ios/macos:
  
  - use universal renderer logic,support bgrx,xrgb,uyvy422,yuv420p,yuv420sp
  - auto use videotoolbox hwaccel by default
  - ffmpeg soft decoder also use same renderer logic as hwaccel
  - auto adjust video rotate

- openssl: upgrade to 1.1.1m

- shell: use universal init/compile shell. see [shell/README.md](shell/README.md)

- opus: depending opus v1.3.1

- libyuv: depending latest libyuv

tag k0.8.8
--------------------------------

- ffmpeg: upgrade to 3.4
- ffmpeg: fix hls some issue
- android: fix seek bug when no audio
- openssl: upgrade to 1.0.2n
- ios: vtb support h265

tag k0.8.7
--------------------------------

tag k0.8.6
--------------------------------

- ijkplayer: fix opengl config error
- ffmpeg: fix a concat issue 

tag k0.8.5
--------------------------------

- ijkplayer: fix opengl config error
- ijkplayer: fix some bug about audio

tag k0.8.4
--------------------------------

- ffmpeg: enable hevc by default
- ijkio: support cache share
- ijkplayer: fix some bug

tag k0.8.3
--------------------------------

- ffmpeg: dns cache refactor
- ijkio: cache support synchronize read avoid frequent lseek
- ijkplayer: fix some bug

tag k0.8.2
--------------------------------

- ffmpeg: fix some bug
- ijkio: update and modify features
- ijkplayer: support don't calculate real frame rate, the first frame will speed up

tag k0.8.1
--------------------------------

- ffmpeg: support dns cache
- ijkio: support inject extra node

tag k0.8.0
--------------------------------

- ffmpeg: upgrade to 3.3
- ffmpeg: enable flac
- android: support sync mediacodec
- android: support framedrop when use mediacodec
- openssl: upgrade to 1.0.2k
- jni4android: upgrade to v0.0.2

tag k0.7.9
--------------------------------

- ffmpeg: add tcp timeout control
- android: support soundtouch

tag k0.7.8
--------------------------------

- ffplay: support accurate seek
- ijkio: fix some issue
- ios: add ijkplayer dynamic target with ssl

tag k0.7.7
--------------------------------

- ffmpeg: enable ijkio protocol
- ffmpeg: avoid some unreasonable pts
- ios: fix a crash caused by videotoolbox sync initialization fail

tag k0.7.6
--------------------------------

- ffmpeg: ass subtitle support
- msg_queue: add resource for msg_queue
- ios: separate vtb sync mode from mixed vtb
- android: fix some thread competition
- android: support setSpeed for pre-M(api<23) versions

tag k0.7.5
--------------------------------

- ffmpeg: disable-asm on architecture x86
- ffmpeg: revert some cutted demuxer and decoder
- ios: add playback volume interface

tag k0.7.4
--------------------------------

- ffplay: fix sample buffer leak introduced in k0.7.1
- doc: add takeoff checklist

tag k0.7.3
--------------------------------

- ios: turn videotoolbox into singleton
- ffmpeg: merge ipv6 issue in tcp.c

tag k0.7.2
-------------------------------

- ios: fix a compile error

tag k0.7.1
-------------------------------

- ffmpeg: upgrade to n3.2

tag k0.6.3
--------------------------------

- ffmpeg: disable clock_gettime added in xcode8
- android: make NDKr13 happy

tag k0.6.2
--------------------------------

- ffmpeg: fix wild pointer when decoder was not found
- player: fix bug introduced in k0.6.0

tag k0.6.1
--------------------------------

- concat: fix crash introduced in k0.6.0
- flvdec: fix seek problem introduced in k0.6.0
- hls: fix regression with ranged media segments

tag k0.6.0
--------------------------------

- openssl: upgrade to 1.0.2h
- ffmpeg: upgrade to n3.1
- MediaCodec: add options to enable resolution change.
- VideoToolbox: add options to enable resolution change.

tag k0.5.1
--------------------------------

- ffmpeg: fix crash introduced in k0.5.0

tag k0.5.0
--------------------------------

- ffmpeg: upgrade to n3.0
- android: support NDKr11

tag k0.4.5
--------------------------------

- ios: support playbackRate change. (iOS 7.0 or later)
- android: support speed change. (Android 6.0 or later)
- player: do not link avfilter by default.
- android: add x86_64 support
- android: move jjk out to jni4android project
- android: support OpenGL ES2 render

tag k0.4.4
--------------------------------

- ios: replace MPMoviePlayerXXX with IJKMPMoviePlayerXXX
- ios: remove target 'IjkMediaPlayer'. 'IjkMediaFramework' should be used instead.
- android: switch ExoPlayer to r1.5.2

tag k0.4.3
--------------------------------

- android: fix several crash when reconfiguring MediaCodec
- android: add jjk to generate API native wrapper
- android: support IMediaDataSource for user to supply media data

tag k0.4.2
--------------------------------

- ios: support Xcode 7
- ios: drop support of iOS 5.x
- ffmpeg: enable libavfilter
- player: limited support of libavfilter
- android: add ExoPlayer as an alternative backend player

tag k0.4.1
--------------------------------

- android: support downloading from jcenter

tag k0.4.0
--------------------------------

- ffmpeg: switch to ffmpeg n2.8

tag k0.3.3
--------------------------------

- player: custom protocol as io hook
- android/sample: support rotation meta (TextureView only)

tag k0.3.2
--------------------------------

- android: drop support of Eclipse
- android: update to SDK 23
- android/sample: better UI
- ios: support SAR
- android/sample: support background play

tag k0.3.1
--------------------------------

- player: key-value options API
- player: remove ijkutil
- build: support cygwin
- ios: optimize performance of VideoToolbox.

tag k0.3.0
--------------------------------

- android: support build with Android Studio / Gradle
- build: improve library fetch
- openssl: switch to openssl 1.0.1o

tag k0.2.4
--------------------------------

- ios: remove armv7s build from default
- player: introduce key-value options
- ios: demo improvement
- ios: support init/play in background.
- ffmpeg: switch to ffmpeg n2.7

tag k0.2.3
--------------------------------

- android: support OpenSL ES
- ios: support NV12 Render
- ios: support VideoToolBox
- ffmpeg: switch to ffmpeg n2.6

tag n0.2.2:
--------------------------------

- ffmpeg: switch to ffmpeg n2.5
- android: fix leak in jni
- player: retrieve media informations

tag n0.2.1:
--------------------------------

- android: support MediaCodec (API 16+)

tag n0.2.0
--------------------------------

- player: fix crash on invalid audio
- android: support build with ndk-r10
- ios: add IJKAVMoviePlayerController based on AVPlayer API
- ios: remove some unused interface
- ios8: fix latency of aout_pause_audio()
- ios8: upgrade project
- ffmpeg: switch to ffmpeg n2.4

tag n0.1.3
--------------------------------

- ffmpeg: switch to ffmpeg n2.2
- player: fix complete/error state handle
- ffmpeg: build with x86_64, armv5
- android: replace vlc-chroma-asm with libyuv

tag n0.1.2:
--------------------------------

- ffmpeg: build with openssl
- player: fix aout leak
- player: reduce memory footprint for I420/YV12 overlay
- ios: snapshot last displayed image

tag n0.1.1:
--------------------------------

- player: remove ugly frame drop trick
- ios: simplify application state handle
- ios: fix 5.1 channel support
- player: handle ffmpeg error
- player: fix leak
- player: improve buffer indicator
- player: drop frame for high fps video

tag n0.1.0:
--------------------------------

- android: replace AbstractMediaPlayer with IMediaPlayer and other misc interfaces
- android: remove list player classes due to lack of regression test
- ios: support build with SDK7
- ffmpeg: switch to n2.1 base
- ios: fix possible block on ijkmp_pause
- ios: set CAEAGLLayer.contentsScale to avoid bad image on retina devices
- ios: fix handle of AudioSession interruption
- ios: add AudioQueue api as replacement of AudioUnit api
- ijksdl: fix non-I420 pixel-format support
- player: improve late packet/frame dropping
- player: prefer h264 stream if multiple video stream exists

tag n0.0.6:
--------------------------------

- android: fix NativeWindow leak
- ios: fix a deadlock related to AudioUnit
- ios: support ffmpeg concat playback
- ios: add ffmpeg options methods
- android: limait audio sample-rate to 4kHz~48kHz
- ios: fix gles texture alignment

tag n0.0.5:
--------------------------------

- build: disable -fmodulo-sched -fmodulo-sched-allow-regmoves, may crash on gcc4.7~4.8
- player: support ios
- ijksdl: support ios gles2 video output
- ijksdl: support ios AudioUnit audio output
- build: add android/ios sub directory
- player: fix some dead lock
- build: use shell scripts instead of git-submodule
- android: use RV32 as default chroma

tag n0.0.4:
--------------------------------

- ffmpeg: enable ac3
- android: target API-18
- build: switch to NDKr9 gcc4.8 toolchain

tag n0.0.3:
--------------------------------

- ffmpeg: switch to tag n2.0
- ffmpeg: remove rarely used decoders, parsers, demuxers
- avformat/hls: fix many bugs
- avformat/http: support reading compressed data
- avformat/mov: optimize short seek
- player: fix AudioTrack latency
- player: refactor play/pause/step/buffering logic
- player: fix A/V sync
- yuv2rgb: treat YUVJ420P as YUV420P
- yuv2rgb: support zero copy of YUV420P frame output to YV12 surface
- ijksdl: fix SDL_GetTickHR() returns wrong time 
