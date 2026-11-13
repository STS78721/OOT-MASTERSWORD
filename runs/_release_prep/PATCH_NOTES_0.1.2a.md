# 🗡️ SteelTempSaint's MasterSword — 0.1.2a Hotfix

## We're sorry

Sorry about the broken 0.1.1 / 0.1.2 packages. Portable and Update zips shipped **without FFmpeg’s transitive runtime DLLs**, so many players hit Windows errors as soon as `mastersword.exe` started — for example:

- `libaom.dll was not found`
- `libvpl-2.dll was not found`
- `libglib-2.0-0.dll was not found`
- `libcairo-2.dll was not found`
- `libgobject-2.0-0.dll was not found`

That also made launcher **PLAY** look like it “did nothing” (the process died immediately, or the launcher rejected a normal Hookshot dump layout).

**0.1.2a fixes the packaging and the launcher.** Please overwrite with the Update zip (or re-extract Portable). Thank you for the reports — and again, sorry for the friction.

> **No copyrighted game files are included.** You need your own legally obtained dump.

---

## ⚡ Changelog — 0.1.2a Hotfix

### 🐛 Critical fixes
* Ship the **full FFmpeg / media runtime DLL set** (~85 DLLs) in both Portable and Update packages (`libaom`, `libcairo`, `libglib`, `libvpl`, …)
* Launcher **PLAY** accepts the recommended Hookshot layout: **`image.bin` + `romfs\`**
* Clearer message if the game exits immediately due to missing runtimes

### ✅ Still included from 0.1.2
* Remade CPU textures re-upload after eviction (garbled blocks on low VRAM)
* NVIDIA Windows returns retired memory pools by default
* Vulkan host API v4 + v5 ABI compatibility
* Foliage / field ports, transparent compass icon, TotK-style README showcase

---

## 📦 Downloads

| Package | Use this if… |
|---------|----------------|
| **`SteelTemple-MasterSword-0.1.2a-Update.zip`** | You already installed 0.1.1 / 0.1.2 — **extract over your game folder** |
| `SteelTemple-MasterSword-0.1.2a-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

After updating, `libaom.dll`, `libcairo-2.dll`, `libglib-2.0-0.dll`, etc. should sit beside `mastersword.exe`.

---

## 🙏 Credits
* Testers who sent the missing-DLL screenshots — thank you
* SteelTempSaint community
* Upstream Hookshot / FFmpeg runtime stack

**Use your own game data. Play fair.**
