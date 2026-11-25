@echo off
setlocal
set "STAR_RENDERER=OpenGL"
set "SDL_OPENGL_LIBRARY=%SystemRoot%\System32\opengl32.dll"
"%~dp0StarSuccessor.exe"
