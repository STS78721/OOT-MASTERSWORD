# MasterSword bench A/B (2026-11-14): the same scripted route on each run, alternated, then the frame rate compared over the
# same stretch of presents (scripts/bench_compare.py). Routes (frame clock, from the bench copy's newest autosave, "Hyrule Castle
# Town"): town = title A, A -> Load menu; A, A, A -> load; forward 7 s -> the market square by the fountain; stand still there.
# field = the same load, then back out of the gate 12 s -> the lighter area outside the town (~750-850 draws); stand there.
param(
    [string]$Game = "C:\Games\Ocarina of Time PC\Hookshot Test - bench",
    [string[]]$Order = @("old", "forge", "old", "forge"),
    [int]$Seconds = 270,
    [string]$Tag = "ab",
    [string]$Sample = "",
    [int]$Start = 1,
    [string]$Dlls = "",    # ("name,name,..": the renderer DLL of each run, "" for the one in place)
    [string]$Envs = "",    # ("K=V;K=V|K=V|..": the extra process environment of each run, "|" between runs)
    [string]$Labels = "",  # ("a,b,..": each run's name in place of the exe's, e.g. shade_on,shade_off)
    [ValidateSet("town", "field")][string]$Route = "town"
)
$ErrorActionPreference = "Stop"
$routes = @{
    town  = "37:A,40.5:A,51.5:A,54:A,57:A,64.5:LUP/7000"
    field = "37:A,40.5:A,51.5:A,54:A,57:A,64.5:LDOWN/12000"
}
$bench = Join-Path $PSScriptRoot "mastersword_bench.ps1"
function Split-List([string]$s, [char[]]$sep) {
    if ($s) { return @($s.Split($sep, [System.StringSplitOptions]::RemoveEmptyEntries)) } else { return @() }
}
$dllList = Split-List $Dlls ", "
$envList = Split-List $Envs "|"
$labelList = Split-List $Labels ", "
$i = $Start - 1
$k = 0
foreach ($exe in $Order) {
    $i++
    $dll = if ($dllList.Count -gt $k) { $dllList[$k] } else { "" }
    $env = if ($envList.Count -gt $k) { $envList[$k] } else { "" }
    $what = if ($labelList.Count -gt $k) { $labelList[$k] } elseif ($dll) { $dll } else { $exe }
    $k++
    $name = "{0}_{1}_{2}" -f $Tag, $i, $what
    & $bench -Game $Game -Exe $exe -Dll $dll -Env $env -Name $name -Seconds $Seconds -Script $routes[$Route] -Sample $Sample
}
