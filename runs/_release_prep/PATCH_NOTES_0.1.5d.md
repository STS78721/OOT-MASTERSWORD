# MasterSword 0.1.5d

**Release date:** 2026-11-11  
**Type:** Hotfix (drop-in over 0.1.5c)

Host options now read `mastersword_options.ini` (the file the Insert menu and launcher actually write). In 0.1.5c the host still looked for legacy `hookshot_options.ini`, so **Game frame rate cap**, output resolution, binds, and a few other host-side settings never applied after a restart.

---

## Highlights

- Host reads **`mastersword_options.ini` first**, then falls back to `hookshot_options.ini`
- **Game frame rate cap** (90 / 120 / Uncapped, …) now sticks after restart
- Same fix for host-side reads of output / ultrawide / audio / binds / prompts

---

## Changed

### Runtime
- `hos_open_options_r`: prefer `mastersword_options.ini` wherever the host used to open only `hookshot_options.ini`

---

## Unchanged (carry-forward)

- Everything from **0.1.5c** (FSR 4 path, tip ports, fullscreen menu fit, DLSS SR / FG / Reflex)

---

## Install

| Asset | Use when |
|---|---|
| **`SteelTemple-MasterSword-0.1.5d-Update.zip`** | You already have 0.1.5c / 0.1.5b — extract over the game folder, or let the launcher auto-update |
| `SteelTemple-MasterSword-0.1.5d-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

1. Close the game (and launcher, if updating manually).
2. Prefer **MasterSwordLauncher** — it detects `0.1.5d` and can install the Update zip automatically.
3. Or extract the Update zip into your game directory.
4. Confirm the warmer / Insert menu show **0.1.5d**.
5. Set **Gameplay → Game frame rate cap**, then **restart once** for it to apply.

Your dump (`image.bin` + `romfs\`), saves, and `mastersword_options.ini` stay on your PC.

---

## Credits

SteelTempSaint community and testers · NotRexed and Hookshot contributors · Eden / Eglantine shader stack · AMD FidelityFX / FSR SDK · everyone who filed FG, menu, and upscaler reports
