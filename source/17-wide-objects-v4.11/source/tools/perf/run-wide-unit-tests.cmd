@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /Od "%~dp0test_wide_background.cpp" /Fo:"%TEMP%\srf-wide-background-test.obj" /Fe:"%TEMP%\srf-wide-background-test.exe"
if errorlevel 1 exit /b 1
"%TEMP%\srf-wide-background-test.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /Od "%~dp0test_wide_sprites.cpp" /Fo:"%TEMP%\srf-wide-sprites-test.obj" /Fe:"%TEMP%\srf-wide-sprites-test.exe"
if errorlevel 1 exit /b 1
"%TEMP%\srf-wide-sprites-test.exe"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /Od "%~dp0test_wide_capture.cpp" /Fo:"%TEMP%\srf-wide-capture-test.obj" /Fe:"%TEMP%\srf-wide-capture-test.exe"
if errorlevel 1 exit /b 1
"%TEMP%\srf-wide-capture-test.exe"
exit /b %errorlevel%
