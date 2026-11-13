# MasterSword CPU placement and CPU Lab — handbook

For the next person or agent (Gemini included) picking up the CPU / frame-pacing work. Written 2026-11-13 after the 0.1.9c
stutter work on the owner's PC: Intel Core Ultra 7 270K Plus (8 P-cores + 16 E-cores, no SMT), RTX 5070 Ti 16 GB, 32 GB RAM,
Windows 11 Insider 26340, 4K output with DLSS.

## The one idea

The Switch 2 gives a game **six identical CPU cores** (Cortex-A78C), each with its own L2. OoT's threads are timed for that.
Hookshot already models it with **six virtual cores** (`host/hos_thread.c`, "virtual cores"): at most six game threads run
guest code at once. On an Intel hybrid CPU, Windows put some of those threads on E-cores (about half the speed, L2 shared by
four). Frames came late and timing-sensitive work lost its races: stutter, flicker, wrong frames. Putting **every game thread on
the P-cores** and the host's background work on the E-cores fixed most of it, and the picture looked steadier (DLSS history).

## What the build does (0.1.9c), and how to switch each part off

| Part | Where | Switch (in `hookshot_env.txt`) |
|---|---|---|
| Fast-core detection (EfficiencyClass) | `hos_fast_cpus()` in `host/hos_thread.c`; `fast_cores()` in `gpu/vk/vk_replay.cpp` | `HOS_P_CORES=0`: no P-core pinning at all |
| Frame threads (MainThread, Presentation, VSync, Display) on P-cores | `hos_thread_pin()` | `HOS_P_CORES=0` |
| Every other game thread on P-cores (not OptimizeChangeBank) | `hos_thread_pin()` | `HOS_GAME_P_CORES=0`: only the frame threads |
| One P-core reserved for the renderer (last core of the fastest kind) | `hos_reserved_cpus()` (upstream) | `HOS_RESERVE_CORE=0` |
| The synchronous replay moved onto that core while it records | `ReplayPin` in `vk_replay.cpp` (upstream 3241a6c6, lost in 0.1.5-0.1.9, restored) | `RX_VK_REPLAY_PIN=0` |
| Parallel replay, thread count from the fast cores | `par_threads()`; `RX_VK_PARALLEL=auto` (8 P-cores -> 4) | `RX_VK_PARALLEL=0` (off is the default) |
| Parallel replay workers on the P-cores less the reserved one, EcoQoS off | `par_worker_main()` | `RX_VK_PAR_ANY_CORE=1` |
| Shader translation / pipeline workers on the E-cores | `jobs_worker_placement()` | `RX_VK_JOBS_ANY_CORE=1` |
| MMCSS "Games" + no power throttling for frame/audio threads | `hos_thread_mmcss_tag()` (0.1.8) | `HOS_NO_MMCSS=1` |
| AMD X3D: game threads on the V-Cache CCD | `hos_cache_cpus()` (upstream) | `HOS_CACHE_CORES=0` |

On a CPU of one kind (most AMD, older Intel) the P/E parts do nothing: every core counts as fast.

Log lines that confirm it at start-up:
`threads: a hybrid CPU: 8 cores of the fastest kind at logical processors 0xc03c03…`,
`vk: replays run on logical processors 0x800000 while they replay`,
`vk: parallel replay auto: 8 fast cores of 24 (a hybrid CPU) -> 4 threads`,
`vk: parallel replay workers on logical processors 0x403c03 (the fast cores, less the replay's own)`,
`vk: background workers on logical processors 0x3fc3fc (the E-cores)`.

## The 270K Plus layout (from `cpulab topo`)

- P-cores: logical 0, 1, 10, 11, 12, 13, 22, 23 — each its own 3 MB L2.
- E-cores: 2-9, 14-21 — four per 4 MB L2 (2-5, 6-9, 14-17, 18-21).
- One 36 MB L3 for all. Interleaved: never assume "the first N" are P-cores.

## CPU Lab (`tools/cpulab/`)

A separate, opt-in program for experiments: rules by thread name in `cpulab.ini`, applied to the running `mastersword.exe`
from outside. No admin rights; nothing persists past the game; it never touches Intel's DTT/APO driver.

- `cpulab topo` / `cpulab threads` / `cpulab watch [ini] [--dry]`. Double-click scripts do the same.
- Rules: `match` (substring of the thread name; `*` any; empty = unnamed threads), `cores` (`P`, `E`, `all`, `list:`,
  `mask:`, `Pminus:`), `method` (`affinity` hard / `cpuset` Windows CPU Sets, soft), `priority`, `power_throttling`.
  First match wins. Unnamed threads (driver, Windows) are left alone by default.
- "Play - my rules only (clean slate).bat" starts the game with all built-in placement off for that session, so only the
  lab decides. Use it to compare layouts fairly.
- The game's threads that matter, by name: `sead::MainThread`, `Presentation Thread` (also records the frame),
  `mkw_vk replay worker` (parallel recording), `mkw_vk worker` (shaders/pipelines), `BaseProcExecutor0-5`,
  `ModuleSystemWorker1-5`, `aal::PathFindingThread` (very busy: ~100k core takes a second), `OptimizeChangeBanc`
  (lowest priority, never stops), `Audio*`.

Ideas not tried yet: `method=cpuset` everywhere; the game on 6 P-cores exactly (`list:0,1,10-13`) with 22/23 for the
renderer; keeping `aal::PathFindingThread` on one fixed P-core for its L2.

## Intel APO, for the record

APO is part of Intel's DTT driver (`dtt_sw.inf`: `ApoProvider.dll`, `configuration/apo.config`). The game list in
`apo.config` is encrypted and signed with the driver; Advanced Mode only shows more titles from Intel's own list. APO's
documented effect is the same kind of thing as above (how a game's threads are scheduled, sometimes on fewer cores). We do
not modify Intel's files or impersonate a listed game: MasterSword's own placement and the CPU Lab do the job explicitly.

Windows' own game list: Game Bar's "Remember this is a game" (Win+G > Settings > General, with the game's window focused)
made play noticeably smoother here. Where Windows stores it on build 26340 was not found (not in
`HKCU\System\GameConfigStore\Children`); there is no public API, so the launcher only shows a one-time how-to on the first
PLAY (`_game_bar_hint_once` in `launcher/MasterSwordLauncher.py`) and never writes Windows settings itself.

## Measuring

- `RX_VK_SLOW_LOG=20`: every frame over 20 ms with the renderer's breakdown. `HOS_PRESENT_TRACE=present_trace.csv`: every
  frame's interval. `RX_VK_PROF_FINE=1`: the per-draw and per-NVN-op breakdown (`a frame (fine)` lines).
- Read `vi: present intervals` (buckets) and `vk: a frame's replay time` in `hookshot_log.txt`. Compare runs in the same
  places (Castle Town market, Hyrule Field, Kokiri Forest).

## What was tried and taken back (do not repeat blindly)

- `RX_VK_ASYNC=1` (replay on its own thread): slower here and white flash frames.
- Old-area texture clear (`RX_VK_OLD_AREA_CLEAR=1`): textures remade from reused memory, a tiled black sky.
- NVIDIA chunk prefetch (`RX_VK_PREFETCH_CHUNKS=2`): no fewer slow imports, more pool merges.
- 16 MB import chunks: mirrors grew past their budget, worse area changes.
- Trimming the render-target pool harder: reuse fell from 45% to 22%.

## Still open

- A steady rhythm of 22-30 ms frames (about 6 a second standing still): suspected work every 10 frames (the texture budget
  check in `texture_frame`); the slow-frame log and present trace will tell.
- 107 failing tests: `runs/_release_prep/TEST_TRIAGE_2026-11-13.md` sorts them into lost fixes, stale tests and setup.
