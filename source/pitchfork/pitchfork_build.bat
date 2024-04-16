@echo off

set app_name=pitchfork

set build_dir=..\..\build

if not exist %build_dir% mkdir %build_dir%
pushd %build_dir%

if not exist fonts.obj goto build_fonts
:ret_build_fonts

echo ~~~~~~~~~~~ debug build ~~~~~~~~~~~
set compile_options= /DUNICODE /D_UNICODE /DBuildModeDebug=1 /DBuildEnableAsserts=1 /DBuildFontProviderSTBTruetype
set compile_flags= -nologo /Zi /FC /Od /I..\source\ /utf-8 /I"%VSInstallDir%DIA SDK\include"
set link_flags= /opt:ref /incremental:no /subsystem:windows /DEBUG:FULL
cl.exe %compile_flags% %compile_options% /DApplicationName="%app_name%" ..\source\pitchfork\pitchfork_main.c fonts.obj /link %link_flags%  /out:"%app_name%_debug.exe"

echo ~~~~~~~~~~ release build ~~~~~~~~~~
set compile_options= /DUNICODE /D_UNICODE /DBuildModeRelease=1 /DBuildFontProviderSTBTruetype
set compile_flags= -nologo /Zi /FC /O2 /I..\source\ /utf-8 /I"%VSInstallDir%DIA SDK\include"
set link_flags= /opt:ref /incremental:no /subsystem:windows /DEBUG:FULL
cl.exe %compile_flags% %compile_options% /DApplicationName="%app_name%" ..\source\pitchfork\pitchfork_main.c fonts.obj /link %link_flags%  /out:"%app_name%.exe"

popd

exit /b

:build_fonts
cl /nologo /FC /c ..\source\external\fonts.c
goto ret_build_fonts
