# Personal first GitHub release — prep (not pushed)

Base: local `hookshot-main-fixes` over Hookshot ~v2.0.5, with Nodix/SMAA/neon ports.
Upstream tip watched: see `runs/_upstream_watch/`. Do **not** push to `NotRexed/Hookshot`.

## Decide first

1. **Repo target** — new private/public fork under your GitHub account (recommended). Existing remote `mastersword` → `STS78721/OOT-MASTERSWORD` is a different project; only use it if that is intentional.
2. **Ship form** — source tag only, or Windows play folder zip, or both.
3. **Version** — e.g. `v0.1.0-yourname` or `v2.0.5-nodix.1` (tag is truth per `docs/versioning.md`; don’t claim to be upstream).

## What this build adds (release notes draft)

### Graphics / menu
- Tabbed Insert menu: Graphics / Upscaling / Frame Gen / Colour
- Nodix-style upscaler + FG rows (many backends still stand-ins except Hookshot native FG + spatial filters)
- SMAA (iryoku Medium) in present path: colour → SMAA → scale
- Colour profiles (vivid / OLED-style grade)
- DLSS removed from default path; TAAU / spatial path preferred for stability

### OoT / neon / foliage
- `#759` register markers never alias native operands (bright red/green foliage stems)
- `#957` storage-pointer handles fragment-only (foliage vanishing on sword swings)
- Vendor-conditional mesh stage buffer zero (off NVIDIA, on AMD)
- `CompositeExtractU32x4` handle program + 160MB SSBO stretch
- A2C off by default
- Lichii `#902` clip-distance race fix
- `#956` BC3 ASTC one layer a run (Hyrule Field grid lines)
- `#894` seed: drop depth+integer compute variants (RADV crash)

### Still open / do not oversell
- `#849` Output resolution row — not merged here (conflicts with local menu/host)
- `#863` draft register-role PR — skipped (abort reports)
- Residual neon/yellow stems may remain after `#759`; treat as improved, not guaranteed gone
- Nodix / XeSS / FSR / DLSS MFG menu entries are largely **labels / stand-ins** unless you ship their DLLs and wire them

## Binary ship set (Windows play folder)

**Include**
- `hookshot.exe`, `hookshot_vk.dll` (same build stamp)
- Runtime DLLs already beside the exe (SDL2, FFmpeg family, etc. as produced by the build)
- `Launch Hookshot.cmd` (clean, no fog/mesh experiment scripts)
- Short `READ ME.txt` / release notes (your own dump required; no game files)

**Exclude**
- `image.bin`, `romfs/`, `exefs`, any `.nup`
- `user/` saves and save copies (`user_fogtest`, Lichii copies, …)
- `hookshot_log*.txt`, dump folders, `*.bin` shader caches optional (large; first-run rebuilds)
- `nvngx_dlss.dll` unless you intend a DLSS build and have redistribution rights
- Patches, AGENTS/HANDOVER, bak_* / before_* binaries, experiment Launch scripts

## Source ship set

**Include**
- Tracked + intentional untracked: SMAA (`gpu/vk/vk_smaa*`, `gpu/vk/smaa/`, shader headers), colour (`vk_color*`, `color_grade*`), neon/menu ports above, tests for markers/mesh/U32x4
- Your release notes + LICENSE / third-party notices (iryoku SMAA, etc.)

**Exclude**
- `runs/`, `work/` build trees, `__pycache__`, personal paths, owner saves, secrets

## Build before tag

1. Full clean rebuild (not VK-only): exe + dll same commit.
2. Install to a **fresh** play folder (not your daily Test-latest with saves).
3. Smoke: Kokiri boot → Load → sword swing (foliage stay) → stems glance → Insert menu tabs → quit.
4. `python -I -B tests/test_lower_register_markers.py` (and any other quick source tests you care about).

## GitHub steps (when you say go)

1. Create empty GitHub repo (or fork).
2. Add remote (e.g. `mine`); push only an intentional branch — never force-push `origin`/`NotRexed`.
3. Commit local stack in conventional commits (`feat`/`fix`) so notes stay clean.
4. Tag `v…`; attach Windows zip + notes; mark prerelease if still experimental.

## Watch while prepping

Hourly loop (PID in `runs/_upstream_watch`) continues: fetch Hookshot + related, port neon/foliage/mesh fixes, rebuild+install Test-latest when useful. Release zip stays separate from the dirty play folder.
