# MasterSword 0.1.5b

**Release date:** 2026-11-10  
**Type:** Maintenance update (drop-in over 0.1.5)

A focused stability and compatibility pass on top of 0.1.5. MasterSword branding, NVIDIA DLSS Super Resolution, Nodix frame generation, and Reflex are unchanged.

---

## Highlights

- Broader shader coverage for Ampere fragment and mesh edge cases
- Hardened AMD paths for cloth and task/mesh workloads
- Safer texture view creation and guest-memory write tracking
- Pipeline cache split so play-time compiles do not inflate the startup cache
- Cleaner handling when the GPU reports `DEVICE_LOST`

---

## Changed

### Rendering & shaders
- Additional Ampere → Maxwell lowering paths for previously failing fragment and mesh programs
- Mesh fallback vertex defaults and `ISBERD` output handling
- TXQ array-type handling in the shader path

### AMD
- Cloth pipeline (`0x5fa56485…`) built without the proprietary optimizer when required
- Unknown-result graphics pipelines may retry with optimization disabled
- Optional task+mesh compute fallback on RX 6700 XT–class devices (`RX_VK_TASK_MESH_FALLBACK`)

### Memory & textures
- Failed image views are not permanently cached; retry is allowed on the next frame after host/device OOM
- Texel-buffer ownership tracking
- Write-back shadow invalidation for CPU-written guest pages

### Runtime
- Pipeline “play” cache: pipelines compiled during play are saved separately and merged on later starts
- Present / submit / reset paths that return `VK_ERROR_DEVICE_LOST` report once and exit cleanly
- Frame-log timestamps use the host clock
- Long-path / relocation and exclusive-mark fixes from the 0.1.5b port set

---

## Unchanged (carry-forward)

- MasterSword identity (startup, Insert menu, launcher, updater)
- NVIDIA DLSS SR (Streamline presets K / M / L)
- Nodix DLSS MFG stand-in frame generation
- NVIDIA Reflex
- Launcher ↔ Insert menu settings sync

---

## Install

| Asset | Use when |
|---|---|
| **`SteelTemple-MasterSword-0.1.5b-Update.zip`** | You already have 0.1.5 or 0.1.4 — extract over the game folder, or let the launcher auto-update |
| `SteelTemple-MasterSword-0.1.5b-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

1. Close the game (and launcher, if updating manually).
2. Prefer **MasterSwordLauncher** — it detects `0.1.5b` and can install the Update zip automatically.
3. Or extract the Update zip into your game directory.
4. Confirm the warmer / Insert menu show **0.1.5b**.

Your dump (`image.bin` + `romfs\`), saves, and `mastersword_options.ini` stay on your PC. Restart after changing Output resolution.

---

## Credits

SteelTempSaint community and testers · NotRexed and Hookshot contributors · Eden / Eglantine shader stack · everyone who filed AMD and shader reports
