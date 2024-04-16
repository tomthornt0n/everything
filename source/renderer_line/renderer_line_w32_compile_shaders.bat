
rem expects to be called from the build directory

set shader_source=..\source\renderer_line\renderer_line_w32_shader.hlsl

fxc.exe /nologo /T vs_5_0 /E vs /O3 /WX /Zpc /Ges /Fh w32_vshader_line.h /Vn w32_vshader_line /Qstrip_reflect /Qstrip_debug /Qstrip_priv %shader_source%
fxc.exe /nologo /T ps_5_0 /E ps /O3 /WX /Zpc /Ges /Fh w32_pshader_line.h /Vn w32_pshader_line /Qstrip_reflect /Qstrip_debug /Qstrip_priv %shader_source%

fxc.exe /nologo /T vs_5_0 /E vs /Od /Zi /WX /Zpc /Ges /Fh w32_vshader_line_debug.h /Vn w32_vshader_line %shader_source%
fxc.exe /nologo /T ps_5_0 /E ps /Od /Zi /WX /Zpc /Ges /Fh w32_pshader_line_debug.h /Vn w32_pshader_line %shader_source%
