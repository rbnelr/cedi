# cedi (my text/code editor)
 cedi - stands for "Code-EDItor" or "C-EDItor" (C the language)<br>
 
## controls
 <table>
	<tr><td>keys</td>				<td>default</td>	<td>function</td></tr>
	<tr><td>arrow keys</td>			<td></td>			<td>text cursor control</td></tr>
	<tr><td>ALT+N</td>				<td>off</td>		<td>toggle newline and tab character visualisation</td></tr>
	<tr><td>ALT+T+<inc/dec></td>	<td>4 spaces</td>	<td>change tab spaces count</td></tr>
 </table>
 
### technical specs
 c++11 and opengl (glfw/glad/opengl, stb_truetype text rendering)<br>
 
 most likely windows only for now (could end up being portable, without me noticing)<br>
 
### how to build
 c++11 (at the very least I use auto everywhere)<br>
 gcc/msvc/llvm should work, but usually not tested, i mainly build with msvc (cl.exe via command line)
 project is compiled as one translation unit, no complicated linking or make system needed<br>
 build with build.bat (works on my machine :) )<br>
  "build.bat vs"   for command line msvc compiler (cl.exe dir needs to be in path)<br>
  "build.bat gcc"  for gcc compiler (gcc.exe dir needs to be in env var called "GCC")<br>
 
### deps (things needed to build this project but not included in this repo, never included are compiler/debugging enviroment)
 deps/glfw-3.2.1.bin.WIN64/lib-vc2015/glfw3dll.lib<br>
 