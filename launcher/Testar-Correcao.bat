@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Testar-Windows.ps1" -Renderer OpenGL -Probe
pause
