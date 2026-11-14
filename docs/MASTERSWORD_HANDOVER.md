# MasterSword — Full Handover (2026-11-14)

MasterSword continues NotRexed's **Hookshot**: a static recompilation of Ocarina of Time (Switch 2), with ARM64 lifted to C, a
Horizon OS host, and NVN replayed on Vulkan 1.3. Hookshot upstream is gone. Its maintained successor is **Longshot**
(MagicBeanDev), which we track and port from. This file covers what lives where, what was found, what changed, and what is
still open.

---

## 1. Locations

| What | Where |
|---|---|
| **Active source (work here)** | `C:\Games\Hookshot - NotRexed - Test e400f88`, branch **`mastersword-fg`** (a git worktree) |
| Main checkout of the same repo | `C:\Games\Hookshot - NotRexed` (branch `main` = old upstream `137c4235`; leave it) |
| Older worktree | `C:\Games\Ocarina of Time PC\Hookshot - NotRexed - Test e400f88` (`hookshot-main-fixes-pc`, historical) |
| **Game run folder ("smoke")** | `C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b` |
| Logs | smoke folder: `hookshot_log*.txt`, `present_trace.csv` (with `RX_VK_SLOW_LOG`) |
| Runtime test switches | smoke folder: `hookshot_env.txt` (NAME=VALUE, `#` comments; path values rejected except `RX_VK_DLSS_DIR`) |
| User options | smoke folder `.ini` (e.g. `vsync`, `hybrid_cpu`); **personal, never ship** |
| CPU Lab (APO/core-placement experiments) | smoke folder `cpulab\` (+ desktop shortcut); source `tools/cpulab/`; guide `docs/CPU_LAB_HANDBOOK.md` |
| Release prep | `runs/_release_prep/` (patch notes, README, triage, stage/upload script) |
| Offline backups | `C:\Games\MasterSword-Backups\2026-11-13_0.1.9c\` (full-history git bundle, source zip, SHA256SUMS) |
| Snapshot branch | `backup/mastersword-0.1.9c-2026-11-13` |

### Remotes (in the active worktree)

| Remote | URL | Use |
|---|---|---|
| `mastersword` | https://github.com/STS78721/OOT-MASTERSWORD | **Ours.** `main` = `45fb4761` (0.1.9c full source), `mastersword-fg` = `eebd3a76` |
| `longshot` | https://git.magicbean.dev/MagicBeanDev/Longshot | Upstream successor, read and port only (`longshot/main` = `514591a3` when fetched) |
| `origin` | https://github.com/NotRexed/Hookshot | Dead upstream, history only |

**Not pushed yet:** 7 local commits on `mastersword-fg` past `mastersword/mastersword-fg` (the Longshot ports in §4 and the
small-reads restore). Push when ready: `git push mastersword mastersword-fg`. Never force-push `main`.

---

## 2. Build, test, deploy

Environment (cmd): `vcvars64` (VS 18 BuildTools), then
`RX_CPM=C:\Users\<you>\.codex\worktrees\oot-main-candidate\OOT EMU\.cache\cpm`, `RX_RELEASE=1`,
`HOOKSHOT_NGX_SDK=<tree>\work\ngx-sdk`, `HOOKSHOT_FSR=1`, then `python -u -X utf8 -I nx2\build.py --standalone --jobs 12`.

- **Renderer only** (`gpu/vk/*`): add `RX_VK_ONLY=1` (~10–30 s). Output: `work\sa\build\mastersword_vk.dll`.
- **Full** (`host/*.c`, lifted game code): without `RX_VK_ONLY`. Output: `work\sa\build\mastersword.exe` (~400 MB).
- **Game patches** (`nx2/oot_patches.py`) are applied when the game is lifted, not at compile time. To apply a new frame-counter
  site to the already-lifted C without a re-lift, run only `patch_whole_frames` over `work/sa/gen/code_*.c`, which skips sites
  it has already patched. Back up the files first, as was done in `work/sa/_gen_bak_frame_whole/`.
- **Tests:** `python -I -B tests/run_all.py`. Current result: **151 passed / 107 failed / 32 skipped** (baseline at 0.1.9c was
  147 / 107 / 32). Every pre-existing failure is triaged in `runs/_release_prep/TEST_TRIAGE_2026-11-13.md` as a lost fix, a
  stale test or the environment. `tests/test_vk_occlusion_queries.py` is stale: it looks for a mesh-fallback block this tree lacks.
- **Deploy:** back up the smoke `mastersword.exe`, `mastersword_vk.dll` and `hookshot_vk.dll` as `.bak_<label>`. Copy the new
  DLL to **both** names, then check the hashes.
- The repo rules are in `runs/_release_prep/STICKY.md` and `.cursor/rules/mastersword-smoke-discipline.mdc`: keep diffs small,
  back up before deploying, and leave `vk_menu.cpp` alone unless asked.

### Hard rules

- No game files, or anything made from them (lifted C, dumps, shader caches, saves), leave the PC.
- Releases never carry personal settings (`.ini`, `.json`, caches, saves, NGX binaries).
- Fresh-install defaults stay safe at 1080p.
- Don't touch Intel DTT or `apo.config`, and don't change Windows settings for the user.
- Never force-push `main`.

---

## 3. Findings: performance and stutter (chronological, what held)

### Big wins (kept)

1. **Parallel replay**: the renderer records one frame's draws on several threads. Measured upstream: 10.9 → 7.75 ms a frame.
   It now defaults to **auto** (`RX_VK_PARALLEL` unset or `auto`): with n fast cores, n≥6 gives clamp(n−4, 2, 4) threads, and
   the 270K Plus gets 4. It was off because Kokiri went dark. The cause was a lost safety rule (no GPU copy of a memory pool
   inside a parallel segment, the `!par` guard), now restored.
2. **Hybrid-CPU placement (Intel Hybrid CPU Patch, F1 menu, Graphics tab)**: the frame threads and all game threads go on
   **P-cores**, minus one core reserved for the replay. The background shader and pipeline workers go on **E-cores**, EcoQoS is
   off for the parallel workers, and the replay thread is pinned (ReplayPin, restored from upstream `3241a6c6`). It is
   "On (Recommended)" for 12th/13th/14th gen Core and Core Ultra. This removed nearly all of the graphical glitches and much of
   the stutter, which suggests the Switch-2-style 6-vcore layout maps well onto P-cores.
   - Switches: `HOS_P_CORES` (overrides detection), `HOS_GAME_P_CORES=0` (frame threads only), `RX_VK_REPLAY_PIN=0`,
     `RX_VK_JOBS_ANY_CORE=1`. ini key `hybrid_cpu`.
   - User CPU: Core Ultra 7 270K Plus. P-cores are logical 0, 1, 10–13, 22 and 23; E-cores are 2–9 and 14–21.
3. **VRAM**: dropping the VMA image-pool prealloc when compaction or target blocks are on freed about 1.6 GB.
4. **BC1 path**: BC1 vs BC3 is now decided at create() from the guest's ASTC blocks, not by re-making the image at first upload,
   which cost 19–352 ms per texture at the driver's limit. The scan reads guest bytes, not write-combined staging memory.
5. **Restored lost upstream fixes**: the pipeline `ready` flag on job push, `!par` in mirror_make, async import (NVIDIA
   default), small reads served from a stream copy while their pool imports.
6. **Windows Game Bar → "Remember this is a game"**: the user found this made the game "insanely smooth" (Intel APO and
   scheduler treatment). The launcher shows a one-time how-to before first launch (`game_bar_hint_shown`).

### Tried and reverted (don't retry without a new idea)

| Experiment | Result |
|---|---|
| 16 MB import chunks | Worse: mirrors went over budget. Back to 4 MB (16 without mirrors). |
| Old-area clear on area change | Tiled black sky. Now opt-in only (`RX_VK_OLD_AREA_CLEAR=1`). |
| NVIDIA chunk prefetch | No gain, more merges. Default 0. |
| Harder render-target pool trim | Reuse fell from 45% to 22%. Reverted. |
| Async replay `RX_VK_ASYNC=1` | White flashes and a lower frame rate. Off. |
| `RX_VK_BG_IMPORT=1` | Hung at "preparing shaders": the Resource Memory thread stalled 2 s at a time. Off. |
| `RX_VK_IMPORT_SPAN_MB=8` | No help. The cost is not size-driven. Off. |

### What remains (the last small hitches)

- **On-demand pool imports** (13–97 ms) inside parallel segments and written bindings. Each import costs Windows'
  commit/VirtualQuery on write-watch memory (6–15 ms) plus the driver's page pinning (7–17 ms), whatever its size. The log line
  `vk: a binding waited X ms for its pool's import` names them. Ideas: import pools ahead from the game's allocation pattern
  (not on start-up, where it hangs), or use a non-write-watch allocation for pools the GPU reads.
- **V-Sync**: with V-Sync On or Mailbox, `vkQueuePresentKHR` can block while it holds `dev.present_mu`. **Turning V-Sync off
  removed nearly all stutter.** The smoke env has `RX_VK_PRESENT_QUEUE=1` (presents on their own compute-family queue).
  Upstream measured frames over 22 ms falling from about 13% to about 1%, but left it off because of a rare device-lost
  (issue 1161). **Still to test: V-Sync Mailbox + `RX_VK_PRESENT_QUEUE=1`.** The user's last run used V-Sync Off (`vsync=2`).
- Visual issues seen (performance came first): duplicate flickering items, an off-colour skybox and Link at Hyrule Castle. The
  shadow-map port (§4) may help some of the dark or green blocks.

### Intel APO research

APO "Advanced Settings" whitelisting is governed by Intel's DTT/APO config (signed, per-title). We don't tamper with it.
Instead the **CPU Lab** (`cpulab.exe` + .bat shortcuts) lets the user try their own affinity and priority rules on the game's
threads, opt-in and reversible. The handbook is written so another assistant (e.g. Gemini) can pick it up.

---

## 4. Longshot ports (2026-11-14, local, not pushed)

| Longshot | Local | What | Notes |
|---|---|---|---|
| `fbd42024` (#198) | `dbdb21aa` | A draw takes its pass and descriptor writer once | Per-draw CPU cut |
| `cd906265` (#197) | `f25d29f3` | Occlusion reports get the GPU's real counts (`RX_VK_OCC_QUERIES`) | The game culls what is hidden |
| `f3fe1763` (#260) | `8c4357f8` | Occlusion counts stay "visible" unless known | Adapted: no mesh-fallback task batching here |
| `08771847` (#78) | `264f5132` | Scarecrow's Song recording and playback timing above 60 fps | 3 frame-counter sites; tests adapted to `hos_frame_whole(u64,u64)` |
| `9f64464b` (#49) | `264f5132` | Deku Tree floor web reacts above 60 fps | 1 frame-counter site; applied to the lifted C in place |
| `6ce8ca3a` (#2226) | `132ded3b` | Big ASTC textures as **BC7**, opt-in `RX_VK_ASTC_BC7=1` | Same VRAM as BC3, better gradients. With it on, BC1 is skipped. Commented line in `hookshot_env.txt` |
| `7315957c` (#107) | `5634ba55` | Clustered lights sample their own shadow maps (handle tables as descriptor arrays) | Fixes dark stair-step blocks on Zora's ice/water and green blocks on snow. `RX_VK_HANDLE_TABLES=0` turns it off. **Shader cache stamp bumped ("HT01"): one longer "preparing shaders" on the next launch** |

| `ae96bad3` (#234) | `f1ee63a7` | **Quick pipelines**: `RX_VK_QUICK_MS=8` (off unless set; **on** in the smoke env). A draw whose pipeline is still compiling gets one built without the driver optimizer (into no cache), not skipped or waited on | Ported without Longshot's AMD driver-fault gate and cache-only ask (not in this tree). Own test: `tests/test_quick_pipelines_ms.py` |
| `dfc8fa5e` (#2787) | `e9be4a9f` | **DLSS / FG ghosting**: motion vectors for rigid one-bone models (Navi, props, held items), two-bone skinning (Navi's wings) and Link's fifth-bone single influence | Merged into this tree's `temporal_skin_check` |

Checked and already in this tree: `5cbfead0` reactive mask, `3319e88a` DLSS mip bias, `549a43b5` DLSS kept through
loading screens, `8b925b67` skinned-draw actor motion, `cd3d5044` DLSS failure latching. Still to port on the FG/DLSS side,
all small and not ghosting fixes: `1bf4284a` (FG uses the multiplier its sets were made for), `ee29dd55` (V-Sync Auto with
DLSS-G), `6b2dafef` (DLSS preset settle), `f3dd4486` (DLAA switch waits for the native scene).

**Frame-rate question (2026-11-14):** the first run after the ports read about 4–5 fps lower in heavy scenes (53–54 vs
56–59). The renderer's CPU per frame was unchanged (submit 8–10 ms), GPU-bound wait was 0, and runs from before the ports
already varied 52–62 by area, with V-Sync modes switched mid-run. An A/B is set up in `hookshot_env.txt`: the same spot, with
and without `RX_VK_OCC_QUERIES=0`. If occlusion queries are not the cause, try `RX_VK_HANDLE_TABLES=0`, which means one
re-translation of about 140 s.

Deployed to the smoke folder on 2026-11-14 (exe + both DLLs). The pre-port backups are `*.bak_pre_longshot_ports`. **Not yet
played**, so the first run should check: shader preparation finishes, Zora's Domain ice and water, Kokiri lighting, the frame
rate, and occlusion culling (nothing popping or missing).

---

## 5. Releases

- **0.1.9c**: released on GitHub with zips and the launcher, in Pearl Abyss patch-note style
  (`runs/_release_prep/PATCH_NOTES_0.1.9c.md`). Contents: the Intel Hybrid CPU Patch toggle, parallel replay by default, the
  VRAM and BC1 fixes, the restored upstream fixes, and the launcher's Game Bar hint.
- **Next (0.1.9d candidate)**: the §4 ports. Before release: a play test, push `mastersword-fg`, merge into `main` (merge
  commit, no force), patch notes in the same style, bump `VERSION` and `launcher/MasterSwordLauncher.py` `CURRENT_VERSION`,
  stage with the `stage_and_upload` script pattern, and make a new backup bundle in `C:\Games\MasterSword-Backups\<date>_<ver>\`.

---

## 6. Key code map

| Area | File |
|---|---|
| Memory imports, mirrors, async/bg import, stream_copy reads | `gpu/vk/vk_memory.cpp` |
| Textures: ASTC → BC1/BC3/BC7, compaction, pool trim, zero-check | `gpu/vk/vk_texture.cpp` |
| Draws, occlusion guard, descriptors, handle tables, fine profiler | `gpu/vk/vk_draw.cpp` (+ `vk_draw.h`) |
| Replay, parallel segments, fast cores, worker placement, ReplayPin | `gpu/vk/vk_replay.cpp` |
| Job workers (E-core placement) | `gpu/vk/vk_jobs.cpp` |
| Shader translation host side, cache stamp (`disk_stamp`) | `gpu/vk/vk_shader.cpp`; recompiler in `gpu/vk/eden/shader_recompiler/` |
| VMA pools | `gpu/vk/vk_vma.cpp` |
| F1 menu (Intel Hybrid CPU Patch row) | `gpu/vk/vk_menu.cpp/.h` |
| Thread pinning (P-cores), `hos_fast_cpus` | `host/hos_thread.c` |
| Frame-step fractions | `host/frame_whole.h`, `host/nn_vi.c` (`hos_frame_whole`) |
| Game patches at lift time | `nx2/oot_patches.py` (`WHOLE_SITES` etc.) |
| Launcher | `launcher/MasterSwordLauncher.py` |

Profilers: `RX_VK_PROF_FINE=1` (per-draw sections), `RX_VK_SLOW_LOG=<ms>` (frames over N ms, plus `present_trace.csv`), and
the OpClock per-NVN-op report in `vk_replay.cpp`.

---

## 7. Open items, in priority order

1. Play-test the 2026-11-14 build (§4 checklist).
2. Test V-Sync Mailbox + `RX_VK_PRESENT_QUEUE=1` and compare with V-Sync Off. If it holds, consider making it the default when
   V-Sync is on.
3. Attack the remaining pool-import hitches (§3).
4. Try `RX_VK_ASTC_BC7=1` for fidelity and watch VRAM.
5. Push `mastersword-fg`, then prepare 0.1.9d.
6. Run the frame-rate A/B (§4) and keep or turn off whichever port costs fps.
7. Visual bugs: duplicate items flickering, skybox and Link colour at Hyrule Castle.
8. Keep watching Longshot (`git fetch longshot`, then `git log mastersword-fg..longshot/main`) for perf and fix commits.
