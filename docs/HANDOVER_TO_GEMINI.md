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

**Checking Longshot for new work:**

```
git fetch longshot
git log --since=<last check date> --format="%h %ad %s" --date=short mastersword-fg..longshot/main | grep -E "perf|fix\(vk|fix\(vcores|fix\(host|fix\(lift"
git show <hash> --format=%B | sed -n "/## Why/,/## Testing/p"     # their PR bodies carry the reason and the measurements
git show <hash> -- <files> > work/x.patch; git apply --check work/x.patch
```

- **Last checked:** 2026-11-16, `longshot/main` = `bbe421f5`. Start the next check from that commit or that date.
- **Porting from Longshot:**
  - Most of their patches no longer apply as-is, because the trees have drifted. Port by hand, and mark ported lines `(longshot #NNN)`.
  - Their tree uses `longshot_env.h` / `env_on()`, and ours uses plain `getenv`.
  - Their `docs/tested/*.md` and tests often don't exist here; skip those.
- **Already ported:** the table in `docs/MASTERSWORD_HANDOVER.md` §4 (occlusion queries, BC7, handle tables, quick pipelines,
  DLSS/FG motion fixes, Scarecrow's Song and Deku web frame timing).

---

## 2. Current state (2026-11-16)

- **Released:** 0.1.9d on GitHub (Biggoron's Forge V1 + the capture copy split, about +4% in the market).
  Release notes: `runs/_release_prep/PATCH_NOTES_0.1.9d.md`. Release handover: `docs/RELEASE_HANDOVER_0.1.9d.md`.
- **Committed and pushed after the release:** `888d0a6f`, the icons. All three `.ico` files now have real transparency, regenerated
  from `assets/mastersword.png`. The game exe has an icon again: `nx2/make_icon.py` now writes `work/mastersword.res`, which
  `nx2/build.py` links. The launcher spec excludes numpy.
- **Committed locally, not pushed:** `b32a315e` (handover: the Longshot check) and this file.

### In progress: the user is play-testing the render target pool (Longshot #994)

- **The change:** uncommitted in the working tree: `gpu/vk/vk_texture.cpp` plus the new, untracked `gpu/vk/target_pool.h`. Also saved as
  `work/targetpool.patch`. It changes three defaults:
  - The pool's bound is a twelfth of the VRAM the driver allows, 256-768 MB (it was a fixed 256 MB).
  - `RX_VK_DEAD_EVERY` 10 → 1.
  - `RX_VK_DEAD_MIN_UNUSED` 3 → 1.
- **Why:** the game makes its per-frame render targets at shifted addresses every frame. Our bench logs show ~6,700 made new and
  ~6,550 destroyed per Town run. Longshot measured in the market at night (RTX 4070 S):
  - images made new: -62%
  - frames over 25 ms: 76 → 49
  - 1% low: 42 → 45 fps
  - median fps: about the same
- **Deployed:** the smoke folder runs it (exe `d9096b63`, DLL `f48dc04b`). The previous pair is kept as `*.bak_pre_pool`.
- **The user's test shortcuts (desktop):**
  - "MasterSword - NEW target pool" runs `Test - NEW target pool.bat` in the smoke folder.
  - "MasterSword - OLD target pool" runs `Test - OLD target pool.bat`, which sets the three old values.
  - Both run the same build.
- **What to compare:**
  - Hitches and 1% lows in the market, at night if possible.
  - The log's `render target images: … made new` count. NEW also logs `render target pool: up to 768 MB`.
  - **Fidelity:** single-frame flashes or wrong images. Longshot's picture check counted more small one-frame changes with it on.
- **When the user reports:**
  - **Better or equal, and no visual problem:** commit (`gpu/vk/vk_texture.cpp` + `gpu/vk/target_pool.h`) and push.
  - **Worse, or a visual problem:** `git checkout gpu/vk/vk_texture.cpp`, delete `target_pool.h`, then restore the smoke `*.bak_pre_pool` files.

### Parked: the getenv cache in `vk_mvec.cpp` `env_on()`

- **The change:** set aside (not in the working tree), kept as `work/envcache.patch`. Re-apply with `git apply work/envcache.patch`.
- **Unsettled:**
  - The first A/B said +3.3% slower, but antivirus USB scanning every 7 s was disturbing runs then; the user has since fixed it.
  - A Mask of Truth sample later said faster: 22.4 vs 25.3 ms a frame.
- **To settle it:** a full clean 6-run A/B (`-Tag envfix3`; DLLs `dll_envfix.dll` vs `dll_ritocount.dll` in the bench folder).
  That run was stopped after run 1 at the user's request.

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

1. **Act on the user's target-pool test** (§2).
2. **Settle the getenv cache** with a clean A/B, or drop it (§2).
3. **Longshot ports worth doing**, all hand ports:
   - **#231 flat per-draw caches** (`3d880bf6`): +3.4 fps in one of two configs on an RTX 5090.
   - **#590 three replay costs** (`6109660c`): vertex-binding ranges, a lock per sampled texture. Its mirror bookkeeping part may
     not apply: NVIDIA here uses mirrors.
   - **#569 renderer PGO** (`4e80ea63`): 3-5% renderer time. Needs our own play profile for `mastersword_vk.dll`.
   - **#588 render thread** (`82886e89`) + **#838** (`343a1dfa`, don't pin it to one core): a big change. It replays frame N
     while the game builds N+1. It needs care with our readback path, because the game waits on a readback every frame (§3).
   - **#38 fast float regions** (`02608169`, `nx2/fastfp.py`): a lift change, so a full re-translate. It overlaps Biggoron's
     Forge's inline float fast paths, so measure against Forge, not against plain.
   - Already covered, no port: **#766 copy lock** (our sharded live-handle set is stronger).
4. **The readback on the critical path** (research doc §12): narrowing the readback refresh, and barrier precision. **Fidelity risk:**
   only with the user watching the picture.
5. **Release:** when enough has landed, follow `docs/RELEASE_HANDOVER_0.1.9d.md` (ship set, Pearl Abyss-style notes, publish on
   `mastersword` only). The clean icons are not in a published zip yet.

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
