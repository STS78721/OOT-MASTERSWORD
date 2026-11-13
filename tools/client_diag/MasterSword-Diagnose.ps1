#Requires -Version 5.1
<#
  MasterSword client diagnostic runner.
  Place this folder beside mastersword.exe, double-click MasterSword-Diagnose.cmd.
#>
$ErrorActionPreference = "Continue"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Find-GameRoot {
    foreach ($cand in @($ScriptDir, (Split-Path $ScriptDir -Parent), (Get-Location).Path)) {
        if ((Test-Path (Join-Path $cand "mastersword.exe")) -or (Test-Path (Join-Path $cand "hookshot.exe"))) {
            return (Resolve-Path $cand).Path
        }
    }
    return $null
}

$GameRoot = Find-GameRoot
if (-not $GameRoot) {
    Write-Host ""
    Write-Host "ERROR: mastersword.exe not found."
    Write-Host "Copy the whole client_diag folder into your MasterSword game folder"
    Write-Host "(next to mastersword.exe), then run MasterSword-Diagnose.cmd again."
    Write-Host ""
    Read-Host "Press Enter to close"
    exit 1
}

$Exe = Join-Path $GameRoot "mastersword.exe"
if (-not (Test-Path $Exe)) { $Exe = Join-Path $GameRoot "hookshot.exe" }

$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$OutDir = Join-Path $env:USERPROFILE "Desktop\MasterSword-Diag-$Stamp"
$ZipPath = "$OutDir.zip"
$CacheBackup = Join-Path $GameRoot ("_diag_cache_backup_" + $Stamp)

Write-Host ""
Write-Host "========================================================"
Write-Host " SteelTemple MasterSword — Client Diagnostic"
Write-Host "========================================================"
Write-Host " Game folder : $GameRoot"
Write-Host " Executable  : $(Split-Path $Exe -Leaf)"
Write-Host " Report will : $ZipPath"
Write-Host "========================================================"
Write-Host ""
Write-Host "This will:"
Write-Host "  1) Back up + clear shader caches (programs/pipelines)"
Write-Host "  2) Launch the game with extra pink/resolution logging"
Write-Host "  3) When YOU close the game, zip logs to your Desktop"
Write-Host ""
Write-Host "Play ~30-60 seconds where it looks pink, then quit."
Write-Host ""
$null = Read-Host "Press Enter to start (or Ctrl+C to cancel)"

# --- stop any leftover process ---
Get-Process mastersword, hookshot -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# --- rotate current logs so this run is clean ---
foreach ($name in @("mastersword_log.txt", "hookshot_log.txt")) {
    $p = Join-Path $GameRoot $name
    if (Test-Path $p) {
        $bak = Join-Path $GameRoot ($name + ".before_diag_" + $Stamp)
        try { Move-Item $p $bak -Force } catch { Copy-Item $p $bak -Force; Remove-Item $p -Force -ErrorAction SilentlyContinue }
    }
}

# --- backup + clear caches ---
New-Item -ItemType Directory -Force -Path $CacheBackup | Out-Null
$cacheNames = @(
    "hookshot_programs.bin",
    "mkw_vk_pipelines.bin",
    "hookshot_pipelines.bin",
    "mastersword_programs.bin",
    "mastersword_pipelines.bin"
)
$moved = @()
foreach ($c in $cacheNames) {
    $src = Join-Path $GameRoot $c
    if (Test-Path $src) {
        Move-Item $src (Join-Path $CacheBackup $c) -Force
        $moved += $c
    }
}
Write-Host ("Caches moved to backup ({0}): {1}" -f $CacheBackup, ($(if ($moved) { $moved -join ', ' } else { 'none found' })))

# --- diagnostic environment (session only) ---
$env:HOS_RES_LOG = "1"                 # 3D scale / sizes every ~300 presents
$env:RX_VK_EXPOSURE_LOG = "1"          # exposure path (washed / white frames)
$env:HOS_SIZE_TRACE = "1"              # texture / RT size changes
$env:HOS_FPS_LOG = "1"                 # present rate
# Keep client's Output setting from ini; do not force HOS_OUTPUT_RES here.
# Force a cleaner present path for this capture (client can re-enable after):
$env:RX_VK_FRAMEGEN_MULT = "0"         # disable FG multiplier override if honored
# Note: menu FG may still be on from ini — KEYLINES will report frame interpolation.

Write-Host ""
Write-Host "Starting game with diagnostic logging..."
Write-Host "Close the game window when finished capturing."
Write-Host ""

$proc = Start-Process -FilePath $Exe -WorkingDirectory $GameRoot -PassThru
$start = Get-Date
try {
    Wait-Process -Id $proc.Id
} catch {
    # process already exited
}
$elapsed = [int]((Get-Date) - $start).TotalSeconds
Write-Host ("Game exited after {0}s (exit={1})" -f $elapsed, $proc.ExitCode)

# --- collect report ---
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$copyNames = @(
    "mastersword_log.txt", "hookshot_log.txt",
    "mastersword_log.1.txt", "mastersword_log.2.txt", "mastersword_log.3.txt", "mastersword_log.4.txt",
    "hookshot_log.1.txt", "hookshot_log.2.txt", "hookshot_log.3.txt", "hookshot_log.4.txt",
    "mastersword_options.ini", "hookshot_options.ini",
    "mastersword_version.txt", "VERSION", "language.txt"
)
foreach ($n in $copyNames) {
    $src = Join-Path $GameRoot $n
    if (Test-Path $src) { Copy-Item $src (Join-Path $OutDir $n) -Force }
}

# SYSTEM.txt
$gpu = (Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name) -join " | "
$os = (Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue).Caption
$exeItem = Get-Item $Exe
$sys = @()
$sys += "MasterSword client diagnostic"
$sys += "stamp=$Stamp"
$sys += "game_root=$GameRoot"
$sys += "exe=$($exeItem.FullName)"
$sys += "exe_size=$($exeItem.Length)"
$sys += "exe_mtime=$($exeItem.LastWriteTime.ToString('s'))"
$sys += "os=$os"
$sys += "gpu=$gpu"
$sys += "run_seconds=$elapsed"
$sys += "exit_code=$($proc.ExitCode)"
$sys += "caches_moved=$($moved -join ',')"
$sys += "diag_env=HOS_RES_LOG=1;RX_VK_EXPOSURE_LOG=1;HOS_SIZE_TRACE=1;HOS_FPS_LOG=1"
$sys -join "`r`n" | Set-Content (Join-Path $OutDir "SYSTEM.txt") -Encoding UTF8

# KEYLINES.txt — grepped highlights for us
$logCandidates = @(
    (Join-Path $OutDir "mastersword_log.txt"),
    (Join-Path $OutDir "hookshot_log.txt")
) | Where-Object { Test-Path $_ }
$keyPatterns = @(
    "hookshot ",
    "output resolution",
    "native output",
    "window crop",
    "GetCurrentGraphicsScale",
    "f_80ea2f30",
    "vi: res:",
    "frame interpolation",
    "present texture",
    "storage buffer NOT bound",
    "white frame",
    "dark frame",
    "exposure",
    "CRASH",
    "OoT visual workaround",
    "network ",
    "a clear after",
    "renderer:"
)
$keyOut = New-Object System.Collections.Generic.List[string]
$keyOut.Add("=== KEYLINES (auto-extracted) ===")
foreach ($log in $logCandidates) {
    $keyOut.Add("--- $(Split-Path $log -Leaf) ---")
    $lines = Get-Content $log -ErrorAction SilentlyContinue
    if (-not $lines) { $keyOut.Add("(empty)"); continue }
    $n = 0
    foreach ($line in $lines) {
        foreach ($pat in $keyPatterns) {
            if ($line -like "*$pat*") {
                $keyOut.Add($line)
                $n++
                break
            }
        }
        if ($n -ge 250) { $keyOut.Add("...(truncated at 250 hits)"); break }
    }
    if ($n -eq 0) { $keyOut.Add("(no pattern hits)") }
}
$keyOut -join "`r`n" | Set-Content (Join-Path $OutDir "KEYLINES.txt") -Encoding UTF8

# README for the zip
@"
Send this whole zip back to SteelTemple / the person who asked for a diagnostic.

Contents:
- mastersword_log.txt  (main capture)
- KEYLINES.txt         (auto highlights)
- SYSTEM.txt           (GPU / paths)
- options ini          (your settings)
- caches were backed up under the game folder: $CacheBackup

"@ | Set-Content (Join-Path $OutDir "SEND_THIS_ZIP.txt") -Encoding UTF8

if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path (Join-Path $OutDir "*") -DestinationPath $ZipPath -Force
Remove-Item $OutDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "========================================================"
Write-Host " DONE"
Write-Host " Zip created: $ZipPath"
Write-Host " Send that zip back."
Write-Host " Shader cache backup kept at:"
Write-Host "   $CacheBackup"
Write-Host "========================================================"
Write-Host ""
Read-Host "Press Enter to close"
