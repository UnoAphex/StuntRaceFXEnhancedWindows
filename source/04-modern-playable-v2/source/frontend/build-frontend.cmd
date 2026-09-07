@echo off
setlocal
if not defined SDL2_ROOT (
  echo Set SDL2_ROOT to your SDL2 development-package folder first.
  exit /b 1
)
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b 1
cl /nologo /EHsc /O2 /MD srf_frontend.cpp ^
  /I "%SDL2_ROOT%\include" ^
  /Fe:StuntRaceFX.exe ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup ^
  /LIBPATH:"%SDL2_ROOT%\lib\x64" SDL2.lib shell32.lib user32.lib
exit /b %errorlevel%

