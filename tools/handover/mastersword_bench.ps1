# MasterSword bench run (Biggoron's Forge A/B, 2026-11-14): one headless, scripted run of an isolated copy of the game folder.
# The copy (-Game) has its own user folder, Slate, caches and hookshot_env.txt, so the player's folder is never touched.
#   -Exe <name>      which exe to run: exe_<name>.exe in -Game is copied over mastersword.exe first (afterwards exe_old.exe)
#   -Dll <name>      the same for the renderer: dll_<name>.dll over mastersword_vk.dll (afterwards dll_base.dll)
#   -Env "K=V;K=V"   more process environment (HOS_PGO is read before hookshot_env.txt is)
#   -Touch "f@s;.."  create file f (relative to -Game) s seconds into the run (HOS_PGO's .reset / .dump)
#   -Script          HOS_INPUT_SCRIPT ("<s>:<BTN>[+<BTN>]/<ms>,..."), on the frame clock (60 native frames = 1 s)
#   -Live            also read HOS_INPUT_LIVE=<run>\live_input.txt (lines "<s from now>:<BTN>/<ms>", "!shots <ms>")
#   -ShotEvery N     a PNG every N presents into <run>\shots (0: none; leave 0 for timing runs)
#   -Sample S,E      Mask of Truth: the sampler's .go file from S to E seconds into the run
param(
    [string]$Game = "C:\Games\Ocarina of Time PC\Hookshot Test - bench",
    [string]$Exe = "old",
    [string]$Dll = "",
    [string]$Env = "",
    [string]$Touch = "",
    [string]$Name = "bench",
    [int]$Seconds = 150,
    [string]$Script = "",
    [switch]$Live,
    [int]$ShotEvery = 0,
    [string]$Sample = ""
)
$ErrorActionPreference = "Stop"
function Copy-Retry($from, $to) {   # (a just-closed exe stays locked for a while: an antivirus scan, the image unmapping)
    for ($i = 0; $i -lt 60; $i++) {
        try { Copy-Item -Force $from $to -ErrorAction Stop; return } catch { Start-Sleep -Seconds 1 }
    }
    throw "could not copy $from over $to for 60 s"
}
$runs = Join-Path $Game "bench_runs\$Name"
$shots = Join-Path $runs "shots"
New-Item -ItemType Directory -Force $runs | Out-Null
if (Get-Process mastersword -ErrorAction SilentlyContinue) { throw "mastersword.exe is running: close the game first" }
Copy-Retry (Join-Path $Game "exe_$Exe.exe") (Join-Path $Game "mastersword.exe")
if ($Dll) { Copy-Retry (Join-Path $Game "dll_$Dll.dll") (Join-Path $Game "mastersword_vk.dll") }
if ($Env) {
    foreach ($pair in $Env.Split(";")) {
        if (-not $pair) { continue }
        $k, $v = $pair.Split("=", 2)
        Set-Item -Path "Env:$k" -Value $v
    }
}
$touches = @()
if ($Touch) {
    foreach ($t in $Touch.Split(";")) {
        if (-not $t) { continue }
        $f, $at = $t.Split("@", 2)
        $touches += [pscustomobject]@{ Path = (Join-Path $Game $f); At = [double]$at; Done = $false }
    }
}
# (every run from the same saves: a run's own autosaves would change which save the Load menu puts on top)
$pristine = Join-Path $Game "user_pristine"
if (Test-Path $pristine) {
    Remove-Item -Recurse -Force (Join-Path $Game "user")
    Copy-Item -Recurse $pristine (Join-Path $Game "user")
}

$env:RX_BACKEND = "vulkan"
$env:HOS_HEADLESS = "1"
$env:HOS_AUDIO = "0"
$env:HOOKSHOT_NO_UPDATE_CHECK = "1"
$env:HOS_NATIVE_OUTPUT = "0"
$env:HOS_INPUT_FRAME_CLOCK = "1"
$env:HOS_INPUT_SCRIPT = $Script
if ($Live) {
    $liveFile = Join-Path $runs "live_input.txt"
    "" | Set-Content -NoNewline $liveFile
    $env:HOS_INPUT_LIVE = $liveFile
} else { Remove-Item Env:HOS_INPUT_LIVE -ErrorAction SilentlyContinue }
if ($ShotEvery -gt 0) {
    New-Item -ItemType Directory -Force $shots | Out-Null
    Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
    $env:RX_VK_SHOTS = $shots
    $env:RX_VK_SHOT_EVERY = "$ShotEvery"
} else {
    Remove-Item Env:RX_VK_SHOTS -ErrorAction SilentlyContinue
    Remove-Item Env:RX_VK_SHOT_EVERY -ErrorAction SilentlyContinue
}
$go = Join-Path $Game "mask_of_truth_samples.txt.go"
Remove-Item $go -ErrorAction SilentlyContinue
$s0, $s1 = 0, 0
if ($Sample) { $s0, $s1 = $Sample.Split(",") | ForEach-Object { [int]$_ } }

$stdout = Join-Path $runs "stdout.txt"
$stderr = Join-Path $runs "stderr.txt"
$p = Start-Process -FilePath (Join-Path $Game "mastersword.exe") -WorkingDirectory $Game -PassThru -NoNewWindow `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr
"pid $($p.Id) exe=$Exe name=$Name seconds=$Seconds" | Set-Content (Join-Path $runs "run.txt")
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt $Seconds) {
    Start-Sleep -Milliseconds 500
    if ($p.HasExited) { break }
    $t = ((Get-Date) - $t0).TotalSeconds
    if ($Sample -and $t -ge $s0 -and $t -lt $s1 -and -not (Test-Path $go)) { New-Item -ItemType File $go | Out-Null }
    if ($Sample -and $t -ge $s1 -and (Test-Path $go)) { Remove-Item $go }
    foreach ($x in $touches) { if (-not $x.Done -and $t -ge $x.At) { New-Item -ItemType File -Force $x.Path | Out-Null; $x.Done = $true } }
}
Remove-Item $go -ErrorAction SilentlyContinue
if (-not $p.HasExited) {
    try { $p.CloseMainWindow() | Out-Null } catch {}
    Start-Sleep -Seconds 3
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}
try { Wait-Process -Id $p.Id -Timeout 30 -ErrorAction Stop } catch {}   # (the exe stays locked until it is gone)
Start-Sleep -Seconds 2
$gameLog = Join-Path $Game "hookshot_log.txt"   # (headless: the log goes to stderr)
if ((Test-Path $gameLog) -and ((Get-Item $gameLog).LastWriteTime -gt $t0)) { Copy-Item -Force $gameLog (Join-Path $runs "log.txt") }
else { Copy-Item -Force $stderr (Join-Path $runs "log.txt") }
$samples = Join-Path $Game "mask_of_truth_samples.txt"
if (Test-Path $samples) { Move-Item -Force $samples (Join-Path $runs "samples.txt") }
Copy-Retry (Join-Path $Game "exe_old.exe") (Join-Path $Game "mastersword.exe")   # (the bench copy is left on the baseline)
if ($Dll) { Copy-Retry (Join-Path $Game "dll_base.dll") (Join-Path $Game "mastersword_vk.dll") }
"done exe=$Exe exited=$($p.HasExited) shots=$(@(Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue).Count)"
