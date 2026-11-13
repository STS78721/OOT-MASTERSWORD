@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cd /d "C:\Games\Hookshot - NotRexed - Test e400f88" || exit /b 1
set RX_CPM=C:\Games\Hookshot - NotRexed - Test e400f88\work\deps\cpm
set RX_VK_ONLY=1
set RX_RELEASE=1
python -I nx2\build.py --standalone --jobs 8
exit /b %ERRORLEVEL%
