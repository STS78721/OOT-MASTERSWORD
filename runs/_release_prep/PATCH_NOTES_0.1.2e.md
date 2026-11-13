# 🗡️ SteelTempSaint's MasterSword — 0.1.2e Hotfix

## Why this build

Two fixes that were blocking players after 0.1.2c / 0.1.2d:

1. **Vulkan race on slower laptops** (e.g. i5-12450H + RTX 4060 Laptop): the display thread was still starting Vulkan when MainThread already created the NVN window and loaded `Pack/GeneralForm…`, which null-crashed. `GetNativeWindow` now waits for the renderer before returning.
2. **Launcher version stuck on 0.1.2c**: 0.1.2d shipped the same launcher binary, so some clients never saw 0.1.2d as an update. This build bumps the launcher to **0.1.2e**.

## ⚡ Changelog — 0.1.2e

* Gate `nn::vi::GetNativeWindow` on `hos_renderer_wait` (Vulkan ready before NVN window / GeneralForm boot)
* Rebuild `MasterSwordLauncher.exe` with `CURRENT_VERSION = 0.1.2e` so auto-update detects this release
* Still includes `hookshot_vk.dll` alias from **0.1.2d**
* Still includes dump layout checks from **0.1.2c** (`image.bin` + `romfs\`)
* Still includes full FFmpeg / `libstdc++` runtime set from **0.1.2b**

## Install

| File | Use |
|------|-----|
| **`SteelTemple-MasterSword-0.1.2e-Update.zip`** | Overwrite 0.1.2 / 0.1.2a–d |
| `SteelTemple-MasterSword-0.1.2e-Portable-StandAlone.zip` | Fresh install |

Requires `image.bin` + `romfs\` beside `mastersword.exe`.
