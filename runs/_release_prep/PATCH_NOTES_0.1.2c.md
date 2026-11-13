# 🗡️ SteelTempSaint's MasterSword — 0.1.2c Hotfix

## What was actually going wrong

After the DLL hotfixes, the same client still saw:

```text
usage: …\mastersword.exe image.bin --romfs <dir> --user <dir>
```

and launcher **exit code 2**.

That is **not** a missing DLL. Exit code 2 means **`image.bin` was not found beside `mastersword.exe`**. The game folder only had the PC build — the dump was never next to it (or PLAY launched with no dump args). Sorry the earlier “missing DLL” popup was misleading for code 2.

---

## ⚡ Changelog — 0.1.2c

* Launcher **requires** `image.bin` + `romfs\` in the game folder before PLAY
* Launcher starts the game as:  
  `mastersword.exe image.bin --romfs romfs --user user`
* Exit code **2** now explains the missing dump (not DLLs)
* Still includes the full runtime DLL set from **0.1.2b** (`libstdc++-6.dll`, FFmpeg tree, …)

---

## ✅ Correct folder layout

```text
C:\Games\…\MasterSword\
├── MasterSwordLauncher.exe
├── mastersword.exe
├── mastersword_vk.dll
├── image.bin              ← YOUR dump (required)
├── romfs\                 ← YOUR dump (required)
├── user\                  ← created on first run
└── (runtime DLLs…)
```

No Nintendo files are included in the zip — you must copy `image.bin` and `romfs\` yourself.

---

## 📦 Downloads

| Package | Use |
|---------|-----|
| **`SteelTemple-MasterSword-0.1.2c-Update.zip`** | Overwrite 0.1.2 / 0.1.2a / 0.1.2b |
| `SteelTemple-MasterSword-0.1.2c-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only |

---

**Use your own game data. Play fair.**
