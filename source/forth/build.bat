@echo off
del *.pdb

@echo on
cl /nologo /c os.c /O2 /Zi /utf-8
nasm -g -f win64 bs.asm
link /nologo /subsystem:windows /opt:ref /incremental:no /debug bs.obj os.obj kernel32.lib user32.lib Gdi32.lib Dwmapi.lib libcmt.lib

@echo off
del *.obj

