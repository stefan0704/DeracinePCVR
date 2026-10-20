@echo off
rem Deracine (PSVR / PS4) in PC VR via OpenXR (Virtual Desktop, SteamVR, Meta Link, Pimax)
rem Settings: pc-vr\settings.txt
title Deracine PC VR
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0pc-vr\launch.ps1" %*
