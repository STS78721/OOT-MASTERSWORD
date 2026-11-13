@echo off
rem Starts MasterSword (its own P-core placement on) and applies cpulab.ini on top while this window is open.
cd /d "%~dp0"
if exist "..\MasterSwordLauncher.exe" (start "" "..\MasterSwordLauncher.exe") else (start "" "..\mastersword.exe")
"%~dp0cpulab.exe" watch cpulab.ini
pause