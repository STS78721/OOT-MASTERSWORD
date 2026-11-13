# 🗡️ SteelTempSaint's MasterSword — 0.1.3

**Output resolution unlock** for DLSS / FSR / XeSS and high-res displays, ported from upstream Hookshot (#849 / e706b9d), kept on SteelTemple MasterSword branding.

> **No copyrighted game files are included.**

---

## ⚡ What's New in 0.1.3

* **Output resolution** Insert-menu row: **2048×1152** (stock), **2560×1440**, **3840×2160**
  * Also via `output_res=` in `mastersword_options.ini` or `HOS_OUTPUT_RES=0|1|2`
* Full-size 3D path at 4K output + render-target pool scaling for larger frames
* Resolution row reaches the 3D path (upstream fixed-scale / scene wiring)
* Keeps MasterSword branding, launcher, and the 0.1.2a–e packaging / Vulkan wait fixes

### Still included from 0.1.2e
* Full FFmpeg/runtime DLL bundle (libstdc++, libaom, cairo, glib, …)
* `hookshot_vk.dll` alias beside `mastersword_vk.dll`
* Renderer wait before guest boot (laptop race mitigation)
* Launcher accepts `image.bin` + `romfs\`

---

## 📦 Downloads

| Package | Use this if… |
|---------|----------------|
| **`SteelTemple-MasterSword-0.1.3-Update.zip`** | You already have 0.1.2 / 0.1.2a–e — **overwrite** into the game folder |
| `SteelTemple-MasterSword-0.1.3-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

After updating, open Insert → **Output resolution** and pick **2560×1440** or **3840×2160** if you use an upscaler or a 1440p/4K display.

---

**Use your own game data. Play fair.**
