# 🗡️ SteelTempSaint's Ocarina of Time — MasterSword 🎮⚡
### *Transforming Ocarina of Time into a Flagship Modern PC Experience*

<p align="center">
  <img src="assets/mastersword.png" alt="SteelTemple MasterSword" width="220">
</p>

[![Latest Release](https://img.shields.io/github/v/release/STS78721/OOT-MASTERSWORD?label=Latest%20Release&color=brightgreen)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)
[![Release Date](https://img.shields.io/github/release-date/STS78721/OOT-MASTERSWORD?color=blue)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows%20x86--64-informational)](https://github.com/STS78721/OOT-MASTERSWORD)
[![Renderer](https://img.shields.io/badge/Renderer-Vulkan%201.3-red)](https://github.com/STS78721/OOT-MASTERSWORD)
[![DLSS](https://img.shields.io/badge/NVIDIA-DLSS%20SR-76B900)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)
[![FSR4](https://img.shields.io/badge/AMD-FSR%204-ED1C24)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)
[![DualSense](https://img.shields.io/badge/Controller-PS5%20DualSense%20Native-blueviolet)](https://github.com/STS78721/OOT-MASTERSWORD)
[![Community Feedback](https://img.shields.io/badge/Community-Feedback%20Welcome-orange)](https://github.com/STS78721/OOT-MASTERSWORD/issues)

> [!IMPORTANT]
> **No copyrighted game files are included.** You need your own legally obtained copy of *The Legend of Zelda: Ocarina of Time* (Nintendo Switch 2 remake), dumped from your own console. No Nintendo binaries, ROMs, or cryptographic keys are distributed with this repository.

---

## 🌟 Our Vision

**SteelTempSaint's MasterSword** is an enthusiast-driven, high-performance static recompilation of *The Legend of Zelda: Ocarina of Time* (Nintendo Switch 2) for native Windows x86-64. There is no emulator at run time: the game's ARM64 code is translated ahead-of-time into C, Horizon OS services are reimplemented on the host, and Switch 2 NVN graphics are rebuilt on **Vulkan 1.3**.

We aim for a **native, flagship PC experience**:
- **Next-Gen Immersion** — DualSense support with tactile feedback and selectable glyph layouts
- **Modern presentation** — **NVIDIA DLSS Super Resolution**, **AMD FSR 4**, Nodix / MasterSword frame gen, SMAA, colour grading, Reflex
- **Faithful Hyrule, sharper on PC** — foliage, terrain, and native-resolution CPU/VRAM optimizations for smooth Kokiri → Hyrule Field play
- **Community workflow** — SteelTemple Sony/Nixxes-style launcher (settings outside the game, 1-click auto-updates, save vault, mod manager)

---

## 🎮 What's in 0.1.5e

<p align="center">
  <img src="assets/launcher_showcase.png" alt="SteelTemple MasterSword Launcher" width="90%">
</p>

### ✨ MasterSword identity
* Startup pipeline warmer, Insert menu, launcher, and updater all stamped **MasterSword 0.1.5e**
* SteelTempSaint logo on the startup screen

### ⚡ 0.1.5e — Native-Res CPU, VRAM Pooling & Auto-Updater Overhaul
* **Render-target image pooling (`256 MB`) & dedicated `128 MB` VMA target blocks** — recycles render targets across scale transitions and keeps transient targets out of sampled texture blocks
* **Per-slot descriptor reuse & lockless TLS sampler cache** — enabled by default (`Descriptor reuse` / `Sampler cache`), cutting Vulkan draw-submission CPU overhead at full native resolution
* **Zero-copy submitted-page cache & multi-threaded shadow copies** — 32-slot `memory_read_as_submitted` pointer cache + parallel `CopyHelpers` shadow page updates
* **VRAM & host pacing wins** — dynamic VRAM pressure relief on mirrors/shadows, opt-in VMA compaction (`RX_VK_COMPACT=1`), zero-texture GPU clear fast-path, movie upscale fallback, reserved OS CPU core, and batched background logging
* **1-click Launcher Auto-Updater & NGX Installer hardening** — `--probe-dpi` executable verification, **FSR 4** / **FSR 3.1** (`options_version=8`) launcher sync, and standalone SHA-256-verified NVIDIA NGX DLSS `v310.9.16` download

### 🧩 From 0.1.5d (still here)
* Host reads **`mastersword_options.ini`** first (frame rate cap, output resolution, binds, audio)

### ❤️ From 0.1.5c (still here)
* **AMD FSR 4** Upscaling row (FidelityFX SDK 2.4 runtime shipped for Windows AMD GPUs)
* Tip-port Graphics polish: V-Sync, mesh shaders, uncapped FPS, upscaler probe on startup
* DLSS / FSR 4 Resolution + Native AA render scales; fullscreen Insert menu stays visible at 4K
* Coherent menu pointer samples (Windows + SDL)

### ❤️ From 0.1.5b (still here)
* Ampere shader / AMD cloth / task+mesh hardening
* Image-view retry, texel-buffer ownership, pipeline play-cache
* Present device-lost ends cleanly after a checkpoint report
* HUD / hearts, mesh fallback, long Unicode paths

### 🚀 From 0.1.4 (still here)
* **NVIDIA DLSS SR** — presets **K / M / L** + **DLAA** in the Insert menu & launcher
* **Nodix / MasterSword** frame generation + **NVIDIA Reflex**
* Launcher SETTINGS sync with the in-game Insert menu

### 🖥️ Launcher (`MasterSwordLauncher.exe`)
* **PLAY / SETTINGS / DIGITAL EXTRAS** over Master Sword key art
* Automatic 1-click update check & in-place install when a newer Update zip ships
* Pre-warm Vulkan shaders, save vault, mod manager

---

## 📥 Quick Start & Downloads

Grab packages from the **[Latest Release (0.1.5e)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)**:

### 🎮 New players — Portable
👉 **[`SteelTemple-MasterSword-0.1.5e-Portable-StandAlone.zip`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.5e/SteelTemple-MasterSword-0.1.5e-Portable-StandAlone.zip)**
1. Extract (e.g. `C:\Games\MasterSword`)
2. Put **your** dump beside it: **`image.bin` + `romfs\`**
3. Run **`MasterSwordLauncher.exe`** → SETTINGS → PLAY  
4. NVIDIA: **Download & Install DLSS SDK** in SETTINGS if `ngx\nvngx_dlss.dll` is not yet installed

```text
C:\Games\MasterSword\
├── MasterSwordLauncher.exe
├── mastersword.exe
├── mastersword_vk.dll
├── amd_fidelityfx_*_vk.dll   <-- FSR 4 (AMD)
├── mastersword_options.ini   <-- SETTINGS / first run
├── image.bin + romfs\        <-- YOUR dump
└── runtime DLLs …
```

### ⚡ Existing players — Update
👉 **[`SteelTemple-MasterSword-0.1.5e-Update.zip`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.5e/SteelTemple-MasterSword-0.1.5e-Update.zip)**  
Open **`MasterSwordLauncher.exe`** and let it download & install **`0.1.5e`** automatically in 1 click — or extract the Update zip directly over your game folder.

### 🖥️ Launcher only
👉 **[`MasterSwordLauncher.exe`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.5e/MasterSwordLauncher.exe)**

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
- **NVIDIA DLSS** — optional NGX Super Resolution (downloaded by the launcher; NVIDIA trademarks)
- **AMD FidelityFX / FSR SDK** — FSR 3.1 & FSR 4 Vulkan upscaling technology
- **SteelTempSaint community & testers** — logs, footage, and feedback across MasterSword, ToTK SteelTemple, and DKB BANANAS

---

## 📜 License

See **[LICENSE](LICENSE)** in this repository. Third-party notices live under `third_party/` / project docs where applicable.

*Thank you to everyone helping build the ultimate Ocarina of Time PC experience!* 🗡️🌲✨
