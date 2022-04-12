@echo off

copy ..\depend\bin\x64\libwinpthread-1.dll ..\x64\Release\libwinpthread-1.dll

copy ..\depend\bin\libgcc_s_dw2-1.dll ..\x64\Release\libgcc_s_dw2-1.dll

copy ..\depend\EGL\x64\Release\libEGL.dll ..\x64\Release\libEGL.dll

copy ..\depend\EGL\x64\Release\libGLESv2.dll ..\x64\Release\libGLESv2.dll

copy ..\depend\ffmpeg\x64\avcodec-58.dll	..\x64\Release\avcodec-58.dll

copy ..\depend\ffmpeg\x64\avformat-58.dll	..\x64\Release\avformat-58.dll

copy ..\depend\ffmpeg\x64\avutil-56.dll	..\x64\Release\avutil-56.dll

copy ..\depend\ffmpeg\x64\swresample-3.dll	..\x64\Release\swresample-3.dll

copy ..\depend\ffmpeg\x64\swscale-5.dll	..\x64\Release\swscale-5.dll

copy ..\depend\sdl2\x64\SDL2.dll	..\x64\Release\SDL2.dll

copy ..\depend\pthread-win32\x64\pthreadVC2.dll ..\x64\Release\pthreadVC2.dll