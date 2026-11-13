@echo off
cd /d "%~dp0"
echo Preview only: shows what cpulab.ini would do. Start the game first. Ctrl+C to stop.
"%~dp0cpulab.exe" watch cpulab.ini --dry
pause