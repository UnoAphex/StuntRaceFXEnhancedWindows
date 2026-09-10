@echo off
setlocal
pushd "%~dp0"
set "SRF_RENDERER=compat"
set "SRF_COMPAT_WIDE=1"
set "SRF_COMPAT_HD_CENTER=1"
set "SRF_FILTER=edge"
set "SRF_SHOW_FPS=0"
set "SRF_GSU_CLOCK=300"
set "SRF_MATERIALS=0"
set "SRF_MOTION=1"
set "SRF_CAP=60"
set "SRF_WINDOW=1920x1080"
start "" "%~dp0StuntRaceFX.exe"
