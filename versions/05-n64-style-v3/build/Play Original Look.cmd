@echo off
setlocal
pushd "%~dp0"
set "SRF_FILTER=nearest"
set "SRF_GSU_CLOCK=100"
set "SRF_MATERIALS=0"
set "SRF_DRAW_DISTANCE=original"
set "SRF_MOTION=0"
set "SRF_CAP=60"
start "" "%~dp0StuntRaceFX.exe"
