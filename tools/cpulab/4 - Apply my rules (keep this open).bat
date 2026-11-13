@echo off
cd /d "%~dp0"
echo Applying cpulab.ini to the game while this window is open. Ctrl+C or close the window to stop.
echo (What it set ends when the game closes.)
"%~dp0cpulab.exe" watch cpulab.ini
pause