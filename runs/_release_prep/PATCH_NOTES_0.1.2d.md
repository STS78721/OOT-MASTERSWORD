# 🗡️ SteelTempSaint's MasterSword — 0.1.2d Hotfix

## Root cause (from `hookshot_log.1.txt`)

The dump **is** loading now (`romfs` mounts). The real failure is:

```text
rx: RX_BACKEND=vulkan, but …\hookshot_vk.dll did not load (126): WebGPU
display: no WebGPU device: the window stays black
CRASH: exception 0xc0000005 …
```

The shipping `mastersword.exe` still loads **`hookshot_vk.dll`**, but 0.1.2 / 0.1.2a–c packages only shipped **`mastersword_vk.dll`**. Vulkan never started → black window → crash.

Sorry — that was on us.

---

## ⚡ Changelog — 0.1.2d

* Ship **both** `hookshot_vk.dll` and `mastersword_vk.dll` (same Vulkan backend)
* Still includes full FFmpeg / `libstdc++` runtime set from 0.1.2b
* Still includes launcher dump checks from 0.1.2c (`image.bin` + `romfs\`)

---

## ✅ After update, game folder must contain

```text
mastersword.exe
hookshot_vk.dll          ← NEW in this hotfix (copy of the Vulkan backend)
mastersword_vk.dll
image.bin + romfs\       ← your dump
(+ runtime DLLs)
```

---

## 📦 Downloads

| Package | Use |
|---------|-----|
| **`SteelTemple-MasterSword-0.1.2d-Update.zip`** | Overwrite 0.1.2 / 0.1.2a–c |
| `SteelTemple-MasterSword-0.1.2d-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher only |

---

**Use your own game data. Play fair.**
