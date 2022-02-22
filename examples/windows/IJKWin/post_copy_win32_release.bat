@echo off

copy ..\Release\ijkwin.lib	..\ijkpackage\win32\ijkwin.lib

copy ..\Release\ijkwin.pdb	..\ijkpackage\win32\ijkwin.pdb

copy .\ijkplayer\ijk_frame.h	..\ijkpackage\win32\ijk_frame.h

copy .\ijkplayer\ijk_ffplay_decoder.h	..\ijkpackage\win32\ijk_ffplay_decoder.h

copy .\ijkplayer\ijk_metadata.h	..\ijkpackage\win32\ijk_metadata.h

