@echo off
setlocal
set "ROM=%~dp0Stunt Race FX (USA) (Rev 1).sfc"
if not exist "%ROM%" (
  echo Copy your US Rev 1 ROM into this folder and name it:
  echo Stunt Race FX ^(USA^) ^(Rev 1^).sfc
  pause
  exit /b 1
)
set "SRF_ASPECT=4:3"
set "SRF_FILTER=best"
set "SRF_VSYNC=1"
"%~dp0srf_launcher.exe" "%ROM%"
endlocal
