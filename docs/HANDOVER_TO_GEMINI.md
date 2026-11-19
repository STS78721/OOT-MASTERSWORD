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

- **Build rule (critical):** Always build via `work\build_full.bat` (`RX_RELEASE=1`), which links a Windows GUI executable (PE subsystem `2`), writes `hookshot_log.txt`, and enables release shader precompile. Direct invocations of `nx2\build.py` without `RX_RELEASE=1` produce a console exe (subsystem `3`) that writes no log. Keep `HOOKSHOT_SHADER_STAMP` untouched at `0x4eae666d8f924912ull`.

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

## 2. Current state (2026-11-19)

- **Released — `0.2.0` (`STS78721/OOT-MASTERSWORD` on GitHub):**
  - Ported 25 `MagicBeanDev/Longshot` PRs up to `544229a2` (`#231`, `#590`, `#621`, `#661`, `#552`, `#442`, `#994`, `#1255`, `#1222`, `#1091`, `#384`, `#672`, `#568`, `#553`, `#554`, `#1251`, `#1248`, `#1244`, `#311`, `#788`, `#1083`, `#468`, `#469`, `#431`, `#440`, `#670`, `#769`, `#1256`, `#594`, `#488`, `#1167`) while keeping **`RX_VK_DEAD_MIN_UNUSED=3`**, 96 MB pressure pool floor, 7.4 GB program cache stamp, ABI `10/11`, and all 91 lifted chunks untouched.
  - Retargeted `host/hos_update.c` and `launcher/MasterSwordLauncher.py` to public GitHub (`STS78721/OOT-MASTERSWORD`, `3cbcca3f`) and landed llvm-mingw / CI portability fixes (`c62897fe..6277b730`). Release handover: `docs/RELEASE_HANDOVER_0.2.0.md`.
- **Released — `0.2.1` (`mastersword-fg`):**
  - **Selective submit-time capture & sync (`RX_VK_CAPTURE=2` default):** captures 2D/compute small buffers + tracked readback pages (`RX_VK_CAPTURE_COMPUTE=1`), pairs with `RX_VK_SHADOW_BASELINE=1` in `mirror_sync()` + `shadow_written_collected()` guard + sky light accumulator reset before compute dispatches, retired-mirror clean-page inheritance (`gpu/vk/mirror_inherit.h`), and GPU-adaptive `p90` capture late-page wait cap (`RX_VK_CAPTURE_WAIT_MAX_MS=20`, `RX_VK_CAPTURE_WAIT_ADAPT=1`).
  - **Vulkan Renderer PGO (`RX_VK_PGO`):** `host/pgo_rt.c` + `nx2/build.py` PGO support (excluding `eden/`, `sirit/`, and `vk_shader.cpp` so the 7.4 GB shader cache stamp stays untouched); `mastersword_vk.dll` compiled with MasterSword-native `work/pgo/mastersword_vk.profdata` (`3,215` functions, `24,477` blocks, `76.08B` counts).
  - **Threaded rendering (`Auto` on >=12 logical CPUs):** `gpu/vk/threaded_rendering.h` + `vk_replay.cpp` + `host/nvn_native.c` (`rx_option 0x4D5301`).
  - **Native zstd & sorted-container name searches:** `host/zstd_native.c`, `host/names_native.c`, and `nx2/oot_patches.py` (with virtual-core polling and stuck-load watchdog).
  - **Renderer throughput & stability:** off-thread FIFO resource reaper (`gpu/vk/vk_vma.cpp` `reap()`, `vk_texture.cpp`), finalized GPU-drawn texture retention (`RX_VK_KEEP_DRAWN=1`) & loss tracking, direct uniform buffer snapshotting (`gpu/vk/direct_ub_snapshot.h`), shared reader-lock pipeline lookup (`RX_VK_PIPE_SHARED=1`) & per-program pipeline ownership (`RX_VK_PIPE_BY_PROGRAM=1`), single TLS `dw()`/`pass()` lookups, lockless `queue_flush()` fast path, `targets_redrawn`, `guarded_compute` retry, `mesh_conv_n`, 512 MB managed-pagefile reserve, AMD 85% idle import give-back & `RX_VK_IMPORT_DIAG`, RTX 50 water hang / white-Link volume-box fix (`RX_VK_FOG_BOXES=1` opt-out), and Windows `ABOVE_NORMAL_PRIORITY_CLASS` default (`HOS_PROCESS_PRIORITY=1`). Release handover: `docs/RELEASE_HANDOVER_0.2.1.md`.

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

1. **Completed in `0.2.1`:** Renderer PGO (`RX_VK_PGO`), threaded rendering (`Auto`), selective submit-time capture (`RX_VK_CAPTURE=2`), native zstd/name-search loops, off-thread FIFO reaper, shared pipeline cache locks, direct UB snapshotting, and RTX 50 water hang fix (`docs/RELEASE_HANDOVER_0.2.1.md`).
2. **Remaining Longshot leads (optional / next cycle):**
   - **#38 fast float regions** (`02608169`, `nx2/fastfp.py`): a lift change, so a full re-translate. Overlaps Biggoron's Forge's inline float fast paths, so measure against Forge, not against plain.
   - Check post-`544229a2` `longshot/main` commits (`a7621bd7` DLAA before bloom/tone map `#1652`, `#1650` RDNA2 mesh fallback, `#1444` frozen Zoras above 60 fps) as needed.
3. **The readback on the critical path** (research doc §12): narrowing the readback refresh, and barrier precision. **Fidelity risk:** only with the user watching the picture.

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
