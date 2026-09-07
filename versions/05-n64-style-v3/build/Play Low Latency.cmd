@echo off
setlocal
pushd "%~dp0"
set "SRF_FILTER=edge"
set "SRF_SHOW_FPS=1"
set "SRF_GSU_CLOCK=300"
set "SRF_MATERIALS=1"
set "SRF_DRAW_DISTANCE=extended"
set "SRF_MOTION=0"
set "SRF_RUNAHEAD=1"
set "SRF_CAP=60"
start "" "%~dp0StuntRaceFX.exe"
