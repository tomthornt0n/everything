
rem expects to be called from the build directory

set shader_source=..\source\renderer_2d\renderer_2d_w32_shader.hlsl

fxc.exe /nologo /T vs_5_0 /E vs /O3 /WX /Zpc /Ges /Fh w32_vshader.h /Vn w32_vshader /Qstrip_reflect /Qstrip_debug /Qstrip_priv %shader_source%
fxc.exe /nologo /T ps_5_0 /E ps /O3 /WX /Zpc /Ges /Fh w32_pshader.h /Vn w32_pshader /Qstrip_reflect /Qstrip_debug /Qstrip_priv %shader_source%

fxc.exe /nologo /T vs_5_0 /E vs /Od /Zi /WX /Zpc /Ges /Fh w32_vshader_debug.h /Vn w32_vshader %shader_source%
fxc.exe /nologo /T ps_5_0 /E ps /Od /Zi /WX /Zpc /Ges /Fh w32_pshader_debug.h /Vn w32_pshader %shader_source%
