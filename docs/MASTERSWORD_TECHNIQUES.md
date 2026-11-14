# MasterSword Techniques — a playbook for other PC ports and recompilations

MasterSword is the continuation of NotRexed's **Hookshot**: a static recompilation of a Switch 2 game, with ARM64 lifted
to C, a Horizon OS host, and NVN replayed on Vulkan 1.3. This document collects the techniques that made it smooth on a
modern Windows PC, written so that **other games and other recompilation or emulation projects can use them**. Each one
says what problem it solves, how it works, how to implement it, how it was measured, and the switch that turns it off.

What came from where:

| Technique | Origin |
|---|---|
| Hybrid-CPU placement (P-cores for the game, E-cores for background work), the "Intel Hybrid CPU Patch" toggle | **MasterSword** (0.1.9c) |
| Parallel replay sized to the CPU's fast cores ("auto") | **MasterSword** (Hookshot had the parallel replay itself, off by default) |
| Memory census | **MasterSword** (2026-11-14) |
| Resident floor | **MasterSword** (2026-11-14) |
| The memory pack (shader code on NVMe, handed to the driver inline) | **MasterSword** (2026-11-14). Hookshot and its successor Longshot keep every shader module for the session |
| MasterSword Triforce Shader Delivery (the pack + a hot module cache + a warm-up) | **MasterSword** (2026-11-14) |
| Import ahead in play | **MasterSword** experiment, measured worse and switched off |
| Reserved core for the replay thread | Hookshot (upstream), restored in MasterSword |
| X3D cache-CCD placement | Hookshot (upstream) |

The hardware these numbers come from: Intel Core Ultra 7 270K Plus (8 P-cores at logical processors 0, 1, 10-13, 22, 23;
16 E-cores at 2-9, 14-21), RTX 5070 Ti 16 GB, 32 GB RAM, Gen 5 NVMe, Windows 11, 4K output with DLSS and frame generation.

---

## 1. Treat the PC like the console it replaces: hybrid-CPU placement

**The finding.** The Switch 2 gives a game six identical CPU cores. An Intel hybrid CPU (12th, 13th and 14th gen Core,
Core Ultra) has two kinds: fast P-cores and slow E-cores. Left to Windows, a game's threads land on E-cores at times, and a
frame thread there ran its work about 1.5-2x slower. The result was uneven frame times and, in MasterSword, many of the
graphical glitches, because timing-sensitive game threads fell behind each other. Pinning the game to the P-cores removed
almost all of the glitches and much of the stutter. In short, **give the recompiled game a set of identical fast cores, as
the console did, and put everything else elsewhere.**

**The layout:**

| Thread | Where | Why |
|---|---|---|
| The game's frame threads | P-cores, minus one | The critical path |
| All other game threads | P-cores, minus one | The console's "identical cores" |
| The replay (GPU command recording) thread | The reserved P-core, alone | On a core shared with game threads it ran ~1.7x slower |
| Parallel recording workers | P-cores, minus the reserved one; EcoQoS off | They finish inside the frame |
| Background workers (shader translation, pipeline builds, memory imports) | E-cores, below-normal priority | Throughput work, never on the frame's path |

**How to implement it (Windows):**

1. Find the core kinds with `GetSystemCpuSetInformation`: each CPU set has `Processor.EfficiencyClass`. A higher value
   means a faster core. The fastest class gives the P-core mask. Treat the CPU as hybrid only if there are 2+ classes and
   4+ cores of the fastest one (a laptop's 2 P-cores shouldn't lose one to a reservation).
2. Reserve the **last core of the fastest kind** for the replay or render thread (on hybrid CPUs the last core of all is
   an E-core, the worst place for it). Pin that thread with `SetThreadAffinityMask` while it works.
3. Pin game threads to `P-mask & ~reserved` (`SetThreadAffinityMask`), and background workers to the E-core mask.
4. Turn off EcoQoS (Windows' power throttling) for the time-critical threads: `SetThreadInformation(...,
   ThreadPowerThrottling, ...)` with `StateMask = 0` and `ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED`. Optionally
   register the frame threads with MMCSS (`AvSetMmThreadCharacteristicsW(L"Games", ...)`).
5. Make it a user toggle, on by default for hybrid CPUs, with an environment override. In MasterSword that's the F1 /
   Insert menu "Intel Hybrid CPU Patch" (ini `hybrid_cpu`), `HOS_P_CORES=0` (anywhere), `HOS_GAME_P_CORES=0` (frame threads
   only), `RX_VK_REPLAY_PIN=0`, and `RX_VK_JOBS_ANY_CORE=1` (workers anywhere).
6. On AMD X3D CPUs with two CCDs, keep the game on the CCD with the 3D V-Cache (the larger L3), found from the cache
   topology. Hookshot measured 57-58 fps -> 59.9 fps on a 9950X3D.

**Also worth telling players:** in Windows' Game Bar, tick "Remember this is a game" for the executable. Windows then
gives it game scheduling (and Intel APO on supported systems). The MasterSword user found this made the game "insanely
smooth". MasterSword's launcher shows a one-time how-to.

Code: `host/hos_thread.c` (`hos_fast_cpus`, the reserved core, game-thread pinning), `gpu/vk/vk_replay.cpp`
(`fast_cores`, `par_worker_main`, `jobs_worker_placement`, the replay pin), `gpu/vk/vk_menu.cpp` (the toggle).

## 2. Size the parallel work to the CPU you're on

Recording a frame's GPU commands on several threads took the renderer from 10.9 to 7.75 ms a frame (measured upstream). The
thread count comes from the CPU: with n fast cores and n ≥ 6, `clamp(n - 4, 2, 4)` workers (the 270K Plus: 4); 0 below that.
It's "auto" by default (`RX_VK_PARALLEL=auto`, or a number). The workers run on the P-cores minus the reserved one (§1).

**A lesson for any parallel renderer:** the first attempt made one area's lighting go dark. The cause was a lost safety
rule, not the parallelism: a GPU copy of a memory pool must never be started inside a parallel segment. Restoring that one
check made parallel replay safe.

## 3. Measure memory before you optimise it: the memory census

**The finding.** The last stutters looked like "streaming" (new areas, crowds of NPCs). The log showed the renderer idle
and the GPU idle during 2-second freezes. Free RAM on the PC had fallen to ~1 GB, and Windows was paging the game's memory.
The question was what filled the RAM, so the game now measures it.

**How:** once a minute, on a background thread:
- `K32QueryWorkingSet` returns every resident page. Sort them, then classify each by address: the game's own memory (the
  guest window, a known range), or `VirtualQuery`'s `MEM_PRIVATE` / `MEM_MAPPED` / `MEM_IMAGE`.
- Group the private pages by `AllocationBase` and list the 10 largest.
- Count what was handed to the graphics driver (here, the SPIR-V given to `vkCreateShaderModule`).
- Every 30 frames, log a "memory pressure" line when the process's page faults spike (`PageFaultCount` delta) or the PC's
  free RAM (`GlobalMemoryStatusEx`) drops under 1.5 GB.

**What it found:** working set 22.3 GB = the game's memory 9.2 GB + **12.8 GB of renderer and driver memory**, and
**194,425 shader modules holding 13.5 GB of SPIR-V** inside the driver. The game itself was never the problem.

Code: `gpu/vk/vk_mempack.cpp` (`census_run`), `gpu/vk/vk_texture.cpp` (the pressure lines). Switch: `RX_MEM_CENSUS=<frames>` or 0.

## 4. The memory pack: shader code on the NVMe, not in the driver

**The finding.** A Vulkan shader module is only needed while a pipeline is being built from it, yet a renderer that keeps
modules for the session makes the driver keep a copy of every shader the game ever compiled. On a game with tens of
thousands of shader programs that means gigabytes of cold data sitting in RAM. Hookshot and Longshot both keep every module.

**How it works:**
1. When a shader stage's SPIR-V is made, **write it to a pack file** on disk (`mastersword_spirv.pack`, beside the game) and
   don't create a `VkShaderModule`. The pack is **content-addressed** (a 64-bit hash of the words), so identical code is
   stored once: 194,425 stages became 50,812 distinct codes, 3.6 GB. An index file (`.idx`, 16 bytes a record: hash, then
   offset and size) makes the pack persist between runs, so a warm start writes nothing.
2. At each pipeline build, **read the code back** with a positional `ReadFile` (an `OVERLAPPED` offset, so readers and the
   writer share no file pointer) and hand it to the driver **inline**: chain a `VkShaderModuleCreateInfo` onto the
   `VkPipelineShaderStageCreateInfo::pNext`, with `module = VK_NULL_HANDLE`. Vulkan allows this when the device has the
   graphics pipeline library feature enabled (or `VK_KHR_maintenance5`). Chain it **after** anything else that sets
   `pNext` (here, the required-subgroup-size struct), and keep the buffer alive until the create call returns.
3. The reads go through **Windows' file cache**, and that's the "dedicated swap" the design wanted: free RAM keeps the hot
   shaders, and under memory pressure Windows drops them instantly, because they're clean file pages already on disk. Nothing
   is written to the pagefile, and nothing has to be paged back on a fault.
4. Fall back to a normal module if the pack can't be written (a full disk), and offer a switch to turn it off
   (`RX_MEM_PACK=0`).

**Results (same PC, same area):**

| | Before | After |
|---|---|---|
| Shader code held by the driver | 13.5 GB | 0 |
| Renderer and driver RAM | 12.8 GB | 8.0 GB |
| Game working set | 22.3 GB | 17.4-17.8 GB |
| Free RAM late in a session | ~1.2 GB | ~5 GB |
| 2-second stall bursts | 1-3 per run | 0 (warm run) |
| Pack read failures | n/a | 0 of 193,000+ |

**For other projects:** this applies to any Vulkan renderer that translates or generates many shaders: emulators,
recompilations, and games with large material permutation sets. A pipeline cache (`VkPipelineCache`) still caches the
driver's compiled output; the pack only removes the long-lived source copy.

Code: `gpu/vk/vk_mempack.cpp` (`stage_set_code`, `stage_bind`), `gpu/vk/vk_shader.h` (`Stage::code_at`, `code_words`,
`StageCode`), and the build sites in `gpu/vk/vk_draw.cpp`.

### 4b. MasterSword Triforce Shader Delivery: the pack, made fast

The pack alone has a cost: every pipeline build reads its shader back and the driver parses it again (53,000 reads,
3.4 GB a session), and area changes' pipeline time doubled. Triforce Shader Delivery keeps the pack's RAM saving without
that cost, in three parts:

| Part | What | How | Switch |
|---|---|---|---|
| **Power** | The pack | Every distinct shader stored once on the NVMe, content-addressed (§4) | `RX_MEM_PACK=0` |
| **Wisdom** | A hot module cache | Modules of shaders used recently are kept, up to a RAM budget (1 GB), and the least recently used one that no build holds is destroyed. A pipeline build takes a reference and gives it back when its create call has returned, so a module is never destroyed under a build. An area's builds reuse one parse | `RX_TRIFORCE_HOT_MB=<MB>`, 0 none |
| **Courage** | A warm-up ahead of need | At start-up, a thread at background priority (low I/O and memory priority, on the E-cores, its own sequential-scan handle) reads the pack front to back in 8 MB reads, so first reads in an area come from Windows' file cache at the drive's sequential speed. Only when free RAM is twice the pack's size; those pages are the first Windows evicts | `RX_TRIFORCE_WARM=0` |

**Measured (same PC, many area changes):** time at 59-60 fps 13% (pack alone) -> 34%; average fps 50.5 -> 53.5; shader
reads from the pack halved (3.4 -> 1.7 GB a session); area-change pipeline time in the slowest frames 924 -> 620 ms; the
hot cache served 28,774 builds; the warm-up read the 3.6 GB pack in 2 s at background priority. The player's verdict:
"that made a huge difference".

The idea in one line, for other projects: **store all shaders once on fast storage, keep only a bounded hot set parsed in
the driver, and stream the cold set in big sequential reads before it's needed.** That's the console data-delivery idea
(below) applied to shaders with the PC's own pieces: an NVMe, the OS file cache, and a bounded cache.

**What didn't work: importing memory ahead in the background.** Mapping new game memory for the GPU on a background thread
before the first draw asked for it (§6) made imports slower, because the driver's page pinning contended with the game's.
Loads got longer. It's switched off, and kept here as a measured negative result.

## 5. The resident floor: protect the game's RAM, softly

Windows trims every process's working set under pressure, and a game whose threads touch trimmed pages stalls on each fault.
A **soft minimum working set** tells Windows to trim other programs first:
`SetProcessWorkingSetSizeEx(GetCurrentProcess(), min, max, QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE)`.
It's soft, so the PC can't be starved. MasterSword uses 55% of physical RAM, at most 18 GB (17.5 GB on 32 GB), with
`RX_MEM_RESIDENT_MB=<MB>` or 0 to change it. Don't use a hard minimum: it can starve the system.

## 6. Do slow work before the frame needs it: import ahead

A recompiled game's memory has to be made visible to the GPU (here, a Vulkan host-pointer import of the guest pages). Each
import costs the OS's commit bookkeeping plus the driver pinning the pages, ~34 ms, whatever the size. The game creates new
memory pools as it streams, and the next frame's draws need the first part of each. MasterSword's play-only mode
(`RX_VK_BG_IMPORT=play`) maps a new pool's first chunk on a background thread (E-cores, below normal) as soon as the game
creates it, before any draw asks. Lessons from the attempt that failed first:
- **Never do it during start-up.** Importing every pool in the background at start-up hung the shader-preparation screen,
  because the game's own memory thread stalled behind the page pinning.
- **Only the part that will be used:** the first chunk, not the whole pool.
- **Cap the queue** (16 here), so a burst of allocations can't flood the thread.

Status: **measured worse and switched off** (only 32 of ~380 chunks imported in time; the game's own imports slowed
from 34 to 43 ms on average, 290 ms worst). The lesson: background page pinning competes with the foreground's. Code:
`gpu/vk/vk_memory.cpp` (`memory_pool_created`, `bg_main`).

## 7. Method notes that made the difference

- **Log what you need to decide, then decide.** Each fix here started from a log line: per-frame slow-frame breakdowns
  (`RX_VK_SLOW_LOG=<ms>`), a per-NVN-op profiler, import timings, the memory census. Several "obvious" fixes (bigger
  import chunks, prefetching, async replay, a harder texture-pool trim) measured worse and were reverted. They're listed in
  the handover so nobody retries them blind.
- **A/B against the previous build in the same spot, warm.** The first run after a shader-cache change re-prepares
  everything and isn't a fair sample. MasterSword's `AB_test` kit swaps the renderer DLL and files each run's log under its
  letter.
- **Separate the frame's critical path from throughput work,** on the CPU (cores) and in time (ahead of the frame).

---

## Future investigations: what consoles do for data delivery

PS5 and Xbox Series X|S move data with dedicated hardware and shipping formats. Each has a PC-side analogue worth
investigating for MasterSword and for other ports:

| Console feature | What it does | What a PC port could do |
|---|---|---|
| **PS5 Kraken** (RAD Game Tools' Oodle Kraken, decoded by dedicated I/O hardware) | Lossless decompression of game data at drive speed, with no CPU cost | Compress the memory pack: SPIR-V typically compresses several times over, so the 3.6 GB pack would shrink, and reads from cold NVMe get faster. Open codecs: zstd or LZ4 on the CPU (E-cores), or GDeflate on the GPU (DirectStorage, NVIDIA nvCOMP). Oodle is proprietary (licensed by Epic Games/RAD) |
| **Xbox BCPack** (+ zlib, hardware-decoded) | Entropy-codes block-compressed (BCn) textures for storage and streaming | MasterSword re-encodes the game's big ASTC textures to BC1/BC3/BC7 on the GPU every time they're loaded. Caching the encoded BCn blocks on disk, compressed, would skip that work on later loads and cut texture-upload stutter |
| **Xbox Advanced Shader Delivery** | Ships pipelines precompiled for the player's exact GPU and driver, so there's no shader compile on first launch | Hard rule: nothing made from the game's files may leave the PC, so precompiled data can't be shipped. Locally the same idea already exists in parts: the driver's own shader cache (NVIDIA Shader Cache size 100 GB), MasterSword's pipeline cache file and the start-up preparation pass. Worth a look: making the start-up preparation incremental and resumable, so a driver update doesn't mean preparing from scratch |

The Switch 2 itself has a hardware decompression engine for game data. A recompiled game that decompresses on the CPU is
doing work the console offloads, and that's another place an E-core or GPU path could help.
