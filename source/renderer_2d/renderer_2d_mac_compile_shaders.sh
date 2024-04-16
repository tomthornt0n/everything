#!/bin/sh

# expects to be called from the build directory

xcrun -sdk macosx metal -c ../source/renderer_2d/renderer_2d_mac_shader.metal -gline-tables-only -frecord-sources -o renderer_2d_mac_shaders.air
xcrun -sdk macosx metallib renderer_2d_mac_shaders.air -o renderer_2d_mac_shaders.metallib
