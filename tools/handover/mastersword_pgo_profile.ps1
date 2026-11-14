# MasterSword PGO profile runs (2026-11-14): the instrumented exe (exe_pgogen.exe, built with RX_PGO=gen) on the bench copy,
# counters cleared once the route has arrived and dumped near the end (host/pgo_rt.c: HOS_PGO=<file>, <file>.reset/.dump).
# Two routes so the profile is not Town's alone: Town (the A/B route, then walking and turning in the market) and Hyrule
# Field (the second-newest autosave, then a long walk). The .profraw files land in bench_runs\pgo_<route>\.
param([string]$Game = "C:\Games\Ocarina of Time PC\Hookshot Test - bench", [int]$Seconds = 300)
$ErrorActionPreference = "Stop"
$bench = Join-Path $PSScriptRoot "mastersword_bench.ps1"
$routes = [ordered]@{
    town  = "37:A,40.5:A,51.5:A,54:A,57:A,64.5:LUP/7000,80:RRIGHT/3000,90:LRIGHT/4000,100:LUP/4000,110:LLEFT/5000," +
            "120:LDOWN/4000,130:RLEFT/3000,140:LUP/6000,155:RRIGHT/2000,160:LDOWN/6000"
    field = "37:A,40.5:A,50.8:DOWN,51.5:A,54:A,57:A,64.5:LUP/8000,75:LRIGHT/5000,82:LUP/10000,95:LLEFT/6000," +
            "105:LUP/10000,118:RRIGHT/4000,125:LUP/10000,140:LLEFT/4000,146:LUP/10000"
}
foreach ($r in $routes.Keys) {
    $name = "pgo_$r"
    $raw = Join-Path $Game "bench_runs\$name\$r.profraw"
    New-Item -ItemType Directory -Force (Split-Path $raw) | Out-Null
    Remove-Item $raw -ErrorAction SilentlyContinue
    $rel = "bench_runs\$name\$r.profraw"
    & $bench -Game $Game -Exe pgogen -Name $name -Seconds $Seconds -Script $routes[$r] -ShotEvery 1200 `
        -Env "HOS_PGO=$raw" -Touch "$rel.reset@120;$rel.dump@$($Seconds - 12)"
    if (-not (Test-Path $raw)) { throw "no profile from $r ($raw)" }
    "profile $r : $((Get-Item $raw).Length) bytes"
}
