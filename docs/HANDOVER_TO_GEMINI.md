# MasterSword — handover to Gemini (2026-11-16)

*Written by Claude at the end of the 0.1.9d cycle. This is the "start here" page: where the code and the upstreams are, what is
in progress right now, what was measured, and what to do next. The long history and reasoning are in
`docs/MASTERSWORD_HANDOVER.md` (§3 findings, §7 open items, §8 after 0.1.9d). The techniques playbook is
`docs/MASTERSWORD_TECHNIQUES.md`, and the CPU/GPU research is `docs/MASTERSWORD_HARDWARE_OFFLOAD_RESEARCH.md` (§6-12).*

**Goal:** a 60 fps base in Hyrule Castle Town's market with **no fidelity loss**. Fidelity first: never trade image quality,
LOD, textures or lighting for fps.

---

## 1. Where things are

| What | Where |
|---|---|
| **Source (work here)** | `C:\Games\Hookshot - NotRexed - Test e400f88`, branch **`mastersword-fg`** |
| Smoke folder (the user plays this) | `C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b` |
| Bench copy (headless runs) | `C:\Games\Ocarina of Time PC\Hookshot Test - bench` (its runs are in `bench_runs\`) |
| Build scripts (git-ignored: they hold this PC's paths) | `work\build_full.bat` (exe + DLL), `work\build_vk.bat` (renderer only) |
| Outputs | `work\sa\build\mastersword.exe` (~405 MB), `work\sa\build\mastersword_vk.dll` (~9 MB) |
| Lifted game code + the hot split | `work\sa\gen\code_*.c`, `code_hot_*.c` (local only, never leaves the PC) |
| Standing project rules | `runs\_release_prep\STICKY.md` (read first) |

The game loads `mastersword_vk.dll` and falls back to `hookshot_vk.dll` only when the first is missing (`host/rx_backend.c`).
Deploy the renderer as `mastersword_vk.dll`.

### Our repository and the upstreams

| Remote | URL | What it is | How we use it |
|---|---|---|---|
| `mastersword` | https://github.com/STS78721/OOT-MASTERSWORD | **Ours** | Push `mastersword-fg` (fast-forward only). Releases are published here. `main` is never force-pushed. |
| `longshot` | https://git.magicbean.dev/MagicBeanDev/Longshot | **The active upstream**: Hookshot's maintained successor, very active (~100 commits a day) | Read and port only. Never push. |
| `origin` | https://github.com/NotRexed/Hookshot | The original Hookshot. Dead. | History only. Never push. |
| (no remote) | https://github.com/Shakoquo/Mako | A GPL-3 Hookshot fork aimed at AMD | Ideas only: its Castle Town import-stall fix and "NaN guard". It publishes no fps numbers. |

**Moving to Codeberg (2026-11-16, GitHub down):** the user is moving the repository to Codeberg.
- **Repository and remote:** create an empty repository there (no README or licence), then
  `git remote add codeberg https://codeberg.org/STS78721/<repo>.git` and `git push codeberg mastersword-fg main`.
- **Credentials:** the user signs in themselves, with a Codeberg access token in the Windows credential prompt. Never put a
  password in a script, a remote URL or a note.
- **Releases don't move with git.** The 0.1.9d zips must be uploaded again on Codeberg.
- **Update checks:** the game's update check (`host/hos_update.c`) and the launcher (`launcher/MasterSwordLauncher.py`) read
  GitHub releases. If Codeberg becomes the home, point both at Codeberg's releases API, which is Forgejo's and
  GitHub-like: `/api/v1/repos/<owner>/<repo>/releases`.

**Checking Longshot for new work:**

```
git fetch longshot
git log --since=<last check date> --format="%h %ad %s" --date=short mastersword-fg..longshot/main | grep -E "perf|fix\(vk|fix\(vcores|fix\(host|fix\(lift"
git show <hash> --format=%B | sed -n "/## Why/,/## Testing/p"     # their PR bodies carry the reason and the measurements
git show <hash> -- <files> > work/x.patch; git apply --check work/x.patch
```

- **Last checked:** 2026-11-17, `longshot/main` = `544229a2` (25 applicable PRs ported across `b14fca13..34038ede`; skipped `#588` render thread, `#977` early reads, `#903`/`#1188` precompile, `#952` Intel UB snapshot, and relift PRs `#1098`/`#1224`/`#1152`/`#1191`/`#1233`).
- **Porting from Longshot:**
  - Most of their patches no longer apply as-is, because the trees have drifted. Port by hand, and mark ported lines `(longshot #NNN)`.
  - Their tree uses `longshot_env.h` / `env_on()`, and ours uses plain `getenv`.
  - Their `docs/tested/*.md` and tests often don't exist here; skip those.
- **Already ported:** the table in `docs/MASTERSWORD_HANDOVER.md` §4 (occlusion queries, BC7, handle tables, quick pipelines,
  DLSS/FG motion fixes, Scarecrow's Song and Deku web frame timing).

---

## 2. Current state (2026-11-17)

- **Released:** 0.1.9d (Biggoron's Forge V1 + capture copy split, ~+4% in market).
  Release notes: `runs/_release_prep/PATCH_NOTES_0.1.9d.md`. Release handover: `docs/RELEASE_HANDOVER_0.1.9d.md`.
- **In progress — 0.2.0 staged & deployed to smoke for live playtest (`mastersword-fg`):**
  - Ported 25 `MagicBeanDev/Longshot` PRs up to `544229a2` (`#231`, `#590`, `#621`, `#661`, `#552`, `#442`, `#994`, `#1255`, `#1222`, `#1091`, `#384`, `#672`, `#568`, `#553`, `#554`, `#1251`, `#1248`, `#1244`, `#311`, `#788`, `#1083`, `#468`, `#469`, `#431`, `#440`, `#670`, `#769`, `#1256`, `#594`, `#488`, `#1167`) while keeping **`RX_VK_DEAD_MIN_UNUSED=3`**, 96 MB pressure pool floor, 7.4 GB program cache stamp, ABI `10/11`, and all 91 lifted chunks untouched.
  - Bumped `VERSION`, `host/hos_update.c` (`permalink/latest`), and `launcher/MasterSwordLauncher.py` to **`0.2.0`**, rebuilt standalone binaries + `MasterSwordLauncher.exe`, deployed to smoke (`C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b` with `*.bak_pre_020` backups, keeping `mastersword_vk.dll.bak_4x50`), and staged `0.2.0` zips in `runs/_release_prep/dist/`. Release handover: `docs/RELEASE_HANDOVER_0.2.0.md`.
- **Note on earlier parked experiments:** `#994` target pool (with `DEAD_MIN_UNUSED=3` + `#1255`) and `vk_mvec.cpp` cached `getenv` are now integrated on `mastersword-fg`.

---

## 3. What we know (findings that held)

### The critical path in Town (Mask of Truth + Sun's Song + GPU profile)

- **The main thread** waits ~60% of its time on one event (`100a758600`, lr `0x80080188`).
  - A **ModuleSystemWorker** signals it after its renderer readback: `rx_nvn_invalidate` → `invalidate` → `wait_recorded_part`.
  - That readback waits for the GPU to finish the last part of the frame's submission.
- **So the frame is serial:** the game records → the renderer records → the GPU runs → the readback → the game continues.
  Recording faster alone did not help.
- **GPU frame (profiled):** copies 4.2 ms (mirror sync 2.7, readback refresh 1.34), passes 4.0, barriers 1.65 (147 full
  barriers + render-pass ends), and **6.2 ms idle waiting for recording**.
- **CPU per frame (sampled, Town):** main ~3.1 ms, module workers ~10.6-10.9 ms, job workers 21-25 ms, recording workers 4.2-5.6 ms.

### Measured on the headless bench (Town market, ~2,700-2,900 draws, runs alternated, compared at equal draws)

| Change | Result | Status |
|---|---|---|
| Biggoron's Forge V1 (hot split from measured heat, `nx2/make_hot_split.py`) | -2.0% frame time | **Shipped 0.1.9d** |
| Capture copy split (`vk_memory.cpp` `copy_split`) | -1.7% | **Shipped 0.1.9d** |
| Forge tier B (more functions at -O2) | slower (+11 MB code) | Off (`RX_MEASURED_TIER_B=0`) |
| PGO on the hot split, `-hot-cold-split` | no gain | Not used |
| "Rito" (mirror-sync copies on a transfer queue) | only 40% movable, ~0.3-0.5 ms | Not built |
| Hero's Shade (per-area recording thread count) | too small to measure | Off by default |
| getenv cache in `vk_mvec.cpp` | unsettled (see above) | Parked |
| CPU-boost hypothesis (critical threads lose clock when all recorders run) | not supported by the samples | Dropped |

**Noise sources to avoid:**
- Heavy work on the PC during runs (paging).
- Antivirus scans.
- Running the bench while the user plays.
- A run whose route didn't reach the market (draws < 2,500).

---

## 4. Tools

| Tool | Where | Use |
|---|---|---|
| Headless bench | `tools/handover/mastersword_bench.ps1` (one run), `mastersword_bench_ab.ps1` (alternated A/B), `mastersword_detached.ps1` (runs beyond a tool's time limit) | Scripted route to the market, 270 s, frame-stat lines in the run's log. Method: `runs/_release_prep/HEADLESS_BENCH_METHOD.md` |
| Compare | `python -I scripts/bench_compare.py "<bench>\bench_runs" <tag>` | Mean frame time per run and per group, at equal draws. Run folders must be named `<tag>_<n>_<what>`. |
| Mask of Truth | `HOS_SAMPLE_ALL=<file>` in the env + `scripts/mask_of_truth.py`, `mask_of_truth_hot.py` | Where every game thread's CPU time goes. The desktop .bat samples 30 s. |
| Sun's Song | `HOS_WAKE_TRACE` + `scripts/suns_song.py` (`host/suns_song.c`) | Which thread wakes which: the critical path |
| Renderer profiles | `RX_VK_GPU_PROFILE`, `RX_VK_PROF_FINE=1`, `RX_VK_LOG_READBACK_WAITS`, `RX_VK_SLOW_LOG=<ms>`, `HOS_WAIT_WHO` | GPU pass/copy/barrier times, per-draw CPU, readback waits |

**Bench pitfalls**
- **`-Envs` drops empty entries.** `"|A||A"` shifts every setting to the wrong run. Give every run an explicit
  setting, even a harmless one.
- `-Dlls` names map to `dll_<name>.dll` in the bench folder. `-Order old` = `exe_old.exe` (the V1 exe, `9199f6c5`).
- **Ask the user before headless runs.** They sometimes prefer to test live. On 2026-11-16 they said "skip headless, I'll test".
- After a run is stopped, put `dll_base.dll` back as the bench's `mastersword_vk.dll`.

---

## 5. Next steps, in order

1. **Live smoke test `0.2.0`** (`C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b` — follow `docs/RELEASE_HANDOVER_0.2.0.md`).
2. **Remaining Longshot leads (not yet ported):**
   - **#569 renderer PGO** (`4e80ea63`): 3-5% renderer time. Needs our own play profile for `mastersword_vk.dll`.
   - **#588 render thread** (`82886e89`) + **#838** (`343a1dfa`, don't pin it to one core): a big change. It replays frame N
     while the game builds N+1. Needs care with our readback path, because the game waits on a readback every frame (§3).
   - **#38 fast float regions** (`02608169`, `nx2/fastfp.py`): a lift change, so a full re-translate. Overlaps Biggoron's
     Forge's inline float fast paths, so measure against Forge, not against plain.
   - Already done on `mastersword-fg` (`b14fca13..34038ede`): **#231** flat caches, **#590** replay costs / `wrote_regions` / `getenv` caches, **#994+#1255** target pool (`RX_VK_DEAD_MIN_UNUSED=3`), **#1222** ASTC reuse, **#1256** shadow bucketing, **#1091** AMD `past_allowance`, **#1083/#468/#469/#431/#440/#769/#594/#488/#1167**, etc.; **#766** already covered.
3. **The readback on the critical path** (research doc §12): narrowing the readback refresh, and barrier precision. **Fidelity risk:**
   only with the user watching the picture.
4. **Release `0.2.0`:** once live smoke passes, push/tag and publish from `runs/_release_prep/dist/` per `docs/RELEASE_HANDOVER_0.2.0.md`.

---

## 6. Rules (the user's, non-negotiable)

- **Fidelity first.** No visual downgrade for fps.
- **Nothing derived from the game leaves the PC:** lifted C, dumps, shader caches, saves, image.bin, romfs.
- **Releases never carry personal files:** `.ini`, `.json`, caches, saves, NGX/DLSS binaries, `hookshot_env.txt` with test switches.
  Fresh-install defaults stay safe (1080p, windowed, upscaling/FG/Reflex off).
- **The program cache stamp:** don't edit `eden/`, `sirit/`, `compat/`, `vk_shader.cpp`, `vk_state.h` or `vk_api.h`, and don't resize
  `Program` / `Shader::Info`. Any of these rebuilds the user's 7.4 GB program cache; use side tables instead.
- **Naming:** MasterSword's own tech gets Zelda-reference names, branded MasterSword / SteelTempleSaint.
- **Git:**
  - Never force-push `main`; push only to `mastersword`.
  - Keep `runs/` notes uncommitted (they contain local paths).
  - Don't overwrite `mastersword_vk.dll.bak_4x50`.
- **The PC:** don't touch Intel DTT / `apo.config` or Windows settings.
- **Privacy:** the user's email and personal details stay out of commits, notes and anything sent outside.
- **Handover:** keep this file and `docs/MASTERSWORD_HANDOVER.md` current as each step lands.
