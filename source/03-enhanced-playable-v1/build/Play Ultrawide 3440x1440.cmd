@echo off
setlocal
set "SRF_FILTER=edge"
set "SRF_SHOW_FPS=1"
set "SRF_CAP=60"
set "SRF_WINDOW=3440x1440"
set "SRF_FULLSCREEN=1"
set "SRF_STRETCH=1"
start "" "%~dp0StuntRaceFX.exe"

