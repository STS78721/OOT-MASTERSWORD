# 🗡️ SteelTempSaint's Ocarina of Time — MasterSword 🎮⚡
### *Transforming Ocarina of Time into a Flagship Modern PC Experience*

<p align="center">
  <img src="assets/mastersword.png" alt="SteelTemple MasterSword" width="220">
</p>

[![Latest Release](https://img.shields.io/github/v/release/STS78721/OOT-MASTERSWORD?label=Latest%20Release&color=brightgreen)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)
[![Release Date](https://img.shields.io/github/release-date/STS78721/OOT-MASTERSWORD?color=blue)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows%20x86--64-informational)](https://github.com/STS78721/OOT-MASTERSWORD)
[![Renderer](https://img.shields.io/badge/Renderer-Vulkan%201.3-red)](https://github.com/STS78721/OOT-MASTERSWORD)
[![DLSS](https://img.shields.io/badge/NVIDIA-DLSS%20SR%20%2B%20MFG-76B900)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)
[![FSR4](https://img.shields.io/badge/AMD-FSR%204%20%2B%203.1-ED1C24)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)
[![DualSense](https://img.shields.io/badge/Controller-PS5%20DualSense%20Native-blueviolet)](https://github.com/STS78721/OOT-MASTERSWORD)
[![Community Feedback](https://img.shields.io/badge/Community-Feedback%20Welcome-orange)](https://github.com/STS78721/OOT-MASTERSWORD/issues)

> [!IMPORTANT]
> **No copyrighted game files are included.** You need your own legally obtained copy of *The Legend of Zelda: Ocarina of Time* (Nintendo Switch 2 remake), dumped from your own console. No Nintendo binaries, ROMs, or cryptographic keys are distributed with this repository.

---

## 🌟 Our Vision

**SteelTempSaint's MasterSword** is an enthusiast-driven, high-performance static recompilation of *The Legend of Zelda: Ocarina of Time* (Nintendo Switch 2) for native Windows x86-64. There is no emulator at run time: the game's ARM64 code is translated ahead-of-time into C, Horizon OS services are reimplemented on the host, and Switch 2 NVN graphics are rebuilt on **Vulkan 1.3**.

We aim for a **native, flagship PC experience**:
- **Next-Gen Immersion** — DualSense support with tactile feedback, native mouse camera, live gameplay FOV (`50%–150%`), and selectable glyph layouts
- **Modern presentation** — **NVIDIA DLSS Super Resolution & Hardware Multi-Frame Generation (2x–4x)**, **AMD FSR 4 & FSR 3.1 FG**, MasterSword / Nodix frame gen, SMAA, colour grading, **5K2K (`5120×2160`) ultrawide**, **NVIDIA Reflex** (active even with Frame Generation!)
- **Faithful Hyrule, sharper on PC** — foliage, terrain, reactive alpha masks, true >60 FPS physics/audio/clock accuracy, shipped zero-stutter pipeline precompilation, and native-resolution CPU/AVX2/VRAM optimizations
- **Community workflow** — SteelTemple Sony/Nixxes-style launcher (safe 1080p out-of-the-box baseline, settings outside the game, 1-click auto-updates, save vault, mod manager)

---

## 🎮 What's in 0.1.8

<p align="center">
  <img src="assets/launcher_showcase.png" alt="SteelTemple MasterSword Launcher" width="90%">
</p>

### ✨ MasterSword identity
* Startup pipeline warmer, Insert menu, launcher, and updater all stamped **MasterSword 0.1.8**
* SteelTempSaint logo on the startup screen & safe 1080p vendor-neutral out-of-the-box defaults

### 🚀 0.1.8 — Major CPU/GPU Performance, Zero-Stutter Precompilation & Cross-PC Frame-Pacing Overhaul
* **Hot/Cold Recompiled Code Split (`-O2`) & `.text` Call-Locality Packing** — isolates `728` hot lifted functions (`99.1%` of CPU execution) with inlined FP fast paths and orders `4,513` hot call-graph neighbours contiguously at the start of `.text`, slashing instruction-cache & iTLB misses
* **Shipped `HKSTATE2` Pipeline State List (`41,839` Pipelines)** — bundles `hookshot_pipeline_states_shipped.bin` with upgrade-aware startup pre-warming so pipelines compile ahead of time instead of stuttering in combat or new areas
* **Zero Recycled Render-Target Upload Spikes & ~10–15× Fewer Mirror-Sync Copies** — eliminates 12–17 ms `8–32 MB` CPU unswizzle spikes over finalized GPU render targets and coalesces contiguous 4 KB capture-to-mirror `VkBufferCopy` transfers
* **AVX2 Dirty-RAM `VPTEST` Scanner, Bloom-Filtered Multi-Wait Sync & Vendor-Smart Barriers** — faster write-watch page scanning, lighter kernel thread wakes, 1-in-4 GPU timestamp sampling, and tighter Vulkan render-pass/compute barriers
* **Windows 11 Hybrid P-Core / MMCSS Scheduling, RomFS Handle Cache & Live 6–8 GB VRAM Guardrail** — pins frame-critical and audio threads onto P-cores / V-Cache CCDs via MMCSS + per-thread Anti-EcoQoS, caches 64 read-only RomFS handles across room transitions, and dynamically trims idle buffer mirrors when live VRAM crosses 85%

### 🏹 From 0.1.7 (still here)
* **Live Gameplay Field of View (`50%–150%`)**, **>60 FPS moving-platform physics, 60 Hz SoundJob gating, time-of-day & movie pacing fixes**, **AMD FSR 3.1 on the shared temporal chain**, **room-transition heap retry**, and **`5120×2160` (5K2K) 21:9 ultrawide**

### 🔥 From 0.1.6 & 0.1.5c–0.1.5e (still here)
* **Real Hardware NVIDIA NGX DLSS Frame Generation (`nvngx_dlssg.dll`, 2x–4x)** + **NVIDIA Reflex**, **AMD FSR 4 & FSR 3.1 FG**, **Transparency Reactive Mask**, **Render-target image pooling (`256 MB`)**, and **1-click NGX SDK installer**

### 🖥️ Launcher (`MasterSwordLauncher.exe`)
* **PLAY / SETTINGS / DIGITAL EXTRAS** over Master Sword key art
* Automatic 1-click update check & in-place install when a newer Update zip ships
* Pre-warm Vulkan shaders, save vault, mod manager

---

## 📥 Quick Start & Downloads

Grab packages from the **[Latest Release (0.1.8)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)**:

### 🎮 New players — Portable
👉 **[`SteelTemple-MasterSword-0.1.8-Portable-StandAlone.zip`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.8/SteelTemple-MasterSword-0.1.8-Portable-StandAlone.zip)**
1. Extract (e.g. `C:\Games\MasterSword`)
2. Put **your** dump beside it: **`image.bin` + `romfs\`**
3. Run **`MasterSwordLauncher.exe`** → SETTINGS → PLAY  
4. NVIDIA: **Download & Install DLSS SDK** in SETTINGS to install `ngx\nvngx_dlss.dll` (DLSS SR) & `ngx\nvngx_dlssg.dll` (DLSS MFG)

```text
C:\Games\MasterSword\
├── MasterSwordLauncher.exe
├── mastersword.exe
├── mastersword_vk.dll
├── hookshot_pipeline_states_shipped.bin  <-- 41,839 pre-baked pipeline states
├── amd_fidelityfx_*_vk.dll               <-- FSR 4 (AMD)
├── ngx\                                  <-- DLSS SR + DLSS-G (installed via Launcher)
├── mastersword_options.ini               <-- SETTINGS / first run (defaults to safe 1080p)
├── image.bin + romfs\                    <-- YOUR dump
└── runtime DLLs …
```

### ⚡ Existing players — Update
👉 **[`SteelTemple-MasterSword-0.1.8-Update.zip`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.8/SteelTemple-MasterSword-0.1.8-Update.zip)**  
Open **`MasterSwordLauncher.exe`** and let it download & install **`0.1.8`** automatically in 1 click — or extract the Update zip directly over your game folder.

### 🖥️ Launcher only
👉 **[`MasterSwordLauncher.exe`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.8/MasterSwordLauncher.exe)**

---

## 🎮 In-game shortcuts

| Action | Shortcut |
| :--- | :--- |
| **Insert menu** (graphics / MasterSword options) | **`Insert`** |
| **Performance overlay** | **`F10`** (where enabled) |
| **Fullscreen / windowed** | **`F11`** or **`Alt + Enter`** |

---

## 🛠️ Credits & Acknowledgments

- **NotRexed & Hookshot contributors** — static recompilation architecture, Horizon OS host, and NVN → Vulkan work this project builds on
- **Eden / Eglantine** — Ampere → Maxwell shader lowering used by the Vulkan path
- **EvilBoyce / evilmfkr (EvilOOT)** and other community GPU / foliage contributors
- **NVIDIA DLSS** — optional NGX Super Resolution & Frame Generation (downloaded by the launcher; NVIDIA trademarks)
- **AMD FidelityFX / FSR SDK** — FSR 3.1 & FSR 4 Vulkan upscaling and frame interpolation technology
- **SteelTempSaint community & testers** — logs, footage, and feedback across MasterSword, ToTK SteelTemple, and DKB BANANAS

---

## 📜 License

See **[LICENSE](LICENSE)** in this repository. Third-party notices live under `third_party/` / project docs where applicable.

*Thank you to everyone helping build the ultimate Ocarina of Time PC experience!* 🗡️🌲✨
