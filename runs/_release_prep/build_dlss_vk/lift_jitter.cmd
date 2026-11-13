@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cd /d "C:\Games\Hookshot - NotRexed - Test e400f88" || exit /b 1
set OOT_DLSS_JITTER=1
python nx2\lift.py --standalone --write --exefs "%LOCALAPPDATA%\HookshotBuilder\exefs" --work "C:\Games\Hookshot - NotRexed - Test e400f88\work"
exit /b %ERRORLEVEL%
