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

### Gemini's session (2026-11-14 13:00-19:05), reviewed, and the fixes that followed

**What Gemini did** (committed as-is in `c7ebc0f6`, kept apart from later work): the Insert menu, FPS counter and start-up
screen drawn with Dear ImGui (ported from Longshot v3.0.0-rc.3; `third_party/imgui` MIT, Libertinus fonts OFL, the game's own
menu font read from the player's files at run time), Menu size and Overlay size options; Sheikah Slate 3 with Temper 3
per-area timing and **MasterSword Hero's Shade** (per-area parallel-replay tuning, `RX_HEROS_SHADE=1` in the env file);
Longshot ports (prepare workers, `RX_VK_READBACK_LATE`, Intel compiler gate, segment-heap manifest). Tests 166/103/32.
Its own notes, kept local (they hold a user path): `runs/_release_prep/HANDOVER_TO_CLAUDE_0.2.0.md`,
`HEADLESS_BENCH_METHOD.md`.

**The build-command bug (root cause of the missing logs and lost base fps):** Gemini built with
`set RX_RELEASE=1 && set HOOKSHOT_FSR=1 && ...`. In cmd that leaves a trailing space ("1 "), so `nx2/build.py`'s `== "1"`
checks failed: its later exes were **console** apps (subsystem 3: `hos_main.c` sees a stderr handle and never opens
`hookshot_log.txt`; the launcher was not the cause), had **no start-up shader preparation** (`HOOKSHOT_RELEASE` undefined,
`precompile_wanted()` off: pipelines compiled in play), and **no FSR 4**. Always build with quoted sets:
`set "RX_RELEASE=1"`, `set "HOOKSHOT_FSR=1"` (as the scratchpad build scripts do). Check after a build: the exe's PE
subsystem must be 2.

**The program cache was rebuilt twice** in Gemini's session (start-up 54 s at 14:07): `vk_shader.cpp` is part of the
cache stamp's *source* hash (`nx2/build.py`: eden/, sirit/, compat/, vk_shader.cpp, vk_state.h, vk_api.h), and Gemini edited
it. `hookshot_programs.bin` was 302 MB after (7.4 GB before).

**Gemini's mirror finding, confirmed and fixed** (commit `5144045d`): the parallel replay's pre-segment sync stamped every
mirror's `synced_epoch` each submission, so `mirrors_give_back` never saw one idle; an area's mirrors stayed after leaving it
(Field 113 / 1.5 GB at 60 fps -> after Town 163 / 2.0 GB, ~4,800 copies a frame, 52 fps). Pools now carry `bound_epoch`,
stamped where they're bound for a draw; the idle test uses it. The pre-segment sync is unchanged (no copy in a parallel
segment). `RX_VK_MIRROR_IDLE_BY_SYNC=1` restores the old test. Gemini's other two suspects (capture/texture-page growth, the
3,500-texture ceiling) are **not yet verified**.

**Deployed (exe + DLL, backups `*.bak_gemini_1905`):** windowed exe, start-up preparation on, FSR 4 in, the mirror fix.
The cache stamp is unchanged from Gemini's last build, but with preparation back on, **the first start prepares every
shader the 302 MB cache doesn't hold yet (likely about a minute, once)**; later starts ~5 s.

**Every MasterSword system verified intact** in source and binaries, and running in the 17:00 log: the memory pack, Wisdom,
Courage (warm-up), Song of Time, Mirror Shield, the Sheikah Slate and Sensor (9 area matches, 8 replayed, 10 bursts), Lens of
Truth (exe export + renderer), the Owl Statue (10 saves), the resident floor, the census, and Gemini's Hero's Shade.

**Next:** one Field -> Town -> Field run on this build for a real log (`hookshot_log.txt` is back), then verify Gemini's
other two suspects and the base-fps plan in its notes (§5: ParLock contention, per-draw descriptor/ubo cost in Town).

### Run 19:22 (the fixed build): logs back, mirrors given back, but base fps still declines over a session

- Build confirmed: `hookshot_log.txt` written (windowed exe), start-up preparation ran (40 s once, refilling the cache),
  Song of Time 14,355 shaders made ready, 11 area-picture matches.
- **Mirror fix works as designed:** 61 mirrors (880 MB) given back; mirror memory held at 1.4-2.0 GB. The count still rose
  127 -> 359 (turnover, or re-creation churn: not yet told apart).
- **Base fps still fell by stretch (split at loading screens):**

  | Stretch | fps | Renderer submit | GPU | Longest vcore wait | Capture texture pages / capture | Free RAM |
  |---|---|---|---|---|---|---|
  | 1 | 55.8 | 6.2 ms | 5.9 ms | 226 ms | 2,172 | 6.6 GB |
  | 2 | 51.6 | 6.7 ms | 5.8 ms | 510 ms | 11,431 | 5.3 GB |
  | 3 | 47.6 | 8.6 ms | 6.1 ms | 253 ms | 12,331 | 5.0 GB |
  | 4 (return) | 38.7 | 6.9 ms | 5.4 ms | 825 ms | 16,958 | 4.1 GB |

  Renderer and GPU flat; the growth is game/host-side. **Gemini's suspect B confirmed in the data:** capture texture pages
  grow 8x. Mechanism (`vk_memory.cpp` `memory_note_texture_upload`): an upload marks its whole pool (up to 256 MB) as
  texture storage, so every page the game writes there is shadow-copied each capture; a span only expires after 1,800
  presents with no upload from it (`publish_tex_spans`).
- **Next (asked of the user):** two runs of the same route, as-is and with `RX_VK_MIRROR_IDLE_BY_SYNC=1`, to clear or
  convict the mirror fix. Then, if cleared: narrow texture-storage spans to the textures' own ranges instead of whole pools
  (no visual change), and measure the shadow copy's time on the game's thread.

### Mirror A/B (19:32 vs 19:37) and a correction about "capture pages"

- **Run 1, mirror fix on (19:32, Field <-> Town three times):** Field 58.8 -> 56.7 -> 55.8 fps across returns (90% / 46% /
  35% of the time at 59+); Town 48.5 / 48.4 / 46.7. Mirror memory held at <= 2.0 GB; 464 mirrors (5.8 GB) given back and
  made again over the session (turnover, not shown to cost).
- **Run 2, old behaviour (`RX_VK_MIRROR_IDLE_BY_SYNC=1`, 19:37):** Field 53.5 on first load, last stretch 47.1; mirror
  memory 1.6 -> 2.5 GB and still growing. **The fix stays**; the switch is commented out again. Logs kept:
  `AB_test/log_mirrorfix_on_1933.txt`, `AB_test/log_mirrorfix_off_1938.txt`.
- **Correction (to my 19:22 reading and Gemini's suspect B):** the `a capture taken ... N texture pages copied (MB)` number
  is `shadow_n`, the pages **held** in the shadow store (capped at 65,536 = 256 MB), not pages copied per capture. It grows
  because the store fills. The real cost is that line's `all` time: **~0.055 ms a capture, 6 a frame, ~0.3 ms a frame**
  (max ~1.4 ms). Not a base-fps driver; not to be narrowed (whole-pool marking may protect texture correctness).
- **Where base fps stands:** the return-to-Field regression is largely fixed (55.8-58.8 vs Gemini's 52). **Town is the
  ceiling, ~47-48 fps:** Temper 3 (Gemini's log) puts it at ~24 ms a frame = renderer submit ~10.6 ms + game/host ~13.5 ms,
  GPU only ~5 ms. Next: Town's per-draw cost (descriptors, commands, ParLock contention: 3,000+ draws) and what the game's
  own threads spend there (Gemini's notes §5). Unverified suspect left: the 3,500-texture ceiling (suspect C).

### Town base fps: the measured breakdown (run 19:32) and the next step

- Gemini's Temper 3 per-area lines: **game/host 11-15.5 ms in every area**, renderer submit 4-9.5 ms; Town ~9.5 + ~12 =
  ~21-22 ms (47 fps), Field ~5.5 + ~11 (60). GPU ~5-6 ms everywhere: CPU-bound on both sides.
- **Lock contention is not it:** outside loading screens the parallel recording threads wait ~0.1 ms per command buffer on
  locks (BufBind, TexTouch, Targets, PassMarks, TexView), one 7 ms spike during a load.
- **The game-side lever: readback waits.** ~26 GPU-result reads a frame, 5.7 of them wait 0.45-0.63 ms: **~3 ms a frame**
  of game threads stopped on the GPU (of the ~12 ms game side). Gemini ported Longshot's `RX_VK_READBACK_LATE` (rules:
  `newest`, or exact range sizes) which reads chosen ranges a frame late; on a Steam Deck the `0x3980` pair was 40-94% of the
  wait. **Never the exposure meter** (a GPU-added sum the CPU reads and zeroes: read late it double-counts = the white
  flashes, `vk_replay.cpp` ~3145).
- **Next:** one Field <-> Town run with `RX_VK_LOG_READBACK_RANGES=1` (in the env file now, measurement only) to see which
  ranges wait on this PC; then a size rule for the non-exposure ones, A/B'd, watching for flashes or popping.
- After that, the renderer side of Town (~9.5 ms submit for ~2,700 draws): per-draw descriptor and command cost.

### Readback ranges measured (19:51) and the first late-read A/B

- The logging run: **82.8 s of game-thread waiting on GPU results over 18,480 frames, ~4.5 ms a frame.** By range size:
  `0x3980` pair 48.9%, `0x1b580` 36.2%, `0x9b040` 14.4%, everything else <1% (the small ranges, where an exposure-style
  sum would be, barely wait). Baseline log: `AB_test/log_readback_wait_baseline_*.txt`.
- **A/B now in the env file:** `RX_VK_READBACK_LATE=3980` (the Steam Deck research's documented rule; `readback_late.h`
  parses it as hex), range logging still on so the log shows the `late` counts. Watch for white flashes, popping at the
  screen's edges, flickering shadows. If clean and faster, the next step is `0x1b580` and `0x9b040`, one at a time.
- **A/B 1 result (20:01, `RX_VK_READBACK_LATE=3980`):** the pair's wait fell 2.19 -> 0.01 ms a frame (8,422 reads late), but
  the wait **moved** to the same frame's next reads (`0x9b040` 0.65 -> 2.67 ms, `0x1b580` 1.62 -> 1.90): total 4.48 -> 4.61 ms
  a frame, fps unchanged (Town 44-46, Field 52-54). The three ranges come from one batch of GPU work a frame.
- **A/B 2 (set up):** `RX_VK_READBACK_LATE=3980,1b580,9b040`, up to ~4.5 ms a frame off the game's threads (Town ~21.5 ->
  ~17 ms if it all comes off). Higher risk: `0x1b580` / `0x9b040` are unidentified; watch for popping at the edges, light or
  shadow flicker. The exposure meter's small ranges still wait. Logs: `AB_test/log_readback_late_3980_*.txt`.
- **A/B 2 result (20:11, all three ranges late; user: "everything looked fine"):** their waits went to ~0, but a fourth range,
  `0x44b040`, took the wait (160 ms -> 39 s): waiting 4.48 -> 2.71 ms a frame, **fps unchanged** (Town 47.5/42.7, Field
  53-57). **Conclusion:** the game is paced by its own frame's GPU work finishing; whichever read comes first holds the
  wait. That work finishes only after the renderer records and submits the frame, so Town's ~9 ms of replay sits inside
  the game's wait. **Late reads are off** (no gain, a frame of staleness). **The lever for Town is the replay's speed.**
  Logs: `AB_test/log_readback_late_three_*.txt`.
- **Profile result (20:18, `RX_VK_PROF_FINE=1`):** on the replay thread itself only dispatches show (~0.6 ms a frame): every
  draw is already in the parallel segments. The replay thread spends ~8 ms of Town's frame **waiting for its 4 workers**,
  whose draws add up to ~15 ms (commands ~8.2 ms, descriptors ~3.7). Lock waits ~0.1 ms. So Town's recording is limited by
  parallel throughput. Log: `AB_test/log_town_profile_*.txt`.
- **Hero's Shade bug found:** its "+1 thread" candidate was clamped back to the default in `heros_shade_tune` and
  `replay_handle`, so it never tried more than 4 workers.
- **Fix deployed (commit `0b5303ac`, DLL only; backup `*.bak_pre_shade_ceiling`):** `par_threads_max()` = fast cores - 2, at
  most 6 (`RX_VK_PARALLEL_MAX=<n>`); the pool starts that many workers (idle ones wait); Hero's Shade adds candidates 5 and
  6 at the default segment and at 96, clamped to the ceiling. Its learned table is now v3, so every area re-learns once
  (~6 s of trials the first time in each area). `tests/test_slate3_step3.py` updated (`b56d25bc`). **Not yet play-tested.**
  Watch the log for `Hero's Shade (area ...): latched winner (threads N, seg M)` with N = 5 or 6 in Town.
- **Learning run (20:26):** Hero's Shade re-learned (v2 table ignored) and now really explores: winners **6 workers / seg 96**
  in area `64e5593ca0762147` (21.63 -> 18.63 ms), **5 / 160** in `8a8100438bf3f2d4` (17.05 -> 16.33); **Town
  (`1fcaa47aa314283b`) kept 4 workers, seg 240** (22.63 -> 20.88 ms); other areas 4 workers at seg 64/96/240. Some wins are
  0.1-0.9 ms (possibly noise while walking). The run's own fps was lower (52.8 / 51.8 / 38.0 by stretch): the first visit to
  every area ran 9 trial combinations, some bad on purpose. **The table is saved in the Slate; the next run applies the
  winners without trials and is the real comparison** (vs 19:32: Field ~57, Town ~48). Log: `AB_test/log_shade_learning_*.txt`.
- **Run 20:37 found a second bug:** none of 20:26's area keys came back; every area re-ran its trials (lighter stretches 52.7
  fps vs ~56-57 before, heavy 47.3). Cause: Hero's Shade keys an area by `picture_sig(head)`, and the Sheikah Slate replaced
  a re-seen area's whole picture, head included (the first 48 programs vary a little each visit). **Fixed (commit after
  `c64a3f62`, DLL only, backup `*.bak_pre_stable_sig`):** a matched picture keeps its head and refreshes only what it makes
  ready. The next run may re-learn once more (keys from 20:37's mixed heads), then areas should keep their winners. Logs:
  `AB_test/log_shade_relearned_*.txt`.
- **Runs 20:44 and 20:48 (stable keys):** 20:44 re-learned 5 areas; **20:48 ran trials in only 1 area** (the rest reused
  their winners: the key fix works). Results vs 19:32: heavy areas' recording 8.5 -> **7.4 ms** but fps unchanged (48.0 ->
  48.1: Town is paced by its frame's GPU work finishing); **lighter areas worse, 57.1 -> 53.9 fps (59+ 57% -> 24%)**,
  recording 5.8 -> 6.8 ms, likely winners latched from noisy trials while walking. **Hero's Shade is not a proven win.**
  Next: one run with `RX_HEROS_SHADE=0`; if the lighter areas return to ~57 fps with no loss in Town, keep it off until its
  trials are noise-resistant (more samples, a stationary-camera gate, a margin before a winner replaces the default).
  Logs: `AB_test/log_shade_stable1_2044.txt`, `AB_test/log_shade_stable2_2048.txt`.

### Town's recording time (from the 20:11 log) and the profiling run

- Town frames: ~2,500 draws held 60 fps with ~7 ms of recording; 3,064 draws took 8.5 ms (54.8 fps). The parallel segments
  account for only ~2 ms of a heavy command buffer (5 segments on 4 threads); the rest of the 6.7-8.5 ms is serial on the
  replay thread (the dry pass, the frame's five other command buffers, memory work, submission).
- **Profiling run set up:** `RX_VK_PROF_FINE=1` in the env file (per-NVN-operation and per-draw-section timers; their own
  cost lowers fps a little, so that run's fps doesn't count). Goal: what the serial replay time is, then make the serial part
  smaller or parallel.
  (Result: see the Hero's Shade ceiling entries under the readback A/B.)

### Research: idle hardware for a CPU-bound game (NPU, Tensor cores, GPU compute, E-cores)

`docs/MASTERSWORD_HARDWARE_OFFLOAD_RESEARCH.md` (the user's request: use the NPU / Tensor cores beyond SR and FG). This PC
exposes `VK_EXT_device_generated_commands`, `VK_EXT_descriptor_buffer`, cooperative matrix (Tensor cores from Vulkan) and
multi-draw; the NPU (Intel AI Boost) is present. In a Town frame the GPU is ~70% idle, the NPU 100%. Verdict: the real
prize is **GPU-driven submission ("Din's Fire")** built on **bindless descriptors**, using the GPU's idle compute to take
over most per-draw CPU recording; the NPU fits as an advisory predictor ("Nayru's Love"); Tensor cores help only GPU work.
Order: measure Vulkan call types in Town -> bindless descriptors -> Din's Fire -> Nayru's Love. Not built yet.

**Update (2026-11-14, evening):** Town's existing fine profile shows the Vulkan calls themselves are cheap (vkCmdDraw 0.16 ms,
the descriptor push 0.18 ms a frame); the cost is MasterSword translating NVN state (texture descriptors 1.65 ms, ~3.3 ms of
descriptor building of ~8.5 summed). Din's Fire demoted; **Gossip Stones** (reuse what repeats frame to frame) first. Counters
built and deployed for one measuring run: `RX_GOSSIP_STONES=count` (vk_replay.cpp, `gossip::`; log line "Gossip Stones
(count)"), the fine profile per thread (vk_draw.cpp `FineRow`, line now "ms (all threads, X in all)"), `RX_VK_SLOW_LOG=20`.
All three are on in the smoke env; remove after the run. DLL backup: mastersword_vk.dll.bak_pre_gossip_count. The user: smoothness
and base fps matter equally.

**Measuring run done (21:19, AB_test/log_gossip_count_2119.txt; switches off again):** whole-segment reuse is dead (0.0-0.1% of
segments repeat; draws 10-42%). The draw work is 8-14 ms a frame over all threads (the old shared counters showed half):
textures 31%, draw start 15%, vertex binds 12%, Vulkan calls ~3%. Big sets (0.29 a draw, ~28 descriptors: the clustered
lights' handle tables) are the texture cost; the view lookup is already cached. Next candidates: a descriptor set cache
("Gossip Stones v2", at most ~1 ms of Town's recording) or **Mask of Truth**, a sampling profiler of the game's own threads
(~7.5 ms of Town's frame never profiled). See the research doc s8.

**Mask of Truth (2026-11-14, built, waiting for its first sample):** the engine already had an all-thread sampler
(`HOS_SAMPLE_ALL=<file>` in host/mkw_glue.c: every 2 ms each guest thread's and `mkw_vk*` thread's stack, while `<file>.go`
exists; it is in the release exe), so nothing was rebuilt. New: `scripts/mask_of_truth.py` (names frames from
work/sa/build/*.map, which match the deployed exe; game functions are `f_<guest address>`; prints running share by thread,
the kind of code, game functions inclusive/innermost, SDK services, what the serial threads wait in, the critical path) and
`scripts/mask_of_truth_sample.bat` (in the smoke folder as "Mask of Truth - sample 30 seconds.bat": waits 10 s, samples 30 s).
scripts/prof.py and crit.py now know the mastersword binaries. Smoke env: `HOS_SAMPLE_ALL=mask_of_truth_samples.txt` (remove
after the run). Read it with: `python -I scripts/mask_of_truth.py "<smoke>/mask_of_truth_samples.txt"`. The sampler takes its
thread list when the .go file appears, so start it in play, not at boot. Rule from Gemini's notes: no automated headless runs
unless the user asks.

**Mask of Truth's first Town sample (21:53, smoke mask_of_truth_samples.txt, 433 MB):** the main thread runs 22% of the time
and waits ~59% for the game's job workers (BaseProcExecutor0-5, ~22% busy each): the job phase is the critical path. 10.7% of
game-side time is in out-of-line float helpers, and the hot split (static guess, 11-12) misses 89.6% of their callers and 19 of
the 20 hottest functions (f_801102e0 alone 7%). Proposed next: **Biggoron's Forge**, the hot split from measured heat
(research doc s10). Gemini's D:\MS_AI AI bench reviewed (s11): nothing in it for MasterSword's base rate. HOS_SAMPLE_ALL left
on in the smoke env (free until the .bat runs) for the Temper sample after the Forge.

**Biggoron's Forge (2026-11-14, built 22:19 and deployed to smoke; waiting for its Temper run):** the hot split re-forged from Mask of Truth's measured heat.
- `scripts/mask_of_truth_hot.py <samples> --write work/measured_heat.txt` writes the heat (per function: samples where it was
  the innermost game function and the thread ran game code, not a wait's spin).
- `nx2/make_hot_split.py` reads it (RX_MEASURED_HEAT=<file>|off, RX_MEASURED_MIN=3): measured functions up to 72 KB go into
  the -O2 hot pieces (tier A: 699, 18.2 MB, 47.4% of Town's measured time); bigger ones stay at -O1 in their chunk with
  the float fast paths (a64_fpx_hot_*) written in place (tier B: 400, 93.6 MB, 44.3%). 16 hot pieces; hot_order.txt
  leads with the measured functions. work/hot_keep.txt (the 728 functions of the 11-12 split) always stays hot.
- Result of the split: Town's measured time in the -O2 pieces 2.9% -> 50.3%. Correction: the 11-12 split has 728 functions
  (the 1176 counted earlier included declarations); none is lost.
- **Before re-splitting, `_cold_backup/code_005.c` was stale** (made 11-12, before the 11-14 Scarecrow/floor-web port patched
  f_80652cb0 in gen): the normal re-split would have restored it and lost the floor web fix. Refreshed from gen (old copy in
  work/sa/_stale_backups/). Check backups against gen before any future re-split.
- Rollback: work/sa/_pre_biggoron/ holds the 11-12 hot pieces, the touched chunks as they were, chunk_costs, hot_order.txt,
  and the old mastersword.exe + maps (the 21:53 Town sample must be read with RX_MAPS=work/sa/_pre_biggoron).
- Built in 306 s (the 16 hot pieces 5-9 s each at -O2; code_005 and code_041 recompiled). f_801102e0 now at the start of
  .text (was 384 MB in). Deployed: smoke mastersword.exe (backup mastersword.exe.bak_pre_biggoron); the renderer DLL kept as
  tested (the rebuilt one differs only by build). Baseline kept: AB_test/log_pre_biggoron_2153.txt (Town-like windows,
  >=2000 draws: 45.6 fps with the sampler off, 44.8 on) and AB_test/mask_of_truth_town_2153_pre_biggoron.txt (read it with
  RX_MAPS=work/sa/_pre_biggoron).
- Temper run: Town ~1 min without the sampler (the fps comparison), then "Mask of Truth - sample 30 seconds.bat" (the profile
  comparison), then two other areas for correctness.
- **Temper run 22:28 (AB_test/log_biggoron_2228.txt, mask_of_truth_town_2228_biggoron.txt):** clean (no faults, normal
  exit; Town and lighter areas). **What it targeted, it did:** out-of-line float helpers 10.7% -> 1.1% of game-side running
  time; the job workers' recompiler-helper share 3.9% -> 0.9%. **The frame rate is not proven:** sampler-off Town-like windows
  45.6 -> 47.0 fps, but the runs stood in different spots (Town's area averaged 2186 draws before, 1830 after; per-area
  game/host changes -6% to +13% on single samples), and the job workers were busy 22% -> 24% (the float work now inside the
  functions; more actors where the second run stood?). Needs a same-scene A/B: a fixed spot, both exes, alternated.
- **Same-scene A/B (overnight 2026-11-14, user-approved headless bench, "Hookshot Test - bench"): the Forge is SLOWER.**
  Old exe 22.26 ms a frame (44.9 fps, 2 runs), Forge 22.98 ms (43.5 fps, 2 runs): +0.72 ms (+3.2%); every Forge run slower
  than every old run; the per-area game/host part +0.4-0.6 ms, recording unchanged. (The Temper sample's busier job workers,
  22% -> 24%, were this, not location.) Smoke folder put back on the old exe (the Forge one kept as
  mastersword.exe.biggoron_v1_slower). Suspect: tier B, the float fast paths inlined into 400 giant -O1 functions, +11 MB of
  code (416 vs 405 MB).
- **V1 = tier A only (now the default; RX_MEASURED_TIER_B=1 brings tier B back): a measured win.** Pooled at equal draws
  (2779; 0.365 ms per 100 draws): old 22.16 ms (4 runs, stdev 0.12), first Forge 22.74 ms (+2.6%), **V1 21.71 ms (-2.0%,
  45.1 -> 46.1 fps)**; both V1 runs outside the old range. Clean (no faults). **Deployed to smoke** (mastersword.exe = V1,
  md5 9199f6c5; the old one stays as mastersword.exe.bak_pre_biggoron). bench_compare.py now also prints equal-draw means.
- **Copy split (take_capture's copies, vk_memory.cpp copy_split): measured win, deployed.** The submit-time capture's copy is
  split into up to 4 parts a thread (64 copies each at least; RX_VK_CAPTURE_PARTS=1: as before), so the game's thread (P-core)
  takes more than the E-core helpers instead of waiting for them. Bench (V1 exe both sides, 2+2 runs alternated): 21.87 ->
  21.50 ms a frame (-1.7%, 45.7 -> 46.5 fps), both copy-split runs below both base runs, no faults. Smoke mastersword_vk.dll =
  f519b943 (backup mastersword_vk.dll.bak_pre_copysplit). Not changeable: capture_make's wait for the exposure meter's
  write-back (11% of the main thread's submit time; the white-flash fix).
- **Bench baseline now V1 + copy split** (exe_old.exe = V1, dll_base.dll = copy split; exe_preforge.exe = before the Forge).
- nx2/build.py: RX_HOT_FLAGS="<flags>" adds compiler flags to the hot pieces only (in their object key), for -hot-cold-split.
- **PGO (in progress, 2026-11-15 night):** instrumented exe (RX_PGO=gen on the V1 split) = bench exe_pgogen.exe.
  tools/handover/mastersword_pgo_profile.ps1 runs it with HOS_PGO + timed .reset/.dump. Profiles: bench_runs/pgo_town (market,
  walking), pgo_field (meant as Field, but the Load menu's DOWN did not take: a second Town profile, the entrance and bridge),
  pgo_field2 (Field reached by walking back out of the Town gate: LDOWN from the Town autosave). Unprofiled functions go cold
  under PGO, so Field matters. Next: merge (llvm-profdata), build RX_PGO=<merged>, A/B against V1 + copy split.
  Merged: work/pgo/biggoron_v1.profdata (2649 functions; top: the float helpers and recomp_load64). PGO exe = bench
  exe_pgo.exe (406 MB). First A/B (pgo vs old, 2+2) was spoiled by an outside disturbance in one old run (2-3 s stalls, the
  first 3000 presents at 41 fps; none of the night's other runs of that exe did this): with the clean baseline runs of the
  copy-split batch (same binary), PGO ~-0.23 ms (-1%) at equal draws, inside the baseline's spread: not proven. Now a
  three-way batch (tag "tri": hcs, pgo, old x2) with PGO + "-mllvm -hot-cold-split=true" (bench exe_hcs.exe; 4804 .cold
  parts in its map). bench_compare.py now prints each run's median window too (robust to stalls).
  **Result (tag "tri", 2 runs each, clean): no gain from either.** At equal draws: baseline (V1 + copy split) 21.41 ms,
  PGO 21.59 ms (+0.18), PGO + hot-cold-split 21.37 ms (-0.04); raw medians: baseline 21.13, PGO 21.40, hcs 21.54. Neither
  deployed. Read: the hot code is already -O2 with the float fast paths; what is left on the critical path (the job system's
  chain and its waits) is not branch layout. The tools stay (RX_PGO, RX_HOT_FLAGS, mastersword_pgo_profile.ps1) for later.
- **New lead (2026-11-15 ~02:30, bench Mask of Truth of V1 + copy split, motb_1_old; maps in work/sa/_maps_v1):** the main
  thread runs 17.5%; it waits on the job event in f_8007fe60 ~60% of the time, but while it does, **64% of the time no job
  worker runs and 36% of the time no sampled thread runs at all** (31.6% of all instants: ~6.8 ms a frame with nothing
  running). The renderer's GPU-bound wait is 0.0 ms. A timeline shows 8-12 ms gaps where everything is idle, then a burst.
  Hypothesis: the game waits for a timed tick (nn_vi.c's 60 Hz vsync thread / the Mastersword Pacer), so frames are quantized
  (17-20 ms of work waits for 33.3 ms). Test queued (job pace1): default vs HOS_FPS_MAX=1000 (the Insert menu's Uncapped:
  vsync at 1 kHz) with HOS_PRESENT_TRACE per-present intervals; check the uncapped route still arrives (screenshots), since
  frame-clock inputs may come early in faster menus. Also: the main thread waits ~9% for the renderer's recording
  (wait_recorded_part in rx_nvn_invalidate / capture_make): recording speed is on the critical path there.
- The session scratchpad was wiped during a pause; build scripts are now work/build_full.bat and work/build_vk.bat.
- **Correction:** after the copy-split batch the bench's mastersword_vk.dll in place was the OLD DLL (791384c; the A/B script
  swaps DLLs only for runs that name one). So the PGO/hcs batch ("tri"), the Mask of Truth sample (motb_1_old), Hero's Shade
  and the pacing runs used V1 + the old DLL, the same on both sides of each comparison (the comparisons stand; "baseline V1 +
  copy split" there means V1 + old DLL). The bench now has the copy-split DLL in place (f519b943), as smoke does.
- **Hero's Shade A/B (bench, 2026-11-15 ~02:26-03:07): no measurable effect; left as the user has it (on).** Clean runs:
  Town on 22.27 / 22.03 ms (median window) vs off 21.95; outside the gate (the "field" route ends looking back at the town,
  ~2840 draws, not the light area) on 19.18 vs off 19.49 / 19.19. Several runs of this batch were disturbed: shtown_3
  (an ~80 s freeze while heavy Python analyses ran beside the 15 GB game: paging, most likely: **never run heavy analysis
  during a bench run**), shtown_4 and shfield_1 slower throughout (cause unknown).
- **The idle gaps are not the vsync, not oversleeping, not the GPU:** the log's present histograms (vi: present intervals)
  show 17.8-30 ms frames and none at 30-40 (no quantization to 33 ms); the GPU-bound wait is 0.0 ms; HOS_SLEEP_LOG=1
  (bench sleeplog_1) shows the job polls (lr 0x8009a478 / 0x8009aff0, in f_800998a0) asking 20 us and sleeping ~22 us,
  PreActorModelCompose x6 at 100 ms, OptimizeChangeBanc (f_8002f7a0) a 16 ms loop. During the gaps the job workers wait for
  jobs (ReceiveLightMessageQueue) that nobody has posted yet. Who posts them late is the open question: it needs a
  wake-chain trace (ETW CSwitch/ReadyThread via WPR, admin), which this session could not take. The uncapped run
  (HOS_FPS_MAX=1000) never left the gate (frame-clock inputs came early in the faster menus): not comparable, dropped.
- **The bench (tools/handover/):** mastersword_bench.ps1 (one headless run of the bench copy: -Exe/-Dll name, -Script on
  the frame clock, -Live, -ShotEvery, -Sample, -Env, -Touch), mastersword_bench_ab.ps1 (the Town route, alternated runs),
  mastersword_detached.ps1 (runs a job outside the tool's time limit: bench_runs/_jobs/<job>.done|.failed|.log),
  scripts/bench_compare.py (frame time over presents 4800-12000, 2500+ draws). The bench copy: romfs/image.bin hard-linked,
  caches copied, user_pristine restored before each run, Hero's Shade off. Route: newest autosave "Hyrule Castle Town",
  "37:A,40.5:A,51.5:A,54:A,57:A,64.5:LUP/7000" -> standing in the market square (~2700-2900 draws, ~45 fps). Logs: stderr.

**Next phase, agreed plan (2026-11-14), in order:**
1. Temper of Biggoron's Forge (Town + Mask of Truth sample + two other areas for correctness). Nothing stacks before it.
2. PGO on the hot pieces: `RX_PGO=gen` instrumented build, a profile session (Town, Field, Kokiri, a dungeon; `HOS_PGO=<path>`
   with `<path>.reset` / `<path>.dump` to leave loads out), `python -I nx2/pgo_pieces.py <in> <out> 16` (16 pieces now), then
   `RX_PGO=<merged .prof>`. Tier B (big functions in the chunks) gets no profile.
3. `-mllvm -hot-cold-split=true` as its own A/B after PGO (needs the profile; expect small).
4. Profile-guided grouping of the hot pieces (split_hot groups by call counts now: 2566 of 3754 hot calls stay in one piece);
   scoped ThinLTO only if hot cross-piece calls still show after that (object keys, /order and PGO all touched).
5. In parallel (DLL only): take_capture / submit_memory_work on the game's submitting thread (~1 ms a frame on MainThread):
   measure its parts per thread (memsync_part_ns), then fewer bytes (write-tracked pages) or off the game thread.
- Dropped with reasons: f_800998a0's 20 us sleep-poll (the job wait itself; the host yields precisely; upstream measured a
  shorter poll: no change; workers are 86% idle waiting for jobs, not for cores) and 1:1 P-core HOS_PIN (4-5 of 8 P-cores
  busy; ~14-16 threads want P-cores; hard pins remove the scheduler's freedom the soft placement was tuned to keep; would need
  an ETW trace showing harmful migration first).

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
