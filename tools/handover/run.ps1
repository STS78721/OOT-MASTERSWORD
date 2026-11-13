param(
    [string]$Game = "C:\Games\Ocarina of Time PC\Hookshot Test - latest",
    [string]$User = "user_fogtest",
    [string]$Name = "base",
    [int]$Seconds = 150,
    [string]$Env = "",
    [int]$ShotEvery = 90
)

$ErrorActionPreference = "Stop"
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not (Test-Path (Join-Path $root "runs"))) { $root = (Get-Location).Path }
$runs = Join-Path $root "runs\$Name"
$shots = [System.IO.Path]::GetFullPath((Join-Path $runs "shots"))
New-Item -ItemType Directory -Force $runs, $shots | Out-Null
Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force

Get-Process hookshot -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
$env:RX_BACKEND = "vulkan"
$env:HOS_HEADLESS = "1"
$env:HOS_AUDIO = "0"
$env:HOOKSHOT_NO_UPDATE_CHECK = "1"
$env:HOS_NATIVE_OUTPUT = "0"
$env:RX_VK_SHOTS = $shots
$env:RX_VK_SHOT_EVERY = "$ShotEvery"
# Title → file select → confirm Load (extra A taps; cold shader cache slows the menus) → walk outdoors (avoid house)
$env:HOS_INPUT_SCRIPT = "18:A,25:A,32:A,40:A,48:A,55:A,65:A,85:LRIGHT/14000,110:LDOWN/6000,130:LRIGHT/12000,160:LUP/5000,190:LRIGHT/10000"
Remove-Item Env:HOS_INPUT_LIVE -ErrorAction SilentlyContinue

Remove-Item Env:RX_VK_MESH_ZERO -ErrorAction SilentlyContinue
Remove-Item Env:RX_VK_MESH_NO_ZERO -ErrorAction SilentlyContinue
if ($Env) {
    foreach ($pair in $Env.Split(";")) {
        if (-not $pair) { continue }
        $k, $v = $pair.Split("=", 2)
        Set-Item -Path "Env:$k" -Value $v
    }
}

$stdout = Join-Path $runs "stdout.txt"
$stderr = Join-Path $runs "stderr.txt"
$live = Join-Path $runs "live.txt"
"" | Set-Content $live

$p = Start-Process -FilePath (Join-Path $Game "hookshot.exe") `
    -ArgumentList @("image.bin", "--romfs", "romfs", "--user", $User) `
    -WorkingDirectory $Game -PassThru -NoNewWindow `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr

"pid $($p.Id) name=$Name seconds=$Seconds env=$Env" | Tee-Object -FilePath $live
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt $Seconds) {
    Start-Sleep -Seconds 5
    if ($p.HasExited) { break }
    $n = @(Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue).Count
    $tail = ""
    if (Test-Path $stderr) {
        $tail = (Get-Content $stderr -Tail 1 -ErrorAction SilentlyContinue)
    }
    "t=$([int]((Get-Date) - $t0).TotalSeconds)s shots=$n :: $tail" | Tee-Object -FilePath $live -Append
}
if (-not $p.HasExited) {
    # graceful first so hookshot_log.txt flushes; force after a short wait
    try { $p.CloseMainWindow() | Out-Null } catch {}
    Start-Sleep -Seconds 2
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}
Start-Sleep -Seconds 1
# Prefer the process log when it flushed; otherwise keep stderr (headless kill often skips the file).
$gameLog = Join-Path $Game "hookshot_log.txt"
$outLog = Join-Path $runs "log.txt"
if ((Test-Path $gameLog) -and ((Get-Item $gameLog).LastWriteTime -gt (Get-Date).AddSeconds(-$Seconds - 30))) {
    Copy-Item -Force $gameLog $outLog
} else {
    Copy-Item -Force $stderr $outLog -ErrorAction SilentlyContinue
}
"done shots=$((Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue).Count)" | Tee-Object -FilePath $live -Append
