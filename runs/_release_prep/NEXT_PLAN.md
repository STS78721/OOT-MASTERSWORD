# Next plan — after FG 4x retest

## Done this turn
- Hard 3x synth cap removed; menu 4x synthesises fully; 6x/8x soft-cap→4 at 1440p+
- Menu FG help string updated; DLL rebuilt+deployed (golden bak untouched)
- Host cherries in source: F11 bare-only (#1608), keep-display (#1538) — need **exe** rebuild to land in smoke

## User retest (smoke)
1. MasterSword FG engine, multiplier **4x** — expect log `soft-capped` only for 6/8, not for 4; game stays 60
2. If still broken at 4x: paste FG lines from `mastersword.log` (OutSet / job drop / synth)

## After green 4x (one task)
- Rebuild `mastersword.exe` so display cherries hit smoke, **or**
- Next safe cherry one-by-one (capture/PNG later — large overlap)
