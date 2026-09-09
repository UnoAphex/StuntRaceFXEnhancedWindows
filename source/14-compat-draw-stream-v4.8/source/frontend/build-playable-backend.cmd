@echo off
setlocal
if not defined SDL2_ROOT (
  echo Set SDL2_ROOT to a vcpkg-style x64-windows SDL2 installation.
  echo Expected: %%SDL2_ROOT%%\include\SDL2\SDL.h and %%SDL2_ROOT%%\lib\SDL2.lib
  exit /b 2
)
if not exist "%SDL2_ROOT%\include\SDL2\SDL.h" exit /b 2
if not exist "%SDL2_ROOT%\lib\SDL2.lib" exit /b 2
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b 1
cl /nologo /EHsc /O2 /W4 /MT "%~dp0srf_frontend.cpp" ^
  /I "%~dp0." ^
  /I "%SDL2_ROOT%\include\SDL2" ^
  /Fo:"%TEMP%\srf_frontend_v48.obj" ^
  /Fe:"%~dp0..\..\build\StuntRaceFX.exe" ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup ^
  "%SDL2_ROOT%\lib\SDL2.lib" shell32.lib user32.lib winmm.lib
exit /b %errorlevel%
