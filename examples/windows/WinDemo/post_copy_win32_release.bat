@echo off

copy ..\depend\ffmpeg\lib\avcodec-57.dll	..\Release\avcodec-57.dll

copy ..\depend\ffmpeg\lib\avfilter-6.dll	..\Release\avfilter-6.dll

copy ..\depend\ffmpeg\lib\avformat-57.dll	..\Release\avformat-57.dll

copy ..\depend\ffmpeg\lib\avutil-55.dll	..\Release\avutil-55.dll

copy ..\depend\ffmpeg\lib\swresample-2.dll	..\Release\swresample-2.dll

copy ..\depend\ffmpeg\lib\swscale-4.dll	..\Release\swscale-4.dll

copy ..\depend\sdl2\lib\SDL2.dll	..\Release\SDL2.dll

copy ..\depend\pthread-win32\lib\pthreadVC2.dll ..\Release\pthreadVC2.dll