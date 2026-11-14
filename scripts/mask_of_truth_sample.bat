@echo off
rem MasterSword Mask of Truth: one timed sample of what every game thread is doing (scripts/mask_of_truth.py reads it).
rem Put this beside mastersword.exe, with HOS_SAMPLE_ALL=mask_of_truth_samples.txt in hookshot_env.txt, and run it while
rem the game is in the place to measure: it waits 10 seconds (time to click back into the game), samples for 30, and stops.
cd /d "%~dp0"
if exist mask_of_truth_samples.txt.go del mask_of_truth_samples.txt.go
echo.
echo  MasterSword Mask of Truth
echo  Click back into the game now. Sampling starts in 10 seconds and runs for 30.
echo  Keep playing normally in the area you want measured.
timeout /t 10 /nobreak >nul
type nul > mask_of_truth_samples.txt.go
echo  Sampling...
timeout /t 30 /nobreak >nul
del mask_of_truth_samples.txt.go
echo  Done. This window closes itself.
timeout /t 3 /nobreak >nul
