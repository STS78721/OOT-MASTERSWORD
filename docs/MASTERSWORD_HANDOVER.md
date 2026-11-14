# MasterSword — Full Handover (2026-11-14)

MasterSword continues NotRexed's **Hookshot**: a static recompilation of Ocarina of Time (Switch 2), with ARM64 lifted to C, a
Horizon OS host, and NVN replayed on Vulkan 1.3. Hookshot upstream is gone. Its maintained successor is **Longshot**
(MagicBeanDev), which we track and port from. This file covers what lives where, what was found, what changed, and what is
still open.

---

**Companion document:** `docs/MASTERSWORD_TECHNIQUES.md` is the playbook of MasterSword's techniques (hybrid-CPU
placement, parallel replay sizing, memory census, the memory pack, resident floor, import ahead), written so other games
and recompilation projects can implement them, with what came from Hookshot and what is MasterSword's own. Keep it in
step with this file.

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

**Pushed 2026-11-14:** `mastersword-fg` on GitHub is at `b8408cd2` (33 commits: the Longshot ports, the memory pack,
Triforce Shader Delivery, Lens of Truth, the Sheikah Slate, the Owl Statue, Temper 1, and all the docs). Push later work the
same way: `git push mastersword mastersword-fg` (fast-forward only; never force-push). `main` is still 0.1.9c (`45fb4761`).

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

### Findings 2026-11-14 (user tests)

- **Ghost clones with DLSS frame generation come from Upscaling = Off.** In that mode DLSS-G gets the renderer's own
  depth/motion (camera reprojection + actor motion only), and characters it can't follow leave a copy. With **Upscaling = DLSS
  SR** the clones are gone. Recommended: DLSS SR with frame generation (DLAA quality keeps native resolution if fidelity is the
  goal). `RX_VK_ACTOR_LOG=1` logs the draws actor motion turns away (mostly static scenery; some mesh-shader draws with large
  palettes, likely crowds/foliage, which actor motion cannot follow).
- **NVIDIA Profile Inspector → "Present Method (Vulkan/OpenGL)" = "Preferred layered on DXGI Swapchain"** gave a small fps gain:
  Vulkan presents go through a DXGI flip-model swapchain (independent flip, Windows' game presentation path). It is a driver
  profile setting the user sets; we do not change it for them. Worth a line in the README/launcher tips.
- **The fps drop is not the occlusion queries**: with `RX_VK_OCC_QUERIES=0` four runs still read 50–53 fps. Pre-port runs
  already ranged 52–62 by area. Remaining suspect: handle tables (`RX_VK_HANDLE_TABLES=0`, costs one ~140 s shader
  re-preparation); the clean test is the same spot with the `*.bak_pre_longshot_ports` DLL.

### Findings 2026-11-14 (later): the remaining stutters are RAM pressure

- The memory-pressure log (`vk: memory pressure:` lines, every 30 frames when page faults spike or free RAM < 1.5 GB) shows
  the PC's free RAM falling from 8.7 GB to ~1 GB over a session while the game's working set grows 14 -> 22.6 GB (private
  commit 27 -> 40 GB; the PC's commit 54 of 80.5 GB). 32 GB PC.
- Two kinds of stall follow from it: (1) **pool imports slow from 13-17 ms to ~58 ms each** (the driver pins pages and
  Windows must evict first): a stream of new 4 MB GPU-written pools in NPC-heavy areas, one a frame, 113 in one session
  (2.4 s); (2) game-thread stalls with an idle renderer next to page-fault spikes (Windows paging the game's memory back).
- The A/B (pre-port vs current renderer) is inside this noise: A 54.6-56.7 fps, B 53.3 (B's runs were the first after a
  switch, cold shader cache, and one had a costly diagnostic on). The ports are not the stutter source.
- Next: a memory diet (measure the host-side consumers: program records in RAM (44k programs after the switches), captures,
  shadows, staging, texture copies; cut what is cold), and keep imports off the frame when free RAM is low. Player side:
  close heavy background apps.

### The memory work, 2026-11-14 (new to MasterSword: Hookshot and Longshot keep every shader module)

**Why:** the remaining streaming stutters were RAM pressure (above). The census showed where the RAM went.

1. **Memory census** (`gpu/vk/vk_mempack.cpp`, `RX_MEM_CENSUS`, default every 3600 frames on a worker, 0 off):
   `vk: memory census` lines split the working set into the guest window (the game's own memory), host private (renderer,
   driver, translator), mapped files and code, list the 10 largest host allocations, and count the SPIR-V handed to the
   driver. Plus `vk: memory pressure` lines (every 30 frames when page faults > 150k or free RAM < 1.5 GB) and free RAM /
   commit / page faults on the `vk: memory: working set` line (`vk_texture.cpp`).
   First census: working set 22.3 GB = guest 9.2 GB + host private **12.8 GB**; **194,425 shader modules, 13.5 GB of SPIR-V**
   kept by the driver for the session.
2. **Resident floor** (`RX_MEM_RESIDENT_MB`, default 55% of RAM, max 18 GB; 0 off): `SetProcessWorkingSetSizeEx` with a
   *soft* minimum (`QUOTA_LIMITS_HARDWS_MIN_DISABLE`), so Windows trims other programs before the game. Soft: it can't
   starve the PC.
3. **The memory pack** (`RX_MEM_PACK`, on where the device has the graphics pipeline library; 0 = modules as before):
   - A stage's SPIR-V goes to `mastersword_spirv.pack` (+ `.idx`: 16-byte records, hash and place/size) in the game folder,
     content-addressed: the 194,425 stages were **50,812 distinct codes, 3.6 GB on disk**. It stays between runs, so a warm
     start writes nothing new.
   - No `VkShaderModule` is kept. `Stage` holds `code_at`/`code_words` (`vk_shader.h`); `stage_set_code()` writes it,
     `stage_bind()` reads it back with a positional `ReadFile` and chains a `VkShaderModuleCreateInfo` on the stage's
     `pNext`, after `pin_subgroup`. Used at every program pipeline build: `build_graphics`, the program compute stages,
     `warm_compute`, the prepare's compute builds, the dispatch build and its `info` lambda (the caller owns the
     `StageCode`, so the code outlives each create).
   - Reads go through Windows' file cache: free RAM keeps the hot code, and Windows drops it at once under pressure (clean
     file pages, nothing to the pagefile). A failed write falls back to a module (`make_module_counted`).
   - Result: shader code held by the driver 13.5 GB -> **0**; host private 12.8 -> 8.0 GB; working set 22.3 -> 17.4-17.8 GB;
     free RAM late in a run ~1.2 GB -> **~5 GB**; the 2-second stall bursts **gone** (0 in the warm run); 0 read failures.
4. **Import ahead in play** (`RX_VK_BG_IMPORT=play`, on in the smoke env; `RX_VK_BG_IMPORT_FROM` frames, default 1800):
   the hitches left after the pack are ~110 imports a session at ~34 ms (slow even with RAM free), nearly all a new
   pool's first 4 MB bound by the next frame's draws. In play only, `memory_pool_created` queues that first chunk on the
   import thread (at most 16), which now runs below normal priority on the E-cores. `RX_VK_BG_IMPORT=1` (every whole pool
   from the start) is still the start-up hang; don't use it. **Not yet play-tested.**

**A/B kit** (`smoke\AB_test\`): `Use A - before ports.bat` / `Use B - current.bat` swap the renderer DLL (game closed);
`Done - file the last log.bat` files the last run's log as `log_<A|B>_<time>.txt`. B is the current build. A run right
after a switch re-prepares shaders: compare warm runs only. Backups of each B are `B_current.dll.bak_*`.

**NVIDIA settings (user tests):** Resizable BAR is active (nvidia-smi BAR1 16384 MiB); the renderer's per-frame streams
already land in VRAM through it (VMA host-visible sequential-write). NVPI's ReBAR profile flags are a DirectX whitelist:
no effect expected on Vulkan. "Present Method (Vulkan/OpenGL) = Layered on DXGI Swapchain" gave a small fps gain. Shader
Cache 100 GB / Pre-compile High: fine. PrimoCache would not help (a disk block cache; its RAM cache takes RAM).

### If this work stops halfway (how to pick it up)

- Read this file, then `git log --oneline -25` on `mastersword-fg` in the active worktree: every step is its own commit
  with the why in the message.
- The newest smoke log is `hookshot_log.txt`; the lines that matter are `memory census`, `memory pack:`, `memory pressure`,
  `imported in`, `a binding waited`, `slow frame`, and the `presents, ... fps` lines.
- Every switch named here goes in the smoke folder's `hookshot_env.txt`; the file says what each line was for and its result.
- Build/deploy recipe: §2. The memory file `mastersword-build-workflow.md` (Claude's memory) has the same.

### MasterSword Triforce Shader Delivery and the import-ahead result (2026-11-14, later)

- **Import ahead (`RX_VK_BG_IMPORT=play`) failed; it's off.** Run 10:38: only 32 of ~380 chunks were imported ahead in
  time (351 dropped), and the background page pinning slowed the game's own imports (avg 34 -> 43 ms, worst 51 -> 290 ms).
  Loads took much longer, with big stutters after them. Commented out in `hookshot_env.txt`. Don't retry it in this form.
- **The pack had a load-time cost.** Every pipeline build read its shader back and the driver parsed it again: 53,000 reads,
  3.4 GB a session. The area changes' slowest frames spent 924 ms in pipelines against 417 ms before the pack.
- **Texture re-encodes are cheap** (3 slow uploads, 127 ms a session), so a BCPack-style texture cache isn't worth it now.
- **MasterSword Triforce Shader Delivery** (`gpu/vk/vk_mempack.cpp`, commit 724087f9), on by default:
  - **Power: the pack.** As before (`RX_MEM_PACK=0` off).
  - **Wisdom: the hot cache.** Modules of recently used shaders are kept, up to `RX_TRIFORCE_HOT_MB` (default 1024; 0 none),
    in an LRU of the modules no build holds. `StageCode` holds a reference (`hot_key`) and releases it in its destructor
    (`triforce_release`), so a module is never destroyed during a create. A miss reads the pack, makes the module outside
    the lock, and keeps the other thread's module if one raced in.
  - **Courage: the warm-up.** When the pack opens, a background-priority thread (`THREAD_MODE_BACKGROUND_BEGIN`, E-cores, its
    own `FILE_FLAG_SEQUENTIAL_SCAN` handle) reads the pack front to back in 8 MB reads into Windows' file cache, only when
    free RAM is at least twice the pack's size (`RX_TRIFORCE_WARM=0` never).
  - Census lines to check: `Triforce hot cache: N modules (X MB of 1024), H builds served from it, M made, E evicted`, and
    `Triforce Shader Delivery: warm-up read ...`.
  - **Play-tested 2026-11-14 10:52, many area changes back and forth. The user: "that made a huge difference".**

    | | Before the pack (A) | Pack only (10:21) | Import-ahead (10:38) | Triforce (10:52) |
    |---|---|---|---|---|
    | Avg fps, busy scenes | 54.6 | 50.5 | 46.6 | 53.5 |
    | Time at 59-60 fps | 17% | 13% | 2% | **34%** |
    | Area-change spike frames | 3 (pipelines 417 ms) | 4 (924 ms) | 18 (1024 ms) | 3 (620 ms), over more changes |
    | Frames over 100 ms | 2 | 3 | 26 | 5 |
    | Pack reads | n/a | 53,300 (3.4 GB) | similar | 24,580 (1.7 GB) |

    Hot cache: 28,774 builds served, 19,834 modules made, 4,992 evicted, holding 14,842 modules (1,023 of 1,024 MB). The
    warm-up read the 3.6 GB pack in 2.06 s at background priority. 0 read failures, no missing objects reported.
  - Still open: imports avg 41 ms (max 103) remain the main hitch source; free RAM ended at 2.5 GB (the hot cache's 1 GB
    and more areas visited), so if pressure returns, try `RX_TRIFORCE_HOT_MB=512`.

### Naming golden rule (the user's)

Everything MasterSword designs is named with a Zelda reference, under the MasterSword / SteelTempleSaint banner (the code
comments, log lines, switches' docs and these documents). Upstream Hookshot/Longshot work keeps its own names and credit.

### MasterSword Lens of Truth, and the Triforce upgrades (2026-11-14, latest; not yet play-tested)

- **Where an import's time went** (the `vk: import N ms: commit+properties A, buffer 0, allocate B, bind 0` lines): about half
  was `VirtualQuery` in the 512 GB write-watched reservation (6-28 ms: Windows walks the page tables to the end of the
  region's run), the other half the driver pinning the pages (`allocate`, 5-35 ms).
- **MasterSword Lens of Truth** (commit 50e7f42b, **exe and DLL**): the host keeps a bitmap, one bit per committed 64 KB
  block of the guest window (`host/mkw_glue.c`: set in `hos_commit` and the first-touch handler, cleared by
  `hos_commit_forget` before the two decommits in `hos_mem.c` and `hos_svc.c`). It exports `hos_guest_committed(lo, hi)`;
  the renderer's `import_pool` finds it with `GetProcAddress` and skips `VirtualQuery` when it answers 1. There's no
  interface change: an older exe or renderer just falls back. Log: `MasterSword Lens of Truth: ... on`, and every 64 imports
  `Lens of Truth: N imports answered by the commit bitmap, M asked Windows`. Off: `RX_VK_COMMIT_BITMAP=0`. Expected: the
  `commit+properties` half gone; the driver's pinning half remains.
- **Triforce upgrades** (commit 123351be):
  - **Wisdom, adaptive:** the hot-cache budget follows free RAM every 300 frames (2 GB with > 8 GB free, 1 GB > 4,
    512 MB > 2, else 256 MB), evicting at once when it shrinks. `RX_TRIFORCE_HOT_MB` fixes it.
  - **Song of Time:** the hot set is saved newest-first to `mastersword_spirv.song` every 1800 frames, and replayed at
    the next start by an E-core thread (up to 90% of the budget). `RX_TRIFORCE_SONG=0` off.
  - **Mirror Shield:** each pack record is hash-checked the first time it's read in a session. A mismatch is never handed
    to the driver; it writes `mastersword_spirv.damaged`, and the next start deletes the pack, index and song and rebuilds.
  - The census line now ends with `Song of Time made N ready; Mirror Shield checked M codes`.
- **The pack's 3.6 GB isn't a limit:** it's every distinct shader the game has. RAM use is governed by the hot cache.

### MasterSword Sheikah Slate: the self-training memory (2026-11-14, latest; not yet play-tested)

The user's request: the system should be self-aware and train itself, keeping snapshots so a cold boot doesn't start over.
Measured first (11:08 run, the first with Lens of Truth): start-up shaders 5.6 s (warm caches), pack warm-up 4 s in the
background, Lens of Truth answered 83 of 128 imports, Wisdom shrank 1 GB -> 512 MB with free RAM, Song of Time 0 (nothing
saved yet).

`gpu/vk/vk_mempack.cpp` (commit 4475506a), `mastersword_slate.bin` in the game folder, saved every 1800 frames and when the
window closes (`slate_close` from `vk_main.cpp`):
- **Scores:** every shader use (hot-cache hit or module made, `slate_note_use` from `stage_bind`, called after `hot_mu`
  is released) is counted; each save gives score = old x 0.75 + this session's uses. Best 65,536 kept; under 0.05 forgotten.
- **Area pictures:** `slate_frame` (each frame, from `texture_frame` in `vk_texture.cpp`, with the texture counter): more than
  8 textures made in one frame means an area streams in, and a picture begins of the codes first used in the next 900
  frames (up to 4096). When an area streams again, its first 24 codes are matched against every picture's first 48; the
  best match of at least 12 makes that picture's remaining codes ready on an E-core thread (`make_ready`). A new picture
  replaces one with half its head in common; 64 pictures at most, the least recently seen dropped. No scene number needed.
- **Wisdom's settled budget** is saved and is the next start's budget.
- **Song of Time** now replays the Slate's best-scored codes at a start (90% of the budget).
- Damaged with the pack: the Mirror Shield deletes `mastersword_slate.bin` with the pack.
- Log lines: `Sheikah Slate: session N; X shaders it knows the worth of, Y area pictures; Wisdom starts at Z MB`,
  `Sheikah Slate: an area streams in; it matches a picture ...`, `Song of Time (...): N shaders made ready ... in M ms`, and
  the census line's `Sheikah Slate made N ready (P area pictures replayed, T taken)`.
- Switches: `RX_SLATE_PICTURES=0` (no pictures), `RX_TRIFORCE_SONG=0` (no replay at start). Delete the file to make it forget.
- To test: 2-3 sessions through the same areas; from the second on, area changes should show picture matches and fewer
  `made` modules in the census.

### The MasterSword Forge & Temper cycle (the working process, from 2026-11-14)

Intel's tick-tock, MasterSword's way. A **Forge** (tock) adds a capability; the next **Temper** (tick) refines what was just
forged: less overhead, more coverage, more robustness. **Every Temper starts from the log's own measurements**, and says so
when something costs too little to be worth touching. Each Temper is one commit titled `MasterSword Forge & Temper, Temper N`
with the measurements in the message, and an entry here.

| Temper | Measured | Changed |
|---|---|---|
| **1** (commit 19317f46) | Census 360-375 ms of a worker + ~90 MB every minute; Lens of Truth missed 45 of 128 imports; the Slate locked once per frame; the Mirror Shield's checks cheap | Census at 600 frames, then every 5 min (`RX_MEM_CENSUS`, default 18000 frames) and once after memory pressure (at most a minute apart), its page list sorted in place (~45 MB less). The Slate takes no lock on idle frames. Lens misses are now split into "gaps in the bitmap" vs "not committed yet", and the first 6 gaps are logged with the first missing block (`Lens of Truth gap N: ...`, `Lens of Truth misses: G gaps, R not committed yet`). Mirror Shield: no change |

| **2** (commit ee15b931, exe + DLL) | Slate scores learned from start-up builds; 0 area pictures in 12 streams; Lens: 0 gaps (nothing to close); Owl Statue: first flush per save 12-14 ms, kept | **The Sheikah Sensor**: each draw bumps its program's counter (relaxed load+store, no locked add, no lock); the Slate is told only at a power-of-two draw count (score = log of draws) or at a program's first draw since an area began streaming. Builds no longer score. Pictures keyed by **program keys** (stable across runs), 16 programs make one, 24 are matched. **SLT2** file with tagged sections (SLT1 still read; its pictures dropped). **Fingerprint** (GPU + driver; a log line when it changed). **Owl Statue awareness**: host exports `hos_owl_busy`; census and Slate saves wait while a save is flushed. Census line adds `Sheikah Sensor: P programs drawn, S score events, B area bursts`. **Not yet play-tested** |

**Temper 3 (next):** from the Sensor's data: are pictures forming and matching, and what do the matches make ready? Then the
Forge of **Farore's Wind** (research doc §5 step 3), switch default off, A/B on a warm and a cold boot.

### Results 2026-11-14 11:08-11:20, the MasterSword Owl Statue, and the Slate research

- **Lens of Truth confirmed:** imports averaged ~22 ms (41 before); on imports the bitmap answered, the Windows half is 0.0 ms.
  Frames over 50 ms: 43 -> 5-13. Temper 1's split: **0 gaps** in the bitmap, all 32 misses were memory not yet committed,
  so the Lens has no holes to close.
- **Sheikah Slate finding:** it learned (`mastersword_slate.bin`, 234 KB), but **0 area pictures** in 12 area streams.
  The start-up preparation builds nearly all pipelines (~26,600 in ~5 s), so play makes too few new builds for a picture.
  The research document `docs/MASTERSWORD_SHEIKAH_SLATE_RESEARCH.md` lays out what it should see instead and how to add that
  without breaking anything.
- **MasterSword Owl Statue** (commit bc02db0a, **exe only**, `host/nn_fs.c`): the player saw a stop at every save. A save's
  `nn::fs::FlushFile` called `FlushFileBuffers` on the game's thread. Now a duplicate handle goes to the Owl Statue thread
  (E-cores, `THREAD_MODE_BACKGROUND_BEGIN`), which flushes and logs the time (`save: MasterSword Owl Statue: a save's
  flush took N ms ...`). Drained at exit (5 s at most); a full queue (64) flushes in place. Slow save writes and renames
  (>= 5 ms) are logged for the next Temper. Trade-off, told to the user: only a power cut or Windows crash within about a
  second of a save could cost the newest save. `HOS_OWL_STATUE=0` restores the old behaviour. **Not yet play-tested.**

### Results 2026-11-14 11:34-11:36: the Owl Statue, Sheikah Slate session 2, frame-generation experiments

- **Owl Statue works.** Each save flushes several files: **the first flush takes 12-14 ms**, the rest 0.7-1.0 ms. That
  12-14 ms used to land on the game's thread at every save (the hitch the user felt); now it runs on the Owl Statue's thread.
  Frames around saves stayed at their usual 20-29 ms. No save write or rename reached 5 ms, so the flush was the only slow
  step. Keep it on.
- **Sheikah Slate, session 2:** `19504 shaders it knows the worth of, 0 area pictures; Wisdom starts at 1024 MB`. Song of
  Time made **6,286 shaders ready in 1.4 s** at start-up, on an E-core thread. Area pictures still 0, as the research doc
  explains (start-up builds nearly everything).
- **Frame generation (11:34 run; the user switched modes in play; different areas per mode, so indicative only):**

  | Generator | Game fps | FG gaps > 25 ms per sample | Present gaps > 40 ms per sample |
  |---|---|---|---|
  | DLSS MFG (NGX) 3x | 53.8 | 1.9 | 2.7 |
  | FSR 3.1 FG 2x | 53.6 | n/a (FSR paces its own frames) | 1.0 |
  | Apex FG (Fast) 3x (stand-in) | 52.8 | 0.3 | 0.9 |

  The game's own frame rate doesn't depend on the generator (CPU-bound). DLSS MFG had the most pacing gaps in this run, but
  its stretch was the longest and included area changes. To settle it: the same route per mode. Image quality (ghosting)
  is best with DLSS SR + DLSS MFG (see the ghosting finding above).

### Run 11:51 (Temper 2's first run): the program cache was rebuilt, a one-time cost (lesson recorded)

- The user saw base fps drop and stay low after the last area change (22-26 fps). Cause: **Temper 2 added two counters to
  `Program` (`vk_shader.h`), and `sizeof(Program)` is part of the program cache's stamp (`disk_stamp`, the ABI words).**
  The 7.4 GB `hookshot_programs.bin` was treated as invalid: `0 program keys kept`, start-up 52.1 s (4.5 s before),
  46,666 shaders translated again, 404 s of translation CPU in total, much of it in play. RAM fell (Wisdom shrank to
  512 MB) and the hot cache thrashed (59,849 made, 54,973 evicted).
- **One-time:** the cache was rebuilt and saved under the new stamp; the next start should be back to ~4.5 s. Not reverted,
  since a revert would cost the same rebuild again.
- **Rule from now on:** don't change `Program`, `Stage`, `Shader::Info` or anything else whose size is in `disk_stamp`
  unless a full re-translation is acceptable. New per-program state goes in a side table, not the struct. Say so before
  deploying if a change does touch the stamp.
- Frame generation is not the cause and not to be moved: base fps is the game's own (CPU-bound, GPU-bound wait 0.0 ms),
  FG only adds frames after it; interpolating before the upscale would upscale every generated frame (more GPU work), and
  NVIDIA's NGX frame generation takes final-resolution frames.
- The Sensor itself worked: 5 area pictures taken, 2 matched ("15 of its first 24 programs", "20 of 24"), 976 and 966
  shaders made ready ahead.

### STATE AT HANDOVER (2026-11-14 11:57): everything confirmed working

The user: "working flawlessly". The confirming run (11:57, Sheikah Slate session 4):

| | Value |
|---|---|
| Program cache | kept (47,866 programs, 7.4 GB); start-up shaders 5.7 s |
| Song of Time | 3,677 shaders made ready at start-up in 1.2 s |
| Sheikah Sensor | 1,386 programs drawn, 13,374 score events, 5 area bursts |
| Area pictures | 4 taken, 4 replayed; a perfect match "24 of its first 24 programs", 929 shaders made ready ahead |
| Owl Statue | a save's first flush 12.2 ms, on its own thread |
| Imports (Lens of Truth) | 104, average 18.9 ms (41 ms before the Lens) |
| Frame rate | 52.0 fps average in busy scenes, 33% of the time at 59-60; 18 frames over 50 ms, 4 over 100 ms |
| Renderer CPU per frame | replay submit ~8.0 ms (draw 12.6 across threads, descriptors 4.4, commands 6.1) |

**What's deployed in the smoke folder (B renderer + exe):** Longshot ports, the memory pack and MasterSword Triforce Shader
Delivery (Power, Wisdom adaptive, Courage, Song of Time, Mirror Shield), Lens of Truth, the Sheikah Slate with the Sheikah
Sensor (SLT2, fingerprint), the Owl Statue, Temper 1 and Temper 2. All on by default; every one has its off switch in
`hookshot_env.txt`. Code and docs are committed on `mastersword-fg`; the commits after `b8408cd2` are local until pushed.

**To resume:** read this file top to bottom, then `docs/MASTERSWORD_TECHNIQUES.md` and
`docs/MASTERSWORD_SHEIKAH_SLATE_RESEARCH.md`. Work in the Forge & Temper cycle, name new tech with Zelda references
(MasterSword / SteelTempleSaint), keep this file current, never resize the structs in the program cache's stamp.

### Next: lifting base fps with what the Slate learns (proposed, not built)

Base fps is CPU-bound (GPU-bound wait 0.0 ms): about 8 ms a frame is the renderer's replay, the rest of the ~19 ms frame
is the game's own threads. The Slate's pictures mostly help area changes, not the steady rate, so the honest path is:
1. **Measure first (Temper 3):** per frame, the game threads' busy time next to the replay's, per area picture, so we know
   which side limits each area. The Sensor's draw counts name the hottest programs per area.
2. **Forge: MasterSword Hero's Shade (per-area self-tuning).** Like the Hero's Shade teaching techniques, the Slate tries
   the renderer's tunables per area (parallel replay threads and segment size, descriptor reuse, compaction timing), keeps
   whichever measured fastest for that picture, and applies it when the area's picture matches. Switch default off, A/B.
   Expected: a few percent where the replay limits, nothing where the game's own threads do.
3. **Targeted Tempers** on the per-draw cost (descriptors ~4.4 ms, commands ~6 ms a frame), guided by the Sensor's hottest
   programs, so effort goes where the draws actually are.

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

1. Lift base fps with the Slate: Temper 3 measurement, then the Hero's Shade Forge (see "Next: lifting base fps" above).
2. If imports still hitch: the driver's pinning half (5-35 ms). Fewer, larger imports at quiet moments, or reusing pinned
   memory, without background pinning during play (that failed).
2. If imports still hitch: find what makes them ~34 ms with RAM free (commit/`VirtualQuery` on write-watch memory, the
   driver's page pinning) and make it cheaper, or import further ahead.
3. Frame rate: warm A vs warm B (A/B kit) to settle whether the Longshot ports cost ~3 fps; suspect the handle tables
   (`RX_VK_HANDLE_TABLES=0`, one shader re-preparation).
4. Test V-Sync Mailbox + `RX_VK_PRESENT_QUEUE=1` against V-Sync Off.
5. Try `RX_VK_ASTC_BC7=1` for fidelity and watch VRAM.
6. Prepare 0.1.9d (`mastersword-fg` is pushed): merge into `main` (merge commit, no force), patch notes in the Pearl Abyss
   style (headline: the memory pack and Triforce Shader Delivery, Lens of Truth, the Owl Statue), release zips, new backup.
7. Visual bugs: duplicate items flickering, skybox and Link colour at Hyrule Castle.
8. Keep watching Longshot (`git fetch longshot`, then `git log mastersword-fg..longshot/main`) for perf and fix commits.
9. **Next investigation (after the import-ahead run): console-style data delivery** (playbook, last section). PS5 Kraken
   -> compress the memory pack (zstd/LZ4 on the E-cores, or GDeflate on the GPU); Xbox BCPack -> cache the GPU's BC1/BC3/BC7
   re-encodes of ASTC textures on disk, compressed, so later loads skip the encode; Xbox Advanced Shader Delivery -> nothing
   game-derived may ship, so make the local start-up preparation incremental instead (a driver update shouldn't mean
   preparing from scratch). Measure first: how long texture re-encodes and pack reads take in the log.
