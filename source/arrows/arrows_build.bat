@echo off

set app_name=arrows

set build_dir=..\..\build

if not exist %build_dir% mkdir %build_dir%
pushd %build_dir%

if not exist fonts.obj goto build_fonts
:ret_build_fonts

echo ~~~~~~~ shader compilation ~~~~~~~~
call ..\source\renderer_2d\renderer_2d_w32_compile_shaders.bat
call ..\source\renderer_line\renderer_line_w32_compile_shaders.bat

echo ~~~~~~~~~~~ debug build ~~~~~~~~~~~
set compile_options= /DUNICODE /D_UNICODE /DBuildModeDebug=1 /DBuildEnableAsserts=1 /DBuildFontProviderSTBTruetype
set compile_flags= -nologo /Zi /FC /Od /I..\source\ /utf-8 /W3
set link_flags= /opt:ref /incremental:no /subsystem:windows /DEBUG:FULL
cl.exe %compile_flags% %compile_options% /DApplicationName="%app_name%" ..\source\arrows\arrows_main.c fonts.obj /link %link_flags%  /out:"%app_name%_debug.exe"

echo ~~~~~~~~~~ release build ~~~~~~~~~~
set compile_options= /DUNICODE /D_UNICODE /DBuildModeRelease=1 /DBuildFontProviderSTBTruetype
set compile_flags= -nologo /Zi /FC /O2 /I..\source\ /utf-8
set link_flags= /opt:ref /incremental:no /subsystem:windows /DEBUG:FULL
cl.exe %compile_flags% %compile_options% /DApplicationName="%app_name%" ..\source\arrows\arrows_main.c fonts.obj /link %link_flags%  /out:"%app_name%.exe"

popd

exit /b

:build_fonts
cl /nologo /FC /c ..\source\external\fonts.c
goto ret_build_fonts
