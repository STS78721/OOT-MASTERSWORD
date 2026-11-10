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
[![DualSense](https://img.shields.io/badge/Controller-PS5%20DualSense%20Native-blueviolet)](https://github.com/STS78721/OOT-MASTERSWORD)
[![Community Feedback](https://img.shields.io/badge/Community-Feedback%20Welcome-orange)](https://github.com/STS78721/OOT-MASTERSWORD/issues)

> [!IMPORTANT]
> **No copyrighted game files are included.** You need your own legally obtained copy of *The Legend of Zelda: Ocarina of Time* (Nintendo Switch 2 remake), dumped from your own console. No Nintendo binaries, ROMs, or cryptographic keys are distributed with this repository.

---

## 🌟 Our Vision

**SteelTempSaint's MasterSword** is an enthusiast-driven, high-performance static recompilation of *The Legend of Zelda: Ocarina of Time* (Nintendo Switch 2) for native Windows x86-64. There is no emulator at run time: the game's ARM64 code is translated ahead-of-time into C, Horizon OS services are reimplemented on the host, and Switch 2 NVN graphics are rebuilt on **Vulkan 1.3**.

We aim for a **native, flagship PC experience**:
- **Next-Gen Immersion** — DualSense support with tactile feedback and selectable glyph layouts
- **Modern presentation** — **NVIDIA DLSS Super Resolution**, Nodix frame gen, SMAA, colour grading, Reflex
- **Faithful Hyrule, sharper on PC** — foliage, terrain, and present-path polish for Kokiri → Field
- **Community workflow** — SteelTemple Sony/Nixxes-style launcher (settings outside the game, 1-click updates, save vault, mod manager)

---

## 🎮 What's in 0.1.5

<p align="center">
  <img src="assets/launcher_showcase.png" alt="SteelTemple MasterSword Launcher" width="90%">
</p>

### ✨ MasterSword identity
* Startup warmer, Insert menu, launcher, and updater all branded **MasterSword 0.1.5**
* SteelTempSaint logo on the startup screen

### ❤️ Stability & feel
* HUD / hearts stay visible after pause (smarter texture memory)
* Smoother Kokiri and busy scenes on many PCs
* Better AMD lighting on cards that use wide warps
* Stronger mesh fallback — fewer missing pieces when the fallback path is active
* Long / Unicode game folder paths work more reliably

### 🚀 From 0.1.4 (still here)
* **NVIDIA DLSS SR** — presets **K / M / L** in the Insert menu & launcher
* **Nodix DLSS MFG** stand-in frame generation + **NVIDIA Reflex**
* Launcher SETTINGS sync with the in-game Insert menu

### 🖥️ Launcher (`MasterSwordLauncher.exe`)
* **PLAY / SETTINGS / DIGITAL EXTRAS** over Master Sword key art
* Auto-update check on launch when a newer Update zip ships
* Pre-warm Vulkan shaders, save vault, mod manager

---

## 📥 Quick Start & Downloads

Grab packages from the **[Latest Release (0.1.5)](https://github.com/STS78721/OOT-MASTERSWORD/releases/latest)**:

### 🎮 New players — Portable
👉 **[`SteelTemple-MasterSword-0.1.5-Portable-StandAlone.zip`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.5/SteelTemple-MasterSword-0.1.5-Portable-StandAlone.zip)**
1. Extract (e.g. `C:\Games\MasterSword`)
2. Put **your** dump beside it: **`image.bin` + `romfs\`**
3. Run **`MasterSwordLauncher.exe`** → SETTINGS → PLAY  
4. NVIDIA: **Download & Install DLSS SDK** if Streamline / `nvngx_dlss.dll` are missing

```text
C:\Games\MasterSword\
├── MasterSwordLauncher.exe
├── mastersword.exe
├── mastersword_vk.dll
├── mastersword_options.ini   <-- SETTINGS / first run
├── image.bin + romfs\        <-- YOUR dump
└── runtime DLLs …
```

### ⚡ Existing players — Update
👉 **[`SteelTemple-MasterSword-0.1.5-Update.zip`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.5/SteelTemple-MasterSword-0.1.5-Update.zip)**  
Overwrite your game folder — or just open the launcher and let it install automatically.

### 🖥️ Launcher only
👉 **[`MasterSwordLauncher.exe`](https://github.com/STS78721/OOT-MASTERSWORD/releases/download/0.1.5/MasterSwordLauncher.exe)**

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
- **NVIDIA DLSS** — optional Streamline / NGX Super Resolution (downloaded by the launcher; NVIDIA trademarks)
- **SteelTempSaint community & testers** — logs, footage, and feedback across MasterSword, ToTK SteelTemple, and DKB BANANAS

---

## 📜 License

See **[LICENSE](LICENSE)** in this repository. Third-party notices live under `third_party/` / project docs where applicable.

*Thank you to everyone helping build the ultimate Ocarina of Time PC experience!* 🗡️🌲✨
