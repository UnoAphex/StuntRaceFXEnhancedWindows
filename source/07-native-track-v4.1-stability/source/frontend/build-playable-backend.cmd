@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b 1
if not exist playable-backend mkdir playable-backend
cl /nologo /EHsc /O2 /MT stuntrace-main\src\compat\srf_frontend.cpp ^
  /I stuntrace-main\src\compat ^
  /I stuntrace-main\build-ninja\vcpkg_installed\x64-windows\include\SDL2 ^
  /Fe:playable-backend\srf_compat.exe ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup ^
  stuntrace-main\build-ninja\vcpkg_installed\x64-windows\lib\SDL2.lib ^
  shell32.lib user32.lib
exit /b %errorlevel%
