@echo off
rem Starts MasterSword with its own thread placement turned off, so only cpulab.ini decides. For this session only.
cd /d "%~dp0"
set HOS_P_CORES=0
set HOS_GAME_P_CORES=0
set RX_VK_PAR_ANY_CORE=1
set RX_VK_JOBS_ANY_CORE=1
set RX_VK_REPLAY_PIN=0
if exist "..\MasterSwordLauncher.exe" (start "" "..\MasterSwordLauncher.exe") else (start "" "..\mastersword.exe")
"%~dp0cpulab.exe" watch cpulab.ini
pause