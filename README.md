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
- **Next-Gen Immersion** — DualSense support with tactile feedback and selectable glyph layouts
- **Modern presentation** — **NVIDIA DLSS Super Resolution & Hardware Multi-Frame Generation (2x–4x)**, **AMD FSR 4 & FSR 3.1 FG**, MasterSword / Nodix frame gen, SMAA, colour grading, **NVIDIA Reflex** (active even with Frame Generation!)
- **Faithful Hyrule, sharper on PC** — foliage, terrain, reactive alpha masks, and native-resolution CPU/FMA/VRAM optimizations for smooth Kokiri → Hyrule Field play
- **Community workflow** — SteelTemple Sony/Nixxes-style launcher (safe 1080p out-of-the-box baseline, settings outside the game, 1-click auto-updates, save vault, mod manager)

---

## 🎮 What's in 0.1.6

<p align="center">
  <img src="assets/launcher_showcase.png" alt="SteelTemple MasterSword Launcher" width="90%">
</p>

### ✨ MasterSword identity
* Startup pipeline warmer, Insert menu, launcher, and updater all stamped **MasterSword 0.1.6**
* SteelTempSaint logo on the startup screen & safe 1080p vendor-neutral out-of-the-box defaults

### 🚀 0.1.6 — Hardware NGX DLSS Frame Generation, Reactive Masks, AVX2/FMA & GPU Stability
* **Real Hardware NVIDIA NGX DLSS Frame Generation (`nvngx_dlssg.dll`, 2x–4x)** — **FG engine = DLSS MFG** now drives native NGX Frame Generation directly from temporal depth, dilated + actor motion vectors, and camera matrices (works both with DLSS SR / DLAA **and** at pure **Native 1080p / 1440p / 4K** with Upscaling = Off!)
* **3x/4x Present-Ahead + V-Sync Auto FIFO + NVIDIA Reflex under FG** — submits multi-frame sets ahead of time with spare swapchain images, uses FIFO on VRR displays under DLSS-G, and keeps **NVIDIA Reflex** active on real game frames while Frame Generation is on
* **Transparency Reactive Mask & Negative LOD Mip Bias** — prevents alpha/particle ghosting on water splashes, fire, and dust under DLSS & FSR 3.1, and sharpens textures under sub-native DLSS presets; also wires full **AMD FSR 3.1 SR & FSR 3.1 Frame Interpolation**
* **Hardware AVX2/FMA Single-Precision Math & Faster Shader Prep** — fuses `FMADD`/`FRECPS`/`FRSQRTS` into single `vfmadd` instructions across all lifted game chunks (22–45% faster on FMA chains) and reuses per-thread shader translation pools (~20–30% faster shader preparation)
* **Major Stability & >60 FPS World Fixes** — fixes the Hyrule Castle tree-shadow GPU hang, 3D BC slice render-target `VK_ERROR_DEVICE_LOST` (enabling foliage LOD dither safely), Kokiri Forest flashing mesh triangles, VRAM compaction active-texture guards, and frozen water ripples + pick-up counters above 60 FPS
* **1-Click Dual NGX SDK Installer in Launcher** — **Download & Install DLSS SDK** in `MasterSwordLauncher.exe` now fetches and SHA-256-verifies both `ngx\nvngx_dlss.dll` and `ngx\nvngx_dlssg.dll` (`v310.9.16`)

### ⚡ From 0.1.5e (still here)
* **Render-target image pooling (`256 MB`) & dedicated `128 MB` VMA target blocks**
* **Per-slot descriptor reuse & lockless TLS sampler cache** — cuts Vulkan draw-submission CPU overhead at native resolution
* **Zero-copy submitted-page cache & multi-threaded shadow copies**, reserved OS CPU core, and `--probe-dpi` 1-click launcher update verification

### 🧩 From 0.1.5c–0.1.5d (still here)
* Host reads **`mastersword_options.ini`** first (frame rate cap, output resolution, binds, audio)
* **AMD FSR 4** Upscaling row (FidelityFX SDK 2.4 runtime shipped for Windows AMD GPUs), V-Sync, mesh shaders, uncapped FPS, fullscreen 4K Insert menu scaling

### 🖥️ Launcher (`MasterSwordLauncher.exe`)
* **PLAY / SETTINGS / DIGITAL EXTRAS** over Master Sword key art
* Automatic 1-click update check & in-place install when a newer Update zip ships
* Pre-warm Vulkan shaders, save vault, mod manager

---

## 📥 Quick Start & Downloads

Grab packages from the **[Latest Release (0.1.6)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)**:

### 🎮 New players — Portable
👉 **[`SteelTemple-MasterSword-0.1.6-Portable-StandAlone.zip`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.6/SteelTemple-MasterSword-0.1.6-Portable-StandAlone.zip)**
1. Extract (e.g. `C:\Games\MasterSword`)
2. Put **your** dump beside it: **`image.bin` + `romfs\`**
3. Run **`MasterSwordLauncher.exe`** → SETTINGS → PLAY  
4. NVIDIA: **Download & Install DLSS SDK** in SETTINGS to install `ngx\nvngx_dlss.dll` (DLSS SR) & `ngx\nvngx_dlssg.dll` (DLSS MFG)

```text
C:\Games\MasterSword\
├── MasterSwordLauncher.exe
├── mastersword.exe
├── mastersword_vk.dll
├── amd_fidelityfx_*_vk.dll   <-- FSR 4 (AMD)
├── ngx\                      <-- DLSS SR + DLSS-G (installed via Launcher)
├── mastersword_options.ini   <-- SETTINGS / first run (defaults to safe 1080p)
├── image.bin + romfs\        <-- YOUR dump
└── runtime DLLs …
```

### ⚡ Existing players — Update
👉 **[`SteelTemple-MasterSword-0.1.6-Update.zip`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.6/SteelTemple-MasterSword-0.1.6-Update.zip)**  
Open **`MasterSwordLauncher.exe`** and let it download & install **`0.1.6`** automatically in 1 click — or extract the Update zip directly over your game folder.

### 🖥️ Launcher only
👉 **[`MasterSwordLauncher.exe`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.6/MasterSwordLauncher.exe)**

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
