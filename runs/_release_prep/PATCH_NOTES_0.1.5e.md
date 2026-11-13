# MasterSword 0.1.5e

**Release date:** 2026-11-11  
**Type:** Performance, VRAM & Updater update (drop-in over 0.1.5d / 0.1.5c / 0.1.5b)

Native-resolution CPU, descriptor/sampler, and VRAM pooling overhaul on top of 0.1.5d, plus a critical launcher auto-updater integrity-probe fix so in-place 1-click updates from earlier MasterSword builds pull and install cleanly.

---

## Highlights

- **Render-target image pooling (`256 MB` cap) & dedicated `128 MB` VMA target blocks** — eliminates `vkCreateImage` / `vkDestroyImage` churn during resolution scaling and transient post-FX passes
- **Per-slot descriptor reuse & lockless TLS sampler parameter cache** — enabled by default (`options_version=8`), cutting replay CPU overhead on heavy Hyrule Field / Kokiri draw submission without lowering render resolution
- **32-slot zero-copy `memory_read_as_submitted` page cache** & **multi-threaded in-place shadow texture page copies** — slashes CPU/RAM bandwidth spikes during GPU write-back and texture capture passes
- **Dynamic VRAM pressure relief** on mirrors & write-back diff shadows, plus opt-in VMA texture pool compaction (`RX_VK_COMPACT=1`)
- **GPU clear fast-path** for zero-filled textures, **movie coopmat linear fallback**, **reserved OS CPU core** on `>= 4`-thread PCs, and **batched background host log writer**
- **Launcher 1-click auto-updater & DLSS SDK installer hardening** — `--probe-dpi` integrity check implemented in `mastersword.exe`, self-contained NGX DLSS `v310.9.16` downloader, and full **FSR 4** / **FSR 3.1** settings sync in `MasterSwordLauncher.exe`

---

## Changed

### Vulkan Renderer — VRAM & Render Targets
- **Render-target image pool (`vk_texture.cpp`, `vk_vma.cpp`):** destroyed render-target `VkImage` + VMA allocations are recycled by `(format, extent, samples, usage)` up to **256 MB** instead of tearing down GPU memory across resolution changes
- **Dedicated VMA render-target blocks (`128 MB`):** render targets allocate from their own VMA block pool so transient targets never fragment sampled world-texture blocks
- **Shifted-overlap sweep (`target_overlap.h`, `keep_rendered.h`):** transient sub-target views unused for 10 presents are evicted automatically (`vk: render targets' images from blocks of their own`)
- **Dynamic VRAM pressure relief (`vram_pressure.h`, `vk_memory.cpp`):** automatically trims mirror allocations and write-back diff shadows when device-local VRAM approaches budget
- **Opt-in VMA texture pool compaction (`RX_VK_COMPACT=1`, `vram_compact.h`):** packs cold sampled textures out of sparse VMA blocks between frames when enabled

### Vulkan Replay — Native-Resolution CPU Wins
- **Per-slot descriptor reuse (`desc_slotgen`, `vk_draw.cpp`):** draws retain cached descriptor sets across texture registrations on slots the shader does not sample (`menu: Descriptor reuse`, on by default)
- **Lockless TLS sampler cache (`sampler_cache`, `vk_bank.cpp`, `vk_draw.cpp`, `vk_replay.cpp`):** 2-way `[2][4096]` per-thread sampler table answers sampler parameters without taking the global registration lock (`menu: Sampler cache`, on by default)
- **32-slot zero-copy submitted-page cache (`vk_memory.cpp`):** `memory_read_as_submitted` serves repeated reads from a direct guest/mirror pointer cache without temporary heap copies
- **Multi-threaded in-place shadow texture copies (`vk_memory.cpp`):** parallel `CopyHelpers` worker fan-out for large write-back shadow page updates
- **Zero-texture GPU clear fast-path (`vk_texture.cpp`):** all-zero uploads clear on the GPU instead of staging zero buffers
- **Movie upscale policy (`movie_upscale_policy.h`):** falls back cleanly to linear scaling during pre-rendered video playback when cooperative-matrix upscale is active

### Host Runtime & Launcher (`MasterSwordLauncher.exe`)
- **Reserved OS CPU core (`hos_thread.c`):** caps guest worker vcores to `max(1, n - 1)` on systems with 4+ hardware threads so Windows audio, driver, and compositor threads never starve
- **Batched background log writer (`hos_thread.c`):** offloads `hos_log` disk writes off critical game/render threads
- **Update integrity probe (`hos_main.c`):** implements `--probe-dpi` (`probe-dpi: PASS`) and `--warm-shaders` so `MasterSwordLauncher.exe` (including existing `0.1.4`–`0.1.5d` launchers) validates and installs `0.1.5e` updates seamlessly in 1 click
- **Launcher settings & NGX installer (`MasterSwordLauncher.py`):** adds **FSR 4** (`upscaling=9`) and updated **FSR 3.1** (`options_version=8`) quality preset sync, version-tuple comparison (`0.1.5e > 0.1.5d`), and standalone SHA-256-verified NVIDIA NGX DLSS `v310.9.16` runtime download

---

## Unchanged (carry-forward)

- **0.1.5d:** Host reads **`mastersword_options.ini`** first (`Game frame rate cap`, output resolution, ultrawide, audio, binds)
- **0.1.5c:** **AMD FSR 4** Upscaling row (FidelityFX SDK 2.4 Vulkan DLLs included), V-Sync / mesh-shader / uncapped FPS rows, fullscreen 4K Insert menu scaling, coherent pointer mailbox
- **0.1.4 / 0.1.5b:** **NVIDIA DLSS Super Resolution** (presets K / M / L + DLAA), **Nodix / MasterSword** frame generation (up to 4x+), **NVIDIA Reflex**, DualSense support, and MasterSword identity across startup warmer, Insert menu, launcher, and updater

---

## Install

| Asset | Use when |
|---|---|
| **`SteelTemple-MasterSword-0.1.5e-Update.zip`** | You already have 0.1.5d / 0.1.5c / 0.1.5b / 0.1.5 / 0.1.4 — open **MasterSwordLauncher** for 1-click auto-update, or extract over your game folder |
| `SteelTemple-MasterSword-0.1.5e-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

1. Close the game before updating.
2. Open **`MasterSwordLauncher.exe`** — it detects **`0.1.5e`**, downloads `SteelTemple-MasterSword-0.1.5e-Update.zip`, verifies `mastersword.exe`, and restarts into **`V 0.1.5e`** automatically (or extract the Update zip directly over your game folder).
3. Launch the game and confirm the startup pipeline warmer and **`Insert`** menu both show **`0.1.5e`**.

Your dump (`image.bin` + `romfs\`), saves (`user\`), and `mastersword_options.ini` stay untouched on your PC.

---

## Credits

SteelTempSaint community and testers · NotRexed and Hookshot contributors · Eden / Eglantine shader stack · AMD FidelityFX / FSR SDK · everyone testing native-res performance, upscalers, and frame generation
