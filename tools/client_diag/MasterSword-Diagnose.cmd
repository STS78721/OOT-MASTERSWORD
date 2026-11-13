@echo off
setlocal
cd /d "%~dp0"
title MasterSword Client Diagnostic
echo.
echo Starting MasterSword diagnostic...
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0MasterSword-Diagnose.ps1"
set ERR=%ERRORLEVEL%
if %ERR% NEQ 0 (
  echo.
  echo Diagnostic script exited with code %ERR%.
  pause
)
endlocal
