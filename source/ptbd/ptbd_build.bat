@echo off

set app_name=PTBDDatabase

set build_dir=..\..\build

if not exist %build_dir% mkdir %build_dir%
pushd %build_dir%

if not exist zlib.lib goto build_zlib
:ret_build_zlib

if not exist sqlite3.lib goto build_sqlite
:ret_build_sqlite

if not exist libxlsxwriter.lib goto build_xlsx_writer
:ret_build_xlsx_writer

if not exist fonts.obj goto build_fonts
:ret_build_fonts

echo ~~~~~~~ shader compilation ~~~~~~~~
call ..\source\renderer_2d\renderer_2d_w32_compile_shaders.bat
call ..\source\renderer_line\renderer_line_w32_compile_shaders.bat

echo ~~~~~~~ w32 resource compilation ~~~~~~~~
rc.exe /nologo ptbd_w32_resources.rc

echo ~~~~~~~~~~~ debug build ~~~~~~~~~~~
set compile_options= /DUNICODE /D_UNICODE /DBuildModeDebug=1 /DBuildEnableAsserts=1 /DBuildZLib /DBuildFontProviderSTBTruetype
set compile_flags= -nologo /Zi /FC /Od /I..\source\ /utf-8 /I"%VSInstallDir%DIA SDK\include" /W3
set link_flags= /incremental:no /DEBUG:FULL /subsystem:windows
cl.exe %compile_flags% %compile_options% /DApplicationName="%app_name%" ..\source\ptbd\ptbd_main.c fonts.obj zlib.lib sqlite3.lib libxlsxwriter.lib ptbd_w32_resources.res /link %link_flags%  /out:"%app_name%_debug.exe"

echo ~~~~~~~~~~ release build ~~~~~~~~~~
set compile_options= /DUNICODE /D_UNICODE /DBuildModeRelease=1 /DBuildZLib /DBuildFontProviderSTBTruetype
set compile_flags= -nologo /Zi /FC /O2 /I..\source\ /utf-8 /I"%VSInstallDir%DIA SDK\include"
set link_flags= /opt:ref /incremental:no /subsystem:windows /DEBUG:FULL
cl.exe %compile_flags% %compile_options% /DApplicationName="%app_name%" ..\source\ptbd\ptbd_main.c fonts.obj zlib.lib sqlite3.lib libxlsxwriter.lib ptbd_w32_resources.res /link %link_flags%  /out:"%app_name%.exe"

popd

exit /b

:build_zlib
set zlib_sources=..\source\external\zlib\adler32.c ..\source\external\zlib\compress.c ..\source\external\zlib\crc32.c ..\source\external\zlib\deflate.c ..\source\external\zlib\infback.c ..\source\external\zlib\inffast.c ..\source\external\zlib\inflate.c ..\source\external\zlib\inftrees.c  ..\source\external\zlib\trees.c ..\source\external\zlib\zutil.c
cl /nologo /FC /c %zlib_sources% /O2
lib /nologo /out:zlib.lib adler32.obj compress.obj crc32.obj deflate.obj infback.obj inffast.obj inflate.obj  inftrees.obj trees.obj zutil.obj
goto ret_build_zlib

:build_sqlite
set sqlite_sources=..\source\external\sqlite3.c
cl /nologo /FC /c %sqlite_sources% /O2
lib /nologo /out:sqlite3.lib sqlite3.obj
goto ret_build_sqlite

:build_xlsx_writer
set xlsx_writer_sources=..\source\external\libxlsxwriter\*.c ..\source\external\libxlsxwriter\third_party\md5\md5.c ..\source\external\libxlsxwriter\third_party\minizip\*.c ..\source\external\libxlsxwriter\third_party\tmpfileplus\tmpfileplus.c
cl /nologo /FC /c /I..\source\external\libxlsxwriter /I..\source\external\zlib %xlsx_writer_sources% /O2
lib /nologo /out:libxlsxwriter.lib app.obj chart.obj chartsheet.obj comment.obj content_types.obj core.obj custom.obj drawing.obj format.obj hash_table.obj metadata.obj packager.obj relationships.obj shared_strings.obj styles.obj table.obj theme.obj utility.obj vml.obj workbook.obj worksheet.obj xmlwriter.obj md5.obj ioapi.obj iowin32.obj mztools.obj unzip.obj zip.obj tmpfileplus.obj
goto ret_build_xlsx_writer

:build_fonts
cl /nologo /FC /c ..\source\external\fonts.c
goto ret_build_fonts
