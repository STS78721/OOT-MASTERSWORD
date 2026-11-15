# MasterSword 0.1.9d — release handover (for Antigravity)

*Written 2026-11-15 by Claude, after the overnight bench work. Branch `mastersword-fg`, pushed to the `mastersword` remote
(STS78721/OOT-MASTERSWORD) at `d6f9d436` + this file. Full history and reasoning: `docs/MASTERSWORD_HANDOVER.md`.*

## What 0.1.9d is

A small performance release. Two changes measured faster on a scripted, repeatable Town route (headless bench, market
square, ~2,700-2,900 draws, same scene every run, runs alternated):

| Change | Where | Town, at equal draws |
|---|---|---|
| **Biggoron's Forge V1** — the hot split re-made from a measured profile (Mask of Truth): 699 measured-hot game functions compiled at -O2 with inlined float fast paths | game exe (`nx2/make_hot_split.py` + `work/measured_heat.txt`) | **-2.0%** frame time (45.1 -> 46.1 fps) |
| **Capture copy split** — the submit-time memory capture cut into smaller parts so the game's thread no longer waits for the slower E-core helpers | renderer DLL (`gpu/vk/vk_memory.cpp` `copy_split`) | **-1.7%** (45.7 -> 46.5 fps) |

Together roughly +1.5-2 fps in Town's market on an RTX 5070 Ti / Core Ultra 7 270K Plus. Modest, but every run of the new
build beat every run of the old one by more than the run-to-run spread. Also in this release (already committed earlier):
Gemini's session (Dear ImGui overlays, Hero's Shade, Slate 3, Longshot ports), the mirror-idle fix (base fps no longer drops
after Field -> Town -> Field), the Scarecrow's Song / Deku Tree floor-web frame-rate ports, and a crash fix in the diagnostic
wait logs (HOS_WAKE_LAT / HOS_WAIT_LOG / HOS_WAIT_WHO; off for players).

**Not in it — do not claim:** PGO, -hot-cold-split (measured: no gain), the "Rito" GPU copy engine (measured: not worth
building), the getenv cache (made recording faster but the frame slower: reverted), Biggoron's Forge tier B (measured
slower: off by default). New diagnostics (Sun's Song `HOS_WAKE_TRACE`, Mask of Truth scripts, the bench) are dev tools only.

## Build (on this PC — the hot split state is local)

1. **Do not re-lift or re-split.** `work/` is git-ignored. The tested hot split lives in `work/sa/gen/code_hot_*.c` (16
   pieces), made by `nx2/make_hot_split.py` from `work/measured_heat.txt` + `work/hot_keep.txt`, tier B off (the default).
   A fresh clone would build the old static split and lose the -2%.
2. Build with `work\build_full.bat` (exe + DLL). Do **not** set `RX_PGO`, `RX_HOT_FLAGS` or `RX_MEASURED_TIER_B` in the
   environment. If writing your own script, quote the sets (`set "RX_RELEASE=1"`): a trailing space made a console exe with
   no log, no precompile and no FSR4 once.
3. Check the result: `work\sa\build\mastersword.exe` ~405 MB, PE subsystem 2 (windows), `hookshot_log.txt` written on start,
   FSR4 listed in the log. The tested binaries for comparison: exe md5 `9199f6c5…` (V1; a rebuild differs only in its build
   stamp bytes), DLL `f519b943…` (copy split). HEAD's DLL also carries a logging-only counter ("MasterSword Rito (600
   submits)"), which ran as a baseline on the bench without any measurable cost.
4. The program cache (7.4 GB) is not invalidated by these changes (no `eden/`, `sirit/`, `compat/`, `vk_shader.cpp`,
   `vk_state.h`, `vk_api.h` or `Program`/`Shader::Info` size change).

## Must be tested live before release (the bench was headless)

The bench ran offscreen at 1080p without a window, present, Reflex or frame generation. Before shipping, play with the
user's real settings (4K, DLSS SR + MFG):
- Town market, Hyrule Field, Kokiri Forest, one dungeon (Deku Tree: the floor web), Scarecrow's Song once.
- Watch for: one-frame white flashes (the exposure meter), flicker on foliage, missing objects, anything new vs 0.1.9c.
- Field -> Town -> Field: base fps should come back (the mirror fix).
- The current smoke folder (`Hookshot Test - smoke_0.1.3b`) already runs the tested pair (V1 exe + copy-split DLL).

## Ship set (from runs/_release_prep/CHECKLIST.md, still valid)

Include the exe + renderer DLL of one build, the runtime DLLs beside them, the launcher, a short read-me. **Never** include:
game files (`image.bin`, `romfs/`, exefs, `.nup`), `user/` saves, `mastersword_options.ini` / any personal `.ini` or
`.json`, logs, caches (`*.bin` shader/pipeline caches, `mastersword_spirv.pack`, `mastersword_slate.bin`), NGX/DLSS DLLs
without redistribution rights, `bak_*` binaries, `AB_test/`, `hookshot_env.txt` with test switches. Fresh-install defaults
must stay safe (1080p baseline). The user's private details stay out of everything (notes in `runs/` mention local paths:
keep them uncommitted).

## Release steps

1. `VERSION` -> `0.1.9d`; `runs/_release_prep/PATCH_NOTES_0.1.9d.md` from the draft below (match 0.1.9c's style).
2. Build (above), test live (above), package per the ship set.
3. Tag and publish on **STS78721/OOT-MASTERSWORD** only. Never push to `origin` (NotRexed/Hookshot) or `longshot`; never
   force-push `main`.
4. Update `docs/MASTERSWORD_HANDOVER.md` ("Releases" section) with what shipped.

## Draft patch notes

> **MasterSword 0.1.9d — Biggoron's Forge**
> - **Faster in busy areas:** the game's hottest code is now compiled from a measured profile of real play (Biggoron's
>   Forge), and the renderer's per-frame memory snapshot no longer waits on the slower CPU cores. About +1.5-2 fps in
>   Hyrule Castle Town's market in our tests; lighter areas were already near 60.
> - Base frame rate no longer drops after going Field -> Town -> Field.
> - Scarecrow's Song and the Deku Tree floor web behave correctly above 60 fps.
> - Dear ImGui overlays, Hero's Shade (per-area renderer tuning) and Sheikah Slate 3 from the previous dev cycle.
> - No change to visuals: every optimisation keeps the game's output identical.

## Open leads after the release (not for 0.1.9d)

- The frame's critical path in Town: the game waits every frame for a GPU readback written at the end of the frame
  (research doc s12). Recording faster alone did not help; the next test is whether the game's critical threads lose
  boost clock when the recording workers all run (thread placement on the favoured P-cores; fewer workers busy at once).
- Mako (Shakoquo/Mako, a GPL-3 Hookshot fork for AMD): its Castle Town import-stall fix and "NaN guard" may be worth a
  look; no fps numbers are published.
