# MasterSword 0.1.5c

**Release date:** 2026-11-11  
**Type:** Feature update (drop-in over 0.1.5b)

Tip-port catch-up and AMD FSR 4 enablement on top of 0.1.5b. MasterSword branding, NVIDIA DLSS Super Resolution, Nodix / MasterSword frame generation, and Reflex stay in place. FSR 4 is built in for AMD GPUs that AMD’s runtime lists; NVIDIA cards keep DLSS SR.

---

## Highlights

- AMD **FSR 4** upscaler path (FidelityFX SDK 2.4 Vulkan runtime) beside DLSS SR
- Tip ports: Graphics polish (V-Sync, mesh shaders, uncapped FPS, upscaler probes) and FSR quality steps
- Under DLSS / FSR 4, Resolution uses Native / quality render scales
- Coherent Insert-menu pointer samples (Windows + SDL)
- Fullscreen menu panel no longer disappears when the swapchain grows to 4K

---

## Changed

### Upscaling
- **FSR 4** on the Upscaling row (quality presets + Native AA), gated when AMD’s library has no FSR 4 model for the GPU
- Opt-in build (`HOOKSHOT_FSR`) with MIT FSR API headers; runtime `amd_fidelityfx_loader_vk.dll` + `amd_fidelityfx_upscaler_vk.dll` (this package includes them for Windows)
- DLSS SR path unchanged (presets K / M / L, DLAA via quality / Native)

### Graphics menu & present
- V-Sync row recreates the swapchain; mesh-shaders row; uncapped frame-rate cap
- Upscalers probed while the startup screen shows
- FSR quality 0…7 (100 / 92 / 83 / 75 then AMD modes); older inis migrate
- Insert menu scales / clips to the live swapchain so Fullscreen On keeps the panel visible

### Frame generation & feel
- MasterSword native FG and vendor FG stand-ins (including DLSS MFG label) continue as before
- Tree-shadow mask stab for a known foliage program

### Input
- Window-thread pointer mailbox (`display_pointer`) so the renderer reads a coherent sample without tearing across DPI / resize / menu capture

### Runtime
- Sampler admission: failed `vkCreateSampler` is not cached as success (already in 0.1.5b tree; documented)

---

## Unchanged (carry-forward)

- MasterSword identity (startup, Insert menu, launcher, updater)
- NVIDIA DLSS SR (Streamline / NGX presets K / M / L)
- Nodix / MasterSword frame generation + NVIDIA Reflex
- Launcher ↔ Insert menu settings sync
- Golden smoke ABI: host 10/11, load `mastersword_vk.dll`

---

## Notes

- **FSR 4 on NVIDIA:** AMD’s signed runtime typically lists no FSR 4 model (e.g. RTX 50-series) — the row may appear after probe but will not upscale; use **DLSS SR**.
- **FSR 4 on AMD:** needs the two `amd_fidelityfx_*_vk.dll` files beside the exe (shipped in this release). Log line: `vk: AMD FSR 4:`.
- Wave C state-list work (#2046 / #1980 / #2032) is **not** in this ship.

---

## Install

| Asset | Use when |
|---|---|
| **`SteelTemple-MasterSword-0.1.5c-Update.zip`** | You already have 0.1.5b / 0.1.5 / 0.1.4 — extract over the game folder, or let the launcher auto-update |
| `SteelTemple-MasterSword-0.1.5c-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

1. Close the game (and launcher, if updating manually).
2. Prefer **MasterSwordLauncher** — it detects `0.1.5c` and can install the Update zip automatically.
3. Or extract the Update zip into your game directory.
4. Confirm the warmer / Insert menu show **0.1.5c**.

Your dump (`image.bin` + `romfs\`), saves, and `mastersword_options.ini` stay on your PC. Restart after changing Output resolution.

---

## Credits

SteelTempSaint community and testers · NotRexed and Hookshot contributors · Eden / Eglantine shader stack · AMD FidelityFX / FSR SDK · everyone who filed FG, menu, and upscaler reports
