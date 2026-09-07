@echo off
setlocal
pushd "%~dp0"
set "SRF_FILTER=edge"
set "SRF_SHOW_FPS=1"
set "SRF_GSU_CLOCK=300"
set "SRF_MATERIALS=1"
set "SRF_DRAW_DISTANCE=far"
set "SRF_MOTION=1"
set "SRF_CAP=60"
set "SRF_WINDOW=3440x1440"
set "SRF_FULLSCREEN=1"
set "SRF_AMBIENT=1"
start "" "%~dp0StuntRaceFX.exe"
