# 🗡️ SteelTempSaint's MasterSword — 0.1.2b Hotfix

## Sorry again

0.1.2a fixed most missing FFmpeg DLLs, but one MinGW runtime was still omitted because of a packaging bug (`+` in the filename):

- **`libstdc++-6.dll was not found`**

**0.1.2b** adds `libstdc++-6.dll` (and re-packs the full runtime set with a corrected dependency scanner). Please overwrite with the Update zip. Sorry for the extra round-trip.

> **No copyrighted game files are included.**

---

## ⚡ Changelog — 0.1.2b

* Include **`libstdc++-6.dll`** (required by cairo / jxl / vpl / x265 / …)
* Fixed DLL collector regex so `libstdc++-*.dll` names are no longer skipped
* Everything from **0.1.2a** still included (full FFmpeg tree, launcher `image.bin`+`romfs` support)

---

## 📦 Downloads

| Package | Use |
|---------|-----|
| **`SteelTemple-MasterSword-0.1.2b-Update.zip`** | Overwrite your 0.1.2 / 0.1.2a folder |
| `SteelTemple-MasterSword-0.1.2b-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher only |

After updating, `libstdc++-6.dll` must sit beside `mastersword.exe`.

---

**Use your own game data. Play fair.**
