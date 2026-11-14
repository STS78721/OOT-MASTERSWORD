# MasterSword: run a bench job detached from the calling shell (overnight runs outlive a tool's time limit).
#   mastersword_detached.ps1 -Job <name> -Command "<PowerShell to run>"
# Writes <Game>\bench_runs\_jobs\<name>.log and, at the end, <name>.done (or <name>.failed with the error).
param([Parameter(Mandatory)][string]$Job, [Parameter(Mandatory)][string]$Command,
      [string]$Game = "C:\Games\Ocarina of Time PC\Hookshot Test - bench")
$dir = Join-Path $Game "bench_runs\_jobs"
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir "$Job.log"
Remove-Item (Join-Path $dir "$Job.done"), (Join-Path $dir "$Job.failed") -ErrorAction SilentlyContinue
$inner = @"
`$ErrorActionPreference = 'Stop'
try {
    & { $Command } *>&1 | Out-File -Append -Encoding utf8 '$log'
    'ok' | Set-Content '$(Join-Path $dir "$Job.done")'
} catch {
    `$_ | Out-String | Set-Content '$(Join-Path $dir "$Job.failed")'
}
"@
$ps1 = Join-Path $dir "$Job.ps1"
$inner | Set-Content -Encoding utf8 $ps1
Start-Process powershell -WindowStyle Hidden -ArgumentList "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$ps1`""
"started $Job"
