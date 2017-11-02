# cedi (my text/code editor)
 cedi - stands for "Code-EDItor" or "C-EDItor" (C the language)<br>
 c++11 and opengl (glfw/glad/opengl portable graphics, stb_truetype text rendering)<br>
 
 most likely windows only for now (could end up being portable, without me noticing)<br>
 
### how to build
 c++11 (at the very least i use auto everywhere)<br>
 gcc/msvc/llvm should work, but usually not tested, i mainly build with msvc (cl.exe via command like)
 project is compiled as one translation unit, no complicated linking or make system needed<br>
 build with build.bat<br> (works on my machine :) )<br>
  "build.bat vs"   for msvc compiler (cl.exe) (needs to be in path env var)<br>
  "build.bat gcc"  for gcc compiler (needs gcc bin path in env var called "GCC")<br>
 
### deps (things needed to build this project but not included in this repo, never included are compiler/debugging enviroment)
 deps/stb/stb_rect_pack.h<br>
 deps/stb/stb_truetype.h<br>
 
 deps/glfw-3.2.1.bin.WIN64/lib-vc2015/glfw3dll.lib<br>
 