@echo off

copy ..\depend\bin\win32\libwinpthread-1.dll ..\Release\libwinpthread-1.dll

copy ..\depend\bin\libgcc_s_dw2-1.dll ..\Release\libgcc_s_dw2-1.dll

copy ..\depend\EGL\lib\Release\libEGL.dll ..\Release\libEGL.dll

copy ..\depend\EGL\lib\Release\libGLESv2.dll ..\Release\libGLESv2.dll

copy ..\depend\ffmpeg\lib\avcodec-58.dll	..\Release\avcodec-58.dll

copy ..\depend\ffmpeg\lib\avformat-58.dll	..\Release\avformat-58.dll

copy ..\depend\ffmpeg\lib\avutil-56.dll	..\Release\avutil-56.dll

copy ..\depend\ffmpeg\lib\swresample-3.dll	..\Release\swresample-3.dll

copy ..\depend\ffmpeg\lib\swscale-5.dll	..\Release\swscale-5.dll

copy ..\depend\sdl2\lib\SDL2.dll	..\Release\SDL2.dll

copy ..\depend\pthread-win32\lib\pthreadVC2.dll ..\Release\pthreadVC2.dll