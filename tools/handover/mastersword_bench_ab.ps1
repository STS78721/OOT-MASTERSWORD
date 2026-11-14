# MasterSword Biggoron's Forge A/B (2026-11-14): the same scripted Town route on both exes, alternated, then the frame rate
# compared over the same stretch of presents. Route (frame clock, from the bench copy's newest autosave, "Hyrule Castle Town"):
# title A, A -> Load menu; A, A, A -> load; forward 7 s -> the market square by the fountain; stand still there.
param(
    [string]$Game = "C:\Games\Ocarina of Time PC\Hookshot Test - bench",
    [string[]]$Order = @("old", "forge", "old", "forge"),
    [int]$Seconds = 270,
    [string]$Tag = "ab",
    [string]$Sample = "",
    [int]$Start = 1,
    [string]$Dlls = ""   # (with -Order of exes: "name,name,..": the renderer DLL of each run, "" for the one in place)
)
$ErrorActionPreference = "Stop"
$route = "37:A,40.5:A,51.5:A,54:A,57:A,64.5:LUP/7000"
$bench = Join-Path $PSScriptRoot "mastersword_bench.ps1"
$i = $Start - 1
foreach ($exe in $Order) {
    $i++
    $name = "{0}_{1}_{2}" -f $Tag, $i, $exe
    $dll = if ($Dlls) { ($Dlls.Split(", ", [System.StringSplitOptions]::RemoveEmptyEntries))[$i - $Start] } else { "" }
    if ($dll) { $name = "{0}_{1}_{2}" -f $Tag, $i, $dll }
    & $bench -Game $Game -Exe $exe -Dll $dll -Name $name -Seconds $Seconds -Script $route -Sample $Sample
}
