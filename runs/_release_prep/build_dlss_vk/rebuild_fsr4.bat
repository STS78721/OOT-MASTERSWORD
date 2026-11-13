@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
set RX_VK_ONLY=
set "RX_CPM=C:\Games\Hookshot - NotRexed - Test e400f88\work\deps\cpm"
if not exist "%RX_CPM%" set "RX_CPM=C:\Users\debee\.codex\worktrees\oot-main-candidate\OOT EMU\.cache\cpm"
set RX_RELEASE=1
set "HOOKSHOT_NGX_SDK=C:\Games\Hookshot - NotRexed - Test e400f88\work\ngx-sdk"
set HOOKSHOT_FSR=1
cd /d "C:\Games\Hookshot - NotRexed - Test e400f88"
python -u -X utf8 nx2\build.py --standalone --jobs 8
exit /b %ERRORLEVEL%
