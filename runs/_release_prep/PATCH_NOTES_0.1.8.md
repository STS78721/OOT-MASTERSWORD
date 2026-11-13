# MasterSword 0.1.8

**Release date:** 2026-11-12  
**Type:** Major CPU/GPU Frame-Time, Zero-Stutter Precompilation, Hybrid P-Core Scheduling & Cross-PC Performance Update (drop-in over 0.1.7 / 0.1.6 / 0.1.5e / 0.1.5d)

Major cross-PC performance and frame-pacing release delivering **Profile-Guided Hot/Cold Code Splitting & `.text` Call-Locality Packing**, **Shipped `HKSTATE2` Pipeline Precompilation (`41,839` pipelines — `0` in-play shader compile stutter)**, **AVX2 64-Page Dirty-RAM Scanning**, **Bloom-Filtered Multi-Wait Synchronization**, **Vendor-Smart Vulkan Render-Pass & Compute Barriers**, **Coalesced 4 KB Mirror-Sync DMA Copies**, **Elimination of Recycled Render-Target Upload Spikes**, **Windows 11 Hybrid P-Core / MMCSS Scheduling**, **Read-Only RomFS Handle Caching**, and **Live `VK_EXT_memory_budget` VRAM Guardrails** across NVIDIA, AMD, Intel, laptop, and handheld PCs.

---

## Highlights

- **Hot/Cold Recompiled Code Split + `.text` Locality Ordering (`728` Hot Functions at `-O2`)** — isolates the top `99.1%` CPU-time lifted functions into dedicated `-O2` translation units (`code_hot_0..7.c`) with inlined single/double FP fast paths and packs `4,513` hot call-graph neighbours contiguously at the front of `.text` via `lld-link /order`, slashing L1i/iTLB misses across Intel, AMD Ryzen, and X3D CPUs
- **Zero-Stutter Gameplay via Shipped Pipeline State List (`41,839` Pipelines)** — ships `hookshot_pipeline_states_shipped.bin` alongside `HKSTATE1`/`HKSTATE2` state-list preparation, active compute filtering (`hookshot_computes.bin`), and upgrade-aware pre-warming so pipelines compile up front instead of hitching mid-swing (`0` skipped draws, `0.0 ms` shader waits in play)
- **Eliminated 12–17 ms Recycled Render-Target Upload Spikes** — prevents finalized GPU-drawn render and depth targets (`nvnTextureFinalize`) from CPU-unswizzling and PCIe-uploading `8–32 MB` of recycled transient-heap buffer bytes over drawn `VkImage` targets
- **Coalesced Capture-to-Mirror DMA Transfers (~10–15× Fewer Copies/Frame)** — merges contiguous 4 KB dirty pages in `mirror_sync()`, caches capture-buffer lookups, and reuses `thread_local` scratch storage, collapsing `~3,200+` tiny per-frame `VkBufferCopy` regions into `~150–250` contiguous DMA spans
- **AVX2 Dirty-Page Scanner, Multi-Wait Bloom Filter & Vendor-Smart Vulkan Barriers** — scans 64 guest pages (`256 KB`) per iteration using AVX2 `VPTEST` + 2-slot buffer pooling, filters `WaitSynchronization` wakes via a 64-bit 2-probe Bloom filter + shared SRW locks, cuts GPU timestamp writes to 1-in-4 submissions, and tightens render-pass/compute barriers
- **Windows 11 Hybrid P-Core / MMCSS Pinning, RomFS Handle Cache & Live 6–8 GB VRAM Guardrail** — registers critical game/present/replay (`"Games"`) and audio (`"Pro Audio"`) threads with Windows MMCSS + per-thread Anti-EcoQoS while leaving background workers free for E-cores, caches 64 read-only RomFS `HANDLE`s + file sizes to bypass repeated NTFS/Defender scans, and dynamically trims idle buffer mirrors when live `VK_EXT_memory_budget` exceeds 85%

---

## Changed

### CPU Recompiler, Threading & OS Layer
- **Hot/Cold Split & Linker Order (`nx2/make_hot_split.py`, `nx2/build.py`, `code_hot_0..7.c`, `hot_order.txt`):** compiles `728` hot lifted functions with `-O2` + inlined `a64_fpx_hot_*` fast paths while keeping cold chunks fast-building at `-O1`, co-locating `91%` of hot call edges and packing `4,513` hot symbols at the start of `.text`
- **AVX2 Dirty-RAM Tracking (`host/rx_backend.c`):** replaces scalar byte loops with an AVX2 64-page (`256 KB`) `VPTEST` fast-skip scanner, 2-slot output buffer pool, and single `SEQ_CST` fence per collection pass
- **Kernel Multi-Wait Sync (`host/nn_os_sync.c`):** adds a 64-bit 2-probe pointer Bloom filter, shared SRWLock read path when immediately signaled, waiter-counted condition wakes, and an `8 µs` adaptive user-mode spin before kernel sleep
- **Windows 11 Hybrid P-Core / MMCSS & Anti-EcoQoS (`host/hos_thread.c`, `gpu/vk/vk_replay.cpp`):** disables per-thread `ThreadPowerThrottling` (`SetThreadInformation`) and registers frame-critical (`L"Games"`, `AVRT_PRIORITY_HIGH`) and real-time audio (`L"Pro Audio"`, `AVRT_PRIORITY_CRITICAL`) threads via `avrt.dll` (`HOS_NO_MMCSS=1` / `RX_VK_NO_MMCSS=1` fallback)
- **Read-Only RomFS LRU Handle Cache (`host/nn_fs.c`):** caches 64 read-only Win32 `HANDLE`s and `GetFileSizeEx` results for lock-free positional `OVERLAPPED` reads across room/door transitions, with automatic path invalidation on any writable open/create/delete/rename (`HOS_NO_FS_CACHE=1` fallback)

### Vulkan Renderer, Precompilation & VRAM Management
- **Shipped Pipeline State List & Upgrade Pre-Warm (`gpu/vk/vk_draw.cpp`, `gpu/vk/vk_main.cpp`, `gpu/vk/vk_shader.h`, `hookshot_pipeline_states_shipped.bin`):** implements `HKSTATE1`/`HKSTATE2` state-list preparation, active compute list tracking (`hookshot_computes.bin`), pre-opened note/state files, serialized first pipeline compile (seeding internal driver caches cleanly), and `hookshot_state_list_prepared.bin` upgrade detection so existing `0.1.7` installs automatically pre-warm newly shipped states once
- **Finalized Render-Target Upload Guard (`gpu/vk/vk_texture.cpp`):** skips CPU unswizzle + upload of recycled guest transient-heap bytes onto finalized GPU-drawn textures (`tex->gpu_written && tex->released`; `RX_VK_UPLOAD_FINALIZED_RT=1` fallback)
- **Mirror-Sync Copy Coalescing (`gpu/vk/vk_memory.cpp`):** coalesces adjacent `(srcOffset, dstOffset)` 4 KB capture-to-mirror `VkBufferCopy` regions, caches capture-buffer map lookups, and reuses `thread_local` run vectors (`RX_VK_NO_MSYNC_COALESCE=1` fallback)
- **Vendor-Smart Barriers & Timestamp Stride (`gpu/vk/vk_texture.cpp`, `gpu/vk/vk_draw.cpp`, `gpu/vk/vk_replay.cpp`):** narrows attachment layout barriers to color/depth stages where safe, skips redundant back-to-back read-only compute barriers, and samples GPU timing queries on 1-in-4 submissions (`RX_VK_CONSERVATIVE_BARRIERS=1` fallback)
- **Live `VK_EXT_memory_budget` Guardrail (`gpu/vk/vk_memory.cpp`, `gpu/vk/vk_texture.cpp`):** dynamically refreshes the 70% live texture budget ceiling and smoothly trims idle buffer mirrors down toward a `256 MB` floor whenever live VRAM usage exceeds `85%` of driver allowance, preventing WDDM PCIe spillover cliffs on 6 GB / 8 GB GPUs (`RX_VK_NO_LIVE_BUDGET=1` fallback)

### Safe Out-of-the-Box Defaults (Golden Rule)
- Fresh installs always default to a safe, vendor-neutral **1920×1080 Windowed** profile (**Upscaling = Off**, **FG engine = Off**, **Reflex = Off**, **Field of view = 100%**) while leaving existing `mastersword_options.ini` settings untouched when updating

---

## Install

| Asset | Use when |
|---|---|
| **`SteelTemple-MasterSword-0.1.8-Update.zip`** | You already have 0.1.7 / 0.1.6 / 0.1.5e / 0.1.5d / 0.1.5c / 0.1.5b / 0.1.5 / 0.1.4 — open **MasterSwordLauncher** for 1-click auto-update, or extract over your game folder |
| `SteelTemple-MasterSword-0.1.8-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

1. Close the game before updating.
2. Open **`MasterSwordLauncher.exe`** — it detects **`0.1.8`**, downloads `SteelTemple-MasterSword-0.1.8-Update.zip` (including `hookshot_pipeline_states_shipped.bin`), verifies `mastersword.exe`, and restarts into **`V 0.1.8`** automatically (or extract the Update zip directly over your game folder).
3. Launch the game — the startup warmer will prepare the newly shipped pipeline states once so gameplay runs stutter-free, and both the startup warmer and **`Insert`** menu will show **`0.1.8`**.

Your dump (`image.bin` + `romfs\`), saves (`user\`), and personal `mastersword_options.ini` stay untouched on your PC.

---

## Credits

SteelTempSaint community and testers · NotRexed and Hookshot contributors · Eden / Eglantine shader stack · AMD FidelityFX / FSR SDK · everyone testing high-FPS gameplay, ultrawide, upscalers, and hardware frame generation
